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
        if (weaponData) {
            Escrever<float>(weaponData + Offsets::WeaponRecoil, 0.0f);
        }

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
