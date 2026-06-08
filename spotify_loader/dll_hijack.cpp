#include <windows.h>

static HMODULE g_hRealVersion = NULL;

static FARPROC GetRealProc(const char* name) {
    if (!g_hRealVersion) {
        wchar_t sysPath[MAX_PATH];
        GetSystemDirectoryW(sysPath, MAX_PATH);
        wcscat_s(sysPath, L"\\version.dll");
        g_hRealVersion = LoadLibraryW(sysPath);
    }
    return g_hRealVersion ? GetProcAddress(g_hRealVersion, name) : NULL;
}

// All forward exports — 17 functions from real version.dll
#define FORWARD_EXPORT(name) __pragma(comment(linker, "/EXPORT:" name "=" name "Impl"))
FORWARD_EXPORT("GetFileVersionInfoA")
FORWARD_EXPORT("GetFileVersionInfoByHandle")
FORWARD_EXPORT("GetFileVersionInfoExA")
FORWARD_EXPORT("GetFileVersionInfoExW")
FORWARD_EXPORT("GetFileVersionInfoSizeA")
FORWARD_EXPORT("GetFileVersionInfoSizeExA")
FORWARD_EXPORT("GetFileVersionInfoSizeExW")
FORWARD_EXPORT("GetFileVersionInfoSizeW")
FORWARD_EXPORT("GetFileVersionInfoW")
FORWARD_EXPORT("VerFindFileA")
FORWARD_EXPORT("VerFindFileW")
FORWARD_EXPORT("VerInstallFileA")
FORWARD_EXPORT("VerInstallFileW")
FORWARD_EXPORT("VerLanguageNameA")
FORWARD_EXPORT("VerLanguageNameW")
FORWARD_EXPORT("VerQueryValueA")
FORWARD_EXPORT("VerQueryValueW")

extern "C" {
BOOL WINAPI GetFileVersionInfoAImpl(LPCSTR a, DWORD b, DWORD c, LPVOID d) {
    typedef BOOL(WINAPI*F)(LPCSTR,DWORD,DWORD,LPVOID);
    return ((F)GetRealProc("GetFileVersionInfoA"))(a,b,c,d);
}
DWORD WINAPI GetFileVersionInfoSizeAImpl(LPCSTR a, LPDWORD b) {
    typedef DWORD(WINAPI*F)(LPCSTR,LPDWORD);
    return ((F)GetRealProc("GetFileVersionInfoSizeA"))(a,b);
}
BOOL WINAPI GetFileVersionInfoWImpl(LPCWSTR a, DWORD b, DWORD c, LPVOID d) {
    typedef BOOL(WINAPI*F)(LPCWSTR,DWORD,DWORD,LPVOID);
    return ((F)GetRealProc("GetFileVersionInfoW"))(a,b,c,d);
}
DWORD WINAPI GetFileVersionInfoSizeWImpl(LPCWSTR a, LPDWORD b) {
    typedef DWORD(WINAPI*F)(LPCWSTR,LPDWORD);
    return ((F)GetRealProc("GetFileVersionInfoSizeW"))(a,b);
}
BOOL WINAPI GetFileVersionInfoExAImpl(DWORD a, LPCSTR b, DWORD c, DWORD d, LPVOID e) {
    typedef BOOL(WINAPI*F)(DWORD,LPCSTR,DWORD,DWORD,LPVOID);
    return ((F)GetRealProc("GetFileVersionInfoExA"))(a,b,c,d,e);
}
BOOL WINAPI GetFileVersionInfoExWImpl(DWORD a, LPCWSTR b, DWORD c, DWORD d, LPVOID e) {
    typedef BOOL(WINAPI*F)(DWORD,LPCWSTR,DWORD,DWORD,LPVOID);
    return ((F)GetRealProc("GetFileVersionInfoExW"))(a,b,c,d,e);
}
DWORD WINAPI GetFileVersionInfoSizeExAImpl(DWORD a, LPCSTR b, LPDWORD c) {
    typedef DWORD(WINAPI*F)(DWORD,LPCSTR,LPDWORD);
    return ((F)GetRealProc("GetFileVersionInfoSizeExA"))(a,b,c);
}
DWORD WINAPI GetFileVersionInfoSizeExWImpl(DWORD a, LPCWSTR b, LPDWORD c) {
    typedef DWORD(WINAPI*F)(DWORD,LPCWSTR,LPDWORD);
    return ((F)GetRealProc("GetFileVersionInfoSizeExW"))(a,b,c);
}
DWORD WINAPI GetFileVersionInfoByHandleImpl(DWORD a) {
    typedef DWORD(WINAPI*F)(DWORD);
    return ((F)GetRealProc("GetFileVersionInfoByHandle"))(a);
}
DWORD WINAPI VerFindFileAImpl(DWORD a, LPCSTR b, LPCSTR c, LPCSTR d, LPSTR e, PUINT f, LPSTR g, PUINT h) {
    typedef DWORD(WINAPI*F)(DWORD,LPCSTR,LPCSTR,LPCSTR,LPSTR,PUINT,LPSTR,PUINT);
    return ((F)GetRealProc("VerFindFileA"))(a,b,c,d,e,f,g,h);
}
DWORD WINAPI VerFindFileWImpl(DWORD a, LPCWSTR b, LPCWSTR c, LPCWSTR d, LPWSTR e, PUINT f, LPWSTR g, PUINT h) {
    typedef DWORD(WINAPI*F)(DWORD,LPCWSTR,LPCWSTR,LPCWSTR,LPWSTR,PUINT,LPWSTR,PUINT);
    return ((F)GetRealProc("VerFindFileW"))(a,b,c,d,e,f,g,h);
}
DWORD WINAPI VerInstallFileAImpl(DWORD a, LPCSTR b, LPCSTR c, LPCSTR d, LPCSTR e, LPCSTR f, LPSTR g, PUINT h) {
    typedef DWORD(WINAPI*F)(DWORD,LPCSTR,LPCSTR,LPCSTR,LPCSTR,LPCSTR,LPSTR,PUINT);
    return ((F)GetRealProc("VerInstallFileA"))(a,b,c,d,e,f,g,h);
}
DWORD WINAPI VerInstallFileWImpl(DWORD a, LPCWSTR b, LPCWSTR c, LPCWSTR d, LPCWSTR e, LPCWSTR f, LPWSTR g, PUINT h) {
    typedef DWORD(WINAPI*F)(DWORD,LPCWSTR,LPCWSTR,LPCWSTR,LPCWSTR,LPCWSTR,LPWSTR,PUINT);
    return ((F)GetRealProc("VerInstallFileW"))(a,b,c,d,e,f,g,h);
}
DWORD WINAPI VerLanguageNameAImpl(DWORD a, LPSTR b, DWORD c) {
    typedef DWORD(WINAPI*F)(DWORD,LPSTR,DWORD);
    return ((F)GetRealProc("VerLanguageNameA"))(a,b,c);
}
DWORD WINAPI VerLanguageNameWImpl(DWORD a, LPWSTR b, DWORD c) {
    typedef DWORD(WINAPI*F)(DWORD,LPWSTR,DWORD);
    return ((F)GetRealProc("VerLanguageNameW"))(a,b,c);
}
BOOL WINAPI VerQueryValueAImpl(LPCVOID a, LPCSTR b, LPVOID* c, PUINT d) {
    typedef BOOL(WINAPI*F)(LPCVOID,LPCSTR,LPVOID*,PUINT);
    return ((F)GetRealProc("VerQueryValueA"))(a,b,c,d);
}
BOOL WINAPI VerQueryValueWImpl(LPCVOID a, LPCWSTR b, LPVOID* c, PUINT d) {
    typedef BOOL(WINAPI*F)(LPCVOID,LPCWSTR,LPVOID*,PUINT);
    return ((F)GetRealProc("VerQueryValueW"))(a,b,c,d);
}
}

static DWORD WINAPI LoadSpotifyLoader(LPVOID param) {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW((HMODULE)param, path, MAX_PATH);
    wchar_t* slash = wcsrchr(path, L'\\');
    if (slash) {
        lstrcpyW(slash + 1, L"SpotifyLoader.dll");
        LoadLibraryW(path);
    }
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hDLL, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hDLL);
        HANDLE hT = CreateThread(NULL, 0, LoadSpotifyLoader, hDLL, 0, NULL);
        if (hT) CloseHandle(hT);
    }
    if (reason == DLL_PROCESS_DETACH && g_hRealVersion)
        FreeLibrary(g_hRealVersion);
    return TRUE;
}
