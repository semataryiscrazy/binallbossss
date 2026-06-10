#pragma once
#include <functional>
#include <sstream>
#include <string>
#include <windows.h>
#include <shlwapi.h>
#include <process.h>
#include <cmath>
#include <cstdio>
#include "Cfg/strenc.h"
#include "Cfg/encrypt.hh"
#include "Cfg/minhook/MinHook.h"
#include "Cfg/minhook/hde/hde64.h"

inline void diag_log(const char* msg) {
    FILE* f = fopen("C:\\satella_dbg.txt", "a");
    if (f) { fprintf(f, "%u: %s\n", GetTickCount(), msg); fflush(f); fclose(f); }
    OutputDebugStringA(msg);
}

// Proccess
inline uintptr_t il2cpp = 0x0;
inline size_t il2cppSize = 0;
inline uintptr_t libunity = 0x0;
inline uintptr_t UnityCpp = 0x0;
inline uintptr_t unity = 0x0;
inline int SWidth = 0;
inline int SHeight = 0;
inline HWND JanelaAlvo = NULL;

enum class ABIType {
    Unknown,
    ARM32,
    ARM64,
    X86,
    X86_64
};

inline ABIType VMM_ABI = ABIType::Unknown;

inline const char* ABIToString(ABIType abi) {
    switch (abi) {
    case ABIType::ARM32: return "ARM32";
    case ABIType::ARM64: return "ARM64";
    case ABIType::X86: return "X86";
    case ABIType::X86_64: return "X86_64";
    default: return "Unknown";
    }
}

inline void GravarRegistroInt(const char* chave, int valor) {
    HKEY hKey;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, AY_OBFUSCATE("Software\\VOIDCORP"), 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExA(hKey, chave, 0, REG_DWORD, (const BYTE*)&valor, sizeof(valor));
        RegCloseKey(hKey);
    }
}

inline void LerRegistroInt(const char* chave, int& valor) {
    HKEY hKey;
    DWORD tamanho = sizeof(valor);
    if (RegOpenKeyExA(HKEY_CURRENT_USER, AY_OBFUSCATE("Software\\VOIDCORP"), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegQueryValueExA(hKey, chave, NULL, NULL, (BYTE*)&valor, &tamanho);
        RegCloseKey(hKey);
    }
}

inline HWND LookupWindowByClassName(const char* toFindClassName) {
    HWND result = NULL;

    auto enumCallback = [&](HWND currWnd) {

        char currClassName[260];

        if (RealGetWindowClassA(currWnd, currClassName, sizeof(currClassName)) < 1)
            return true;

        if (strcmp(currClassName, toFindClassName) == 0)
        {
            result = currWnd;
            return false;
        }

        return true;
        };

    std::function<bool(HWND)> callbackWrapper = enumCallback;

    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        auto callback = reinterpret_cast<std::function<bool(HWND)>*>(lParam);

        if ((*callback)(hwnd) == false)
            return FALSE;

        EnumChildWindows(hwnd, [](HWND childHwnd, LPARAM childLParam) -> BOOL {
            auto childCallback = reinterpret_cast<std::function<bool(HWND)>*>(childLParam);
            return (*childCallback)(childHwnd);
            }, lParam);

        return TRUE;
        }, reinterpret_cast<LPARAM>(&callbackWrapper));

    return result;
}

inline std::string ObterLocalDeInstalacao(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess != NULL) {
        char buffer[MAX_PATH];
        DWORD dwSize = sizeof(buffer);
        if (QueryFullProcessImageNameA(hProcess, 0, buffer, &dwSize)) {
            PathRemoveFileSpecA(buffer);
            CloseHandle(hProcess);
            return buffer;
        }
        CloseHandle(hProcess);
    }
    return "";
}

inline void ShellQuiet(const char* cmd) {
    static int x = 0;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    if (CreateProcessA(NULL, const_cast<char*>(cmd), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {

        WaitForSingleObject(pi.hProcess, INFINITE);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

inline std::string Shell_Return(const char* cmd) {
    HANDLE hReadPipe, hWritePipe;
    SECURITY_ATTRIBUTES saAttr{ sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

    if (!CreatePipe(&hReadPipe, &hWritePipe, &saAttr, 0)) {
        return "";
    }

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.hStdError = hWritePipe;
    si.hStdOutput = hWritePipe;
    si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi{};
    std::string command = std::string("cmd.exe /C ") + cmd;

    BOOL success = CreateProcessA(
        NULL,
        const_cast<char*>(command.c_str()),
        NULL, NULL, TRUE,
        CREATE_NO_WINDOW,
        NULL, NULL,
        &si, &pi
    );

    CloseHandle(hWritePipe);

    std::string result;
    if (success) {
        char buffer[4096];
        DWORD bytesRead;

        while (ReadFile(hReadPipe, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
            result.append(buffer, bytesRead);
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    CloseHandle(hReadPipe);
    return result;
}


inline void ShellQuiet_Adb(const char* cmd) {
    std::string AdbPath = ObterLocalDeInstalacao(GetCurrentProcessId());
    std::string fullCmd = "\"" + AdbPath + "\\HD-Adb\" " + cmd;
    if (GetFileAttributesA((AdbPath + "\\HD-Adb.exe").c_str()) == INVALID_FILE_ATTRIBUTES) {
        static const char* altPaths[] = {
            "C:\\Program Files\\BlueStacks_nxt\\HD-Adb.exe",
            "C:\\Program Files\\BlueStacks\\HD-Adb.exe"
        };
        for (auto& p : altPaths) {
            if (GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES) {
                std::string dir = p;
                dir.erase(dir.rfind('\\'));
                fullCmd = "\"" + dir + "\\HD-Adb\" " + cmd;
                break;
            }
        }
    }
    ShellQuiet(fullCmd.c_str());
}

inline std::string ShellReturn_Adb(const char* cmd) {
    std::string AdbPath = ObterLocalDeInstalacao(GetCurrentProcessId());
    std::string fullCmd = "\"" + AdbPath + "\\HD-Adb\" " + cmd;
    if (GetFileAttributesA((AdbPath + "\\HD-Adb.exe").c_str()) == INVALID_FILE_ATTRIBUTES) {
        static const char* altPaths[] = {
            "C:\\Program Files\\BlueStacks_nxt\\HD-Adb.exe",
            "C:\\Program Files\\BlueStacks\\HD-Adb.exe"
        };
        for (auto& p : altPaths) {
            if (GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES) {
                std::string dir = p;
                dir.erase(dir.rfind('\\'));
                fullCmd = "\"" + dir + "\\HD-Adb\" " + cmd;
                break;
            }
        }
    }
    return Shell_Return(fullCmd.c_str());
}


inline void ShellReturn_HDPlayer(const char* cmd) {
    std::string AdbPath = ObterLocalDeInstalacao(GetCurrentProcessId());
    AdbPath += (const char*)AY_OBFUSCATE("\\HD-Player ");
    AdbPath += cmd;
    ShellQuiet(AdbPath.c_str());
}

inline std::string DetectPortFromNetstat() {
    DWORD pid = GetCurrentProcessId();
    std::string cmd = "cmd /c netstat -ano | findstr LISTENING";
    std::string output = Shell_Return(cmd.c_str());
    if (output.empty()) return "127.0.0.1:5555";

    std::istringstream stream(output);
    std::string line;
    std::string fallback;

    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        size_t s = line.find_first_not_of(" \t");
        if (s != std::string::npos) line = line.substr(s);
        if (line.empty()) continue;

        std::vector<std::string> tokens;
        {
            std::istringstream ts(line);
            std::string t;
            while (ts >> t) tokens.push_back(t);
        }
        if (tokens.size() < 5) continue;

        std::string localAddr = tokens[1];
        std::string pidStr = tokens[4];

        // Strategy 1: exact PID match (original)
        if (pidStr == std::to_string(pid)) return localAddr;

        // Strategy 2: port in ADB range (5554-5580)
        auto colonPos = localAddr.rfind(':');
        if (colonPos != std::string::npos) {
            std::string portStr = localAddr.substr(colonPos + 1);
            try {
                int portnum = std::stoi(portStr);
                if (portnum >= 5554 && portnum <= 5580) {
                    fallback = localAddr;
                }
            } catch (...) {}
        }
    }

    if (!fallback.empty()) return fallback;
    return "127.0.0.1:5555";
}

inline ABIType DetectABIFromAndroid() {
    std::string abiCmd = "-s " + DetectPortFromNetstat() + " shell getprop ro.product.cpu.abi";
    std::string abi = ShellReturn_Adb(abiCmd.c_str());

    abi.erase(abi.find_last_not_of(" \n\r\t") + 1);

    if (abi == "x86_64") {
        std::cout << "[DetectABIFromAndroid] ABI detectado: x86_64\n";
        return ABIType::X86_64;
    }
    if (abi == "arm64-v8a") {
        std::cout << "[DetectABIFromAndroid] ABI detectado: arm64\n";
        return ABIType::ARM64;
    }
    if (abi == "x86") {
        std::cout << "[DetectABIFromAndroid] ABI detectado: x86\n";
        return ABIType::X86;
    }
    if (abi == "armeabi-v7a") {
        std::cout << "[DetectABIFromAndroid] ABI detectado: arm (v7a)\n";
        return ABIType::ARM32;
    }

    std::cout << "[DetectABIFromAndroid] ABI desconhecido: " << abi << "\n";
    return ABIType::Unknown;
}

inline bool ConnectEmulator() {
    static bool Conectado = false;

    if (!Conectado) {
        std::string porta = DetectPortFromNetstat();
        if (porta.empty()) {
            std::cerr << "[ConnectEmulator] Falha ao detectar porta do netstat!" << std::endl;
            return false;
        }

        std::string conn_adb = "connect " + porta;
        std::cout << "[ConnectEmulator] Conectando ADB: " << conn_adb << std::endl;
        ShellQuiet_Adb(conn_adb.c_str());

        VMM_ABI = DetectABIFromAndroid();
        std::cout << "[ConnectEmulator] ABI detectada: " << ABIToString(VMM_ABI) << std::endl;
        std::cout << "[ConnectEmulator] Offsets configurados para funcionar com todas as arquiteturas" << std::endl;

        Conectado = true;
    }

    return Conectado;
}

// ─── Forward declarations for Ler/Escrever ───
template<typename T> T Ler(uint32_t virtualAddress);
template<typename T> void Escrever(uint32_t virtualAddress, T value);

// ─── AOB Scan & Patch ───
struct AOBPattern {
    const uint8_t* search;
    const uint8_t* replace;
    const char* mask;  // 'x' = match, '?' = wildcard
    size_t length;
    const char* name;
};

static constexpr size_t AOB_PAGE = 4096;
inline uintptr_t AOBScan(const uint8_t* pattern, const char* mask, uintptr_t start, size_t size) {
    size_t patternLen = strlen(mask);
    if (patternLen == 0 || size < patternLen) return 0;
    uint8_t page[AOB_PAGE];
    uintptr_t end = start + size;
    for (uintptr_t pos = start; pos < end; pos += AOB_PAGE - patternLen) {
        size_t chunk = (std::min)(end - pos, AOB_PAGE);
        bool ok = true;
        for (size_t k = 0; k < chunk; k++) {
            page[k] = Ler<uint8_t>((uint32_t)(pos + k));
            if (k == 0 && page[k] == 0 && pos + k > start) { ok = false; break; }
        }
        if (!ok) continue;
        for (size_t j = 0; j <= chunk - patternLen; j++) {
            bool match = true;
            for (size_t i = 0; i < patternLen; i++) {
                if (mask[i] != '?' && page[j + i] != pattern[i]) { match = false; break; }
            }
            if (match) return pos + j;
        }
    }
    return 0;
}

inline bool AOBApply(const AOBPattern& p, uintptr_t base, size_t libSize) {
    uintptr_t addr = AOBScan(p.search, p.mask, base, libSize);
    if (addr == 0) return false;
    for (size_t i = 0; i < p.length; i++) {
        if (p.mask[i] == 'x')
            Escrever<uint8_t>((uint32_t)(addr + i), p.replace[i]);
    }
    return true;
}

inline uintptr_t ObterEnderecoDaBiblioteca(const char* libName) {

    std::string pid;
    pid = (const char*)AY_OBFUSCATE("-s ") + DetectPortFromNetstat();
    pid += (const char*)AY_OBFUSCATE(" shell pidof com.dts.freefireth");
    pid = ShellReturn_Adb(pid.c_str());
    pid.erase(pid.find_last_not_of(" \n\r\t") + 1);

    char command[256];
    sprintf_s(command, (const char*)AY_OBFUSCATE("-s %s shell /boot/android/android/system/xbin/bstk/su 0 busybox cat /proc/%s/maps"), DetectPortFromNetstat().c_str(), pid.c_str());

    std::string output = ShellReturn_Adb(command);

    std::istringstream stream(output);
    std::string line;
    size_t biggestSize = 0;
    uintptr_t foundBase = 0;
    while (std::getline(stream, line)) {
        if (line.find(libName) != std::string::npos) {
            size_t dash = line.find('-');
            if (dash == std::string::npos) continue;
            std::string baseStr = line.substr(0, dash);
            uintptr_t base = std::stoull(baseStr, nullptr, 16);
            size_t space = line.find(' ', dash + 1);
            if (space == std::string::npos) continue;
            std::string endStr = line.substr(dash + 1, space - dash - 1);
            uintptr_t endAddr = std::stoull(endStr, nullptr, 16);
            size_t sz = endAddr - base;
            if (sz > biggestSize) {
                biggestSize = sz;
                foundBase = base;
            }
        }
    }
    if (libName == AY_OBFUSCATE("libil2cpp.so")) {
        il2cppSize = biggestSize;
    }
    return foundBase;
}


inline struct {
    void* pVM = NULL;
    uint64_t GuestCR3 = 0;
    uintptr_t PhysicalBase = 0;      // Base f�sica do libil2cpp.so
    uintptr_t VirtualBase = 0;       // Base virtual do libil2cpp.so
    bool UseDirectMapping = false;   // Se true, usa mapeamento direto
} VMM;

inline void* (*VMMGetCpuById)(void* pVM, int idCpu);
inline int (*PGMPhysRead)(void* pVM, uintptr_t GCPhys, void* pvBuf, size_t bufSize);
inline int (*PGMPhysWrite)(void* pVM, uintptr_t GCPhys, void* pvBuf, size_t bufSize);
inline uint64_t(*CPUMGetGuestCR3)(void* pVCpu);
inline int (*PGMPhysGCPtr2GCPhys)(void* pVCpu, uintptr_t GCPtr, uintptr_t* pGCPhys);  // NOVA FUNÃ‡ÃƒO!

inline int (*PGMPhysRead_Orig)(void* pVM, uintptr_t GCPhys, void* pvBuf, size_t bufSize);
inline BYTE PGMPhysRead_Backup[16] = {0};
inline int PGMPhysReadHook(void* pVM, uintptr_t GCPhys, void* pvBuf, size_t bufSize) {
    VMM.pVM = pVM;
    return PGMPhysRead_Orig(pVM, GCPhys, pvBuf, bufSize);
}

// Global mutex to serialize all VMM access across threads
inline std::recursive_mutex g_vmmMutex;

template<typename T>
T ReadPhysicalMemory(uintptr_t physicalAddress) {
    std::lock_guard<std::recursive_mutex> lock(g_vmmMutex);
    T var = 0;
    void* pVM = VMM.pVM;
    if (pVM != NULL) {
        int Read = PGMPhysRead(pVM, physicalAddress, &var, sizeof(T));
        if (Read == 0) {
            return var;
        }
    }
    return T();
}

inline uintptr_t EnderecoVirtualParaFisico64(uint64_t guestCR3, uint64_t virtualAddr) {
    std::lock_guard<std::recursive_mutex> lock(g_vmmMutex);
    if (!VMM.pVM || !PGMPhysRead)
        return 0;

    const uint64_t P_BIT = 1ULL << 0;
    const uint64_t RW_BIT = 1ULL << 1;
    const uint64_t US_BIT = 1ULL << 2;
    const uint64_t PS_BIT = 1ULL << 7;
    const uint64_t NX_BIT = 1ULL << 63;

    if ((virtualAddr >> 47) & 1) {
        if ((virtualAddr >> 48) != 0xFFFF)
            return 0;
    }
    else {
        if ((virtualAddr >> 48) != 0)
            return 0;
    }

    uint64_t pml4Base = guestCR3 & 0x000FFFFFFFFFF000ULL;
    uint64_t pml4Index = (virtualAddr >> 39) & 0x1FFULL;
    uint64_t pml4Entry = 0;

    if (PGMPhysRead(VMM.pVM, pml4Base + (pml4Index * 8), &pml4Entry,
        sizeof(uint64_t)) != 0)
        return 0;

    if (!(pml4Entry & P_BIT))
        return 0;

    uint64_t pdptBase = pml4Entry & 0x000FFFFFFFFFF000ULL;
    uint64_t pdptIndex = (virtualAddr >> 30) & 0x1FFULL;
    uint64_t pdptEntry = 0;

    if (PGMPhysRead(VMM.pVM, pdptBase + (pdptIndex * 8), &pdptEntry,
        sizeof(uint64_t)) != 0)
        return 0;

    if (!(pdptEntry & P_BIT))
        return 0;

    if (pdptEntry & PS_BIT) {
        uint64_t base1G = pdptEntry & 0x000FFFFFC0000000ULL;
        uint64_t offs1G = virtualAddr & 0x3FFFFFFFULL;
        return base1G | offs1G;
    }

    uint64_t pdBase = pdptEntry & 0x000FFFFFFFFFF000ULL;
    uint64_t pdIndex = (virtualAddr >> 21) & 0x1FFULL;
    uint64_t pdEntry = 0;

    if (PGMPhysRead(VMM.pVM, pdBase + (pdIndex * 8), &pdEntry,
        sizeof(uint64_t)) != 0)
        return 0;

    if (!(pdEntry & P_BIT))
        return 0;

    if (pdEntry & PS_BIT) {
        uint64_t base2M = pdEntry & 0x000FFFFFFFFFE00000ULL;
        uint64_t offs2M = virtualAddr & 0x1FFFFFULL;
        return base2M | offs2M;
    }

    uint64_t ptBase = pdEntry & 0x000FFFFFFFFFF000ULL;
    uint64_t ptIndex = (virtualAddr >> 12) & 0x1FFULL;
    uint64_t ptEntry = 0;

    if (PGMPhysRead(VMM.pVM, ptBase + (ptIndex * 8), &ptEntry,
        sizeof(uint64_t)) != 0)
        return 0;

    if (!(ptEntry & P_BIT))
        return 0;

    return (ptEntry & 0x000FFFFFFFFFF000ULL) | (virtualAddr & 0xFFFULL);
}

inline uintptr_t EnderecoVirtualParaFisico32(uint64_t guestCR3, uint32_t virtualAddr) {
    std::lock_guard<std::recursive_mutex> lock(g_vmmMutex);
    constexpr uint32_t P_BIT = 1U << 0;

    if (!VMM.pVM || !PGMPhysRead)
        return 0;

    static int logCount = 0;
    bool shouldLog = (logCount < 3);
    
    if (shouldLog) {
        std::cout << "[V2P32] Traducao 32-bit: VA=0x" << std::hex << virtualAddr 
                  << ", CR3=0x" << guestCR3 << std::endl;
    }

    uint32_t pdeBase = static_cast<uint32_t>(guestCR3 & 0xFFFFF000ULL);
    uint32_t pdeIndex = (virtualAddr >> 22) & 0x3FF;
    uint32_t pdeEntry = 0;

    if (shouldLog) {
        std::cout << "[V2P32] pdeBase=0x" << std::hex << pdeBase 
                  << ", pdeIndex=0x" << pdeIndex << std::endl;
    }

    if (PGMPhysRead(VMM.pVM, pdeBase + (pdeIndex * 4), &pdeEntry,
        sizeof(pdeEntry)) != 0 ||
        !(pdeEntry & P_BIT)) {
        if (shouldLog) {
            std::cout << "[V2P32] FALHA: PDE read failed ou not present. pdeEntry=0x" 
                      << std::hex << pdeEntry << std::endl;
            logCount++;
        }
        return 0;
    }

    uint32_t ptBase = pdeEntry & 0xFFFFF000;
    uint32_t ptIndex = (virtualAddr >> 12) & 0x3FF;
    uint32_t ptEntry = 0;

    if (PGMPhysRead(VMM.pVM, ptBase + (ptIndex * 4), &ptEntry, sizeof(ptEntry)) !=
        0 ||
        !(ptEntry & P_BIT)) {
        if (shouldLog) {
            std::cout << "[V2P32] FALHA: PTE read failed ou not present. ptEntry=0x" 
                      << std::hex << ptEntry << std::endl;
            logCount++;
        }
        return 0;
    }

    uintptr_t result = (ptEntry & 0xFFFFF000) | (virtualAddr & 0xFFF);
    
    if (shouldLog) {
        std::cout << "[V2P32] SUCESSO: PA=0x" << std::hex << result << std::endl;
        logCount++;
    }
    
    return result;
}

inline uintptr_t TranslateVirtualToPhysical(uintptr_t va, uintptr_t cr3, uintptr_t& pa) {
    // FORÃ‡AR modo 32 bits para Free Fire no BlueStacks
    // O Free Fire sempre roda em 32 bits mesmo em emuladores x86_64
    if (va < 0x100000000ULL) {
        // EndereÃ§o virtual Ã© 32 bits, usar traduÃ§Ã£o 32 bits
        pa = EnderecoVirtualParaFisico32(cr3, static_cast<uint32_t>(va));
        return pa;
    }
    
    // Fallback para 64 bits se o endereÃ§o for realmente 64 bits
    switch (VMM_ABI) {
    case ABIType::X86_64:
    case ABIType::ARM64:
        pa = EnderecoVirtualParaFisico64(cr3, va);
        return pa;

    case ABIType::X86:
    case ABIType::ARM32: {
        pa = EnderecoVirtualParaFisico32(cr3, static_cast<uint32_t>(va));
        return pa;
    }

    default:
        return 0;
    }
}

template<typename T>
T Ler(uint32_t virtualAddress) {
    std::lock_guard<std::recursive_mutex> lock(g_vmmMutex);
    T var{};
    void* pVM = VMM.pVM;
    
    if (pVM != nullptr && PGMPhysGCPtr2GCPhys != nullptr) {
        // Tentar converter usando mÃºltiplas CPUs (como o projeto funcional faz)
        for (int cpuId = 0; cpuId < 4; cpuId++) {
            void* cpu = VMMGetCpuById(pVM, cpuId);
            if (cpu == nullptr) continue;
            
            uintptr_t physAddr = 0;
            if (PGMPhysGCPtr2GCPhys(cpu, virtualAddress, &physAddr) == 0) {
                if (PGMPhysRead(pVM, physAddr, &var, sizeof(T)) == 0) {
                    return var;
                }
            }
        }
    }
    
    return T();
}

template<typename T>
void Escrever(uint32_t virtualAddress, T value) {
    std::lock_guard<std::recursive_mutex> lock(g_vmmMutex);
    void* pVM = VMM.pVM;
    
    if (pVM != nullptr && PGMPhysGCPtr2GCPhys != nullptr) {
        // Tentar converter usando mÃºltiplas CPUs
        for (int cpuId = 0; cpuId < 4; cpuId++) {
            void* cpu = VMMGetCpuById(pVM, cpuId);
            if (cpu == nullptr) continue;
            
            uintptr_t physAddr = 0;
            if (PGMPhysGCPtr2GCPhys(cpu, virtualAddress, &physAddr) == 0) {
                PGMPhysWrite(pVM, physAddr, &value, sizeof(T));
                return;
            }
        }
    }
}

inline void UnloadHooks() {
    if (PGMPhysRead) {
        MH_DisableHook(PGMPhysRead);
        MH_RemoveHook(PGMPhysRead);
    }
    MH_Uninitialize();
}

inline void VMMFallbackScan() {
    if (il2cpp != 0) return;
    if (VMM.pVM == nullptr) {
        std::cout << "[VMMFallback] Waiting for VMM init..." << std::endl;
        for (int i = 0; i < 100; i++) {
            Sleep(100);
            if (VMM.pVM != nullptr) break;
        }
    }
    if (VMM.pVM == nullptr) {
        std::cout << "[VMMFallback] VMM not available" << std::endl;
        return;
    }
    if (!VMMGetCpuById || !PGMPhysGCPtr2GCPhys || !PGMPhysRead) {
        std::cout << "[VMMFallback] VMM functions not available" << std::endl;
        return;
    }
    void* cpu0 = VMMGetCpuById(VMM.pVM, 0);
    if (cpu0 == nullptr) {
        std::cout << "[VMMFallback] CPU 0 not available" << std::endl;
        return;
    }
    
    // Try each CPU until we find il2cpp
    void* cpus[4] = { cpu0, nullptr, nullptr, nullptr };
    for (int i = 1; i < 4; i++)
        cpus[i] = VMMGetCpuById(VMM.pVM, i);
    
    std::cout << "[VMMFallback] Scanning guest virtual memory for libil2cpp..." << std::endl;
    DWORD startTick = GetTickCount();
    int scannedPages = 0;
    for (int cpuIdx = 0; cpuIdx < 4; cpuIdx++) {
        void* cpu = cpus[cpuIdx];
        if (cpu == nullptr) continue;
        if (il2cpp != 0) break;
        
        for (uintptr_t va = 0x60000000; va < 0x90000000; va += 0x1000) {
            if (il2cpp != 0) break;
            if (GetTickCount() - startTick > 15000) break;
            
            uintptr_t physAddr = 0;
            if (PGMPhysGCPtr2GCPhys(cpu, va, &physAddr) != 0) continue;
            scannedPages++;
            
            uint32_t magic = 0;
            if (PGMPhysRead(VMM.pVM, physAddr, &magic, sizeof(magic)) != 0) continue;
            if (magic == 0x464C457F) {
                il2cpp = va;
                libunity = va;
                UnityCpp = va;
                std::cout << "[VMMFallback] Found ELF at 0x" << std::hex << va << " (first hit, using as il2cpp)" << std::endl;
                break;
            }
        }
    }
    if (il2cpp == 0) {
        std::cout << "[VMMFallback] Primary scan failed (" << scannedPages << " pages, " << (GetTickCount() - startTick) << "ms). Trying secondary range 0x10000-0x5FFFFFF..." << std::endl;
        void* cpu = cpus[0] ? cpus[0] : (cpus[1] ? cpus[1] : cpus[2]);
        if (cpu) {
            for (uintptr_t va = 0x10000; va < 0x60000000 && GetTickCount() - startTick < 25000; va += 0x1000) {
                uintptr_t physAddr = 0;
                if (PGMPhysGCPtr2GCPhys(cpu, va, &physAddr) != 0) continue;
                scannedPages++;
                uint32_t magic = 0;
                if (PGMPhysRead(VMM.pVM, physAddr, &magic, sizeof(magic)) != 0) continue;
                if (magic == 0x464C457F) {
                    il2cpp = va;
                    libunity = va;
                    UnityCpp = va;
                    std::cout << "[VMMFallback] Found ELF at 0x" << std::hex << va << " (secondary scan)" << std::endl;
                    break;
                }
            }
        }
    }
    if (il2cpp == 0) {
        std::cout << "[VMMFallback] No ELF found after " << scannedPages << " mapped pages, " << (GetTickCount() - startTick) << "ms" << std::endl;
    }
}

static std::atomic<bool> g_vmmStarted{ false };

inline void LoadLibraryAndHook() {
    if (g_vmmStarted.exchange(true)) {
        diag_log("LoadLibraryAndHook: already started, skipping");
        return;
    }
    diag_log("LoadLibraryAndHook: start");
    LPCSTR dllName = AY_OBFUSCATE("BstkVMM.dll");
    LPCSTR fn2 = AY_OBFUSCATE("PGMPhysRead");
    LPCSTR fn3 = AY_OBFUSCATE("PGMPhysWrite");
    LPCSTR fn4 = AY_OBFUSCATE("PGMPhysGCPtr2GCPhys");
    LPCSTR fn1 = AY_OBFUSCATE("VMMGetCpuById");
    HMODULE BstkVMM = GetModuleHandleA(dllName);
    if (BstkVMM == 0) BstkVMM = LoadLibraryA(dllName);
    if (BstkVMM == 0) { diag_log("LoadLibraryAndHook: BstkVMM not found"); return; }

    diag_log("LoadLibraryAndHook: dll loaded");
    VMMGetCpuById = (void* (*)(void*, int))GetProcAddress(BstkVMM, fn1);
    PGMPhysRead = (int (*)(void*, uintptr_t, void*, size_t))GetProcAddress(BstkVMM, fn2);
    PGMPhysWrite = (int (*)(void*, uintptr_t, void*, size_t))GetProcAddress(BstkVMM, fn3);
    PGMPhysGCPtr2GCPhys = (int (*)(void*, uintptr_t, uintptr_t*))GetProcAddress(BstkVMM, fn4);

    // Use MinHook to capture pVM, then immediately remove the hook
    if (PGMPhysRead_Orig == nullptr && PGMPhysRead_Backup[0] == 0) {
        __try {
            // Backup original bytes before hooking
            memcpy(PGMPhysRead_Backup, PGMPhysRead, 16);
            MH_Initialize();
            MH_CreateHook(PGMPhysRead, PGMPhysReadHook, (LPVOID*)&PGMPhysRead_Orig);
            MH_EnableHook(PGMPhysRead);
        } __except(EXCEPTION_EXECUTE_HANDLER) {}
    }

    diag_log("LoadLibraryAndHook: hook installed, waiting for VMM.pVM");
    int waitAttempts = 0;
    while (VMM.pVM == nullptr && waitAttempts < 500) {
        Sleep(10);
        waitAttempts++;
    }
    if (VMM.pVM == nullptr) { diag_log("LoadLibraryAndHook: VMM.pVM timed out!"); return; }
    diag_log("LoadLibraryAndHook: VMM.pVM ready");

    // Restore original PGMPhysRead bytes to remove MinHook trampoline overhead
    if (PGMPhysRead_Backup[0] != 0) {
        DWORD oldProt;
        VirtualProtect((BYTE*)PGMPhysRead, 16, PAGE_EXECUTE_READWRITE, &oldProt);
        memcpy((BYTE*)PGMPhysRead, PGMPhysRead_Backup, 16);
        VirtualProtect((BYTE*)PGMPhysRead, 16, oldProt, &oldProt);
        diag_log("LoadLibraryAndHook: hook removed");
    }
    VMM.GuestCR3 = 1;
}
