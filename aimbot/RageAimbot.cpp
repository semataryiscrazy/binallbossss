#include "../source/Imports/Includes.h"
#include "../source/Imports/EntityCache.h"
#include "../source/Imports/Scope.h"
#include "RageAimbot.hpp"

static EntityData* FindTarget() {
    float cx = SWidth * 0.5f, cy = SHeight * 0.5f;
    if (cx <= 0 || cy <= 0) return nullptr;
    float best = (float)(AimbotFOV * AimbotFOV);
    EntityData* t = nullptr;
    UnityMatrix mat = renderMatrix;

    for (auto& [addr, e] : GetEntityCache()) {
        if (!e.valid || e.dying) continue;
        Vector3 hp;
        switch (AimbotHitbox) {
        case 1: hp = e.boneWorld[1]; break;
        case 2: hp = e.boneWorld[2]; break;
        case 3: hp = e.bodyPos; break;
        default: hp = e.headPos; break;
        }
        Vector3 s = World2Screen(mat, hp);
        if (s.Z != 0.f) continue;
        float d = (s.X - cx) * (s.X - cx) + (s.Y - cy) * (s.Y - cy);
        if (d >= best) continue;
        best = d;
        t = const_cast<EntityData*>(&e);
    }
    return t;
}

void Aim::RageAimbot::Aimbot() {
    if (!RageAimEnabled || !Auth.Attached || !Auth.AtivarFuncoes) return;
    if (renderMatrixTimestamp.load() == 0) return;
    if (SWidth <= 0 || SHeight <= 0) return;

    bool active = !RageAimRequireKey || (GetAsyncKeyState(RageAimKey) & 0x8000);

    std::lock_guard<std::mutex> lk(GetCacheWriteMutex());
    EntityData* tgt = FindTarget();
    if (!tgt || !tgt->address) return;

    uint32_t col = Ler<uint32_t>((uint32_t)(tgt->address + Offsets::ColliderHECFNHJKOMN));
    if (!col) return;

    if (active) {
        Escrever<uint32_t>((uint32_t)(tgt->address + Offsets::ColliderINICDNFOFJB), 0u);
        Escrever<uint32_t>((uint32_t)(tgt->address + Offsets::ColliderINICDNFOFJB), col);
    } else {
        uint32_t cur = Ler<uint32_t>((uint32_t)(tgt->address + Offsets::ColliderINICDNFOFJB));
        if (cur != 0)
            Escrever<uint32_t>((uint32_t)(tgt->address + Offsets::ColliderINICDNFOFJB), 0u);
    }
}
