#if !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif
#pragma execution_character_set("utf-8")
#include "Imports/includes.h"
#include "Main.h"
#include "Imports/EntityCache.cpp"
#include <chrono>
#include <ctime>
#include <unordered_map>
#include <shlobj.h>
#include <shellapi.h>
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
#include "Imports/SharedConfig.hpp"
#include "Unity/Quaternion.h"

// ─── Stream Mode Auto-Detection ───
static const wchar_t* g_StreamingProcesses[] = {
    L"obs64.exe", L"obs32.exe", L"obs-browser-page.exe", L"obs-virtualcam.exe",
    L"Discord.exe", L"discord.exe",
    L"anydesk.exe", L"AnyDesk.exe",
    L"TeamViewer.exe", L"teamviewer.exe",
    L"xsplit.core.exe", L"XSplit.Core.exe",
    L"streamlabsobs64.exe", L"streamlabsobs32.exe", L"Streamlabs OBS.exe",
    L"twitchstudio.exe", L"TwitchStudio.exe",
    L"ffmpeg.exe",
    L"vlc.exe", L"VLC.exe",
    L"obs-virtualsource-manager.exe",
    L"displayfusion.exe",
    L"capture.exe"
};
static DWORD g_LastStreamCheck = 0;
static bool DetectStreamingSoftware() {
    DWORD now = GetTickCount();
    if (now - g_LastStreamCheck < 2000) return StreamModeActive;
    g_LastStreamCheck = now;

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return StreamModeActive;

    PROCESSENTRY32W pe = { sizeof(pe) };
    bool found = false;
    if (Process32FirstW(snap, &pe)) {
        do {
            for (int i = 0; i < ARRAYSIZE(g_StreamingProcesses); i++) {
                if (_wcsicmp(pe.szExeFile, g_StreamingProcesses[i]) == 0) {
                    found = true;
                    break;
                }
            }
        } while (!found && Process32NextW(snap, &pe));
    }
    CloseHandle(snap);

    StreamModeActive = found;
    return found;
}

#include "Imports/Cleaner.h"
#include "Imports/Cleaner.cpp"
//#include "../aimbot/RageAimbot.cpp"
#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x11
#endif
HINSTANCE g_hDll = nullptr;
bool g_Unload = false;
bool g_d3dReady = false;
ImFont* FontAwesomeRegular = nullptr;
ImFont* FontAwesomeSolid14 = nullptr;
ImFont* FontAwesomeBrands = nullptr;
void UnloadCheat();
extern "C" __declspec(dllexport) void TriggerUnload() { UnloadCheat(); }

static uint64_t ReadLocalPlayer() {
    if (il2cpp < 0x10000) return 0;
    uintptr_t b = Ler<uintptr_t>(il2cpp + Offsets::InitBase);
    if (b < 0x10000) return 0;
    uintptr_t f = Ler<uintptr_t>(b);
    if (f < 0x10000) return 0;
    uintptr_t sf = Ler<uintptr_t>(f + Offsets::StaticClass);
    if (sf < 0x10000) return 0;
    uintptr_t ge = Ler<uintptr_t>(sf);
    if (ge < 0x10000) return 0;
    uintptr_t cm = Ler<uintptr_t>(ge + Offsets::CurrentMatch);
    if (cm < 0x10000) return 0;
    int ms = Ler<int>(cm + Offsets::MatchStatus);
    if (ms != 1) return 0;
    return Ler<uintptr_t>(cm + Offsets::LocalPlayer);
}

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


}

// --- ESP -----------------------------------------------------

void DesenharESP(int width, int height) {
    espOffsetX = 0; espOffsetY = 0;
    SWidth = width; SHeight = height;

    if (!Auth.AtivarFuncoes || !Auth.Attached) return;
    AimbotDistMax = 9999.9f; AimbotTarget = 0;

    // ─── Leitura direta (sem cache thread) ───
    uintptr_t base = Ler<uintptr_t>(il2cpp + Offsets::InitBase);
    if (base == 0 || base < 0x10000) return;
    uintptr_t facade = Ler<uintptr_t>(base);
    if (facade == 0 || facade < 0x10000) return;
    uintptr_t staticFacade = Ler<uintptr_t>(facade + Offsets::StaticClass);
    if (staticFacade == 0 || staticFacade < 0x10000) return;
    uintptr_t GameEngine = Ler<uintptr_t>(staticFacade);
    if (GameEngine == 0 || GameEngine < 0x10000) return;

    uintptr_t Partida = Ler<uintptr_t>(GameEngine + Offsets::CurrentMatch);
    if (Partida == 0) return;
    int MatchState = Ler<int>(Partida + Offsets::MatchStatus);
    if (MatchState != 1) return;

    uintptr_t JogadorLocal = Ler<uintptr_t>(Partida + Offsets::LocalPlayer);
    if (JogadorLocal == 0 || JogadorLocal < 0x10000) return;

    // Camera
    uintptr_t CameraControllerManager = Ler<uintptr_t>(GameEngine + 0x74);
    if (CameraControllerManager == 0) return;
    uintptr_t Camera = Ler<uintptr_t>(CameraControllerManager + 0x10);
    if (Camera == 0) return;
    uintptr_t IntPtrCam = Ler<uintptr_t>(Camera + 0x8);
    if (IntPtrCam == 0) return;
    UnityMatrix matrix = Ler<UnityMatrix>(IntPtrCam + Offsets::ViewMatrix);

    Vector3 minhaPos = Transform_ObterPosicao(Ler<uintptr_t>(JogadorLocal + Offsets::MainTransform));

    // Deteccao de offsets
    DetectAndSetOffsets();

    // Entidades
    uintptr_t dict = Ler<uintptr_t>(GameEngine + Offsets::DictionaryEntities);
    if (dict == 0) return;
    uintptr_t list = Ler<uintptr_t>(dict + Offsets::Il2CppDictionaryDataPtr);
    if (list == 0) return;
    list += 0x10;
    int cnt = Ler<int>(dict + Offsets::Il2CppDictionaryCount);
    if (cnt < 1 || cnt > 200) return;

    int entityCount = 0;
    for (int i = 0; i < cnt; i++) {
        uintptr_t e = Ler<uintptr_t>(list + (i * 0x10) + 0xC);
        if (e == 0 || e == JogadorLocal) continue;

        uintptr_t am = Ler<uintptr_t>(e + Offsets::AvatarManager);
        if (am == 0 || am < 0x10000) continue;
        uintptr_t uas = Ler<uintptr_t>(am + Offsets::UmaAvatarSimple);
        if (uas == 0 || uas < 0x10000) continue;
        uintptr_t ud = Ler<uintptr_t>(uas + Offsets::UMAData);
        if (ud == 0 || ud < 0x10000) continue;
        uintptr_t pri = Ler<uintptr_t>(e + Offsets::PRIDataPool);
        if (pri == 0 || pri < 0x10000) continue;
        uintptr_t rdu = Ler<uintptr_t>(Ler<uintptr_t>(pri + Offsets::ReplicationDataPoolUnsafe) + Offsets::ReplicationDataUnsafe);
        if (rdu == 0 || rdu < 0x10000) continue;
        short health = Ler<short>(rdu + Offsets::Health);
        if (health <= 0) continue;

        bool dying = false;
        uintptr_t pd = Ler<uintptr_t>(e + Offsets::Player_Data);
        if (pd != 0 && pd > 0x10000) dying = (Ler<int>(pd + Offsets::Player_IsDead) == 8);

        bool isTeam = Ler<bool>(ud + Offsets::TeamMate);
        if (isTeam && !ESPMostrarTime) continue;
        if (dying && !ESPMostrarDerrubado) continue;

        bool garota = Ler<bool>(e + Offsets::CDOBMFNCJHD);
        uintptr_t mtOff = g_AutoMainTransform ? g_AutoMainTransform : Offsets::MainTransform;
        Vector3 bodyPos = Transform_ObterPosicao(Ler<uintptr_t>(e + mtOff));
        Vector3 headPos = GetHeadPosition(e);

        float dist = Vector3::Distance(bodyPos, minhaPos);
        bool inEspRange = dist <= espMaxDistance;
        if (!inEspRange && !AimbotLegit) continue;

        std::string pName = "BOT";
        uintptr_t bpi = Ler<uintptr_t>(e + Offsets::Player_Name);
        if (bpi != 0) {
            uintptr_t pn = Ler<uintptr_t>(bpi + 0x18);
            if (pn != 0) {
                int nc = Ler<int>(pn + 0x8);
                pName = ObterStr(pn + 0xC, nc);
            }
        }

        std::string wpnName;
        uintptr_t wpn = Ler<uintptr_t>(e + Offsets::Weapon);
        if (wpn > 0x10000) {
            uintptr_t wpnd = Ler<uintptr_t>(wpn + Offsets::WeaponData);
            if (wpnd > 0x10000) {
                auto wpnBpi = Ler<uintptr_t>(wpnd + 0x8);
                if (wpnBpi != 0) {
                    auto wpnPn = Ler<uintptr_t>(wpnBpi + 0x18);
                    if (wpnPn != 0) {
                        int wpnNc = Ler<int>(wpnPn + 0x8);
                        wpnName = ObterStr(wpnPn + 0xC, wpnNc);
                    }
                }
            }
        }

        ImVec4 col = dying ? ImVec4(colorDying[0],colorDying[1],colorDying[2],colorDying[3])
                           : ImVec4(colorName[0],colorName[1],colorName[2],colorName[3]);
        float ox = espOffsetX, oy = espOffsetY;

        Vector3 WorldEnemyHeadPos = World2Screen(matrix, headPos);
        Vector3 WorldEnemyFootPos = World2Screen(matrix, bodyPos);
        if (WorldEnemyHeadPos.Z != 0 || WorldEnemyFootPos.Z != 0) continue;
        entityCount++;

        // Aimbot target selection
        if (AimbotLegit && !isTeam) {
            float adx = WorldEnemyHeadPos.X - (SWidth * 0.5f);
            float ady = WorldEnemyHeadPos.Y - (SHeight * 0.5f);
            float fov = (float)AimbotFOV;
            float maxDist = (float)AimbotMaxDistance;
            bool skipKnocked = AimbotIgnoreKnocked && dying;
            bool skipBot = AimbotIgnoreBots && Ler<bool>((uint32_t)(e + Offsets::IsClientBot));
            bool notVisible = AimVisibleCheck && !Ler<bool>(uas + Offsets::Avatar_IsVisible);
            if (dist <= maxDist && !skipKnocked && !skipBot && !notVisible && adx*adx + ady*ady <= fov*fov) {
                float d = sqrtf(adx*adx + ady*ady);
                if (d < AimbotDistMax) { AimbotDistMax = d; AimbotTarget = e; }
            }
        }

        if (!inEspRange) continue;

        float Height = WorldEnemyFootPos.Y - WorldEnemyHeadPos.Y;
        if (Height < 10.f) Height = 10.f;
        if (Height > 350.f) Height = 350.f;
        float Width = Height * 0.40f;
        float bx = WorldEnemyHeadPos.X - Width*0.5f;
        float by = WorldEnemyHeadPos.Y;

        if (ESPNome) {
            const char* name = pName.c_str();
            float tw = ImGui::CalcTextSize(name).x;
            float tx = WorldEnemyHeadPos.X+ox - tw*0.5f;
            float ty = by+oy - espTextSize - 2;
            if (espBgAlpha > 0.01f)
                DrawRectFilledIm(tx-3, ty-2, tw+6, espTextSize+4, 0,0,0,espBgAlpha);
            DrawTextShadowIm(name, tx, ty, col.x, col.y, col.z, espTextSize);
        }

        if (ESPHealthText) {
            char hBuf[16]; snprintf(hBuf, sizeof(hBuf), "%d HP", health);
            float htw = ImGui::CalcTextSize(hBuf).x;
            float htx = WorldEnemyHeadPos.X+ox - htw*0.5f;
            float hty = by+oy - espTextSize - 2 - (ESPNome ? espTextSize + 2 : 0);
            if (espBgAlpha > 0.01f)
                DrawRectFilledIm(htx-3, hty-2, htw+6, espTextSize+4, 0,0,0,espBgAlpha);
            DrawTextShadowIm(hBuf, htx, hty, col.x, col.y, col.z, espTextSize);
        }

        if (ESPWeaponName && !wpnName.empty()) {
            float wtw = ImGui::CalcTextSize(wpnName.c_str()).x;
            float wtx = WorldEnemyHeadPos.X+ox - wtw*0.5f;
            float wty = by+oy + Height + 2 + (ESPDistancia ? espTextSize + 2 : 0);
            if (espBgAlpha > 0.01f)
                DrawRectFilledIm(wtx-3, wty-2, wtw+6, espTextSize+4, 0,0,0,espBgAlpha);
            DrawTextShadowIm(wpnName.c_str(), wtx, wty, colorBox[0], colorBox[1], colorBox[2], espTextSize);
        }

        float rawH = WorldEnemyFootPos.Y - WorldEnemyHeadPos.Y;
        if (ESPLinha && rawH > 5.0f) {
            float lineEndY = WorldEnemyHeadPos.Y+oy+Height+(ESPDistancia?espTextSize+2:1)+(ESPWeaponName&&!wpnName.empty()?espTextSize+2:0);
            float sx = WorldEnemyHeadPos.X+ox;
            if (linePosition==1 && sx > 0 && sx < width && lineEndY > 0 && lineEndY < height)
                DrawLineIm(width/2,height,sx,lineEndY,espThickness,col.x,col.y,col.z);
            if (linePosition==0 && sx > 0 && sx < width)
                DrawLineIm(width/2,0,sx,WorldEnemyHeadPos.Y+oy-(ESPNome?espTextSize+2:1)-(ESPHealthText?espTextSize+2:0),espThickness,col.x,col.y,col.z);
        }

        if (ESPCaixa == 1) {
            if (ESPFilledBox)
                DrawRectFilledIm(bx+ox, by+oy, Width, Height, col.x*0.15f, col.y*0.15f, col.z*0.15f, 0.3f);
            DrawBoxIm(bx+ox, by+oy, Width, Height, espThickness, colorBox[0], colorBox[1], colorBox[2]);
        } else if (ESPCaixa == 2) {
            if (ESPFilledBox)
                DrawRectFilledIm(bx+ox, by+oy, Width, Height, col.x*0.15f, col.y*0.15f, col.z*0.15f, 0.3f);
            DrawCornerBoxIm(bx+ox, by+oy, Width, Height, Width*0.25f, espThickness, colorBox[0], colorBox[1], colorBox[2]);
        }

        // Health Bar
        if (ESPHealthBarPos == 1)
            DrawHealthBarIm(bx+ox,by+oy,Width,Height,health,200);
        else if (ESPHealthBarPos == 2) {
            float barW = 4;
            float barX = bx + ox + Width + 2;
            auto* dl = ESP_DL();
            dl->AddRectFilled(ImVec2(barX, by+oy), ImVec2(barX + barW, by+oy + Height), IM_COL32(40, 40, 40, 130));
            float pct = (float)health / 200.0f; if (pct < 0) pct = 0; if (pct > 1) pct = 1;
            if (pct > 0) {
                float fillH = Height * pct;
                ImU32 fillCol = pct <= 0.3f ? IM_COL32(255,50,50,255) : (pct <= 0.6f ? IM_COL32(255,255,50,255) : IM_COL32(50,255,50,255));
                dl->AddRectFilled(ImVec2(barX+1, by+oy+Height-fillH), ImVec2(barX+barW-1, by+oy+Height), fillCol);
            }
        } else if (ESPHealthBarPos == 3) {
            float barH = 3;
            auto* dl = ESP_DL();
            dl->AddRectFilled(ImVec2(bx+ox, by+oy-barH-1), ImVec2(bx+ox+Width, by+oy-1), IM_COL32(40,40,40,130));
            float pct = (float)health / 200.0f; if (pct < 0) pct = 0; if (pct > 1) pct = 1;
            if (pct > 0) {
                float fillW = Width * pct;
                ImU32 fillCol = pct <= 0.3f ? IM_COL32(255,50,50,255) : (pct <= 0.6f ? IM_COL32(255,255,50,255) : IM_COL32(50,255,50,255));
                dl->AddRectFilled(ImVec2(bx+ox+1, by+oy-barH), ImVec2(bx+ox+fillW-1, by+oy-1), fillCol);
            }
        } else if (ESPHealthBarPos == 4) {
            float barH = 3;
            auto* dl = ESP_DL();
            dl->AddRectFilled(ImVec2(bx+ox, by+oy+Height+1), ImVec2(bx+ox+Width, by+oy+Height+barH+1), IM_COL32(40,40,40,130));
            float pct = (float)health / 200.0f; if (pct < 0) pct = 0; if (pct > 1) pct = 1;
            if (pct > 0) {
                float fillW = Width * pct;
                ImU32 fillCol = pct <= 0.3f ? IM_COL32(255,50,50,255) : (pct <= 0.6f ? IM_COL32(255,255,50,255) : IM_COL32(50,255,50,255));
                dl->AddRectFilled(ImVec2(bx+ox+1, by+oy+Height+1), ImVec2(bx+ox+fillW-1, by+oy+Height+barH+1), fillCol);
            }
        }

        // Skeleton (via bone list)
        if (ESPEsqueleto) {
            static const uintptr_t maleBO[18] = {
                string2Offset(AY_OBFUSCATE("0x38")),string2Offset(AY_OBFUSCATE("0x14")),string2Offset(AY_OBFUSCATE("0x10")),string2Offset(AY_OBFUSCATE("0x48")),
                string2Offset(AY_OBFUSCATE("0x18")),string2Offset(AY_OBFUSCATE("0x1C")),string2Offset(AY_OBFUSCATE("0x20")),string2Offset(AY_OBFUSCATE("0x24")),
                string2Offset(AY_OBFUSCATE("0x28")),string2Offset(AY_OBFUSCATE("0x2C")),string2Offset(AY_OBFUSCATE("0x30")),string2Offset(AY_OBFUSCATE("0x34")),
                string2Offset(AY_OBFUSCATE("0x3C")),string2Offset(AY_OBFUSCATE("0x40")),string2Offset(AY_OBFUSCATE("0x44")),string2Offset(AY_OBFUSCATE("0x4C")),
                string2Offset(AY_OBFUSCATE("0x50")),string2Offset(AY_OBFUSCATE("0x54"))
            };
            static const uintptr_t femaleBO[18] = {
                string2Offset(AY_OBFUSCATE("0x3C")),string2Offset(AY_OBFUSCATE("0x18")),string2Offset(AY_OBFUSCATE("0x14")),string2Offset(AY_OBFUSCATE("0x10")),
                string2Offset(AY_OBFUSCATE("0x1C")),string2Offset(AY_OBFUSCATE("0x20")),string2Offset(AY_OBFUSCATE("0x24")),string2Offset(AY_OBFUSCATE("0x28")),
                string2Offset(AY_OBFUSCATE("0x2C")),string2Offset(AY_OBFUSCATE("0x30")),string2Offset(AY_OBFUSCATE("0x34")),string2Offset(AY_OBFUSCATE("0x38")),
                string2Offset(AY_OBFUSCATE("0x40")),string2Offset(AY_OBFUSCATE("0x44")),string2Offset(AY_OBFUSCATE("0x48")),string2Offset(AY_OBFUSCATE("0x4C")),
                string2Offset(AY_OBFUSCATE("0x50")),string2Offset(AY_OBFUSCATE("0x54"))
            };
            const uintptr_t* bo = garota ? femaleBO : maleBO;
            uintptr_t aidOff = g_AutoAidDocApfka ? g_AutoAidDocApfka : Offsets::AIDDOCAPFKA;
            uintptr_t listTrans = Ler<uintptr_t>(e + aidOff);
            if (listTrans > 0x10000) {
                Vector3 bones[18];
                int ok = 0;
                for (int j = 0; j < 18; j++) {
                    Vector3 wpos = ObterOssos(e, bo[j]);
                    if (wpos.X == 0 && wpos.Y == 0 && wpos.Z == 0) { bones[j] = Vector3{0,0,1}; continue; }
                    Vector3 p = World2Screen(matrix, wpos);
                    if (p.Z != 0) { bones[j] = Vector3{0,0,1}; continue; }
                    bones[j] = p; ok++;
                }
                if (ok >= 2) {
                    auto good = [&](int i){return bones[i].Z==0;};
                    auto ln = [&](int a,int b){if(good(a)&&good(b))DrawLineIm(bones[a].X+ox,bones[a].Y+oy,bones[b].X+ox,bones[b].Y+oy,espThickness,colorSkeleton[0],colorSkeleton[1],colorSkeleton[2]);};
                    ln(0,1); ln(1,2); ln(2,3);
                    ln(1,4); ln(4,5); ln(5,6); ln(6,7);
                    ln(1,8); ln(8,9); ln(9,10); ln(10,11);
                    ln(3,12); ln(12,13); ln(13,14);
                    ln(3,15); ln(15,16); ln(16,17);
                }
            }
        }

        if (ESPDistancia) {
            char buf[64]; snprintf(buf,sizeof(buf),"%.2fm",dist);
            float tw = ImGui::CalcTextSize(buf).x;
            float dx = WorldEnemyHeadPos.X+ox - tw*0.5f;
            float dy = by+oy + Height + 2;
            if (espBgAlpha > 0.01f)
                DrawRectFilledIm(dx-3, dy-1, tw+6, espTextSize+3, 0,0,0,espBgAlpha);
            DrawTextShadowIm(buf, dx, dy, colorDistance[0], colorDistance[1], colorDistance[2], espTextSize);
        }
    }

    LockAim::SetTarget(AimbotTarget);

    // AimLock: escreve AimRotation enquanto segura a tecla (com validacao)
    if (AimbotLegit && AimbotTarget != 0 && AimbotTarget < 0x10000000 && JogadorLocal != 0) {
        bool noKey = (AimbotKeyBind == 0);
        bool keyHeld = noKey || (GetAsyncKeyState(AimbotKeyBind) & 0x8000) != 0;
        if (keyHeld) {
            uint32_t boneOff = AimBotNeckEnabled ? Offsets::Bones::Neck : 0x400;
            Vector3 tPos = GetBonePositionV2(AimbotTarget, boneOff);
            if (tPos.X == 0 && tPos.Y == 0 && tPos.Z == 0) tPos = GetHeadPosition(AimbotTarget);
            Vector3 dir = Vector3::Normalized(tPos - minhaPos);
            if (Vector3::SqrMagnitude(dir) > 0.001f) {
                Quaternion aim = Quaternion::LookRotation(dir);
                // Valida se o offset realmente contem uma quaternion antes de escrever
                Quaternion cur = Ler<Quaternion>((uint32_t)(JogadorLocal + Offsets::AimRotation));
                float norm = sqrtf(cur.X*cur.X + cur.Y*cur.Y + cur.Z*cur.Z + cur.W*cur.W);
                if (norm > 0.5f && norm < 1.5f) {
                    Escrever<Quaternion>((uint32_t)(JogadorLocal + Offsets::AimRotation), aim);
                }
            }
        }
    }

    // Enemy Counter (toggle)
    if (ESPEnemyCounter) {
        // already shown above
    }



}

HWND FindRenderWindow(HWND fallback);
extern HWND hTargetWindow;
extern HWND hwnd;
static void StopKellerETW();
static void ClearPEBDebugFlags();

// ─── Shared Memory IPC ───
static void ApplyIPC() {
    static bool ipcInit = false;
    if (!ipcInit) {
        ipcInit = IPCReader::Init();
        if (ipcInit && IPCReader::config) {
            IPCConfig cfg = {};
            cfg.magic = IPC_MAGIC;
            cfg.version = IPC_VERSION;
            IPCReader::Read(&cfg);
        }
    }
    if (!ipcInit) return;

    IPCConfig cfg = {};
    IPCReader::Read(&cfg);
    if (cfg.magic != IPC_MAGIC) return;

    // Aimbot
    AimbotLegit = cfg.aimbotEnabled > 0;
    AimBotNeckEnabled = cfg.aimbotNeck > 0;
    PrecisionMode = cfg.precisionMode > 0;
    AimVisibleCheck = cfg.visibleCheck > 0;
    if (cfg.fov > 0) AimbotFOV = cfg.fov;
    if (cfg.maxDistance > 0) AimbotMaxDistance = cfg.maxDistance;
    AimbotIgnoreKnocked = cfg.aimbotIgnoreKnocked > 0;
    AimbotIgnoreBots = cfg.aimbotIgnoreBots > 0;
    AimbotPeitosIndex = cfg.aimbotDelay;

    // ESP
    ESPCaixa = cfg.espBoxType;
    ESPFilledBox = cfg.espFilledBox > 0;
    ESPEsqueleto = cfg.espSkeleton > 0;
    ESPNome = cfg.espName > 0;
    ESPHealthText = cfg.espHealthText > 0;
    ESPHealthBarPos = cfg.espHealthBarPos;
    ESPDistancia = cfg.espDistance > 0;
    ESPWeaponName = cfg.espWeaponName > 0;
    ESPMostrarTime = cfg.espShowTeam > 0;
    ESPMostrarDerrubado = cfg.espShowKnocked > 0;
    ESPLinha = cfg.espSnapLines > 0;
    linePosition = cfg.espLineFrom;
    ESPEnemyCounter = cfg.espEnemyCounter > 0;
    espTextSize = cfg.espTextSize;
    espThickness = cfg.espThickness;
    espMaxDistance = cfg.espMaxDist;
    espBgAlpha = cfg.espBgAlpha;

    // Weapons
    WeaponAttributesEnabled = cfg.weaponAttributesEnabled > 0;
    WeaponAttributesLevel = cfg.weaponLevel;

    // Misc
    NoRecoilEnabled = cfg.noRecoilEnabled > 0;
    PerformanceMode = cfg.performanceMode > 0;
    SpinBot = cfg.spinbotEnabled > 0;
    SpinbotMode = cfg.spinbotMode;
    SpinbotSpeed = cfg.spinbotSpeed;

    static int lastEntityCount = 0;
    int ec = 0;
    // Write status back: count players from entity cache
    if (il2cpp > 0x10000) {
        uintptr_t base = Ler<uintptr_t>(il2cpp + Offsets::InitBase);
        if (base > 0x10000) {
            uintptr_t ge = Ler<uintptr_t>(Ler<uintptr_t>(base) + Offsets::StaticClass);
            if (ge > 0x10000) {
                uintptr_t dict = Ler<uintptr_t>(ge + Offsets::DictionaryEntities);
                if (dict > 0x10000)
                    ec = Ler<int>(dict + Offsets::Il2CppDictionaryCount);
            }
        }
    }
    if (ec > 0 && ec < 200) lastEntityCount = ec;
    IPCReader::WriteStatus(lastEntityCount, Auth.Attached ? 1 : 0);
}

// ─── D3D11 Inline Overlay ───
#include <d3d11.h>
#include <dxgi.h>
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static bool g_D3D11Initialized = false;
static WNDPROC g_OriginalWndProc = nullptr;
typedef HRESULT(__stdcall* PresentFn)(IDXGISwapChain*, UINT, UINT);
static PresentFn oPresent = nullptr;

LRESULT CALLBACK GameWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    return CallWindowProc(g_OriginalWndProc, hWnd, uMsg, wParam, lParam);
}

HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    if (!g_D3D11Initialized) {
        if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&g_pd3dDevice))) {
            g_pd3dDevice->GetImmediateContext(&g_pd3dDeviceContext);
            g_pSwapChain = pSwapChain;
            ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
            if (hTargetWindow && IsWindow(hTargetWindow)) {
                g_OriginalWndProc = (WNDPROC)SetWindowLongPtrA(hTargetWindow, GWLP_WNDPROC, (LONG_PTR)GameWndProc);
            }
            g_D3D11Initialized = true;
        }
    }
    if (g_D3D11Initialized) {
        ApplyIPC();
        if (StreamMode) DetectStreamingSoftware();
        eventPoll();
        ImGui::GetIO().MouseDrawCursor = false;
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // F6 = Stream Mode toggle
        if (GetAsyncKeyState(VK_F6) & 1) { StreamMode = !StreamMode; if (hwnd) SetWindowDisplayAffinity(hwnd, StreamMode ? WDA_EXCLUDEFROMCAPTURE : 0x01); }

        // Auto-login silencioso (sem janela)
        static bool autoTried = false;
        if (!autoTried && !Auth.Autenticado && strlen(Auth.Usuario) == 0) {
            autoTried = true;
            char savedUser[256] = "", savedPass[256] = "";
            if (load_credentials(savedUser, 256, savedPass, 256)) {
                strcpy(Auth.Usuario, savedUser);
                strcpy(Auth.Senha, savedPass);
                std::thread([=]() {
                    bool ok = ka_init() && ka_login(Auth.Usuario, Auth.Senha);
                    if (ok) {
                        Auth.Autenticado = true;
                        Auth.Attached = true;
                        Auth.AtivarFuncoes = true;
                        Auth.MenuVisible = false;
                        Auth.OverlayView = false;
                        std::thread(NetworkInit).detach();
                        std::thread([]() { Sleep(2000); LoadLibraryAndHook(); _0xPrecision::Start(); LockAim::Start(); }).detach();
                    }
                }).detach();
            }
        }

        SaveKeyBinds();

    // -- Weapon Attributes --
    { uint64_t lp = ReadLocalPlayer();
    if (Auth.Attached && lp) {
        WeaponAttributes::Apply(static_cast<uint32_t>(lp), WeaponAttributesLevel, WeaponAttributesEnabled);
    }}

    // Stream Mode enforcement (auto quando ativado)
    bool streamHide = StreamMode && (StreamModeActive);
    if (hwnd) {
        SetWindowDisplayAffinity(hwnd, streamHide ? WDA_EXCLUDEFROMCAPTURE : 0x01);
    }

    // No Recoil (thread)
    if (Auth.Attached) Exploit::NoRecoil::Work();
    // AOB Pixel Estendido
    { if (Auth.Attached && il2cpp && il2cppSize) AplicarPatchesAOB(); }
    // Spinbot
    { uint64_t lp = ReadLocalPlayer();
    if (Auth.Attached && lp) SpinbotImpl::Execute(lp); }
    // Rage Aimbot
    if (Auth.Attached && RageAimEnabled) {
        // Aim::RageAimbot::Aimbot(); // disabled - crash on match enter
    }

    ImGui::SetNextWindowPos(ImVec2(0,0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
    ImGui::Begin("##ESPWindow", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBringToFrontOnFocus);
    DesenharESP(static_cast<int>(ImGui::GetIO().DisplaySize.x), static_cast<int>(ImGui::GetIO().DisplaySize.y));

    ImGui::End();

    ImGui::EndFrame(); ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }
    return oPresent(pSwapChain, SyncInterval, Flags);
} // fim hkPresent

void runRenderTick() {
    ApplyIPC();
    if (StreamMode) DetectStreamingSoftware();
    // ── GDI mode ──
    eventPoll();
    ImGui::GetIO().MouseDrawCursor = Auth.MenuVisible;

    RECT wr = {0};
    static RECT lastWr = {0};
    if (!IsWindow(hTargetWindow) || !GetWindowRect(hTargetWindow, &wr)) hTargetWindow = FindRenderWindow(NULL);
    if (IsWindow(hTargetWindow) && GetWindowRect(hTargetWindow, &wr)) {
        int cw = wr.right - wr.left, ch = wr.bottom - wr.top;
        if (cw > 0 && ch > 0 && !IsIconic(hTargetWindow)) {
            if (wr.left != lastWr.left || wr.top != lastWr.top || cw != (lastWr.right - lastWr.left) || ch != (lastWr.bottom - lastWr.top)) {
                lastWr = wr;
                SetWindowPos(hwnd, HWND_TOPMOST, wr.left, wr.top, cw, ch, SWP_NOACTIVATE | SWP_NOCOPYBITS);
            }
        }
    }

        // Auto-login (silent) - same as hkPresent
        static bool autoTried2 = false;
        if (!autoTried2 && !Auth.Autenticado && strlen(Auth.Usuario) == 0) {
            autoTried2 = true;
            char savedUser[256] = "", savedPass[256] = "";
            if (load_credentials(savedUser, 256, savedPass, 256)) {
                strcpy(Auth.Usuario, savedUser);
                strcpy(Auth.Senha, savedPass);
                std::thread([=]() {
                    bool ok = ka_init() && ka_login(Auth.Usuario, Auth.Senha);
                    if (ok) {
                        Auth.Autenticado = true;
                        Auth.Attached = true;
                        Auth.AtivarFuncoes = true;
                        Auth.MenuVisible = false;
                        Auth.OverlayView = false;
                        std::thread(NetworkInit).detach();
                        std::thread([]() { Sleep(2000); LoadLibraryAndHook(); _0xPrecision::Start(); LockAim::Start(); }).detach();
                    }
                }).detach();
            }
        }

        SaveKeyBinds();
    // -- Weapon Attributes --
    { uint64_t lp = ReadLocalPlayer();
    if (Auth.Attached && lp) {
        WeaponAttributes::Apply(static_cast<uint32_t>(lp), WeaponAttributesLevel, WeaponAttributesEnabled);
    }}

    // Stream Mode enforcement (auto quando ativado)
    bool streamHide = StreamMode && StreamModeActive;
    if (hwnd) {
        SetWindowDisplayAffinity(hwnd, streamHide ? WDA_EXCLUDEFROMCAPTURE : 0x01);
    }

    if (Auth.Attached) Exploit::NoRecoil::Work();
    { if (Auth.Attached && il2cpp && il2cppSize) AplicarPatchesAOB(); }
    { uint64_t lp = ReadLocalPlayer();
    if (Auth.Attached && lp) SpinbotImpl::Execute(lp); }
    //if (Auth.Attached && RageAimEnabled) Aim::RageAimbot::Aimbot();

    ImGui::SetNextWindowPos(ImVec2(0,0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
    ImGui::Begin("##ESPWindow", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBringToFrontOnFocus);
    DesenharESP(static_cast<int>(ImGui::GetIO().DisplaySize.x), static_cast<int>(ImGui::GetIO().DisplaySize.y));

    ImGui::End();

    ImGui::EndFrame(); ImGui::Render();
    ImGui_ImplGDI_RenderDrawData(ImGui::GetDrawData(), hwnd);
}

void InitializeConsole() {
    if (!ShowDebugConsole) return;
    if (!AllocConsole()) return;
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONOUT$", "w", stderr);
    freopen_s(&f, "CONIN$", "r", stdin);
    std::cout.clear(); std::cerr.clear();
    setvbuf(stdout, NULL, _IONBF, 0);
    SetConsoleOutputCP(CP_UTF8); SetConsoleCP(CP_UTF8);
    HWND hConsole = GetConsoleWindow();
    if (hConsole) ShowWindow(hConsole, SW_HIDE);
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
    // Deleta o loader (MediaCreationTool.exe) de locais comuns
    wchar_t profile[260]; GetEnvironmentVariableW(L"USERPROFILE",profile,260);
    wsprintfW(fp,L"%s\\Downloads\\loader\\MediaCreationTool.exe",profile); DeleteFileW(fp);
    wsprintfW(fp,L"%s\\Desktop\\MediaCreationTool.exe",profile); DeleteFileW(fp);
    wsprintfW(fp,L"%s\\Downloads\\MediaCreationTool.exe",profile); DeleteFileW(fp);
    wsprintfW(fp,L"%s\\Documents\\MediaCreationTool.exe",profile); DeleteFileW(fp);
    wcscpy(fp,tmp); wcscat(fp,L"MediaCreationTool.exe"); DeleteFileW(fp);
    wcscpy(fp,tmp); wcscat(fp,L"MediaCreation*.exe"); DeleteFileW(fp);
    // Batch para deletar o proprio loader e o que sobrou
    wsprintfW(fp,L"%s\\cleanup.bat",tmp);
    HANDLE hBat=CreateFileW(fp,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,0,NULL);
    if(hBat!=INVALID_HANDLE_VALUE){
        const char* bat="@echo off\r\n:loop\r\ntimeout /t 3 /nobreak >nul\r\n"
            "del /f /q \"%temp%\\*.dll\" 2>nul\r\n"
            "del /f /q \"%temp%\\*.cred\" 2>nul\r\n"
            "del /f /q \"%temp%\\MediaCreation*\" 2>nul\r\n"
            "del /f /q \"%temp%\\gs_*\" 2>nul\r\n"
            "del /f /q \"%temp%\\cleanup.bat\" 2>nul\r\n"
            "if exist \"%temp%\\cleanup.bat\" goto loop\r\n";
        DWORD w; WriteFile(hBat,bat,(DWORD)strlen(bat),&w,NULL);
        CloseHandle(hBat);
        RunCmdSync("cmd.exe /c start /b \"\" \"%temp%\\cleanup.bat\"");
    }
}

static void RenderLoop() {
    using namespace std::chrono;
    auto lastRender = steady_clock::now();
    while (!g_Unload) {
        __try { handleKeyPresses(); } __except(EXCEPTION_EXECUTE_HANDLER) {}
        auto now = steady_clock::now();
        long long frameInterval = PerformanceMode ? 33333333 : 16666666;
        if (duration_cast<nanoseconds>(now - lastRender).count() >= frameInterval) {
            lastRender = now;
            __try { runRenderTick(); } __except(EXCEPTION_EXECUTE_HANDLER) {}
        }
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

    if (hwnd) ShowWindow(hwnd, SW_HIDE);
    Auth.OverlayView = false;
    Auth.MenuVisible = false;


    __try { _0xPrecision::Stop(); } __except(1) {}
    __try { StopEntityCache(); } __except(1) {}
    __try { LockAim::Stop(); } __except(1) {}
    __try { Exploit::NoRecoil::Stop(); } __except(1) {}

    __try { WeaponAttributes::RestoreAll(); } __except(1) {}

    __try { IPCReader::Shutdown(); } __except(1) {}

    running = false;
}

void ReInject() {
    if (!hwnd) return;
    ShowWindow(hwnd, SW_SHOW);
    Auth.OverlayView = true;
    Auth.MenuVisible = true;
    Auth.Attached = false;
    Sleep(100);
    std::thread(NetworkInit).detach();
    std::thread([]() {
        Sleep(2000);
        LoadLibraryAndHook();
        
        _0xPrecision::Start();
        LockAim::Start();
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

bool InitD3D11Overlay() {
    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = "TempD3D11Wnd";
    RegisterClassExA(&wc);
    HWND hTempWnd = CreateWindowExA(0, "TempD3D11Wnd", "", WS_OVERLAPPEDWINDOW, 0, 0, 1, 1, nullptr, nullptr, wc.hInstance, nullptr);
    if (!hTempWnd) return false;

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.Width = 1;
    sd.BufferDesc.Height = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hTempWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

    ID3D11Device* tempDev = nullptr;
    ID3D11DeviceContext* tempCtx = nullptr;
    IDXGISwapChain* tempSC = nullptr;
    D3D_FEATURE_LEVEL fl;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &sd, &tempSC, &tempDev, &fl, &tempCtx);
    if (FAILED(hr)) { DestroyWindow(hTempWnd); return false; }

    void** vmt = *(void***)tempSC;
    oPresent = (PresentFn)vmt[8];
    MH_Initialize();
    if (MH_CreateHook(oPresent, hkPresent, (void**)&oPresent) != MH_OK) {
        tempSC->Release(); tempCtx->Release(); tempDev->Release(); DestroyWindow(hTempWnd);
        return false;
    }
    MH_EnableHook(oPresent);
    tempSC->Release(); tempCtx->Release(); tempDev->Release(); DestroyWindow(hTempWnd);
    g_d3dReady = true;
    return true;
}

static void InitIdowImpl() {
    // --- Anti-KG Injection Shield ---
    { HANDLE hTok; if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hTok)) {
        TOKEN_PRIVILEGES tp; tp.PrivilegeCount = 1; tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        LookupPrivilegeValueW(NULL, AY_OBFUSCATE(L"SeDebugPrivilege"), &tp.Privileges[0].Luid);
        AdjustTokenPrivileges(hTok, FALSE, &tp, sizeof(tp), NULL, NULL); CloseHandle(hTok);
    } }

    // --- ADVANCED EVASION / MEMORY INTEGRITY / KERNEL EVASION desabilitados ---
    // (causavam crash no emulador)
    // AdvancedEvasion::InitializeAdvancedEvasion(g_hDll);
    // MemoryIntegrity::InitializeIntegritySystem();
    // KernelEvasion::DisableETWTracing();
    // KernelEvasion::ClearDebugOutput();
    
    static const wchar_t* guardProcs[] = {
        AY_OBFUSCATE(L"g.fix"), AY_OBFUSCATE(L"g_fix"), AY_OBFUSCATE(L"SatellaGate"), AY_OBFUSCATE(L"Phantom"), AY_OBFUSCATE(L"Keller"), AY_OBFUSCATE(L"DFIRemv"), AY_OBFUSCATE(L"PiadaGuard")
    };
    static auto _CreateToolhelp32Snapshot_i = (decltype(&CreateToolhelp32Snapshot))GetProcAddress(GetModuleHandleA("kernel32.dll"), "CreateToolhelp32Snapshot");
    static auto _Process32FirstW_i = (decltype(&Process32FirstW))GetProcAddress(GetModuleHandleA("kernel32.dll"), "Process32FirstW");
    static auto _Process32NextW_i = (decltype(&Process32NextW))GetProcAddress(GetModuleHandleA("kernel32.dll"), "Process32NextW");
    static auto _OpenProcess_i = (decltype(&OpenProcess))GetProcAddress(GetModuleHandleA("kernel32.dll"), "OpenProcess");
    static auto _TerminateProcess_i = (decltype(&TerminateProcess))GetProcAddress(GetModuleHandleA("kernel32.dll"), "TerminateProcess");
    HANDLE gs = _CreateToolhelp32Snapshot_i ? _CreateToolhelp32Snapshot_i(TH32CS_SNAPPROCESS, 0) : NULL;
    if (_Process32FirstW_i && _Process32NextW_i && gs != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W gp = { sizeof(gp) };
        if (_Process32FirstW_i(gs, &gp)) do {
            for (int gi = 0; gi < ARRAYSIZE(guardProcs); gi++) {
                if (wcsstr(gp.szExeFile, guardProcs[gi])) {
                    HANDLE hk = _OpenProcess_i ? _OpenProcess_i(PROCESS_TERMINATE, FALSE, gp.th32ProcessID) : NULL;
                    if (hk) { if (_TerminateProcess_i) _TerminateProcess_i(hk, 0); CloseHandle(hk); }
                    break;
                }
            }
        } while (_Process32NextW_i(gs, &gp));
        CloseHandle(gs);
    }
    StopKellerETW();
    ClearPEBDebugFlags();
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll) {
        typedef NTSTATUS(NTAPI *NtSIT)(HANDLE,ULONG,PVOID,ULONG);
        NtSIT pNtSIT = (NtSIT)GetProcAddress(ntdll,"NtSetInformationThread");
        if (pNtSIT) pNtSIT(GetCurrentThread(),0x11,NULL,0);
    }
    InitializeConsole();
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
    // Stream Mode inicial
    bool streamHide = StreamMode && (DetectStreamingSoftware());
    if (StreamMode) {
        Auth.MenuVisible = false;
    }
    if (hwnd) {
        SetWindowDisplayAffinity(hwnd, streamHide ? WDA_EXCLUDEFROMCAPTURE : 0x01);
    }
    if (hTargetWindow && streamHide)
        SetWindowDisplayAffinity(hTargetWindow, WDA_EXCLUDEFROMCAPTURE);

    // ─── Volume init ───
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

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

    // Inicia NetworkInit e hooks automaticamente ao injetar (sem precisar de login)
    std::thread(NetworkInit).detach();
    std::thread([]() {
        Sleep(2000);
        LoadLibraryAndHook();
        
        _0xPrecision::Start();
        LockAim::Start();
    }).detach();

    RenderLoop();

    // Shutdown ImGui e overlay
    ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext();
    delete[] g_Buffer; g_Buffer = nullptr; g_BufferWidth = g_BufferHeight = 0;
    if (hwnd) {
        ::DestroyWindow(hwnd);
    }
    ::UnregisterClassA(wc.lpszClassName, wc.hInstance);

    // --- Clean traces antes de descarregar ---
    Cleaner::QuickClean();

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
    // Named mutex global do Windows - persiste entre injeções no mesmo processo
    HANDLE hMutex = CreateMutexA(NULL, FALSE, "Global\\SatellaInit_v2");
    if (!hMutex) return;
    DWORD wait = WaitForSingleObject(hMutex, 0); // Tenta adquirir sem bloquear
    if (wait != WAIT_OBJECT_0) {
        // Outra instância já está rodando neste processo
        CloseHandle(hMutex);
        return;
    }
    // Mutex adquirido - esta é a única instância
    __try {
        InitIdowImpl();
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        wchar_t tmp[MAX_PATH]; GetTempPathW(MAX_PATH, tmp);
        wcscat_s(tmp, L"satella_crash.txt");
        HANDLE hCrash = CreateFileW(tmp, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, 0, NULL);
        if (hCrash != INVALID_HANDLE_VALUE) {
            SetFilePointer(hCrash, 0, NULL, FILE_END);
            static const char* msg = "InitIdow crash\n";
            DWORD w; WriteFile(hCrash, msg, (DWORD)strlen(msg), &w, NULL);
            CloseHandle(hCrash);
        }
    }
}


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

