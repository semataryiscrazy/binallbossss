#include "PrecisionMode.h"
#include <thread>
#include <atomic>
#include <chrono>
#include <unordered_map>
#include <vector>
#include "Offsets.h"
#include "Utils.h"
#include "Scope.h"
#include "EntityCache.h"

namespace _0xPrecision {

    static std::atomic<bool> running{ false };
    static std::thread thread;
    static bool wasActive = false;
    static uint32_t lastWeaponAddr = 0;

    struct WeaponOrig {
        float damage, damageInc, fireInterval, repeatFire, multiFire, range;
        float damageHead, damageLimb, prefireDelay, spread;
        float paramsFire, paramsRepeat, paramsMulti;
    };
    static std::unordered_map<uint32_t, WeaponOrig> g_originals;

    static bool ValidatePtr(uint32_t addr) {
        return addr > 0x10000 && addr < 0x7FFFFFFF;
    }

    static bool GetWeaponChain(uint32_t& weapon, uint32_t& weaponData, uint32_t& weaponParams) {
        uint32_t lp = static_cast<uint32_t>(cachedLocalPlayer);
        if (!ValidatePtr(lp)) return false;

        weapon = Ler<uint32_t>(lp + Offsets::Weapon);
        if (!ValidatePtr(weapon)) return false;

        weaponData = Ler<uint32_t>(weapon + Offsets::WeaponData);
        if (!ValidatePtr(weaponData)) return false;

        weaponParams = Ler<uint32_t>(weaponData + Offsets::WeaponParams);
        return ValidatePtr(weaponParams);
    }

    static void SaveOriginal(uint32_t wd, uint32_t wp) {
        if (g_originals.find(wd) != g_originals.end()) return;
        WeaponOrig o;
        o.damage = Ler<float>(wd + Offsets::WD_Damage);
        o.damageInc = Ler<float>(wd + Offsets::WD_DamageIncrease);
        o.fireInterval = Ler<float>(wd + Offsets::WD_FireInterval);
        o.repeatFire = Ler<float>(wd + Offsets::WD_RepeatFireInterval);
        o.multiFire = Ler<float>(wd + Offsets::WD_MultiFireInterval);
        o.range = Ler<float>(wd + Offsets::WD_Range);
        o.damageHead = Ler<float>(wd + Offsets::WD_DamageHead);
        o.damageLimb = Ler<float>(wd + Offsets::WD_DamageLimb);
        o.prefireDelay = Ler<float>(wd + Offsets::WD_PrefireDelay);
        o.spread = Ler<float>(wd + 0x10);
        o.paramsFire = Ler<float>(wp + Offsets::WeaponParams_FireInterval);
        o.paramsRepeat = Ler<float>(wp + Offsets::WeaponParams_RepeatFireInterval);
        o.paramsMulti = Ler<float>(wp + Offsets::WeaponParams_MultiFireInterval);
        g_originals[wd] = o;
    }

    static void RestoreWeapon(uint32_t wd, uint32_t wp) {
        auto it = g_originals.find(wd);
        if (it == g_originals.end()) return;
        auto& o = it->second;
        Escrever<float>(wd + Offsets::WD_Damage, o.damage);
        Escrever<float>(wd + Offsets::WD_DamageIncrease, o.damageInc);
        Escrever<float>(wd + Offsets::WD_FireInterval, o.fireInterval);
        Escrever<float>(wd + Offsets::WD_RepeatFireInterval, o.repeatFire);
        Escrever<float>(wd + Offsets::WD_MultiFireInterval, o.multiFire);
        Escrever<float>(wd + Offsets::WD_Range, o.range);
        Escrever<float>(wd + Offsets::WD_DamageHead, o.damageHead);
        Escrever<float>(wd + Offsets::WD_DamageLimb, o.damageLimb);
        Escrever<float>(wd + Offsets::WD_PrefireDelay, o.prefireDelay);
        Escrever<float>(wd + 0x10, o.spread);
        Escrever<float>(wp + Offsets::WeaponParams_FireInterval, o.paramsFire);
        Escrever<float>(wp + Offsets::WeaponParams_RepeatFireInterval, o.paramsRepeat);
        Escrever<float>(wp + Offsets::WeaponParams_MultiFireInterval, o.paramsMulti);
    }

    static void RestoreAll() {
        for (auto& [wd, o] : g_originals) {
            uint32_t wp = Ler<uint32_t>(wd + Offsets::WeaponParams);
            if (ValidatePtr(wp)) {
                Escrever<float>(wd + Offsets::WD_Damage, o.damage);
                Escrever<float>(wd + Offsets::WD_DamageIncrease, o.damageInc);
                Escrever<float>(wd + Offsets::WD_FireInterval, o.fireInterval);
                Escrever<float>(wd + Offsets::WD_RepeatFireInterval, o.repeatFire);
                Escrever<float>(wd + Offsets::WD_MultiFireInterval, o.multiFire);
                Escrever<float>(wd + Offsets::WD_Range, o.range);
                Escrever<float>(wd + Offsets::WD_DamageHead, o.damageHead);
                Escrever<float>(wd + Offsets::WD_DamageLimb, o.damageLimb);
                Escrever<float>(wd + Offsets::WD_PrefireDelay, o.prefireDelay);
                Escrever<float>(wd + 0x10, o.spread);
                Escrever<float>(wp + Offsets::WeaponParams_FireInterval, o.paramsFire);
                Escrever<float>(wp + Offsets::WeaponParams_RepeatFireInterval, o.paramsRepeat);
                Escrever<float>(wp + Offsets::WeaponParams_MultiFireInterval, o.paramsMulti);
            }
        }
        g_originals.clear();
    }

    static void ApplyPrecision(uint32_t wd, uint32_t wp) {
        float z = 0.0f;
        Escrever<float>(wd + Offsets::WD_Damage, 65.0f);
        Escrever<float>(wd + Offsets::WD_DamageIncrease, 25.0f);
        Escrever<float>(wd + Offsets::WD_FireInterval, 0.04f);
        Escrever<float>(wd + Offsets::WD_RepeatFireInterval, 0.04f);
        Escrever<float>(wd + Offsets::WD_MultiFireInterval, 0.04f);
        Escrever<float>(wd + Offsets::WD_Range, 300.0f);
        Escrever<float>(wd + Offsets::WD_DamageHead, 200.0f);
        Escrever<float>(wd + Offsets::WD_DamageLimb, 65.0f);
        Escrever<float>(wd + Offsets::WD_PrefireDelay, z);
        Escrever<float>(wd + 0x10, z);
        Escrever<float>(wp + Offsets::WeaponParams_FireInterval, 0.04f);
        Escrever<float>(wp + Offsets::WeaponParams_RepeatFireInterval, 0.04f);
        Escrever<float>(wp + Offsets::WeaponParams_MultiFireInterval, 0.04f);
    }

    static void Run() {
        while (running) {
            try {
                bool en = PrecisionMode;
                uint32_t weapon = 0, weaponData = 0, weaponParams = 0;
                bool haveChain = GetWeaponChain(weapon, weaponData, weaponParams);

                if (en) {
                    if (!haveChain) {
                        wasActive = false;
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        continue;
                    }

                    if (weaponData != lastWeaponAddr) {
                        if (lastWeaponAddr != 0)
                            RestoreWeapon(lastWeaponAddr, Ler<uint32_t>(lastWeaponAddr + Offsets::WeaponParams));
                        lastWeaponAddr = weaponData;
                    }

                    SaveOriginal(weaponData, weaponParams);
                    ApplyPrecision(weaponData, weaponParams);

                    uint32_t attr = Ler<uint32_t>(static_cast<uint32_t>(cachedLocalPlayer) + Offsets::PlayerAttributes);
                    if (ValidatePtr(attr))
                        Escrever<float>(attr + Offsets::PlayerAttributes_FireIntervalScale, 0.3f);

                    wasActive = true;
                } else {
                    if (wasActive) {
                        if (lastWeaponAddr) {
                            uint32_t wp = Ler<uint32_t>(lastWeaponAddr + Offsets::WeaponParams);
                            if (ValidatePtr(wp))
                                RestoreWeapon(lastWeaponAddr, wp);
                        }
                        RestoreAll();
                        lastWeaponAddr = 0;
                        wasActive = false;

                        uint32_t attr = Ler<uint32_t>(static_cast<uint32_t>(cachedLocalPlayer) + Offsets::PlayerAttributes);
                        if (ValidatePtr(attr))
                            Escrever<float>(attr + Offsets::PlayerAttributes_FireIntervalScale, 1.0f);
                    }
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            } catch (...) {
                continue;
            }
        }
    }

    void Start() {
        if (running) return;
        running = true;
        thread = std::thread(Run);
    }

    void Stop() {
        if (!running) return;
        running = false;
        if (thread.joinable()) thread.join();
        RestoreAll();
    }
}
