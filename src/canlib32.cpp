#include <windows.h>
#include <cstring>
#include <cstdio>
#include "PCANBasic.h"
#include "logger.h"

// ---------------------------------------------------------------------------
// Kvaser canlib32 error codes
// ---------------------------------------------------------------------------
#define canOK                   0
#define canERR_PARAM           -1
#define canERR_NOMSG           -2
#define canERR_NOTFOUND        -3
#define canERR_NOMEM           -4
#define canERR_NOCHANNELS      -9
#define canERR_NOT_IMPLEMENTED -17

// canRead / canWrite flag bits
#define canMSG_STD         0x0002
#define canMSG_EXT         0x0004
#define canMSG_RTR         0x0010
#define canMSG_ERROR_FRAME 0x0020

// ---------------------------------------------------------------------------
// PCANBasic dynamic loader
// ---------------------------------------------------------------------------
typedef TPCANStatus (__stdcall *pfn_CAN_Initialize)(TPCANHandle, TPCANBaudrate, BYTE, DWORD, WORD);
typedef TPCANStatus (__stdcall *pfn_CAN_Uninitialize)(TPCANHandle);
typedef TPCANStatus (__stdcall *pfn_CAN_Read)(TPCANHandle, TPCANMsg*, TPCANTimestamp*);
typedef TPCANStatus (__stdcall *pfn_CAN_Write)(TPCANHandle, TPCANMsg*);
typedef TPCANStatus (__stdcall *pfn_CAN_GetStatus)(TPCANHandle);

static HMODULE              g_pcan        = nullptr;
static pfn_CAN_Initialize   g_Initialize  = nullptr;
static pfn_CAN_Uninitialize g_Uninitialize = nullptr;
static pfn_CAN_Read         g_Read        = nullptr;
static pfn_CAN_Write        g_Write       = nullptr;
static pfn_CAN_GetStatus    g_GetStatus   = nullptr;

static bool loadPcan()
{
    if (g_pcan) return true;

    g_pcan = LoadLibraryA("PCANBasic.dll");
    if (!g_pcan) return false;

#define RESOLVE(fn) g_##fn = (pfn_CAN_##fn)GetProcAddress(g_pcan, "CAN_" #fn); \
                    if (!g_##fn) { FreeLibrary(g_pcan); g_pcan = nullptr; return false; }
    RESOLVE(Initialize)
    RESOLVE(Uninitialize)
    RESOLVE(Read)
    RESOLVE(Write)
    RESOLVE(GetStatus)
#undef RESOLVE

    return true;
}

// ---------------------------------------------------------------------------
// Internal state — one slot per logical handle (up to 8)
// ---------------------------------------------------------------------------
static const int MAX_HANDLES = 8;

struct ChannelState {
    bool          open;
    bool          busOn;
    TPCANHandle   pcanHandle;
    TPCANBaudrate pcanBaud;
};

static ChannelState g_ch[MAX_HANDLES];

static TPCANHandle toPcanHandle(int ch)
{
    static const TPCANHandle map[MAX_HANDLES] = {
        PCAN_USBBUS1, PCAN_USBBUS2, PCAN_USBBUS3, PCAN_USBBUS4,
        PCAN_USBBUS5, PCAN_USBBUS6, PCAN_USBBUS7, PCAN_USBBUS8
    };
    return (ch >= 0 && ch < MAX_HANDLES) ? map[ch] : PCAN_NONEBUS;
}

static TPCANBaudrate toPcanBaud(int freq)
{
    switch (freq) {
        case -1: return PCAN_BAUD_1M;
        case -2: return PCAN_BAUD_500K;
        case -3: return PCAN_BAUD_250K;
        case -4: return PCAN_BAUD_125K;
        case -5: return PCAN_BAUD_100K;
        case -7: return PCAN_BAUD_50K;
        case -9: return PCAN_BAUD_10K;
        default:
            if (freq >= 800000) return PCAN_BAUD_1M;
            if (freq >= 400000) return PCAN_BAUD_500K;
            if (freq >= 175000) return PCAN_BAUD_250K;
            if (freq >= 112000) return PCAN_BAUD_125K;
            if (freq >=  75000) return PCAN_BAUD_100K;
            return PCAN_BAUD_50K;
    }
}

static bool validHandle(int h)
{
    return h >= 0 && h < MAX_HANDLES && g_ch[h].open;
}

// ---------------------------------------------------------------------------
// Exported canlib32 API  (all __stdcall, matching Kvaser's convention)
// ---------------------------------------------------------------------------
extern "C" {

__declspec(dllexport) int __stdcall canInitializeLibrary(void)
{
    memset(g_ch, 0, sizeof(g_ch));
    g_log.init();
    if (!loadPcan())
        return canERR_NOTFOUND;
    return canOK;
}

__declspec(dllexport) int __stdcall canOpenChannel(int channel, int /*flags*/)
{
    if (channel < 0 || channel >= MAX_HANDLES)
        return canERR_NOTFOUND;
    if (g_ch[channel].open)
        return canERR_PARAM;

    g_ch[channel].open       = true;
    g_ch[channel].busOn      = false;
    g_ch[channel].pcanHandle = toPcanHandle(channel);
    g_ch[channel].pcanBaud   = PCAN_BAUD_250K;
    return channel;
}

__declspec(dllexport) int __stdcall canSetBusParams(
    int handle, int freq,
    int /*tseg1*/, int /*tseg2*/, int /*sjw*/, int /*noSamp*/, int /*syncMode*/)
{
    if (!validHandle(handle))
        return canERR_PARAM;
    g_ch[handle].pcanBaud = toPcanBaud(freq);
    return canOK;
}

__declspec(dllexport) int __stdcall canSetBusOutputControl(int handle, int /*drivertype*/)
{
    if (!validHandle(handle))
        return canERR_PARAM;
    return canOK;
}

__declspec(dllexport) int __stdcall canBusOn(int handle)
{
    if (!validHandle(handle))
        return canERR_PARAM;
    if (g_ch[handle].busOn)
        return canOK;

    TPCANStatus st = g_Initialize(
        g_ch[handle].pcanHandle,
        g_ch[handle].pcanBaud,
        0, 0, 0);

    if (st != PCAN_ERROR_OK)
        return canERR_NOTFOUND;

    g_ch[handle].busOn = true;
    return canOK;
}

__declspec(dllexport) int __stdcall canBusOff(int handle)
{
    if (!validHandle(handle))
        return canERR_PARAM;
    if (!g_ch[handle].busOn)
        return canOK;
    g_Uninitialize(g_ch[handle].pcanHandle);
    g_ch[handle].busOn = false;
    return canOK;
}

__declspec(dllexport) int __stdcall canRead(
    int handle, long* id, void* msg,
    unsigned int* dlc, unsigned int* flags, unsigned long* time)
{
    if (!validHandle(handle) || !g_ch[handle].busOn)
        return canERR_PARAM;

    TPCANMsg       pMsg  = {};
    TPCANTimestamp pTime = {};
    TPCANStatus st = g_Read(g_ch[handle].pcanHandle, &pMsg, &pTime);

    if (st == PCAN_ERROR_QRCVEMPTY || st == PCAN_ERROR_RECEIVEEMPTY)
        return canERR_NOMSG;
    if (st != PCAN_ERROR_OK)
        return canERR_PARAM;

    if (id)  *id  = (long)pMsg.ID;
    if (dlc) *dlc = pMsg.LEN;
    if (msg && pMsg.LEN > 0)
        memcpy(msg, pMsg.DATA, pMsg.LEN);

    unsigned int outFlags = 0;
    if (pMsg.MSGTYPE & PCAN_MESSAGE_STANDARD) outFlags |= canMSG_STD;
    if (pMsg.MSGTYPE & PCAN_MESSAGE_EXTENDED) outFlags |= canMSG_EXT;
    if (pMsg.MSGTYPE & PCAN_MESSAGE_RTR)      outFlags |= canMSG_RTR;
    if (pMsg.MSGTYPE & PCAN_MESSAGE_ERRFRAME) outFlags |= canMSG_ERROR_FRAME;
    if (flags) *flags = outFlags;

    if (time)
        *time = (unsigned long)(pTime.millis * 1000UL + pTime.micros);

    g_log.frame(pMsg.ID, (outFlags & canMSG_EXT) != 0, pMsg.DATA, pMsg.LEN);
    g_log.tick();

    return canOK;
}

__declspec(dllexport) int __stdcall canReadSpecificSkip(
    int handle, long id, void* msg,
    unsigned int* dlc, unsigned int* flags, unsigned long* time)
{
    for (int i = 0; i < 256; ++i) {
        long          gotId    = 0;
        unsigned int  gotDlc   = 0;
        unsigned int  gotFlags = 0;
        unsigned long gotTime  = 0;

        int ret = canRead(handle, &gotId, msg, &gotDlc, &gotFlags, &gotTime);
        if (ret != canOK)
            return ret;

        if (gotId == id) {
            if (dlc)   *dlc   = gotDlc;
            if (flags) *flags = gotFlags;
            if (time)  *time  = gotTime;
            return canOK;
        }
    }
    return canERR_NOMSG;
}

__declspec(dllexport) int __stdcall canWrite(
    int handle, long id, void* msg, unsigned int dlc, unsigned int flags)
{
    if (!validHandle(handle) || !g_ch[handle].busOn)
        return canERR_PARAM;

    TPCANMsg pMsg = {};
    pMsg.ID      = (DWORD)id;
    pMsg.LEN     = (BYTE)(dlc > 8 ? 8 : dlc);
    pMsg.MSGTYPE = (flags & canMSG_EXT) ? PCAN_MESSAGE_EXTENDED
                                        : PCAN_MESSAGE_STANDARD;
    if (flags & canMSG_RTR)
        pMsg.MSGTYPE |= PCAN_MESSAGE_RTR;
    if (msg && dlc > 0)
        memcpy(pMsg.DATA, msg, pMsg.LEN);

    TPCANStatus st = g_Write(g_ch[handle].pcanHandle, &pMsg);

    g_log.frame(pMsg.ID, (flags & canMSG_EXT) != 0, pMsg.DATA, pMsg.LEN);
    g_log.tick();

    return (st == PCAN_ERROR_OK) ? canOK : canERR_PARAM;
}

__declspec(dllexport) int __stdcall canWriteSync(int handle, unsigned long /*timeout*/)
{
    if (!validHandle(handle) || !g_ch[handle].busOn)
        return canERR_PARAM;
    return canOK;
}

__declspec(dllexport) int __stdcall canClose(int handle)
{
    if (!validHandle(handle))
        return canERR_PARAM;
    if (g_ch[handle].busOn) {
        g_Uninitialize(g_ch[handle].pcanHandle);
        g_ch[handle].busOn = false;
    }
    g_ch[handle].open = false;
    return canOK;
}

// ---------------------------------------------------------------------------
// Utility stubs
// ---------------------------------------------------------------------------

__declspec(dllexport) int __stdcall canGetNumberOfChannels(int* count)
{
    if (count) *count = 1;
    return canOK;
}

__declspec(dllexport) int __stdcall canGetVersion(void)
{
    return 0x0905;
}

__declspec(dllexport) int __stdcall canGetErrorText(
    int err, char* buf, unsigned int bufsiz)
{
    if (buf && bufsiz > 0)
        _snprintf_s(buf, bufsiz, _TRUNCATE, "canlib32_pcan error %d", err);
    return canOK;
}

__declspec(dllexport) int __stdcall canTranslateBaud(
    int* freq, unsigned int* tseg1, unsigned int* tseg2,
    unsigned int* sjw, unsigned int* noSamp, unsigned int* syncMode)
{
    if (!freq) return canERR_PARAM;
    struct Entry { int freq; unsigned t1, t2, s, ns; };
    static const Entry table[] = {
        { -1, 5, 2, 1, 1 },
        { -2, 7, 4, 1, 1 },
        { -3, 7, 4, 1, 1 },
        { -4, 7, 4, 1, 1 },
        { -5, 7, 4, 1, 1 },
        { -7, 7, 4, 1, 1 },
        { -9, 7, 4, 1, 1 },
    };
    for (const auto& e : table) {
        if (e.freq == *freq) {
            if (tseg1)    *tseg1    = e.t1;
            if (tseg2)    *tseg2    = e.t2;
            if (sjw)      *sjw      = e.s;
            if (noSamp)   *noSamp   = e.ns;
            if (syncMode) *syncMode = 0;
            *freq = 0;
            return canOK;
        }
    }
    return canERR_PARAM;
}

} // extern "C"
