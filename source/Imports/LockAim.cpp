#include "LockAim.h"
#include <atomic>
#include <chrono>
#include <windows.h>
#include "Offsets.h"
#include "Utils.h"
#include "Scope.h"

namespace LockAim {

    static std::atomic<uintptr_t> g_target{ 0 };
    static std::chrono::steady_clock::time_point kpt;
    static bool kp = false, dc = false;
    static uintptr_t ct = 0;
    static auto g_lastTick = std::chrono::steady_clock::now();

    static void Disable(uintptr_t tg) {
        if (tg == 0) return;
        Escrever<uint32_t>((uint32_t)(tg + Offsets::ColliderINICDNFOFJB), 0u);
    }

    void Tick() {
        auto now = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_lastTick).count();

        if (!Auth.Attached) {
            if (ms >= 100) g_lastTick = now;
            return;
        }

        if (!AimbotLegit) {
            if (ct) { Disable(ct); ct = 0; }
            if (ms >= 100) { kp = false; dc = false; g_lastTick = now; }
            return;
        }

        bool noKey = (AimbotKeyBind == 0);
        bool kh = noKey || (GetAsyncKeyState(AimbotKeyBind) & 0x8000) != 0;

        if (kh && !kp) { kpt = now; kp = true; dc = false; g_lastTick = now; return; }
        else if (!kh && kp) {
            kp = false; dc = false;
            if (ct) { Disable(ct); ct = 0; }
            if (ms >= 16) g_lastTick = now;
            return;
        }

        if (!kp) {
            if (ms >= 16) g_lastTick = now;
            return;
        }

        // Delay configuravel
        if (!dc) {
            static const int delays[] = { 0, 235, 325, 415 };
            int ix = (AimbotPeitosIndex < 0) ? 0 : (AimbotPeitosIndex > 3) ? 3 : AimbotPeitosIndex;
            auto el = std::chrono::duration_cast<std::chrono::milliseconds>(now - kpt).count();
            if (el >= delays[ix]) { dc = true; g_lastTick = now; }
            else { if (ms >= 8) g_lastTick = now; return; }
        }

        g_lastTick = now;

        uintptr_t tg = g_target.load();
        if (!tg) {
            if (ct) { Disable(ct); ct = 0; }
            return;
        }

        if (ct && ct != tg) Disable(ct);
        ct = tg;

        uintptr_t hca = tg + Offsets::ColliderHECFNHJKOMN;
        uintptr_t laa = tg + Offsets::ColliderINICDNFOFJB;
        if (hca < 0x10000 || laa < 0x10000)
            return;

        uint32_t hc = Ler<uint32_t>((uint32_t)hca);
        if (hc == 0)
            return;

        uint32_t cr = Ler<uint32_t>((uint32_t)laa);
        if (cr != hc) {
            Escrever<uint32_t>((uint32_t)laa, 0u);
            Escrever<uint32_t>((uint32_t)laa, hc);
        }
    }

    void Start() {}
    void Stop() {}
    void SetTarget(uintptr_t tg) { g_target.store(tg); }
}
