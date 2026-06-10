#include "EntityCache.h"
#include "Offsets.h"
#include "Utils.h"
#include "Process.h"
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

static uintptr_t GetEngine() {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - engineTimer).count() > 500) {
        engineTimer = now;
        cachedGE = 0;
        if (il2cpp < 0x10000) return 0;
        uintptr_t base = Ler<uintptr_t>(il2cpp + Offsets::InitBase);
        if (base != 0 && base > 0x10000) {
            uintptr_t facade = Ler<uintptr_t>(base);
            if (facade != 0 && facade > 0x10000) {
                uintptr_t sf = Ler<uintptr_t>(facade + Offsets::StaticClass);
                if (sf != 0 && sf > 0x10000) cachedGE = Ler<uintptr_t>(sf);
            }
        }
    }
    return cachedGE;
}

static uintptr_t GetLocal(uintptr_t ge) {
    if (ge == 0) return 0;
    uintptr_t m = Ler<uintptr_t>(ge + Offsets::CurrentMatch);
    if (m == 0) { cachedLocal = 0; return 0; }
    if (Ler<int>(m + Offsets::MatchStatus) != 1) { cachedLocal = 0; return 0; }
    if (cachedLocal != 0) return cachedLocal;
    cachedLocal = Ler<uintptr_t>(m + Offsets::LocalPlayer);
    return cachedLocal;
}

static std::thread cacheThread;
static std::atomic<bool> cacheRunning{ false };

static void CacheLoop() {
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
            if (failCount > 60) { // ~3s sem engine = conexao perdida
                Auth.Attached = false;
                failCount = 0;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50)); continue;
        }
        failCount = 0;

        // Atualiza camera matrix + copia atomica para render
        uintptr_t ccm = Ler<uintptr_t>(ge + 0x74);
        if (ccm > 0x10000) {
            uintptr_t cam = Ler<uintptr_t>(ccm + string2Offset(AY_OBFUSCATE("0x10")));
            if (cam > 0x10000) {
                uintptr_t ic = Ler<uintptr_t>(cam + string2Offset(AY_OBFUSCATE("0x8")));
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
            // Partida encerrou — limpa cache pra ESP nao ficar travado
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

        // Cleanup stale entries a cada 5s (nao bloqueia swap)
        if (now - lastCleanup > 5000) {
            lastCleanup = now;
            std::lock_guard<std::mutex> lock(g_cacheSwapMutex);
            for (auto it = g_cacheBack.begin(); it != g_cacheBack.end();)
                if (now - it->second.lastUpdate > 10000) it = g_cacheBack.erase(it); else ++it;
        }

        uintptr_t dict = Ler<uintptr_t>(ge + Offsets::DictionaryEntities); if (dict == 0) { g_cacheBack.clear(); SwapEntityCache(); continue; }
        uintptr_t list = Ler<uintptr_t>(dict + Offsets::Il2CppDictionaryDataPtr); if (list == 0) { g_cacheBack.clear(); SwapEntityCache(); continue; }
        list += 0x10;
        int cnt = Ler<int>(dict + Offsets::Il2CppDictionaryCount); if (cnt < 1 || cnt > 200) { g_cacheBack.clear(); SwapEntityCache(); continue; }

        Vector3 myPos = Transform_ObterPosicao(Ler<uintptr_t>(cachedLocalPlayer + Offsets::MainTransform));

    for (int i = 0; i < cnt; i++) {
        uintptr_t e = Ler<uintptr_t>(list + (i * 0x10) + 0xC);
        if (e == 0 || e == cachedLocalPlayer) continue;

        EntityData fr;
        uintptr_t am = Ler<uintptr_t>(e + Offsets::AvatarManager); if (am == 0 || am < 0x10000) continue;
        uintptr_t uas = Ler<uintptr_t>(am + Offsets::UmaAvatarSimple); if (uas == 0 || uas < 0x10000) continue;
        uintptr_t ud = Ler<uintptr_t>(uas + Offsets::UMAData); if (ud == 0 || ud < 0x10000) continue;
        if (Ler<int>(e + string2Offset(AY_OBFUSCATE("0xC1C"))) == 0)
            if (!Ler<bool>(uas + Offsets::Avatar_IsVisible)) continue;
        uintptr_t pri = Ler<uintptr_t>(e + Offsets::PRIDataPool); if (pri == 0 || pri < 0x10000) continue;
        uintptr_t rdu = Ler<uintptr_t>(Ler<uintptr_t>(pri + Offsets::ReplicationDataPoolUnsafe) + Offsets::ReplicationDataUnsafe); if (rdu == 0 || rdu < 0x10000) continue;
        fr.health = Ler<short>(rdu + Offsets::Health); if (fr.health <= 0) continue;

        fr.address = e;
        fr.dying = false;
        fr.garota = Ler<bool>(e + Offsets::CDOBMFNCJHD);
        uintptr_t pd = Ler<uintptr_t>(e + Offsets::Player_Data);
        if (pd != 0 && pd > 0x10000) fr.dying = (Ler<int>(pd + Offsets::Player_IsDead) == 8);
        fr.isTeam = Ler<bool>(ud + Offsets::TeamMate);

        fr.headPos = GetHeadPosition(e); if (fr.headPos.X == 0 && fr.headPos.Y == 0 && fr.headPos.Z == 0) continue;
        fr.bodyPos = GetPlayerPosition(e, 0);
        fr.dist = Vector3::Distance(fr.bodyPos, myPos);

        Vector3 sb = World2Screen(cachedMatrix, fr.bodyPos);
        Vector3 sh = World2Screen(cachedMatrix, fr.headPos);
        if (sb.Z != 0 || sh.Z != 0) continue;
        fr.screenBody = sb; fr.screenHead = sh;

        auto bpi = Ler<uintptr_t>(e + Offsets::Player_Name);
        if (bpi != 0) {
            auto pn = Ler<uintptr_t>(bpi + string2Offset(AY_OBFUSCATE("0x18")));
            if (pn != 0) {
                int nc = Ler<int>(pn + string2Offset(AY_OBFUSCATE("0x8")));
                fr.name = ObterStr(pn + string2Offset(AY_OBFUSCATE("0xC")), nc);
            }
        }

        // Weapon name
        {
            uintptr_t wpn = Ler<uintptr_t>(e + Offsets::Weapon);
            if (wpn > 0x10000) {
                uintptr_t wpnd = Ler<uintptr_t>(wpn + Offsets::WeaponData);
                if (wpnd > 0x10000) {
                    auto wpnBpi = Ler<uintptr_t>(wpnd + string2Offset(AY_OBFUSCATE("0x8")));
                    if (wpnBpi != 0) {
                        auto wpnPn = Ler<uintptr_t>(wpnBpi + string2Offset(AY_OBFUSCATE("0x18")));
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

        // Anti-flicker: keep entities valid within 100ms, but don't refresh lastUpdate
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

        // Swap back to front atomically
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
