#include "mapper/mapper.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "actions.h"
#include "animation.h"
#include "art.h"
#include "character_editor.h"
#include "color.h"
#include "config.h"
#include "critter.h"
#include "db.h"
#include "debug.h"
#include "draw.h"
#include "game.h"
#include "game_config.h"
#include "game_dialog.h"
#include "game_mouse.h"
#include "game_movie.h"
#include "game_sound.h"
#include "graph_lib.h"
#include "input.h"
#include "interface.h"
#include "inventory.h"
#include "kb.h"
#include "light.h"
#include "loadsave.h"
#include "map.h"
#include "mapper/map_func.h"
#include "mapper/mp_proto.h"
#include "mapper/mp_scrpt.h"
#include "mapper/mp_targt.h"
#include "mapper/mp_text.h"
#include "memory.h"
#include "mouse.h"
#include "object.h"
#include "party_member.h"
#include "proto.h"
#include "proto_instance.h"
#include "scripts.h"
#include "settings.h"
#include "svga.h"
#include "text_font.h"
#include "tile.h"
#include "window_manager.h"
#include "window_manager_private.h"

namespace fallout {

static void MapperInit();
static int mapper_edit_init(int argc, char** argv);
static void mapper_edit_exit();
static int bookmarkInit();
static int bookmarkExit();
static void bookmarkHide();
static void bookmarkUnHide();
static int categoryInit();
static int categoryExit();
static int categoryHide();
static int categoryToggleState();
static int categoryUnhide();
static bool proto_user_is_librarian();
static void edit_mapper();
static void mapper_load_toolbar(int type, int* out_offset);
static void mapper_save_toolbar();
static void redraw_toolname();
static void clear_toolname();
static void update_toolname(int* pid, int type, int id);
static void update_high_obj_name(Object* obj);
static void mapper_destroy_highlight_obj(Object** a1, Object** a2);
static void mapper_refresh_rotation();
static void update_art(int type, int offset);
static int mapperPickObject(Object* obj, int* outOffset);
static int mapperPickTile(int* outOffset);
static void handle_new_map(int* a1, int* a2);
static int mapper_mark_exit_grid();
static void mapper_mark_all_exit_grids();
static void mapper_enter_play_mode(int* pCurrentType, int* pScrollOffset, Object** pHlObj1, Object* dudeToRestore);
static void mapper_exit_play_mode(int* pCurrentType, int* pScrollOffset, Object* dudeToRestore);

// TODO: Underlying menu/pulldown interface wants menu items to be non-const,
// needs some refactoring.

static char kSeparator[] = "";

static char kNew[] = " New ";
static char kOpen[] = " Open ";
static char kSave[] = " Save ";
static char kSaveAs[] = " Save As... ";
static char kInfo[] = " Info ";
static char kOpenFromText[] = " Open From Text ";
static char kQuit[] = " Quit ";

static char kCreatePattern[] = " Create Pattern ";
static char kUsePattern[] = " Use Pattern ";
static char kMoveMap[] = " Move Map ";
static char kMoveMapElev[] = " Move Map Elev ";
static char kCopyMapElev[] = " Copy Map Elev ";
static char kEditObjDude[] = " Edit Obj_dude ";
static char kFlushCache[] = " Flush Cache ";
static char kToggleAnimStepping[] = " Toggle Anim Stepping ";
static char kFixMapObjectsToPids[] = " Fix map-objects to pids ";
static char kSetBookmark[] = " Set Bookmark ";
static char kToggleBlockObjView[] = " Toggle Block Obj View ";
static char kToggleClickToScroll[] = " Toggle Click-To-Scroll ";
static char kSetExitGridData[] = " Set Exit-Grid Data ";
static char kMarkExitGrids[] = " Mark Exit-Grids ";
static char kMarkAllExitGrids[] = " Mark *ALL* Exit Grids ";
static char kClearMapLevel[] = " *Clear Map Level* ";
static char kCreateAllMapTexts[] = " Create ALL MAP TEXTS ";
static char kRebuildAllMaps[] = " Rebuild ALL MAPS ";

static char kListAllScripts[] = " List all Scripts ";
static char kSetStartHex[] = " Set Start Hex ";
static char kPlaceSpatialScript[] = " Place Spatial Script ";
static char kDeleteSpatialScript[] = " Delete Spatial Script ";
static char kDeleteAllSpatialScripts[] = " Delete *ALL* Spatial SCRIPTS! ";
static char kCreateScript[] = " Create Script ";
static char kSetMapScript[] = " Set Map Script ";
static char kShowMapScript[] = " Show Map Script ";

static char kRebuildWeapons[] = " Rebuild Weapons ";
static char kRebuildProtoList[] = " Rebuild Proto List ";
static char kRebuildAll[] = " Rebuild ALL ";
static char kRebuildBinary[] = " Rebuild Binary ";
static char kArtToProtos[] = " Art => New Protos ";
static char kSwapPrototypse[] = " Swap Prototypes ";

static char kTmpMapName[] = "TMP$MAP#.MAP";

// 0x559618
int rotate_arrows_x_offs[] = {
    31,
    38,
    31,
    11,
    3,
    11,
};

// 0x559630
int rotate_arrows_y_offs[] = {
    7,
    23,
    37,
    37,
    23,
    7,
};

// 0x559648
char* menu_0[] = {
    kNew,
    kOpen,
    kSave,
    kSaveAs,
    kSeparator,
    kInfo,
    kOpenFromText,
    kQuit,
};

// 0x559668
char* menu_1[] = {
    kCreatePattern,
    kUsePattern,
    kSeparator,
    kMoveMap,
    kMoveMapElev,
    kCopyMapElev,
    kSeparator,
    kEditObjDude,
    kFlushCache,
    kToggleAnimStepping,
    kFixMapObjectsToPids,
    kSetBookmark,
    kToggleBlockObjView,
    kToggleClickToScroll,
    kSetExitGridData,
    kMarkExitGrids,
    kMarkAllExitGrids,
    kClearMapLevel,
    kSeparator,
    kCreateAllMapTexts,
    kRebuildAllMaps,
};

// 0x5596BC
char* menu_2[] = {
    kListAllScripts,
    kSetStartHex,
    kPlaceSpatialScript,
    kDeleteSpatialScript,
    kDeleteAllSpatialScripts,
    kCreateScript,
    kSetMapScript,
    kShowMapScript,
};

// 0x5596DC
char* menu_3[] = {
    kRebuildWeapons,
    kRebuildProtoList,
    kRebuildAll,
    kRebuildBinary,
    kSeparator,
    kArtToProtos,
    kSwapPrototypse,
};

// 0x5596F8
char** menu_names[] = {
    menu_0,
    menu_1,
    menu_2,
    menu_3,
};

// 0x559748
MapTransition mapInfo = { -1, -1, 0, 0 };

// 0x559880
int max_art_buttons = 7;

// 0x559884
int art_scale_width = 49;

// 0x559888
int art_scale_height = 48;

// Toolbar scroll state — 6 types × 48 bytes.
// Saved to / loaded from a per-map .cfg sidecar file alongside each .MAP.
struct ToolbarInfo {
    int offset;
    int bookmark[10]; // digit-key bookmarks 0-9
    int _pad;
};
static ToolbarInfo toolbar_info[6];

// Currently highlighted art-slot index in the toolbar; -1 = none.
static int tool_active = -1;

// Color (palette index) used to draw the selection box around the active slot.
static int toolbar_selection_color = 21;

// Highlighted object on screen — used by mapper_destroy_highlight_obj.
static Object* _screen_obj = nullptr;

// Tracks user context: 0 = none, 1 = toolbar, 2 = hex grid.
// Used to distinguish toolbar button clicks from keyboard keys (e.g. 'e').
static int _edit_area = 0;

// 0x5598A0
static bool map_entered = false;

// 0x5598A4
static char* tmp_map_name = kTmpMapName;

// 0x5598A8
static int bookmarkWin = -1;

// 0x5598AC
static int categoryWin = -1;

// 0x5598B0
static bool categoryIsHidden = false;

// 0x6EAA40
int menu_val_0[8];

// 0x6EAA60
int menu_val_2[8];

// 0x6EAA80
unsigned char e_num[4][19 * 26];

// 0x6EBD28
unsigned char rotate_arrows[2][6][10 * 10];

// 0x6EC408
int menu_val_1[21];

// 0x6EC468
unsigned char* art_shape;

// 0x6EC46C
int to_paint_bid;

// 0x6EC470
int edit_bid;

// 0x6EC474
int paste_bid;

// 0x6EC478
int misc_bid;

// 0x6EC47C
int tile_bid;

// 0x6EC480
int copy_bid;

// 0x6EC484
int delete_bid;

// 0x6EC488
int wall_bid;

// 0x6EC48C
int obj_bid;

// 0x6EC490
int to_topdown_bid;

// 0x6EC494
int roof_bid;

// 0x6EC498
int hex_bid;

// 0x6EC49C
int to_iso_bid;

// 0x6EC4A0
int scen_bid;

// 0x6EC4A4
int crit_bid;

// 0x6EC4A8
unsigned char* tool_buf;

// 0x6EC4AC
int tool_win;

// 0x6EC4B0
int menu_bar;

// 0x6EC4B4
unsigned char* lbm_buf;

// 0x6EC4B8
unsigned char height_inc_up[18 * 23];

// 0x6EC656
unsigned char height_dec_up[18 * 23];

// 0x6EC7F4
unsigned char height_dec_down[18 * 23];

// 0x6EC992
unsigned char height_inc_down[18 * 23];

// 0x6ECB30
unsigned char obj_down[66 * 13];

// 0x6ECE8A
unsigned char to_iso_down[58 * 13];

// 0x6ED17C
unsigned char scen_up[66 * 13];

// 0x6ED4D6
unsigned char roof_up[58 * 13];

// 0x6ED7C8
unsigned char crit_down[66 * 13];

// 0x6EDB22
unsigned char obj_up[66 * 13];

// 0x6EDE7C
unsigned char crit_up[66 * 13];

// 0x6EE1D6
unsigned char to_topdown_down[58 * 13];

// 0x6EE4C8
unsigned char hex_up[58 * 13];

// 0x6EE7BA
unsigned char hex_down[58 * 13];

// 0x6EEAAC
unsigned char to_topdown_up[58 * 13];

// 0x6EED9E
unsigned char scen_down[66 * 13];

// 0x6EF0F8
unsigned char edec_down[18 * 23];

// 0x6EF296
unsigned char to_iso_up[58 * 13];

// 0x6EF588
unsigned char roof_down[58 * 13];

// 0x6EF87A
unsigned char r_up[18 * 23];

// 0x6EFA18
unsigned char einc_down[18 * 23];

// 0x6EFBB6
unsigned char shift_l_up[18 * 23];

// 0x6EFD54
unsigned char edec_up[18 * 23];

// 0x6EFEF2
unsigned char shift_r_up[18 * 23];

// 0x6F0090
unsigned char shift_r_down[18 * 23];

// 0x6F022E
unsigned char r_down[18 * 23];

// 0x6F03CC
unsigned char einc_up[18 * 23];

// 0x6F056A
unsigned char l_down[18 * 23];

// 0x6F0708
unsigned char shift_l_down[18 * 23];

// 0x6F08A6
unsigned char l_up[18 * 23];

// 0x6F0A44
unsigned char to_edit_up[45 * 43];

// 0x6F11D3
unsigned char erase_up[45 * 43];

// 0x6F1962
unsigned char copy_group_up[45 * 43];

// 0x6F20F1
unsigned char to_paint_down[45 * 43];

// 0x6F2880
unsigned char erase_down[45 * 43];

// 0x6F300F
unsigned char copy_group_down[45 * 43];

// 0x6F379E
unsigned char to_edit_down[45 * 43];

// 0x6F3F2D
unsigned char copy_up[49 * 19];

// 0x6F42D0
unsigned char misc_down[53 * 18];

// 0x6F4581
unsigned char wall_down[53 * 18];

// 0x6F4832
unsigned char delete_up[49 * 19];

// 0x6F4BD5
unsigned char edit_up[49 * 19];

// 0x6F4F78
unsigned char tile_up[53 * 18];

// 0x6F5229
unsigned char edit_down[49 * 19];

// 0x6F55CC
unsigned char paste_down[49 * 19];

// 0x6F596F
unsigned char delete_down[49 * 19];

// 0x6F5D12
unsigned char tile_down[53 * 18];

// 0x6F5FC3
unsigned char copy_down[49 * 19];

// 0x6F6366
unsigned char misc_up[53 * 18];

// 0x6F6617
unsigned char paste_up[49 * 19];

// 0x6F69BA
unsigned char to_paint_up[1935];

// 0x6F7149
unsigned char wall_up[53 * 18];

// 0x6F73FA
bool draw_mode;

// 0x6F73FB
bool view_mode;

// Button IDs from toolbar — must match buttonCreate calls in mapper_edit_init.
// Left-click: 161 + max_art_buttons + index
// Right-click: 160 + index
constexpr int kArtButtonBase = 161;

constexpr int kBtnScrollLeft = '-';
constexpr int kBtnScrollRight = '=';
constexpr int kBtnScrollPageLeft = '_';
constexpr int kBtnScrollPageRight = '+';
constexpr int kBtnElevUp = KEY_PAGE_UP;
constexpr int kBtnElevDown = KEY_PAGE_DOWN;
constexpr int kBtnHideItem = KEY_CTRL_F1;
constexpr int kBtnHideCrit = KEY_CTRL_F2;
constexpr int kBtnHideScen = KEY_CTRL_F3;
constexpr int kBtnHideWall = KEY_CTRL_F4;
constexpr int kBtnHideTile = KEY_CTRL_F5;
constexpr int kBtnHideMisc = KEY_CTRL_F6;
constexpr int kBtnCopy = 'c';
constexpr int kBtnPaste = 'C';
constexpr int kBtnEdit = 'e';
constexpr int kBtnDelete = KEY_DELETE;
constexpr int kBtnRotateRight = KEY_CTRL_ARROW_RIGHT;
constexpr int kBtnRotateLeft = KEY_CTRL_ARROW_LEFT;
constexpr int kBtnRoof = 'r';
constexpr int kBtnHex = 'h';
constexpr int kMenuHeaderFile = KEY_ALT_F;
constexpr int kMenuHeaderTools = KEY_ALT_V;
constexpr int kMenuHeaderScripts = KEY_ALT_T;
constexpr int kMenuHeaderLibrarian = KEY_ALT_J;
constexpr int kMenuBarActivation = KEY_F8;
constexpr int kMenuBarActivationAlt = KEY_CTRL_F12;
constexpr int kBtnEraseMap = KEY_F12;

constexpr int kBtnMoveMapElev = 4186;
constexpr int kBtnCopyMapElev = 4188;
constexpr int kBtnBookmark = 2165;
constexpr int kBtnToggleBlockObjView = 3123;
constexpr int kBtnSetExitGridData = 5677;
constexpr int kBtnMarkExitGrids = 5678;
constexpr int kBtnMarkAllExitGrids = 5679;
constexpr int kBtnClearMapLevel = 5666;
constexpr int kBtnCreateAllMapTexts = 5406;
constexpr int kBtnRebuildAllMaps = 5405;
constexpr int kBtnSetStartHex = 5400;
constexpr int kBtnDeleteAllSpatialScripts = 5410;
constexpr int kBtnShowMapScript = 5544;

// FILE menu pulldown keycodes
constexpr int kBtnNew = KEY_ALT_N;
constexpr int kBtnOpen = KEY_ALT_O;
constexpr int kBtnSave = KEY_ALT_S;
constexpr int kBtnSaveAs = KEY_ALT_A;
constexpr int kBtnInfo = KEY_ALT_I;
constexpr int kBtnLoadAllTexts = KEY_ALT_K;

// TOOLS menu pulldown keycodes
constexpr int kBtnCreatePattern = KEY_ALT_U;
constexpr int kBtnUsePattern = KEY_ALT_Y;
constexpr int kBtnMoveMap = KEY_ALT_G;
constexpr int kBtnEditDude = KEY_ALT_B;
constexpr int kBtnFlushCache = KEY_ALT_E;
constexpr int kBtnAnimStepping = KEY_ALT_D;
constexpr int kBtnFixMapPids = KEY_LOWERCASE_B;
constexpr int kBtnClickToScroll = KEY_ALT_Z;

// SCRIPTS menu pulldown keycodes
constexpr int kBtnListScripts = KEY_LOWERCASE_I;
constexpr int kBtnPlaceSpatial = KEY_LOWERCASE_S;
constexpr int kBtnDeleteSpatial = KEY_CTRL_F8;
constexpr int kBtnCreateScript = KEY_GRAVE;
constexpr int kBtnSetMapScript = KEY_ALT_W;

// Toolbar type selection (F1–F6)
constexpr int kBtnTypeItem = KEY_F1;
constexpr int kBtnTypeCritter = KEY_F2;
constexpr int kBtnTypeScenery = KEY_F3;
constexpr int kBtnTypeWall = KEY_F4;
constexpr int kBtnTypeTile = KEY_F5;
constexpr int kBtnTypeMisc = KEY_F6;

// Direct keys (not menu-bound)
constexpr int kBtnProtoEditor = KEY_UPPERCASE_E;
constexpr int kBtnPickProto = KEY_LOWERCASE_P;
constexpr int kBtnLightDistDec = KEY_LOWERCASE_Z;
constexpr int kBtnLightIntensityDec = KEY_UPPERCASE_Z;
constexpr int kBtnUse = KEY_LOWERCASE_U;
constexpr int kBtnKill = KEY_LOWERCASE_K;
constexpr int kBtnLock = KEY_LOWERCASE_L;
constexpr int kBtnProtoNewEdit = KEY_COLON;
constexpr int kBtnPickToolbarType = KEY_LOWERCASE_T;
constexpr int kBtnLightAmbientDec = KEY_BRACKET_LEFT;
constexpr int kBtnLightAmbientInc = KEY_BRACKET_RIGHT;
constexpr int kBtnQuit = KEY_ESCAPE;
constexpr int kBtnClearScroll = 0x175;
constexpr int kBtnLastProto = 0x14F;
constexpr int kBtnGotoDudeElev = 0x147;
constexpr int kBtnToggleInterruptWalk = KEY_CTRL_F11;
constexpr int kBtnRotation0 = KEY_CTRL_F9;
constexpr int kBtnRotation3 = KEY_CTRL_F10;
constexpr int kBtnDestroyAllScripts = KEY_UPPERCASE_A;
constexpr int kBtnRebuildSprayTools = 0x143;
constexpr int kBtnRebuildProtoList = 0x144;
constexpr int kBtnDestroyProtoList = 0x185;
constexpr int kBtnHighlightByProto = 0x123;
constexpr int kBtnAnimDebugStep = 0x12E;

constexpr int kArtMaxDirect = 0x4B0;

// gnw_main
// 0x485DD0
int mapper_main(int argc, char** argv)
{
    MapperInit();

    if (mapper_edit_init(argc, argv) == -1) {
        mem_check();
        return 0;
    }

    edit_mapper();
    mapper_edit_exit();
    mem_check();

    return 0;
}

// 0x485E00
void MapperInit()
{
    menu_val_0[0] = kBtnNew;
    menu_val_0[1] = kBtnOpen;
    menu_val_0[2] = kBtnSave;
    menu_val_0[3] = kBtnSaveAs;
    menu_val_0[4] = KEY_ESCAPE;
    menu_val_0[5] = kBtnLoadAllTexts;
    menu_val_0[6] = kBtnInfo;
    menu_val_0[7] = KEY_ESCAPE;

    menu_val_1[0] = kBtnCreatePattern;
    menu_val_1[1] = kBtnUsePattern;
    menu_val_1[2] = KEY_ESCAPE;
    menu_val_1[3] = kBtnMoveMap;
    menu_val_1[4] = kBtnMoveMapElev;
    menu_val_1[5] = kBtnCopyMapElev;
    menu_val_1[6] = KEY_ESCAPE;
    menu_val_1[7] = kBtnEditDude;
    menu_val_1[8] = kBtnFlushCache;
    menu_val_1[9] = kBtnAnimStepping;
    menu_val_1[10] = kBtnFixMapPids;
    menu_val_1[11] = kBtnBookmark;
    menu_val_1[12] = kBtnToggleBlockObjView;
    menu_val_1[13] = kBtnClickToScroll;
    menu_val_1[14] = kBtnSetExitGridData;
    menu_val_1[15] = kBtnMarkExitGrids;
    menu_val_1[16] = kBtnMarkAllExitGrids;
    menu_val_1[17] = kBtnClearMapLevel;
    menu_val_1[18] = KEY_ESCAPE;
    menu_val_1[19] = kBtnCreateAllMapTexts;
    menu_val_1[20] = kBtnRebuildAllMaps;

    menu_val_2[0] = kBtnListScripts;
    menu_val_2[1] = kBtnSetStartHex;
    menu_val_2[2] = kBtnPlaceSpatial;
    menu_val_2[3] = kBtnDeleteSpatial;
    menu_val_2[4] = kBtnDeleteAllSpatialScripts;
    menu_val_2[5] = kBtnCreateScript;
    menu_val_2[6] = kBtnSetMapScript;
    menu_val_2[7] = kBtnShowMapScript;
}

static int loadMapperLbm(int lbmBufWidth, int lbmBufHeight)
{
    lbm_buf = (unsigned char*)internal_malloc(lbmBufWidth * lbmBufHeight);
    return load_lbm_to_buf("data\\mapper2.lbm",
        lbm_buf,
        0,
        0,
        lbmBufWidth - 1,
        lbmBufHeight - 1);
}

// 0x485F94
int mapper_edit_init(int argc, char** argv)
{
    constexpr int lbmBufWidth = 640;
    constexpr int lbmBufHeight = 480;

    int index;

    if (gameInitWithOptions("FALLOUT Mapper", true, 2, WINDOW_MANAGER_INIT_FLAG_NONE, argc, argv) == -1) {
        return -1;
    }

    tileEnable();
    gmouse_set_mapper_mode(true);
    settings.system.executable = "mapper";

    if (settings.mapper.override_librarian) {
        can_modify_protos = true;
        target_override_protection();
    }

    setup_map_dirs();
    mapper_load_toolbar(4, nullptr);
    art_shape = (unsigned char*)internal_malloc(art_scale_height * art_scale_width);
    if (art_shape == nullptr) {
        printf("Can't malloc memory!!\n");
        exit(1);
    }

    if (loadMapperLbm(lbmBufWidth, lbmBufHeight) == -1) {
        // TODO: proper cleanup?
        return -1;
    }

    const int screenWidth = rectGetWidth(&_scr_size);

    max_art_buttons = (screenWidth - 135) / 50;

    menu_bar = windowCreate(0,
        0,
        screenWidth,
        16,
        _colorTable[0],
        WINDOW_HIDDEN);
    _win_register_menu_bar(menu_bar,
        0,
        0,
        screenWidth,
        16,
        260,
        _colorTable[8456]);
    _win_register_menu_pulldown(menu_bar,
        8,
        "FILE",
        289,
        8,
        menu_names[0],
        260,
        _colorTable[8456]);
    _win_register_menu_pulldown(menu_bar,
        40,
        "TOOLS",
        303,
        21,
        menu_names[1],
        260,
        _colorTable[8456]);
    _win_register_menu_pulldown(menu_bar,
        80,
        "SCRIPTS",
        276,
        8,
        menu_names[2],
        260,
        _colorTable[8456]);

    if (can_modify_protos) {
        _win_register_menu_pulldown(menu_bar,
            130,
            "LIBRARIAN",
            292,
            6,
            &(menu_1[14]),
            260,
            _colorTable[8456]);
    }

    tool_win = windowCreate(0,
        _scr_size.bottom - 99,
        screenWidth,
        100,
        256,
        0);

    tool_buf = windowGetBuffer(tool_win);

    blitBufferToBuffer(lbm_buf + 380 * lbmBufWidth, lbmBufWidth, 100, lbmBufWidth, tool_buf, screenWidth);

    blitBufferToBuffer(lbm_buf + 406 * lbmBufWidth + 101, 18, 23, lbmBufWidth, l_up, 18);
    blitBufferToBuffer(lbm_buf + 253 * lbmBufWidth + 101, 18, 23, lbmBufWidth, l_down, 18);
    buttonCreate(tool_win, 101, 26, 18, 23, -1, -1, kBtnScrollLeft, -1, l_up, l_down, nullptr, 0);

    blitBufferToBuffer(lbm_buf + 406 * lbmBufWidth + 622, 18, 23, lbmBufWidth, r_up, 18);
    blitBufferToBuffer(lbm_buf + 253 * lbmBufWidth + 622, 18, 23, lbmBufWidth, r_down, 18);
    buttonCreate(tool_win, _scr_size.right - 18, 26, 18, 23, -1, -1, kBtnScrollRight, -1, r_up, r_down, nullptr, 0);

    blitBufferToBuffer(lbm_buf + 381 * lbmBufWidth + 101, 18, 23, lbmBufWidth, shift_l_up, 18);
    blitBufferToBuffer(lbm_buf + 228 * lbmBufWidth + 101, 18, 23, lbmBufWidth, shift_l_down, 18);
    buttonCreate(tool_win, 101, 1, 18, 23, -1, -1, kBtnScrollPageLeft, -1, shift_l_up, shift_l_down, nullptr, 0);

    blitBufferToBuffer(lbm_buf + 381 * lbmBufWidth + 622, 18, 23, lbmBufWidth, shift_r_up, 18);
    blitBufferToBuffer(lbm_buf + 228 * lbmBufWidth + 622, 18, 23, lbmBufWidth, shift_r_down, 18);
    buttonCreate(tool_win, _scr_size.right - 18, 1, 18, 23, -1, -1, kBtnScrollPageRight, -1, shift_r_up, shift_r_down, nullptr, 0);

    for (index = 0; index < max_art_buttons; index++) {
        int btn = buttonCreate(tool_win, index * (art_scale_width + 1) + 121, 1, art_scale_width, art_scale_height, index + max_art_buttons + 161, 58, 160 + index, -1, nullptr, nullptr, nullptr, 0);
        buttonSetRightMouseCallbacks(btn, 160 + index, -1, nullptr, nullptr);
    }

    // ELEVATION INC
    blitBufferToBuffer(lbm_buf + 431 * lbmBufWidth + 1, 18, 23, lbmBufWidth, einc_up, 18);
    blitBufferToBuffer(lbm_buf + 325 * lbmBufWidth + 1, 18, 23, lbmBufWidth, einc_down, 18);
    buttonCreate(tool_win, 1, 51, 18, 23, -1, -1, kBtnElevUp, -1, einc_up, einc_down, nullptr, 0);

    // ELEVATION DEC
    blitBufferToBuffer(lbm_buf + 456 * lbmBufWidth + 1, 18, 23, lbmBufWidth, edec_up, 18);
    blitBufferToBuffer(lbm_buf + 350 * lbmBufWidth + 1, 18, 23, lbmBufWidth, edec_down, 18);
    buttonCreate(tool_win, 1, 76, 18, 23, -1, -1, kBtnElevDown, -1, edec_up, edec_down, nullptr, 0);

    // ELEVATION
    for (index = 0; index < 4; index++) {
        blitBufferToBuffer(lbm_buf + 293 * lbmBufWidth + 19 * index, 19, 26, lbmBufWidth, e_num[index], 19);
    }

    view_mode = false;

    blitBufferToBuffer(lbm_buf + 169 * lbmBufWidth + 64, 58, 13, lbmBufWidth, to_iso_up, 58);
    blitBufferToBuffer(lbm_buf + 108 * lbmBufWidth + 64, 58, 13, lbmBufWidth, to_iso_down, 58);

    // ROOF
    blitBufferToBuffer(lbm_buf + 449 * lbmBufWidth + 64, 58, 13, lbmBufWidth, roof_up, 58);
    blitBufferToBuffer(lbm_buf + 343 * lbmBufWidth + 64, 58, 13, lbmBufWidth, roof_down, 58);
    roof_bid = buttonCreate(tool_win, 64, 69, 58, 13, -1, -1, kBtnRoof, kBtnRoof, roof_up, roof_down, nullptr, BUTTON_FLAG_CHECKABLE | BUTTON_FLAG_CHECKED);

    if (tileRoofIsVisible()) {
        tile_toggle_roof(false);
    }

    // HEX
    blitBufferToBuffer(lbm_buf + 464 * lbmBufWidth + 64, 58, 13, lbmBufWidth, hex_up, 58);
    blitBufferToBuffer(lbm_buf + 358 * lbmBufWidth + 64, 58, 13, lbmBufWidth, hex_down, 58);
    hex_bid = buttonCreate(tool_win, 64, 84, 58, 13, -1, -1, kBtnHex, kBtnHex, hex_up, hex_down, nullptr, BUTTON_FLAG_CHECKABLE);

    // ITEM
    blitBufferToBuffer(lbm_buf + 434 * lbmBufWidth + 125, 66, 13, lbmBufWidth, obj_up, 66);
    blitBufferToBuffer(lbm_buf + 328 * lbmBufWidth + 125, 66, 13, lbmBufWidth, obj_down, 66);
    obj_bid = buttonCreate(tool_win, 125, 54, 66, 13, -1, -1, kBtnHideItem, kBtnHideItem, obj_up, obj_down, nullptr, BUTTON_FLAG_CHECKABLE);

    // CRIT
    blitBufferToBuffer(lbm_buf + 449 * lbmBufWidth + 125, 66, 13, lbmBufWidth, crit_up, 66);
    blitBufferToBuffer(lbm_buf + 343 * lbmBufWidth + 125, 66, 13, lbmBufWidth, crit_down, 66);
    crit_bid = buttonCreate(tool_win, 125, 69, 66, 13, -1, -1, kBtnHideCrit, kBtnHideCrit, crit_up, crit_down, nullptr, BUTTON_FLAG_CHECKABLE);

    // SCEN
    blitBufferToBuffer(lbm_buf + 464 * lbmBufWidth + 125, 66, 13, lbmBufWidth, scen_up, 66);
    blitBufferToBuffer(lbm_buf + 358 * lbmBufWidth + 125, 66, 13, lbmBufWidth, scen_down, 66);
    scen_bid = buttonCreate(tool_win, 125, 84, 66, 13, -1, -1, kBtnHideScen, kBtnHideScen, scen_up, scen_down, nullptr, BUTTON_FLAG_CHECKABLE);

    // WALL
    blitBufferToBuffer(lbm_buf + 434 * lbmBufWidth + 194, 53, 13, lbmBufWidth, wall_up, 53);
    blitBufferToBuffer(lbm_buf + 328 * lbmBufWidth + 194, 53, 13, lbmBufWidth, wall_down, 53);
    wall_bid = buttonCreate(tool_win, 194, 54, 53, 13, -1, -1, kBtnHideWall, kBtnHideWall, wall_up, wall_down, nullptr, BUTTON_FLAG_CHECKABLE);

    // TILE
    blitBufferToBuffer(lbm_buf + 449 * lbmBufWidth + 194, 53, 13, lbmBufWidth, tile_up, 53);
    blitBufferToBuffer(lbm_buf + 343 * lbmBufWidth + 194, 53, 13, lbmBufWidth, tile_down, 53);
    tile_bid = buttonCreate(tool_win, 194, 69, 53, 13, -1, -1, kBtnHideTile, kBtnHideTile, tile_up, tile_down, nullptr, BUTTON_FLAG_CHECKABLE);

    // MISC
    blitBufferToBuffer(lbm_buf + 464 * lbmBufWidth + 194, 53, 13, lbmBufWidth, misc_up, 53);
    blitBufferToBuffer(lbm_buf + 358 * lbmBufWidth + 194, 53, 13, lbmBufWidth, misc_down, 53);
    misc_bid = buttonCreate(tool_win, 194, 84, 53, 13, -1, -1, kBtnHideMisc, kBtnHideMisc, misc_up, misc_down, nullptr, BUTTON_FLAG_CHECKABLE);

    // HEIGHT INC
    blitBufferToBuffer(lbm_buf + 431 * lbmBufWidth + 251, 18, 23, lbmBufWidth, height_inc_up, 18);
    blitBufferToBuffer(lbm_buf + 325 * lbmBufWidth + 251, 18, 23, lbmBufWidth, height_inc_down, 18);
    buttonCreate(tool_win, 251, 51, 18, 23, -1, -1, kBtnRotateRight, -1, height_inc_up, height_inc_down, nullptr, 0);

    // HEIGHT DEC
    blitBufferToBuffer(lbm_buf + 456 * lbmBufWidth + 251, 18, 23, lbmBufWidth, height_dec_up, 18);
    blitBufferToBuffer(lbm_buf + 350 * lbmBufWidth + 251, 18, 23, lbmBufWidth, height_dec_down, 18);
    buttonCreate(tool_win, 251, 76, 18, 23, -1, -1, kBtnRotateLeft, -1, height_dec_up, height_dec_down, nullptr, 0);

    // ARROWS
    for (index = 0; index < ROTATION_COUNT; index++) {
        int x = rotate_arrows_x_offs[index] + 285;
        int y = rotate_arrows_y_offs[index] + 25;
        unsigned char bgColor = lbm_buf[27 * lbmBufWidth + 287];
        int k;

        blitBufferToBuffer(lbm_buf + y * lbmBufWidth + x, 10, 10, lbmBufWidth, rotate_arrows[1][index], 10);

        for (k = 0; k < 100; k++) {
            if (rotate_arrows[1][index][k] == bgColor) {
                rotate_arrows[1][index][k] = 0;
            }
        }

        blitBufferToBuffer(lbm_buf + y * lbmBufWidth + x - 52, 10, 10, lbmBufWidth, rotate_arrows[0][index], 10);

        for (k = 0; k < 100; k++) {
            if (rotate_arrows[0][index][k] == bgColor) {
                rotate_arrows[0][index][k] = 0;
            }
        }
    }

    // COPY
    blitBufferToBuffer(lbm_buf + 435 * lbmBufWidth + 325, 49, 19, lbmBufWidth, copy_up, 49);
    blitBufferToBuffer(lbm_buf + 329 * lbmBufWidth + 325, 49, 19, lbmBufWidth, copy_down, 49);
    copy_bid = buttonCreate(tool_win, 325, 55, 49, 19, -1, -1, kBtnCopy, -1, copy_up, copy_down, nullptr, 0);

    // PASTE
    blitBufferToBuffer(lbm_buf + 457 * lbmBufWidth + 325, 49, 19, lbmBufWidth, paste_up, 49);
    blitBufferToBuffer(lbm_buf + 351 * lbmBufWidth + 325, 49, 19, lbmBufWidth, paste_down, 49);
    paste_bid = buttonCreate(tool_win, 325, 77, 49, 19, -1, -1, kBtnPaste, -1, paste_up, paste_down, nullptr, 0);

    // EDIT
    blitBufferToBuffer(lbm_buf + 435 * lbmBufWidth + 378, 49, 19, lbmBufWidth, edit_up, 49);
    blitBufferToBuffer(lbm_buf + 329 * lbmBufWidth + 378, 49, 19, lbmBufWidth, edit_down, 49);
    edit_bid = buttonCreate(tool_win, 378, 55, 49, 19, -1, -1, kBtnEdit, -1, edit_up, edit_down, nullptr, 0);

    // DELETE
    blitBufferToBuffer(lbm_buf + 457 * lbmBufWidth + 378, 49, 19, lbmBufWidth, delete_up, 49);
    blitBufferToBuffer(lbm_buf + 351 * lbmBufWidth + 378, 49, 19, lbmBufWidth, delete_down, 49);
    delete_bid = buttonCreate(tool_win, 378, 77, 49, 19, -1, -1, kBtnDelete, -1, delete_up, delete_down, nullptr, 0);

    draw_mode = false;

    blitBufferToBuffer(lbm_buf + 169 * lbmBufWidth + 430, 45, 43, lbmBufWidth, to_edit_up, 45);
    blitBufferToBuffer(lbm_buf + 108 * lbmBufWidth + 430, 45, 43, lbmBufWidth, to_edit_down, 45);

    blitBufferToBuffer(lbm_buf + 169 * lbmBufWidth + 327, 45, 43, lbmBufWidth, copy_group_up, 45);
    blitBufferToBuffer(lbm_buf + 108 * lbmBufWidth + 327, 45, 43, lbmBufWidth, copy_group_down, 45);

    blitBufferToBuffer(lbm_buf + 169 * lbmBufWidth + 379, 45, 43, lbmBufWidth, erase_up, 45);
    blitBufferToBuffer(lbm_buf + 108 * lbmBufWidth + 379, 45, 43, lbmBufWidth, erase_down, 45);

    internal_free(lbm_buf);
    windowRefresh(tool_win);

    if (bookmarkInit() == -1) {
        debugPrint("\nbookmarkInit() Failed!");
    }

    if (categoryInit() == -1) {
        debugPrint("\ncategoryInit() Failed!");
    }

    tileScrollBlockingDisable();
    tileScrollLimitingDisable();
    init_mapper_protos();
    mapInit();
    target_init();
    mouseShowCursor();

    if (settings.mapper.rebuild_protos) {
        proto_build_all_texts();
        settings.mapper.rebuild_protos = false;
    }

    return 0;
}

// 0x48752C
void mapper_edit_exit()
{
    remove(mapBuildPath("TMP$MAP#.MAP"));
    remove(mapBuildPath("TMP$MAP#.CFG"));

    MapDirErase("MAPS\\", "SAV");

    if (can_modify_protos) {
        copy_proto_lists();

        // NOTE: There is a call to an ambiguous function at `0x4B9ACC`, likely
        // `proto_save`.
    }

    target_exit();
    mapExit();
    bookmarkExit();
    categoryExit();

    windowDestroy(tool_win);
    tool_buf = nullptr;

    windowDestroy(menu_bar);

    internal_free(art_shape);
    gameExit();
}

// 0x4875B4
int bookmarkInit()
{
    return 0;
}

// 0x4875B8
int bookmarkExit()
{
    if (bookmarkWin == -1) {
        return -1;
    }

    windowDestroy(bookmarkWin);
    bookmarkWin = -1;

    return 0;
}

// 0x4875E0
void bookmarkHide()
{
    if (bookmarkWin != -1) {
        windowHide(bookmarkWin);
    }
}

// 0x4875F8
void bookmarkUnHide()
{
    if (bookmarkWin != -1) {
        windowShow(bookmarkWin);
    }
}

void bookmarkChoose(int type, int* outOffset)
{
    if (bookmarkWin == -1) return;

    windowShow(bookmarkWin);
    // TODO: full interactive bookmark selection
    // For now just shows the bookmark window for visual reference.
}

// 0x4875B4
int categoryInit()
{
    return 0;
}

// 0x487700
int categoryExit()
{
    if (categoryWin == -1) {
        return -1;
    }

    windowDestroy(categoryWin);
    categoryWin = -1;

    return 0;
}

// 0x487728
int categoryHide()
{
    if (categoryWin == -1) {
        return -1;
    }

    windowHide(categoryWin);
    categoryIsHidden = true;

    return 0;
}

// 0x487768
int categoryToggleState()
{
    if (categoryIsHidden) {
        return categoryUnhide();
    } else {
        return categoryHide();
    }
}

// 0x487774
int categoryUnhide()
{
    if (categoryWin == -1) {
        return -1;
    }

    windowShow(categoryWin);
    categoryIsHidden = false;

    return 0;
}

// 0x487784
bool proto_user_is_librarian()
{
    if (!settings.mapper.librarian) {
        return false;
    }

    can_modify_protos = true;
    return true;
}

static void toolbarSetObjectType(int newType, int& currentType, int& scrollOffset, Object** hl)
{
    if (newType == currentType) return;

    toolbar_info[currentType].offset = scrollOffset;
    currentType = newType;
    scrollOffset = toolbar_info[currentType].offset;
    print_toolbar_name(currentType);
    tool_active = -1;
    update_art(currentType, scrollOffset);
    clear_toolname();
    mapper_destroy_highlight_obj(hl, nullptr);
    _screen_obj = nullptr;
}

static void elevationNumberRefresh()
{
    int pitch = rectGetWidth(&_scr_size);
    blitBufferToBuffer(e_num[gElevation], 19, 26, 19, tool_buf + 62 * pitch + 30, pitch);
    Rect numRect = { 30, 62, 50, 88 };
    windowRefreshRect(tool_win, &numRect);
}

// This is the main mapper UI loop: handles all input and redrawing.
// 0x4877D0
void edit_mapper()
{
    int currentType = OBJ_TYPE_TILE;
    int scrollOffset = 0;
    int selectedPid = -1;
    int markExitGridMode = 0;
    Object* hl_obj1 = nullptr;
    Object* hl_obj2 = nullptr;

    handle_new_map(&currentType, &scrollOffset);

    Object* dudeToRestore = gDude;

    _scr_game_exit();
    mapper_refresh_rotation();
    scriptsClearDudeScript();

    proto_user_is_librarian();

    bool playModeEnabled = settings.mapper.default_f8_as_game;

    if (!settings.mapper.startup_map.empty()) {
        char localName[16];
        const char* src = settings.mapper.startup_map.c_str();
        char* dst = localName;
        while (*src) {
            *dst++ = toupper(*src++);
        }
        *dst = '\0';

        if (strstr(localName, ".MAP") == nullptr) {
            strcat(localName, ".MAP");
        }

        map_scr_toggle_hexes();
        mapLoadByName(localName);
        map_scr_toggle_hexes();
    }

    // TODO: these calls aren't in original code, figure out how it worked without them
    _gmouse_enable();
    interfaceBarHide();
    while (true) {
        sharedFpsLimiter.mark();
        int keyCode = inputGetInput();

        // ----------------------------------------------------------------
        // In play mode: forward input to game engine, then loop.
        // Only F8 and Escape are NOT forwarded (they toggle modes).
        // ----------------------------------------------------------------
        bool routeToGame = map_entered && playModeEnabled
            && keyCode != kMenuBarActivation && keyCode != kMenuBarActivationAlt
            && keyCode != KEY_ESCAPE;

        if (routeToGame) {
            gameHandleKey(keyCode, false);

            if (_game_user_wants_to_quit != GAME_QUIT_REQUEST_NONE) {
                _game_user_wants_to_quit = GAME_QUIT_REQUEST_NONE;
                if (map_entered) {
                    // Simulate F8 press to exit play mode.
                    mapper_exit_play_mode(&currentType, &scrollOffset, dudeToRestore);
                }
            }

            scriptsHandleRequests();
            mapHandleTransition();
            continue;
        }

        // ----------------------------------------------------------------
        // Edit-mode input dispatcher — runs for ALL non-play-mode inputs
        // (F8, Escape, mouse clicks, keyboard, everything).
        // ----------------------------------------------------------------

        if (gameGetState() == GAME_STATE_5) {
            _gdialogSystemEnter();
        }

        bool mouseInMenuArea = _mouse_click_in(0, 0, _scr_size.right - _scr_size.left, 15);
        if (!map_entered && mouseInMenuArea) {
            windowShow(menu_bar);
        } else if (!mouseInMenuArea) {
            windowHide(menu_bar);
        }

        if (keyCode == -1) {
            renderPresent();
            sharedFpsLimiter.throttle();
            continue;
        }

        // ---- Mouse event on hex grid ----
        if (keyCode == -2) {
            int buttons = mouseGetEvent();

            if (buttons & MOUSE_EVENT_LEFT_BUTTON_REPEAT) {
                constexpr int kToolbarReservedHeight = 100;
                if (_mouse_click_in(0, 16,
                        windowGetWidth(gIsoWindow) - 1,
                        windowGetHeight(gIsoWindow) - 1 - kToolbarReservedHeight)) {
                    if (tool_active != -1) {
                        if (selectedPid != -1) {
                            if (PID_TYPE(selectedPid) == OBJ_TYPE_TILE) {
                                placeTile(selectedPid, gGameMouseBouncingCursor->fid);
                            } else {
                                placeObject(selectedPid, gGameMouseBouncingCursor->fid);
                            }
                        }
                    } else if (_screen_obj != nullptr) {
                        int newTile = gGameMouseBouncingCursor->tile;
                        Rect rect;
                        objectSetLocation(_screen_obj, newTile, gElevation, &rect);
                        tileWindowRefreshRect(&rect, gElevation);
                        if (hl_obj1 != nullptr) {
                            objectSetLocation(hl_obj1, newTile, gElevation, &rect);
                            tileWindowRefreshRect(&rect, gElevation);
                        }
                    }
                }

            } else if (buttons & MOUSE_EVENT_LEFT_BUTTON_DOWN) {
                if (_mouse_click_in(0, 16,
                        windowGetWidth(gIsoWindow) - 1,
                        windowGetHeight(gIsoWindow) - 1)) {
                    _edit_area = 2;

                    int tile = gGameMouseBouncingCursor->tile;

                    // Display tile number on toolbar (vanilla: x=7, y=27, maxWidth=260, color=35)
                    char tileNumStr[32];
                    snprintf(tileNumStr, sizeof(tileNumStr), "%d", tile);
                    windowDrawText(tool_win, tileNumStr, 42, 7, 27, 35);
                    int textH = fontGetLineHeight();
                    Rect tileNumRect = { 7, 27, 57, 27 + textH };
                    windowRefreshRect(tool_win, &tileNumRect);

                    // toolbar item selected: place object
                    if (tool_active != -1) {
                        if (selectedPid != -1) {
                            if (PID_TYPE(selectedPid) == OBJ_TYPE_TILE) {
                                placeTile(selectedPid, gGameMouseBouncingCursor->fid);
                            } else {
                                placeObject(selectedPid, gGameMouseBouncingCursor->fid);
                            }
                        }
                    } else {
                        // object_under_mouse_(currentType, 0, gElevation)
                        int objTypeFilter = (currentType == OBJ_TYPE_MISC) ? -1 : currentType;
                        Object* foundObj = gameMouseGetObjectUnderCursor(objTypeFilter, false, gElevation);
                        _screen_obj = foundObj;

                        if (foundObj != nullptr) {
                            // destroy old highlight only (pass &highlight, NULL → don't touch screen_obj)
                            if (hl_obj1 != nullptr) {
                                mapper_destroy_highlight_obj(&hl_obj1, nullptr);
                            }

                            // create new highlight
                            update_high_obj_name(_screen_obj);

                            int hfid = buildFid(OBJ_TYPE_INTERFACE, 1, 0, 0, 0);
                            Object* hlObj;
                            if (objectCreateWithFidPid(&hlObj, hfid, -1) != -1) {
                                hlObj->flags |= OBJECT_SHOOT_THRU | OBJECT_LIGHT_THRU | OBJECT_NO_SAVE;
                                _obj_toggle_flat(hlObj, nullptr);

                                Rect rect;
                                objectSetLocation(hlObj, _screen_obj->tile, gElevation, nullptr);
                                objectGetRect(hlObj, &rect);
                                tileWindowRefreshRect(&rect, gElevation);
                                hl_obj1 = hlObj;
                            }
                            tool_active = -1;
                        } else {
                            if (hl_obj1 != nullptr) {
                                // Had highlight → destroy it + clear screen_obj
                                mapper_destroy_highlight_obj(&hl_obj1, nullptr);
                                _screen_obj = nullptr;
                            }
                            tool_active = -1;
                        }
                    }
                }
            }

            if (buttons & MOUSE_EVENT_RIGHT_BUTTON_DOWN) {
                // clear toolbar selection
                if (tool_active != -1) {
                    tool_active = -1;
                    selectedPid = -1;
                    draw_mode = false;
                    toolbar_info[currentType].offset = scrollOffset;
                    mapper_destroy_highlight_obj(&hl_obj1, nullptr);
                    _screen_obj = nullptr;
                    gameMouseResetBouncingCursorFid();
                    Rect artRect = { 121, 1, _scr_size.right - 19, art_scale_height + 1 };
                    windowRefreshRect(tool_win, &artRect);
                }
            }

            renderPresent();
            sharedFpsLimiter.throttle();
            continue;
        }

        // Menu header delegation: if user clicked a menu header, read the
        // pulldown selection and map it to the real handler code.
        if (keyCode == kMenuHeaderFile) {
            int index = inputGetInput();
            if (index == -1) continue;
            keyCode = menu_val_0[index];
        } else if (keyCode == kMenuHeaderTools) {
            int index = inputGetInput();
            if (index == -1) continue;
            keyCode = menu_val_1[index];
        } else if (keyCode == kMenuHeaderScripts) {
            int index = inputGetInput();
            if (index == -1) continue;
            keyCode = menu_val_2[index];
        }

        // Toolbar art-slot left-click: select proto from toolbar
        {
            int leftBase = kArtButtonBase + max_art_buttons;
            if (keyCode >= leftBase && keyCode < leftBase + max_art_buttons) {

                _edit_area = 1;
                if (!map_entered) {
                    if (!settings.mapper.use_art_not_protos) {
                        int slotIndex = keyCode - leftBase;
                        update_toolname(&selectedPid, currentType, scrollOffset + slotIndex);
                    }
                }
                continue;
            }
        }

        // Toolbar art-slot right-click: select a proto for placement
        {
            int rightBase = 160;
            if (keyCode >= rightBase && keyCode <= rightBase + max_art_buttons) {

                if (map_entered) continue;

                if (!settings.mapper.use_art_not_protos) {
                    int slotIndex = keyCode - rightBase;
                    int pid = toolbar_proto(currentType, scrollOffset + slotIndex);
                    if (pid != -1) {
                        selectedPid = pid;
                        tool_active = slotIndex;
                        draw_mode = true;
                        _edit_area = 1;

                        // Set mouse cursor to proto's art FID
                        Proto* proto;
                        if (protoGetProto(pid, &proto) != -1) {
                            int artFid = proto->fid;
                            if (artExists(artFid)) {
                                gGameMouseBouncingCursor->fid = artFid;
                                Rect mouseRect;
                                objectSetRotation(gGameMouseBouncingCursor, rotation, &mouseRect);
                                tileWindowRefreshRect(&mouseRect, gElevation);
                            }
                        }

                        // Destroy any existing highlight
                        if (hl_obj1 != nullptr) {
                            mapper_destroy_highlight_obj(&hl_obj1, nullptr);
                            _screen_obj = nullptr;
                        }

                        update_toolname(&selectedPid, currentType, scrollOffset + slotIndex);

                        // Refresh toolbar art row
                        Rect artRect = { 121, 1, _scr_size.right - 19, art_scale_height + 1 };
                        windowRefreshRect(tool_win, &artRect);
                    }
                }
                continue;
            }
        }

        // ---- Command dispatch ----
        switch (keyCode) {

        // --- FILE menu ---
        case kBtnNew:
            map_toggle_block_obj_viewing(0);
            mapNewMap();
            handle_new_map(&currentType, &scrollOffset);
            break;
        case kBtnEraseMap:
            if (map_entered) {
                break;
            }
            if (win_yes_no("Erase this map?", 80, 80, 0x10104)) {
                bool wasBlockOn = map_toggle_block_obj_viewing_on();
                if (wasBlockOn) {
                    map_toggle_block_obj_viewing(0);
                }
                mapper_destroy_highlight_obj(&hl_obj1, &_screen_obj);
                mapNewMap();
                handle_new_map(&currentType, &scrollOffset);
                interfaceBarHide();
                if (wasBlockOn) {
                    map_toggle_block_obj_viewing(1);
                }
            }
            break;
        case kBtnOpen:
            if (map_entered) {
                win_timed_msg("This map has been Entered.  Can't Load.", _colorTable[31744]);
                break;
            }
            {
                mapper_destroy_highlight_obj(&hl_obj1, &_screen_obj);
                bool wasBlockOn = map_toggle_block_obj_viewing_on();
                if (wasBlockOn) {
                    map_toggle_block_obj_viewing(0);
                }
                map_scr_toggle_hexes();
                map_load_dialog();
                map_scr_toggle_hexes();
                handle_new_map(&currentType, &scrollOffset);
                map_scr_toggle_hexes();
                map_scr_toggle_hexes();
                interfaceBarHide();
                mapper_load_toolbar(currentType, &scrollOffset);
                if (wasBlockOn) {
                    map_toggle_block_obj_viewing(1);
                }
            }
            break;
        case kBtnSave: {
            if (map_entered) {
                win_timed_msg("This map has been Entered.  Can't Save.", _colorTable[31744]);
                break;
            }
            bool wasBlockOn = map_toggle_block_obj_viewing_on();
            if (wasBlockOn) map_toggle_block_obj_viewing(0);
            map_save_dialog();
            mapper_save_toolbar();
            if (wasBlockOn) map_toggle_block_obj_viewing(1);
            break;
        }
        case kBtnSaveAs: {
            if (map_entered) {
                win_timed_msg("This map has been Entered.  Can't Save.", _colorTable[31744]);
                break;
            }
            bool wasBlockOn = map_toggle_block_obj_viewing_on();
            if (wasBlockOn) map_toggle_block_obj_viewing(0);
            map_save_as();
            mapper_save_toolbar();
            if (wasBlockOn) map_toggle_block_obj_viewing(1);
            break;
        }
        case kBtnInfo:
            map_info_dialog();
            break;
        case kBtnLoadAllTexts:
            load_all_maps_text();
            break;

        // --- TOOLS menu ---
        case kBtnCreatePattern:
            create_spray_tool();
            break;
        case kBtnUsePattern:
            copy_spray_tile();
            break;
        case kBtnMoveMap:
            mapper_shift_map();
            break;
        case kBtnMoveMapElev:
            mapper_shift_map_elev();
            break;
        case kBtnCopyMapElev:
            mapper_copy_map_elev();
            break;
        case kBtnEditDude:
            characterEditorShow(gDude);
            break;
        case kBtnFlushCache:
            mapper_flush_cache();
            break;
        case kBtnAnimStepping:
            // TODO: toggle anim debug stepping
            break;
        case kBtnFixMapPids:
            // TODO: fix map objects to PID
            break;
        case kBtnBookmark:
            bookmarkChoose(currentType, &scrollOffset);
            break;
        case kBtnToggleBlockObjView:
            map_toggle_block_obj_viewing(-1);
            break;
        case kBtnClickToScroll:
            // TODO: toggle click-to-scroll mode
            break;
        case kBtnSetExitGridData: {
            int val;
            if (win_get_num_i(&val, -1, 1000, false, "Map #:", 80, 80) != -1) mapInfo.map = val;
            if (win_get_num_i(&val, -1, 20000, false, "Tile #:", 80, 80) != -1) mapInfo.tile = val;
            if (win_get_num_i(&val, -1, 4, false, "Elevation #:", 80, 80) != -1) mapInfo.elevation = val;
            if (win_get_num_i(&val, -1, 5, false, "Rotation #:", 80, 80) != -1) mapInfo.rotation = val;
            break;
        }
        case kBtnMarkExitGrids:
            markExitGridMode = 1;
            mapper_mark_exit_grid();
            break;
        case kBtnMarkAllExitGrids:
            mapper_mark_all_exit_grids();
            break;
        case kBtnClearMapLevel:
            map_clear_elevation();
            break;
        case kBtnCreateAllMapTexts:
            if (!map_entered) {
                load_all_maps_text();
            }
            break;
        case kBtnRebuildAllMaps:
            proto_build_all_texts();
            break;

        // --- SCRIPTS menu ---
        case kBtnListScripts:
            scr_debug_print_scripts();
            break;
        case kBtnSetStartHex:
            place_entrance_hex();
            break;
        case kBtnPlaceSpatial: {
            if (map_entered) {
                break;
            }
            int screenWidth = _scr_size.right - _scr_size.left;
            windowDrawText(tool_win, "Place Script", 0x104, screenWidth - 149, 70, _colorTable[31744]);
            redraw_toolname();

            int tile = pickHex();

            clear_toolname();

            if (tile != -1) {
                if (map_scr_add_spatial(tile, gElevation) == -1) {
                    win_timed_msg("Error creating spatial Script!", _colorTable[31744]);
                }
            }
            break;
        }
        case kBtnDeleteSpatial: {
            int x, y;
            mouseGetPosition(&x, &y);
            int tile = tileFromScreenXY(x, y);
            if (tile != -1) {
                map_scr_remove_spatial(tile, gElevation);
            }
            break;
        }
        case kBtnDeleteAllSpatialScripts:
            map_scr_remove_all_spatials();
            break;
        case kBtnCreateScript:
            // TODO: create a new script file
            break;
        case kBtnSetMapScript:
            map_set_script();
            break;
        case kBtnShowMapScript:
            map_show_script();
            break;

        // --- Toolbar scroll arrows ---
        case kBtnScrollLeft:
            if (scrollOffset > 0) {
                scrollOffset--;
                update_art(currentType, scrollOffset);
            }
            break;
        case kBtnScrollRight: {
            int limit = settings.mapper.use_art_not_protos ? kArtMaxDirect : proto_max_id(currentType);
            if (scrollOffset + max_art_buttons < limit) {
                scrollOffset++;
                update_art(currentType, scrollOffset);
            }
            break;
        }
        case kBtnScrollPageLeft:
            scrollOffset -= max_art_buttons;
            if (scrollOffset < 0) scrollOffset = 0;
            update_art(currentType, scrollOffset);
            break;
        case kBtnScrollPageRight: {
            int limit = settings.mapper.use_art_not_protos ? kArtMaxDirect : proto_max_id(currentType);
            scrollOffset += max_art_buttons;
            if (scrollOffset + max_art_buttons > limit) {
                scrollOffset = limit - max_art_buttons;
                if (scrollOffset < 0) scrollOffset = 0;
            }
            update_art(currentType, scrollOffset);
            break;
        }

        // --- Toolbar type switch (F1–F6) ---
        case kBtnTypeItem:
        case kBtnTypeCritter:
        case kBtnTypeScenery:
        case kBtnTypeWall:
        case kBtnTypeTile:
        case kBtnTypeMisc: {
            int newType = keyCode - kBtnTypeItem;
            toolbarSetObjectType(newType, currentType, scrollOffset, &hl_obj1);
            break;
        }

        // --- Elevation ---
        case kBtnElevUp:
            if (gElevation < ELEVATION_COUNT - 1) {
                mapSetElevation(gElevation + 1);
                tileWindowRefresh();
                elevationNumberRefresh();
            }
            break;
        case kBtnElevDown:
            if (gElevation > 0) {
                mapSetElevation(gElevation - 1);
                tileWindowRefresh();
                elevationNumberRefresh();
            }
            break;

        // --- Rotation (Height Inc/Dec keys rotate objects) ---
        case kBtnRotateRight:
        case kBtnRotateLeft: {
            Object* obj = map_entered ? gDude : (_screen_obj ? _screen_obj : gGameMouseBouncingCursor);
            if (obj != nullptr) {
                Rect rect;
                int newRot = (keyCode == kBtnRotateRight) ? (obj->rotation + 1) % 6 : (obj->rotation + 5) % 6;
                objectSetRotation(obj, newRot, &rect);
                tileWindowRefreshRect(&rect, gElevation);
                rotation = obj->rotation;
                mapper_refresh_rotation();
            }
            break;
        }

        // --- Type toggle buttons ---
        case kBtnHideItem:
        case kBtnHideCrit:
        case kBtnHideScen:
        case kBtnHideWall:
        case kBtnHideTile:
        case kBtnHideMisc:
            artToggleObjectTypeHidden(keyCode - kBtnHideItem);
            tileWindowRefresh();
            break;

        // --- Hex/Roof overlay toggles ---
        case kBtnHex:
            map_scr_toggle_hexes();
            break;
        case kBtnRoof:
            tile_toggle_roof(true);
            break;

        // --- Copy / Edit / Paste / Delete ---
        case kBtnCopy:
            if (!map_entered) {
                if (currentType == OBJ_TYPE_TILE) {
                    copyTile();
                } else {
                    copyObject();
                }
            }
            break;
        case kBtnEdit:
            // Lowercase 'e' — instance editor on selected hex-grid object
            if (!map_entered && _edit_area == 2 && _screen_obj != nullptr) {
                protoInstEdit(_screen_obj);
            }
            break;
        case kBtnProtoEditor:
            // Uppercase 'E' — proto editor on current toolbar selection
            if (!map_entered && !settings.mapper.use_art_not_protos && !draw_mode && tool_active != -1) {
                int pid = toolbar_proto(currentType, scrollOffset + tool_active);
                if (pid != -1) {
                    protoEdit(pid);
                }
            }
            break;
        case kBtnPaste:
            if (!map_entered) {
                if (currentType == OBJ_TYPE_TILE) {
                    copyTile();
                } else {
                    copyObject();
                }
            }
            break;
        case kBtnDelete:
            if (!map_entered) {
                if (_screen_obj != nullptr) {
                    mapper_destroy_highlight_obj(&hl_obj1, &_screen_obj);
                } else {
                    eraseObject();
                }
            }
            break;

        // --- 'p' — jump toolbar to object proto ---
        case kBtnPickProto:
            if (!map_entered) {
                if (currentType == OBJ_TYPE_TILE) {
                    int tileOffset;
                    if (mapperPickTile(&tileOffset) != -1) {
                        scrollOffset = tileOffset;
                        update_art(OBJ_TYPE_TILE, scrollOffset);
                    }
                } else if (_screen_obj != nullptr) {
                    int protoOffset;
                    if (mapperPickObject(_screen_obj, &protoOffset) != -1) {
                        int objType = PID_TYPE(_screen_obj->pid);
                        currentType = objType;
                        scrollOffset = protoOffset;
                        toolbarSetObjectType(currentType, currentType, scrollOffset, &hl_obj1);
                        print_toolbar_name(currentType);
                        update_art(currentType, scrollOffset);
                    }
                }
            }
            break;

        // --- Play mode toggle ---
        case kMenuBarActivation:
        case kMenuBarActivationAlt:
            if (map_entered) {
                mapper_exit_play_mode(&currentType, &scrollOffset, dudeToRestore);
            } else {
                mapper_enter_play_mode(&currentType, &scrollOffset, &hl_obj1, dudeToRestore);
            }
            break;

        // --- Quit ---
        case kBtnQuit:
            if (markExitGridMode == 1) {
                markExitGridMode = 0;
                win_timed_msg("Exiting mark exit-grids!", _colorTable[31744]);
                break;
            }
            if (win_yes_no("Are you sure you want to quit?", 80, 80, 0x10104)) {
                goto exitLoop;
            }
            break;

        // --- 'z' / 'Z' — Light adjustment ---
        case kBtnLightDistDec:
            if (map_entered) {
                int newDist = gDude->lightDistance - 1;
                if (newDist < 0) newDist = 8;
                Rect rect;
                objectSetLight(gDude, newDist, gDude->lightIntensity, &rect);
                tileWindowRefreshRect(&rect, gDude->elevation);
            }
            break;
        case kBtnLightIntensityDec:
            if (map_entered) {
                int newIntensity = gDude->lightIntensity - 0x1999;
                if (newIntensity < 0) newIntensity = 0x10000;
                Rect rect;
                objectSetLight(gDude, gDude->lightDistance, newIntensity, &rect);
                tileWindowRefreshRect(&rect, gDude->elevation);
            }
            break;

        // --- 'u' — Use/open object ---
        case kBtnUse:
            if (!map_entered && _screen_obj != nullptr) {
                objectOpen(_screen_obj);
            }
            break;

        // --- 'k' — Kill critter ---
        case kBtnKill:
            if (!map_entered && _screen_obj != nullptr && PID_TYPE(_screen_obj->pid) == OBJ_TYPE_CRITTER) {
                critterKill(_screen_obj, 0x3F, true);
            }
            break;

        // --- 'l' — Lock/unlock object ---
        case kBtnLock:
            if (!map_entered && _screen_obj != nullptr) {
                Object* obj = _screen_obj;
                if (obj->flags & OBJ_LOCKED) {
                    objectUnlock(obj);
                    win_timed_msg("Unlocked.", _colorTable[31744]);
                } else {
                    objectLock(obj);
                    win_timed_msg("Locked.", _colorTable[31744]);
                }
            }
            break;

        // --- ':' — Edit proto from toolbar slot ---
        case kBtnProtoNewEdit:
            if (!map_entered && !settings.mapper.use_art_not_protos && tool_active != -1) {
                int pid = toolbar_proto(currentType, scrollOffset + tool_active);
                if (pid == -1) {
                    proto_new(&pid, currentType);
                }
                protoEdit(pid);
            }
            break;

        // --- 't' — Pick toolbar type ---
        case kBtnPickToolbarType:
            if (!map_entered) {
                int newType = pickToolbar(0);
                if (newType != -1 && newType != currentType) {
                    toolbarSetObjectType(newType, currentType, scrollOffset, &hl_obj1);
                }
            }
            break;

        // --- Bookmark select (digit keys 0-9) ---
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            if (!map_entered) {
                scrollOffset = toolbar_info[currentType].bookmark[keyCode - '0'];
                update_art(currentType, scrollOffset);
            }
            break;

        // --- Clear scroll / reset to 0 ---
        case kBtnClearScroll:
            if (!map_entered && scrollOffset != 0) {
                scrollOffset = 0;
                tool_active = -1;
                update_art(currentType, 0);
            }
            break;

        // --- Jump to last proto ---
        case kBtnLastProto:
            if (!map_entered) {
                int limit = settings.mapper.use_art_not_protos ? kArtMaxDirect : proto_max_id(currentType);
                int lastScroll = limit - max_art_buttons;
                if (lastScroll < 0) lastScroll = 0;
                if (lastScroll != scrollOffset) {
                    scrollOffset = lastScroll;
                    tool_active = -1;
                    update_art(currentType, scrollOffset);
                }
            }
            break;

        // --- Goto dude elevation ---
        case kBtnGotoDudeElev:
            if (gDude != nullptr) {
                mapSetElevation(gDude->elevation);
                tileSetCenter(gDude->tile, TILE_SET_CENTER_FLAG_IGNORE_SCROLL_RESTRICTIONS);
                tileWindowRefresh();
                elevationNumberRefresh();
            }
            break;

        // --- Toggle interrupt walk ---
        case kBtnToggleInterruptWalk:
            // TODO: toggle interrupt_walk config via gameConfig
            break;

        // --- Set rotation to 0 ---
        case kBtnRotation0:
            if (!map_entered) {
                Object* targetObj = _screen_obj ? _screen_obj : gGameMouseBouncingCursor;
                if (targetObj != nullptr) {
                    rotation = 0;
                    Rect rect;
                    objectSetRotation(targetObj, 0, &rect);
                    tileWindowRefreshRect(&rect, targetObj->elevation);
                    mapper_refresh_rotation();
                }
            }
            break;

        // --- Set rotation to 3 ---
        case kBtnRotation3:
            if (!map_entered) {
                Object* targetObj = _screen_obj ? _screen_obj : gGameMouseBouncingCursor;
                if (targetObj != nullptr) {
                    rotation = 3;
                    Rect rect;
                    objectSetRotation(targetObj, 3, &rect);
                    tileWindowRefreshRect(&rect, targetObj->elevation);
                    mapper_refresh_rotation();
                }
            }
            break;

        // --- Destroy all scripts ---
        case kBtnDestroyAllScripts:
            if (map_entered) break;
            if (win_yes_no("Do you want to destroy all scripts!?", 80, 80, 0x10104)) {
                if (win_yes_no("ARE YOU SURE?", 80, 80, 0x10104)) {
                    // TODO: iterate objects, remove scripts, call scr_remove_all
                }
            }
            break;

        // --- Rebuild spray tools ---
        case kBtnRebuildSprayTools:
            if (map_entered) break;
            if (!can_modify_protos) break;
            if (win_yes_no("Do you REALLY want to rebuild spray tools?", 80, 80, 0x10104)) {
                // TODO: rebuild_spray_tools()
                win_timed_msg("Rebuild spray tools not yet implemented.", _colorTable[31744]);
            }
            break;

        // --- Rebuild proto list for current type ---
        case kBtnRebuildProtoList:
            if (map_entered) break;
            if (!can_modify_protos) break;
            if (win_yes_no("Do you REALLY want to rebuild this list?", 80, 80, 0x10104)) {
                // TODO: proto_remove_all + proto_build_all_type(currentType)
                win_timed_msg("Rebuild proto list not yet implemented.", _colorTable[31744]);
            }
            break;

        // --- Destroy space proto list and rebuild ---
        case kBtnDestroyProtoList:
            if (map_entered) break;
            if (!can_modify_protos) break;
            if (win_yes_no("Do you REALLY want to destroy the space proto list?", 80, 80, 0x10104)) {
                // TODO: proto_remove_all + proto_build_all_texts
                win_timed_msg("Destroy proto list not yet implemented.", _colorTable[31744]);
            }
            break;

        // --- Highlight object by proto ---
        case kBtnHighlightByProto:
            if (map_entered) {
                win_timed_msg("This map has been Entered.  Can't Highlight.", _colorTable[31744]);
                break;
            }
            // TODO: pick_object(currentType) → set highlight, scroll toolbar to matching proto
            break;

        // --- Anim debug step ---
        case kBtnAnimDebugStep:
            // TODO: anim_debug_can_do_step = 1
            break;

        // --- Light ambient adjust ---
        case kBtnLightAmbientDec:
            if (map_entered) {
                lightDecreaseAmbient(0x28F);
            }
            break;
        case kBtnLightAmbientInc:
            if (map_entered) {
                lightIncreaseAmbient(0x28F);
            }
            break;

        default:
            gameHandleKey(keyCode, false);
            break;
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

exitLoop:
    toolbar_info[currentType].offset = scrollOffset;
    mapper_save_toolbar();
}

// 0x48AFFC
void mapper_load_toolbar(int type, int* out_offset)
{
    char name[16];
    name[0] = '\0';
    strncpy(name, gMapHeader.name, sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';
    if (name[0] == '\0') return;

    char* dot = strrchr(name, '.');
    if (dot != nullptr) *dot = '\0';
    strcat(name, ".cfg");

    File* stream = fileOpen(mapBuildPath(name), "rb");
    if (stream != nullptr) {
        fileRead(toolbar_info, sizeof(ToolbarInfo), 6, stream);
        fileClose(stream);
    }

    if (out_offset != nullptr) {
        *out_offset = toolbar_info[type].offset;
        update_art(type, *out_offset);
    }
}

void mapper_save_toolbar()
{
    char name[16];
    name[0] = '\0';
    strncpy(name, gMapHeader.name, sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';
    if (name[0] == '\0') return;

    char* dot = strrchr(name, '.');
    if (dot != nullptr) *dot = '\0';
    strcat(name, ".cfg");

    File* stream = fileOpen(mapBuildPath(name), "wb");
    if (stream != nullptr) {
        fileWrite(toolbar_info, sizeof(ToolbarInfo), 6, stream);
        fileClose(stream);
    }
}

// 0x48B16C
void print_toolbar_name(int object_type)
{
    Rect rect;
    char name[80];

    rect.left = 0;
    rect.top = 0;
    rect.right = 99;
    rect.bottom = 22;
    bufferFill(tool_buf + 2 + 2 * (_scr_size.right - _scr_size.left) + 2,
        96,
        19,
        _scr_size.right - _scr_size.left + 1,
        _colorTable[21140]);

    sprintf(name, "%s", artGetObjectTypeName(object_type));
    name[0] = static_cast<char>(toupper(name[0]));
    windowDrawText(tool_win, name, 0, 7, 7, _colorTable[32747] | 0x2000000);
    windowRefreshRect(tool_win, &rect);
}

// 0x48B230
void redraw_toolname()
{
    Rect rect;

    rect.left = _scr_size.right - _scr_size.left - 149;
    rect.top = 60;
    rect.right = _scr_size.right - _scr_size.left + 1;
    rect.bottom = 95;
    windowRefreshRect(tool_win, &rect);
}

// 0x48B278
void clear_toolname()
{
    windowDrawText(tool_win, "", 120, _scr_size.right - _scr_size.left - 149, 60, 260);
    windowDrawText(tool_win, "", 120, _scr_size.right - _scr_size.left - 149, 70, 260);
    windowDrawText(tool_win, "", 120, _scr_size.right - _scr_size.left - 149, 80, 260);
    redraw_toolname();
}

// 0x48B328
void update_toolname(int* pid, int type, int id)
{
    Proto* proto;

    *pid = toolbar_proto(type, id);

    if (protoGetProto(*pid, &proto) == -1) {
        return;
    }

    windowDrawText(tool_win,
        protoGetName(proto->pid),
        120,
        _scr_size.right - _scr_size.left - 149,
        60,
        260);

    switch (PID_TYPE(proto->pid)) {
    case OBJ_TYPE_ITEM:
        windowDrawText(tool_win,
            gItemTypeNames[proto->item.type],
            120,
            _scr_size.right - _scr_size.left - 149,
            70,
            260);
        break;
    case OBJ_TYPE_CRITTER:
        windowDrawText(tool_win,
            "",
            120,
            _scr_size.right - _scr_size.left - 149,
            70,
            260);
        break;
    case OBJ_TYPE_WALL:
        windowDrawText(tool_win,
            proto_wall_light_str(proto->wall.flags),
            120,
            _scr_size.right - _scr_size.left - 149,
            70,
            260);
        break;
    case OBJ_TYPE_TILE:
        windowDrawText(tool_win,
            "",
            120,
            _scr_size.right - _scr_size.left - 149,
            70,
            260);
        break;
    case OBJ_TYPE_MISC:
        windowDrawText(tool_win,
            "",
            120,
            _scr_size.right - _scr_size.left - 149,
            70,
            260);
        break;
    default:
        windowDrawText(tool_win,
            "",
            120,
            _scr_size.right - _scr_size.left - 149,
            70,
            260);
        break;
    }

    windowDrawText(tool_win,
        "",
        120,
        _scr_size.right - _scr_size.left - 149,
        80,
        260);

    redraw_toolname();
}

// 0x48B5BC
void update_high_obj_name(Object* obj)
{
    Proto* proto;

    if (protoGetProto(obj->pid, &proto) != -1) {
        windowDrawText(tool_win, protoGetName(obj->pid), 120, _scr_size.right - _scr_size.left - 149, 60, 260);
        windowDrawText(tool_win, "", 120, _scr_size.right - _scr_size.left - 149, 70, 260);
        windowDrawText(tool_win, "", 120, _scr_size.right - _scr_size.left - 149, 80, 260);
        redraw_toolname();
    }
}

// 0x48B680
void mapper_destroy_highlight_obj(Object** a1, Object** a2)
{
    Rect rect;
    int elevation;

    if (a2 != nullptr && *a2 != nullptr) {
        elevation = (*a2)->elevation;
        reg_anim_clear(*a2);
        objectDestroy(*a2, &rect);
        tileWindowRefreshRect(&rect, elevation);
        *a2 = nullptr;
    }

    if (a1 != nullptr && *a1 != nullptr) {
        elevation = (*a1)->elevation;
        objectDestroy(*a1, &rect);
        tileWindowRefreshRect(&rect, elevation);
        *a1 = nullptr;
    }
}

// 0x48B6EC
void mapper_refresh_rotation()
{
    Rect rect;
    char string[2];
    int index;

    constexpr int kRotationRectTop = 50;
    constexpr int kRotationTextY = 71;
    constexpr int kRotationArrowBaseY = 49;

    rect.left = 270;
    rect.top = kRotationRectTop;
    rect.right = 317;
    rect.bottom = kRotationRectTop + 47;

    sprintf(string, "%d", rotation);

    if (tool_buf != nullptr) {
        windowFill(tool_win,
            290,
            kRotationTextY,
            10,
            12,
            tool_buf[kRotationTextY * (_scr_size.right + 1) + 289]);
        windowDrawText(tool_win,
            string,
            10,
            292,
            kRotationTextY,
            0x2010104);

        for (index = 0; index < 6; index++) {
            int x = rotate_arrows_x_offs[index] + 269;
            int y = rotate_arrows_y_offs[index] + kRotationArrowBaseY;

            blitBufferToBufferTrans(rotate_arrows[index == rotation][index],
                10,
                10,
                10,
                tool_buf + y * (_scr_size.right + 1) + x,
                _scr_size.right + 1);
        }

        windowRefreshRect(tool_win, &rect);
    } else {
        debugPrint("Error: mapper_refresh_rotation: tool buffer invalid!");
    }
}

// 0x48B850
void update_art(int type, int offset)
{
    int limit = settings.mapper.use_art_not_protos ? kArtMaxDirect : proto_max_id(type);

    int screen_width = _scr_size.right - _scr_size.left + 1;
    int slot_stride = art_scale_width + 1;

    unsigned char* buf = windowGetBuffer(tool_win);
    // Row 1, col 121 is where art slots begin.
    unsigned char* slot_start = buf + screen_width + 121;

    // Clear all slot backgrounds.
    unsigned char* p = slot_start;
    for (int i = 0; i < max_art_buttons; i++, p += slot_stride) {
        bufferFill(p, art_scale_width, art_scale_height, screen_width, 106);
    }

    // Render thumbnails for visible slots.
    p = slot_start;
    for (int i = offset; i < offset + max_art_buttons && i < limit; i++, p += slot_stride) {
        int fid;
        if (settings.mapper.use_art_not_protos) {
            fid = buildFid(type, i, 0, 0, 0);
        } else {
            Proto* proto;
            int pid = toolbar_proto(type, i);
            if (protoGetProto(pid, &proto) == -1) continue;
            fid = proto->fid;
        }
        artRender(fid, p, art_scale_width, art_scale_height, screen_width);
    }

    // Draw selection box around the active slot.
    if (tool_active != -1) {
        int left = 121 + slot_stride * tool_active;
        int right = left + art_scale_width - 1;
        unsigned char* raw_buf = windowGetBuffer(tool_win);
        bufferDrawRect(raw_buf, screen_width, left, 1, right, art_scale_height, toolbar_selection_color);
    }

    // Refresh the art display area.
    Rect art_rect = { 121, 1, _scr_size.right - 19, art_scale_height + 1 };
    windowRefreshRect(tool_win, &art_rect);

    // Draw the current scroll offset as a count indicator.
    char text[32];
    sprintf(text, "(%d)", offset);
    windowDrawText(tool_win, text, 40, 52, 27, 260);

    // Refresh the count text area.
    int text_h = fontGetLineHeight();
    Rect text_rect = { 52, 27, 82, 27 + text_h };
    windowRefreshRect(tool_win, &text_rect);
}

// mapper_pick_object
static int mapperPickObject(Object* obj, int* outOffset)
{
    if (obj == nullptr) {
        return -1;
    }

    int type = PID_TYPE(obj->pid);
    int maxId = proto_max_id(type);
    constexpr int kScrollOffset = 10;

    for (int idx = 0; idx < maxId; idx++) {
        int pid = (type << 24) | idx;
        Proto* proto;
        if (protoGetProto(pid, &proto) == -1) {
            return -1;
        }
        if (proto->pid == obj->pid) {
            int clampedIdx = idx;
            if (idx > maxId - kScrollOffset) {
                clampedIdx = maxId - kScrollOffset;
            }
            *outOffset = clampedIdx;
            return 0;
        }
    }
    return 0;
}

// mapper_pick_tile
static int mapperPickTile(int* outOffset)
{
    int x, y;
    mouseGetPosition(&x, &y);
    int tileNum = tileFromScreenXY(x, y);

    int maxId = proto_max_id(OBJ_TYPE_TILE);
    constexpr int kScrollOffset = 10;

    if (tileNum == -1) {
        *outOffset = 0;
        return 0;
    }

    int packedTile = _square[gElevation]->field_0[tileNum];
    int tileFid;
    if (tileRoofIsVisible()) {
        tileFid = (packedTile >> 16) & 0xFFF;
    } else {
        tileFid = packedTile & 0xFFF;
    }
    int artFid = buildFid(OBJ_TYPE_TILE, tileFid, 0, 0, 0);

    for (int idx = 0; idx < maxId; idx++) {
        int pid = (OBJ_TYPE_TILE << 24) | idx;
        Proto* proto;
        if (protoGetProto(pid, &proto) == -1) {
            return 0;
        }
        if (proto->fid == artFid) {
            int clampedIdx = idx;
            if (idx > maxId - kScrollOffset) {
                clampedIdx = maxId - kScrollOffset;
            }
            *outOffset = clampedIdx;
            return 0;
        }
    }
    return 0;
}

// 0x48C524
void handle_new_map(int* a1, int* a2)
{
    Rect rect;

    rect.left = 30;
    rect.top = 62;
    rect.right = 50;
    rect.bottom = 88;
    blitBufferToBuffer(e_num[gElevation],
        19,
        26,
        19,
        tool_buf + rect.top * rectGetWidth(&_scr_size) + rect.left,
        rectGetWidth(&_scr_size));
    windowRefreshRect(tool_win, &rect);

    if (*a1 < 0 || *a1 > 6) {
        *a1 = 4;
    }

    *a2 = 0;
    update_art(*a1, *a2);

    print_toolbar_name(OBJ_TYPE_TILE);

    map_entered = false;

    if (tileRoofIsVisible()) {
        tile_toggle_roof(true);
    }
}

// 0x48C604
int mapper_inven_unwield(Object* obj, int right_hand)
{
    Object* item;
    int fid;

    reg_anim_begin(ANIMATION_REQUEST_RESERVED);

    if (right_hand) {
        item = critterGetItem2(obj);
    } else {
        item = critterGetItem1(obj);
    }

    if (item != nullptr) {
        item->flags &= ~OBJECT_IN_ANY_HAND;
    }

    animationRegisterAnimate(obj, ANIM_PUT_AWAY, 0);

    fid = buildFid(OBJ_TYPE_CRITTER, obj->fid & 0xFFF, 0, 0, FID_ROTATION(obj->fid));
    animationRegisterSetFid(obj, fid, 0);

    return reg_anim_end();
}

// 0x48C678
int mapper_mark_exit_grid()
{
    int y;
    int x;
    int tile;
    Object* obj;

    for (y = -2000; y != 2000; y += 200) {
        for (x = -10; x < 10; x++) {
            tile = gGameMouseBouncingCursor->tile + y + x;

            obj = objectFindFirstAtElevation(gElevation);
            while (obj != nullptr) {
                if (isExitGridPid(obj->pid)) {
                    obj->data.misc.map = mapInfo.map;
                    obj->data.misc.tile = mapInfo.tile;
                    obj->data.misc.elevation = mapInfo.elevation;
                    obj->data.misc.rotation = mapInfo.rotation;
                }
                obj = objectFindNextAtElevation();
            }
        }
    }

    return 0;
}

static void mapper_enter_play_mode(int* pCurrentType, int* pScrollOffset, Object** pHlObj1, Object* dudeToRestore)
{
    windowHide(menu_bar);

    gmouse_set_mapper_mode(0);

    mapper_destroy_highlight_obj(pHlObj1, nullptr);

    _screen_obj = nullptr;

    // TODO: bookmarkWin hide

    // TODO: gameLoadGlobalVars();

    windowHide(tool_win);

    // TODO: save map name for restoration on exit

    remove(mapBuildPath(tmp_map_name));
    remove(mapBuildPath("TMP$MAP#.MAP"));
    remove(mapBuildPath("TMP$MAP#.CFG"));
    MapDirErase("MAPS\\", "SAV");

    map_toggle_block_obj_viewing(0);

    // TODO: mapSaveAs(tmp_map_name);

    int centerTile = (gDude != nullptr) ? gDude->tile : 0;
    objectSetLocation(gDude, centerTile, gElevation, nullptr);

    objectSetRotation(gDude, 0, nullptr);

    // TODO: objectShow(gDude);

    // TODO: proto_dude_reset("premade\\blank.gcd")

    // TODO: set dude FID from art_vault_guy_num

    _scr_game_init();

    interfaceBarShow();

    map_entered = true;

    gameMouseResetBouncingCursorFid();
    gameMouseObjectsShow();
    gameMouseSetCursor(0);

    // TODO: _draw_mode = 0

    lightSetAmbientIntensity(0x10000, true);

    tileScrollBlockingEnable();
    tileScrollLimitingEnable();

    bool runAsGame = false;
    configGetBool(&gGameConfig, "mapper", "run_mapper_as_game", &runAsGame);
    if (runAsGame) {
        // TODO: scriptExecProc(gMapSid, 0xF);
        // TODO: _scr_load_all_scripts();
    }

    // TODO: scriptExecMapEnterScripts() / scriptExecMapUpdateScripts()

    tileSetCenter(gDude->tile, 1);

    // TODO: mapEnableBkProcesses()

    // TODO: wmMapMusicStart()

    tileWindowRefresh();
}

static void mapper_exit_play_mode(int* pCurrentType, int* pScrollOffset, Object* dudeToRestore)
{
    gmouse_set_mapper_mode(1);

    gDude = dudeToRestore;

    _scr_game_exit();

    _partyMemberClear();

    scriptsClearDudeScript();

    // TODO: mapLoadByName(tmp_map_name);

    remove(mapBuildPath(tmp_map_name));
    remove(mapBuildPath("TMP$MAP#.MAP"));
    remove(mapBuildPath("TMP$MAP#.CFG"));
    MapDirErase("MAPS\\", "SAV");

    // TODO: restore saved map name -> gMapHeader.name

    handle_new_map(pCurrentType, pScrollOffset);

    objectHide(gDude, nullptr);

    map_entered = false;

    map_scr_toggle_hexes();
    map_scr_toggle_hexes();

    if (tileRoofIsVisible()) {
        tile_toggle_roof(true);
    }

    interfaceBarHide();

    gameMoviesReset();

    windowShow(tool_win);

    lightSetAmbientIntensity(0x10000, true);

    tileScrollBlockingDisable();
    tileScrollLimitingDisable();

    // TODO: gmouse click-to-scroll restore

    // TODO: gmouse_3d_set_mode(0)

    backgroundSoundDelete();

    // TODO: bookmarkWin show
    tileWindowRefresh();
}

static void mapper_mark_all_exit_grids()
{
    Object* obj;

    obj = objectFindFirstAtElevation(gElevation);
    while (obj != nullptr) {
        if (isExitGridPid(obj->pid)) {
            obj->data.misc.map = mapInfo.map;
            obj->data.misc.tile = mapInfo.tile;
            obj->data.misc.elevation = mapInfo.elevation;
            obj->data.misc.rotation = mapInfo.rotation;
        }
        obj = objectFindNextAtElevation();
    }
}

} // namespace fallout
