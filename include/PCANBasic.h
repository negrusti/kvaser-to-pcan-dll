// Minimal PCANBasic.h stub — enough to compile without the PEAK SDK installed.
// Replace this file (or override the include path in CMake) with the real header
// from the PCAN-Basic SDK: https://www.peak-system.com/PCAN-Basic.239.0.html
//
// Real SDK default install path:
//   C:\Program Files\PEAK-System\PCAN-Basic API\Include\PCANBasic.h
//   (CMakeLists.txt already adds that directory; it takes precedence over this stub)

#pragma once
#include <windows.h>

// PCAN channel handles
#define PCAN_NONEBUS   0x00U
#define PCAN_USBBUS1   0x51U
#define PCAN_USBBUS2   0x52U
#define PCAN_USBBUS3   0x53U
#define PCAN_USBBUS4   0x54U
#define PCAN_USBBUS5   0x55U
#define PCAN_USBBUS6   0x56U
#define PCAN_USBBUS7   0x57U
#define PCAN_USBBUS8   0x58U

// Baud rates
#define PCAN_BAUD_1M    0x0014U
#define PCAN_BAUD_800K  0x0016U
#define PCAN_BAUD_500K  0x001CU
#define PCAN_BAUD_250K  0x011CU
#define PCAN_BAUD_125K  0x031CU
#define PCAN_BAUD_100K  0x432FU
#define PCAN_BAUD_95K   0xC34EU
#define PCAN_BAUD_83K   0x852BU
#define PCAN_BAUD_50K   0x472FU
#define PCAN_BAUD_47K   0x1414U
#define PCAN_BAUD_33K   0x8B2FU
#define PCAN_BAUD_20K   0x532FU
#define PCAN_BAUD_10K   0x672FU
#define PCAN_BAUD_5K    0x7F7FU

// Error/status codes
#define PCAN_ERROR_OK           0x00000U
#define PCAN_ERROR_XMTFULL      0x00001U
#define PCAN_ERROR_OVERRUN      0x00002U
#define PCAN_ERROR_BUSLIGHT     0x00004U
#define PCAN_ERROR_BUSHEAVY     0x00008U
#define PCAN_ERROR_BUSWARNING   PCAN_ERROR_BUSHEAVY
#define PCAN_ERROR_BUSPASSIVE   0x40000U
#define PCAN_ERROR_BUSOFF       0x00010U
#define PCAN_ERROR_ANYBUSERR    (PCAN_ERROR_BUSWARNING | PCAN_ERROR_BUSLIGHT | PCAN_ERROR_BUSHEAVY | PCAN_ERROR_BUSOFF | PCAN_ERROR_BUSPASSIVE)
#define PCAN_ERROR_QRCVEMPTY    0x00020U
#define PCAN_ERROR_RECEIVEEMPTY 0x00020U  // alias
#define PCAN_ERROR_QOVERRUN     0x00040U
#define PCAN_ERROR_QXMTFULL     0x00080U
#define PCAN_ERROR_REGTEST      0x00100U
#define PCAN_ERROR_NODRIVER     0x00200U
#define PCAN_ERROR_HWINUSE      0x00400U
#define PCAN_ERROR_NETINUSE     0x00800U
#define PCAN_ERROR_ILLHW        0x01400U
#define PCAN_ERROR_ILLNET       0x01800U
#define PCAN_ERROR_ILLCLIENT    0x01C00U
#define PCAN_ERROR_ILLHANDLE    (PCAN_ERROR_ILLHW | PCAN_ERROR_ILLNET | PCAN_ERROR_ILLCLIENT)
#define PCAN_ERROR_RESOURCE     0x02000U
#define PCAN_ERROR_ILLPARAMTYPE 0x04000U
#define PCAN_ERROR_ILLPARAMVAL  0x08000U
#define PCAN_ERROR_UNKNOWN      0x10000U
#define PCAN_ERROR_ILLDATA      0x20000U
#define PCAN_ERROR_ILLMODE      0x80000U
#define PCAN_ERROR_CAUTION      0x2000000U
#define PCAN_ERROR_INITIALIZE   0x4000000U
#define PCAN_ERROR_ILLOPERATION 0x8000000U

// Message type flags
#define PCAN_MESSAGE_STANDARD  0x00U
#define PCAN_MESSAGE_RTR       0x01U
#define PCAN_MESSAGE_EXTENDED  0x02U
#define PCAN_MESSAGE_FD        0x04U
#define PCAN_MESSAGE_BRS       0x08U
#define PCAN_MESSAGE_ESI       0x10U
#define PCAN_MESSAGE_ECHO      0x20U
#define PCAN_MESSAGE_ERRFRAME  0x40U
#define PCAN_MESSAGE_STATUS    0x80U

// Hardware type (for CAN_Initialize non-USB)
#define PCAN_USB  0x08U

typedef DWORD  TPCANStatus;
typedef WORD   TPCANBaudrate;
typedef BYTE   TPCANHandle;
typedef BYTE   TPCANMessageType;

#pragma pack(push, 1)
typedef struct tagTPCANMsg {
    DWORD            ID;        // 11 or 29 bit identifier
    TPCANMessageType MSGTYPE;   // type of the message
    BYTE             LEN;       // data length (0..8)
    BYTE             DATA[8];   // data bytes
} TPCANMsg;

typedef struct tagTPCANTimestamp {
    DWORD millis;        // base timestamp in milliseconds
    WORD  millis_overflow;
    WORD  micros;        // microseconds (0..999)
} TPCANTimestamp;
#pragma pack(pop)

// Functions are loaded dynamically via LoadLibrary — no declarations needed here.
