#pragma once
#include <cstdint>
#include <unordered_map>

class WeaponAttributes {
public:
    static void Apply(uint32_t localPlayer, float speed, bool enabled);

    static void RestoreAll();
    static void RestoreGlobalScales(uint32_t localPlayer);

private:
    struct WeaponOriginalValues {
        float fireInterval;
        float repeatFireInterval;
        float multiFireInterval;
        float addFireSpeed;
    };

    static std::unordered_map<uint32_t, WeaponOriginalValues> restoredWeapons;
    static uint32_t lastWeaponAddr;
    static float lastSpeed;
    static bool wasEnabled;

    static void ApplyToWeapon(uint32_t localPlayer, float speed);
    static void ApplyGlobalScales(uint32_t localPlayer, float speed);
};
