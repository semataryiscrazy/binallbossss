#include "PrecisionMode.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <vector>
#include "../Unity/Vector3.h"
#include "Offsets.h"
#include "Utils.h"
#include "Scope.h"
#include "EntityCache.h"

namespace _0xPrecision {

    static bool wasActive = false;
    static uint32_t lastWd = 0;
    static auto g_lastTick = std::chrono::steady_clock::now();

    static bool ValidatePtr(uint32_t addr) {
        return addr > 0x10000 && addr < 0x7FFFFFFF;
    }

    static bool GetWeaponData(uint32_t& weaponData) {
        if (!Auth.Attached || !cachedLocalPlayer || !ValidatePtr(static_cast<uint32_t>(cachedLocalPlayer)))
            return false;
        uint32_t weapon = Ler<uint32_t>(static_cast<uint32_t>(cachedLocalPlayer) + Offsets::Weapon);
        if (!ValidatePtr(weapon)) return false;
        weaponData = Ler<uint32_t>(weapon + Offsets::WeaponData);
        return ValidatePtr(weaponData);
    }

    void Tick() {
        auto now = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_lastTick).count();
        if (ms < 250) return;
        g_lastTick = now;

        bool enabled = PrecisionMode;
        uint32_t wd = 0;
        if (!GetWeaponData(wd)) {
            wasActive = false;
            return;
        }

        if (enabled && (!wasActive || wd != lastWd)) {
            Escrever<float>(wd + Offsets::WD_Damage, 55.0f);
            Escrever<float>(wd + Offsets::WD_FireInterval, 0.05f);
            Escrever<float>(wd + 0x10, 0.0f);
            Escrever<float>(wd + Offsets::WD_Range, 100.0f);
        } else if (!enabled && wasActive) {
            if (wd == lastWd) {
                Escrever<float>(wd + Offsets::WD_Damage, 15.0f);
                Escrever<float>(wd + Offsets::WD_FireInterval, 0.12f);
                Escrever<float>(wd + 0x10, 0.0f);
                Escrever<float>(wd + Offsets::WD_Range, 10.0f);
            }
        }

        wasActive = enabled;
        lastWd = wd;
    }

    void Start() {}
    void Stop() {}
}
