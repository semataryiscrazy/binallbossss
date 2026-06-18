#include "NoRecoil.hpp"
#include "Offsets.h"
#include "Utils.h"
#include "Scope.h"
#include <thread>
#include <atomic>
#include <chrono>

static std::atomic<bool> nr_running{ false };
static std::thread nr_thread;

static void NoRecoilLoop() {
    while (nr_running) {
        if (!NoRecoilEnabled || !cachedLocalPlayer) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        uint32_t localPlayer = static_cast<uint32_t>(cachedLocalPlayer);
        if (localPlayer == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        uint32_t weaponAddr = Ler<uint32_t>(localPlayer + Offsets::Weapon);
        if (!weaponAddr) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        uint32_t weaponData = Ler<uint32_t>(weaponAddr + Offsets::WeaponData);
        if (!weaponData) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        uint32_t wp = weaponData + Offsets::WeaponParams;
        uint32_t wp2 = weaponAddr + Offsets::Scatter_Weapon;
        Escrever<float>(weaponData + Offsets::WeaponRecoil, 0.0f);
        Escrever<float>(wp + 0x0C, 0.0f);
        Escrever<float>(wp + 0x10, 0.0f);
        Escrever<float>(wp + 0x30, 0.0f);
        Escrever<float>(wp + 0x34, 0.0f);
        Escrever<float>(wp + 0x38, 0.0f);
        Escrever<float>(wp + 0x3C, 0.0f);
        Escrever<float>(wp + 0x40, 0.0f);
        Escrever<float>(wp + 0x44, 0.0f);
        Escrever<float>(wp + 0x48, 0.0f);
        Escrever<float>(wp + 0x4C, 0.0f);
        Escrever<float>(wp2 + 0x0C, 0.0f);
        Escrever<float>(wp2 + 0x10, 0.0f);
        Escrever<float>(wp2 + 0x30, 0.0f);
        Escrever<float>(wp2 + 0x34, 0.0f);
        Escrever<float>(wp2 + 0x38, 0.0f);
        Escrever<float>(wp2 + 0x3C, 0.0f);
        Escrever<float>(wp2 + 0x40, 0.0f);
        Escrever<float>(wp2 + 0x44, 0.0f);
        Escrever<float>(wp2 + 0x48, 0.0f);
        Escrever<float>(wp2 + 0x4C, 0.0f);

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void Exploit::NoRecoil::Work() {
    if (!NoRecoilEnabled) {
        if (nr_running) Stop();
        return;
    }
    if (!nr_running) Start();
}

void Exploit::NoRecoil::Start() {
    if (nr_running) return;
    nr_running = true;
    nr_thread = std::thread(NoRecoilLoop);
    nr_thread.detach();
}

void Exploit::NoRecoil::Stop() {
    nr_running = false;
}
