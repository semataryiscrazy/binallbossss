#include "SilentAim.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include "../Unity/Vector3.h"
#include "Offsets.h"
#include "Utils.h"
#include "Scope.h"
#include "Process.h"

namespace _0xW3X4Y5Z6 {

    static std::atomic<uintptr_t> g_target{ 0 };
    static auto g_lastTick = std::chrono::steady_clock::now();

    void Tick() {
        if (!Auth.Attached || !cachedLocalPlayer)
            return;

        auto now = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_lastTick).count();

        if (!AimSilent) {
            if (ms >= 50) g_lastTick = now;
            return;
        }

        bool noKey = (SilentAimKeyBind == 0);
        if (!noKey && (GetAsyncKeyState(SilentAimKeyBind) & 0x8000) == 0) {
            if (ms >= 1) g_lastTick = now;
            return;
        }

        g_lastTick = now;

        uintptr_t tg = g_target.load();
        if (tg == 0 || tg < 0x10000)
            return;

        uint32_t weaponBase = Ler<uint32_t>(cachedLocalPlayer + Offsets::Sillent);
        if (weaponBase == 0 || weaponBase < 0x10000)
            return;

        Vector3 targetPos;
        {
            std::lock_guard<std::mutex> lock(GetCacheWriteMutex());
            auto& cache = GetEntityCache();
            auto it = cache.find((uintptr_t)tg);
            if (it == cache.end() || !it->second.valid)
                return;
            if (SilentAimHitbox == 0)
                targetPos = it->second.headPos;
            else
                targetPos = it->second.bodyPos;
        }

        Vector3 startPos = Ler<Vector3>(weaponBase + Offsets::AimInfo_StartPos);
        if (startPos.X == 0 && startPos.Y == 0 && startPos.Z == 0)
            return;

        Vector3 dir = targetPos - startPos;
        float mag = sqrtf(dir.X * dir.X + dir.Y * dir.Y + dir.Z * dir.Z);
        if (mag < 0.001f) return;
        dir.X /= mag; dir.Y /= mag; dir.Z /= mag;

        Escrever<Vector3>(weaponBase + Offsets::AimInfo_RayDir, dir);
    }

    void Start() {}
    void Stop() {}
    void SetTarget(uintptr_t tg) { g_target.store(tg); }
}
