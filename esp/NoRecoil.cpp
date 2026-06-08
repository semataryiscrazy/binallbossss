#include "NoRecoil.hpp"
#include "Offsets.hpp"
#include <thread>
#include <atomic>
#include <windows.h>

static std::atomic<bool> nr_running{ false };
static std::thread nr_thread;

static void NoRecoilLoop() {
    while (nr_running) {
        if (!g_Globals.Exploits.NoRecoil || !g_Globals.EspConfig.LocalPlayer) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        uint32_t localPlayer = g_Globals.EspConfig.LocalPlayer;
        if (localPlayer == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        uint32_t weaponAddr = 0;
        if (!Mem.ReadFast2<uint32_t>(localPlayer + Offsets::Weapon, &weaponAddr) || !weaponAddr) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        uint32_t weaponData = 0;
        if (Mem.ReadFast2<uint32_t>(weaponAddr + Offsets::WeaponData, &weaponData) && weaponData) {
            Mem.Write<float>(weaponData + Offsets::WeaponRecoil, 0.0f);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void Exploit::NoRecoil::Work() {
    if (!g_Globals.Exploits.NoRecoil) {
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
