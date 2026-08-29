#ifndef OBJ_TYPES_H
#define OBJ_TYPES_H

#include "worldmap_defs.h"

namespace fallout {

// Rotation
enum Rotation : int {
    ROTATION_INVALID = -1,
    ROTATION_NE, // 0
    ROTATION_E, // 1
    ROTATION_SE, // 2
    ROTATION_SW, // 3
    ROTATION_W, // 4
    ROTATION_NW, // 5
    ROTATION_COUNT,
    ROTATION_FIRST = ROTATION_NE,
    ROTATION_LAST = ROTATION_NW,
};

inline Rotation operator+(Rotation lhs, int rhs)
{
    return static_cast<Rotation>(static_cast<int>(lhs) + rhs);
}

inline Rotation operator-(Rotation lhs, int rhs)
{
    return static_cast<Rotation>(static_cast<int>(lhs) - rhs);
}

inline Rotation operator%(Rotation lhs, int rhs)
{
    return static_cast<Rotation>(static_cast<int>(lhs) % rhs);
}

inline Rotation operator++(Rotation& e, int)
{
    Rotation result = e;
    e = e + 1;
    return result;
}

inline Rotation& operator--(Rotation& e)
{
    e = e - 1;
    return e;
}

inline bool rotationIsValid(int rotation)
{
    return rotation >= ROTATION_FIRST && rotation < ROTATION_COUNT;
}

inline Rotation rotationFromFid(int fid)
{
    int rotation = (fid & 0x70000000) >> 28;
    return static_cast<Rotation>(rotation);
}

enum ObjectType : int {
    OBJ_TYPE_INVALID = -1,
    OBJ_TYPE_ITEM,
    OBJ_TYPE_CRITTER,
    OBJ_TYPE_SCENERY,
    OBJ_TYPE_WALL,
    OBJ_TYPE_TILE,
    OBJ_TYPE_MISC,
    OBJ_TYPE_INTERFACE,
    OBJ_TYPE_INVENTORY,
    OBJ_TYPE_HEAD,
    OBJ_TYPE_BACKGROUND,
    OBJ_TYPE_SKILLDEX,
    OBJ_TYPE_COUNT,
    OBJ_TYPE_PROTO_COUNT = OBJ_TYPE_INTERFACE,
    OBJ_TYPE_FIRST = OBJ_TYPE_ITEM
};

inline ObjectType operator++(ObjectType& e, int)
{
    ObjectType result = e;
    e = static_cast<ObjectType>(static_cast<int>(e) + 1);
    return result;
}

inline bool objectTypeIsValid(int type)
{
    return type >= OBJ_TYPE_FIRST && type < OBJ_TYPE_COUNT;
}

inline ObjectType objectTypeFromFid(int fid)
{
    int objectType = (fid & 0xF000000) >> 24;
    return static_cast<ObjectType>(objectType);
}

inline ObjectType objectTypeFromPid(int pid)
{
    int objectType = pid >> 24;
    return static_cast<ObjectType>(objectType);
}

enum ObjectFrameId : int {
    OBJECT_FRAME_ID_INVALID = -1,
    OBJECT_FRAME_ID_FIRST = 0,
    OBJECT_FRAME_ID_LAST = 4095
};

inline constexpr ObjectFrameId operator+(ObjectFrameId lhs, int rhs) {
    return static_cast<ObjectFrameId>(static_cast<int>(lhs) + rhs);
}

inline constexpr ObjectFrameId operator-(ObjectFrameId lhs, int rhs) {
    return static_cast<ObjectFrameId>(static_cast<int>(lhs) - rhs);
}

inline ObjectFrameId operator++(ObjectFrameId& e, int)
{
    ObjectFrameId result = e;
    e = e + 1;
    return result;
}

inline bool objectFrameIdIsValid(int frmId)
{
    return frmId >= OBJECT_FRAME_ID_FIRST && frmId <= OBJECT_FRAME_ID_LAST;
}

inline ObjectFrameId objectFrameIdFromFid(int fid)
{
    return static_cast<ObjectFrameId>(fid & 0xFFF);
}

inline ObjectFrameId objectFrameIdFromPid(int pid)
{
    return static_cast<ObjectFrameId>(pid & 0xFFFFFF);
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

enum TileFrameId : int {
    TILE_FRM_ID_FIRST = 0,
    TILE_FRM_ID_1 = 1,
    TILE_FRM_ID_LAST = 4095
};

inline constexpr TileFrameId operator+(TileFrameId lhs, int rhs) {
    return static_cast<TileFrameId>(static_cast<int>(lhs) + rhs);
}

inline constexpr TileFrameId operator-(TileFrameId lhs, int rhs) {
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
    return static_cast<TileFrameId>(fid & 0xFFF);
}

inline TileFrameId tileFrameIdFromPid(int pid)
{
    return static_cast<TileFrameId>(pid & 0xFFFFFF);
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

inline constexpr MiscFrameId operator+(MiscFrameId lhs, int rhs) {
    return static_cast<MiscFrameId>(static_cast<int>(lhs) + rhs);
}

inline constexpr MiscFrameId operator-(MiscFrameId lhs, int rhs) {
    return static_cast<MiscFrameId>(static_cast<int>(lhs) - rhs);
}

inline MiscFrameId miscFrameIdFromFid(int fid)
{
    return static_cast<MiscFrameId>(fid & 0xFFF);
}

inline MiscFrameId miscFrameIdFromPid(int pid)
{
    return static_cast<MiscFrameId>(pid & 0xFFFFFF);
}

enum InterfaceFrameId : int {
    INTF_FRM_ID_INVALID = -1,
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

inline constexpr InterfaceFrameId operator+(InterfaceFrameId lhs, int rhs) {
    return static_cast<InterfaceFrameId>(static_cast<int>(lhs) + rhs);
}

inline constexpr InterfaceFrameId operator-(InterfaceFrameId lhs, int rhs) {
    return static_cast<InterfaceFrameId>(static_cast<int>(lhs) - rhs);
}

#define SID_TYPE(value) (value) >> 24

enum OutlineType : int {
    OUTLINE_TYPE_NONE = 0x00,
    OUTLINE_TYPE_HOSTILE = 0x01,
    OUTLINE_TYPE_SAME_TEAM = 0x02,
    OUTLINE_TYPE_BODY = 0x04,
    OUTLINE_TYPE_FRIENDLY = 0x08,
    OUTLINE_TYPE_ITEM = 0x10,
    OUTLINE_TYPE_BLOCKED = 0x20,
    OUTLINE_TYPE_MAX = 0xFFFFFF
};

#define OUTLINE_PALETTED 0x40000000
#define OUTLINE_DISABLED 0x80000000

constexpr inline OutlineType operator&(OutlineType lhs, OutlineType rhs)
{
    return static_cast<OutlineType>(static_cast<int>(lhs) & static_cast<int>(rhs));
}

constexpr inline OutlineType operator&(OutlineType lhs, unsigned int rhs)
{
    return static_cast<OutlineType>(static_cast<int>(lhs) & rhs);
}

constexpr inline OutlineType operator&(OutlineType lhs, int rhs)
{
    return static_cast<OutlineType>(static_cast<int>(lhs) & rhs);
}

constexpr inline OutlineType operator|(OutlineType lhs, int rhs)
{
    return static_cast<OutlineType>(static_cast<int>(lhs) | rhs);
}

constexpr inline OutlineType operator|(OutlineType lhs, unsigned int rhs)
{
    return static_cast<OutlineType>(static_cast<int>(lhs) | rhs);
}

inline OutlineType& operator&=(OutlineType& lhs, unsigned int rhs)
{
    lhs = lhs & rhs;
    return lhs;
}

inline OutlineType& operator|=(OutlineType& lhs, unsigned int rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

enum ObjectFlags : unsigned int {
    OBJECT_NONE = 0x00,
    OBJECT_HIDDEN = 0x01,
    OBJECT_0X02 = 0x02,

    // Specifies that the object should not be saved to the savegame file.
    //
    // This flag is used in these situations:
    //  - To prevent saving of system objects like dude (which has separate
    // saving routine), egg, mouse cursors, etc.
    //  - To prevent saving of temporary objects (projectiles, explosion
    // effects, etc.).
    //  - To prevent saving of objects which cannot be removed for some reason,
    // like objects trying to delete themselves from scripting engine (used
    // together with `OBJECT_HIDDEN` to prevent affecting game world).
    OBJECT_NO_SAVE = 0x04,
    OBJECT_FLAT = 0x08,
    OBJECT_NO_BLOCK = 0x10,
    OBJECT_LIGHTING = 0x20,

    // Specifies that the object should not be removed (freed) from the game
    // world for whatever reason.
    //
    // This flag is used to prevent freeing of system objects like dude, egg,
    // mouse cursors, etc.
    OBJECT_NO_REMOVE = 0x400,
    OBJECT_MULTIHEX = 0x800,
    OBJECT_NO_HIGHLIGHT = 0x1000,
    OBJECT_QUEUED = 0x2000, // set if there was/is any event for the object
    OBJECT_TRANS_RED = 0x4000,
    OBJECT_TRANS_NONE = 0x8000,
    OBJECT_TRANS_WALL = 0x10000,
    OBJECT_TRANS_GLASS = 0x20000,
    OBJECT_TRANS_STEAM = 0x40000,
    OBJECT_TRANS_ENERGY = 0x80000,
    OBJECT_IN_LEFT_HAND = 0x1000000,
    OBJECT_IN_RIGHT_HAND = 0x2000000,
    OBJECT_WORN = 0x4000000,
    OBJECT_WALL_TRANS_END = 0x10000000,
    OBJECT_LIGHT_THRU = 0x20000000,
    OBJECT_SEEN = 0x40000000,
    OBJECT_SHOOT_THRU = 0x80000000,

    OBJECT_IN_ANY_HAND = OBJECT_IN_LEFT_HAND | OBJECT_IN_RIGHT_HAND,
    OBJECT_EQUIPPED = OBJECT_IN_ANY_HAND | OBJECT_WORN,
    OBJECT_FLAG_0xFC000 = OBJECT_TRANS_ENERGY | OBJECT_TRANS_STEAM | OBJECT_TRANS_GLASS | OBJECT_TRANS_WALL | OBJECT_TRANS_NONE | OBJECT_TRANS_RED,
    OBJECT_OPEN_DOOR = OBJECT_SHOOT_THRU | OBJECT_LIGHT_THRU | OBJECT_NO_BLOCK,
};

constexpr inline ObjectFlags operator&(ObjectFlags lhs, ObjectFlags rhs)
{
    return static_cast<ObjectFlags>(static_cast<unsigned int>(lhs) & static_cast<unsigned int>(rhs));
}

constexpr inline ObjectFlags operator|(ObjectFlags lhs, ObjectFlags rhs)
{
    return static_cast<ObjectFlags>(static_cast<unsigned int>(lhs) | static_cast<unsigned int>(rhs));
}

constexpr inline ObjectFlags operator~(ObjectFlags rhs)
{
    return static_cast<ObjectFlags>(~static_cast<unsigned int>(rhs));
}

constexpr inline ObjectFlags operator^(ObjectFlags lhs, ObjectFlags rhs)
{
    return static_cast<ObjectFlags>(static_cast<unsigned int>(lhs) ^ static_cast<unsigned int>(rhs));
}

inline ObjectFlags& operator&=(ObjectFlags& lhs, ObjectFlags rhs)
{
    lhs = lhs & rhs;
    return lhs;
}

inline ObjectFlags& operator|=(ObjectFlags& lhs, ObjectFlags rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

inline ObjectFlags& operator^=(ObjectFlags& lhs, ObjectFlags rhs)
{
    lhs = lhs ^ rhs;
    return lhs;
}

enum DudeState : int {
    DUDE_STATE_SNEAKING = 0,
    DUDE_STATE_POISONED = 1,
    DUDE_STATE_RADIATED = 2,
    DUDE_STATE_LEVEL_UP_AVAILABLE = 3,
    DUDE_STATE_ADDICTED = 4,
    DUDE_STATE_COUNT = 5,
    DUDE_STATE_FIRST = DUDE_STATE_SNEAKING
};

inline bool dudeStateIsValid(int state)
{
    return state >= DUDE_STATE_FIRST && state < DUDE_STATE_COUNT;
}

enum CritterFlags : int {
    CRITTER_NONE = 0x00,
    // CRITTER_DUDE_XXX are valid only for PC
    CRITTER_DUDE_SNEAKING = 0x01,
    CRITTER_DUDE_RADIATED = 0x02,
    CRITTER_DUDE_LEVEL_UP_AVAILABLE = 0x08,
    CRITTER_DUDE_ADDICTED = 0x10,
    CRITTER_BARTER = 0x02,
    CRITTER_NO_STEAL = 0x20,
    CRITTER_NO_DROP = 0x40,
    CRITTER_NO_LIMBS = 0x80,
    CRITTER_NO_AGE = 0x100,
    CRITTER_NO_HEAL = 0x200,
    CRITTER_INVULNERABLE = 0x400,
    CRITTER_FLAT = 0x800,
    CRITTER_SPECIAL_DEATH = 0x1000,
    CRITTER_LONG_LIMBS = 0x2000,
    CRITTER_NO_KNOCKBACK = 0x4000,
};

constexpr inline CritterFlags operator&(CritterFlags lhs, CritterFlags rhs)
{
    return static_cast<CritterFlags>(static_cast<int>(lhs) & static_cast<int>(rhs));
}

constexpr inline CritterFlags operator|(CritterFlags lhs, CritterFlags rhs)
{
    return static_cast<CritterFlags>(static_cast<int>(lhs) | static_cast<int>(rhs));
}

constexpr inline CritterFlags operator~(CritterFlags rhs)
{
    return static_cast<CritterFlags>(~static_cast<int>(rhs));
}

inline CritterFlags& operator&=(CritterFlags& lhs, CritterFlags rhs)
{
    lhs = lhs & rhs;
    return lhs;
}

inline CritterFlags& operator|=(CritterFlags& lhs, CritterFlags rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

// These two values are the same but stored in different fields.
#define CONTAINER_FLAG_JAMMED 0x04000000
#define DOOR_FLAG_JAMMGED 0x04000000

#define CONTAINER_FLAG_LOCKED 0x02000000
#define DOOR_FLAG_LOCKED 0x02000000

enum CritterManeuver : int {
    CRITTER_MANEUVER_NONE = 0,
    CRITTER_MANEUVER_ENGAGING = 0x01,
    CRITTER_MANEUVER_DISENGAGING = 0x02,
    CRITTER_MANUEVER_FLEEING = 0x04,
};

constexpr inline CritterManeuver operator&(CritterManeuver lhs, CritterManeuver rhs)
{
    return static_cast<CritterManeuver>(static_cast<int>(lhs) & static_cast<int>(rhs));
}

constexpr inline CritterManeuver operator|(CritterManeuver lhs, CritterManeuver rhs)
{
    return static_cast<CritterManeuver>(static_cast<int>(lhs) | static_cast<int>(rhs));
}

constexpr inline CritterManeuver operator~(CritterManeuver rhs)
{
    return static_cast<CritterManeuver>(~static_cast<int>(rhs));
}

inline CritterManeuver& operator&=(CritterManeuver& lhs, CritterManeuver rhs)
{
    lhs = lhs & rhs;
    return lhs;
}

inline CritterManeuver& operator|=(CritterManeuver& lhs, CritterManeuver rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

enum Dam : int {
    DAM_NONE = 0x00,
    DAM_KNOCKED_OUT = 0x01,
    DAM_KNOCKED_DOWN = 0x02,
    DAM_CRIP_LEG_LEFT = 0x04,
    DAM_CRIP_LEG_RIGHT = 0x08,
    DAM_CRIP_ARM_LEFT = 0x10,
    DAM_CRIP_ARM_RIGHT = 0x20,
    DAM_BLIND = 0x40,
    DAM_DEAD = 0x80,
    DAM_HIT = 0x100,
    DAM_CRITICAL = 0x200,
    DAM_ON_FIRE = 0x400,
    DAM_BYPASS = 0x800,
    DAM_EXPLODE = 0x1000,
    DAM_DESTROY = 0x2000,
    DAM_DROP = 0x4000,
    DAM_LOSE_TURN = 0x8000,
    DAM_HIT_SELF = 0x10000,
    DAM_LOSE_AMMO = 0x20000,
    DAM_DUD = 0x40000,
    DAM_HURT_SELF = 0x80000,
    DAM_RANDOM_HIT = 0x100000,
    DAM_CRIP_RANDOM = 0x200000,
    DAM_BACKWASH = 0x400000,
    DAM_PERFORM_REVERSE = 0x800000,
    DAM_CRIP_LEG_ANY = DAM_CRIP_LEG_LEFT | DAM_CRIP_LEG_RIGHT,
    DAM_CRIP_ARM_ANY = DAM_CRIP_ARM_LEFT | DAM_CRIP_ARM_RIGHT,
    DAM_CRIP = DAM_CRIP_LEG_ANY | DAM_CRIP_ARM_ANY | DAM_BLIND,
};

constexpr inline Dam operator&(Dam lhs, Dam rhs)
{
    return static_cast<Dam>(static_cast<int>(lhs) & static_cast<int>(rhs));
}

constexpr inline Dam operator|(Dam lhs, Dam rhs)
{
    return static_cast<Dam>(static_cast<int>(lhs) | static_cast<int>(rhs));
}

constexpr inline Dam operator~(Dam rhs)
{
    return static_cast<Dam>(~static_cast<int>(rhs));
}

inline Dam& operator&=(Dam& lhs, Dam rhs)
{
    lhs = lhs & rhs;
    return lhs;
}

inline Dam& operator|=(Dam& lhs, Dam rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

#define OBJ_LOCKED 0x02000000
#define OBJ_JAMMED 0x04000000

typedef struct Object Object;

typedef struct InventoryItem {
    Object* item;
    int quantity;
} InventoryItem;

// Represents inventory of the object.
typedef struct Inventory {
    int length;
    int capacity;
    InventoryItem* items;
} Inventory;

typedef struct WeaponObjectData {
    int ammoQuantity; // obj_pudg.pudweapon.cur_ammo_quantity
    int ammoTypePid; // obj_pudg.pudweapon.cur_ammo_type_pid
} WeaponObjectData;

typedef struct AmmoItemData {
    int quantity; // obj_pudg.pudammo.cur_ammo_quantity
} AmmoItemData;

typedef struct MiscItemData {
    int charges; // obj_pudg.pudmisc_item.curr_charges
} MiscItemData;

typedef struct KeyItemData {
    int keyCode; // obj_pudg.pudkey_item.cur_key_code
} KeyItemData;

typedef union ItemObjectData {
    WeaponObjectData weapon;
    AmmoItemData ammo;
    MiscItemData misc;
    KeyItemData key;
} ItemObjectData;

typedef struct CritterCombatData {
    CritterManeuver maneuver; // obj_pud.combat_data.maneuver
    int ap; // obj_pud.combat_data.curr_mp
    Dam results; // obj_pud.combat_data.results
    int damageLastTurn; // obj_pud.combat_data.damage_last_turn
    int aiPacket; // obj_pud.combat_data.ai_packet
    int team; // obj_pud.combat_data.team_num
    union {
        Object* whoHitMe; // obj_pud.combat_data.who_hit_me
        int whoHitMeCid;
    };
} CritterCombatData;

typedef struct CritterObjectData {
    int reaction; // obj_pud.reaction_to_pc (unused)
    CritterCombatData combat; // obj_pud.combat_data
    int hp; // obj_pud.curr_hp
    int radiation; // obj_pud.curr_rad
    int poison; // obj_pud.curr_poison
} CritterObjectData;

typedef struct DoorSceneryData {
    int openFlags; // obj_pudg.pudportal.cur_open_flags
} DoorSceneryData;

typedef struct StairsSceneryData {
    Map destinationMap; // obj_pudg.pudstairs.destMap
    int destinationBuiltTile; // obj_pudg.pudstairs.destBuiltTile
} StairsSceneryData;

typedef struct ElevatorSceneryData {
    int type;
    int level;
} ElevatorSceneryData;

typedef struct LadderSceneryData {
    Map destinationMap;
    int destinationBuiltTile;
} LadderSceneryData;

typedef union SceneryObjectData {
    DoorSceneryData door;
    StairsSceneryData stairs;
    ElevatorSceneryData elevator;
    LadderSceneryData ladder;
} SceneryObjectData;

typedef struct MiscObjectData {
    Map map;
    int tile;
    int elevation;
    Rotation rotation;
} MiscObjectData;

// TODO: use C-style inheritance for different ObjectData variants instead of unions within unions.
typedef struct ObjectData {
    Inventory inventory;
    union {
        CritterObjectData critter;
        struct {
            int flags;
            union {
                ItemObjectData item;
                SceneryObjectData scenery;
                MiscObjectData misc;
            };
        };
    };
} ObjectData;

typedef struct Object {
    int id; // obj_id
    int tile; // obj_tile_num
    int x; // obj_x
    int y; // obj_y
    int sx; // obj_sx
    int sy; // obj_sy
    int frame; // obj_cur_frm
    Rotation rotation; // obj_cur_rot
    int fid; // obj_fid
    ObjectFlags flags; // obj_flags
    int elevation; // obj_elev
    ObjectData data;
    int pid; // obj_pid
    int cid; // obj_cid
    int lightDistance; // obj_light_distance
    int lightIntensity; // obj_light_intensity
    OutlineType outline; // obj_outline
    int sid; // obj_sid
    Object* owner;
    int scriptIndex; // TODO: remove
} Object;

typedef struct ObjectListNode {
    Object* obj;
    struct ObjectListNode* next;
} ObjectListNode;

#define BUILT_TILE_TILE_MASK 0x3FFFFFF
#define BUILT_TILE_ELEVATION_MASK 0xE0000000
#define BUILT_TILE_ELEVATION_SHIFT 29
#define BUILT_TILE_ROTATION_MASK 0x1C000000
#define BUILT_TILE_ROTATION_SHIFT 26

static inline int builtTileGetTile(int builtTile)
{
    return builtTile & BUILT_TILE_TILE_MASK;
}

static inline int builtTileGetElevation(int builtTile)
{
    return (builtTile & BUILT_TILE_ELEVATION_MASK) >> BUILT_TILE_ELEVATION_SHIFT;
}

static inline Rotation builtTileGetRotation(int builtTile)
{
    return static_cast<Rotation>((builtTile & BUILT_TILE_ROTATION_MASK) >> BUILT_TILE_ROTATION_SHIFT);
}

static inline int builtTileCreate(int tile, int elevation)
{
    return tile | ((elevation << BUILT_TILE_ELEVATION_SHIFT) & BUILT_TILE_ELEVATION_MASK);
}

} // namespace fallout

#endif /* OBJ_TYPES_H */
