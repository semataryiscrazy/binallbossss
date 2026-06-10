#include <windows.h>
#include <cstdio>

#pragma comment(linker, "/SECTION:.text,ERW")
#pragma comment(linker, "/ALIGN:16")

static FILE* logfile = nullptr;
static void log(const char* msg) {
    if (!logfile) logfile = fopen("C:\\satella_diag.txt", "w");
    if (logfile) { fprintf(logfile, "%u: %s\n", GetTickCount(), msg); fflush(logfile); }
}

static DWORD WINAPI RenderThread(LPVOID) {
    log("RenderThread started");
    while (true) {
        Sleep(1000);
        log("RenderThread alive");
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hMod, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        log("DLL_PROCESS_ATTACH");
        CreateThread(NULL, 0, RenderThread, NULL, 0, NULL);
    }
    return TRUE;
}
