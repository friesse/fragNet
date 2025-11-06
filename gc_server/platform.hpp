#ifndef PLATFORM_H
#define PLATFORM_H

#ifdef _WIN32
#include <Windows.h>
#endif

namespace platform {
#ifdef _WIN32
    bool win32_enable_vt_mode() {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut == INVALID_HANDLE_VALUE) return false;

        DWORD dwMode = 0;
        if (!GetConsoleMode(hOut, &dwMode)) return false;

        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        if (!SetConsoleMode(hOut, dwMode)) return false;

        return true;
    }
#endif
}

#endif