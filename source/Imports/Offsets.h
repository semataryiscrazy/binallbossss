#pragma once
#include <cstdint>
#include "Cfg/strenc.h"
#include "Cfg/encrypt.hh"

class Offsets {
public:
    // ==========================================
    // 🔥 OFFSETS v7a - FF NORMAL 🔥
    // ==========================================
    
    // === BASE ===
    static inline uintptr_t Il2Cpp = 0x0;
    static inline uintptr_t UnityCpp = 0x0;
    static inline uintptr_t libunity = 0x0;
    static inline uintptr_t InitBase = string2Offset(AY_OBFUSCATE("0x9EC1C48"));
    static inline uintptr_t StaticClass = string2Offset(AY_OBFUSCATE("0x5C"));
    
    // === DICTIONARY ===
    static inline uintptr_t Il2CppDictionaryDataPtr = string2Offset(AY_OBFUSCATE("0xC"));
    static inline uintptr_t Il2CppDictionaryCount = string2Offset(AY_OBFUSCATE("0x10"));
    
    // === MATCH ===
    static inline uintptr_t CurrentMatch = string2Offset(AY_OBFUSCATE("0x50"));
    static inline uintptr_t MatchStatus = string2Offset(AY_OBFUSCATE("0x8C"));
    static inline uintptr_t LocalPlayer = string2Offset(AY_OBFUSCATE("0x94"));
    static inline uintptr_t DictionaryEntities = string2Offset(AY_OBFUSCATE("0x68"));
    static inline uintptr_t TimerPtr = string2Offset(AY_OBFUSCATE("0x10"));
    static inline uintptr_t Timer = string2Offset(AY_OBFUSCATE("0x24"));
    
    // === PLAYER ===
    static inline uintptr_t Player_IsDead = string2Offset(AY_OBFUSCATE("0x50"));
    static inline uintptr_t Player_Name = string2Offset(AY_OBFUSCATE("0x2E4"));
    static inline uintptr_t Player_Data = string2Offset(AY_OBFUSCATE("0x48"));
    static inline uintptr_t ShadowState = string2Offset(AY_OBFUSCATE("0x15E8"));
    static inline uintptr_t Player_ShadowBase = string2Offset(AY_OBFUSCATE("0x16BC"));
    static inline uintptr_t XPose = string2Offset(AY_OBFUSCATE("0x78"));
    static inline uintptr_t YPose = string2Offset(AY_OBFUSCATE("0x7C"));
    static inline uintptr_t ZPose = string2Offset(AY_OBFUSCATE("0x80"));
    static inline uintptr_t PlayerPosition = string2Offset(AY_OBFUSCATE("0x78"));
    
    // === AVATAR ===
    static inline uintptr_t AvatarManager = string2Offset(AY_OBFUSCATE("0x4C4"));
    static inline uintptr_t Avatar = string2Offset(AY_OBFUSCATE("0xA0"));
    static inline uintptr_t Avatar_IsVisible = string2Offset(AY_OBFUSCATE("0x95"));
    static inline uintptr_t Avatar_Data = string2Offset(AY_OBFUSCATE("0x14"));
    static inline uintptr_t Avatar_Data_IsTeam = string2Offset(AY_OBFUSCATE("0x59"));
    static inline uintptr_t Avatar_Data_IsBot = string2Offset(AY_OBFUSCATE("0x2EC"));
    
    static inline uintptr_t PlayerRotation = string2Offset(AY_OBFUSCATE("0xFC"));
    
    // === CAMERA ===
    static inline uintptr_t FollowCamera = string2Offset(AY_OBFUSCATE("0x454"));
    static inline uintptr_t Camera = string2Offset(AY_OBFUSCATE("0x18"));
    static inline uintptr_t AimRotation = string2Offset(AY_OBFUSCATE("0x404"));
    static inline uintptr_t AimRotationCheck = string2Offset(AY_OBFUSCATE("0x414"));
    static inline uintptr_t AuxAimRotation = string2Offset(AY_OBFUSCATE("0x414"));
    static inline uintptr_t MainCameraTransform = string2Offset(AY_OBFUSCATE("0x254"));
    static inline uintptr_t ViewMatrix = string2Offset(AY_OBFUSCATE("0xE8"));
    
    // === WEAPON ===
    static inline uint32_t Weapon = string2Offset(AY_OBFUSCATE("0x3F8"));
    static inline uint32_t WeaponFallback = string2Offset(AY_OBFUSCATE("0x35C"));
    // Dump: GPBDEDFKJNA+0x64 -> OOIPMACFIFL* (CSV weapon data)
    static inline uint32_t WeaponData = string2Offset(AY_OBFUSCATE("0x64"));
    static inline uint32_t WeaponRecoil = string2Offset(AY_OBFUSCATE("0xC"));
    static inline uintptr_t WeaponOnHand = string2Offset(AY_OBFUSCATE("0x4C"));
    static inline uintptr_t InventoryManager = string2Offset(AY_OBFUSCATE("0x4AC"));
    static inline uintptr_t m_itemOnHand = string2Offset(AY_OBFUSCATE("0x54"));
    static inline uintptr_t m_FireDuration = string2Offset(AY_OBFUSCATE("0x4B4"));
    // Dump: OOIPMACFIFL+0xE0 -> FKPFNILEOHE inline struct (weapon params)
    static inline uintptr_t WeaponParams = string2Offset(AY_OBFUSCATE("0xE0"));
    // FKPFNILEOHE+0x1C = float LGJHGKLFGJB (FireInterval)
    static inline uintptr_t WeaponParams_FireInterval = string2Offset(AY_OBFUSCATE("0x1C"));
    // Dump: FKPFNILEOHE+0x28 = float MEIHFCGIPMF (RepeatFireInterval)
    static inline uintptr_t WeaponParams_RepeatFireInterval = string2Offset(AY_OBFUSCATE("0x28"));
    // Dump: FKPFNILEOHE+0x2C = float JGGBOFBFKEI (MultiFireInterval)
    static inline uintptr_t WeaponParams_MultiFireInterval = string2Offset(AY_OBFUSCATE("0x2C"));
    // Dump: GPBDEDFKJNA+0x45C = float CGMBLIDAPNH (add fire speed)
    static inline uintptr_t Weapon_AddFireSpeed = string2Offset(AY_OBFUSCATE("0x45C"));
    // Dump: GPBDEDFKJNA+0x568 -> UGCWeaponRepItem*
    static inline uintptr_t UGCWeaponRepItem = string2Offset(AY_OBFUSCATE("0x568"));
    // Dump: UGCWeaponRepItem+0xB8 -> int Damage
    static inline uintptr_t UGCRep_Damage = string2Offset(AY_OBFUSCATE("0xB8"));
    // Dump: UGCWeaponRepItem+0xC0 -> float FireInterval
    static inline uintptr_t UGCRep_FireInterval = string2Offset(AY_OBFUSCATE("0xC0"));
    // Dump: UGCWeaponRepItem+0xE8 -> float RepeatFireInterval
    static inline uintptr_t UGCRep_RepeatFireInterval = string2Offset(AY_OBFUSCATE("0xE8"));
    
    // === PLAYER ATTRIBUTES ===
    static inline uintptr_t LocalPlayerAttributes = string2Offset(AY_OBFUSCATE("0x4C0"));
    // Dump: Player+0x4C0 -> PlayerAttributes (confirmed in dump line 1019368)
    static inline uintptr_t PlayerAttributes = string2Offset(AY_OBFUSCATE("0x4C0"));
    static inline uintptr_t PlayerAttributes_Ptr = string2Offset(AY_OBFUSCATE("0x4C0"));
    static inline uintptr_t LevelUp = string2Offset(AY_OBFUSCATE("0x14A8"));
    
    // === DAMAGE ATTRIBUTES ===
    static inline uintptr_t DamageAdditionScale = string2Offset(AY_OBFUSCATE("0x10"));
    static inline uintptr_t ExecuteDamageScale = string2Offset(AY_OBFUSCATE("0x14"));
    static inline uintptr_t PlayerAttributes_FireIntervalScale = string2Offset(AY_OBFUSCATE("0x184"));
    
    // === BOT ===
    static inline uintptr_t IsClientBot = string2Offset(AY_OBFUSCATE("0x2EC"));
    
    // === FIRING ===
    static inline uint32_t IsFiring = string2Offset(AY_OBFUSCATE("0x544"));
    static inline uint32_t LocalPlayerIsFiring = string2Offset(AY_OBFUSCATE("0x544"));
    
    // === AIMBOT COLLIDERS ===
    static inline uintptr_t HeadCollider = string2Offset(AY_OBFUSCATE("0x4A8"));
    static inline uintptr_t HeadCollider_Alt = string2Offset(AY_OBFUSCATE("0x4A8"));
    static inline uintptr_t Player_TargetCollider = string2Offset(AY_OBFUSCATE("0x54"));
    static inline uintptr_t ReplaceCollider = string2Offset(AY_OBFUSCATE("0x54"));
    static inline uintptr_t ColliderHECFNHJKOMN = string2Offset(AY_OBFUSCATE("0x4A8"));
    static inline uintptr_t ColliderINICDNFOFJB = string2Offset(AY_OBFUSCATE("0x54"));
    
    // === RELOAD ===
    static inline uint32_t NoReload = string2Offset(AY_OBFUSCATE("0x99"));
    static inline uint32_t NoReload2 = string2Offset(AY_OBFUSCATE("0x91"));
    
    // === WEAPON DATA OFFSETS ===
    static inline uintptr_t WD_Damage = string2Offset(AY_OBFUSCATE("0x0"));
    static inline uintptr_t WD_DamageIncrease = string2Offset(AY_OBFUSCATE("0x4"));
    static inline uintptr_t WD_AmmoClipSize = string2Offset(AY_OBFUSCATE("0x14"));
    static inline uintptr_t WD_FireInterval = string2Offset(AY_OBFUSCATE("0x1C"));
    static inline uintptr_t WD_RepeatFireInterval = string2Offset(AY_OBFUSCATE("0x24"));
    static inline uintptr_t WD_MultiFireInterval = string2Offset(AY_OBFUSCATE("0x28"));
    static inline uintptr_t WD_AttachmentFireInterval = string2Offset(AY_OBFUSCATE("0x30"));
    static inline uintptr_t WD_BiteArmor = string2Offset(AY_OBFUSCATE("0x38"));
    static inline uintptr_t WD_Range = string2Offset(AY_OBFUSCATE("0x40"));
    static inline uintptr_t WD_ReloadSpeed = string2Offset(AY_OBFUSCATE("0x84"));
    static inline uintptr_t WD_IsSingleShot = string2Offset(AY_OBFUSCATE("0xA8"));
    static inline uintptr_t WD_DamageHead = string2Offset(AY_OBFUSCATE("0xAC"));
    static inline uintptr_t WD_DamageLimb = string2Offset(AY_OBFUSCATE("0xB0"));
    static inline uintptr_t WD_PrefireDelay = string2Offset(AY_OBFUSCATE("0x140"));

    // === SILENT AIM ===
    static inline uint32_t Sillent = string2Offset(AY_OBFUSCATE("0x948"));
    static inline uintptr_t AimInfo_RayDir = string2Offset(AY_OBFUSCATE("0x2C"));
    static inline uintptr_t AimInfo_StartPos = string2Offset(AY_OBFUSCATE("0x38"));
    static inline uintptr_t AimInfo_HitPos = string2Offset(AY_OBFUSCATE("0x14"));
    static inline uintptr_t AimInfo_Distance = string2Offset(AY_OBFUSCATE("0x48"));
    static inline uintptr_t Scatter_Weapon = string2Offset(AY_OBFUSCATE("0x6C"));
    
    // === EXTRAS ===
    static inline uintptr_t Vida = string2Offset(AY_OBFUSCATE("0x10"));
    static inline uintptr_t pomba = string2Offset(AY_OBFUSCATE("0x488"));
    static inline uintptr_t bisteca = string2Offset(AY_OBFUSCATE("0x854"));
    static inline uintptr_t arma = string2Offset(AY_OBFUSCATE("0x38"));
    static inline uintptr_t tiro = string2Offset(AY_OBFUSCATE("0x2c"));
    static inline uintptr_t FireInterval = string2Offset(AY_OBFUSCATE("0x188"));
    static inline uintptr_t Weapon_Direction = string2Offset(AY_OBFUSCATE("0x2c"));
    static inline uintptr_t FastSwitch = string2Offset(AY_OBFUSCATE("0x4BC"));
    static inline uintptr_t telepneu = string2Offset(AY_OBFUSCATE("0x0"));
    static inline uintptr_t GhostMode = string2Offset(AY_OBFUSCATE("0x524"));
    static inline uintptr_t Ghost = string2Offset(AY_OBFUSCATE("0x524"));
    static inline uintptr_t MainTransform = string2Offset(AY_OBFUSCATE("0x38"));
    static inline uintptr_t CDOBMFNCJHD = string2Offset(AY_OBFUSCATE("0x7C1"));
    static inline uintptr_t AIDDOCAPFKA = string2Offset(AY_OBFUSCATE("0x754"));
    static inline uintptr_t ListEntities = string2Offset(AY_OBFUSCATE("0x13C"));
    static inline uintptr_t UmaAvatarSimple = string2Offset(AY_OBFUSCATE("0xA0"));
    static inline uintptr_t UMAData = string2Offset(AY_OBFUSCATE("0x14"));
    static inline uintptr_t TeamMate = string2Offset(AY_OBFUSCATE("0x59"));
    static inline uintptr_t PRIDataPool = string2Offset(AY_OBFUSCATE("0x48"));
    static inline uintptr_t ReplicationDataPoolUnsafe = string2Offset(AY_OBFUSCATE("0x8"));
    static inline uintptr_t ReplicationDataUnsafe = string2Offset(AY_OBFUSCATE("0x10"));
    static inline uintptr_t Health = string2Offset(AY_OBFUSCATE("0x10"));
    static inline uintptr_t LPEIEILIKGC = string2Offset(AY_OBFUSCATE("0x478"));
    static inline uintptr_t GameTimer = string2Offset(AY_OBFUSCATE("0x10"));
    static inline uintptr_t GameVariables = string2Offset(AY_OBFUSCATE("0xB0"));
    
    static inline uintptr_t JoystickAnalogX = 0;
    static inline uintptr_t JoystickAnalogY = 0;
    
    static inline uintptr_t Position = string2Offset(AY_OBFUSCATE("0x7"));
    static inline uintptr_t Rotation = string2Offset(AY_OBFUSCATE("0x39"));
    
    static inline uintptr_t SpinBotMatrixRotation = string2Offset(AY_OBFUSCATE("0x10"));
    
    static inline uintptr_t SpinBotChainQuaternion = string2Offset(AY_OBFUSCATE("0x70"));
    static inline uintptr_t ParachuteDragA = string2Offset(AY_OBFUSCATE("0x1A8"));
    static inline uintptr_t ParachuteDragB = string2Offset(AY_OBFUSCATE("0x1F4"));
    static inline uintptr_t CurrentObserver = string2Offset(AY_OBFUSCATE("0x64"));
    static inline uintptr_t ObserverPlayer = string2Offset(AY_OBFUSCATE("0x28"));
    static inline uintptr_t RightcameraOffset = string2Offset(AY_OBFUSCATE("0x38"));
    static inline uintptr_t FOVcameraoffset = string2Offset(AY_OBFUSCATE("0x44"));
    static inline uintptr_t Backcameraoffset = string2Offset(AY_OBFUSCATE("0x40"));
    static inline uintptr_t Upcameraoffset = string2Offset(AY_OBFUSCATE("0x3c"));
    static inline uintptr_t Phase1CameraEulerAnglesY = string2Offset(AY_OBFUSCATE("0x48"));
    
    // ==========================================
    // 🦴 BONES v7a
    // ==========================================
    class Bones {
    public:
        static inline uint32_t Head = string2Offset(AY_OBFUSCATE("0x45C"));
        static inline uint32_t Neck = string2Offset(AY_OBFUSCATE("0x464"));
        static inline uint32_t Hip = string2Offset(AY_OBFUSCATE("0x460"));
        static inline uint32_t Pelvis = string2Offset(AY_OBFUSCATE("0x468"));
        static inline uint32_t Hip2 = string2Offset(AY_OBFUSCATE("0x46C"));
        static inline uint32_t Root = string2Offset(AY_OBFUSCATE("0x470"));
        static inline uint32_t RootBone = string2Offset(AY_OBFUSCATE("0x474"));
        static inline uint32_t LeftShoulder = string2Offset(AY_OBFUSCATE("0x490"));
        static inline uint32_t RightShoulder = string2Offset(AY_OBFUSCATE("0x494"));
        static inline uint32_t LeftElbow = string2Offset(AY_OBFUSCATE("0x4A4"));
        static inline uint32_t RightElbow = string2Offset(AY_OBFUSCATE("0x4A0"));
        static inline uint32_t LeftHand = string2Offset(AY_OBFUSCATE("0x49C"));
        static inline uint32_t RightHand = string2Offset(AY_OBFUSCATE("0x458"));
        static inline uint32_t LeftWrist = string2Offset(AY_OBFUSCATE("0x498"));
        static inline uint32_t RightWrist = string2Offset(AY_OBFUSCATE("0x498"));
        static inline uint32_t LeftCalf = string2Offset(AY_OBFUSCATE("0x478"));
        static inline uint32_t LeftFoot = string2Offset(AY_OBFUSCATE("0x478"));
        static inline uint32_t RightCalf = string2Offset(AY_OBFUSCATE("0x47C"));
        static inline uint32_t RightFoot = string2Offset(AY_OBFUSCATE("0x47C"));
        static inline uint32_t LeftAnkle = string2Offset(AY_OBFUSCATE("0x478"));
        static inline uint32_t RightAnkle = string2Offset(AY_OBFUSCATE("0x47C"));
        static inline uint32_t LeftKnee = string2Offset(AY_OBFUSCATE("0x484"));
        static inline uint32_t RightKnee = string2Offset(AY_OBFUSCATE("0x488"));
        static inline uint32_t Spine = string2Offset(AY_OBFUSCATE("0x48C"));
        static inline uint32_t Chest = string2Offset(AY_OBFUSCATE("0x464"));
        
        static inline uint32_t Node_WorldPos = string2Offset(AY_OBFUSCATE("0x28"));
        static inline uint32_t Node_Position = string2Offset(AY_OBFUSCATE("0x28"));
    };
};
