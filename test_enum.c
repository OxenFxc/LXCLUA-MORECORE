#include <windows.h>
#include <psapi.h>
#include <stdio.h>

void* get_symbol_win(const char* name) {
    HMODULE hMods[1024];
    DWORD cbNeeded;
    if (EnumProcessModules(GetCurrentProcess(), hMods, sizeof(hMods), &cbNeeded)) {
        for (unsigned int i = 0; i < (cbNeeded / sizeof(HMODULE)); i++) {
            void* addr = (void*)GetProcAddress(hMods[i], name);
            if (addr) return addr;
        }
    }
    return NULL;
}
