#if !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif
#pragma execution_character_set("utf-8")
#include "Imports/includes.h"
#include "Main.h"
#include "Imports/EntityCache.cpp"
#include <unordered_map>
#include <shlobj.h>
#include "../keyauth/ka_bridge.h"
#include "../dynimp.h"
#include "Imports/WeaponAttributes.cpp"
#include "Imports/Spinbot.cpp"
#include "Imports/KernelEvasion.hpp"
#include "Imports/AdvancedEvasion.hpp"
#include "Imports/MemoryIntegrity.hpp"
#include "Cfg/Obfuscation.h"
#include "Imports/LockAim.cpp"
#include "Imports/NoRecoil.cpp"
HINSTANCE g_hDll = nullptr;
bool g_Unload = false;
ImFont* FontAwesomeRegular = nullptr;
ImFont* FontAwesomeSolid14 = nullptr;
ImFont* FontAwesomeBrands = nullptr;
void UnloadCheat();
extern "C" __declspec(dllexport) void TriggerUnload() { UnloadCheat(); }

// Thread-local variables (for login/register)
static bool loggingIn = false;
static bool REGing = false;
static bool regLoading = false;
static char RegUser[256] = "";
static char RegPass[256] = "";
static char RegKey[256] = "";

// ─── Auto-login: salva/carrega credenciais no registro ───
static const char* REG_KEY = AY_OBFUSCATE("Software\\Satella");
static const char* REG_VAL_USER = AY_OBFUSCATE("user");
static const char* REG_VAL_PASS = AY_OBFUSCATE("pass");
static const int XOR_KEY = 0x5A;

static void xor_obfuscate(char* buf, int len) {
    for (int i = 0; i < len; i++) buf[i] ^= XOR_KEY;
}

static void save_credentials(const char* u, const char* p) {
    HKEY hk;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, REG_KEY, 0, NULL, 0, KEY_WRITE, NULL, &hk, NULL) == ERROR_SUCCESS) {
        RegSetValueExA(hk, REG_VAL_USER, 0, REG_SZ, (BYTE*)u, (DWORD)strlen(u) + 1);
        char buf[256]; strcpy(buf, p); xor_obfuscate(buf, (int)strlen(p));
        RegSetValueExA(hk, REG_VAL_PASS, 0, REG_BINARY, (BYTE*)buf, (DWORD)strlen(p));
        RegCloseKey(hk);
    }
}

static bool load_credentials(char* u, int u_sz, char* p, int p_sz) {
    HKEY hk;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, REG_KEY, 0, KEY_READ, &hk) != ERROR_SUCCESS) return false;
    DWORD type, sz = u_sz;
    bool ok = false;
    if (RegQueryValueExA(hk, REG_VAL_USER, NULL, &type, (BYTE*)u, &sz) == ERROR_SUCCESS && type == REG_SZ) {
        sz = p_sz;
        if (RegQueryValueExA(hk, REG_VAL_PASS, NULL, &type, (BYTE*)p, &sz) == ERROR_SUCCESS && type == REG_BINARY) {
            xor_obfuscate(p, (int)sz);
            ok = true;
        }
    }
    RegCloseKey(hk);
    return ok;
}
// ─────────────────────────────────────────────────────────

// --- Utilitarios de desenho ImGui (foreground draw list) ---
static ImDrawList* ESP_DL() { return ImGui::GetWindowDrawList(); }
static void DrawLineIm(float x1, float y1, float x2, float y2, float thickness, float r, float g, float b, float a = 1.0f) {
    ESP_DL()->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), ImColor(r, g, b, a), thickness);
}
static void DrawBoxIm(float x, float y, float w, float h, float thickness, float r, float g, float b, float a = 1.0f) {
    auto* dl = ESP_DL();
    ImU32 col = ImColor(r, g, b, a);
    dl->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), col, 0.0f, 0, thickness);
}
static void DrawCircleIm(float cx, float cy, float radius, float thickness, float r, float g, float b, float a = 1.0f) {
    ESP_DL()->AddCircle(ImVec2(cx, cy), radius, ImColor(r, g, b, a), 0, thickness);
}
static void DrawTextShadowIm(const char* text, float x, float y, float r, float g, float b, float size = 15) {
    auto* dl = ESP_DL();
    dl->AddText(ImGui::GetFont(), size, ImVec2(x + 1, y + 1), IM_COL32(0, 0, 0, 180), text);
    dl->AddText(ImGui::GetFont(), size, ImVec2(x, y), ImColor(r, g, b), text);
}
static void DrawHealthBarIm(float x, float y, float w, float h, short health, short maxHealth) {
    if (maxHealth <= 0) return;
    float pct = (float)health / maxHealth;
    if (pct < 0) pct = 0;
    float barW = 4;
    float barX = x - barW - 2;
    auto* dl = ESP_DL();
    dl->AddRectFilled(ImVec2(barX, y), ImVec2(barX + barW, y + h), IM_COL32(40, 40, 40, 130));
    if (pct > 0) {
        float fillH = h * pct;
        ImU32 fillCol;
        if (pct <= 0.3f) fillCol = IM_COL32(255, 50, 50, 255);
        else if (pct <= 0.6f) fillCol = IM_COL32(255, 255, 50, 255);
        else fillCol = IM_COL32(50, 255, 50, 255);
        dl->AddRectFilled(ImVec2(barX + 1, y + h - fillH), ImVec2(barX + barW - 1, y + h), fillCol);
    }
}
static void DrawRectFilledIm(float x, float y, float w, float h, float r, float g, float b, float a = 1.0f) {
    ESP_DL()->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), ImColor(r, g, b, a));
}
static void DrawCornerBoxIm(float x, float y, float w, float h, float len, float thickness, float r, float g, float b, float a = 1.0f) {
    auto* dl = ESP_DL();
    ImU32 col = ImColor(r, g, b, a);
    float l = len < w*0.5f ? len : w*0.3f;
    dl->AddLine(ImVec2(x, y + l), ImVec2(x, y), col, thickness);
    dl->AddLine(ImVec2(x, y), ImVec2(x + l, y), col, thickness);
    dl->AddLine(ImVec2(x + w - l, y), ImVec2(x + w, y), col, thickness);
    dl->AddLine(ImVec2(x + w, y), ImVec2(x + w, y + l), col, thickness);
    dl->AddLine(ImVec2(x, y + h - l), ImVec2(x, y + h), col, thickness);
    dl->AddLine(ImVec2(x, y + h), ImVec2(x + l, y + h), col, thickness);
    dl->AddLine(ImVec2(x + w - l, y + h), ImVec2(x + w, y + h), col, thickness);
    dl->AddLine(ImVec2(x + w, y + h), ImVec2(x + w, y + h - l), col, thickness);
}

void UnloadCheat();

// ─── AOB Patches ───
static void AplicarPatchesAOB() {
    if (il2cpp == 0 || il2cppSize == 0) return;

    static const uint8_t pat_06DA93B0[] = {0x90,0xC1,0x00,0x00,0x70,0x41,0x01,0x00,0x00,0x00,0x00,0x00,0xC0,0x3F,0x00,0x00,0x00,0x3F,0x00,0x00,0x80,0x3F};
    static const uint8_t pat_06DA93B0_mod[] = {0x90,0xC1,0x00,0x00,0x70,0x41,0x01,0x00,0x00,0x00,0x00,0x00,0xC0,0x00,0x00,0x00,0x00,0x3F,0x00,0x00,0x80,0x3F};
    static const uint8_t pat_visao10x[] = {0xF4,0x04,0x00,0xA0,0xE1,0x0A,0x0A,0x84,0xED,0x0E,0x2A,0x84,0xED,0x05,0x1A,0x84,0xED,0x00,0x3A,0x84,0xED,0xF0,0x81,0xBD,0xE8,0x00,0x00,0xB4,0x43,0xDB,0x0F,0x49,0x40,0x10,0x2A,0x00,0xEE,0x00,0x10,0x80,0xE5,0x10,0x3A,0x01,0xEE,0x14,0x10,0x80,0xE5,0x00,0x2A,0x30,0xEE,0x00,0x10,0x00,0xE3,0x41,0x3A,0x30,0xEE,0x80,0x1F};
    static const uint8_t pat_visao10x_mod[] = {0xF4,0x04,0x00,0xA0,0xE1,0x0A,0x0A,0x84,0xED,0x0E,0x2A,0x84,0xED,0x05,0x1A,0x84,0xED,0x00,0x3A,0x84,0xED,0xF0,0x81,0xBD,0xE8,0x00,0x00,0xB4,0x43,0xDB,0x0F,0x90,0x40,0x10,0x2A,0x00,0xEE,0x00,0x10,0x80,0xE5,0x10,0x3A,0x01,0xEE,0x14,0x10,0x80,0xE5,0x00,0x2A,0x30,0xEE,0x00,0x10,0x00,0xE3,0x41,0x3A,0x30,0xEE,0x80,0x1F};
    static const uint8_t pat_norecoil[] = {0x7A,0x44,0xF0,0x48,0x2D,0xE9,0x10,0xB0,0x8D,0xE2,0x02,0x8B,0x2D,0xED,0x08,0xD0};
    static const uint8_t pat_norecoil_mod[] = {0x7A,0xFF,0xF0,0x48,0x2D,0xE9,0x10,0xB0,0x8D,0xE2,0x02,0x8B,0x2D,0xED,0x08,0xD0};
    static const uint8_t pat_fov360[] = {0x70,0x42,0x00,0x00,0x00,0x00,0x00,0x00,0xC0,0x3F,0x0A,0xD7,0xA3,0x3B,0x0A,0xD7,0xA3,0x3B,0x8F,0xC2,0x75,0x3D,0xAE,0x47,0xE1,0x3D,0x9A,0x99,0x19,0x3E,0xCD,0xCC,0x4C,0x3E,0xA4,0x70,0xFD,0x3E};
    static const uint8_t pat_fov360_mod[] = {0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0xC0,0x3F,0x0A,0xD7,0xA3,0x3B,0x0A,0xD7,0xA3,0x3B,0x8F,0xC2,0x75,0x3D,0xAE,0x47,0xE1,0x3D,0x9A,0x99,0x19,0x3E,0xCD,0xCC,0x4C,0x3E,0xA4,0x70,0xFD,0x3E};
    static const uint8_t pat_balainf[] = {0xD8,0x00,0xD0,0xE5,0x1E,0xFF,0x2F,0xE1};
    static const uint8_t pat_balainf_mod[] = {0xFC,0x26,0x00,0x00,0x1E,0xFF,0x2F,0xE1};
    static const uint8_t pat_wallhack[] = {0x3F,0xAE,0x47,0x81,0x3F,0xAE,0x47,0x81,0x3F,0xAE,0x47,0x81,0x3F,0x00,0x1A,0xB7,0xEE,0xDC,0x3A,0x9F,0xED,0x30,0x00,0x4F,0xE2,0x43,0x2A,0xB0,0xEE,0xEF,0x0A,0x60,0xF4,0x43,0x6A,0xF0,0xEE,0x1C,0x00,0x8A,0xE2,0x43,0x5A,0xF0,0xEE,0x8F,0x0A,0x48,0xF4,0x43,0x2A,0xF0,0xEE,0x43,0x7A,0xB0,0xEE,0x8F,0x0A,0x40,0xF4,0x41,0xAA,0xB0,0xEE,0xFE,0x05,0xA0,0xE3,0x41,0x1A,0xF0,0xEE,0x2C,0x00,0x8A,0xE5,0x10};
    static const uint8_t pat_wallhack_mod[] = {0x3F,0xAE,0x47,0x81,0x3F,0xAE,0x47,0x81,0xBF,0xAE,0x47,0x81,0x3F,0x00,0x1A,0xB7,0xEE,0xDC,0x3A,0x9F,0xED,0x30,0x00,0x4F,0xE2,0x43,0x2A,0xB0,0xEE,0xEF,0x0A,0x60,0xF4,0x43,0x6A,0xF0,0xEE,0x1C,0x00,0x8A,0xE2,0x43,0x5A,0xF0,0xEE,0x8F,0x0A,0x48,0xF4,0x43,0x2A,0xF0,0xEE,0x43,0x7A,0xB0,0xEE,0x8F,0x0A,0x40,0xF4,0x41,0xAA,0xB0,0xEE,0xFE,0x05,0xAE,0xE3,0x41,0x1A,0xF0,0xEE,0x2C,0x00,0x8A,0xE5,0x10};
    static const uint8_t pat_speedhack[] = {0x02,0x2B,0x07,0x3D,0x02,0x2B,0x07,0x3D,0x02,0x2B,0x07,0x3D,0x00,0x00,0x00,0x00,0x9B,0x6C,0xF2,0x41,0x00,0x00,0x00,0x00};
    static const uint8_t pat_speedhack_mod[] = {0xE3,0xA5,0x9B,0x3C,0xE3,0xA5,0x9B,0x3C,0x02,0x2B,0x07,0x3D,0x00,0x00,0x00,0x00,0x9B,0x6C,0xF2,0x41,0x00,0x00,0x00,0x00};
    static const uint8_t pat_camleft[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x3F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0xBF,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0xBF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x3F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
    static const uint8_t pat_camleft_mod[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0xBF,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0xBF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x3F,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0xBF};
    static const uint8_t pat_tracking2x[] = {0xC0,0x3F,0x33,0x33,0x93,0x3F,0x8F,0xC2,0xF5,0x3C,0xCD,0xCC,0xCC,0x3D,0x02,0x00,0x00,0x00,0xEC,0x51,0xB8,0x3D,0xCD,0xCC,0x4C,0x3F,0x00,0x00,0x00,0x00,0x00,0x00,0xA0,0x42,0x00,0x00,0xC0,0x3F,0x33,0x33,0x13,0x40,0x00,0x00,0xF0,0x3F,0x00,0x00,0x80,0x3F,0x01};
    static const uint8_t pat_tracking2x_mod[] = {0xC0,0x3F,0x33,0x33,0x93,0x3F,0x8F,0xC2,0xF5,0x3C,0xCD,0xCC,0xCC,0x3D,0x00,0x00,0x00,0x00,0xEC,0x51,0xB8,0x3D,0xCD,0xCC,0x4C,0x3F,0x00,0x00,0x00,0x00,0x00,0x00,0xA0,0x42,0x00,0x00,0xC0,0x3F,0x33,0x33,0x13,0x40,0x00,0x00,0xF0,0x3F,0x00,0x00,0x80,0x5C,0x01};

    struct { const uint8_t* search; const uint8_t* replace; const char* mask; size_t len; bool* applied; bool* toggle; } tbl[] = {
        { pat_06DA93B0, pat_06DA93B0_mod, "xxxxxxxxxxxxxxxxxxxxxx", 22, &PatchPixelEstendidoAplicado, &PatchPixelEstendido },
        { pat_visao10x, pat_visao10x_mod, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", 68, &PatchVisao10XAplicado, &PatchVisao10X },
        { pat_norecoil, pat_norecoil_mod, "xxxxxxxxxxxxxxxx", 16, &PatchNoRecoilAOBAplicado, &PatchNoRecoilAOB },
        { pat_fov360, pat_fov360_mod, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", 38, &PatchFOV360Aplicado, &PatchFOV360 },
        { pat_balainf, pat_balainf_mod, "xxxxxxxx", 8, &PatchBalaInfinitaAplicado, &PatchBalaInfinita },
        { pat_wallhack, pat_wallhack_mod, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", 78, &PatchWallHackAplicado, &PatchWallHack },
        { pat_speedhack, pat_speedhack_mod, "xxxxxxxxxxxxxxxxxxxxxxxx", 24, &PatchSpeedHackAplicado, &PatchSpeedHack },
        { pat_camleft, pat_camleft_mod, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", 48, &PatchCameraLeftAplicado, &PatchCameraLeft },
        { pat_tracking2x, pat_tracking2x_mod, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", 51, &PatchTracking2XAplicado, &PatchTracking2X },
    };

    for (auto& p : tbl) {
        if (*p.toggle && !*p.applied) {
            AOBPattern pat = { p.search, p.replace, p.mask, p.len, "" };
            if (AOBApply(pat, il2cpp, il2cppSize)) {
                *p.applied = true;
            }
        }
    }
}

// --- ESP -----------------------------------------------------

static DWORD SafeTick();
static void LogCrash(const char* context);

template<typename Fn>
static void SafeThread(Fn fn, const char* name) {
    __try { fn(); } __except(EXCEPTION_EXECUTE_HANDLER) { LogCrash(name); }
}

static void SafeThreadFn(void (*fn)(), const char* name) {
    __try { fn(); } __except(EXCEPTION_EXECUTE_HANDLER) { LogCrash(name); }
}

static void ThreadAutoLogin();
static void ThreadManualLogin();
static void ThreadRegister();

void DesenharESP(int width, int height) {
    espOffsetX = 0; espOffsetY = 0;
    SWidth = width; SHeight = height;
    if (!Auth.AtivarFuncoes || !Auth.Attached) return;
    UpdateEntityCache();

    // Cache deve estar inicializado
    uint64_t matrixTs = renderMatrixTimestamp.load();
    if (matrixTs == 0) return;

    int entityCount = 0;
    AimbotDistMax = 9999.9f; AimbotTarget = 0;
    { std::lock_guard<std::mutex> lock(GetCacheWriteMutex());
    for (auto& [addr, c] : GetEntityCache()) {
        if (!c.valid) continue;
        if (c.isTeam && !ESPMostrarTime) continue;
        if (c.dying && !ESPMostrarDerrubado) continue;
        bool inEspRange = c.dist <= espMaxDistance;
        if (!inEspRange && !AimbotLegit && !AimSilent) continue;

        ImVec4 col = c.dying ? ImVec4(colorDying[0],colorDying[1],colorDying[2],colorDying[3])
                             : ImVec4(colorName[0],colorName[1],colorName[2],colorName[3]);
        float ox = espOffsetX, oy = espOffsetY;

        // Projeta em tempo real com a matrix mais recente para acompanhar a camera
        Vector3 WorldEnemyHeadPos = World2Screen(renderMatrix, c.headPos);
        Vector3 WorldEnemyFootPos = World2Screen(renderMatrix, c.bodyPos);
        if (WorldEnemyHeadPos.Z != 0 || WorldEnemyFootPos.Z != 0) continue;
        entityCount++;

        // Seleção de alvo para LockAim + SilentAim
        if ((AimbotLegit || AimSilent) && !c.isTeam) {
            float adx = WorldEnemyHeadPos.X - (SWidth * 0.5f);
            float ady = WorldEnemyHeadPos.Y - (SHeight * 0.5f);
            float fov = AimbotLegit ? (float)AimbotFOV : SilentAimFOV;
            float maxDist = AimbotLegit ? (float)AimbotMaxDistance : (float)SilentAimDistance;
            bool skipKnocked = (AimbotLegit ? AimbotIgnoreKnocked : SilentAimIgnoreKnocked) && c.dying;
            bool skipBot = (AimbotLegit ? AimbotIgnoreBots : SilentAimIgnoreBots) && Ler<bool>((uint32_t)(addr + Offsets::IsClientBot));
            if (c.dist <= maxDist && !skipKnocked && !skipBot && adx*adx + ady*ady <= fov*fov) {
                float d = sqrtf(adx*adx + ady*ady);
                if (d < AimbotDistMax) { AimbotDistMax = d; AimbotTarget = addr; }
            }
        }

        float Height = WorldEnemyFootPos.Y - WorldEnemyHeadPos.Y;
        if (Height < 10.f) Height = 10.f;
        if (Height > 350.f) Height = 350.f;
        float Width = Height * 0.40f;
        float bx = WorldEnemyHeadPos.X - Width*0.5f;
        float by = WorldEnemyHeadPos.Y;

        if (ESPNome) {
            const char* name = c.name.empty()?"BOT":c.name.c_str();
            float tw = ImGui::CalcTextSize(name).x;
            float tx = WorldEnemyHeadPos.X+ox - tw*0.5f;
            float ty = by+oy - espTextSize - 2;
            if (espBgAlpha > 0.01f)
                DrawRectFilledIm(tx-3, ty-2, tw+6, espTextSize+4, 0,0,0,espBgAlpha);
            DrawTextShadowIm(name, tx, ty, col.x, col.y, col.z, espTextSize);
        }

        // Health Text (acima do nome, antes da caixa)
        if (ESPHealthText) {
            char hBuf[16]; snprintf(hBuf, sizeof(hBuf), "%d HP", c.health);
            float htw = ImGui::CalcTextSize(hBuf).x;
            float htx = WorldEnemyHeadPos.X+ox - htw*0.5f;
            float hty = by+oy - espTextSize - 2 - (ESPNome ? espTextSize + 2 : 0);
            if (espBgAlpha > 0.01f)
                DrawRectFilledIm(htx-3, hty-2, htw+6, espTextSize+4, 0,0,0,espBgAlpha);
            DrawTextShadowIm(hBuf, htx, hty, col.x, col.y, col.z, espTextSize);
        }

        // Weapon Name (abaixo da distancia)
        if (ESPWeaponName && !c.weaponName.empty()) {
            float wtw = ImGui::CalcTextSize(c.weaponName.c_str()).x;
            float wtx = WorldEnemyHeadPos.X+ox - wtw*0.5f;
            float wty = by+oy + Height + 2 + (ESPDistancia ? espTextSize + 2 : 0);
            if (espBgAlpha > 0.01f)
                DrawRectFilledIm(wtx-3, wty-2, wtw+6, espTextSize+4, 0,0,0,espBgAlpha);
            DrawTextShadowIm(c.weaponName.c_str(), wtx, wty, colorBox[0], colorBox[1], colorBox[2], espTextSize);
        }

        // Snap Lines (s� se entidade tem altura vis�vel na tela)
        float rawH = WorldEnemyFootPos.Y - WorldEnemyHeadPos.Y;
        if (ESPLinha && rawH > 5.0f) {
            float lineEndY = WorldEnemyHeadPos.Y+oy+Height+(ESPDistancia?espTextSize+2:1)+(ESPWeaponName&&!c.weaponName.empty()?espTextSize+2:0);
            float sx = WorldEnemyHeadPos.X+ox;
            if (linePosition==1 && sx > 0 && sx < width && lineEndY > 0 && lineEndY < height)
                DrawLineIm(width/2,height,sx,lineEndY,espThickness,col.x,col.y,col.z);
            if (linePosition==0 && sx > 0 && sx < width)
                DrawLineIm(width/2,0,sx,WorldEnemyHeadPos.Y+oy-(ESPNome?espTextSize+2:1)-(ESPHealthText?espTextSize+2:0),espThickness,col.x,col.y,col.z);
        }

        // Box
        if (ESPCaixa == 1) {
            if (ESPFilledBox)
                DrawRectFilledIm(bx+ox, by+oy, Width, Height, col.x*0.15f, col.y*0.15f, col.z*0.15f, 0.3f);
            DrawBoxIm(bx+ox, by+oy, Width, Height, espThickness, colorBox[0], colorBox[1], colorBox[2]);
        } else if (ESPCaixa == 2) {
            if (ESPFilledBox)
                DrawRectFilledIm(bx+ox, by+oy, Width, Height, col.x*0.15f, col.y*0.15f, col.z*0.15f, 0.3f);
            DrawCornerBoxIm(bx+ox, by+oy, Width, Height, Width*0.25f, espThickness, colorBox[0], colorBox[1], colorBox[2]);
        }

        // Health Bar (multi-posicao)
        if (ESPHealthBarPos == 1) // Left
            DrawHealthBarIm(bx+ox,by+oy,Width,Height,c.health,200);
        else if (ESPHealthBarPos == 2) { // Right
            float barW = 4;
            float barX = bx + ox + Width + 2;
            auto* dl = ESP_DL();
            dl->AddRectFilled(ImVec2(barX, by+oy), ImVec2(barX + barW, by+oy + Height), IM_COL32(40, 40, 40, 130));
            float pct = (float)c.health / 200.0f; if (pct < 0) pct = 0; if (pct > 1) pct = 1;
            if (pct > 0) {
                float fillH = Height * pct;
                ImU32 fillCol = pct <= 0.3f ? IM_COL32(255,50,50,255) : (pct <= 0.6f ? IM_COL32(255,255,50,255) : IM_COL32(50,255,50,255));
                dl->AddRectFilled(ImVec2(barX+1, by+oy+Height-fillH), ImVec2(barX+barW-1, by+oy+Height), fillCol);
            }
        } else if (ESPHealthBarPos == 3) { // Top
            float barH = 3;
            auto* dl = ESP_DL();
            dl->AddRectFilled(ImVec2(bx+ox, by+oy-barH-1), ImVec2(bx+ox+Width, by+oy-1), IM_COL32(40,40,40,130));
            float pct = (float)c.health / 200.0f; if (pct < 0) pct = 0; if (pct > 1) pct = 1;
            if (pct > 0) {
                float fillW = Width * pct;
                ImU32 fillCol = pct <= 0.3f ? IM_COL32(255,50,50,255) : (pct <= 0.6f ? IM_COL32(255,255,50,255) : IM_COL32(50,255,50,255));
                dl->AddRectFilled(ImVec2(bx+ox+1, by+oy-barH), ImVec2(bx+ox+fillW-1, by+oy-1), fillCol);
            }
        } else if (ESPHealthBarPos == 4) { // Bottom
            float barH = 3;
            auto* dl = ESP_DL();
            dl->AddRectFilled(ImVec2(bx+ox, by+oy+Height+1), ImVec2(bx+ox+Width, by+oy+Height+barH+1), IM_COL32(40,40,40,130));
            float pct = (float)c.health / 200.0f; if (pct < 0) pct = 0; if (pct > 1) pct = 1;
            if (pct > 0) {
                float fillW = Width * pct;
                ImU32 fillCol = pct <= 0.3f ? IM_COL32(255,50,50,255) : (pct <= 0.6f ? IM_COL32(255,255,50,255) : IM_COL32(50,255,50,255));
                dl->AddRectFilled(ImVec2(bx+ox+1, by+oy+Height+1), ImVec2(bx+ox+fillW-1, by+oy+Height+barH+1), fillCol);
            }
        }

        if (ESPEsqueleto) {
            Vector3 bs[18];
            int ok = 0;
            for (int i=0;i<18;i++) {
                Vector3 p = World2Screen(renderMatrix, c.boneWorld[i]);
                if (p.Z!=0) { bs[i]=Vector3{0,0,1}; continue; }
                bs[i]=p; ok++;
            }
            if (ok>=2) {
                auto good = [&](int i){return bs[i].Z==0;};
                auto ln = [&](int a,int b){if(good(a)&&good(b))DrawLineIm(bs[a].X+ox,bs[a].Y+oy,bs[b].X+ox,bs[b].Y+oy,espThickness,colorSkeleton[0],colorSkeleton[1],colorSkeleton[2]);};
                ln(0,1); ln(1,2); ln(2,3);
                ln(1,4); ln(4,5); ln(5,6); ln(6,7);
                ln(1,8); ln(8,9); ln(9,10); ln(10,11);
                ln(3,12); ln(12,13); ln(13,14);
                ln(3,15); ln(15,16); ln(16,17);
            }
        }

        if (ESPDistancia) {
            char buf[64]; snprintf(buf,sizeof(buf),"%.2fm",c.dist);
            float tw = ImGui::CalcTextSize(buf).x;
            float dx = WorldEnemyHeadPos.X+ox - tw*0.5f;
            float dy = by+oy + Height + 2;
            if (espBgAlpha > 0.01f)
                DrawRectFilledIm(dx-3, dy-1, tw+6, espTextSize+3, 0,0,0,espBgAlpha);
            DrawTextShadowIm(buf, dx, dy, colorDistance[0], colorDistance[1], colorDistance[2], espTextSize);
        }
    }

    } // lock_guard

    LockAim::SetTarget(AimbotTarget);
    _0xW3X4Y5Z6::SetTarget(AimbotTarget);

    // Enemy Counter
    if (ESPEnemyCounter) {
        char ecBuf[64]; snprintf(ecBuf, sizeof(ecBuf), "Enemies: %d", entityCount);
        float ecw = ImGui::CalcTextSize(ecBuf).x;
        DrawRectFilledIm(width - ecw - 12, 8, ecw + 10, 26, 0,0,0,0.5f);
        DrawTextShadowIm(ecBuf, width - ecw - 7, 10, 1,1,1, 14);
    }

    // Watermark + Stream Mode indicator
    float wmX = 8;
    if (StreamMode) {
        const char* smText = AY_OBFUSCATE("SM ON");
        float smW = ImGui::CalcTextSize(smText).x;
        float smH = 26.0f;
        DrawRectFilledIm(wmX, 8, smW + 12, smH, 0,0,0,0.5f);
        DrawTextShadowIm(smText, wmX + 6, 10, 0.3f, 1.0f, 0.3f, 14);
        wmX += smW + 18;
    }
    if (Watermark) {
        char wmBuf[128];
        static DWORD wmStart = SafeTick();
        DWORD wmNow = SafeTick();
        DWORD wmSec = (wmNow - wmStart) / 1000;
        int wmH = (int)(wmSec / 3600);
        int wmM = ((int)wmSec % 3600) / 60;
        int wmS = (int)wmSec % 60;
        snprintf(wmBuf, sizeof(wmBuf), "Satella Private | %02d:%02d:%02d", wmH, wmM, wmS);
        DrawRectFilledIm(wmX, 8, ImGui::CalcTextSize(wmBuf).x + 12, 26, 0,0,0,0.5f);
        DrawTextShadowIm(wmBuf, wmX + 6, 10, 219/255.f, 0, 166/255.f, 14);
    }
}

HWND FindRenderWindow(HWND fallback);
extern HWND hTargetWindow;
extern HWND hwnd;
static void StopKellerETW();
static void ClearPEBDebugFlags();
static void SafeRenderGDI();

void runRenderTick() {
    eventPoll();
    ImGui::GetIO().MouseDrawCursor = Auth.MenuVisible;

    RECT wr = {0};
    if (!IsWindow(hTargetWindow) || !GetWindowRect(hTargetWindow, &wr)) {
        hTargetWindow = FindRenderWindow(NULL);
        if (!hTargetWindow) return;
        if (!GetWindowRect(hTargetWindow, &wr)) return;
    }

    int cw = wr.right - wr.left, ch = wr.bottom - wr.top;
    if (cw <= 0 || ch <= 0 || IsIconic(hTargetWindow)) return;
    SetWindowPos(hwnd, HWND_TOPMOST, wr.left, wr.top, cw, ch,
        SWP_NOACTIVATE | SWP_NOCOPYBITS);

    ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();

    // ─── Stream Mode (F6) ───
    if (GetAsyncKeyState(VK_F6) & 1) {
        StreamMode = !StreamMode;
        if (StreamMode) {
            SetWindowDisplayAffinity(hwnd, 0x11);
        } else {
            SetWindowDisplayAffinity(hwnd, 0);
        }
    }
    // F7 = Unload completo
    if (GetAsyncKeyState(VK_F7) & 1) { UnloadCheat(); }
    // F8 = Mostra/oculta overlay
    if ((GetAsyncKeyState(VK_F8) & 1)) {
        Auth.MenuVisible = !Auth.MenuVisible;
        Auth.OverlayView = true;
        if (Auth.MenuVisible) {
            SetForegroundWindow(hwnd);
        } else {
            SetForegroundWindow(hTargetWindow);
        }
    }

    // ─── Partículas (original) ───
    struct Particle { float x, y, speed, size; };
    static std::vector<Particle> particles;
    static DWORD lastPartTick2 = SafeTick();
    DWORD nowP = SafeTick();
    float dt = (nowP - lastPartTick2) / 1000.0f; lastPartTick2 = nowP;
    if (!PerformanceMode && particles.empty()) {
        for (int i = 0; i < 40; i++) particles.push_back({static_cast<float>(rand() % 2000) / 2000.0f * 640, static_cast<float>(rand() % 2000) / 2000.0f * 460, 15 + static_cast<float>(rand() % 500) / 100, 0.5f + static_cast<float>(rand() % 100) / 200.0f});
    }

    if (Auth.MenuVisible) {
        static float AnimaTab = 0, Anima = 0;
        static int LastCurrentTab = 0, LastCurrentSub = 0, CurrentSub = 0;
        if (LastCurrentTab != CurrentTab) { AnimaTab = (LastCurrentTab > CurrentTab) ? -460.f : 460.f; LastCurrentTab = CurrentTab; }
        AnimaTab = ImLerp(AnimaTab, 0.f, 8.f * ImGui::GetIO().DeltaTime);
        if (LastCurrentSub != CurrentSub) { Anima = (LastCurrentSub > CurrentSub) ? -460.f : 460.f; LastCurrentSub = CurrentSub; }
        Anima = ImLerp(Anima, 0.f, 8.f * ImGui::GetIO().DeltaTime);

        NotificationManager::DesenharNotificacoes();
        if (CurrentWindow == 0) {

            // ─── Auto-login ───
            static bool autoTried = false;
            if (!autoTried && !Auth.Autenticado && strlen(Auth.Usuario) == 0) {
                autoTried = true;
                char savedUser[256] = "", savedPass[256] = "";
                if (load_credentials(savedUser, 256, savedPass, 256)) {
                    strcpy(Auth.Usuario, savedUser);
                    strcpy(Auth.Senha, savedPass);
                    loggingIn = true;
                    std::thread([=]() {
                        __try { ThreadAutoLogin(); } __except(EXCEPTION_EXECUTE_HANDLER) { LogCrash("[T] autoLogin crash"); }
                    }).detach();
                }
            }
            const float winW = 560, winH = 380;
            const float padX = 40;
            const float inputW = winW - padX * 2;
            const float btnH = 38;

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 10));
            ImGui::SetNextWindowSize(ImVec2(winW, winH));
            ImGui::Begin(AY_OBFUSCATE("Satella"), nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollWithMouse);
            {
                ImDrawList* dl = ImGui::GetWindowDrawList();
                ImVec2 pos = ImGui::GetWindowPos(), sz = ImGui::GetWindowSize();

                dl->AddRectFilled(pos, pos + sz, IM_COL32(0, 0, 0, 255), 12.0f);
                if (!PerformanceMode) {
                    for (auto& p : particles) {
                        p.y -= p.speed * dt; p.x += sinf(p.y * 0.01f) * dt * 15;
                        if (p.y < -10) { p.y = sz.y + 10; p.x = static_cast<float>(rand() % 2000) / 2000.0f * sz.x; }
                        float a = (1 - (p.y / sz.y)) * 0.3f;
                        dl->AddCircleFilled(ImVec2(pos.x + p.x, pos.y + p.y), p.size, IM_COL32(219, 0, 166, static_cast<int>(a * 255)), 6);
                    }
                }
                {
                    ImGui::PushFont(InterBold);
                    const char* titulo = AY_OBFUSCATE("Satella Private");
                    ImVec2 ts = ImGui::CalcTextSize(titulo);
                    dl->AddText(pos + ImVec2((sz.x - ts.x) * 0.5f, 14), ImColor(219, 0, 166), titulo);
                    ImGui::PopFont();
                }

                ImGui::PushFont(InterRegular);
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.15f, 0.18f, 0.45f));

                float curY = 55;
                if (!REGing) {
                    // -- AUTH --
                    ImGui::SetCursorPos(ImVec2(padX, curY));
                    ImGui::TextColored(ImColor(160, 160, 165, 200), "Username");
                    ImGui::SetCursorPos(ImVec2(padX, ImGui::GetCursorPosY() + 4));
                    ImGui::SetNextItemWidth(inputW); ImGui::InputTextWithHint("##user", "Username", Auth.Usuario, IM_ARRAYSIZE(Auth.Usuario));

                    ImGui::SetCursorPos(ImVec2(padX, ImGui::GetCursorPosY() + 10));
                    ImGui::TextColored(ImColor(160, 160, 165, 200), "Password");
                    ImGui::SetCursorPos(ImVec2(padX, ImGui::GetCursorPosY() + 4));
                    ImGui::SetNextItemWidth(inputW); ImGui::InputTextWithHint("##pass", "Password", Auth.Senha, IM_ARRAYSIZE(Auth.Senha), ImGuiInputTextFlags_Password);

                    ImGui::SetCursorPos(ImVec2(padX, ImGui::GetCursorPosY() + 18));

                    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, loggingIn ? 0.5f : 1.0f);
                    ImGui::BeginDisabled(loggingIn);
                    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(219, 0, 166, 220));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(240, 30, 190, 240));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(255, 60, 210, 255));
                    if (ImGui::Button("Entrar", ImVec2(inputW, btnH))) {
                        loggingIn = true;
                        if (strlen(Auth.Usuario) > 0 && strlen(Auth.Senha) > 0) {
                            std::thread([=]() {
                                __try { ThreadManualLogin(); } __except(EXCEPTION_EXECUTE_HANDLER) { LogCrash("[T] manualLogin crash"); }
                            }).detach();
                        } else {
                            loggingIn = false;
                            NotificationManager::AdicionarNotificacao("Preencha usuario e senha", 5.0f, true);
                        }
                    }
                    ImGui::PopStyleColor(3);
                    ImGui::SetCursorPos(ImVec2(padX, ImGui::GetCursorPosY() + 6));
                    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(60, 60, 70, 220));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(80, 80, 90, 240));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(100, 100, 110, 255));
                    if (ImGui::Button("Registrar", ImVec2(inputW, btnH))) {
                        REGing = true;
                    }
                    ImGui::PopStyleColor(3);
                    ImGui::EndDisabled();
                    ImGui::PopStyleVar();
                } else {
                    // -- REG --
                    ImGui::SetCursorPos(ImVec2(padX, curY));
                    ImGui::TextColored(ImColor(160, 160, 165, 200), "Username");
                    ImGui::SetCursorPos(ImVec2(padX, ImGui::GetCursorPosY() + 4));
                    ImGui::SetNextItemWidth(inputW); ImGui::InputTextWithHint("##reguser", "Username", RegUser, IM_ARRAYSIZE(RegUser));

                    ImGui::SetCursorPos(ImVec2(padX, ImGui::GetCursorPosY() + 10));
                    ImGui::TextColored(ImColor(160, 160, 165, 200), "Password");
                    ImGui::SetCursorPos(ImVec2(padX, ImGui::GetCursorPosY() + 4));
                    ImGui::SetNextItemWidth(inputW); ImGui::InputTextWithHint("##regpass", "Password", RegPass, IM_ARRAYSIZE(RegPass), ImGuiInputTextFlags_Password);

                    ImGui::SetCursorPos(ImVec2(padX, ImGui::GetCursorPosY() + 10));
                    ImGui::TextColored(ImColor(160, 160, 165, 200), "License Key");
                    ImGui::SetCursorPos(ImVec2(padX, ImGui::GetCursorPosY() + 4));
                    ImGui::SetNextItemWidth(inputW); ImGui::InputTextWithHint("##regkey", "XXXXX-XXXXX-XXXXX", RegKey, IM_ARRAYSIZE(RegKey));

                    ImGui::SetCursorPos(ImVec2(padX, ImGui::GetCursorPosY() + 14));

                    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, regLoading ? 0.5f : 1.0f);
                    ImGui::BeginDisabled(regLoading);
                    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(219, 0, 166, 220));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(240, 30, 190, 240));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(255, 60, 210, 255));
                    if (ImGui::Button("Registrar", ImVec2(inputW, btnH))) {
                        if (strlen(RegUser) > 0 && strlen(RegPass) > 0 && strlen(RegKey) > 0) {
                            regLoading = true;
                            std::thread([=]() {
                                __try { ThreadRegister(); } __except(EXCEPTION_EXECUTE_HANDLER) { LogCrash("[T] register crash"); }
                            }).detach();
                        } else {
                            NotificationManager::AdicionarNotificacao("Preencha todos os campos", 5.0f, true);
                        }
                    }
                    ImGui::PopStyleColor(3);
                    ImGui::SetCursorPos(ImVec2(padX, ImGui::GetCursorPosY() + 6));
                    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(60, 60, 70, 220));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(80, 80, 90, 240));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(100, 100, 110, 255));
                    if (ImGui::Button("Voltar", ImVec2(inputW, btnH))) {
                        REGing = false;
                    }
                    ImGui::PopStyleColor(3);
                    ImGui::EndDisabled();
                    ImGui::PopStyleVar();
                }

                ImGui::PopStyleColor();
                ImGui::PopFont();
            }
            ImGui::End();
            ImGui::PopStyleVar();
        } else if (CurrentWindow == 1) {
            static bool g_AutoStarted = false;
            if (Auth.Autenticado && !g_AutoStarted) {
                g_AutoStarted = true;
                std::thread([]() { SafeThreadFn(NetworkInit, "[T] NetworkInit"); }).detach();
                std::thread([]() { __try { Sleep(2000); LoadLibraryAndHook(); _0xW3X4Y5Z6::Start(); _0xPrecision::Start(); LockAim::Start(); } __except(EXCEPTION_EXECUTE_HANDLER) { LogCrash("[T] InitHooks crash"); } }).detach();
            }

            JUNK(); AntiDebugCheck();
            ImGui::SetNextWindowSize(ImVec2(640, 440));
            ImGui::Begin("Satella", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoTitleBar);
            {
                ImDrawList* dl = ImGui::GetWindowDrawList();
                ImVec2 pos = ImGui::GetWindowPos(), sz = ImGui::GetWindowSize();

                dl->AddRectFilled(pos, pos + sz, IM_COL32(0, 0, 0, 255), 12.0f);

                if (!PerformanceMode) {
                    for (auto& p : particles) {
                        p.y -= p.speed * dt; p.x += sinf(p.y * 0.01f) * dt * 20;
                        if (p.y < -10) { p.y = sz.y + 10; p.x = static_cast<float>(rand() % 2000) / 2000.0f * sz.x; }
                        float a = (1 - (p.y / sz.y)) * 0.3f;
                        dl->AddCircleFilled(ImVec2(pos.x + p.x, pos.y + p.y), p.size, IM_COL32(219, 0, 166, static_cast<int>(a * 255)), 6);
                    }
                }

                ImGui::PushFont(InterBold);
                ImVec2 title_size = ImGui::CalcTextSize("Satella Private");
                dl->AddText(pos + ImVec2((sz.x - title_size.x) / 2, 15), ImColor(219, 0, 166), "Satella Private");
                ImGui::PopFont();

                static float AnimaTab = 0.0f;
                static int LastCurrentTab = 2;
                if (LastCurrentTab != CurrentTab) {
                    AnimaTab = (LastCurrentTab > CurrentTab) ? -460.f : 460.f;
                    LastCurrentTab = CurrentTab;
                }
                AnimaTab = ImLerp(AnimaTab, 0.f, 6.f * ImGui::GetIO().DeltaTime);

                float contentHeight = sz.y - 85.0f;

                if (CurrentTab == 2) {
                    float startX = 35 + AnimaTab;
                    ImGui::SetCursorPos(ImVec2(startX, 65));
                    ImGui::CustomChild("Aimbot Normal", ImVec2(275, contentHeight));
                    {
                        ImGui::Checkbox("Aimbot", &AimbotLegit);
                        if (AimbotLegit) {
                            ImGui::KeyBind("Key", &AimbotKeyBind, (int*)0);
                            ImGui::SliderInt("FOV", &AimbotFOV, 10, 500);
                            ImGui::SliderInt("Max Distance", &AimbotMaxDistance, 10, 500);
                            static const char* delayOpts[] = { "Instant", "235ms", "325ms", "415ms" };
                            ImGui::Combo("Delay", &AimbotPeitosIndex, delayOpts, IM_ARRAYSIZE(delayOpts));
                            ImGui::Checkbox("Ignore Knocked", &AimbotIgnoreKnocked);
                            ImGui::Checkbox("Ignore Bots", &AimbotIgnoreBots);
                        }
                    }
                    ImGui::EndCustomChild();
                    
                    ImGui::SetCursorPos(ImVec2(startX + 285, 65));
                    ImGui::CustomChild("Aimbot Extras", ImVec2(275, contentHeight));
                    {
                        ImGui::Separator();
                        ImGui::Checkbox("Silent Aim", &AimSilent);
                        if (AimSilent) {
                            ImGui::KeyBind("Key", &SilentAimKeyBind, (int*)0);
                            ImGui::SliderFloat("FOV", &SilentAimFOV, 1.0f, 180.0f, "%.0f");
                            ImGui::SliderInt("Distance", &SilentAimDistance, 10, 500);
                            static const char* hitboxOpts[] = { "Head", "Body" };
                            ImGui::Combo("Hitbox", &SilentAimHitbox, hitboxOpts, IM_ARRAYSIZE(hitboxOpts));
                            ImGui::Checkbox("Ignore Knocked", &SilentAimIgnoreKnocked);
                            ImGui::Checkbox("Ignore Bots", &SilentAimIgnoreBots);
                        }
                        ImGui::Separator();
                        ImGui::Checkbox("Precision", &PrecisionMode);
                        ImGui::Separator();
                        ImGui::Checkbox("No Recoil", &NoRecoilEnabled);
                        ImGui::Separator();
                    }
                    ImGui::EndCustomChild();
                    
                } else if (CurrentTab == 3) {
                    float startX = 35 + AnimaTab;
                    ImGui::SetCursorPos(ImVec2(startX, 65));
                    ImGui::CustomChild("ESP Features", ImVec2(275, contentHeight));
                    {
                        ImGui::PushFont(InterBold); ImGui::TextColored(ImVec4(219/255.f,0,166/255.f,1), "Player"); ImGui::PopFont();
                        ImGui::Separator();
                        ImGui::Combo("Box Type", &ESPCaixa, "Off\0Full\0Cornered\0");
                        if (ESPCaixa > 0) {
                            ImGui::Checkbox("Filled Box", &ESPFilledBox);
                        }
                        ImGui::Checkbox("Skeleton", &ESPEsqueleto);
                        ImGui::Checkbox("Name", &ESPNome);
                        ImGui::Separator();
                        ImGui::PushFont(InterBold); ImGui::TextColored(ImVec4(219/255.f,0,166/255.f,1), "Info"); ImGui::PopFont();
                        ImGui::Separator();
                        ImGui::Checkbox("Health Text", &ESPHealthText);
                        ImGui::Combo("Health Bar", &ESPHealthBarPos, "Off\0Left\0Right\0Top\0Bottom\0");
                        ImGui::Checkbox("Distance", &ESPDistancia);
                        ImGui::Checkbox("Weapon Name", &ESPWeaponName);
                        ImGui::Separator();
                        ImGui::PushFont(InterBold); ImGui::TextColored(ImVec4(219/255.f,0,166/255.f,1), "Visibility"); ImGui::PopFont();
                        ImGui::Separator();
                        ImGui::Checkbox("Show Teammates", &ESPMostrarTime);
                        ImGui::Checkbox("Show Knocked", &ESPMostrarDerrubado);
                    }
                    ImGui::EndCustomChild();
                    
                    ImGui::SetCursorPos(ImVec2(startX + 285, 65));
                    ImGui::CustomChild("ESP Visuals", ImVec2(275, contentHeight));
                    {
                        ImGui::PushFont(InterBold); ImGui::TextColored(ImVec4(219/255.f,0,166/255.f,1), "Effects"); ImGui::PopFont();
                        ImGui::Separator();
                        ImGui::Checkbox("Snap Lines", &ESPLinha);
                        if (ESPLinha) {
                            ImGui::Combo("Line From", &linePosition, "Ground\0Top\0");
                        }
                        ImGui::Separator();
                        ImGui::PushFont(InterBold); ImGui::TextColored(ImVec4(219/255.f,0,166/255.f,1), "Display"); ImGui::PopFont();
                        ImGui::Separator();
                        ImGui::Checkbox("Enemy Counter", &ESPEnemyCounter);
                        ImGui::Checkbox("Watermark", &Watermark);
                        ImGui::Separator();
                        ImGui::PushFont(InterBold); ImGui::TextColored(ImVec4(219/255.f,0,166/255.f,1), "Colors"); ImGui::PopFont();
                        ImGui::Separator();
                        ImGui::ColorEdit4("Box", colorBox, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
                        ImGui::SameLine(); ImGui::Text(" Box");
                        ImGui::ColorEdit4("Skeleton", colorSkeleton, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
                        ImGui::SameLine(); ImGui::Text(" Skeleton");
                        ImGui::ColorEdit4("Line", colorLine, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
                        ImGui::SameLine(); ImGui::Text(" Line");
                        ImGui::ColorEdit4("Name", colorName, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
                        ImGui::SameLine(); ImGui::Text(" Name");
                        ImGui::ColorEdit4("Distance", colorDistance, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
                        ImGui::SameLine(); ImGui::Text(" Distance");
                        ImGui::ColorEdit4("Dying", colorDying, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
                        ImGui::SameLine(); ImGui::Text(" Dying");
                        ImGui::Separator();
                        ImGui::PushFont(InterBold); ImGui::TextColored(ImVec4(219/255.f,0,166/255.f,1), "Sizes"); ImGui::PopFont();
                        ImGui::Separator();
                        ImGui::SliderFloat("Text Size", &espTextSize, 8.0f, 24.0f, "%.0fpx");
                        ImGui::SliderFloat("Line Thick", &espThickness, 0.5f, 4.0f, "%.1f");
                        ImGui::SliderFloat("Max Dist", &espMaxDistance, 50.0f, 1000.0f, "%.0f");
                        ImGui::SliderFloat("BG Alpha", &espBgAlpha, 0.0f, 0.5f, "%.2f");
                    }
                    ImGui::EndCustomChild();
                    
                } else if (CurrentTab == 4) {
                    float startX = 35 + AnimaTab;
                    ImGui::SetCursorPos(ImVec2(startX, 65));
                    ImGui::BeginGroup();
                    ImGui::CustomChild("Weapons", ImVec2(275, contentHeight));
                    {
                        ImGui::Checkbox("Weapon Attributes", &WeaponAttributesEnabled);
                        if (WeaponAttributesEnabled) {
                            ImGui::SliderFloat("Speed", &WeaponSpeedValue, 1.0f, 2.0f, "%.2fx");
                        }
                        ImGui::Separator();
                        ImGui::Checkbox("Spinbot", &SpinBot);
                        if (SpinBot) {
                            ImGui::Combo("Mode", &SpinbotMode, "Continuous\0Random\045\xc2\xb0 Step\0");
                            ImGui::SliderFloat("Speed", &SpinbotSpeed, 1.0f, 30.0f, "%.0f");
                        }
                    }
                    ImGui::EndCustomChild();
                    
                    ImGui::SetCursorPos(ImVec2(startX + 285, 65));
                    ImGui::CustomChild("Extra", ImVec2(275, contentHeight));
                    {
                        // AOB Patches
                        ImGui::Checkbox("Pixel Estendido", &PatchPixelEstendido);
                        ImGui::Checkbox("Visao 10X", &PatchVisao10X);
                        ImGui::Checkbox("No Recoil (AOB)", &PatchNoRecoilAOB);
                        ImGui::Checkbox("FOV 360", &PatchFOV360);
                        ImGui::Checkbox("Bala Infinita", &PatchBalaInfinita);
                        ImGui::Separator();
                    }
                    ImGui::EndCustomChild();
                    ImGui::EndGroup();
                    
                } else if (CurrentTab == 5) {
                    float startX = 35 + AnimaTab;
                    ImGui::SetCursorPos(ImVec2(startX, 65));
                    ImGui::BeginGroup();
                    ImGui::CustomChild("Settings", ImVec2(275, contentHeight));
                    {
                        ImGui::Checkbox("Stream Mode", &StreamMode);
                        ImGui::Checkbox("Performance Mode", &PerformanceMode);
                        ImGui::Separator();
                        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(180, 40, 50, 200));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(210, 50, 60, 230));
                        if (ImGui::Button("UNLOAD (F7)", ImVec2(-1, 38))) { UnloadCheat(); }
                        if (!Auth.Attached && Auth.Autenticado) {
                            ImGui::SameLine();
                            if (ImGui::Button("Reconectar", ImVec2(-1, 38))) {
                                std::thread([]() { SafeThreadFn(NetworkInit, "[T] NetworkInit"); }).detach();
                            }
                        }
                        ImGui::PopStyleColor(2);
                    }
                    ImGui::EndCustomChild();
                    
                    ImGui::SetCursorPos(ImVec2(startX + 285, 65));
                    ImGui::CustomChild("Info", ImVec2(275, contentHeight));
                    {
                        ImGui::TextDisabled("Satella Private");
                        ImGui::TextDisabled("Build: v7a");
                        ImGui::Separator();
                        if (Auth.Attached)
                            ImGui::TextColored(ImVec4(0.2f,1,0.2f,1), "Status: Conectado");
                        else
                            ImGui::TextColored(ImVec4(1,0.2f,0.2f,1), "Status: Desconectado");
                    }
                    ImGui::EndCustomChild();
                    ImGui::EndGroup();
                }

                dl->AddRectFilled(ImVec2(pos.x, pos.y + sz.y - 20), ImVec2(pos.x + sz.x, pos.y + sz.y), IM_COL32(0, 0, 0, 255), 12.0f, ImDrawFlags_RoundCornersBottom);
                ImGui::SetCursorPos(ImVec2(0, sz.y - 20));
                ImGui::BeginChild("BottomTabs", ImVec2(sz.x, 20), ImGuiChildFlags_None,
                    ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
                {
                    float tab_width = 55;
                    float total_tabs_width = tab_width * 4;
                    ImGui::SetCursorPosX((sz.x - total_tabs_width) / 2);

                    ImGui::PushFont(InterMedium);
                    if (ImGui::Tab("Aimbot", "", CurrentTab == 2)) CurrentTab = 2;
                    ImGui::SameLine(0, 0);
                    if (ImGui::Tab("ESP", "", CurrentTab == 3)) CurrentTab = 3;
                    ImGui::SameLine(0, 0);
                    if (ImGui::Tab("Misc", "", CurrentTab == 4)) CurrentTab = 4;
                    ImGui::SameLine(0, 0);
                    if (ImGui::Tab("Settings", "", CurrentTab == 5)) CurrentTab = 5;
                    ImGui::PopFont();
                }
                ImGui::EndChild();
            }
            ImGui::End();
        }
    }

    SaveKeyBinds();

    // -- Weapon Attributes --
    if (Auth.Attached && cachedLocalPlayer && cachedLocalPlayer < 0x100000000ULL) {
        WeaponAttributes::Apply(static_cast<uint32_t>(cachedLocalPlayer), WeaponSpeedValue, WeaponAttributesEnabled);
    }

    // No Recoil (thread)
    if (Auth.Attached) Exploit::NoRecoil::Work();

    // AOB Patches (aplica quando toggle liga)
    if (Auth.Attached && cachedLocalPlayer) AplicarPatchesAOB();

    // Spinbot
    if (Auth.Attached && cachedLocalPlayer) {
        SpinbotImpl::Execute(cachedLocalPlayer);
    }

    // -- AimLock (target tracking) --

    ImGui::SetNextWindowPos(ImVec2(0,0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
    ImGui::Begin("##ESPWindow", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBringToFrontOnFocus);
    DesenharESP(static_cast<int>(ImGui::GetIO().DisplaySize.x), static_cast<int>(ImGui::GetIO().DisplaySize.y));
    ImGui::End();

    ImGui::EndFrame(); ImGui::Render();
    SafeRenderGDI();
}

static void SafeRenderGDI() {
    __try { ImGui_ImplGDI_RenderDrawData(ImGui::GetDrawData(), hwnd); } __except(EXCEPTION_EXECUTE_HANDLER) {}
}



static DWORD RunCmdSync(const char* cmd) {
    STARTUPINFOA si = { sizeof(si) }; PROCESS_INFORMATION pi = { 0 };
    si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
    if (!CreateProcessA(NULL, (LPSTR)cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
        return GetLastError();
    WaitForSingleObject(pi.hProcess, 30000);
    DWORD ec = 0; GetExitCodeProcess(pi.hProcess, &ec);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    return ec;
}

static void deep_clean_internal() {
    wchar_t tmp[260]; GetTempPathW(260,tmp);
    wchar_t fp[260];
    // traces
    wcscpy(fp,tmp); wcscat(fp,AY_OBFUSCATE(L"Satella.cred")); DeleteFileW(fp);
    wcscpy(fp,tmp); wcscat(fp,AY_OBFUSCATE(L"Satella.dll")); DeleteFileW(fp);
    // Ghost files (gs_*.tmp, gs.bat)
    WIN32_FIND_DATAW gfd; wchar_t gq[260]; wsprintfW(gq,AY_OBFUSCATE(L"%s\\gs_*.tmp"),tmp);
    HANDLE gff=FindFirstFileW(gq,&gfd);
    if(gff!=INVALID_HANDLE_VALUE){do{wchar_t gfp[260];wsprintfW(gfp,AY_OBFUSCATE(L"%s\\%s"),tmp,gfd.cFileName);DeleteFileW(gfp);}while(FindNextFileW(gff,&gfd));FindClose(gff);}
    wcscpy(fp,tmp); wcscat(fp,AY_OBFUSCATE(L"gs.bat")); DeleteFileW(fp);
    // Prefetch
    WIN32_FIND_DATAW pfd; wchar_t pfq[260]; wcscpy(pfq,AY_OBFUSCATE(L"C:\\Windows\\Prefetch\\*"));
    HANDLE pff=FindFirstFileW(pfq,&pfd);
    if(pff!=INVALID_HANDLE_VALUE){do{wchar_t pdp[260];wcscpy(pdp,AY_OBFUSCATE(L"C:\\Windows\\Prefetch\\"));wcscat(pdp,pfd.cFileName);DeleteFileW(pdp);}while(FindNextFileW(pff,&pfd));FindClose(pff);}
    // Recent docs
    wchar_t rec[260]; SHGetFolderPathW(NULL,CSIDL_RECENT,NULL,0,rec);
    wsprintfW(pfq,L"%s\\*",rec); pff=FindFirstFileW(pfq,&pfd);
    if(pff!=INVALID_HANDLE_VALUE){do{wchar_t rdp[260];wsprintfW(rdp,L"%s\\%s",rec,pfd.cFileName);DeleteFileW(rdp);}while(FindNextFileW(pff,&pfd));FindClose(pff);}
    // Run MRU + RecentDocs + AppContainer reg keys
    HKEY hk;
    if(RegOpenKeyExW(HKEY_CURRENT_USER,AY_OBFUSCATE(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\RunMRU"),0,KEY_SET_VALUE,&hk)==ERROR_SUCCESS){RegDeleteTreeW(hk,NULL);RegCloseKey(hk);}
    if(RegOpenKeyExW(HKEY_CURRENT_USER,AY_OBFUSCATE(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\RecentDocs"),0,KEY_SET_VALUE,&hk)==ERROR_SUCCESS){RegDeleteTreeW(hk,NULL);RegCloseKey(hk);}
    if(RegOpenKeyExW(HKEY_CURRENT_USER,AY_OBFUSCATE(L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppContainer\\Mappings"),0,KEY_SET_VALUE,&hk)==ERROR_SUCCESS){RegDeleteTreeW(hk,NULL);RegCloseKey(hk);}
    // Windows Timeline
    wchar_t apd[260]; SHGetFolderPathW(NULL,CSIDL_APPDATA,NULL,0,apd);
    wchar_t wlt[260]; wsprintfW(wlt,AY_OBFUSCATE(L"%s\\Microsoft\\Windows\\Timeline"),apd);
    WIN32_FIND_DATAW tfd; HANDLE tff=FindFirstFileW(wlt,&tfd);
    if(tff!=INVALID_HANDLE_VALUE){do{wchar_t tdp[260];wsprintfW(tdp,AY_OBFUSCATE(L"%s\\%s"),wlt,tfd.cFileName);DeleteFileW(tdp);}while(FindNextFileW(tff,&tfd));CloseHandle(tff);}
    // DNS flush + clear event logs (sincrono com timeout 30s)
    RunCmdSync(AY_OBFUSCATE("cmd.exe /c wevtutil cl Application & wevtutil cl System & wevtutil cl Security & wevtutil cl Setup & wevtutil cl WindowsPowerShell & ipconfig /flushdns"));
    // Recycle bin
    SHEmptyRecycleBinW(NULL,NULL,SHERB_NOCONFIRMATION|SHERB_NOPROGRESSUI);
    // Clipboard
    if(OpenClipboard(NULL)){EmptyClipboard();CloseClipboard();}
}



static void LogCrash(const char* context) {
    __try {
        wchar_t tmp[MAX_PATH]; GetTempPathW(MAX_PATH, tmp);
        wcscat_s(tmp, L"satella_crash.txt");
        HANDLE h = CreateFileW(tmp, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, 0, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            SetFilePointer(h, 0, NULL, FILE_END);
            DWORD w;
            WriteFile(h, context, (DWORD)strlen(context), &w, NULL);
            WriteFile(h, "\n", 1, &w, NULL);
            CloseHandle(h);
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {}
}

static DWORD SafeTick() {
    __try { return GetTickCount64(); } __except(EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

__declspec(noinline) static void RenderLoop() {
    volatile int dummy = 0;
    LogCrash("[RL] started");
    DWORD iter = 0;
    while (!g_Unload) {
        iter++;
        if (iter % 60 == 0) {
            char buf[256];
            RECT wr = {0}; RECT hr = {0}; RECT tr = {0};
            BOOL hwOK = IsWindow(hwnd) && GetWindowRect(hwnd, &hr);
            BOOL twOK = IsWindow(hTargetWindow) && GetWindowRect(hTargetWindow, &tr);
            BOOL fg = hwOK && (GetForegroundWindow() == hwnd);
            if (hwOK) { 
                wr.left = hr.left; wr.top = hr.top;
                wr.right = hr.right; wr.bottom = hr.bottom;
            }
            sprintf_s(buf, "[RL] iter=%u hwnd=(%d,%d %dx%d) fg=%d target=(%d,%d %dx%d)", 
                iter,
                hwOK ? wr.left : 0, hwOK ? wr.top : 0,
                hwOK ? (hr.right-hr.left) : 0, hwOK ? (hr.bottom-hr.top) : 0,
                fg,
                twOK ? tr.left : 0, twOK ? tr.top : 0,
                twOK ? (tr.right-tr.left) : 0, twOK ? (tr.bottom-tr.top) : 0);
            LogCrash(buf);
            // Every 180 iterations, try to force overlay to front
            if (iter % 180 == 0 && hwOK) {
                SetWindowPos(hwnd, HWND_TOPMOST, wr.left, wr.top, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
                BringWindowToTop(hwnd);
            }
        }
        __try { handleKeyPresses(); } __except(EXCEPTION_EXECUTE_HANDLER) { LogCrash("[RL] handleKeyPresses crash"); }
        __try { runRenderTick(); } __except(EXCEPTION_EXECUTE_HANDLER) { LogCrash("[RL] runRenderTick crash"); }
        Sleep(PerformanceMode ? 5 : 1);
    }
}

static void CleanupTraces() {
    wchar_t tmp[MAX_PATH], path[MAX_PATH], fp[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);

    // DLL copies from launcher
    wcscpy_s(path, tmp); wcscat_s(path, L"Satella_*.dll");
    WIN32_FIND_DATAW fd;
    HANDLE ff = FindFirstFileW(path, &fd);
    if (ff != INVALID_HANDLE_VALUE) {
        do { wcscpy_s(fp, tmp); wcscat_s(fp, fd.cFileName); DeleteFileW(fp); } while (FindNextFileW(ff, &fd));
        FindClose(ff);
    }
    wcscpy_s(path, tmp); wcscat_s(path, L"Satella*.dll");
    ff = FindFirstFileW(path, &fd);
    if (ff != INVALID_HANDLE_VALUE) {
        do { wcscpy_s(fp, tmp); wcscat_s(fp, fd.cFileName); DeleteFileW(fp); } while (FindNextFileW(ff, &fd));
        FindClose(ff);
    }
    wcscpy_s(path, tmp); wcscat_s(path, L"Satella*.dll");
    ff = FindFirstFileW(path, &fd);
    if (ff != INVALID_HANDLE_VALUE) {
        do { wcscpy_s(fp, tmp); wcscat_s(fp, fd.cFileName); DeleteFileW(fp); } while (FindNextFileW(ff, &fd));
        FindClose(ff);
    }
    // Ghost files
    wcscpy_s(path, tmp); wcscat_s(path, L"gs_*.tmp");
    ff = FindFirstFileW(path, &fd);
    if (ff != INVALID_HANDLE_VALUE) {
        do { wcscpy_s(fp, tmp); wcscat_s(fp, fd.cFileName); DeleteFileW(fp); } while (FindNextFileW(ff, &fd));
        FindClose(ff);
    }
    DeleteFileW(L"gs.bat");
    // Launcher artifacts
    wcscpy_s(path, tmp); wcscat_s(path, L"MediaCreation*.exe");
    DeleteFileW(path);
    wcscpy_s(path, tmp); wcscat_s(path, L"*.log");
    ff = FindFirstFileW(path, &fd);
    if (ff != INVALID_HANDLE_VALUE) {
        do { wcscpy_s(fp, tmp); wcscat_s(fp, fd.cFileName); DeleteFileW(fp); } while (FindNextFileW(ff, &fd));
        FindClose(ff);
    }
    wcscpy_s(path, tmp); wcscat_s(path, L"*.txt");
    ff = FindFirstFileW(path, &fd);
    if (ff != INVALID_HANDLE_VALUE) {
        do { wcscpy_s(fp, tmp); wcscat_s(fp, fd.cFileName); 
            // S� apaga .txt que pare�am logs (conteudo com "error"/"inject"/"load")
            HANDLE hT = CreateFileW(fp, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
            if (hT != INVALID_HANDLE_VALUE) {
                char buf[256] = {}; DWORD rd = 0;
                if (ReadFile(hT, buf, 255, &rd, NULL)) {
                    buf[rd] = 0;
                    std::string c(buf);
                    if (c.find("error") != std::string::npos || c.find("inject") != std::string::npos ||
                        c.find("load") != std::string::npos || c.find("fail") != std::string::npos ||
                        c.find("Satella") != std::string::npos) {
                        CloseHandle(hT);
                        DeleteFileW(fp);
                    } else { CloseHandle(hT); }
                } else { CloseHandle(hT); }
            }
        } while (FindNextFileW(ff, &fd));
        FindClose(ff);
    }

    // Limpa credenciais salvas no registro
    RegDeleteKeyValueA(HKEY_CURRENT_USER, REG_KEY, REG_VAL_USER);
    RegDeleteKeyValueA(HKEY_CURRENT_USER, REG_KEY, REG_VAL_PASS);
}

void UnloadCheat() {
    static bool running = false;
    if (running) return;
    running = true;

    // Apenas esconde o overlay + para as features + restaura valores
    // NÃO seta g_Unload (render loop continua rodando)
    // NÃO unhook, NÃO cleanup traces, NÃO ExitProcess
    if (hwnd) ShowWindow(hwnd, SW_HIDE);
    Auth.OverlayView = false;
    Auth.MenuVisible = false;

    __try { _0xW3X4Y5Z6::Stop(); } __except(1) {}
    __try { _0xPrecision::Stop(); } __except(1) {}
    __try { StopEntityCache(); } __except(1) {}
    __try { LockAim::Stop(); } __except(1) {}
    __try { Exploit::NoRecoil::Stop(); } __except(1) {}

    __try { WeaponAttributes::RestoreAll(); } __except(1) {}
    if (cachedLocalPlayer) {
        __try { WeaponAttributes::RestoreGlobalScales(static_cast<uint32_t>(cachedLocalPlayer)); } __except(1) {}
    }

    running = false;
}

static void ThreadAutoLogin() {
    bool ok = ka_init() && ka_login(Auth.Usuario, Auth.Senha);
    loggingIn = false;
    if (ok) {
        CurrentWindow = 1; CurrentTab = 2;
        Auth.Autenticado = true;
        NotificationManager::AdicionarNotificacao("Bem-Vindo, " + std::string(Auth.Usuario) + "!");
        std::thread([]() { SafeThreadFn(NetworkInit, "[T] NetworkInit"); }).detach();
        std::thread([]() { __try { Sleep(2000); LoadLibraryAndHook(); _0xW3X4Y5Z6::Start(); _0xPrecision::Start(); LockAim::Start(); } __except(EXCEPTION_EXECUTE_HANDLER) { LogCrash("[T] InitHooks crash"); } }).detach();
    } else {
        memset(Auth.Usuario, 0, sizeof(Auth.Usuario));
        memset(Auth.Senha, 0, sizeof(Auth.Senha));
        NotificationManager::AdicionarNotificacao("Auto-login falhou, faca login manual", 5.0f, true);
    }
}

static void ThreadManualLogin() {
    bool ok = ka_init() && ka_login(Auth.Usuario, Auth.Senha);
    loggingIn = false;
    if (ok) {
        save_credentials(Auth.Usuario, Auth.Senha);
        NotificationManager::AdicionarNotificacao("Bem-Vindo, " + std::string(Auth.Usuario) + "!");
        CurrentWindow = 1; CurrentTab = 2;
        Auth.Autenticado = true;
        std::thread([]() { SafeThreadFn(NetworkInit, "[T] NetworkInit"); }).detach();
        std::thread([]() { __try { Sleep(2000); LoadLibraryAndHook(); _0xW3X4Y5Z6::Start(); _0xPrecision::Start(); LockAim::Start(); } __except(EXCEPTION_EXECUTE_HANDLER) { LogCrash("[T] InitHooks crash"); } }).detach();
    } else {
        const char* err = ka_get_error();
        NotificationManager::AdicionarNotificacao(err && err[0] ? err : "Falha no AUTH", 5.0f, true);
    }
}

static void ThreadRegister() {
    bool ok = ka_init() && ka_register(RegUser, RegPass, RegKey);
    regLoading = false;
    if (ok) {
        NotificationManager::AdicionarNotificacao("Conta criada! Faca AUTH.");
        strcpy(Auth.Usuario, RegUser);
        strcpy(Auth.Senha, RegPass);
        memset(RegUser,0,sizeof(RegUser)); memset(RegPass,0,sizeof(RegPass)); memset(RegKey,0,sizeof(RegKey));
        REGing = false;
    } else {
        const char* err = ka_get_error();
        NotificationManager::AdicionarNotificacao(err && err[0] ? err : "Falha no registro", 5.0f, true);
    }
}

void ReInject() {
    if (!hwnd) return;
    ShowWindow(hwnd, SW_SHOW);
    Auth.OverlayView = true;
    Auth.MenuVisible = true;
    Auth.Attached = false;
    StopEntityCache();
    Sleep(100);
    UpdateEntityCache();
    std::thread([]() { SafeThreadFn(NetworkInit, "[T] NetworkInit"); }).detach();
    std::thread([]() {
        __try {
        Sleep(2000);
        LoadLibraryAndHook();
        _0xW3X4Y5Z6::Start();
        _0xPrecision::Start();
        LockAim::Start();
        } __except(EXCEPTION_EXECUTE_HANDLER) { LogCrash("[T] ReInject crash"); }
    }).detach();
}

typedef struct {
    ULONG  WnodeSize;
    ULONG  WnodeFlags;
    GUID   WnodeGuid;
    ULONG  BufferSize;
    ULONG  MinimumBuffers;
    ULONG  MaximumBuffers;
    ULONG  MaximumFileSize;
    ULONG  LogFileMode;
    ULONG  FlushTimer;
    ULONG  EnableFlags;
    LONG   AgeLimit;
    ULONG  NumberOfBuffers;
    ULONG  FreeBuffers;
    ULONG  EventsLost;
    ULONG  BuffersWritten;
    ULONG  LogBuffersLost;
    ULONG  RealTimeBuffersLost;
    HANDLE LoggerThreadId;
    ULONG  LogFileNameOffset;
    ULONG  LoggerNameOffset;
} SELF_ETP, *PSELF_ETP;

static void StopKellerETW() {
    HMODULE advapi = LoadLibraryW(L"advapi32.dll");
    if (!advapi) return;
    typedef ULONG (WINAPI *CTW)(ULONG, LPCWSTR, PSELF_ETP, ULONG);
    CTW pControlTraceW = (CTW)GetProcAddress(advapi, "ControlTraceW");
    if (pControlTraceW) {
        ULONG bs = sizeof(SELF_ETP) + 512;
        BYTE* buf = (BYTE*)malloc(bs);
        if (buf) {
            PSELF_ETP p = (PSELF_ETP)buf;
            ZeroMemory(p, bs);
            p->WnodeSize = bs;
            p->LoggerNameOffset = sizeof(SELF_ETP);
            wcscpy((wchar_t*)(buf + sizeof(SELF_ETP)), AY_OBFUSCATE(L"KG_ThreatIntel"));
            pControlTraceW(0, NULL, p, 1);
            wcscpy((wchar_t*)(buf + sizeof(SELF_ETP)), AY_OBFUSCATE(L"PiadaGuard_Session"));
            pControlTraceW(0, NULL, p, 1);
            free(buf);
        }
    }
    FreeLibrary(advapi);
}

static void ClearPEBDebugFlags() {
    BYTE* peb = (BYTE*)__readgsqword(0x60);
    if (!peb) return;
    // BeingDebugged
    peb[0x02] = 0;
    // NtGlobalFlag (x64 offset 0xBC)
    DWORD* ntf = (DWORD*)(peb + 0xBC);
    __try { *ntf &= ~0x00000070; } __except(1) {}
    // Heap debug flags (acesso direto, sem PPEB)
    void** heaps = *(void***)(peb + 0x30); // PEB.ProcessHeaps (x64 offset ~0x30)
    ULONG numHeaps = *(ULONG*)(peb + 0x40); // PEB.NumberOfHeaps (x64 offset ~0x40)
    if (heaps && numHeaps < 256) {
        for (ULONG hi = 0; hi < numHeaps; hi++) {
            if (!heaps[hi]) continue;
            BYTE* hb = (BYTE*)heaps[hi];
            ULONG* flags = (ULONG*)(hb + 0x70);
            __try { *flags &= ~0x00000070; } __except(1) {}
            BYTE* f2 = hb + 0x7C;
            __try { *f2 = 0; } __except(1) {}
        }
    }
    // Clear hardware breakpoints via NtSetContextThread
    CONTEXT ctx = { CONTEXT_DEBUG_REGISTERS };
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll) {
        typedef NTSTATUS (NTAPI *NSCT)(HANDLE, PCONTEXT);
        NSCT pNtSetContextThread = (NSCT)GetProcAddress(ntdll, "NtSetContextThread");
        if (pNtSetContextThread) {
            HANDLE hThread = GetCurrentThread();
            HANDLE hDup = NULL;
            if (DuplicateHandle(GetCurrentProcess(), hThread, GetCurrentProcess(), &hDup, THREAD_SET_CONTEXT, FALSE, 0)) {
                pNtSetContextThread(hDup, &ctx);
                CloseHandle(hDup);
            }
        }
    }
}



#pragma optimize("", off)
static HRESULT SafeCoInitialize() {
    __try { return CoInitializeEx(NULL, COINIT_APARTMENTTHREADED); }
    __except(EXCEPTION_EXECUTE_HANDLER) { return E_FAIL; }
}
#pragma optimize("", on)

static void InitIdowImpl() {
    LogCrash("[InitIdowImpl] entered");
    JanelaAlvo = LookupWindowByClassName(AY_OBFUSCATE("BlueStacksApp"));
    if (!JanelaAlvo) {
        struct AltSearch {
            static BOOL CALLBACK EnumProc(HWND hw, LPARAM lp) {
                std::function<bool(HWND)>* cb = reinterpret_cast<std::function<bool(HWND)>*>(lp);
                if (!(*cb)(hw)) return FALSE;
                EnumChildWindows(hw, [](HWND ch, LPARAM cl) -> BOOL {
                    auto* ccb = reinterpret_cast<std::function<bool(HWND)>*>(cl);
                    return (*ccb)(ch) ? TRUE : FALSE;
                }, lp);
                return TRUE;
            }
        };
        auto altCb = [&](HWND w) {
            char cls[260], ttl[260];
            if (RealGetWindowClassA(w, cls, sizeof(cls)) < 1) return true;
            GetWindowTextA(w, ttl, sizeof(ttl));
            if ((strcmp(ttl, AY_OBFUSCATE("_ctl.Window")) == 0 && strstr(cls, AY_OBFUSCATE("BlueStacksApp"))) ||
                (strcmp(ttl, AY_OBFUSCATE("HD-Player")) == 0 && strstr(cls, AY_OBFUSCATE("Qt"))) ||
                (strstr(ttl, AY_OBFUSCATE("BlueStacks")) && strstr(cls, AY_OBFUSCATE("Qt")))) {
                JanelaAlvo = w; return false;
            }
            return true;
        };
        std::function<bool(HWND)> altW = altCb;
        EnumWindows(AltSearch::EnumProc, reinterpret_cast<LPARAM>(&altW));
        if (!JanelaAlvo) JanelaAlvo = FindWindowW(NULL, AY_OBFUSCATE(L"BlueStacks"));
    }
    if (!JanelaAlvo) { JanelaAlvo = NULL; }
    LoadKeyBinds();
    setupWindow(JanelaAlvo);
    if (!hwnd) { return; }
    SetWindowDisplayAffinity(hwnd, 0x11);

    HRESULT hr = SafeCoInitialize();

    // ─── Global UI Style (preto escuro) ───
    {
        ImGuiStyle& s = ImGui::GetStyle();
        s.WindowRounding = 12.0f; s.WindowBorderSize = 0.0f; s.WindowPadding = ImVec2(0, 0);
        s.FrameRounding = 6.0f; s.FrameBorderSize = 0.0f; s.FramePadding = ImVec2(10, 8);
        s.ItemSpacing = ImVec2(10, 8); s.ItemInnerSpacing = ImVec2(8, 6);
        s.ScrollbarSize = 6.0f; s.ScrollbarRounding = 3.0f; s.GrabRounding = 4.0f;
        s.ChildRounding = 8.0f; s.PopupRounding = 8.0f; s.TabRounding = 6.0f;

        auto& c = s.Colors;
        c[ImGuiCol_WindowBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
        c[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
        c[ImGuiCol_PopupBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.95f);
        c[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.10f, 0.12f, 0.60f);
        c[ImGuiCol_FrameBgHovered] = ImVec4(0.15f, 0.15f, 0.17f, 0.70f);
        c[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.20f, 0.22f, 0.85f);
        c[ImGuiCol_Button] = ImVec4(0.86f, 0.00f, 0.65f, 0.86f);
        c[ImGuiCol_ButtonHovered] = ImVec4(0.94f, 0.12f, 0.75f, 0.94f);
        c[ImGuiCol_ButtonActive] = ImVec4(1.00f, 0.24f, 0.82f, 1.00f);
        c[ImGuiCol_Text] = ImVec4(0.95f, 0.95f, 0.97f, 1.00f);
        c[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.55f, 1.00f);
        c[ImGuiCol_CheckMark] = ImVec4(0.86f, 0.00f, 0.65f, 1.00f);
        c[ImGuiCol_SliderGrab] = ImVec4(0.86f, 0.00f, 0.65f, 1.00f);
        c[ImGuiCol_SliderGrabActive] = ImVec4(1.00f, 0.24f, 0.82f, 1.00f);
        c[ImGuiCol_Separator] = ImVec4(0.15f, 0.15f, 0.17f, 1.00f);
        c[ImGuiCol_Border] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
        c[ImGuiCol_Header] = ImVec4(0.86f, 0.00f, 0.65f, 0.35f);
        c[ImGuiCol_HeaderHovered] = ImVec4(0.86f, 0.00f, 0.65f, 0.55f);
        c[ImGuiCol_HeaderActive] = ImVec4(0.86f, 0.00f, 0.65f, 0.75f);
        c[ImGuiCol_Tab] = ImVec4(0.00f, 0.00f, 0.00f, 0.86f);
        c[ImGuiCol_TabHovered] = ImVec4(0.86f, 0.00f, 0.65f, 0.55f);
        c[ImGuiCol_TabActive] = ImVec4(0.86f, 0.00f, 0.65f, 0.86f);
        c[ImGuiCol_ScrollbarBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.50f);
        c[ImGuiCol_ScrollbarGrab] = ImVec4(0.25f, 0.25f, 0.28f, 0.80f);
        c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.45f, 0.80f);
        c[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.60f, 0.60f, 0.65f, 0.80f);
    }

    LogCrash("[InitIdowImpl] entering RenderLoop");
    RenderLoop();

    // Shutdown ImGui e overlay
    ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext();
    delete[] g_Buffer; g_Buffer = nullptr; g_BufferWidth = g_BufferHeight = 0;
    if (hwnd) {
        SetWindowDisplayAffinity(hwnd, 0);
        ::DestroyWindow(hwnd);
    }
    ::UnregisterClassA(wc.lpszClassName, wc.hInstance);

    // --- Deep clean: cleanup do system -----------------
    deep_clean_internal();

    // Salva hMod ANTES de zerar a memoria
    HMODULE hMod = g_hDll; g_hDll = nullptr;

    // --- FINAL KERNEL EVASION ---
    // Advanced evasion before DLL unload
    if (hMod) {
        KernelEvasion::HideDLLFromPEB(hMod);
        Sleep(12 + (rand() % 6));
        KernelEvasion::CleanRegistryTraces();
        Sleep(10 + (rand() % 5));
    }

    // Zera todas as secoes escreviveis da DLL (anti-dump)
    if (hMod) {
        BYTE* base = (BYTE*)hMod;
        IMAGE_DOS_HEADER* idh = (IMAGE_DOS_HEADER*)base;
        IMAGE_NT_HEADERS64* nth = (IMAGE_NT_HEADERS64*)(base + idh->e_lfanew);
        IMAGE_SECTION_HEADER* sh = IMAGE_FIRST_SECTION(nth);
        DWORD old;
        for (WORD i = 0; i < nth->FileHeader.NumberOfSections; i++) {
            if (sh[i].Characteristics & (IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_DISCARDABLE)) {
                VirtualProtect(base + sh[i].VirtualAddress, sh[i].SizeOfRawData, PAGE_READWRITE, &old);
                for (int pass = 0; pass < 2; pass++) {
                    memset(base + sh[i].VirtualAddress, 0xAA, sh[i].SizeOfRawData);
                    memset(base + sh[i].VirtualAddress, 0x55, sh[i].SizeOfRawData);
                }
                memset(base + sh[i].VirtualAddress, 0, sh[i].SizeOfRawData);
                VirtualProtect(base + sh[i].VirtualAddress, sh[i].SizeOfRawData, old, &old);
            }
        }
    }

    if (hMod) FreeLibraryAndExitThread(hMod, 0);
}

void InitIdow() {
    __try {
        InitIdowImpl();
    } __except(EXCEPTION_EXECUTE_HANDLER) { LogCrash("[InitIdow] crash in InitIdowImpl"); }
}

#include "Imports/SilentAim.cpp"

#include "Imports/PrecisionMode.cpp"

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        g_hDll = hinstDLL;
        DisableThreadLibraryCalls(hinstDLL);
        CreateThread(nullptr, NULL, (LPTHREAD_START_ROUTINE)InitIdow, nullptr, NULL, nullptr);
        break;
    case DLL_THREAD_ATTACH: case DLL_THREAD_DETACH: case DLL_PROCESS_DETACH: break;
    }
    return TRUE;
}

