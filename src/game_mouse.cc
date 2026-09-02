#include "game_mouse.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#if __APPLE__
#include <TargetConditionals.h>
#endif

#include <algorithm>

#include "actions.h"
#include "animation.h"
#include "art.h"
#include "color.h"
#include "combat.h"
#include "content_config.h"
#include "critter.h"
#include "dinput.h"
#include "draw.h"
#include "game.h"
#include "game_sound.h"
#include "input.h"
#include "interface.h"
#include "item.h"
#include "kb.h"
#include "map_edge.h"
#include "mouse.h"
#include "object.h"
#include "party_member.h"
#include "proto.h"
#include "proto_instance.h"
#include "settings.h"
#include "skill.h"
#include "skilldex.h"
#include "svga.h"
#include "text_font.h"
#include "tile.h"
#include "window_manager.h"

namespace fallout {

typedef enum ScrollableDirections {
    SCROLLABLE_W = 0x01,
    SCROLLABLE_E = 0x02,
    SCROLLABLE_N = 0x04,
    SCROLLABLE_S = 0x08,
} ScrollableDirections;

static constexpr int REFRESH_BOUNCING_CURSOR = 0x01;
static constexpr int REFRESH_HEX_CURSOR = 0x02;
static constexpr int REFRESH_BOTH_CURSORS = REFRESH_BOUNCING_CURSOR | REFRESH_HEX_CURSOR;

// 0x518BF8 gmouse_initialized
static bool gGameMouseInitialized = false;

// 0x518BFC gmouse_enabled
static bool _gmouse_enabled = false;

// 0x518C00 gmouse_mapper_mode
static int _gmouse_mapper_mode = 0;

// 0x518C04 gmouse_click_to_scroll
static bool _gmouse_click_to_scroll = false;

// 0x518C08 gmouse_scrolling_enabled
static bool _gmouse_scrolling_enabled = true;

// 0x518C0C gmouse_current_cursor
static int gGameMouseCursor = MOUSE_CURSOR_NONE;

// 0x518C10 gmouse_current_cursor_key
static CacheEntry* gGameMouseCursorFrmHandle = INVALID_CACHE_ENTRY;

// 0x518C14 gmouse_cursor_nums
static constexpr FrmId kGameMouseCursorFrmIds[MOUSE_CURSOR_TYPE_COUNT] = {
    FrmId(InterfaceFrameId::MouseCursorBlank),
    FrmId(InterfaceFrameId::MouseCursorStandardArrow),
    FrmId(InterfaceFrameId::MouseCursorSmallUpArrow),
    FrmId(InterfaceFrameId::MouseCursorSmallDownArrow),
    FrmId(InterfaceFrameId::MouseCursorScrollNorthWest),
    FrmId(InterfaceFrameId::MouseCursorScrollNorth),
    FrmId(InterfaceFrameId::MouseCursorScrollNorthEast),
    FrmId(InterfaceFrameId::MouseCursorScrollEast),
    FrmId(InterfaceFrameId::MouseCursorScrollSouthEast),
    FrmId(InterfaceFrameId::MouseCursorScrollSouth),
    FrmId(InterfaceFrameId::MouseCursorScrollSouthWest),
    FrmId(InterfaceFrameId::MouseCursorScrollWest),
    FrmId(InterfaceFrameId::MouseCursorInvalidScrollNorthWest),
    FrmId(InterfaceFrameId::MouseCursorInvalidScrollNorth),
    FrmId(InterfaceFrameId::MouseCursorInvalidScrollNorthEast),
    FrmId(InterfaceFrameId::MouseCursorInvalidScrollEast),
    FrmId(InterfaceFrameId::MouseCursorInvalidScrollSouthEast),
    FrmId(InterfaceFrameId::MouseCursorInvalidScrollSouth),
    FrmId(InterfaceFrameId::MouseCursorInvalidScrollSouthWest),
    FrmId(InterfaceFrameId::MouseCursorInvalidScrollWest),
    FrmId(InterfaceFrameId::MouseCursorCrosshair),
    FrmId(InterfaceFrameId::MouseCursorPlus),
    FrmId(InterfaceFrameId::MouseCursorDestroy),
    FrmId(InterfaceFrameId::MouseCursorUseCrosshair),
    FrmId(InterfaceFrameId::MouseCursorSingleFrameWatch),
    FrmId(InterfaceFrameId::MouseCursorWaitPlanet),
    FrmId(InterfaceFrameId::MouseCursorWaitWatch),
};

// 0x518C80 gmouse_3d_initialized
static bool gGameMouseObjectsInitialized = false;

// 0x518C84 gmouse_3d_hover_test
static bool _gmouse_3d_hover_test = false;

// 0x518C88 gmouse_3d_last_move_time
static unsigned int _gmouse_3d_last_move_time = 0;

// actmenu.frm
// 0x518C8C gmouse_3d_menu_frame
static Art* gGameMouseActionMenuFrm = nullptr;

// 0x518C90 gmouse_3d_menu_frame_key
static CacheEntry* gGameMouseActionMenuFrmHandle = INVALID_CACHE_ENTRY;

// 0x518C94 gmouse_3d_menu_frame_width
static int gGameMouseActionMenuFrmWidth = 0;

// 0x518C98 gmouse_3d_menu_frame_height
static int gGameMouseActionMenuFrmHeight = 0;

// 0x518C9C gmouse_3d_menu_frame_size
static int gGameMouseActionMenuFrmDataSize = 0;

// 0x518CA0 gmouse_3d_menu_frame_hot_x
static int _gmouse_3d_menu_frame_hot_x = 0;

// 0x518CA4 gmouse_3d_menu_frame_hot_y
static int _gmouse_3d_menu_frame_hot_y = 0;

// 0x518CA8 gmouse_3d_menu_frame_data
static unsigned char* gGameMouseActionMenuFrmData = nullptr;

// actpick.frm
// 0x518CAC gmouse_3d_pick_frame
static Art* gGameMouseActionPickFrm = nullptr;

// 0x518CB0 gmouse_3d_pick_frame_key
static CacheEntry* gGameMouseActionPickFrmHandle = INVALID_CACHE_ENTRY;

// 0x518CB4 gmouse_3d_pick_frame_width
static int gGameMouseActionPickFrmWidth = 0;

// 0x518CB8 gmouse_3d_pick_frame_height
static int gGameMouseActionPickFrmHeight = 0;

// 0x518CBC gmouse_3d_pick_frame_size
static int gGameMouseActionPickFrmDataSize = 0;

// 0x518CC0 gmouse_3d_pick_frame_hot_x
static int _gmouse_3d_pick_frame_hot_x = 0;

// 0x518CC4 gmouse_3d_pick_frame_hot_y
static int _gmouse_3d_pick_frame_hot_y = 0;

// 0x518CC8 gmouse_3d_pick_frame_data
static unsigned char* gGameMouseActionPickFrmData = nullptr;

// acttohit.frm
// 0x518CCC gmouse_3d_to_hit_frame
static Art* gGameMouseActionHitFrm = nullptr;

// 0x518CD0 gmouse_3d_to_hit_frame_key
static CacheEntry* gGameMouseActionHitFrmHandle = INVALID_CACHE_ENTRY;

// 0x518CD4 gmouse_3d_to_hit_frame_width
static int gGameMouseActionHitFrmWidth = 0;

// 0x518CD8 gmouse_3d_to_hit_frame_height
static int gGameMouseActionHitFrmHeight = 0;

// 0x518CDC gmouse_3d_to_hit_frame_size
static int gGameMouseActionHitFrmDataSize = 0;

// 0x518CE0 gmouse_3d_to_hit_frame_data
static unsigned char* gGameMouseActionHitFrmData = nullptr;

// blank.frm
// 0x518CE4 gmouse_3d_hex_base_frame
static Art* gGameMouseBouncingCursorFrm = nullptr;

// 0x518CE8 gmouse_3d_hex_base_frame_key
static CacheEntry* gGameMouseBouncingCursorFrmHandle = INVALID_CACHE_ENTRY;

// 0x518CEC gmouse_3d_hex_base_frame_width
static int gGameMouseBouncingCursorFrmWidth = 0;

// 0x518CF0 gmouse_3d_hex_base_frame_height
static int gGameMouseBouncingCursorFrmHeight = 0;

// 0x518CF4 gmouse_3d_hex_base_frame_size
static int gGameMouseBouncingCursorFrmDataSize = 0;

// 0x518CF8 gmouse_3d_hex_base_frame_data
static unsigned char* gGameMouseBouncingCursorFrmData = nullptr;

// msef000.frm
// 0x518CFC gmouse_3d_hex_frame
static Art* gGameMouseHexCursorFrm = nullptr;

// 0x518D00 gmouse_3d_hex_frame_key
static CacheEntry* gGameMouseHexCursorFrmHandle = INVALID_CACHE_ENTRY;

// 0x518D04 gmouse_3d_hex_frame_width
static int gGameMouseHexCursorFrmWidth = 0;

// 0x518D08 gmouse_3d_hex_frame_height
static int gGameMouseHexCursorHeight = 0;

// 0x518D0C gmouse_3d_hex_frame_size
static int gGameMouseHexCursorDataSize = 0;

// 0x518D10 gmouse_3d_hex_frame_data
static unsigned char* gGameMouseHexCursorFrmData = nullptr;

// 0x518D14 gmouse_3d_menu_available_actions
static unsigned char gGameMouseActionMenuItemsLength = 0;

// 0x518D18 gmouse_3d_menu_actions_start
static unsigned char* _gmouse_3d_menu_actions_start = nullptr;

// 0x518D1C gmouse_3d_menu_current_action_index
static unsigned char gGameMouseActionMenuHighlightedItemIndex = 0;

// 0x518D1E gmouse_3d_action_nums
static constexpr int GAME_MOUSE_ACTION_MENU_ITEM_FRAME_COUNT = 2;
static constexpr int GAME_MOUSE_ACTION_MENU_ITEM_FRAME_HIGHLIGHTED = 0;
static constexpr int GAME_MOUSE_ACTION_MENU_ITEM_FRAME_NORMAL = 1;

static constexpr FrmId kGameMouseActionMenuItemFrmIds[GAME_MOUSE_ACTION_MENU_ITEM_COUNT][GAME_MOUSE_ACTION_MENU_ITEM_FRAME_COUNT] = {
    { FrmId(InterfaceFrameId::CancelHighlighted) , FrmId(InterfaceFrameId::Cancel) },
    { FrmId(InterfaceFrameId::DropHighlighted), FrmId(InterfaceFrameId::Drop) },
    { FrmId(InterfaceFrameId::InventoryHighlighted), FrmId(InterfaceFrameId::Inventory) },
    { FrmId(InterfaceFrameId::LookHighlighted), FrmId(InterfaceFrameId::Look) },
    { FrmId(InterfaceFrameId::RotateHighlighted), FrmId(InterfaceFrameId::Rotate) },
    { FrmId(InterfaceFrameId::TalkHighlighted), FrmId(InterfaceFrameId::Talk) },
    { FrmId(InterfaceFrameId::UseGetHighlighted), FrmId(InterfaceFrameId::UseGet) },
    { FrmId(InterfaceFrameId::UnloadHighlighted), FrmId(InterfaceFrameId::Unload) },
    { FrmId(InterfaceFrameId::SkillHighlighted), FrmId(InterfaceFrameId::Skill) },
    { FrmId(InterfaceFrameId::PushHighlighted), FrmId(InterfaceFrameId::Push) },
};

// 0x518D34 gmouse_3d_modes_enabled
static int _gmouse_3d_modes_enabled = 1;

// 0x518D38 gmouse_3d_current_mode
static int gGameMouseMode = GAME_MOUSE_MODE_MOVE;

// 0x518D3C gmouse_3d_mode_nums
static InterfaceFrameId gGameMouseModeFrmIds[GAME_MOUSE_MODE_COUNT] = {
    InterfaceFrameId::ActionMove,
    InterfaceFrameId::ActionArrow,
    InterfaceFrameId::ActionCrosshair,
    InterfaceFrameId::MouseCursorUseCrosshair,
    InterfaceFrameId::MouseCursorUseCrosshair,
    InterfaceFrameId::MouseCursorUseCrosshair,
    InterfaceFrameId::MouseCursorUseCrosshair,
    InterfaceFrameId::MouseCursorUseCrosshair,
    InterfaceFrameId::MouseCursorUseCrosshair,
    InterfaceFrameId::MouseCursorUseCrosshair,
    InterfaceFrameId::MouseCursorUseCrosshair,
};

// 0x518D68 gmouse_skill_table
static const Skill gGameMouseModeSkills[GAME_MOUSE_MODE_SKILL_COUNT] = {
    SKILL_FIRST_AID,
    SKILL_DOCTOR,
    SKILL_LOCKPICK,
    SKILL_STEAL,
    SKILL_TRAPS,
    SKILL_SCIENCE,
    SKILL_REPAIR,
};

// 0x518D84 gmouse_wait_cursor_frame
static int gGameMouseAnimatedCursorNextFrame = 0;

// 0x518D88 gmouse_wait_cursor_time
static unsigned int gGameMouseAnimatedCursorLastUpdateTimestamp = 0;

// 0x518D8C gmouse_bk_last_cursor
static int _gmouse_bk_last_cursor = -1;

// 0x518D90 gmouse_3d_item_highlight
static bool gGameMouseItemHighlightEnabled = true;

// 0x518D94 outlined_object
static Object* gGameMouseHighlightedItem = nullptr;

// 0x518D98 gmouse_clicked_on_edge
bool _gmouse_clicked_on_edge = false;

// 0x518D9C mouse_flat_tile
static int dword_518D9C = -1;

// 0x596C3C gmouse_3d_menu_frame_actions
static int gGameMouseActionMenuItems[GAME_MOUSE_ACTION_MENU_ITEM_COUNT];

// 0x596C64 gmouse_3d_last_mouse_x
static int gGameMouseLastX;

// 0x596C68 gmouse_3d_last_mouse_y
static int gGameMouseLastY;

// blank.frm
// 0x596C6C obj_mouse
Object* gGameMouseBouncingCursor;

// msef000.frm
// 0x596C70 obj_mouse_flat
Object* gGameMouseHexCursor;

// 0x596C74
static Object* gGameMousePointedObject;

static void _gmouse_3d_enable_modes();
static int gameMouseSetBouncingCursorFid(int fid);
static int gameMouseRenderAccuracy(const char* string, Color color);
static int gameMouseRenderActionPoints(const char* string, Color color);
static int gameMouseObjectsInit();
static int gameMouseObjectsReset();
static void gameMouseObjectsFree();
static int gameMouseActionMenuInit();
static void gameMouseActionMenuFree();
static int gmouse_3d_set_flat_fid(int fid, Rect* rect);
static int gameMouseUpdateHexCursorFid(Rect* rect);
static int _gmouse_3d_move_to(int x, int y, int elevation, Rect* rect);
static int gameMouseHandleScrolling(int x, int y, int cursor);
static int objectIsDoor(Object* object);
static bool gameMouseClickOnInterfaceBar();
static bool gameMouseLongPressUsesLootActionForCritter(Object* object);

static void customMouseModeFrmsInit();

static bool gameMouseLongPressUsesLootActionForCritter(Object* object)
{
    constexpr int kExpandedActionMenuFrmHeight = 302;

    return settings.qol.party_trade_from_menu
        && gGameMouseActionMenuFrmHeight >= kExpandedActionMenuFrmHeight // make sure we can accommodate 7 items in the action menu
        && object != nullptr
        && object != gDude
        && !isInCombat()
        && objectIsPartyMember(object)
        && _obj_action_can_talk_to(object);
}

// 0x44B2B0 gmouse_init
int gameMouseInit()
{
    if (gGameMouseInitialized) {
        return -1;
    }

    if (gameMouseObjectsInit() != 0) {
        return -1;
    }

    gGameMouseInitialized = true;
    _gmouse_enabled = true;

    gameMouseSetCursor(MOUSE_CURSOR_ARROW);

    // SFALL
    customMouseModeFrmsInit();

    return 0;
}

// 0x44B2E8 gmouse_reset
int gameMouseReset()
{
    if (!gGameMouseInitialized) {
        return -1;
    }

    // NOTE: Uninline.
    if (gameMouseObjectsReset() != 0) {
        return -1;
    }

    // NOTE: Uninline.
    _gmouse_enable();

    _gmouse_scrolling_enabled = true;
    gameMouseSetCursor(MOUSE_CURSOR_ARROW);
    gGameMouseAnimatedCursorNextFrame = 0;
    gGameMouseAnimatedCursorLastUpdateTimestamp = 0;
    _gmouse_clicked_on_edge = false;

    return 0;
}

// 0x44B3B8 gmouse_exit
void gameMouseExit()
{
    if (!gGameMouseInitialized) {
        return;
    }

    mouseHideCursor();

    mouseSetFrame(nullptr, 0, 0, 0, 0, 0, 0);

    // NOTE: Uninline.
    gameMouseObjectsFree();

    if (gGameMouseCursorFrmHandle != INVALID_CACHE_ENTRY) {
        artUnlock(gGameMouseCursorFrmHandle);
    }
    gGameMouseCursorFrmHandle = INVALID_CACHE_ENTRY;

    _gmouse_enabled = false;
    gGameMouseInitialized = false;
    gGameMouseCursor = -1;
}

// 0x44B454 gmouse_enable
void _gmouse_enable()
{
    if (!_gmouse_enabled) {
        gGameMouseCursor = -1;
        gameMouseSetCursor(MOUSE_CURSOR_NONE);
        _gmouse_scrolling_enabled = true;
        _gmouse_enabled = true;
        _gmouse_bk_last_cursor = -1;
    }
}

// 0x44B48C gmouse_disable
void _gmouse_disable(int allowScrolling)
{
    if (_gmouse_enabled) {
        gameMouseSetCursor(MOUSE_CURSOR_NONE);
        _gmouse_enabled = false;

        if (allowScrolling & 1) {
            _gmouse_scrolling_enabled = true;
        } else {
            _gmouse_scrolling_enabled = false;
        }
    }
}

// 0x44B4CC gmouse_enable_scrolling
void _gmouse_enable_scrolling()
{
    _gmouse_scrolling_enabled = true;
}

// 0x44B4D8 gmouse_disable_scrolling
void _gmouse_disable_scrolling()
{
    _gmouse_scrolling_enabled = false;
}

// NOTE: Inlined.
//
// 0x44B4E4 gmouse_scrolling_is_enabled
bool gmouse_scrolling_is_enabled()
{
    return _gmouse_scrolling_enabled;
}

// 0x44B504 gmouse_get_click_to_scroll
bool _gmouse_get_click_to_scroll()
{
    return _gmouse_click_to_scroll;
}

// gmouse_set_click_to_scroll
void _gmouse_set_click_to_scroll(bool value)
{
    if (value != _gmouse_click_to_scroll) {
        _gmouse_click_to_scroll = value;
        _gmouse_clicked_on_edge = false;
    }
}

// 0x44B54C gmouse_is_scrolling
int _gmouse_is_scrolling()
{
    int isScrolling = 0;

    if (_gmouse_scrolling_enabled) {
        int x;
        int y;
        mouseGetPosition(&x, &y);
        if (x == _scr_size.left || x == _scr_size.right || y == _scr_size.top || y == _scr_size.bottom) {
            switch (gGameMouseCursor) {
            case MOUSE_CURSOR_SCROLL_NW:
            case MOUSE_CURSOR_SCROLL_N:
            case MOUSE_CURSOR_SCROLL_NE:
            case MOUSE_CURSOR_SCROLL_E:
            case MOUSE_CURSOR_SCROLL_SE:
            case MOUSE_CURSOR_SCROLL_S:
            case MOUSE_CURSOR_SCROLL_SW:
            case MOUSE_CURSOR_SCROLL_W:
            case MOUSE_CURSOR_SCROLL_NW_INVALID:
            case MOUSE_CURSOR_SCROLL_N_INVALID:
            case MOUSE_CURSOR_SCROLL_NE_INVALID:
            case MOUSE_CURSOR_SCROLL_E_INVALID:
            case MOUSE_CURSOR_SCROLL_SE_INVALID:
            case MOUSE_CURSOR_SCROLL_S_INVALID:
            case MOUSE_CURSOR_SCROLL_SW_INVALID:
            case MOUSE_CURSOR_SCROLL_W_INVALID:
                isScrolling = 1;
                break;
            default:
                return isScrolling;
            }
        }
    }

    return isScrolling;
}

// 0x44B684 gmouse_bk_process
void gameMouseRefresh()
{
    if (!gGameMouseInitialized) {
        return;
    }

    int mouseX;
    int mouseY;

    if (gGameMouseCursor >= FIRST_GAME_MOUSE_ANIMATED_CURSOR) {
        _mouse_info();

        // NOTE: Uninline.
        if (gmouse_scrolling_is_enabled()) {
            mouseGetPosition(&mouseX, &mouseY);
            int oldMouseCursor = gGameMouseCursor;

            if (gameMouseHandleScrolling(mouseX, mouseY, gGameMouseCursor) == 0) {
                switch (oldMouseCursor) {
                case MOUSE_CURSOR_SCROLL_NW:
                case MOUSE_CURSOR_SCROLL_N:
                case MOUSE_CURSOR_SCROLL_NE:
                case MOUSE_CURSOR_SCROLL_E:
                case MOUSE_CURSOR_SCROLL_SE:
                case MOUSE_CURSOR_SCROLL_S:
                case MOUSE_CURSOR_SCROLL_SW:
                case MOUSE_CURSOR_SCROLL_W:
                case MOUSE_CURSOR_SCROLL_NW_INVALID:
                case MOUSE_CURSOR_SCROLL_N_INVALID:
                case MOUSE_CURSOR_SCROLL_NE_INVALID:
                case MOUSE_CURSOR_SCROLL_E_INVALID:
                case MOUSE_CURSOR_SCROLL_SE_INVALID:
                case MOUSE_CURSOR_SCROLL_S_INVALID:
                case MOUSE_CURSOR_SCROLL_SW_INVALID:
                case MOUSE_CURSOR_SCROLL_W_INVALID:
                    break;
                default:
                    _gmouse_bk_last_cursor = oldMouseCursor;
                    break;
                }
                return;
            }

            if (_gmouse_bk_last_cursor != -1) {
                gameMouseSetCursor(_gmouse_bk_last_cursor);
                _gmouse_bk_last_cursor = -1;
                return;
            }
        }

        gameMouseSetCursor(gGameMouseCursor);
        return;
    }

    if (!_gmouse_enabled) {
        // NOTE: Uninline.
        if (gmouse_scrolling_is_enabled()) {
            mouseGetPosition(&mouseX, &mouseY);
            int oldMouseCursor = gGameMouseCursor;

            if (gameMouseHandleScrolling(mouseX, mouseY, gGameMouseCursor) == 0) {
                switch (oldMouseCursor) {
                case MOUSE_CURSOR_SCROLL_NW:
                case MOUSE_CURSOR_SCROLL_N:
                case MOUSE_CURSOR_SCROLL_NE:
                case MOUSE_CURSOR_SCROLL_E:
                case MOUSE_CURSOR_SCROLL_SE:
                case MOUSE_CURSOR_SCROLL_S:
                case MOUSE_CURSOR_SCROLL_SW:
                case MOUSE_CURSOR_SCROLL_W:
                case MOUSE_CURSOR_SCROLL_NW_INVALID:
                case MOUSE_CURSOR_SCROLL_N_INVALID:
                case MOUSE_CURSOR_SCROLL_NE_INVALID:
                case MOUSE_CURSOR_SCROLL_E_INVALID:
                case MOUSE_CURSOR_SCROLL_SE_INVALID:
                case MOUSE_CURSOR_SCROLL_S_INVALID:
                case MOUSE_CURSOR_SCROLL_SW_INVALID:
                case MOUSE_CURSOR_SCROLL_W_INVALID:
                    break;
                default:
                    _gmouse_bk_last_cursor = oldMouseCursor;
                    break;
                }

                return;
            }

            if (_gmouse_bk_last_cursor != -1) {
                gameMouseSetCursor(_gmouse_bk_last_cursor);
                _gmouse_bk_last_cursor = -1;
            }
        }

        return;
    }

    mouseGetPosition(&mouseX, &mouseY);

    int oldMouseCursor = gGameMouseCursor;
    if (gameMouseHandleScrolling(mouseX, mouseY, MOUSE_CURSOR_NONE) == 0) {
        switch (oldMouseCursor) {
        case MOUSE_CURSOR_SCROLL_NW:
        case MOUSE_CURSOR_SCROLL_N:
        case MOUSE_CURSOR_SCROLL_NE:
        case MOUSE_CURSOR_SCROLL_E:
        case MOUSE_CURSOR_SCROLL_SE:
        case MOUSE_CURSOR_SCROLL_S:
        case MOUSE_CURSOR_SCROLL_SW:
        case MOUSE_CURSOR_SCROLL_W:
        case MOUSE_CURSOR_SCROLL_NW_INVALID:
        case MOUSE_CURSOR_SCROLL_N_INVALID:
        case MOUSE_CURSOR_SCROLL_NE_INVALID:
        case MOUSE_CURSOR_SCROLL_E_INVALID:
        case MOUSE_CURSOR_SCROLL_SE_INVALID:
        case MOUSE_CURSOR_SCROLL_S_INVALID:
        case MOUSE_CURSOR_SCROLL_SW_INVALID:
        case MOUSE_CURSOR_SCROLL_W_INVALID:
            break;
        default:
            _gmouse_bk_last_cursor = oldMouseCursor;
            break;
        }
        return;
    }

    if (_gmouse_bk_last_cursor != -1) {
        gameMouseSetCursor(_gmouse_bk_last_cursor);
        _gmouse_bk_last_cursor = -1;
    }

    if (windowGetAtPoint(mouseX, mouseY) != gIsoWindow) {
        if (gGameMouseCursor == MOUSE_CURSOR_NONE) {
            gameMouseObjectsHide();
            gameMouseSetCursor(MOUSE_CURSOR_ARROW);

            if (gGameMouseMode >= 2 && !isInCombat()) {
                gameMouseSetMode(GAME_MOUSE_MODE_MOVE);
            }
        }
        return;
    }

    // NOTE: Strange set of conditions and jumps. Not sure about this one.
    switch (gGameMouseCursor) {
    case MOUSE_CURSOR_NONE:
    case MOUSE_CURSOR_ARROW:
    case MOUSE_CURSOR_SMALL_ARROW_UP:
    case MOUSE_CURSOR_SMALL_ARROW_DOWN:
    case MOUSE_CURSOR_CROSSHAIR:
    case MOUSE_CURSOR_USE_CROSSHAIR:
        if (gGameMouseCursor != MOUSE_CURSOR_NONE) {
            gameMouseSetCursor(MOUSE_CURSOR_NONE);
        }

        if ((gGameMouseHexCursor->flags & OBJECT_HIDDEN) != OBJECT_NONE) {
            gameMouseObjectsShow();
        }

        break;
    }

    Rect r1;
    if (_gmouse_3d_move_to(mouseX, mouseY, gElevation, &r1) == 0) {
        tileWindowRefreshRect(&r1, gElevation);
    }

    if ((gGameMouseHexCursor->flags & OBJECT_HIDDEN) != OBJECT_NONE || _gmouse_mapper_mode != 0) {
        return;
    }

    unsigned int v3 = _get_bk_time();
    if (mouseX == gGameMouseLastX && mouseY == gGameMouseLastY) {
        if (_gmouse_3d_hover_test || getTicksBetween(v3, _gmouse_3d_last_move_time) < 250) {
            return;
        }

        if (gGameMouseMode != GAME_MOUSE_MODE_MOVE) {
            if (gGameMouseMode == GAME_MOUSE_MODE_ARROW) {
                _gmouse_3d_last_move_time = v3;
                _gmouse_3d_hover_test = true;

                Object* pointedObject = gameMouseGetObjectUnderCursor(OBJ_TYPE_INVALID, true, gElevation);
                if (pointedObject != nullptr) {
                    int primaryAction = -1;
                    ObjectType objectType = objectTypeFromFid(pointedObject->fid);
                    switch (objectType) {
                    case OBJ_TYPE_SCENERY:
                    case OBJ_TYPE_WALL:
                    case OBJ_TYPE_MISC: {
                        // if the pointedObject is OBJ_TYPE_SCENERY/OBJ_TYPE_WALL/OBJ_TYPE_MISC object then try to find possible itemObject behind it
                        // this must be in sync with the switch/case in _gmouse_handle_event
                        Object* itemObject = gameMouseGetObjectUnderCursor(OBJ_TYPE_ITEM, true, gElevation);

                        if (itemObject == nullptr && objectType == OBJ_TYPE_MISC) {
                            break;
                        }

                        bool itemIsOutlined = objectHasVisibleOutline(itemObject);

                        if (!itemIsOutlined) {
                            // filter out potential itemObject behind usable scenery which is not outlined already
                            if (objectType == OBJ_TYPE_SCENERY && _obj_action_can_use(pointedObject)) {
                                primaryAction = GAME_MOUSE_ACTION_MENU_ITEM_USE;
                                break;
                            }

                            bool canBeShootOrSeenThroughObject = (pointedObject->flags & (OBJECT_SHOOT_THRU | OBJECT_LIGHT_THRU)) != OBJECT_NONE;

                            // filter out not found itemObject and itemObject behind the blocking walls which is not outlined already
                            if (itemObject == nullptr || (objectType == OBJ_TYPE_WALL && !canBeShootOrSeenThroughObject)) {
                                primaryAction = GAME_MOUSE_ACTION_MENU_ITEM_LOOK;
                                break;
                            }
                        }

                        // intentionally fall trough the OBJ_TYPE_ITEM to enforce auto outlining of the itemObject and changing the mouse action menu
                        pointedObject = itemObject;
                    }
                    // FALLTHROUGH
                    case OBJ_TYPE_ITEM:
                        primaryAction = GAME_MOUSE_ACTION_MENU_ITEM_USE;
                        if (gGameMouseItemHighlightEnabled) {
                            Rect tmp;
                            if (objectSetOutline(pointedObject, OUTLINE_TYPE_ITEM, &tmp) == 0) {
                                tileWindowRefreshRect(&tmp, gElevation);
                                gGameMouseHighlightedItem = pointedObject;
                            }
                        }
                        break;
                    case OBJ_TYPE_CRITTER:
                        if (pointedObject == gDude) {
                            primaryAction = GAME_MOUSE_ACTION_MENU_ITEM_ROTATE;
                        } else {
                            if (_obj_action_can_talk_to(pointedObject)) {
                                if (isInCombat()) {
                                    primaryAction = GAME_MOUSE_ACTION_MENU_ITEM_LOOK;
                                } else {
                                    primaryAction = GAME_MOUSE_ACTION_MENU_ITEM_TALK;
                                }
                            } else {
                                if (critterFlagCheck(pointedObject->pid, CRITTER_NO_STEAL)) {
                                    primaryAction = GAME_MOUSE_ACTION_MENU_ITEM_LOOK;
                                } else {
                                    primaryAction = GAME_MOUSE_ACTION_MENU_ITEM_USE;
                                }
                            }
                        }
                        break;
                    default:
                        break;
                    }

                    if (primaryAction != -1) {
                        if (gameMouseRenderPrimaryAction(mouseX, mouseY, primaryAction, _scr_size.right - _scr_size.left + 1, _scr_size.bottom - _scr_size.top - 99) == 0) {
                            Rect tmp;
                            // NOTE: Uninline.
                            if (gmouse_3d_set_flat_fid(FrmId(InterfaceFrameId::ActionPick).fid(), &tmp) == 0) {
                                tileWindowRefreshRect(&tmp, gElevation);
                            }
                        }
                    }

                    if (pointedObject != gGameMousePointedObject) {
                        gGameMousePointedObject = pointedObject;
                        objectLookAt(gDude, gGameMousePointedObject);
                    }
                }
            } else if (gGameMouseMode == GAME_MOUSE_MODE_CROSSHAIR) {
                Object* pointedObject = gameMouseGetObjectUnderCursor(OBJ_TYPE_CRITTER, false, gElevation);
                if (pointedObject == nullptr) {
                    pointedObject = gameMouseGetObjectUnderCursor(OBJ_TYPE_INVALID, false, gElevation);
                    if (!objectIsDoor(pointedObject)) {
                        pointedObject = nullptr;
                    }
                }

                if (pointedObject != nullptr) {
                    bool pointedObjectIsCritter = objectTypeFromFid(pointedObject->fid) == OBJ_TYPE_CRITTER;

                    if (settings.preferences.combat_looks) {
                        if (objectExamine(gDude, pointedObject) == -1) {
                            objectLookAt(gDude, pointedObject);
                        }
                    }

                    Color color;
                    int accuracy;
                    char formattedAccuracy[8];
                    if (_combat_to_hit(pointedObject, &accuracy)) {
                        snprintf(formattedAccuracy, sizeof(formattedAccuracy), "%d%%", accuracy);

                        if (pointedObjectIsCritter) {
                            if (pointedObject->data.critter.combat.team != 0) {
                                color = COLOR_WHITE;
                            } else {
                                color = COLOR_LIGHT_ORANGE;
                            }
                        } else {
                            color = COLOR_GREY;
                        }
                    } else {
                        snprintf(formattedAccuracy, sizeof(formattedAccuracy), " %c ", 'X');

                        if (pointedObjectIsCritter) {
                            if (pointedObject->data.critter.combat.team != 0) {
                                color = COLOR_RED;
                            } else {
                                color = COLOR_SAGE_GREEN;
                            }
                        } else {
                            color = COLOR_LIGHT_PINK;
                        }
                    }

                    if (gameMouseRenderAccuracy(formattedAccuracy, color) == 0) {
                        Rect tmp;
                        // NOTE: Uninline.
                        if (gmouse_3d_set_flat_fid(FrmId(InterfaceFrameId::ActionToHit).fid(), &tmp) == 0) {
                            tileWindowRefreshRect(&tmp, gElevation);
                        }
                    }

                    if (gGameMousePointedObject != pointedObject) {
                        gGameMousePointedObject = pointedObject;
                    }
                } else {
                    Rect tmp;
                    if (gameMouseUpdateHexCursorFid(&tmp) == 0) {
                        tileWindowRefreshRect(&tmp, gElevation);
                    }
                }

                _gmouse_3d_last_move_time = v3;
                _gmouse_3d_hover_test = true;
            }
            return;
        }

        char formattedActionPoints[8];
        Color color;
        int distance = _make_path(gDude, gDude->tile, gGameMouseHexCursor->tile, nullptr, 1);
        if (distance != 0) {
            if (!isInCombat()) {
                formattedActionPoints[0] = '\0';
                color = COLOR_RED;
            } else {
                int actionPointsMax = critterGetMovementPointCostAdjustedForCrippledLegs(gDude, distance);
                int actionPointsRequired = std::max(0, actionPointsMax - _combat_free_move);

                if (actionPointsRequired <= gDude->data.critter.combat.ap) {
                    snprintf(formattedActionPoints, sizeof(formattedActionPoints), "%d", actionPointsRequired);
                    color = COLOR_WHITE;
                } else {
                    snprintf(formattedActionPoints, sizeof(formattedActionPoints), "%c", 'X');
                    color = COLOR_RED;
                }
            }
        } else {
            snprintf(formattedActionPoints, sizeof(formattedActionPoints), "%c", 'X');
            color = COLOR_RED;
        }

        if (gameMouseRenderActionPoints(formattedActionPoints, color) == 0) {
            Rect tmp;
            objectGetRect(gGameMouseHexCursor, &tmp);
            tileWindowRefreshRect(&tmp, 0);
        }

        _gmouse_3d_last_move_time = v3;
        _gmouse_3d_hover_test = true;
        dword_518D9C = gGameMouseHexCursor->tile;
        return;
    }

    _gmouse_3d_last_move_time = v3;
    _gmouse_3d_hover_test = false;
    gGameMouseLastX = mouseX;
    gGameMouseLastY = mouseY;

    if (!_gmouse_mapper_mode) {
        gameMouseSetBouncingCursorFid(FrmId(InterfaceFrameId::Blank).fid());
    }

    int v34 = 0;

    Rect r2;
    Rect r26;
    if (gameMouseUpdateHexCursorFid(&r2) == 0) {
        v34 |= 1;
    }

    if (gGameMouseHighlightedItem != nullptr) {
        if (objectClearOutline(gGameMouseHighlightedItem, &r26) == 0) {
            v34 |= 2;
        }
        gGameMouseHighlightedItem = nullptr;
    }

    switch (v34) {
    case 3:
        rectUnion(&r2, &r26, &r2);
        // FALLTHROUGH
    case 1:
        tileWindowRefreshRect(&r2, gElevation);
        break;
    case 2:
        tileWindowRefreshRect(&r26, gElevation);
        break;
    }
}

bool gameMouseClickOnInterfaceBar()
{
    Rect interfaceBarWindowRect;
    windowGetRect(gInterfaceBarWindow, &interfaceBarWindowRect);

    int interfaceBarWindowRectLeft = 0;
    int interfaceBarWindowRectRight = _scr_size.right;

    if (settings.ui.iface_bar_mode) {
        interfaceBarWindowRectLeft = interfaceBarWindowRect.left;
        interfaceBarWindowRectRight = interfaceBarWindowRect.right;
    }

    return _mouse_click_in(interfaceBarWindowRectLeft, interfaceBarWindowRect.top, interfaceBarWindowRectRight, interfaceBarWindowRect.bottom);
}

// 0x44BFA8 gmouse_handle_event
void _gmouse_handle_event(int mouseX, int mouseY, int mouseState)
{
    if (!gGameMouseInitialized) {
        return;
    }

    if (gGameMouseCursor >= MOUSE_CURSOR_WAIT_PLANET) {
        return;
    }

    if (!_gmouse_enabled) {
        return;
    }

    if (_gmouse_clicked_on_edge) {
        if (_gmouse_get_click_to_scroll()) {
            return;
        }
    }

    if (gameMouseClickOnInterfaceBar()) {
        return;
    }
    if (mapEdgeIsOverClippedArea(mouseX, mouseY)) {
        return;
    }

    // Check if we should block mouse button up events for inventoryOpenUseItemOn inventory window
    if (gBlockMouseUpEvent && (mouseState & MOUSE_EVENT_LEFT_BUTTON_UP) != 0) {
        gBlockMouseUpEvent = false;
        return;
    }

    if ((mouseState & MOUSE_EVENT_RIGHT_BUTTON_DOWN) != 0) {
        if ((mouseState & MOUSE_EVENT_RIGHT_BUTTON_REPEAT) == 0 && (gGameMouseHexCursor->flags & OBJECT_HIDDEN) == OBJECT_NONE) {
            gameMouseCycleMode();
        }
        return;
    }

    if ((mouseState & MOUSE_EVENT_LEFT_BUTTON_UP) != 0) {
        if (gGameMouseMode == GAME_MOUSE_MODE_MOVE) {
            int actionPoints;
            if (isInCombat()) {
                actionPoints = _combat_free_move + gDude->data.critter.combat.ap;
            } else {
                actionPoints = -1;
            }

            if (gPressedPhysicalKeys[SDL_SCANCODE_LSHIFT] || gPressedPhysicalKeys[SDL_SCANCODE_RSHIFT]) {
                if (settings.preferences.running) {
                    _dude_move(actionPoints);
                    return;
                }
            } else {
                if (!settings.preferences.running) {
                    _dude_move(actionPoints);
                    return;
                }
            }

            _dude_run(actionPoints);
            return;
        }

        if (gGameMouseMode == GAME_MOUSE_MODE_ARROW) {
            Object* targetObj = gameMouseGetObjectUnderCursor(OBJ_TYPE_INVALID, true, gElevation);
            if (targetObj != nullptr) {
                ObjectType objectType = objectTypeFromFid(targetObj->fid);
                switch (objectType) {
                case OBJ_TYPE_WALL:
                case OBJ_TYPE_SCENERY:
                case OBJ_TYPE_MISC: {
                    // if the targetObj is OBJ_TYPE_SCENERY/OBJ_TYPE_WALL/OBJ_TYPE_MISC object then it allows to pickup outlined itemObj potentially behind it
                    // this must be in sync with the switch/case in gameMouseRefresh
                    Object* itemObj = gameMouseGetObjectUnderCursor(OBJ_TYPE_ITEM, true, gElevation);
                    if (!objectHasVisibleOutline(itemObj)) {
                        if (objectType == OBJ_TYPE_SCENERY && _obj_action_can_use(targetObj)) {
                            _action_use_an_object(gDude, targetObj);
                            break;
                        }

                        if (objectType != OBJ_TYPE_MISC && objectExamine(gDude, targetObj) == -1) {
                            objectLookAt(gDude, targetObj);
                        }
                        break;
                    }

                    // intentionally fall through the OBJ_TYPE_ITEM to enforce actionPickUp
                    targetObj = itemObj;
                }
                // FALLTHROUGH
                case OBJ_TYPE_ITEM:
                    actionPickUp(gDude, targetObj);
                    break;
                case OBJ_TYPE_CRITTER:
                    if (targetObj == gDude) {
                        if (animationTypeFromFid(gDude->fid) == ANIM_STAND) {
                            Rect dudeRect;
                            if (objectRotateClockwise(targetObj, &dudeRect) == 0) {
                                tileWindowRefreshRect(&dudeRect, targetObj->elevation);
                            }
                        }
                    } else {
                        if (_obj_action_can_talk_to(targetObj)) {
                            if (isInCombat()) {
                                if (objectExamine(gDude, targetObj) == -1) {
                                    objectLookAt(gDude, targetObj);
                                }
                            } else {
                                actionTalk(gDude, targetObj);
                            }
                        } else {
                            actionLootCritter(gDude, targetObj);
                        }
                    }
                    break;
                default:
                    break;
                }
            }
            return;
        }

        if (gGameMouseMode == GAME_MOUSE_MODE_CROSSHAIR) {
            Object* targetObj = gameMouseGetObjectUnderCursor(OBJ_TYPE_CRITTER, false, gElevation);
            if (targetObj == nullptr) {
                targetObj = gameMouseGetObjectUnderCursor(OBJ_TYPE_INVALID, false, gElevation);
                if (!objectIsDoor(targetObj)) {
                    targetObj = nullptr;
                }
            }

            if (targetObj != nullptr) {
                _combat_attack_this(targetObj);
                _gmouse_3d_hover_test = true;
                gGameMouseLastY = mouseY;
                gGameMouseLastX = mouseX;
                _gmouse_3d_last_move_time = getTicks() - 250;
            }
            return;
        }

        if (gGameMouseMode == GAME_MOUSE_MODE_USE_CROSSHAIR) {
            Object* object = gameMouseGetObjectUnderCursor(OBJ_TYPE_INVALID, true, gElevation);
            if (object != nullptr) {
                Object* weapon;
                if (interfaceGetActiveItem(&weapon) != -1) {
                    if (isInCombat()) {
                        HitMode hitMode = interfaceGetCurrentHand()
                            ? HIT_MODE_RIGHT_WEAPON_PRIMARY
                            : HIT_MODE_LEFT_WEAPON_PRIMARY;

                        int actionPointsRequired = itemGetActionPointCost(gDude, hitMode, false);
                        if (actionPointsRequired <= gDude->data.critter.combat.ap) {
                            if (_action_use_an_item_on_object(gDude, object, weapon) != -1) {
                                int actionPoints = gDude->data.critter.combat.ap;
                                if (actionPointsRequired > actionPoints) {
                                    gDude->data.critter.combat.ap = 0;
                                } else {
                                    gDude->data.critter.combat.ap -= actionPointsRequired;
                                }
                                interfaceRenderActionPoints(gDude->data.critter.combat.ap, _combat_free_move);
                            }
                        }
                    } else {
                        _action_use_an_item_on_object(gDude, object, weapon);
                    }
                }
            }
            gameMouseSetCursor(MOUSE_CURSOR_NONE);
            gameMouseSetMode(GAME_MOUSE_MODE_MOVE);
            return;
        }

        if (gGameMouseMode == GAME_MOUSE_MODE_USE_FIRST_AID
            || gGameMouseMode == GAME_MOUSE_MODE_USE_DOCTOR
            || gGameMouseMode == GAME_MOUSE_MODE_USE_LOCKPICK
            || gGameMouseMode == GAME_MOUSE_MODE_USE_STEAL
            || gGameMouseMode == GAME_MOUSE_MODE_USE_TRAPS
            || gGameMouseMode == GAME_MOUSE_MODE_USE_SCIENCE
            || gGameMouseMode == GAME_MOUSE_MODE_USE_REPAIR) {
            Object* object = gameMouseGetObjectUnderCursor(OBJ_TYPE_INVALID, true, gElevation);
            if (object == nullptr || actionUseSkill(gDude, object, gGameMouseModeSkills[gGameMouseMode - FIRST_GAME_MOUSE_MODE_SKILL]) != -1) {
                gameMouseSetCursor(MOUSE_CURSOR_NONE);
                gameMouseSetMode(GAME_MOUSE_MODE_MOVE);
            }
            return;
        }
    }

    if ((mouseState & MOUSE_EVENT_LEFT_BUTTON_DOWN_REPEAT) == MOUSE_EVENT_LEFT_BUTTON_DOWN_REPEAT && gGameMouseMode == GAME_MOUSE_MODE_ARROW) {
        Object* targetObj = gameMouseGetObjectUnderCursor(OBJ_TYPE_INVALID, true, gElevation);
        if (targetObj != nullptr) {
            int actionMenuItemsCount = 0;
            int actionMenuItems[GAME_MOUSE_ACTION_MENU_ITEM_COUNT - 1];
            switch (objectTypeFromFid(targetObj->fid)) {
            case OBJ_TYPE_ITEM:
                actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_USE;
                actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_LOOK;
                if (itemGetType(targetObj) == ITEM_TYPE_CONTAINER) {
                    actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_INVENTORY;
                    actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_USE_SKILL;
                }
                actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_CANCEL;
                break;
            case OBJ_TYPE_CRITTER:
                if (targetObj == gDude) {
                    actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_ROTATE;
                } else {
                    if (_obj_action_can_talk_to(targetObj)) {
                        if (!isInCombat()) {
                            actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_TALK;
                        }

                        if (gameMouseLongPressUsesLootActionForCritter(targetObj)) {
                            actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_USE;
                        }
                    } else {
                        if (!critterFlagCheck(targetObj->pid, CRITTER_NO_STEAL)) {
                            actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_USE;
                        }
                    }

                    if (actionCheckPush(gDude, targetObj)) {
                        actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_PUSH;
                    }
                }

                actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_LOOK;
                actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_INVENTORY;
                actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_USE_SKILL;
                actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_CANCEL;
                break;
            case OBJ_TYPE_SCENERY:
                if (_obj_action_can_use(targetObj)) {
                    actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_USE;
                }

                actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_LOOK;
                actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_INVENTORY;
                actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_USE_SKILL;
                actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_CANCEL;
                break;
            case OBJ_TYPE_WALL:
                actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_LOOK;
                if (_obj_action_can_use(targetObj)) {
                    actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_INVENTORY;
                }
                actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_CANCEL;
                break;
            default:
                break;
            }

            if (gameMouseRenderActionMenuItems(mouseX, mouseY, actionMenuItems, actionMenuItemsCount, _scr_size.right - _scr_size.left + 1, _scr_size.bottom - _scr_size.top - 99) == 0) {
                Rect cursorRect;
                // NOTE: Uninline.
                if (gmouse_3d_set_flat_fid(FrmId(InterfaceFrameId::ActionMenu).fid(), &cursorRect) == 0 && _gmouse_3d_move_to(mouseX, mouseY, gElevation, &cursorRect) == 0) {
                    tileWindowRefreshRect(&cursorRect, gElevation);
                    isoDisable();

                    int newMouseY = mouseY;
                    int actionIndex = 0;
                    while ((mouseGetEvent() & MOUSE_EVENT_LEFT_BUTTON_UP) == 0) {
                        sharedFpsLimiter.mark();

                        inputGetInput();

                        if (_game_user_wants_to_quit != GAME_QUIT_REQUEST_NONE) {
                            actionMenuItems[actionIndex] = 0;
                        }

                        int updatedMouseX;
                        int updatedMouseY;
                        mouseGetPosition(&updatedMouseX, &updatedMouseY);

                        if (abs(updatedMouseY - newMouseY) > 10) {
                            int nextActionIndex;
                            if (newMouseY >= updatedMouseY) {
                                nextActionIndex = actionIndex - 1;
                            } else {
                                nextActionIndex = actionIndex + 1;
                            }

                            if (gameMouseHighlightActionMenuItemAtIndex(nextActionIndex) == 0) {
                                tileWindowRefreshRect(&cursorRect, gElevation);
                                actionIndex = nextActionIndex;
                            }
                            newMouseY = updatedMouseY;
                        }

                        renderPresent();
                        sharedFpsLimiter.throttle();
                    }

                    isoEnable();

                    _gmouse_3d_hover_test = false;
                    gGameMouseLastX = mouseX;
                    gGameMouseLastY = mouseY;
                    _gmouse_3d_last_move_time = getTicks();

                    mouseDeviceSetLogicalPosition(mouseX, mouseY);
                    _mouse_set_position(mouseX, mouseY);

                    if (gameMouseUpdateHexCursorFid(&cursorRect) == 0) {
                        tileWindowRefreshRect(&cursorRect, gElevation);
                    }

                    switch (actionMenuItems[actionIndex]) {
                    case GAME_MOUSE_ACTION_MENU_ITEM_INVENTORY:
                        inventoryOpenUseItemOn(targetObj);
                        break;
                    case GAME_MOUSE_ACTION_MENU_ITEM_LOOK:
                        if (objectExamine(gDude, targetObj) == -1) {
                            objectLookAt(gDude, targetObj);
                        }
                        break;
                    case GAME_MOUSE_ACTION_MENU_ITEM_ROTATE:
                        if (objectRotateClockwise(targetObj, &cursorRect) == 0) {
                            tileWindowRefreshRect(&cursorRect, targetObj->elevation);
                        }
                        break;
                    case GAME_MOUSE_ACTION_MENU_ITEM_TALK:
                        actionTalk(gDude, targetObj);
                        break;
                    case GAME_MOUSE_ACTION_MENU_ITEM_USE:
                        switch (objectTypeFromFid(targetObj->fid)) {
                        case OBJ_TYPE_SCENERY:
                            _action_use_an_object(gDude, targetObj);
                            break;
                        case OBJ_TYPE_CRITTER:
                            if (gameMouseLongPressUsesLootActionForCritter(targetObj)) {
                                // party member: trade via steal skill
                                actionUseSkill(gDude, targetObj, SKILL_STEAL);
                            } else {
                                actionLootCritter(gDude, targetObj);
                            }
                            break;
                        default:
                            actionPickUp(gDude, targetObj);
                            break;
                        }
                        break;
                    case GAME_MOUSE_ACTION_MENU_ITEM_USE_SKILL:
                        if (1) {
                            Skill skill = SKILL_INVALID;

                            SkilldexRC rc = skilldexOpen();
                            switch (rc) {
                            case SKILLDEX_RC_SNEAK:
                                _action_skill_use(SKILL_SNEAK);
                                break;
                            case SKILLDEX_RC_LOCKPICK:
                                skill = SKILL_LOCKPICK;
                                break;
                            case SKILLDEX_RC_STEAL:
                                skill = SKILL_STEAL;
                                break;
                            case SKILLDEX_RC_TRAPS:
                                skill = SKILL_TRAPS;
                                break;
                            case SKILLDEX_RC_FIRST_AID:
                                skill = SKILL_FIRST_AID;
                                break;
                            case SKILLDEX_RC_DOCTOR:
                                skill = SKILL_DOCTOR;
                                break;
                            case SKILLDEX_RC_SCIENCE:
                                skill = SKILL_SCIENCE;
                                break;
                            case SKILLDEX_RC_REPAIR:
                                skill = SKILL_REPAIR;
                                break;
                            default:
                                break;
                            }

                            if (skill != SKILL_INVALID) {
                                actionUseSkill(gDude, targetObj, skill);
                            }
                        }
                        break;
                    case GAME_MOUSE_ACTION_MENU_ITEM_PUSH:
                        actionPush(gDude, targetObj);
                        break;
                    }
                }
            }
        }
    }
}

// 0x44C840 gmouse_set_cursor
int gameMouseSetCursor(int cursor)
{
    if (!gGameMouseInitialized) {
        return -1;
    }

    if (cursor != MOUSE_CURSOR_ARROW && cursor == gGameMouseCursor && (gGameMouseCursor < 25 || gGameMouseCursor >= 27)) {
        return -1;
    }

    CacheEntry* mouseCursorFrmHandle;
    const FrmId fid = kGameMouseCursorFrmIds[cursor];
    Art* mouseCursorFrm = artLock(fid, &mouseCursorFrmHandle);
    if (mouseCursorFrm == nullptr) {
        return -1;
    }

    bool shouldUpdate = true;
    int frame = 0;
    if (cursor >= FIRST_GAME_MOUSE_ANIMATED_CURSOR) {
        unsigned int tick = getTicks();

        if ((gGameMouseHexCursor->flags & OBJECT_HIDDEN) == OBJECT_NONE) {
            gameMouseObjectsHide();
        }

        unsigned int delay = 1000 / artGetFramesPerSecond(mouseCursorFrm);
        if (getTicksBetween(tick, gGameMouseAnimatedCursorLastUpdateTimestamp) < delay) {
            shouldUpdate = false;
        } else {
            if (artGetFrameCount(mouseCursorFrm) <= gGameMouseAnimatedCursorNextFrame) {
                gGameMouseAnimatedCursorNextFrame = 0;
            }

            frame = gGameMouseAnimatedCursorNextFrame;
            gGameMouseAnimatedCursorLastUpdateTimestamp = tick;
            gGameMouseAnimatedCursorNextFrame++;
        }
    }

    if (!shouldUpdate) {
        return -1;
    }

    int width = artGetWidth(mouseCursorFrm, frame);
    int height = artGetHeight(mouseCursorFrm, frame);

    int offsetX;
    int offsetY;
    artGetRotationOffsets(mouseCursorFrm, ROTATION_NE, &offsetX, &offsetY);

    offsetX = width / 2 - offsetX;
    offsetY = height - 1 - offsetY;

    unsigned char* mouseCursorFrmData = artGetFrameData(mouseCursorFrm, frame);
    if (mouseSetFrame(mouseCursorFrmData, width, height, width, offsetX, offsetY, 0) != 0) {
        return -1;
    }

    if (gGameMouseCursorFrmHandle != INVALID_CACHE_ENTRY) {
        artUnlock(gGameMouseCursorFrmHandle);
    }

    gGameMouseCursor = cursor;
    gGameMouseCursorFrmHandle = mouseCursorFrmHandle;

    return 0;
}

// 0x44C9E8 gmouse_get_cursor
int gameMouseGetCursor()
{
    return gGameMouseCursor;
}

// 0x44C9F0 gmouse_set_mapper_mode
void gmouse_set_mapper_mode(int mode)
{
    _gmouse_mapper_mode = mode;
}

// 0x44C9F8 gmouse_3d_enable_modes
void _gmouse_3d_enable_modes()
{
    _gmouse_3d_modes_enabled = 1;
}

// 0x44CA18 gmouse_3d_set_mode
void gameMouseSetMode(int mode)
{
    if (!gGameMouseInitialized) {
        return;
    }

    if (!_gmouse_3d_modes_enabled) {
        return;
    }

    if (mode == gGameMouseMode) {
        return;
    }

    gameMouseSetBouncingCursorFid(FrmId(InterfaceFrameId::Blank).fid());

    const FrmId frmId = FrmId(gGameMouseModeFrmIds[mode]);

    Rect rect;
    // NOTE: Uninline.
    if (gmouse_3d_set_flat_fid(frmId.fid(), &rect) == -1) {
        return;
    }

    int mouseX;
    int mouseY;
    mouseGetPosition(&mouseX, &mouseY);

    Rect cursorRect;
    if (_gmouse_3d_move_to(mouseX, mouseY, gElevation, &cursorRect) == 0) {
        rectUnion(&rect, &cursorRect, &rect);
    }

    int v5 = 0;
    if (gGameMouseMode == GAME_MOUSE_MODE_CROSSHAIR) {
        v5 = -1;
    }

    if (mode != 0) {
        if (mode == GAME_MOUSE_MODE_CROSSHAIR) {
            v5 = 1;
        }

        if (gGameMouseMode == 0) {
            if (objectDisableOutline(gGameMouseHexCursor, &cursorRect) == 0) {
                rectUnion(&rect, &cursorRect, &rect);
            }
        }
    } else {
        if (objectEnableOutline(gGameMouseHexCursor, &cursorRect) == 0) {
            rectUnion(&rect, &cursorRect, &rect);
        }
    }

    gGameMouseMode = mode;
    _gmouse_3d_hover_test = false;
    _gmouse_3d_last_move_time = getTicks();

    tileWindowRefreshRect(&rect, gElevation);

    switch (v5) {
    case 1:
        _combat_outline_on();
        break;
    case -1:
        _combat_outline_off();
        break;
    }

    // On iPad, keep the gameplay cursor in trackpad mode so tapping the
    // screen doesn't teleport the cursor mid-combat. UI screens (inventory,
    // skilldex, elevator, menus) still enable touchscreen mode explicitly.
#if __APPLE__ && TARGET_OS_IOS
    touch_set_touchscreen_mode(false);
#else
    touch_set_touchscreen_mode(mode == GAME_MOUSE_MODE_MOVE);
#endif
}

// 0x44CB6C
int gameMouseGetMode()
{
    return gGameMouseMode;
}

// 0x44CB74 gmouse_3d_toggle_mode
void gameMouseCycleMode()
{
    int mode = (gGameMouseMode + 1) % 3;

    if (isInCombat()) {
        Object* item;
        if (interfaceGetActiveItem(&item) == 0) {
            if (item != nullptr && itemGetType(item) != ITEM_TYPE_WEAPON && mode == GAME_MOUSE_MODE_CROSSHAIR) {
                mode = GAME_MOUSE_MODE_MOVE;
            }
        }
    } else {
        if (mode == GAME_MOUSE_MODE_CROSSHAIR) {
            mode = GAME_MOUSE_MODE_MOVE;
        }
    }

    gameMouseSetMode(mode);
}

// 0x44CBD0 gmouse_3d_refresh
void _gmouse_3d_refresh()
{
    gGameMouseLastX = -1;
    gGameMouseLastY = -1;
    _gmouse_3d_hover_test = false;
    _gmouse_3d_last_move_time = 0;
    gameMouseRefresh();
}

// 0x44CBFC gmouse_3d_set_fid
int gameMouseSetBouncingCursorFid(int fid)
{
    if (!gGameMouseInitialized) {
        return -1;
    }

    if (!artExists(fid)) {
        return -1;
    }

    if (gGameMouseBouncingCursor->fid == fid) {
        return -1;
    }

    if (!_gmouse_mapper_mode) {
        return objectSetFid(gGameMouseBouncingCursor, fid, nullptr);
    }

    int refreshFlags = 0;

    Rect oldRect;
    if (gGameMouseBouncingCursor->fid != -1) {
        objectGetRect(gGameMouseBouncingCursor, &oldRect);
        refreshFlags |= REFRESH_BOUNCING_CURSOR;
    }

    int rc = -1;

    Rect rect;
    if (objectSetFid(gGameMouseBouncingCursor, fid, &rect) == 0) {
        rc = 0;
        refreshFlags |= REFRESH_HEX_CURSOR;
    }

    if ((gGameMouseHexCursor->flags & OBJECT_HIDDEN) == OBJECT_NONE) {
        if (refreshFlags == REFRESH_BOUNCING_CURSOR) {
            tileWindowRefreshRect(&oldRect, gElevation);
        } else if (refreshFlags == REFRESH_HEX_CURSOR) {
            tileWindowRefreshRect(&rect, gElevation);
        } else if (refreshFlags == REFRESH_BOTH_CURSORS) {
            rectUnion(&oldRect, &rect, &oldRect);
            tileWindowRefreshRect(&oldRect, gElevation);
        }
    }

    return rc;
}

// 0x44CD0C gmouse_3d_reset_fid
void gameMouseResetBouncingCursorFid()
{
    gameMouseSetBouncingCursorFid(FrmId(InterfaceFrameId::Blank).fid());
}

// 0x44CD2C gmouse_3d_on
void gameMouseObjectsShow()
{
    if (!gGameMouseInitialized) {
        return;
    }

    int refreshFlags = 0;

    Rect rect1;
    if (objectShow(gGameMouseBouncingCursor, &rect1) == 0) {
        refreshFlags |= REFRESH_BOUNCING_CURSOR;
    }

    Rect rect2;
    if (objectShow(gGameMouseHexCursor, &rect2) == 0) {
        refreshFlags |= REFRESH_HEX_CURSOR;
    }

    Rect tmp;
    if (gGameMouseMode != GAME_MOUSE_MODE_MOVE) {
        if (objectDisableOutline(gGameMouseHexCursor, &tmp) == 0) {
            if ((refreshFlags & REFRESH_HEX_CURSOR) != 0) {
                rectUnion(&rect2, &tmp, &rect2);
            } else {
                memcpy(&rect2, &tmp, sizeof(rect2));
                refreshFlags |= REFRESH_HEX_CURSOR;
            }
        }
    }

    if (gameMouseUpdateHexCursorFid(&tmp) == 0) {
        if ((refreshFlags & REFRESH_HEX_CURSOR) != 0) {
            rectUnion(&rect2, &tmp, &rect2);
        } else {
            memcpy(&rect2, &tmp, sizeof(rect2));
            refreshFlags |= REFRESH_HEX_CURSOR;
        }
    }

    if (refreshFlags != 0) {
        Rect* rect;
        switch (refreshFlags) {
        case REFRESH_BOUNCING_CURSOR:
            rect = &rect1;
            break;
        case REFRESH_HEX_CURSOR:
            rect = &rect2;
            break;
        case REFRESH_BOTH_CURSORS:
            rectUnion(&rect1, &rect2, &rect1);
            rect = &rect1;
            break;
        default:
            assert(false && "Should be unreachable");
        }

        tileWindowRefreshRect(rect, gElevation);
    }

    _gmouse_3d_hover_test = false;
    _gmouse_3d_last_move_time = getTicks() - 250;
}

// 0x44CE34 gmouse_3d_off
void gameMouseObjectsHide()
{
    if (!gGameMouseInitialized) {
        return;
    }

    int refreshFlags = 0;

    Rect rect1;
    if (objectHide(gGameMouseBouncingCursor, &rect1) == 0) {
        refreshFlags |= REFRESH_BOUNCING_CURSOR;
    }

    Rect rect2;
    if (objectHide(gGameMouseHexCursor, &rect2) == 0) {
        refreshFlags |= REFRESH_HEX_CURSOR;
    }

    if (refreshFlags == REFRESH_BOUNCING_CURSOR) {
        tileWindowRefreshRect(&rect1, gElevation);
    } else if (refreshFlags == REFRESH_HEX_CURSOR) {
        tileWindowRefreshRect(&rect2, gElevation);
    } else if (refreshFlags == REFRESH_BOTH_CURSORS) {
        rectUnion(&rect1, &rect2, &rect1);
        tileWindowRefreshRect(&rect1, gElevation);
    }
}

// 0x44CEB0 gmouse_3d_is_on
bool gameMouseObjectsIsVisible()
{
    return (gGameMouseHexCursor->flags & OBJECT_HIDDEN) == OBJECT_NONE;
}

// 0x44CEC4 object_under_mouse
Object* gameMouseGetObjectUnderCursor(ObjectType objectType, bool includeDude, int elevation)
{
    int mouseX;
    int mouseY;
    mouseGetPosition(&mouseX, &mouseY);

    bool intersectsRoof = false;
    if (objectType == -1) {
        if (_square_roof_intersect(mouseX, mouseY, elevation)) {
            if (_obj_intersects_with(gEgg, mouseX, mouseY) == 0) {
                intersectsRoof = true;
            }
        }
    }

    Object* found = nullptr;
    if (!intersectsRoof) {
        ObjectWithFlags* entries;
        int count = _obj_create_intersect_list(mouseX, mouseY, elevation, objectType, &entries);
        for (int index = count - 1; index >= 0; index--) {
            ObjectWithFlags* ptr = &(entries[index]);
            if (includeDude || gDude != ptr->object) {
                found = ptr->object;
                if ((ptr->flags & OBJECT_HIDDEN) != OBJECT_NONE) {
                    if ((ptr->flags & OBJECT_NO_SAVE) == OBJECT_NONE) {
                        if (objectTypeFromFid(ptr->object->fid) != OBJ_TYPE_CRITTER || (ptr->object->data.critter.combat.results & (DAM_KNOCKED_OUT | DAM_DEAD)) == 0) {
                            break;
                        }
                    }
                }
            }
        }

        if (count != 0) {
            _obj_delete_intersect_list(&entries);
        }
    }
    return found;
}

// 0x44CFA0 gmouse_3d_build_pick_frame
int gameMouseRenderPrimaryAction(int x, int y, int menuItem, int width, int height)
{
    CacheEntry* menuItemFrmHandle;
    const FrmId menuItemFrmId = kGameMouseActionMenuItemFrmIds[menuItem][GAME_MOUSE_ACTION_MENU_ITEM_FRAME_NORMAL];
    Art* menuItemFrm = artLock(menuItemFrmId, &menuItemFrmHandle);
    if (menuItemFrm == nullptr) {
        return -1;
    }

    CacheEntry* arrowFrmHandle;
    const FrmId arrowFrmId = FrmId(gGameMouseModeFrmIds[GAME_MOUSE_MODE_ARROW]);
    Art* arrowFrm = artLock(arrowFrmId, &arrowFrmHandle);
    if (arrowFrm == nullptr) {
        artUnlock(menuItemFrmHandle);
        return -1;
    }

    unsigned char* arrowFrmData = artGetFrameData(arrowFrm);
    int arrowFrmWidth = artGetWidth(arrowFrm);
    int arrowFrmHeight = artGetHeight(arrowFrm);

    unsigned char* menuItemFrmData = artGetFrameData(menuItemFrm);
    int menuItemFrmWidth = artGetWidth(menuItemFrm);
    int menuItemFrmHeight = artGetHeight(menuItemFrm);

    if (gGameMouseActionPickFrm == nullptr
        || gGameMouseActionPickFrmData == nullptr
        || gGameMouseActionPickFrmDataSize <= 0
        || arrowFrmData == nullptr
        || menuItemFrmData == nullptr
        || arrowFrmWidth <= 0
        || arrowFrmHeight <= 0
        || menuItemFrmWidth <= 0
        || menuItemFrmHeight <= 0
        || arrowFrmWidth + menuItemFrmWidth > gGameMouseActionPickFrmWidth
        || std::max(arrowFrmHeight, menuItemFrmHeight) > gGameMouseActionPickFrmHeight) {
        artUnlock(arrowFrmHandle);
        artUnlock(menuItemFrmHandle);
        return -1;
    }

    unsigned char* arrowFrmDest = gGameMouseActionPickFrmData;
    unsigned char* menuItemFrmDest = gGameMouseActionPickFrmData;

    _gmouse_3d_pick_frame_hot_x = 0;
    _gmouse_3d_pick_frame_hot_y = 0;

    gGameMouseActionPickFrm->xOffsets[0] = gGameMouseActionPickFrmWidth / 2;
    gGameMouseActionPickFrm->yOffsets[0] = gGameMouseActionPickFrmHeight - 1;

    int maxX = x + menuItemFrmWidth + arrowFrmWidth - 1;
    int maxY = y + menuItemFrmHeight - 1;
    int shiftY = maxY - height + 2;
    int maxShiftY = gGameMouseActionPickFrmHeight - arrowFrmHeight;
    if (maxShiftY < 0) {
        artUnlock(arrowFrmHandle);
        artUnlock(menuItemFrmHandle);
        return -1;
    }
    shiftY = std::clamp(shiftY, 0, maxShiftY);

    if (maxX < width) {
        menuItemFrmDest += arrowFrmWidth;
        if (maxY >= height) {
            _gmouse_3d_pick_frame_hot_y = shiftY;
            gGameMouseActionPickFrm->yOffsets[0] -= shiftY;
            arrowFrmDest += gGameMouseActionPickFrmWidth * shiftY;
        }
    } else {
        // mirrored cursor for far-right side of screen
        artUnlock(arrowFrmHandle);

        arrowFrm = artLock(FrmId(InterfaceFrameId::ActionArrowMirrored), &arrowFrmHandle);
        if (arrowFrm == nullptr) {
            artUnlock(menuItemFrmHandle);
            return -1;
        }

        arrowFrmData = artGetFrameData(arrowFrm);
        if (arrowFrmData == nullptr) {
            artUnlock(arrowFrmHandle);
            artUnlock(menuItemFrmHandle);
            return -1;
        }

        arrowFrmDest += menuItemFrmWidth;

        gGameMouseActionPickFrm->xOffsets[0] = -gGameMouseActionPickFrm->xOffsets[0];
        _gmouse_3d_pick_frame_hot_x += menuItemFrmWidth + arrowFrmWidth;

        if (maxY >= height) {
            _gmouse_3d_pick_frame_hot_y += shiftY;
            gGameMouseActionPickFrm->yOffsets[0] -= shiftY;

            arrowFrmDest += gGameMouseActionPickFrmWidth * shiftY;
        }
    }

    memset(gGameMouseActionPickFrmData, 0, gGameMouseActionPickFrmDataSize);

    blitBufferToBuffer(arrowFrmData, arrowFrmWidth, arrowFrmHeight, arrowFrmWidth, arrowFrmDest, gGameMouseActionPickFrmWidth);
    blitBufferToBuffer(menuItemFrmData, menuItemFrmWidth, menuItemFrmHeight, menuItemFrmWidth, menuItemFrmDest, gGameMouseActionPickFrmWidth);

    artUnlock(arrowFrmHandle);
    artUnlock(menuItemFrmHandle);

    return 0;
}

// 0x44D200 gmouse_3d_pick_frame_hot
int _gmouse_3d_pick_frame_hot(int* x, int* y)
{
    *x = _gmouse_3d_pick_frame_hot_x;
    *y = _gmouse_3d_pick_frame_hot_y;
    return 0;
}

// 0x44D214 gmouse_3d_build_menu_frame
int gameMouseRenderActionMenuItems(int x, int y, const int* menuItems, int menuItemsLength, int width, int height)
{
    _gmouse_3d_menu_actions_start = nullptr;
    gGameMouseActionMenuHighlightedItemIndex = 0;
    gGameMouseActionMenuItemsLength = 0;

    if (menuItems == nullptr) {
        return -1;
    }

    if (menuItemsLength == 0 || menuItemsLength >= GAME_MOUSE_ACTION_MENU_ITEM_COUNT) {
        return -1;
    }

    CacheEntry* menuItemFrmHandles[GAME_MOUSE_ACTION_MENU_ITEM_COUNT];
    Art* menuItemFrms[GAME_MOUSE_ACTION_MENU_ITEM_COUNT];

    for (int index = 0; index < menuItemsLength; index++) {
        if (menuItems[index] < 0 || menuItems[index] >= GAME_MOUSE_ACTION_MENU_ITEM_COUNT) {
            while (--index >= 0) {
                artUnlock(menuItemFrmHandles[index]);
            }
            return -1;
        }

        const FrmId frmId = kGameMouseActionMenuItemFrmIds[menuItems[index]][index ? GAME_MOUSE_ACTION_MENU_ITEM_FRAME_NORMAL : GAME_MOUSE_ACTION_MENU_ITEM_FRAME_HIGHLIGHTED];

        menuItemFrms[index] = artLock(frmId, &(menuItemFrmHandles[index]));
        if (menuItemFrms[index] == nullptr) {
            while (--index >= 0) {
                artUnlock(menuItemFrmHandles[index]);
            }
            return -1;
        }
    }

    const FrmId frmId = FrmId(gGameMouseModeFrmIds[GAME_MOUSE_MODE_ARROW]);
    CacheEntry* arrowFrmHandle;
    Art* arrowFrm = artLock(frmId, &arrowFrmHandle);
    if (arrowFrm == nullptr) {
        for (int index = 0; index < menuItemsLength; index++) {
            artUnlock(menuItemFrmHandles[index]);
        }
        return -1;
    }

    int arrowWidth = artGetWidth(arrowFrm);
    int arrowHeight = artGetHeight(arrowFrm);
    unsigned char* arrowData = artGetFrameData(arrowFrm);

    int menuItemWidth = artGetWidth(menuItemFrms[0]);
    int menuItemHeight = artGetHeight(menuItemFrms[0]);

    if (gGameMouseActionMenuFrm == nullptr
        || gGameMouseActionMenuFrmData == nullptr
        || gGameMouseActionMenuFrmDataSize <= 0
        || arrowData == nullptr
        || arrowWidth <= 0
        || arrowHeight <= 0
        || menuItemWidth <= 0
        || menuItemHeight <= 0
        || arrowWidth + menuItemWidth > gGameMouseActionMenuFrmWidth
        || menuItemsLength * menuItemHeight > gGameMouseActionMenuFrmHeight) {
        artUnlock(arrowFrmHandle);
        for (int index = 0; index < menuItemsLength; index++) {
            artUnlock(menuItemFrmHandles[index]);
        }
        return -1;
    }

    _gmouse_3d_menu_frame_hot_x = 0;
    _gmouse_3d_menu_frame_hot_y = 0;

    gGameMouseActionMenuFrm->xOffsets[0] = gGameMouseActionMenuFrmWidth / 2;
    gGameMouseActionMenuFrm->yOffsets[0] = gGameMouseActionMenuFrmHeight - 1;

    int maxY = y + menuItemsLength * menuItemHeight - 1;
    int shiftY = maxY - height + 2;
    int maxShiftY = gGameMouseActionMenuFrmHeight - arrowHeight;
    if (maxShiftY < 0) {
        artUnlock(arrowFrmHandle);
        for (int index = 0; index < menuItemsLength; index++) {
            artUnlock(menuItemFrmHandles[index]);
        }
        return -1;
    }
    shiftY = std::clamp(shiftY, 0, maxShiftY);

    unsigned char* arrowFrmDest = gGameMouseActionMenuFrmData;
    unsigned char* menuItemFrmDest = arrowFrmDest;

    if (x + arrowWidth + menuItemWidth - 1 < width) {
        menuItemFrmDest = arrowFrmDest + arrowWidth;
        if (height <= maxY) {
            _gmouse_3d_menu_frame_hot_y += shiftY;
            arrowFrmDest += gGameMouseActionMenuFrmWidth * shiftY;
            gGameMouseActionMenuFrm->yOffsets[0] -= shiftY;
        }
    } else {
        // Mirrored arrow (from left to right).
        artUnlock(arrowFrmHandle);
        arrowFrm = artLock(FrmId(InterfaceFrameId::ActionArrowMirrored), &arrowFrmHandle);
        if (arrowFrm == nullptr) {
            for (int index = 0; index < menuItemsLength; index++) {
                artUnlock(menuItemFrmHandles[index]);
            }
            return -1;
        }

        arrowData = artGetFrameData(arrowFrm);
        if (arrowData == nullptr) {
            artUnlock(arrowFrmHandle);
            for (int index = 0; index < menuItemsLength; index++) {
                artUnlock(menuItemFrmHandles[index]);
            }
            return -1;
        }

        arrowFrmDest += menuItemWidth;

        gGameMouseActionMenuFrm->xOffsets[0] = -gGameMouseActionMenuFrm->xOffsets[0];
        _gmouse_3d_menu_frame_hot_x += menuItemWidth + arrowWidth;
        if (maxY >= height) {
            _gmouse_3d_menu_frame_hot_y += shiftY;
            gGameMouseActionMenuFrm->yOffsets[0] -= shiftY;
            arrowFrmDest += gGameMouseActionMenuFrmWidth * shiftY;
        }
    }

    memset(gGameMouseActionMenuFrmData, 0, gGameMouseActionMenuFrmDataSize);
    blitBufferToBuffer(arrowData, arrowWidth, arrowHeight, arrowWidth, arrowFrmDest, gGameMouseActionMenuFrmWidth);

    unsigned char* dest = menuItemFrmDest;
    for (int index = 0; index < menuItemsLength; index++) {
        unsigned char* data = artGetFrameData(menuItemFrms[index]);
        if (data == nullptr) {
            artUnlock(arrowFrmHandle);
            for (int index = 0; index < menuItemsLength; index++) {
                artUnlock(menuItemFrmHandles[index]);
            }
            return -1;
        }

        blitBufferToBuffer(data, menuItemWidth, menuItemHeight, menuItemWidth, dest, gGameMouseActionMenuFrmWidth);
        dest += gGameMouseActionMenuFrmWidth * menuItemHeight;
    }

    artUnlock(arrowFrmHandle);

    for (int index = 0; index < menuItemsLength; index++) {
        artUnlock(menuItemFrmHandles[index]);
    }

    memcpy(gGameMouseActionMenuItems, menuItems, sizeof(*gGameMouseActionMenuItems) * menuItemsLength);
    gGameMouseActionMenuItemsLength = menuItemsLength;
    _gmouse_3d_menu_actions_start = menuItemFrmDest;

    Sound* sound = soundEffectLoad("iaccuxx1", nullptr);
    if (sound != nullptr) {
        soundEffectPlay(sound);
    }

    return 0;
}

// 0x44D630 gmouse_3d_highlight_menu_frame
int gameMouseHighlightActionMenuItemAtIndex(int menuItemIndex)
{
    if (menuItemIndex < 0 || menuItemIndex >= gGameMouseActionMenuItemsLength) {
        return -1;
    }

    if (_gmouse_3d_menu_actions_start == nullptr
        || gGameMouseActionMenuFrmData == nullptr
        || gGameMouseActionMenuFrmDataSize <= 0
        || gGameMouseActionMenuHighlightedItemIndex >= gGameMouseActionMenuItemsLength) {
        return -1;
    }

    auto canBlitActionMenuItem = [](int index, int width, int height) {
        if (width <= 0 || height <= 0 || index < 0) {
            return false;
        }

        int baseOffset = static_cast<int>(_gmouse_3d_menu_actions_start - gGameMouseActionMenuFrmData);
        if (baseOffset < 0 || baseOffset >= gGameMouseActionMenuFrmDataSize) {
            return false;
        }

        int rowOffset = gGameMouseActionMenuFrmWidth * height * index;
        int needed = gGameMouseActionMenuFrmWidth * (height - 1) + width;
        return rowOffset >= 0
            && needed > 0
            && baseOffset + rowOffset <= gGameMouseActionMenuFrmDataSize - needed;
    };

    CacheEntry* handle;
    if (gGameMouseActionMenuItems[gGameMouseActionMenuHighlightedItemIndex] < 0
        || gGameMouseActionMenuItems[gGameMouseActionMenuHighlightedItemIndex] >= GAME_MOUSE_ACTION_MENU_ITEM_COUNT) {
        return -1;
    }

    FrmId frmId = kGameMouseActionMenuItemFrmIds[gGameMouseActionMenuItems[gGameMouseActionMenuHighlightedItemIndex]][GAME_MOUSE_ACTION_MENU_ITEM_FRAME_NORMAL];
    Art* art = artLock(frmId, &handle);
    if (art == nullptr) {
        return -1;
    }

    int width = artGetWidth(art);
    int height = artGetHeight(art);
    unsigned char* data = artGetFrameData(art);
    if (data == nullptr || !canBlitActionMenuItem(gGameMouseActionMenuHighlightedItemIndex, width, height)) {
        artUnlock(handle);
        return -1;
    }

    blitBufferToBuffer(data, width, height, width, _gmouse_3d_menu_actions_start + gGameMouseActionMenuFrmWidth * height * gGameMouseActionMenuHighlightedItemIndex, gGameMouseActionMenuFrmWidth);
    artUnlock(handle);

    if (gGameMouseActionMenuItems[menuItemIndex] < 0
        || gGameMouseActionMenuItems[menuItemIndex] >= GAME_MOUSE_ACTION_MENU_ITEM_COUNT) {
        return -1;
    }

    frmId = kGameMouseActionMenuItemFrmIds[gGameMouseActionMenuItems[menuItemIndex]][GAME_MOUSE_ACTION_MENU_ITEM_FRAME_HIGHLIGHTED];
    art = artLock(frmId, &handle);
    if (art == nullptr) {
        return -1;
    }

    width = artGetWidth(art);
    height = artGetHeight(art);
    data = artGetFrameData(art);
    if (data == nullptr || !canBlitActionMenuItem(menuItemIndex, width, height)) {
        artUnlock(handle);
        return -1;
    }

    blitBufferToBuffer(data, width, height, width, _gmouse_3d_menu_actions_start + gGameMouseActionMenuFrmWidth * height * menuItemIndex, gGameMouseActionMenuFrmWidth);
    artUnlock(handle);

    gGameMouseActionMenuHighlightedItemIndex = menuItemIndex;

    return 0;
}

// 0x44D774 gmouse_3d_build_to_hit_frame
int gameMouseRenderAccuracy(const char* string, Color color)
{
    CacheEntry* crosshairFrmHandle;
    const FrmId frmId = FrmId(gGameMouseModeFrmIds[GAME_MOUSE_MODE_CROSSHAIR]);
    Art* crosshairFrm = artLock(frmId, &crosshairFrmHandle);
    if (crosshairFrm == nullptr) {
        return -1;
    }

    memset(gGameMouseActionHitFrmData, 0, gGameMouseActionHitFrmDataSize);

    int crosshairFrmWidth = artGetWidth(crosshairFrm);
    int crosshairFrmHeight = artGetHeight(crosshairFrm);
    unsigned char* crosshairFrmData = artGetFrameData(crosshairFrm);
    blitBufferToBuffer(crosshairFrmData,
        crosshairFrmWidth,
        crosshairFrmHeight,
        crosshairFrmWidth,
        gGameMouseActionHitFrmData,
        gGameMouseActionHitFrmWidth);

    int oldFont = fontGetCurrent();
    fontSetCurrent(101);

    fontDrawText(gGameMouseActionHitFrmData + gGameMouseActionHitFrmWidth + crosshairFrmWidth + 1,
        string,
        gGameMouseActionHitFrmWidth - crosshairFrmWidth,
        gGameMouseActionHitFrmWidth,
        color);

    bufferOutline(gGameMouseActionHitFrmData + crosshairFrmWidth,
        gGameMouseActionHitFrmWidth - crosshairFrmWidth,
        gGameMouseActionHitFrmHeight,
        gGameMouseActionHitFrmWidth,
        COLOR_BLACK);

    fontSetCurrent(oldFont);

    artUnlock(crosshairFrmHandle);

    return 0;
}

// 0x44D878 gmouse_3d_build_hex_frame
int gameMouseRenderActionPoints(const char* string, Color color)
{
    memset(gGameMouseHexCursorFrmData, 0, gGameMouseHexCursorFrmWidth * gGameMouseHexCursorHeight);

    if (*string == '\0') {
        return 0;
    }

    int oldFont = fontGetCurrent();
    fontSetCurrent(101);

    int length = fontGetStringWidth(string);
    fontDrawText(gGameMouseHexCursorFrmData + gGameMouseHexCursorFrmWidth * (gGameMouseHexCursorHeight - fontGetLineHeight()) / 2 + (gGameMouseHexCursorFrmWidth - length) / 2, string, gGameMouseHexCursorFrmWidth, gGameMouseHexCursorFrmWidth, color);

    bufferOutline(gGameMouseHexCursorFrmData, gGameMouseHexCursorFrmWidth, gGameMouseHexCursorHeight, gGameMouseHexCursorFrmWidth, COLOR_BLACK);

    fontSetCurrent(oldFont);

    gameMouseSetBouncingCursorFid(FrmId(InterfaceFrameId::HexMouseCursor).fid());

    return 0;
}

// 0x44D954 gmouse_3d_synch_item_highlight
void gameMouseLoadItemHighlight()
{
    gGameMouseItemHighlightEnabled = settings.preferences.item_highlight;
}

// 0x44D984 gmouse_3d_init
int gameMouseObjectsInit()
{
    if (gGameMouseObjectsInitialized) {
        return -1;
    }

    if (objectCreateWithFidPid(&gGameMouseBouncingCursor, FrmId(InterfaceFrameId::Blank).fid(), -1) != 0) {
        return -1;
    }

    if (objectCreateWithFidPid(&gGameMouseHexCursor, FrmId(InterfaceFrameId::HexMouseCursor).fid(), -1) != 0) {
        return -1;
    }

    if (objectSetOutline(gGameMouseHexCursor, OUTLINE_TYPE_SAME_TEAM | OUTLINE_PALETTED, nullptr) != 0) {
        return -1;
    }

    if (gameMouseActionMenuInit() != 0) {
        return -1;
    }

    gGameMouseBouncingCursor->flags |= OBJECT_LIGHT_THRU;
    gGameMouseBouncingCursor->flags |= OBJECT_NO_SAVE;
    gGameMouseBouncingCursor->flags |= OBJECT_NO_REMOVE;
    gGameMouseBouncingCursor->flags |= OBJECT_SHOOT_THRU;
    gGameMouseBouncingCursor->flags |= OBJECT_NO_BLOCK;

    gGameMouseHexCursor->flags |= OBJECT_NO_REMOVE;
    gGameMouseHexCursor->flags |= OBJECT_NO_SAVE;
    gGameMouseHexCursor->flags |= OBJECT_LIGHT_THRU;
    gGameMouseHexCursor->flags |= OBJECT_SHOOT_THRU;
    gGameMouseHexCursor->flags |= OBJECT_NO_BLOCK;

    _obj_toggle_flat(gGameMouseHexCursor, nullptr);

    int x;
    int y;
    mouseGetPosition(&x, &y);

    Rect v9;
    _gmouse_3d_move_to(x, y, gElevation, &v9);

    gGameMouseObjectsInitialized = true;

    gameMouseLoadItemHighlight();

    return 0;
}

// NOTE: Inlined.
//
// 0x44DAC0 gmouse_3d_reset
int gameMouseObjectsReset()
{
    if (!gGameMouseObjectsInitialized) {
        return -1;
    }

    // NOTE: Uninline.
    _gmouse_3d_enable_modes();

    // NOTE: Uninline.
    gameMouseResetBouncingCursorFid();

    gameMouseSetMode(GAME_MOUSE_MODE_MOVE);
    gameMouseObjectsShow();

    gGameMouseLastX = -1;
    gGameMouseLastY = -1;
    _gmouse_3d_hover_test = false;
    _gmouse_3d_last_move_time = getTicks();
    gameMouseLoadItemHighlight();

    return 0;
}

// NOTE: Inlined.
//
// 0x44DB34 gmouse_3d_exit
void gameMouseObjectsFree()
{
    if (gGameMouseObjectsInitialized) {
        gameMouseActionMenuFree();

        gGameMouseBouncingCursor->flags &= ~OBJECT_NO_SAVE;
        gGameMouseHexCursor->flags &= ~OBJECT_NO_SAVE;

        objectDestroy(gGameMouseBouncingCursor, nullptr);
        objectDestroy(gGameMouseHexCursor, nullptr);

        gGameMouseObjectsInitialized = false;
    }
}

// 0x44DB78 gmouse_3d_lock_frames
int gameMouseActionMenuInit()
{
    gGameMouseActionMenuFrm = artLock(FrmId(InterfaceFrameId::ActionMenu), &gGameMouseActionMenuFrmHandle);
    if (gGameMouseActionMenuFrm == nullptr) {
        goto err;
    }

    gGameMouseActionPickFrm = artLock(FrmId(InterfaceFrameId::ActionPick), &gGameMouseActionPickFrmHandle);
    if (gGameMouseActionPickFrm == nullptr) {
        goto err;
    }

    gGameMouseActionHitFrm = artLock(FrmId(InterfaceFrameId::ActionToHit), &gGameMouseActionHitFrmHandle);
    if (gGameMouseActionHitFrm == nullptr) {
        goto err;
    }

    gGameMouseBouncingCursorFrm = artLock(FrmId(InterfaceFrameId::Blank), &gGameMouseBouncingCursorFrmHandle);
    if (gGameMouseBouncingCursorFrm == nullptr) {
        goto err;
    }

    gGameMouseHexCursorFrm = artLock(FrmId(InterfaceFrameId::HexMouseCursor), &gGameMouseHexCursorFrmHandle);
    if (gGameMouseHexCursorFrm == nullptr) {
        goto err;
    }

    gGameMouseActionMenuFrmWidth = artGetWidth(gGameMouseActionMenuFrm);
    gGameMouseActionMenuFrmHeight = artGetHeight(gGameMouseActionMenuFrm);
    gGameMouseActionMenuFrmDataSize = gGameMouseActionMenuFrmWidth * gGameMouseActionMenuFrmHeight;
    gGameMouseActionMenuFrmData = artGetFrameData(gGameMouseActionMenuFrm);

    gGameMouseActionPickFrmWidth = artGetWidth(gGameMouseActionPickFrm);
    gGameMouseActionPickFrmHeight = artGetHeight(gGameMouseActionPickFrm);
    gGameMouseActionPickFrmDataSize = gGameMouseActionPickFrmWidth * gGameMouseActionPickFrmHeight;
    gGameMouseActionPickFrmData = artGetFrameData(gGameMouseActionPickFrm);

    gGameMouseActionHitFrmWidth = artGetWidth(gGameMouseActionHitFrm);
    gGameMouseActionHitFrmHeight = artGetHeight(gGameMouseActionHitFrm);
    gGameMouseActionHitFrmDataSize = gGameMouseActionHitFrmWidth * gGameMouseActionHitFrmHeight;
    gGameMouseActionHitFrmData = artGetFrameData(gGameMouseActionHitFrm);

    gGameMouseBouncingCursorFrmWidth = artGetWidth(gGameMouseBouncingCursorFrm);
    gGameMouseBouncingCursorFrmHeight = artGetHeight(gGameMouseBouncingCursorFrm);
    gGameMouseBouncingCursorFrmDataSize = gGameMouseBouncingCursorFrmWidth * gGameMouseBouncingCursorFrmHeight;
    gGameMouseBouncingCursorFrmData = artGetFrameData(gGameMouseBouncingCursorFrm);

    gGameMouseHexCursorFrmWidth = artGetWidth(gGameMouseHexCursorFrm);
    gGameMouseHexCursorHeight = artGetHeight(gGameMouseHexCursorFrm);
    gGameMouseHexCursorDataSize = gGameMouseHexCursorFrmWidth * gGameMouseHexCursorHeight;
    gGameMouseHexCursorFrmData = artGetFrameData(gGameMouseHexCursorFrm);

    return 0;

err:

    // NOTE: Original code is different. There is no call to this function.
    // Instead it either use deep nesting or bunch of goto's to unwind
    // locked frms from the point of failure.
    gameMouseActionMenuFree();

    return -1;
}

// 0x44DE44 gmouse_3d_unlock_frames
void gameMouseActionMenuFree()
{
    if (gGameMouseBouncingCursorFrmHandle != INVALID_CACHE_ENTRY) {
        artUnlock(gGameMouseBouncingCursorFrmHandle);
    }
    gGameMouseBouncingCursorFrm = nullptr;
    gGameMouseBouncingCursorFrmHandle = INVALID_CACHE_ENTRY;
    gGameMouseBouncingCursorFrmData = nullptr;
    gGameMouseBouncingCursorFrmWidth = 0;
    gGameMouseBouncingCursorFrmHeight = 0;
    gGameMouseBouncingCursorFrmDataSize = 0;

    if (gGameMouseHexCursorFrmHandle != INVALID_CACHE_ENTRY) {
        artUnlock(gGameMouseHexCursorFrmHandle);
    }
    gGameMouseHexCursorFrm = nullptr;
    gGameMouseHexCursorFrmHandle = INVALID_CACHE_ENTRY;
    gGameMouseHexCursorFrmData = nullptr;
    gGameMouseHexCursorFrmWidth = 0;
    gGameMouseHexCursorHeight = 0;
    gGameMouseHexCursorDataSize = 0;

    if (gGameMouseActionHitFrmHandle != INVALID_CACHE_ENTRY) {
        artUnlock(gGameMouseActionHitFrmHandle);
    }
    gGameMouseActionHitFrm = nullptr;
    gGameMouseActionHitFrmHandle = INVALID_CACHE_ENTRY;
    gGameMouseActionHitFrmData = nullptr;
    gGameMouseActionHitFrmWidth = 0;
    gGameMouseActionHitFrmHeight = 0;
    gGameMouseActionHitFrmDataSize = 0;

    if (gGameMouseActionMenuFrmHandle != INVALID_CACHE_ENTRY) {
        artUnlock(gGameMouseActionMenuFrmHandle);
    }
    gGameMouseActionMenuFrm = nullptr;
    gGameMouseActionMenuFrmHandle = INVALID_CACHE_ENTRY;
    gGameMouseActionMenuFrmData = nullptr;
    gGameMouseActionMenuFrmWidth = 0;
    gGameMouseActionMenuFrmHeight = 0;
    gGameMouseActionMenuFrmDataSize = 0;

    if (gGameMouseActionPickFrmHandle != INVALID_CACHE_ENTRY) {
        artUnlock(gGameMouseActionPickFrmHandle);
    }

    gGameMouseActionPickFrm = nullptr;
    gGameMouseActionPickFrmHandle = INVALID_CACHE_ENTRY;

    gGameMouseActionPickFrmData = nullptr;
    gGameMouseActionPickFrmWidth = 0;
    gGameMouseActionPickFrmHeight = 0;
    gGameMouseActionPickFrmDataSize = 0;
}

// NOTE: Inlined.
//
// 0x44DF1C gmouse_3d_set_flat_fid
static int gmouse_3d_set_flat_fid(int fid, Rect* rect)
{
    if (objectSetFid(gGameMouseHexCursor, fid, rect) == 0) {
        return 0;
    }

    return -1;
}

// 0x44DF40 gmouse_3d_reset_flat_fid
int gameMouseUpdateHexCursorFid(Rect* rect)
{
    const FrmId frmId = FrmId(gGameMouseModeFrmIds[gGameMouseMode]);
    if (gGameMouseHexCursor->fid == frmId.fid()) {
        return -1;
    }

    // NOTE: Uninline.
    return gmouse_3d_set_flat_fid(frmId.fid(), rect);
}

// 0x44DF94 gmouse_3d_move_to
int _gmouse_3d_move_to(int x, int y, int elevation, Rect* rect)
{
    if (_gmouse_mapper_mode == 0) {
        if (gGameMouseMode != GAME_MOUSE_MODE_MOVE) {
            int offsetX = 0;
            int offsetY = 0;
            CacheEntry* hexCursorFrmHandle;
            Art* hexCursorFrm = artLock(gGameMouseHexCursor->fid, &hexCursorFrmHandle);
            if (hexCursorFrm != nullptr) {
                artGetRotationOffsets(hexCursorFrm, ROTATION_NE, &offsetX, &offsetY);

                int frameOffsetX;
                int frameOffsetY;
                artGetFrameOffsets(hexCursorFrm, 0, ROTATION_NE, &frameOffsetX, &frameOffsetY);

                offsetX += frameOffsetX;
                offsetY += frameOffsetY;

                artUnlock(hexCursorFrmHandle);
            }

            _obj_move(gGameMouseHexCursor, x + offsetX, y + offsetY, elevation, rect);
        } else {
            int tile = tileFromScreenXY(x, y);
            if (tile != -1) {
                int screenX;
                int screenY;

                bool v1 = false;
                Rect rect1;
                if (tileToScreenXY(tile, &screenX, &screenY) == 0) {
                    if (_obj_move(gGameMouseBouncingCursor, screenX + 16, screenY + 15, 0, &rect1) == 0) {
                        v1 = true;
                    }
                }

                Rect rect2;
                if (objectSetLocation(gGameMouseHexCursor, tile, elevation, &rect2) == 0) {
                    if (v1) {
                        rectUnion(&rect1, &rect2, &rect1);
                    } else {
                        rectCopy(&rect1, &rect2);
                    }

                    rectCopy(rect, &rect1);
                }
            }
        }
        return 0;
    }

    int tile;
    int x1 = 0;
    int y1 = 0;

    int fid = gGameMouseBouncingCursor->fid;
    if (objectTypeFromFid(fid) == OBJ_TYPE_TILE) {
        int squareTile = squareTileFromScreenXY(x, y, elevation);
        if (squareTile != -1) {
            tile = HEX_GRID_WIDTH * (2 * (squareTile / SQUARE_GRID_WIDTH) + 1) + 2 * (squareTile % SQUARE_GRID_WIDTH) + 1;
            x1 = -8;
            y1 = 13;

            if (settings.system.executableIsMapper() && tileRoofIsVisible() && (gDude->flags & OBJECT_HIDDEN) == OBJECT_NONE) {
                y1 = -83;
            }
        } else {
            tile = -1;
        }
    } else {
        tile = tileFromScreenXY(x, y);
    }

    if (tile != -1) {
        bool v1 = false;

        Rect rect1;
        Rect rect2;

        if (objectSetLocation(gGameMouseBouncingCursor, tile, elevation, &rect1) == 0) {
            if (x1 != 0 || y1 != 0) {
                if (_obj_offset(gGameMouseBouncingCursor, x1, y1, &rect2) == 0) {
                    rectUnion(&rect1, &rect2, &rect1);
                }
            }
            v1 = true;
        }

        if (gGameMouseMode != GAME_MOUSE_MODE_MOVE) {
            int offsetX = 0;
            int offsetY = 0;
            CacheEntry* hexCursorFrmHandle;
            Art* hexCursorFrm = artLock(gGameMouseHexCursor->fid, &hexCursorFrmHandle);
            if (hexCursorFrm != nullptr) {
                artGetRotationOffsets(hexCursorFrm, ROTATION_NE, &offsetX, &offsetY);

                int frameOffsetX;
                int frameOffsetY;
                artGetFrameOffsets(hexCursorFrm, 0, ROTATION_NE, &frameOffsetX, &frameOffsetY);

                offsetX += frameOffsetX;
                offsetY += frameOffsetY;

                artUnlock(hexCursorFrmHandle);
            }

            if (_obj_move(gGameMouseHexCursor, x + offsetX, y + offsetY, elevation, &rect2) == 0) {
                if (v1) {
                    rectUnion(&rect1, &rect2, &rect1);
                } else {
                    rectCopy(&rect1, &rect2);
                    v1 = true;
                }
            }
        } else {
            if (objectSetLocation(gGameMouseHexCursor, tile, elevation, &rect2) == 0) {
                if (v1) {
                    rectUnion(&rect1, &rect2, &rect1);
                } else {
                    rectCopy(&rect1, &rect2);
                    v1 = true;
                }
            }
        }

        if (v1) {
            rectCopy(rect, &rect1);
        }
    }

    return 0;
}

// 0x44E42C gmouse_check_scrolling
int gameMouseHandleScrolling(int x, int y, int cursor)
{
    if (!_gmouse_scrolling_enabled) {
        return -1;
    }

    int flags = 0;

    if (x <= _scr_size.left) {
        flags |= SCROLLABLE_W;
    }

    if (x >= _scr_size.right) {
        flags |= SCROLLABLE_E;
    }

    if (y <= _scr_size.top) {
        flags |= SCROLLABLE_N;
    }

    if (y >= _scr_size.bottom) {
        flags |= SCROLLABLE_S;
    }

    // Click-to-scroll mode (mapper Alt-Z): only scroll while the left button is held at an
    // edge. Track this in _gmouse_clicked_on_edge so callers can react (e.g. suppress the
    // regular 3D cursor update while an edge-click scroll is active).
    // TODO: reconcile with other site where _gmouse_clicked_on_edge is set to true
    if (_gmouse_click_to_scroll) {
        bool atEdge = flags != 0;
        bool leftHeld = (mouseGetEvent() & MOUSE_EVENT_LEFT_BUTTON_REPEAT) != 0;
        _gmouse_clicked_on_edge = atEdge && leftHeld;
        if (!_gmouse_clicked_on_edge) {
            return -1;
        }
    }

    int dx = 0;
    int dy = 0;

    switch (flags) {
    case SCROLLABLE_W:
        dx = -1;
        cursor = MOUSE_CURSOR_SCROLL_W;
        break;
    case SCROLLABLE_E:
        dx = 1;
        cursor = MOUSE_CURSOR_SCROLL_E;
        break;
    case SCROLLABLE_N:
        dy = -1;
        cursor = MOUSE_CURSOR_SCROLL_N;
        break;
    case SCROLLABLE_N | SCROLLABLE_W:
        dx = -1;
        dy = -1;
        cursor = MOUSE_CURSOR_SCROLL_NW;
        break;
    case SCROLLABLE_N | SCROLLABLE_E:
        dx = 1;
        dy = -1;
        cursor = MOUSE_CURSOR_SCROLL_NE;
        break;
    case SCROLLABLE_S:
        dy = 1;
        cursor = MOUSE_CURSOR_SCROLL_S;
        break;
    case SCROLLABLE_S | SCROLLABLE_W:
        dx = -1;
        dy = 1;
        cursor = MOUSE_CURSOR_SCROLL_SW;
        break;
    case SCROLLABLE_S | SCROLLABLE_E:
        dx = 1;
        dy = 1;
        cursor = MOUSE_CURSOR_SCROLL_SE;
        break;
    }

    if (dx == 0 && dy == 0) {
        if (mapEdgeIsOverClippedArea(x, y)) {
            gameMouseObjectsHide();
            gameMouseSetCursor(MOUSE_CURSOR_ARROW);
            return 0;
        }
        return -1;
    }

    int rc = mapScroll(dx, dy);
    switch (rc) {
    case -1:
        // Scrolling is blocked for whatever reason, upgrade cursor to
        // appropriate blocked version.
        cursor += 8;
        // FALLTHROUGH
    case 0:
        gameMouseSetCursor(cursor);
        break;
    }

    return 0;
}

// 0x44E544 gmouse_remove_item_outline
void _gmouse_remove_item_outline(Object* object)
{
    if (gGameMouseHighlightedItem != nullptr && gGameMouseHighlightedItem == object) {
        Rect rect;
        if (objectClearOutline(object, &rect) == 0) {
            tileWindowRefreshRect(&rect, gElevation);
        }
        gGameMouseHighlightedItem = nullptr;
    }
}

// 0x44E580 gmObjIsValidTarget
int objectIsDoor(Object* object)
{
    if (object == nullptr) {
        return false;
    }

    if (objectTypeFromPid(object->pid) != OBJ_TYPE_SCENERY) {
        return false;
    }

    Proto* proto;
    if (protoGetProto(object->pid, &proto) == -1) {
        return false;
    }

    return proto->scenery.type == SCENERY_TYPE_DOOR;
}

static void customMouseModeFrmsInit()
{
    configGetEnum<InterfaceFrameId>(&gContentConfig, CONTENT_CONFIG_SKILLDEX_SECTION, "first_aid", &(gGameMouseModeFrmIds[GAME_MOUSE_MODE_USE_FIRST_AID]), InterfaceFrameId::MouseCursorUseCrosshair);
    configGetEnum<InterfaceFrameId>(&gContentConfig, CONTENT_CONFIG_SKILLDEX_SECTION, "doctor", &(gGameMouseModeFrmIds[GAME_MOUSE_MODE_USE_DOCTOR]), InterfaceFrameId::MouseCursorUseCrosshair);
    configGetEnum<InterfaceFrameId>(&gContentConfig, CONTENT_CONFIG_SKILLDEX_SECTION, "lockpick", &(gGameMouseModeFrmIds[GAME_MOUSE_MODE_USE_LOCKPICK]), InterfaceFrameId::MouseCursorUseCrosshair);
    configGetEnum<InterfaceFrameId>(&gContentConfig, CONTENT_CONFIG_SKILLDEX_SECTION, "steal", &(gGameMouseModeFrmIds[GAME_MOUSE_MODE_USE_STEAL]), InterfaceFrameId::MouseCursorUseCrosshair);
    configGetEnum<InterfaceFrameId>(&gContentConfig, CONTENT_CONFIG_SKILLDEX_SECTION, "traps", &(gGameMouseModeFrmIds[GAME_MOUSE_MODE_USE_TRAPS]), InterfaceFrameId::MouseCursorUseCrosshair);
    configGetEnum<InterfaceFrameId>(&gContentConfig, CONTENT_CONFIG_SKILLDEX_SECTION, "science", &(gGameMouseModeFrmIds[GAME_MOUSE_MODE_USE_SCIENCE]), InterfaceFrameId::MouseCursorUseCrosshair);
    configGetEnum<InterfaceFrameId>(&gContentConfig, CONTENT_CONFIG_SKILLDEX_SECTION, "repair", &(gGameMouseModeFrmIds[GAME_MOUSE_MODE_USE_REPAIR]), InterfaceFrameId::MouseCursorUseCrosshair);
}

void gameMouseRefreshImmediately()
{
    gameMouseRefresh();
    renderPresent();
}

Object* gmouse_get_outlined_object()
{
    return gGameMouseHighlightedItem;
}

} // namespace fallout
