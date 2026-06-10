#include <windows.h>

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        MessageBoxA(NULL, "Satella DLL injected!", "Test", MB_OK | MB_SYSTEMMODAL);
    }
    return TRUE;
}
