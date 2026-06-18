#pragma once
#include <cstdint>
#include <unordered_map>

class WeaponAttributes {
public:
    static void Apply(uint32_t localPlayer, int level, bool enabled);
    static void RestoreAll();

private:
    struct WeaponOriginalValues {
        float fireInterval;
        float repeatFireInterval;
        float multiFireInterval;
        float addFireSpeed;
    };

    static std::unordered_map<uint32_t, WeaponOriginalValues> restoredWeapons;
    static uint32_t lastWeaponAddr;
    static int lastLevel;
    static bool wasEnabled;

    static float GetFireMultiplier(int level);
    static void ApplyToWeapon(uint32_t localPlayer, int level);
    static void ApplyGlobalScales(uint32_t localPlayer, int level);
    static void RestoreGlobalScales(uint32_t localPlayer);
};
