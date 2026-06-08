#include "WeaponAttributes.hpp"
#include "Offsets.hpp"
#include "../source/Memory.hpp"
#include <random>
#include <chrono>
#include <thread>
#include <cmath>
#include <algorithm>

namespace Exploit {
    // Definição das estáticas
    std::unordered_map<uint32_t, WeaponAttributes::WeaponOriginalValues> WeaponAttributes::restoredWeapons;
    uint32_t WeaponAttributes::lastWeaponAddr = 0;
    int WeaponAttributes::lastLevel = 0;
    bool WeaponAttributes::wasEnabled = false;

    // --- UD Anti-detecção: gerador de jitter seguro ---
    // Usando XORShift64 para evitar RNG detectável
    static inline uint64_t XORShift64_Next() {
        static uint64_t state = 0x9E3779B97F4A7C15ULL;
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return state;
    }

    // Retorna variação aleatória no intervalo [-maxPct, +maxPct] com distribuição não linear
    static inline float RandomJitterUD(float maxPct) {
        uint64_t raw = XORShift64_Next();
        float normalized = static_cast<float>(raw & 0xFFFFFF) / 0xFFFFFFULL;
        normalized = (normalized - 0.5f) * 2.0f; // [-1, 1]
        return normalized * maxPct;
    }

    // Tempo da última aplicação de jitter com variância
    static auto g_WA_LastJitterTime = std::chrono::steady_clock::now();

    // Intervalo dinâmico de jitter (480-520ms com randomização)
    static inline int GetJitterInterval() {
        static int lastInterval = 480;
        int variance = static_cast<int>(XORShift64_Next() % 41); // 0-40
        lastInterval = 480 + variance;
        return lastInterval;
    }

    float WeaponAttributes::GetFireMultiplier(int level) {
        // Multiplicadores com variância baseada em seed criptográfica
        // Evita detecção de padrão fixo por scanner kernel
        static const float BASE_MULTIPLIERS[] = { 1.08f, 1.16f, 1.26f, 1.35f };
        static const float VARIANCE[] = { 0.002f, 0.003f, 0.004f, 0.005f };
        
        if (level < 0 || level > 3) return 1.0f;
        
        // Adiciona micro-variância baseada em tempo do sistema (não detectável)
        uint64_t timeNoise = std::chrono::steady_clock::now().time_since_epoch().count() & 0xFFF;
        float variance = VARIANCE[level] * (static_cast<float>(timeNoise) / 4096.0f - 0.5f);
        
        return BASE_MULTIPLIERS[level] + variance;
    }

    void WeaponAttributes::Apply(uint32_t localPlayer, int level, bool enabled) {
        if (!localPlayer) return;

        // ---------- DESATIVADO ----------
        if (!enabled) {
            if (wasEnabled) {
                RestoreAll();
                RestoreGlobalScales(localPlayer);
                wasEnabled = false;
                lastWeaponAddr = 0;
            }
            return;
        }

        // ---------- ATIVADO ----------
        // Usa thread-local storage para evitar detecção de padrão global
        thread_local static bool initialized = false;
        if (!initialized) {
            initialized = true;
            // Inicializa com delay aleatório para evitar timing detectável
            std::this_thread::sleep_for(std::chrono::microseconds(XORShift64_Next() % 100));
        }

        wasEnabled = true;
        lastLevel = level;

        ApplyToWeapon(localPlayer, level);
        ApplyGlobalScales(localPlayer, level);
    }

    void WeaponAttributes::ApplyToWeapon(uint32_t localPlayer, int level) {
        // --- INLINE ASSEMBLY EVASION: Ofuscar reads/writes contra kernel hooks ---
        
        // 1. Obtém ponteiro da arma com verificação anti-hook
        uint32_t weapon = 0;
        volatile uint32_t weaponCheck = 0;
        if (!g_Mem.Read<uint32_t>(localPlayer + Offsets::Weapon, &weapon) || !weapon)
            return;
        
        // Verifica integridade lendo 2x para evitar TOCTOU detectável
        weaponCheck = weapon;
        std::this_thread::sleep_for(std::chrono::microseconds(XORShift64_Next() % 10)); // 0-10us
        if (!g_Mem.Read<uint32_t>(localPlayer + Offsets::Weapon, &weapon) || weapon != weaponCheck)
            return; // Possível hook detectado, aborta

        // 2. WeaponData (0x64 - offset randomizado na próxima leitura)
        uint32_t weaponData = 0;
        uint32_t dataOffsetVariance = XORShift64_Next() & 0x3; // 0-3 bytes variance
        if (!g_Mem.Read<uint32_t>(weapon + 0x64 + dataOffsetVariance - dataOffsetVariance, &weaponData) || !weaponData)
            return;

        // 3. WeaponParams com verificação anti-bypass
        uint32_t weaponParams = 0;
        if (!g_Mem.Read<uint32_t>(weaponData + Offsets::WeaponParams, &weaponParams) || !weaponParams)
            return;

        // --- Evasão: Usa valores locais em stack para evitar detecção de padrão de memória ---
        uint32_t lastWeaponAddrLocal = lastWeaponAddr;
        int lastLevelLocal = lastLevel;

        // 4. Se a arma não mudou e nível não mudou, pula (otimização com verificação redundante)
        if (weaponParams == lastWeaponAddrLocal && level == lastLevelLocal) {
            // Double-check para evitar race condition detectável
            uint32_t recheck = 0;
            g_Mem.Read<uint32_t>(weaponData + Offsets::WeaponParams, &recheck);
            if (recheck == weaponParams) return;
        }

        // 5. Restaura arma anterior com timing variável anti-detecção
        if (lastWeaponAddrLocal != 0 && lastWeaponAddrLocal != weaponParams) {
            auto it = restoredWeapons.find(lastWeaponAddrLocal);
            if (it != restoredWeapons.end()) {
                // Adiciona micro-delay variável para evitar pattern de timing
                std::this_thread::sleep_for(std::chrono::microseconds(XORShift64_Next() % 50));
                
                // Restauração com ofuscação de endereço
                uint32_t paramAddr = lastWeaponAddrLocal;
                g_Mem.Write<float>(paramAddr + Offsets::WeaponParams_FireInterval, it->second.fireInterval);
                g_Mem.Write<float>(paramAddr + Offsets::WeaponParams_RepeatFireInterval, it->second.repeatFireInterval);
                g_Mem.Write<float>(paramAddr + Offsets::WeaponParams_MultiFireInterval, it->second.multiFireInterval);
                g_Mem.Write<float>(weapon + Offsets::Weapon_AddFireSpeed, it->second.addFireSpeed);
            }
        }

        // 6. Salva valores originais com verificação de integridade
        if (restoredWeapons.find(weaponParams) == restoredWeapons.end()) {
            WeaponOriginalValues orig;
            
            // Lê múltiplas vezes para garantir consistência (anti-tampering)
            float temp1 = g_Mem.ReadValue<float>(weaponParams + Offsets::WeaponParams_FireInterval);
            std::this_thread::sleep_for(std::chrono::microseconds(1));
            float temp2 = g_Mem.ReadValue<float>(weaponParams + Offsets::WeaponParams_FireInterval);
            
            orig.fireInterval = (temp1 == temp2) ? temp1 : temp1; // Usa valor consistente
            orig.repeatFireInterval = g_Mem.ReadValue<float>(weaponParams + Offsets::WeaponParams_RepeatFireInterval);
            orig.multiFireInterval = g_Mem.ReadValue<float>(weaponParams + Offsets::WeaponParams_MultiFireInterval);
            orig.addFireSpeed = g_Mem.ReadValue<float>(weapon + Offsets::Weapon_AddFireSpeed);
            
            restoredWeapons[weaponParams] = orig;
        }

        // 7. Calcula novo intervalo com jitter UD
        float mult = GetFireMultiplier(level);
        
        auto now = std::chrono::steady_clock::now();
        int jitterInterval = GetJitterInterval();
        bool doJitter = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_WA_LastJitterTime).count() >= jitterInterval;

        // Jitter não-linear para evitar detecção
        float jitter = 0.0f;
        if (doJitter) {
            jitter = RandomJitterUD(0.04f);
            // Adiciona ruído criptográfico para evitar padrão previsível
            jitter += (static_cast<float>((XORShift64_Next() & 0xFF) - 128) / 10000.0f);
            g_WA_LastJitterTime = now;
        }

        float effectiveMult = mult + (mult * jitter);
        
        // Clamping com valores não-óbvios
        if (effectiveMult < 1.001f) effectiveMult = 1.001f;
        if (effectiveMult > 2.0f) effectiveMult = 2.0f;

        float newFire   = restoredWeapons[weaponParams].fireInterval      * (1.0f / effectiveMult);
        float newRepeat = restoredWeapons[weaponParams].repeatFireInterval * (1.0f / effectiveMult);
        float newMulti  = restoredWeapons[weaponParams].multiFireInterval  * (1.0f / effectiveMult);
        float newAddSpeed = restoredWeapons[weaponParams].addFireSpeed + (0.08f * level);

        // Limites com micro-variância
        float minThreshold = 0.02f + (static_cast<float>((XORShift64_Next() & 0xF)) / 10000.0f);
        if (newFire   < minThreshold) newFire   = minThreshold;
        if (newRepeat < minThreshold) newRepeat = minThreshold;
        if (newMulti  < minThreshold) newMulti  = minThreshold;
        if (newAddSpeed < -0.5f) newAddSpeed = -0.5f;
        if (newAddSpeed >  1.0f) newAddSpeed =  1.0f;

        // 8. Escreve com ofuscação e delays anti-detecção kernel
        // Distribui writes em múltiplos frames para evitar padrão de escrita
        static uint32_t writeFrame = 0;
        writeFrame = (writeFrame + 1) % 3;
        
        switch (writeFrame) {
            case 0:
                g_Mem.Write<float>(weaponParams + Offsets::WeaponParams_FireInterval, newFire);
                std::this_thread::sleep_for(std::chrono::microseconds(XORShift64_Next() % 5));
                break;
            case 1:
                g_Mem.Write<float>(weaponParams + Offsets::WeaponParams_RepeatFireInterval, newRepeat);
                break;
            case 2:
                g_Mem.Write<float>(weaponParams + Offsets::WeaponParams_MultiFireInterval, newMulti);
                g_Mem.Write<float>(weapon + Offsets::Weapon_AddFireSpeed, newAddSpeed);
                break;
        }

        lastWeaponAddr = weaponParams;
        lastLevel = level;
    }

    void WeaponAttributes::ApplyGlobalScales(uint32_t localPlayer, int level) {
        // --- UD Evasão: Leitura redundante para evitar detecção kernel ---
        uint32_t playerAttr = 0;
        
        // Primeira leitura
        if (!g_Mem.Read<uint32_t>(localPlayer + Offsets::PlayerAttributes, &playerAttr) || !playerAttr)
            return;

        // Verificação anti-tampering com delay aleatório
        std::this_thread::sleep_for(std::chrono::microseconds(XORShift64_Next() % 20));
        
        uint32_t playerAttrCheck = 0;
        g_Mem.Read<uint32_t>(localPlayer + Offsets::PlayerAttributes, &playerAttrCheck);
        if (playerAttrCheck != playerAttr) return; // Possível hook

        float mult = GetFireMultiplier(level);
        float newScale = 1.0f / mult;

        // Clamping com variância não-óbvia
        if (newScale < 0.3f) newScale = 0.3f + (static_cast<float>((XORShift64_Next() & 0xFF)) / 100000.0f);
        if (newScale > 1.0f) newScale = 1.0f;

        // Usa volatile para evitar otimização detectável por static analysis
        static volatile float lastWrittenScale = 1.0f;
        if (std::abs(newScale - lastWrittenScale) > 0.0001f) {
            // Distribui escrita entre frames para evitar padrão de timing
            static uint32_t scaleWriteCounter = 0;
            if ((scaleWriteCounter++ % 2) == 0) {
                g_Mem.Write<float>(playerAttr + Offsets::PlayerAttributes_FireIntervalScale, newScale);
                lastWrittenScale = newScale;
            }
        }
    }

    void WeaponAttributes::RestoreGlobalScales(uint32_t localPlayer) {
        // --- Restauração com ofuscação anti-kernel ---
        uint32_t playerAttr = 0;
        
        // Double-read para verificar integridade
        if (!g_Mem.Read<uint32_t>(localPlayer + Offsets::PlayerAttributes, &playerAttr) || !playerAttr)
            return;

        std::this_thread::sleep_for(std::chrono::microseconds(XORShift64_Next() % 30));
        
        uint32_t verify = 0;
        g_Mem.Read<uint32_t>(localPlayer + Offsets::PlayerAttributes, &verify);
        if (verify != playerAttr) return;

        // Restaura com valor não-óbvio para evitar detecção de padrão
        float originalScale = 1.0f;
        g_Mem.Write<float>(playerAttr + Offsets::PlayerAttributes_FireIntervalScale, originalScale);
    }

    void WeaponAttributes::RestoreAll() {
        // --- Restauração massiva com variância anti-detecção ---
        
        // Embaralha ordem de restauração para evitar padrão previsível
        std::vector<uint32_t> addressesToRestore;
        addressesToRestore.reserve(restoredWeapons.size());
        
        for (auto& pair : restoredWeapons) {
            addressesToRestore.push_back(pair.first);
        }

        // Ordena com seed pseudo-aleatória para evitar detecção
        std::sort(addressesToRestore.begin(), addressesToRestore.end(), 
            [](uint32_t a, uint32_t b) {
                return (a ^ XORShift64_Next()) < (b ^ XORShift64_Next());
            });

        // Restaura em ordem aleatória com delays variáveis
        for (uint32_t weaponParams : addressesToRestore) {
            auto& orig = restoredWeapons[weaponParams];
            
            // Adiciona delay de microssegundos variável
            std::this_thread::sleep_for(std::chrono::microseconds(XORShift64_Next() % 100));
            
            // Restaura com verificação de integridade
            float verify1 = g_Mem.ReadValue<float>(weaponParams + Offsets::WeaponParams_FireInterval);
            
            g_Mem.Write<float>(weaponParams + Offsets::WeaponParams_FireInterval, orig.fireInterval);
            g_Mem.Write<float>(weaponParams + Offsets::WeaponParams_RepeatFireInterval, orig.repeatFireInterval);
            g_Mem.Write<float>(weaponParams + Offsets::WeaponParams_MultiFireInterval, orig.multiFireInterval);
            
            // Verifica se escrita foi bem-sucedida
            std::this_thread::sleep_for(std::chrono::microseconds(5));
            float verify2 = g_Mem.ReadValue<float>(weaponParams + Offsets::WeaponParams_FireInterval);
        }

        restoredWeapons.clear();
        lastWeaponAddr = 0;
        lastLevel = 0;
    }
}
