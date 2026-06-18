#include "EntityCache.h"
#include "Offsets.h"
#include "Utils.h"
#include "ProcUtils.h"
#include "Scope.h"
#include "../Unity/Vector3.h"
#include <mutex>
#include "../Cfg/Obfuscation.h"

// EntityData is defined in EntityCache.h

// Double-buffer cache: cache thread writes to back, then atomically swaps to front
static std::unordered_map<uintptr_t, EntityData> g_cacheFront;
static std::unordered_map<uintptr_t, EntityData> g_cacheBack;
static std::mutex g_cacheSwapMutex;
UnityMatrix cachedMatrix{};
UnityMatrix renderMatrix{};
std::atomic<uint64_t> renderMatrixTimestamp{ 0 };
std::atomic<uint64_t> renderMatrixGen{ 0 };
int cachedScreenW = 0, cachedScreenH = 0;
uintptr_t cachedLocalPlayer = 0;
uint64_t lastEntityUpdate = 0;
std::atomic<int> g_entityCount{ 0 };

std::unordered_map<uintptr_t, EntityData>& GetEntityCache() { return g_cacheFront; }
std::mutex& GetCacheWriteMutex() { return g_cacheSwapMutex; }

void SwapEntityCache() {
    std::lock_guard<std::mutex> lock(g_cacheSwapMutex);
    g_cacheFront.swap(g_cacheBack);
    g_cacheBack.clear();
    g_entityCount.store((int)g_cacheFront.size());
}

static uintptr_t cachedGE = 0;
static uintptr_t cachedLocal = 0;
static auto engineTimer = std::chrono::steady_clock::now();

// Auto-detected offsets
static uintptr_t g_AutoMainTransform = 0;
static uintptr_t g_AutoAidDocApfka = 0;
static uintptr_t g_AutoCdobmnfcjhd = 0;
static uintptr_t g_AutoCameraChain = 0;
static bool g_AutoDetectDone = false;
static int g_AutoDetectFail = 0;

uintptr_t GetCachedEngine() { return cachedGE; }

static bool LooksLikeTransform(uintptr_t ptr) {
    if (ptr < 0x10000) return false;
    uintptr_t access = Ler<uint32_t>(ptr + 0x8);
    if (access < 0x10000) return false;
    int idx = Ler<int>(access + 0x24);
    if (idx < 0 || idx > 500) return false;
    uintptr_t matrixPtr = Ler<uint32_t>(access + 0x20);
    if (matrixPtr < 0x10000) return false;
    uintptr_t values = Ler<uint32_t>(matrixPtr + 0x18);
    if (values < 0x10000) return false;
    return true;
}

static uintptr_t DetectMainTransform(uintptr_t entity) {
    static const uint32_t cand[] = {0x30,0x34,0x38,0x3C,0x40,0x44,0x48,0x4C,0x50,0x54,0x58,0x5C};
    for (auto off : cand) {
        uintptr_t ptr = Ler<uint32_t>(entity + off);
        if (LooksLikeTransform(ptr)) {
            std::cout << "[Auto] MainTransform found at 0x" << std::hex << off << std::endl;
            return off;
        }
    }
    return 0;
}

static uintptr_t DetectAidDocApfka(uintptr_t entity) {
    static const uint32_t cand[] = {0x750,0x754,0x758,0x75C,0x760,0x764,0x768,0x76C,0x770,0x774,0x778};
    for (auto off : cand) {
        uintptr_t list = Ler<uint32_t>(entity + off);
        if (list > 0x10000) {
            uintptr_t first = Ler<uint32_t>(list + 0x8);
            if (LooksLikeTransform(first)) {
                std::cout << "[Auto] AIDDOCAPFKA found at 0x" << std::hex << off << std::endl;
                return off;
            }
        }
    }
    return 0;
}

static uintptr_t DetectCameraChain(uintptr_t ge) {
    static const uint32_t cand[] = {0x68,0x6C,0x70,0x74,0x78,0x7C,0x80,0x84,0x88,0x8C,0x90};
    for (auto off : cand) {
        uintptr_t ccm = Ler<uint32_t>(ge + off);
        if (ccm < 0x10000) continue;
        uintptr_t cam = Ler<uint32_t>(ccm + 0x10);
        if (cam < 0x10000) continue;
        uintptr_t ic = Ler<uint32_t>(cam + 0x8);
        if (ic < 0x10000) continue;
        UnityMatrix m = Ler<UnityMatrix>(ic + Offsets::ViewMatrix);
        if (m._11 != 0 || m._22 != 0 || m._33 != 0) {
            std::cout << "[Auto] CameraChain found at 0x" << std::hex << off << std::endl;
            return off;
        }
    }
    return 0;
}

bool DetectAndSetOffsets() {
    uintptr_t ge = cachedGE;
    if (ge == 0) return false;

    if (g_AutoCameraChain == 0) {
        uintptr_t camOff = DetectCameraChain(ge);
        if (camOff) g_AutoCameraChain = camOff;
    }

        if (g_AutoMainTransform == 0 || g_AutoAidDocApfka == 0 || g_AutoCdobmnfcjhd == 0) {
        uintptr_t dict = Ler<uint32_t>(ge + Offsets::DictionaryEntities);
        if (dict > 0x10000) {
            uintptr_t list = Ler<uint32_t>(dict + Offsets::Il2CppDictionaryDataPtr);
            if (list > 0x10000) {
                list += 0x10;
                int cnt = Ler<int>(dict + Offsets::Il2CppDictionaryCount);
                if (cnt > 0 && cnt < 200) {
                    for (int i = 0; i < cnt && i < 10; i++) {
                        uintptr_t e = Ler<uint32_t>(list + (i * 0x10) + 0xC);
                        if (e == 0 || e < 0x10000) continue;
                        if (g_AutoMainTransform == 0)
                            g_AutoMainTransform = DetectMainTransform(e);
                        if (g_AutoAidDocApfka == 0)
                            g_AutoAidDocApfka = DetectAidDocApfka(e);
                        if (g_AutoCdobmnfcjhd == 0) {
                            static const uint32_t cdCand[] = {0x7C1,0x7C5,0x7C9,0x7B1,0x7B5,0x7B9,0x7BD,0x7C0,0x7C4,0x7C8,0x7CC};
                            for (auto coff : cdCand) {
                                uintptr_t cv = Ler<uint32_t>(e + coff);
                                if (cv == 0 || cv == 1) { g_AutoCdobmnfcjhd = coff; break; }
                            }
                        }
                        if (g_AutoMainTransform && g_AutoAidDocApfka && g_AutoCdobmnfcjhd) break;
                    }
                }
            }
        }
    }

    g_AutoDetectDone = (g_AutoMainTransform != 0 || g_AutoAidDocApfka != 0 || g_AutoCameraChain != 0);
    if (g_AutoMainTransform) { Offsets::MainTransform = g_AutoMainTransform; std::cout << "[Auto] MainTransform <- 0x" << std::hex << g_AutoMainTransform << std::endl; }
    if (g_AutoAidDocApfka) { Offsets::AIDDOCAPFKA = g_AutoAidDocApfka; std::cout << "[Auto] AIDDOCAPFKA <- 0x" << std::hex << g_AutoAidDocApfka << std::endl; }
    if (g_AutoCdobmnfcjhd) { Offsets::CDOBMFNCJHD = g_AutoCdobmnfcjhd; std::cout << "[Auto] CDOBMFNCJHD <- 0x" << std::hex << g_AutoCdobmnfcjhd << std::endl; }
    return g_AutoDetectDone;
}

static uintptr_t g_AutoInitBase = 0;
static std::chrono::steady_clock::time_point g_AutoInitBaseScanStart;
static bool g_AutoInitBaseScanning = false;

static uintptr_t ScanInitBase() {
    if (il2cpp < 0x10000) return 0;
    // Only scan a few key ranges, time-boxed
    static const uint32_t ranges[][2] = {
        {0x9EC1800, 0x9EC2000},  // F5 dump area (common)
        {0xA110000, 0xA120000},  // around old InitBase
        {0x9EB0000, 0x9ED0000},  // wider around F5 area
        {0xA100000, 0xA130000},  // wider around InitBase
        {0x9E00000, 0x9F00000},  // full scan area
        {0xA000000, 0xA300000},
    };
    g_AutoInitBaseScanning = true;
    g_AutoInitBaseScanStart = std::chrono::steady_clock::now();
    for (int r = 0; r < sizeof(ranges)/sizeof(ranges[0]); r++) {
        auto elapsed = std::chrono::steady_clock::now() - g_AutoInitBaseScanStart;
        if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() > 5000) break; // max 5s total
        uint32_t start = ranges[r][0], end = ranges[r][1];
        for (uint32_t off = start; off < end; off += 4) {
            uintptr_t base = Ler<uint32_t>(il2cpp + off);
            if (base > 0x10000 && base < 0xFFF00000) {
                uintptr_t facade = Ler<uint32_t>(base);
                if (facade > 0x10000 && facade < 0xFFF00000) {
                    uintptr_t sf = Ler<uint32_t>(facade + Offsets::StaticClass);
                    if (sf > 0x10000 && sf < 0xFFF00000) {
                        uintptr_t eng = Ler<uint32_t>(sf);
                        if (eng > 0x10000 && eng < 0xFFF00000) {
                            std::cout << "[Auto] InitBase candidate at 0x" << std::hex << off << " eng=0x" << eng << std::endl;
                            g_AutoInitBaseScanning = false;
                            return off;
                        }
                    }
                }
            }
        }
    }
    std::cout << "[Auto] InitBase scan finished, not found" << std::endl;
    g_AutoInitBaseScanning = false;
    return 0;
}

static int g_EngineFailCount = 0;
static uintptr_t GetEngine() {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - engineTimer).count() > 500) {
        engineTimer = now;
        if (il2cpp < 0x10000) { cachedGE = 0; return 0; }
        uintptr_t initOff = g_AutoInitBase ? g_AutoInitBase : Offsets::InitBase;
        uintptr_t base = Ler<uint32_t>(il2cpp + initOff);
        if (base > 0x10000) {
            uintptr_t facade = Ler<uint32_t>(base);
            if (facade > 0x10000) {
                uintptr_t sf = Ler<uint32_t>(facade + Offsets::StaticClass);
                if (sf > 0x10000) cachedGE = Ler<uint32_t>(sf);
            }
        }
    }
    return cachedGE;
}

static uintptr_t GetLocal(uintptr_t ge) {
    if (ge == 0) return 0;
    uintptr_t m = Ler<uint32_t>(ge + Offsets::CurrentMatch);
    if (m == 0) { cachedLocal = 0; return 0; }
    if (Ler<int>(m + Offsets::MatchStatus) != 1) { cachedLocal = 0; return 0; }
    if (cachedLocal != 0) return cachedLocal;
    cachedLocal = Ler<uint32_t>(m + Offsets::LocalPlayer);
    return cachedLocal;
}

static std::thread cacheThread;
static std::atomic<bool> cacheRunning{ false };

static void CacheLoop() {
    std::cout << "[CacheLoop] INICIO" << std::endl;
    JUNK(); JUNK_FALSE(); AntiDebugCheck();
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll) {
        typedef NTSTATUS(NTAPI *NtSIT)(HANDLE,ULONG,PVOID,ULONG);
        NtSIT pNtSIT = (NtSIT)GetProcAddress(ntdll,"NtSetInformationThread");
        if (pNtSIT) pNtSIT(GetCurrentThread(),0x11,NULL,0);
    }
    uint64_t lastCleanup = 0;
    int failCount = 0;
    while (cacheRunning) {
        uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        if (now - lastEntityUpdate < 16) { std::this_thread::sleep_for(std::chrono::milliseconds(3)); continue; }

        uintptr_t ge = GetEngine(); if (ge == 0) {
            failCount++;
            if (failCount > 120) {
                std::cout << "[CacheLoop] failCount>120, engine still not ready" << std::endl;
                failCount = 60;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50)); continue;
        }
        if (failCount > 0) std::cout << "[CacheLoop] GetEngine OK! ge=" << std::hex << ge << std::endl;
        failCount = 0;

        uintptr_t camOff = g_AutoCameraChain ? g_AutoCameraChain : string2Offset(AY_OBFUSCATE("0x74"));
        uintptr_t ccm = Ler<uint32_t>(ge + camOff);
        if (ccm > 0x10000) {
            uintptr_t cam = Ler<uint32_t>(ccm + string2Offset(AY_OBFUSCATE("0x10")));
            if (cam > 0x10000) {
                uintptr_t ic = Ler<uint32_t>(cam + string2Offset(AY_OBFUSCATE("0x8")));
                if (ic > 0x10000) {
                    cachedMatrix = Ler<UnityMatrix>(ic + Offsets::ViewMatrix);
                    cachedScreenW = SWidth; cachedScreenH = SHeight;
                    uint64_t gen = renderMatrixGen.load(std::memory_order_relaxed);
                    renderMatrixGen.store(gen + 1, std::memory_order_release);
                    renderMatrix = cachedMatrix;
                    renderMatrixTimestamp.store(now, std::memory_order_release);
                    renderMatrixGen.store(gen + 2, std::memory_order_release);
                }
            }
        }

        cachedLocalPlayer = GetLocal(ge);
        if (cachedLocalPlayer == 0) {
            {
                std::lock_guard<std::mutex> lock(g_cacheSwapMutex);
                g_cacheBack.clear();
                g_cacheFront.clear();
                g_entityCount.store(0);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        lastEntityUpdate = now;

        if (now - lastCleanup > 5000) {
            lastCleanup = now;
            std::lock_guard<std::mutex> lock(g_cacheSwapMutex);
            for (auto it = g_cacheBack.begin(); it != g_cacheBack.end();)
                if (now - it->second.lastUpdate > 10000) it = g_cacheBack.erase(it); else ++it;
        }

        uintptr_t dict = Ler<uint32_t>(ge + Offsets::DictionaryEntities); if (dict == 0) { g_cacheBack.clear(); SwapEntityCache(); continue; }
        uintptr_t list = Ler<uint32_t>(dict + Offsets::Il2CppDictionaryDataPtr); if (list == 0) { g_cacheBack.clear(); SwapEntityCache(); continue; }
        list += 0x10;
        int cnt = Ler<int>(dict + Offsets::Il2CppDictionaryCount); if (cnt < 1 || cnt > 200) { g_cacheBack.clear(); SwapEntityCache(); continue; }

        if (!g_AutoDetectDone) DetectAndSetOffsets();

        uintptr_t mtOff = g_AutoMainTransform ? g_AutoMainTransform : Offsets::MainTransform;
        Vector3 myPos = Transform_ObterPosicao(Ler<uint32_t>(cachedLocalPlayer + mtOff));

    for (int i = 0; i < cnt; i++) {
        uintptr_t e = Ler<uint32_t>(list + (i * 0x10) + 0xC);
        if (e == 0 || e == cachedLocalPlayer) continue;

        EntityData fr;
        uintptr_t am = Ler<uint32_t>(e + Offsets::AvatarManager); if (am == 0 || am < 0x10000) continue;
        uintptr_t uas = Ler<uint32_t>(am + Offsets::UmaAvatarSimple); if (uas == 0 || uas < 0x10000) continue;
        uintptr_t ud = Ler<uint32_t>(uas + Offsets::UMAData); if (ud == 0 || ud < 0x10000) continue;
        uintptr_t pri = Ler<uint32_t>(e + Offsets::PRIDataPool); if (pri == 0 || pri < 0x10000) continue;
        uintptr_t rdu = Ler<uint32_t>(Ler<uint32_t>(pri + Offsets::ReplicationDataPoolUnsafe) + Offsets::ReplicationDataUnsafe); if (rdu == 0 || rdu < 0x10000) continue;
        fr.health = Ler<short>(rdu + Offsets::Health); if (fr.health <= 0) continue;

        fr.address = e;
        fr.dying = false;
        fr.garota = Ler<bool>(e + Offsets::CDOBMFNCJHD);
        uintptr_t pd = Ler<uint32_t>(e + Offsets::Player_Data);
        if (pd != 0 && pd > 0x10000) fr.dying = (Ler<int>(pd + Offsets::Player_IsDead) == 8);
        fr.isTeam = Ler<bool>(ud + Offsets::TeamMate);

        fr.headPos = GetHeadPosition(e); if (fr.headPos.X == 0 && fr.headPos.Y == 0 && fr.headPos.Z == 0) continue;
        fr.bodyPos = GetPlayerPosition(e, 0);
        fr.dist = Vector3::Distance(fr.bodyPos, myPos);

        Vector3 sb = World2Screen(cachedMatrix, fr.bodyPos);
        Vector3 sh = World2Screen(cachedMatrix, fr.headPos);
        if (sb.Z != 0 || sh.Z != 0) continue;
        fr.screenBody = sb; fr.screenHead = sh;

        auto bpi = Ler<uint32_t>(e + Offsets::Player_Name);
        if (bpi != 0) {
            auto pn = Ler<uint32_t>(bpi + string2Offset(AY_OBFUSCATE("0x18")));
            if (pn != 0) {
                int nc = Ler<int>(pn + string2Offset(AY_OBFUSCATE("0x8")));
                fr.name = ObterStr(pn + string2Offset(AY_OBFUSCATE("0xC")), nc);
            }
        }

        {
            uintptr_t wpn = Ler<uint32_t>(e + Offsets::Weapon);
            if (wpn > 0x10000) {
                uintptr_t wpnd = Ler<uint32_t>(wpn + Offsets::WeaponData);
                if (wpnd > 0x10000) {
                    auto wpnBpi = Ler<uint32_t>(wpnd + string2Offset(AY_OBFUSCATE("0x8")));
                    if (wpnBpi != 0) {
                        auto wpnPn = Ler<uint32_t>(wpnBpi + string2Offset(AY_OBFUSCATE("0x18")));
                        if (wpnPn != 0) {
                            int wpnNc = Ler<int>(wpnPn + string2Offset(AY_OBFUSCATE("0x8")));
                            fr.weaponName = ObterStr(wpnPn + string2Offset(AY_OBFUSCATE("0xC")), wpnNc);
                        }
                    }
                }
            }
        }

        fr.hasBones = false;
        if (ESPEsqueleto) {
            static const uintptr_t maleBO[18] = {
                string2Offset(AY_OBFUSCATE("0x38")),string2Offset(AY_OBFUSCATE("0x14")),string2Offset(AY_OBFUSCATE("0x10")),string2Offset(AY_OBFUSCATE("0x48")),
                string2Offset(AY_OBFUSCATE("0x18")),string2Offset(AY_OBFUSCATE("0x1C")),string2Offset(AY_OBFUSCATE("0x20")),string2Offset(AY_OBFUSCATE("0x24")),
                string2Offset(AY_OBFUSCATE("0x28")),string2Offset(AY_OBFUSCATE("0x2C")),string2Offset(AY_OBFUSCATE("0x30")),string2Offset(AY_OBFUSCATE("0x34")),
                string2Offset(AY_OBFUSCATE("0x3C")),string2Offset(AY_OBFUSCATE("0x40")),string2Offset(AY_OBFUSCATE("0x44")),string2Offset(AY_OBFUSCATE("0x4C")),
                string2Offset(AY_OBFUSCATE("0x50")),string2Offset(AY_OBFUSCATE("0x54"))
            };
            static const uintptr_t femaleBO[18] = {
                string2Offset(AY_OBFUSCATE("0x3C")),string2Offset(AY_OBFUSCATE("0x18")),string2Offset(AY_OBFUSCATE("0x14")),string2Offset(AY_OBFUSCATE("0x10")),
                string2Offset(AY_OBFUSCATE("0x1C")),string2Offset(AY_OBFUSCATE("0x20")),string2Offset(AY_OBFUSCATE("0x24")),string2Offset(AY_OBFUSCATE("0x28")),
                string2Offset(AY_OBFUSCATE("0x2C")),string2Offset(AY_OBFUSCATE("0x30")),string2Offset(AY_OBFUSCATE("0x34")),string2Offset(AY_OBFUSCATE("0x38")),
                string2Offset(AY_OBFUSCATE("0x40")),string2Offset(AY_OBFUSCATE("0x44")),string2Offset(AY_OBFUSCATE("0x48")),string2Offset(AY_OBFUSCATE("0x4C")),
                string2Offset(AY_OBFUSCATE("0x50")),string2Offset(AY_OBFUSCATE("0x54"))
            };
            const uintptr_t* bo = fr.garota ? femaleBO : maleBO;
            fr.hasBones = true;
            for (int j = 0; j < 18; j++) {
                Vector3 wpos = ObterOssos(e, bo[j]);
                fr.boneWorld[j] = wpos;
                fr.bones[j] = World2Screen(cachedMatrix, wpos);
            }
        }

        fr.valid = true;
        fr.lastUpdate = now;
        g_cacheBack[e] = fr;
    }

        if (!g_cacheBack.empty()) {
            uint64_t flickerGuard = now - 100;
            for (auto& [addr, old] : g_cacheFront) {
                if (old.valid && old.lastUpdate > flickerGuard && old.health > 0) {
                    if (g_cacheBack.find(addr) == g_cacheBack.end()) {
                        g_cacheBack[addr] = old;
                    }
                }
            }
        }

        SwapEntityCache();
    }
}

void UpdateEntityCache() {
    if (!cacheRunning) {
        cacheRunning = true;
        cacheThread = std::thread(CacheLoop);
    }
}

void StopEntityCache() {
    cacheRunning = false;
    if (cacheThread.joinable()) cacheThread.join();
}
