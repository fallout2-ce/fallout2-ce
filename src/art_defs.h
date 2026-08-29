#ifndef ART_DEFS_H
#define ART_DEFS_H

namespace fallout {

inline int frameIdFromFid(int fid)
{
    return fid & 0xFFF;
}

inline int frameIdFromPid(int pid)
{
    return pid & 0xFFFFFF;
}

enum HeadFrameId : int {
    HEAD_INVALID = -1,
    HEAD_NONE,
    HEAD_MARCUS,
    HEAD_MYRON,
    HEAD_ELDER,
    HEAD_LYNETTE,
    HEAD_HAROLD,
    HEAD_TANDI,
    HEAD_COM_OFFICER,
    HEAD_SULIK,
    HEAD_PRESIDENT,
    HEAD_HAKUNIN,
    HEAD_BOSS,
    HEAD_DYING_HAKUNIN,
};

inline bool headFrameIdIsValid(int head)
{
    return head >= HEAD_NONE;
}

inline HeadFrameId headFrameIdFromFid(int fid)
{
    return static_cast<HeadFrameId>(frameIdFromFid(fid));
}

enum HeadAnimation : int {
    HEAD_ANIMATION_VERY_GOOD_REACTION = 0,
    HEAD_ANIMATION_GOOD = 1,
    HEAD_ANIMATION_GOOD_TO_NEUTRAL = 2,
    HEAD_ANIMATION_NEUTRAL_TO_GOOD = 3,
    HEAD_ANIMATION_NEUTRAL = 4,
    HEAD_ANIMATION_NEUTRAL_TO_BAD = 5,
    HEAD_ANIMATION_BAD_TO_NEUTRAL = 6,
    HEAD_ANIMATION_BAD = 7,
    HEAD_ANIMATION_VERY_BAD_REACTION = 8,
    HEAD_ANIMATION_GOOD_PHONEMES = 9,
    HEAD_ANIMATION_NEUTRAL_PHONEMES = 10,
    HEAD_ANIMATION_BAD_PHONEMES = 11,
};

enum HeadFidget : int {
    FIDGET_INVALID = -1,
    FIDGET_GOOD = 1,
    FIDGET_NEUTRAL = 4,
    FIDGET_BAD = 7,
};

inline HeadFidget headFidgetFromFid(int fid)
{
    int fidget = (fid & 0xFF0000) >> 16;
    return static_cast<HeadFidget>(fidget);
}

inline HeadAnimation headAnimationFromHeadFidget(HeadFidget fidget)
{
    return fidget != FIDGET_INVALID ? static_cast<HeadAnimation>(fidget) : HEAD_ANIMATION_VERY_GOOD_REACTION;
}

enum BackgroundFrameId : int {
    BACKGROUND_INVALID = -1,
    BACKGROUND_0,
    BACKGROUND_1,
    BACKGROUND_2,
    BACKGROUND_HUB,
    BACKGROUND_NECROPOLIS,
    BACKGROUND_BROTHERHOOD,
    BACKGROUND_MILITARY_BASE,
    BACKGROUND_JUNK_TOWN,
    BACKGROUND_CATHEDRAL,
    BACKGROUND_SHADY_SANDS,
    BACKGROUND_VAULT,
    BACKGROUND_MASTER,
    BACKGROUND_FOLLOWER,
    BACKGROUND_RAIDERS,
    BACKGROUND_CAVE,
    BACKGROUND_ENCLAVE,
    BACKGROUND_WASTELAND,
    BACKGROUND_BOSS,
    BACKGROUND_PRESIDENT,
    BACKGROUND_TENT,
    BACKGROUND_ADOBE,
};

inline bool backgroundFrameIdIsValid(int background)
{
    return background >= BACKGROUND_0;
}

enum DudeNativeLook : int {
    // Hero looks as one the tribals (before finishing Temple of Trails).
    DUDE_NATIVE_LOOK_TRIBAL,

    // Hero have finished Temple of Trails and received Vault Jumpsuit.
    DUDE_NATIVE_LOOK_JUMPSUIT,
    DUDE_NATIVE_LOOK_COUNT,
};

enum WeaponAnimation : int {
    WEAPON_ANIMATION_INVALID = -1,
    WEAPON_ANIMATION_NONE,
    WEAPON_ANIMATION_KNIFE, // d
    WEAPON_ANIMATION_CLUB, // e
    WEAPON_ANIMATION_HAMMER, // f
    WEAPON_ANIMATION_SPEAR, // g
    WEAPON_ANIMATION_PISTOL, // h
    WEAPON_ANIMATION_SMG, // i
    WEAPON_ANIMATION_SHOTGUN, // j
    WEAPON_ANIMATION_LASER_RIFLE, // k
    WEAPON_ANIMATION_MINIGUN, // l
    WEAPON_ANIMATION_LAUNCHER, // m
    WEAPON_ANIMATION_SFALL_S, // s
    WEAPON_ANIMATION_SFALL_O, // o
    WEAPON_ANIMATION_SFALL_P, // p
    WEAPON_ANIMATION_SFALL_Q, // q
    WEAPON_ANIMATION_SFALL_T, // t
    WEAPON_ANIMATION_COUNT,

    // There's mixed usage of WeaponAnimation and CharacterSoundEffect in the code, lets merge those as we any cannot distinguish between them.
    CHARACTER_SOUND_EFFECT_UNUSED = WEAPON_ANIMATION_NONE,
    CHARACTER_SOUND_EFFECT_KNOCKDOWN = WEAPON_ANIMATION_KNIFE,
    CHARACTER_SOUND_EFFECT_PASS_OUT = WEAPON_ANIMATION_CLUB,
    CHARACTER_SOUND_EFFECT_DIE = WEAPON_ANIMATION_HAMMER,
    CHARACTER_SOUND_EFFECT_CONTACT = WEAPON_ANIMATION_SPEAR,
};

inline bool weaponAnimationIsValid(int weaponAnimation)
{
    return weaponAnimation >= WEAPON_ANIMATION_NONE && weaponAnimation < WEAPON_ANIMATION_COUNT;
}

inline WeaponAnimation weaponAnimationFromFid(int fid)
{
    int anim = (fid & 0xF000) >> 12;
    return static_cast<WeaponAnimation>(anim);
}

enum SkillDexFrameId : int {
    SKILLDEX_FRM_ID_FIRST = 0,
    SKILLDEX_FRM_ID_0 = 0,
    SKILLDEX_FRM_ID_1 = 1,
    SKILLDEX_FRM_ID_2 = 2,
    SKILLDEX_FRM_ID_3 = 3,
    SKILLDEX_FRM_ID_4 = 4,
    SKILLDEX_FRM_ID_5 = 5,
    SKILLDEX_FRM_ID_6 = 6,
    SKILLDEX_FRM_ID_7 = 7,
    SKILLDEX_FRM_ID_8 = 8,
    SKILLDEX_FRM_ID_9 = 9,
    SKILLDEX_FRM_ID_10 = 10,
    SKILLDEX_FRM_ID_11 = 11,
    SKILLDEX_FRM_ID_12 = 12,
    SKILLDEX_FRM_ID_13 = 13,
    SKILLDEX_FRM_ID_14 = 14,
    SKILLDEX_FRM_ID_15 = 15,
    SKILLDEX_FRM_ID_16 = 16,
    SKILLDEX_FRM_ID_17 = 17,
    SKILLDEX_FRM_ID_18 = 18,
    SKILLDEX_FRM_ID_19 = 19,
    SKILLDEX_FRM_ID_20 = 20,
    SKILLDEX_FRM_ID_21 = 21,
    SKILLDEX_FRM_ID_22 = 22,
    SKILLDEX_FRM_ID_23 = 23,
    SKILLDEX_FRM_ID_24 = 24,
    SKILLDEX_FRM_ID_25 = 25,
    SKILLDEX_FRM_ID_26 = 26,
    SKILLDEX_FRM_ID_27 = 27,
    SKILLDEX_FRM_ID_28 = 28,
    SKILLDEX_FRM_ID_29 = 29,
    SKILLDEX_FRM_ID_30 = 30,
    SKILLDEX_FRM_ID_31 = 31,
    SKILLDEX_FRM_ID_32 = 32,
    SKILLDEX_FRM_ID_33 = 33,
    SKILLDEX_FRM_ID_34 = 34,
    SKILLDEX_FRM_ID_35 = 35,
    SKILLDEX_FRM_ID_36 = 36,
    SKILLDEX_FRM_ID_37 = 37,
    SKILLDEX_FRM_ID_38 = 38,
    SKILLDEX_FRM_ID_39 = 39,
    SKILLDEX_FRM_ID_40 = 40,
    SKILLDEX_FRM_ID_41 = 41,
    SKILLDEX_FRM_ID_42 = 42,
    SKILLDEX_FRM_ID_43 = 43,
    SKILLDEX_FRM_ID_44 = 44,
    SKILLDEX_FRM_ID_45 = 45,
    SKILLDEX_FRM_ID_46 = 46,
    SKILLDEX_FRM_ID_47 = 47,
    SKILLDEX_FRM_ID_48 = 48,
    SKILLDEX_FRM_ID_52 = 52,
    SKILLDEX_FRM_ID_53 = 53,
    SKILLDEX_FRM_ID_54 = 54,
    SKILLDEX_FRM_ID_55 = 55,
    SKILLDEX_FRM_ID_56 = 56,
    SKILLDEX_FRM_ID_57 = 57,
    SKILLDEX_FRM_ID_58 = 58,
    SKILLDEX_FRM_ID_59 = 59,
    SKILLDEX_FRM_ID_60 = 60,
    SKILLDEX_FRM_ID_61 = 61,
    SKILLDEX_FRM_ID_62 = 62,
    SKILLDEX_FRM_ID_63 = 63,
    SKILLDEX_FRM_ID_64 = 64,
    SKILLDEX_FRM_ID_65 = 65,
    SKILLDEX_FRM_ID_66 = 66,
    SKILLDEX_FRM_ID_67 = 67,
    SKILLDEX_FRM_ID_69 = 69,
    SKILLDEX_FRM_ID_70 = 70,
    SKILLDEX_FRM_ID_71 = 71,
    SKILLDEX_FRM_ID_72 = 72,
    SKILLDEX_FRM_ID_73 = 73,
    SKILLDEX_FRM_ID_74 = 74,
    SKILLDEX_FRM_ID_75 = 75,
    SKILLDEX_FRM_ID_76 = 76,
    SKILLDEX_FRM_ID_77 = 77,
    SKILLDEX_FRM_ID_78 = 78,
    SKILLDEX_FRM_ID_79 = 79,
    SKILLDEX_FRM_ID_80 = 80,
    SKILLDEX_FRM_ID_81 = 81,
    SKILLDEX_FRM_ID_82 = 82,
    SKILLDEX_FRM_ID_83 = 83,
    SKILLDEX_FRM_ID_84 = 84,
    SKILLDEX_FRM_ID_85 = 85,
    SKILLDEX_FRM_ID_86 = 86,
    SKILLDEX_FRM_ID_87 = 87,
    SKILLDEX_FRM_ID_88 = 88,
    SKILLDEX_FRM_ID_89 = 89,
    SKILLDEX_FRM_ID_90 = 90,
    SKILLDEX_FRM_ID_91 = 91,
    SKILLDEX_FRM_ID_92 = 92,
    SKILLDEX_FRM_ID_93 = 93,
    SKILLDEX_FRM_ID_94 = 94,
    SKILLDEX_FRM_ID_95 = 95,
    SKILLDEX_FRM_ID_96 = 96,
    SKILLDEX_FRM_ID_97 = 97,
    SKILLDEX_FRM_ID_98 = 98,
    SKILLDEX_FRM_ID_99 = 99,
    SKILLDEX_FRM_ID_100 = 100,
    SKILLDEX_FRM_ID_101 = 101,
    SKILLDEX_FRM_ID_102 = 102,
    SKILLDEX_FRM_ID_103 = 103,
    SKILLDEX_FRM_ID_104 = 104,
    SKILLDEX_FRM_ID_105 = 105,
    SKILLDEX_FRM_ID_106 = 106,
    SKILLDEX_FRM_ID_107 = 107,
    SKILLDEX_FRM_ID_108 = 108,
    SKILLDEX_FRM_ID_109 = 109,
    SKILLDEX_FRM_ID_110 = 110,
    SKILLDEX_FRM_ID_111 = 111,
    SKILLDEX_FRM_ID_112 = 112,
    SKILLDEX_FRM_ID_113 = 113,
    SKILLDEX_FRM_ID_114 = 114,
    SKILLDEX_FRM_ID_115 = 115,
    SKILLDEX_FRM_ID_116 = 116,
    SKILLDEX_FRM_ID_117 = 117,
    SKILLDEX_FRM_ID_118 = 118,
    SKILLDEX_FRM_ID_119 = 119,
    SKILLDEX_FRM_ID_120 = 120,
    SKILLDEX_FRM_ID_121 = 121,
    SKILLDEX_FRM_ID_122 = 122,
    SKILLDEX_FRM_ID_123 = 123,
    SKILLDEX_FRM_ID_124 = 124,
    SKILLDEX_FRM_ID_125 = 125,
    SKILLDEX_FRM_ID_126 = 126,
    SKILLDEX_FRM_ID_127 = 127,
    SKILLDEX_FRM_ID_128 = 128,
    SKILLDEX_FRM_ID_129 = 129,
    SKILLDEX_FRM_ID_130 = 130,
    SKILLDEX_FRM_ID_131 = 131,
    SKILLDEX_FRM_ID_132 = 132,
    SKILLDEX_FRM_ID_133 = 133,
    SKILLDEX_FRM_ID_134 = 134,
    SKILLDEX_FRM_ID_135 = 135,
    SKILLDEX_FRM_ID_136 = 136,
    SKILLDEX_FRM_ID_137 = 137,
    SKILLDEX_FRM_ID_138 = 138,
    SKILLDEX_FRM_ID_139 = 139,
    SKILLDEX_FRM_ID_140 = 140,
    SKILLDEX_FRM_ID_141 = 141,
    SKILLDEX_FRM_ID_142 = 142,
    SKILLDEX_FRM_ID_144 = 144,
    SKILLDEX_FRM_ID_145 = 145,
    SKILLDEX_FRM_ID_149 = 149,
    SKILLDEX_FRM_ID_150 = 150,
    SKILLDEX_FRM_ID_153 = 153,
    SKILLDEX_FRM_ID_154 = 154,
    SKILLDEX_FRM_ID_155 = 155,
    SKILLDEX_FRM_ID_156 = 156,
    SKILLDEX_FRM_ID_157 = 157,
    SKILLDEX_FRM_ID_158 = 158,
    SKILLDEX_FRM_ID_159 = 159,
    SKILLDEX_FRM_ID_160 = 160,
    SKILLDEX_FRM_ID_161 = 161,
    SKILLDEX_FRM_ID_162 = 162,
    SKILLDEX_FRM_ID_163 = 163,
    SKILLDEX_FRM_ID_164 = 164,
    SKILLDEX_FRM_ID_165 = 165,
    SKILLDEX_FRM_ID_166 = 166,
    SKILLDEX_FRM_ID_167 = 167,
    SKILLDEX_FRM_ID_168 = 168,
    SKILLDEX_FRM_ID_169 = 169,
    SKILLDEX_FRM_ID_170 = 170,
    SKILLDEX_FRM_ID_171 = 171,
    SKILLDEX_FRM_ID_172 = 172,
    SKILLDEX_FRM_ID_173 = 173,
};

enum CritterFrameId : int {
    CRITTER_FRM_ID_INVALID = -1,
    CRITTER_FRM_ID_FIRST = 0,
    CRITTER_FRM_ID_1 = 1,
};

inline constexpr CritterFrameId operator+(CritterFrameId lhs, int rhs)
{
    return static_cast<CritterFrameId>(static_cast<int>(lhs) + rhs);
}

inline constexpr CritterFrameId operator-(CritterFrameId lhs, int rhs)
{
    return static_cast<CritterFrameId>(static_cast<int>(lhs) - rhs);
}

inline CritterFrameId operator++(CritterFrameId& e, int)
{
    CritterFrameId result = e;
    e = e + 1;
    return result;
}

inline CritterFrameId critterFrameIdFromFid(int fid)
{
    return static_cast<CritterFrameId>(frameIdFromFid(fid));
}

inline CritterFrameId critterFrameIdFromPid(int pid)
{
    return static_cast<CritterFrameId>(frameIdFromPid(pid));
}

enum SceneryFrameId : int {
    SCENERY_FRM_ID_FIRST = 0,
};

inline constexpr SceneryFrameId operator-(SceneryFrameId lhs, int rhs)
{
    return static_cast<SceneryFrameId>(static_cast<int>(lhs) - rhs);
}

inline SceneryFrameId sceneryFrameIdFromPid(int pid)
{
    return static_cast<SceneryFrameId>(frameIdFromPid(pid));
}

enum WallFrameId : int {
    WALL_FRM_ID_FIRST = 0,
};

inline constexpr WallFrameId operator-(WallFrameId lhs, int rhs)
{
    return static_cast<WallFrameId>(static_cast<int>(lhs) - rhs);
}

inline WallFrameId wallFrameIdFromPid(int pid)
{
    return static_cast<WallFrameId>(frameIdFromPid(pid));
}

enum ItemFrameId : int {
    ITEM_FRM_ID_FIRST = 0,
};

inline constexpr ItemFrameId operator-(ItemFrameId lhs, int rhs)
{
    return static_cast<ItemFrameId>(static_cast<int>(lhs) - rhs);
}

inline ItemFrameId itemFrameIdFromPid(int pid)
{
    return static_cast<ItemFrameId>(frameIdFromPid(pid));
}

enum TileFrameId : int {
    TILE_FRM_ID_FIRST = 0,
    TILE_FRM_ID_1 = 1,
    TILE_FRM_ID_LAST = 4095
};

inline constexpr TileFrameId operator+(TileFrameId lhs, int rhs)
{
    return static_cast<TileFrameId>(static_cast<int>(lhs) + rhs);
}

inline constexpr TileFrameId operator-(TileFrameId lhs, int rhs)
{
    return static_cast<TileFrameId>(static_cast<int>(lhs) - rhs);
}

inline TileFrameId operator++(TileFrameId& e, int)
{
    TileFrameId result = e;
    e = e + 1;
    return result;
}

inline TileFrameId tileFrameIdFromFid(int fid)
{
    return static_cast<TileFrameId>(frameIdFromFid(fid));
}

inline TileFrameId tileFrameIdFromPid(int pid)
{
    return static_cast<TileFrameId>(frameIdFromPid(pid));
}

enum MiscFrameId : int {
    MISC_FRM_ID_INVALID = -1,
    MISC_FRM_ID_FIRST = 0,
    MISC_FRM_ID_2 = 2,
    MISC_FRM_ID_10 = 10, // roktxpd.frm
    MISC_FRM_ID_12 = 12,
    MISC_FRM_ID_29 = 29,
    MISC_FRM_ID_31 = 31,
};

inline constexpr MiscFrameId operator+(MiscFrameId lhs, int rhs)
{
    return static_cast<MiscFrameId>(static_cast<int>(lhs) + rhs);
}

inline constexpr MiscFrameId operator-(MiscFrameId lhs, int rhs)
{
    return static_cast<MiscFrameId>(static_cast<int>(lhs) - rhs);
}

inline MiscFrameId miscFrameIdFromFid(int fid)
{
    return static_cast<MiscFrameId>(frameIdFromFid(fid));
}

inline MiscFrameId miscFrameIdFromPid(int pid)
{
    return static_cast<MiscFrameId>(frameIdFromPid(pid));
}

enum InterfaceFrameId : int {
    INTF_FRM_ID_INVALID = -1,
    INTF_FRM_ID_FIRST = 0,
    INTF_FRM_ID_LAST = 4095,
    INTF_FRM_ID_0 = 0, // blank.frm - used be mset000.frm for top of bouncing mouse cursor
    INTF_FRM_ID_1 = 1, // msef000.frm - hex mouse cursor
    INTF_FRM_ID_2 = 2, // egg
    INTF_FRM_ID_3 = 3, // exit grid marker
    INTF_FRM_ID_6 = 6, // skilldex button normal
    INTF_FRM_ID_7 = 7, // skilldex button pressed
    INTF_FRM_ID_8 = 8, // lilredup.frm - little red button up
    INTF_FRM_ID_9 = 9, // lilreddn.frm - little red button down
    INTF_FRM_ID_10 = 10, // map button pressed
    INTF_FRM_ID_13 = 13, // map button normal
    INTF_FRM_ID_16 = 16, // display monitor background
    INTF_FRM_ID_17 = 17, // options button pressed
    INTF_FRM_ID_18 = 18, // options button normal
    INTF_FRM_ID_31 = 31, // item button pressed
    INTF_FRM_ID_32 = 32, // item button normal
    INTF_FRM_ID_38 = 38,
    INTF_FRM_ID_40 = 40, // burst
    INTF_FRM_ID_41 = 41, // kick.frm - kick text
    INTF_FRM_ID_42 = 42, // punch
    INTF_FRM_ID_43 = 43, // single
    INTF_FRM_ID_44 = 44, // swing
    INTF_FRM_ID_45 = 45, // thrust
    INTF_FRM_ID_46 = 46, // inventory button pressed
    INTF_FRM_ID_47 = 47, // inventory button normal
    INTF_FRM_ID_48 = 48,
    INTF_FRM_ID_49 = 49, // button normal
    INTF_FRM_ID_50 = 50, // button pressed
    INTF_FRM_ID_51 = 51,
    INTF_FRM_ID_52 = 52,
    INTF_FRM_ID_53 = 53,
    INTF_FRM_ID_54 = 54,
    INTF_FRM_ID_56 = 56, // character button pressed
    INTF_FRM_ID_57 = 57, // character button normal
    INTF_FRM_ID_58 = 58, // pipboy button pressed
    INTF_FRM_ID_59 = 59, // pipboy button normal
    INTF_FRM_ID_73 = 73, // item button disabled
    INTF_FRM_ID_82 = 82, // numbers.frm - numbers for the hit points and fatigue counters
    INTF_FRM_ID_83 = 83, // green light
    INTF_FRM_ID_84 = 84, // yellow light
    INTF_FRM_ID_85 = 85, // red light
    INTF_FRM_ID_86 = 86, // perk dialog background
    INTF_FRM_ID_87 = 87, // di_bgup1.frm - dialog big up arrow
    INTF_FRM_ID_88 = 88, // di_bgup2.frm - dialog big up arrow
    INTF_FRM_ID_89 = 89, // di_bgdn1.frm - dialog big down arrow
    INTF_FRM_ID_90 = 90, // di_bgdn2.frm - dialog big down arrow
    INTF_FRM_ID_91 = 91, // di_done1.frm - dialog big done button up
    INTF_FRM_ID_92 = 92, // di_done2.frm - dialog big done button down
    INTF_FRM_ID_93 = 93,
    INTF_FRM_ID_94 = 94,
    INTF_FRM_ID_95 = 95, // red button pressed
    INTF_FRM_ID_96 = 96, // red button normal
    INTF_FRM_ID_97 = 97, // review button normal
    INTF_FRM_ID_98 = 98, // review button pressed
    INTF_FRM_ID_99 = 99, // di_talk.frm - dialog screen subwindow (NPC's)
    INTF_FRM_ID_100 = 100,
    INTF_FRM_ID_101 = 101,
    INTF_FRM_ID_102 = 102, // dialog background
    INTF_FRM_ID_103 = 103, // alltlk.frm - dialog screen background
    INTF_FRM_ID_104 = 104, // interface bar end button
    INTF_FRM_ID_105 = 105, // end button normal
    INTF_FRM_ID_106 = 106, // end button pressed
    INTF_FRM_ID_107 = 107, // end combat button normal
    INTF_FRM_ID_108 = 108, // end combat button pressed
    INTF_FRM_ID_109 = 109, // endltgrn.frm - green lights around end turn/combat window
    INTF_FRM_ID_110 = 110, // endltred.frm - red lights around end turn/combat window
    INTF_FRM_ID_111 = 111, // barter.frm - barter window
    INTF_FRM_ID_113 = 113, // inventory background
    INTF_FRM_ID_114 = 114, // loot background
    INTF_FRM_ID_115 = 115, // hilight1.frm - dialogue upper hilight
    INTF_FRM_ID_116 = 116, // hilight2.frm - dialogue lower hilight
    INTF_FRM_ID_117 = 117, // throw
    INTF_FRM_ID_118 = 118, // called shot background
    INTF_FRM_ID_119 = 119,
    INTF_FRM_ID_120 = 120,
    INTF_FRM_ID_121 = 121,
    INTF_FRM_ID_122 = 122, // previous button normal
    INTF_FRM_ID_123 = 123, // previous button pressed
    INTF_FRM_ID_124 = 124, // next button normal
    INTF_FRM_ID_125 = 125, // next button pressed
    INTF_FRM_ID_126 = 126, // indicator box
    INTF_FRM_ID_127 = 127,
    INTF_FRM_ID_128 = 128,
    INTF_FRM_ID_129 = 129, // months.frm - month strings for pip boy
    INTF_FRM_ID_130 = 130,
    INTF_FRM_ID_131 = 131,
    INTF_FRM_ID_132 = 132,
    INTF_FRM_ID_133 = 133,
    INTF_FRM_ID_136 = 136, // worldmap background
    INTF_FRM_ID_138 = 138, // wmaploc.frm - world map location marker
    INTF_FRM_ID_139 = 139, // wmaptarg.frm - world map move target maker #1
    INTF_FRM_ID_140 = 140, // main menu background image
    INTF_FRM_ID_141 = 141, // ebut_in.frm - map elevator screen
    INTF_FRM_ID_142 = 142, // ebut_out.frm - map elevator screen
    INTF_FRM_ID_143 = 143,
    INTF_FRM_ID_144 = 144,
    INTF_FRM_ID_145 = 145,
    INTF_FRM_ID_146 = 146,
    INTF_FRM_ID_147 = 147,
    INTF_FRM_ID_148 = 148,
    INTF_FRM_ID_149 = 149, // gaj000.frm - map elevator screen
    INTF_FRM_ID_150 = 150,
    INTF_FRM_ID_151 = 151,
    INTF_FRM_ID_154 = 154,
    INTF_FRM_ID_155 = 155,
    INTF_FRM_ID_168 = 168, // hotspot1.frm - town map selector shape #1
    INTF_FRM_ID_169 = 169,
    INTF_FRM_ID_170 = 170, // BIGNUM.frm
    INTF_FRM_ID_171 = 171, // automap.frm - automap window
    INTF_FRM_ID_172 = 172, // autoup.frm - switch up
    INTF_FRM_ID_173 = 173, // autodwn.frm - switch down
    INTF_FRM_ID_174 = 174, // character selector background
    INTF_FRM_ID_175 = 175,
    INTF_FRM_ID_176 = 176,
    INTF_FRM_ID_177 = 177,
    INTF_FRM_ID_178 = 178,
    INTF_FRM_ID_179 = 179,
    INTF_FRM_ID_180 = 180,
    INTF_FRM_ID_181 = 181, // dnarwoff.frm - character editor
    INTF_FRM_ID_182 = 182, // dnarwon.frm - character editor
    INTF_FRM_ID_183 = 183,
    INTF_FRM_ID_184 = 184,
    INTF_FRM_ID_185 = 185,
    INTF_FRM_ID_186 = 186,
    INTF_FRM_ID_187 = 187,
    INTF_FRM_ID_188 = 188,
    INTF_FRM_ID_189 = 189,
    INTF_FRM_ID_190 = 190,
    INTF_FRM_ID_191 = 191,
    INTF_FRM_ID_192 = 192,
    INTF_FRM_ID_193 = 193,
    INTF_FRM_ID_194 = 194,
    INTF_FRM_ID_195 = 195,
    INTF_FRM_ID_196 = 196,
    INTF_FRM_ID_197 = 197,
    INTF_FRM_ID_198 = 198,
    INTF_FRM_ID_199 = 199, // uparwoff.frm - character editor
    INTF_FRM_ID_200 = 200, // uparwon.frm - character editor
    INTF_FRM_ID_201 = 201,
    INTF_FRM_ID_202 = 202,
    INTF_FRM_ID_203 = 203,
    INTF_FRM_ID_204 = 204,
    INTF_FRM_ID_205 = 205,
    INTF_FRM_ID_206 = 206,
    INTF_FRM_ID_207 = 207,
    INTF_FRM_ID_208 = 208, // charwin.frm - character editor
    INTF_FRM_ID_209 = 209, // donebox.frm - character editor
    INTF_FRM_ID_210 = 210,
    INTF_FRM_ID_211 = 211,
    INTF_FRM_ID_212 = 212,
    INTF_FRM_ID_213 = 213,
    INTF_FRM_ID_214 = 214,
    INTF_FRM_ID_215 = 215,
    INTF_FRM_ID_216 = 216,
    INTF_FRM_ID_217 = 217, // LGDIALOG.FRM - Large generic dialog box
    INTF_FRM_ID_218 = 218, // MEDIALOG.FRM - Medium generic dialog box
    INTF_FRM_ID_219 = 219,
    INTF_FRM_ID_220 = 220, // opbase.frm - character editor
    INTF_FRM_ID_221 = 221, // opbtnoff.frm - character editor
    INTF_FRM_ID_222 = 222, // opbtnon.frm - character editor
    INTF_FRM_ID_223 = 223, // hotspot2.frm - town map selector shape #2
    INTF_FRM_ID_224 = 224, // loadbox.frm - character editor
    INTF_FRM_ID_225 = 225, // savebox.frm - character editor
    INTF_FRM_ID_226 = 226,
    INTF_FRM_ID_237 = 237, // lsgame.frm - load/save game
    INTF_FRM_ID_238 = 238, // lsgbox.frm - load/save game
    INTF_FRM_ID_239 = 239, // lscover.frm - load/save game
    INTF_FRM_ID_240 = 240, // prefscrn.frm - options screen
    INTF_FRM_ID_241 = 241, // prfsldof.frm - options screen
    INTF_FRM_ID_242 = 242, // prfbknbs.frm - options screen
    INTF_FRM_ID_243 = 243, // prflknbs.frm - options screen
    INTF_FRM_ID_244 = 244, // prfxin.frm - options screen
    INTF_FRM_ID_245 = 245, // prfxout.frm - options screen
    INTF_FRM_ID_246 = 246, // prefcvr.frm - options screen
    INTF_FRM_ID_247 = 247, // prfsldon.frm - options screen
    INTF_FRM_ID_249 = 249,
    INTF_FRM_ID_250 = 250, // action arrow
    INTF_FRM_ID_251 = 251,
    INTF_FRM_ID_253 = 253, // Cancel
    INTF_FRM_ID_255 = 255, // Drop
    INTF_FRM_ID_257 = 257, // Inventory
    INTF_FRM_ID_259 = 259, // Look
    INTF_FRM_ID_261 = 261, // Rotate
    INTF_FRM_ID_263 = 263, // Talk
    INTF_FRM_ID_265 = 265, // Use/Get
    INTF_FRM_ID_266 = 266, // blank
    INTF_FRM_ID_267 = 267,
    INTF_FRM_ID_268 = 268,
    INTF_FRM_ID_269 = 269,
    INTF_FRM_ID_270 = 270,
    INTF_FRM_ID_271 = 271,
    INTF_FRM_ID_272 = 272,
    INTF_FRM_ID_273 = 273,
    INTF_FRM_ID_274 = 274,
    INTF_FRM_ID_275 = 275,
    INTF_FRM_ID_276 = 276,
    INTF_FRM_ID_277 = 277,
    INTF_FRM_ID_278 = 278,
    INTF_FRM_ID_279 = 279,
    INTF_FRM_ID_280 = 280,
    INTF_FRM_ID_281 = 281,
    INTF_FRM_ID_282 = 282, // actpick.frm - action pick
    INTF_FRM_ID_283 = 283, // actmenu.frm - action menu
    INTF_FRM_ID_284 = 284, // acttohit.frm - action to hit
    INTF_FRM_ID_285 = 285, // mirrored arrow
    INTF_FRM_ID_286 = 286, // pointing hand
    INTF_FRM_ID_288 = 288, // bullseye aiming
    INTF_FRM_ID_289 = 289, // action points
    INTF_FRM_ID_290 = 290, // movement points numbers
    INTF_FRM_ID_291 = 291, // reload action menu
    INTF_FRM_ID_292 = 292, // use action menu
    INTF_FRM_ID_293 = 293,
    INTF_FRM_ID_294 = 294, // use on action menu
    INTF_FRM_ID_295 = 295,
    INTF_FRM_ID_297 = 297, // help background
    INTF_FRM_ID_299 = 299, // main menu button normal
    INTF_FRM_ID_300 = 300, // main menu button pressed
    INTF_FRM_ID_302 = 302, // Unload
    INTF_FRM_ID_304 = 304, // Skill
    INTF_FRM_ID_305 = 305,
    INTF_FRM_ID_306 = 306, // timer overlay
    INTF_FRM_ID_307 = 307,
    INTF_FRM_ID_308 = 308,
    INTF_FRM_ID_309 = 309, // DEATH.FRM
    INTF_FRM_ID_310 = 310,
    INTF_FRM_ID_327 = 327, // endgame ending panning screen
    INTF_FRM_ID_328 = 328,
    INTF_FRM_ID_329 = 329,
    INTF_FRM_ID_330 = 330,
    INTF_FRM_ID_331 = 331,
    INTF_FRM_ID_332 = 332,
    INTF_FRM_ID_333 = 333,
    INTF_FRM_ID_334 = 334,
    INTF_FRM_ID_335 = 335,
    INTF_FRM_ID_336 = 336, // city size small
    INTF_FRM_ID_337 = 337, // city size medium
    INTF_FRM_ID_338 = 338, // city size large
    INTF_FRM_ID_363 = 363, // wmscreen - worldmap overlay screen
    INTF_FRM_ID_364 = 364, // wmtabs.frm - worldmap town tabs underlay
    INTF_FRM_ID_365 = 365, // wmdial.frm - worldmap night/day dial
    INTF_FRM_ID_366 = 366, // wmglobe.frm - worldmap globe stamp overlay
    INTF_FRM_ID_367 = 367, // wmtbedge.frm - worldmap town tabs edging overlay
    INTF_FRM_ID_388 = 388,
    INTF_FRM_ID_389 = 389, // di_talkp.frm - dialog screen subwindow (party members)
    INTF_FRM_ID_390 = 390, // control.frm - party member control interface
    INTF_FRM_ID_391 = 391, // custom.frm - party member control interface
    INTF_FRM_ID_392 = 392,
    INTF_FRM_ID_393 = 393,
    INTF_FRM_ID_394 = 394,
    INTF_FRM_ID_395 = 395,
    INTF_FRM_ID_396 = 396,
    INTF_FRM_ID_397 = 397,
    INTF_FRM_ID_398 = 398,
    INTF_FRM_ID_399 = 399,
    INTF_FRM_ID_400 = 400,
    INTF_FRM_ID_401 = 401,
    INTF_FRM_ID_402 = 402,
    INTF_FRM_ID_403 = 403,
    INTF_FRM_ID_404 = 404,
    INTF_FRM_ID_405 = 405,
    INTF_FRM_ID_406 = 406,
    INTF_FRM_ID_407 = 407,
    INTF_FRM_ID_408 = 408,
    INTF_FRM_ID_409 = 409,
    INTF_FRM_ID_410 = 410,
    INTF_FRM_ID_411 = 411,
    INTF_FRM_ID_412 = 412,
    INTF_FRM_ID_413 = 413,
    INTF_FRM_ID_414 = 414,
    INTF_FRM_ID_415 = 415,
    INTF_FRM_ID_416 = 416,
    INTF_FRM_ID_417 = 417,
    INTF_FRM_ID_418 = 418,
    INTF_FRM_ID_419 = 419, // game dialog background
    INTF_FRM_ID_420 = 420, // trade.frm - party member barter/trade interface
    INTF_FRM_ID_421 = 421, // chop punch
    INTF_FRM_ID_422 = 422, // cm_prckk.frm - death blossom kick text
    INTF_FRM_ID_423 = 423, // dragon punch
    INTF_FRM_ID_424 = 424, // force punch
    INTF_FRM_ID_425 = 425, // hammer punch
    INTF_FRM_ID_426 = 426, // hipk.frm - kip kick text
    INTF_FRM_ID_427 = 427, // cm_hookk.frm - jump kick text
    INTF_FRM_ID_428 = 428, // lightning punch
    INTF_FRM_ID_429 = 429, // cm_pwkck.frm - roundhouse kick text
    INTF_FRM_ID_430 = 430, // skick.frm - strong kick text
    INTF_FRM_ID_431 = 431, // snapkick.frm - snap kick text
    INTF_FRM_ID_432 = 432, // strong punch
    INTF_FRM_ID_433 = 433, // wmcarmve.frm - worldmap car movie
    INTF_FRM_ID_435 = 435, // Push
    INTF_FRM_ID_436 = 436,
    INTF_FRM_ID_437 = 437,
    INTF_FRM_ID_438 = 438,
    INTF_FRM_ID_439 = 439,
};

inline constexpr InterfaceFrameId operator+(InterfaceFrameId lhs, int rhs)
{
    return static_cast<InterfaceFrameId>(static_cast<int>(lhs) + rhs);
}

inline constexpr InterfaceFrameId operator-(InterfaceFrameId lhs, int rhs)
{
    return static_cast<InterfaceFrameId>(static_cast<int>(lhs) - rhs);
}

} // namespace fallout
#endif /* ART_DEFS_H */
