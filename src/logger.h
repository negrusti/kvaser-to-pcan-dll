#pragma once
#include <windows.h>
#include <cstdio>
#include <cstring>

// Logs CAN frames in candump -t a format:
//   (1609459200.123456) pcan0 0CF00400#F868FFFFFEFFFFFF
//
// Controlled by canlib32_pcan.ini next to the host exe:
//   [logging]
//   enabled=1
//   file=C:\path\to\can.log
//   interface=pcan0
//
// Config is re-read every LOG_RECHECK_INTERVAL frames so toggling
// enabled=0/1 takes effect without restarting the application.

static const int LOG_RECHECK_INTERVAL = 256;

// 100-ns intervals between Windows epoch (1601-01-01) and Unix epoch (1970-01-01)
static const ULONGLONG EPOCH_DIFF_100NS = 116444736000000000ULL;

struct Logger {
    bool enabled;
    char iniPath[MAX_PATH];
    char logPath[MAX_PATH];
    char iface[64];
    int  tickCount;
    FILE* fp;

    void init()
    {
        enabled   = false;
        logPath[0] = '\0';
        tickCount  = 0;
        fp         = nullptr;
        strncpy_s(iface, "pcan0", _TRUNCATE);
        buildIniPath();
        reload();
    }

    void tick()
    {
        if (++tickCount >= LOG_RECHECK_INTERVAL) {
            tickCount = 0;
            reload();
        }
    }

    // Write one frame in candump -t a format.
    // isExtended: true for 29-bit IDs (canMSG_EXT flag set)
    void frame(DWORD id, bool isExtended, const BYTE* data, BYTE len)
    {
        if (!enabled) return;
        openIfNeeded();
        if (!fp) return;

        // Timestamp: Unix seconds + microseconds
        FILETIME ft;
        GetSystemTimePreciseAsFileTime(&ft);
        ULONGLONG t100ns = ((ULONGLONG)ft.dwHighDateTime << 32 | ft.dwLowDateTime)
                           - EPOCH_DIFF_100NS;
        ULONGLONG sec  = t100ns / 10000000ULL;
        ULONGLONG usec = (t100ns % 10000000ULL) / 10ULL;

        // ID: 3 hex digits for standard (11-bit), 8 for extended (29-bit)
        if (isExtended)
            fprintf(fp, "(%llu.%06llu) %s %08X#", sec, usec, iface, id);
        else
            fprintf(fp, "(%llu.%06llu) %s %03X#", sec, usec, iface, id);

        // Data bytes — no spaces, uppercase
        for (BYTE i = 0; i < len && i < 8; ++i)
            fprintf(fp, "%02X", data[i]);

        fputc('\n', fp);
        fflush(fp);
    }

private:
    void buildIniPath()
    {
        GetModuleFileNameA(nullptr, iniPath, MAX_PATH);
        char* slash = strrchr(iniPath, '\\');
        if (slash) *(slash + 1) = '\0';
        strcat_s(iniPath, "canlib32_pcan.ini");
    }

    void reload()
    {
        DWORD en = GetPrivateProfileIntA("logging", "enabled", 0, iniPath);

        // Read raw path from ini, expand env vars, fall back to %TEMP%\canlib32_pcan.log
        char rawPath[MAX_PATH] = {};
        GetPrivateProfileStringA("logging", "file", "", rawPath, MAX_PATH, iniPath);

        char newPath[MAX_PATH] = {};
        if (rawPath[0] != '\0') {
            ExpandEnvironmentStringsA(rawPath, newPath, MAX_PATH);
        } else {
            DWORD len = GetTempPathA(MAX_PATH, newPath);
            if (len == 0 || len > MAX_PATH)
                strncpy_s(newPath, "C:\\Temp\\", _TRUNCATE);
            strcat_s(newPath, "canlib32_pcan.log");
        }

        char newIface[64] = {};
        GetPrivateProfileStringA("logging", "interface", "pcan0", newIface, sizeof(newIface), iniPath);

        // Close log if path or enabled state changed
        bool pathChanged = strcmp(newPath, logPath) != 0;
        if (fp && (!en || pathChanged)) {
            fclose(fp);
            fp = nullptr;
        }

        enabled = (en != 0);
        strncpy_s(logPath, newPath, _TRUNCATE);
        strncpy_s(iface,   newIface, _TRUNCATE);
    }

    void openIfNeeded()
    {
        if (fp || logPath[0] == '\0') return;
        fopen_s(&fp, logPath, "a");
    }
};

static Logger g_log;
