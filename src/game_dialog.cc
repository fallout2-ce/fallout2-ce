#include "game_dialog.h"

#include <algorithm>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <unordered_map>

#include "actions.h"
#include "animation.h"
#include "art.h"
#include "color.h"
#include "combat.h"
#include "combat_ai.h"
#include "content_config.h"
#include "critter.h"
#include "cycle.h"
#include "debug.h"
#include "delay.h"
#include "dialog.h"
#include "display_monitor.h"
#include "draw.h"
#include "game.h"
#include "game_mouse.h"
#include "game_sound.h"
#include "input.h"
#include "interface.h"
#include "inventory.h"
#include "item.h"
#include "kb.h"
#include "lips.h"
#include "map.h"
#include "memory.h"
#include "mouse.h"
#include "object.h"
#include "party_member.h"
#include "perk.h"
#include "proto.h"
#include "queue.h"
#include "random.h"
#include "scripts.h"
#include "settings.h"
#include "skill.h"
#include "stat.h"
#include "svga.h"
#include "text_font.h"
#include "text_object.h"
#include "tile.h"
#include "touch.h"
#include "window_manager.h"

namespace fallout {

#define DIALOG_REVIEW_ENTRIES_CAPACITY 80

#define DIALOG_OPTION_ENTRIES_CAPACITY 30

#define GAME_DIALOG_WINDOW_WIDTH 640
#define GAME_DIALOG_WINDOW_HEIGHT 480

#define GAME_DIALOG_REPLY_WINDOW_X 135
#define GAME_DIALOG_REPLY_WINDOW_Y 225
#define GAME_DIALOG_REPLY_WINDOW_WIDTH 379
#define GAME_DIALOG_REPLY_WINDOW_HEIGHT 58

#define GAME_DIALOG_OPTIONS_WINDOW_X 127
#define GAME_DIALOG_OPTIONS_WINDOW_Y 335
#define GAME_DIALOG_OPTIONS_WINDOW_WIDTH 393
#define GAME_DIALOG_OPTIONS_WINDOW_HEIGHT 117

#define GAME_DIALOG_REVIEW_WINDOW_WIDTH 640
#define GAME_DIALOG_REVIEW_WINDOW_HEIGHT 480

typedef enum GameDialogReviewWindowButton {
    GAME_DIALOG_REVIEW_WINDOW_BUTTON_SCROLL_UP,
    GAME_DIALOG_REVIEW_WINDOW_BUTTON_SCROLL_DOWN,
    GAME_DIALOG_REVIEW_WINDOW_BUTTON_DONE,
    GAME_DIALOG_REVIEW_WINDOW_BUTTON_COUNT,
} GameDialogReviewWindowButton;

typedef enum GameDialogReviewWindowButtonFrm {
    GAME_DIALOG_REVIEW_WINDOW_BUTTON_FRM_ARROW_UP_NORMAL,
    GAME_DIALOG_REVIEW_WINDOW_BUTTON_FRM_ARROW_UP_PRESSED,
    GAME_DIALOG_REVIEW_WINDOW_BUTTON_FRM_ARROW_DOWN_NORMAL,
    GAME_DIALOG_REVIEW_WINDOW_BUTTON_FRM_ARROW_DOWN_PRESSED,
    GAME_DIALOG_REVIEW_WINDOW_BUTTON_FRM_DONE_NORMAL,
    GAME_DIALOG_REVIEW_WINDOW_BUTTON_FRM_DONE_PRESSED,
    GAME_DIALOG_REVIEW_WINDOW_BUTTON_FRM_COUNT,
} GameDialogReviewWindowButtonFrm;

static void partyMemberCustomizationMessageListReset();

typedef enum GameDialogReaction {
    GAME_DIALOG_REACTION_GOOD = 49,
    GAME_DIALOG_REACTION_NEUTRAL = 50,
    GAME_DIALOG_REACTION_BAD = 51,
} GameDialogReaction;

// `dialogMode` tracks the active dialog screen, while
// `dialogSwitchMode` carries pending and in-progress transitions between
// these screens. The flow is: talk -> barter or party control ->
// party customization -> control -> talk, with `dialogSwitchMode` used to
// request and finalize those transitions on the ticker/main-loop boundary.
typedef enum GameDialogMode {
    GAME_DIALOG_MODE_NONE = 0,

    // possible values for dialogMode
    GAME_DIALOG_MODE_TALK = 1,
    GAME_DIALOG_MODE_BARTER = 4,
    GAME_DIALOG_MODE_PARTY_CONTROL = 10,
    GAME_DIALOG_MODE_PARTY_CUSTOMIZATION = 13,

    // possible values for dialogSwitchMode (in addition to TALK)
    GAME_DIALOG_MODE_SWITCH_TO_BARTER = 2,
    GAME_DIALOG_MODE_BARTER_ACTIVE = 3,
    GAME_DIALOG_MODE_SWITCH_TO_PARTY_CONTROL = 8,
    GAME_DIALOG_MODE_PARTY_CONTROL_ACTIVE = 9,
    GAME_DIALOG_MODE_SWITCH_TO_PARTY_CUSTOMIZATION = 11,
    GAME_DIALOG_MODE_PARTY_CUSTOMIZATION_ACTIVE = 12,
} GameDialogMode;

typedef enum GameDialogStatus {
    GAME_DIALOG_NONE = -1,
    GAME_DIALOG_INACTIVE = 0,
    GAME_DIALOG_ACTIVE = 1,
} GameDialogStatus;

typedef struct GameDialogReviewEntry {
    int replyMessageListId;
    int replyMessageId;
    // Can be NULL.
    char* replyText;
    int optionMessageListId;
    int optionMessageId;
    char* optionText;
} GameDialogReviewEntry;

typedef struct GameDialogOptionEntry {
    int messageListId;
    int messageId;
    int reaction;
    int proc;
    int btn;
    int top;
    char text[900];
    int bottom;
} GameDialogOptionEntry;

// Provides button configuration for party member combat control and
// customization interface.
typedef struct GameDialogButtonData {
    int x;
    int y;
    InterfaceFrameId upFrmId;
    InterfaceFrameId downFrmId;
    InterfaceFrameId disabledFrmId;
    CacheEntry* upFrmHandle;
    CacheEntry* downFrmHandle;
    CacheEntry* disabledFrmHandle;
    int keyCode;
    int value;
} GameDialogButtonData;

struct PartyMemberOptionSetting {
    int messageId;
    union {
        AreaAttackMode areaAttackMode;
        RunAwayMode runAwayMode;
        BestWeapon bestWeapon;
        DistanceMode distanceMode;
        AttackWho attackWho;
        ChemUse chemUse;
    };

    PartyMemberOptionSetting(int messageId, AreaAttackMode areaAttackMode)
        : messageId(messageId)
        , areaAttackMode(areaAttackMode)
    {
    }
    PartyMemberOptionSetting(int messageId, RunAwayMode runAwayMode)
        : messageId(messageId)
        , runAwayMode(runAwayMode)
    {
    }
    PartyMemberOptionSetting(int messageId, BestWeapon bestWeapon)
        : messageId(messageId)
        , bestWeapon(bestWeapon)
    {
    }
    PartyMemberOptionSetting(int messageId, DistanceMode distanceMode)
        : messageId(messageId)
        , distanceMode(distanceMode)
    {
    }
    PartyMemberOptionSetting(int messageId, AttackWho attackWho)
        : messageId(messageId)
        , attackWho(attackWho)
    {
    }
    PartyMemberOptionSetting(int messageId, ChemUse chemUse)
        : messageId(messageId)
        , chemUse(chemUse)
    {
    }
};

typedef enum PartyMemberCustomizationOption {
    PARTY_MEMBER_CUSTOMIZATION_OPTION_AREA_ATTACK_MODE,
    PARTY_MEMBER_CUSTOMIZATION_OPTION_RUN_AWAY_MODE,
    PARTY_MEMBER_CUSTOMIZATION_OPTION_BEST_WEAPON,
    PARTY_MEMBER_CUSTOMIZATION_OPTION_DISTANCE,
    PARTY_MEMBER_CUSTOMIZATION_OPTION_ATTACK_WHO,
    PARTY_MEMBER_CUSTOMIZATION_OPTION_CHEM_USE,
    PARTY_MEMBER_CUSTOMIZATION_OPTION_COUNT,
} PartyMemberCustomizationOption;

// 0x444D10 Dogs
static int _Dogs[3] = {
    0x1000088,
    0x1000156,
    0x1000180,
};

static std::unordered_map<int, AiMessageRange> partyMemberCcMsgIds;

// 0x5186D4 dialog_state_fix
static int _dialog_state_fix = 0;

// 0x5186D8 gdNumOptions
static int gameDialogOptionEntriesLength = 0;

// 0x5186DC curReviewSlot
static int gameDialogReviewEntriesLength = 0;

// 0x5186E0 headWindowBuffer
static unsigned char* gameDialogDisplayBuffer = nullptr;

// 0x5186E4 gReplyWin
static int gameDialogReplyWindow = -1;

// 0x5186E8 gOptionWin
static int gameDialogOptionsWindow = -1;

// 0x5186EC gdialog_window_created
static bool _gdialog_window_created = false;

// 0x5186F0 boxesWereDisabled
static int _boxesWereDisabled = 0;

// 0x5186F4 fidgetFID
static HeadFrmId gameDialogFidgetFrmId = HeadFrameId::None;

// 0x5186F8 fidgetKey
static CacheEntry* gameDialogFidgetFrmHandle = nullptr;

// 0x5186FC fidgetFp
static Art* gameDialogFidgetFrm = nullptr;

// 0x518700 backgroundFrmId
static BackgroundFrmId gameDialogBackgroundFrmId = BackgroundFrameId::RustyMetal;

// 0x518704 lipsFID
static HeadFrmId _lipsFrmId = HeadFrameId::None;

// 0x518708 lipsKey
static CacheEntry* _lipsKey = nullptr;

// 0x51870C lipsFp
static Art* _lipsFp = nullptr;

// 0x518710 gdialog_speech_playing
static bool gameDialogLipSyncStarted = false;

// 0x518714
// what dialog mode is active (talk/barter/party control/party customization)
static GameDialogMode dialogMode = GAME_DIALOG_MODE_NONE;

// 0x518718
// what dialog mode are we switching to
static GameDialogMode dialogSwitchMode = GAME_DIALOG_MODE_NONE;

// 0x51871C
// whether the dialog system is active or not
static GameDialogStatus _gdialog_state = GAME_DIALOG_NONE;

// 0x518720 gdDialogWentOff
static bool _gdDialogWentOff = false;

// 0x518724 gdDialogTurnMouseOff
static bool _gdDialogTurnMouseOff = false;

// 0x518728 gdReenterLevel
static int _gdReenterLevel = 0;

// 0x51872C gdReplyTooBig
static bool _gdReplyTooBig = false;

// CE: When a dialog option's script proc triggers the barter screen *before*
// setting its Reply() text (e.g. Tubby, Buck Dunton), the reply's speech and
// lip-sync would otherwise start immediately and play out over the barter
// window while the matching on-screen text stays hidden behind it. When true,
// the reply speech was held back in `_gdProcessUpdate` and is started once the
// barter screen closes so audio, lips, and text line up.
static bool gameDialogDeferReplySpeech = false;

// A hidden object (PID -1) created during barter to serve as a container for player items offered to the NPC.
//
// 0x518730 peon_table_obj
static Object* gameDialogPlayerTableObj = nullptr;

// A hidden object (PID -1) created during barter to serve as a container for NPC items requested by player.
//
// 0x518734 barterer_table_obj
static Object* gameDialogBartererTableObj = nullptr;

// A hidden object (PID -1) created during barter.
// TODO: find use or remove it
// 0x518738 barterer_temp_obj
static Object* _barterer_temp_obj = nullptr;

// A barter price modifier set by gdialog_set_barter_mod, in percents.
//
// 0x51873C gdBarterMod
static int gameDialogBarterModifier = 0;

// dialogueBackWindow
// 0x518740 dialogueBackWindow
static int gameDialogBackgroundWindow = -1;

// Dialog sub-window: barter, party control, customization
// 0x518744 dialogueWindow
static int gameDialogWindow = -1;

static bool gameDialogUseHrArt = false;

// 0x518748 backgrndRects
static Rect _backgrndRects[8] = {
    { 126, 14, 152, 40 },
    { 488, 14, 514, 40 },
    { 126, 188, 152, 214 },
    { 488, 188, 514, 214 },
    { 152, 14, 488, 24 },
    { 152, 204, 488, 214 },
    { 126, 40, 136, 188 },
    { 504, 40, 514, 188 },
};

static bool gameDialogShouldUseHrArt()
{
    return settings.ui.dialog_border
        && (screenGetWidth() > GAME_DIALOG_WINDOW_WIDTH || screenGetHeight() > GAME_DIALOG_WINDOW_HEIGHT);
}

static int gameDialogHrArtYOffset()
{
    return gameDialogUseHrArt ? 5 : 0;
}

static Rect gameDialogGetBackgroundRect(int index)
{
    Rect rect = _backgrndRects[index];
    int yOffset = gameDialogHrArtYOffset();
    rect.top += yOffset;
    rect.bottom += yOffset;
    return rect;
}

static int gameDialogGetBackgroundWindowY()
{
    // center on play area if large enough, else center on screen
    int visibleHeight = screenGetVisibleHeight();
    if (visibleHeight >= GAME_DIALOG_WINDOW_HEIGHT) {
        return (visibleHeight - GAME_DIALOG_WINDOW_HEIGHT) / 2;
    }

    return (screenGetHeight() - GAME_DIALOG_WINDOW_HEIGHT) / 2;
}

// 0x5187CC can_start_new_fidget
static bool _can_start_new_fidget = false;

// 0x5187D0 gd_replyWin
static int _gd_replyWin = -1;

// 0x5187D4 gd_optionsWin
static int _gd_optionsWin = -1;

// 0x5187D8 gDialogMusicVol
static int gameDialogOldMusicVolume = -1;

// 0x5187DC gdCenterTile
static int gameDialogOldCenterTile = -1;

// 0x5187E0 gdPlayerTile
static int gameDialogOldDudeTile = -1;

// 0x5187E4 light_BlendTable
static Color* _light_BlendTable = nullptr;

// 0x5187E8 dark_BlendTable
static Color* _dark_BlendTable = nullptr;

// 0x5187EC dialogue_just_started
static int _dialogue_just_started = 0;

// 0x5187F0 dialogue_seconds_since_last_input
static int _dialogue_seconds_since_last_input = 0;

// 0x518818 reviewFidWids
static const int gameDialogReviewWindowButtonWidths[GAME_DIALOG_REVIEW_WINDOW_BUTTON_COUNT] = {
    35,
    35,
    82,
};

// 0x518824 reviewFidLens
static const int gameDialogReviewWindowButtonHeights[GAME_DIALOG_REVIEW_WINDOW_BUTTON_COUNT] = {
    35,
    37,
    46,
};

// 0x518830 reviewFids
static constexpr InterfaceFrmId kGameDialogReviewWindowButtonFrmIds[GAME_DIALOG_REVIEW_WINDOW_BUTTON_FRM_COUNT] = {
    InterfaceFrameId::DialogBigUpArrowUp,
    InterfaceFrameId::DialogBigUpArrowDown,
    InterfaceFrameId::DialogBigDownArrowUp,
    InterfaceFrameId::DialogBigDownArrowDown,
    InterfaceFrameId::DialogBigDoneButtonUp,
    InterfaceFrameId::DialogBigDoneButtonDown,
};

// 0x518848 dialog_target
Object* gGameDialogSpeaker = nullptr;

// 0x51884C dialog_target_is_party
bool gGameDialogSpeakerIsPartyMember = false;

// 0x518850 dialogue_head
HeadFrmId gGameDialogHeadFrmId = HeadFrameId::None;

// 0x518854 dialogue_scr_id
int gGameDialogSid = -1;

// Maps phoneme to talking head frame.
//
// 0x518858 head_phoneme_lookup
static int _head_phoneme_lookup[PHONEME_COUNT] = {
    0,
    3,
    1,
    1,
    3,
    1,
    1,
    1,
    7,
    8,
    7,
    3,
    1,
    8,
    1,
    7,
    7,
    6,
    6,
    2,
    2,
    2,
    2,
    4,
    4,
    5,
    5,
    2,
    2,
    2,
    2,
    2,
    6,
    2,
    2,
    5,
    8,
    2,
    2,
    2,
    2,
    8,
};

// 0x518900 phone_anim
static HeadAnimation _phone_anim = HeadAnimation::VeryGoodReaction;

// 0x518904 loop_cnt
static int _loop_cnt = -1;

// 0x518908 tocksWaiting
static unsigned int _tocksWaiting = 10000;

// 0x51890C react_strs
static const char* _react_strs[3] = {
    "Said Good",
    "Said Neutral",
    "Said Bad",
};

// 0x518918 dialogue_subwin_len
static int _dialogue_subwin_len = 0;

// Extra pixels the expanded barter frame adds below the 480-tall dialog background.
constexpr int kExpandedBarterExtraHeight = kExpandedBarterExtraSlots * INVENTORY_SLOT_HEIGHT;

// Set to true by gameDialogCreateBarterWindow when the expanded FRM loaded
// successfully; reset to false at the end of gameDialogDestroyBarterWindow.
// This is the single source of truth used by Destroy and inventory setup.
static bool barterWindowExpanded = false;

// Cached FRM availability; set once per dialog session in talk_to_create_background_window.
static bool expandedBarterEnabled = false;

// 0x51891C control_button_info
static GameDialogButtonData gameDialogDispositionButtonsData[5] = {
    { 438, 37, InterfaceFrameId::PartyBerserkUp, InterfaceFrameId::PartyBerserkDown, InterfaceFrameId::PartyBerserkDisabled, nullptr, nullptr, nullptr, 2098, 4 },
    { 438, 67, InterfaceFrameId::PartyAggressiveUp, InterfaceFrameId::PartyAggressiveDown, InterfaceFrameId::PartyAggressiveDisabled, nullptr, nullptr, nullptr, 2103, 3 },
    { 438, 96, InterfaceFrameId::PartyDefensiveUp, InterfaceFrameId::PartyDefensiveDown, InterfaceFrameId::PartyDefensiveDisabled, nullptr, nullptr, nullptr, 2102, 2 },
    { 438, 126, InterfaceFrameId::PartyCowardUp, InterfaceFrameId::PartyCowardDown, InterfaceFrameId::PartyCowardDisabled, nullptr, nullptr, nullptr, 2111, 1 },
    { 438, 156, InterfaceFrameId::PartyCustomUp, InterfaceFrameId::PartyCustomDown, InterfaceFrameId::PartyCustomDisabled, nullptr, nullptr, nullptr, 2099, 0 },
};

// 0x5189E4 custom_settings
static PartyMemberOptionSetting _custom_settings[PARTY_MEMBER_CUSTOMIZATION_OPTION_COUNT][6] = {
    {
        PartyMemberOptionSetting(100, AREA_ATTACK_MODE_ALWAYS), // Always!
        PartyMemberOptionSetting(101, AREA_ATTACK_MODE_SOMETIMES), // Sometimes, don't worry about hitting me
        PartyMemberOptionSetting(102, AREA_ATTACK_MODE_BE_SURE), // Be sure you won't hit me
        PartyMemberOptionSetting(103, AREA_ATTACK_MODE_BE_CAREFUL), // Be careful not to hit me
        PartyMemberOptionSetting(104, AREA_ATTACK_MODE_BE_ABSOLUTELY_SURE), // Be absolutely sure you won't hit me
        PartyMemberOptionSetting(-1, AREA_ATTACK_MODE_FIRST),
    },
    {
        // runAwayMode values are shifted by one here by purpose
        PartyMemberOptionSetting(200, RUN_AWAY_MODE_NONE), // Abject coward
        PartyMemberOptionSetting(201, RUN_AWAY_MODE_COWARD), // Your finger hurts
        PartyMemberOptionSetting(202, RUN_AWAY_MODE_FINGER_HURTS), // You're bleeding a bit
        PartyMemberOptionSetting(203, RUN_AWAY_MODE_BLEEDING), // Not feeling good
        PartyMemberOptionSetting(204, RUN_AWAY_MODE_NOT_FEELING_GOOD), // You need a tourniquet
        PartyMemberOptionSetting(205, RUN_AWAY_MODE_TOURNIQUET), // Never!
    },
    {
        PartyMemberOptionSetting(300, BEST_WEAPON_NO_PREF), // None
        PartyMemberOptionSetting(301, BEST_WEAPON_MELEE), // Melee
        PartyMemberOptionSetting(302, BEST_WEAPON_MELEE_OVER_RANGED), // Melee then ranged
        PartyMemberOptionSetting(303, BEST_WEAPON_RANGED_OVER_MELEE), // Ranged then melee
        PartyMemberOptionSetting(304, BEST_WEAPON_RANGED), // Ranged
        PartyMemberOptionSetting(305, BEST_WEAPON_UNARMED), // Unarmed
    },
    {
        PartyMemberOptionSetting(400, DISTANCE_STAY_CLOSE), // Stay close to me
        PartyMemberOptionSetting(401, DISTANCE_CHARGE), // Charge!
        PartyMemberOptionSetting(402, DISTANCE_SNIPE), // Snipe the enemy
        PartyMemberOptionSetting(403, DISTANCE_ON_YOUR_OWN), // On your own
        PartyMemberOptionSetting(404, DISTANCE_STAY), // Say where you are
        PartyMemberOptionSetting(-1, DISTANCE_FIRST),
    },
    {
        PartyMemberOptionSetting(500, ATTACK_WHO_WHOMEVER_ATTACKING_ME), // Whomever is attacking me
        PartyMemberOptionSetting(501, ATTACK_WHO_STRONGEST), // The strongest
        PartyMemberOptionSetting(502, ATTACK_WHO_WEAKEST), // The weakest
        PartyMemberOptionSetting(503, ATTACK_WHO_WHOMEVER), // Whomever you want
        PartyMemberOptionSetting(504, ATTACK_WHO_CLOSEST), // Whoever is closest
        PartyMemberOptionSetting(-1, ATTACK_WHO_FIRST),
    },
    {
        PartyMemberOptionSetting(600, CHEM_USE_CLEAN), // I'm clean
        PartyMemberOptionSetting(601, CHEM_USE_STIMS_WHEN_HURT_LITTLE), // Stimpaks when hurt a bit
        PartyMemberOptionSetting(602, CHEM_USE_STIMS_WHEN_HURT_LOTS), // Stimpaks when hurt a lot
        PartyMemberOptionSetting(603, CHEM_USE_SOMETIMES), // Any drug some of the time
        PartyMemberOptionSetting(604, CHEM_USE_ANYTIME), // Any drug any time
        PartyMemberOptionSetting(-1, CHEM_USE_FIRST),
    },
};

// 0x518B04 custom_button_info
static GameDialogButtonData _custom_button_info[PARTY_MEMBER_CUSTOMIZATION_OPTION_COUNT] = {
    { 95, 9, InterfaceFrameId::PartyBurstUp, InterfaceFrameId::PartyBurstDown, InterfaceFrameId::Invalid, nullptr, nullptr, nullptr, 0, 0 },
    { 96, 38, InterfaceFrameId::PartyRunUp, InterfaceFrameId::PartyRunDown, InterfaceFrameId::Invalid, nullptr, nullptr, nullptr, 1, 0 },
    { 96, 68, InterfaceFrameId::PartyWeaponUp, InterfaceFrameId::PartyWeaponDown, InterfaceFrameId::Invalid, nullptr, nullptr, nullptr, 2, 0 },
    { 96, 98, InterfaceFrameId::PartyDistanceUp, InterfaceFrameId::PartyDistanceDown, InterfaceFrameId::Invalid, nullptr, nullptr, nullptr, 3, 0 },
    { 96, 127, InterfaceFrameId::PartyAttackUp, InterfaceFrameId::PartyAttackDown, InterfaceFrameId::Invalid, nullptr, nullptr, nullptr, 4, 0 },
    { 96, 157, InterfaceFrameId::PartyChemUp, InterfaceFrameId::PartyChemDown, InterfaceFrameId::Invalid, nullptr, nullptr, nullptr, 5, 0 },
};

// 0x518BF4 totalHotx
static int _totalHotx = 0;

// 0x58EA80 custom_current_selected
static int _custom_current_selected[PARTY_MEMBER_CUSTOMIZATION_OPTION_COUNT];

// custom.msg
//
// 0x58EA98 custom_msg_file
static MessageList customMessageList;

// 0x58EAA0 light_GrayTable
static Color _light_GrayTable[COLOR_COUNT];

// 0x58EBA0 dark_GrayTable
static Color _dark_GrayTable[COLOR_COUNT];

// 0x58ECA0 backgrndBufs
static unsigned char* _backgrndBufs[8];

// 0x58ECC0 optionRect
static Rect _optionRect;

// 0x58ECD0 replyRect
static Rect _replyRect;

// 0x58ECE0 reviewList
static GameDialogReviewEntry dialogReviewEntries[DIALOG_REVIEW_ENTRIES_CAPACITY];

// 0x58F460 custom_buttons_start
static int _custom_buttons_start;

// 0x58F464 control_buttons_start
static int _control_buttons_start;

// 0x58F468 reviewOldFont
static int gameDialogReviewWindowOldFont;

// 0x58F470 gdialog_buttons
static int _gdialog_buttons[9];

static void partyMemberCustomizationMessageListReset()
{
    messageListRepositorySetStandardMessageList(STANDARD_MESSAGE_LIST_CUSTOM, nullptr);
    messageListFree(&customMessageList);
}

// 0x58F4C8 oldFont
static int _oldFont;

// 0x58F4CC fidgetLastTime
static unsigned int gameDialogFidgetLastUpdateTimestamp;

// 0x58F4D0 fidgetAnim
static HeadFidget gameDialogFidgetReaction;

// 0x58F4D4 dialogBlock
static Program* dialogReplyProgram;

// 0x58F4D8
static int dialogReplyMessageListId;

// 0x58F4DC
static int dialogReplyMessageId;

// 0x58F4E0
static int dialogReplyTextOffset;

// NOTE: The is something odd about this variable. There are 2700 bytes, which
// is 3 x 900, but anywhere in the app only 900 characters is used. The length
// of text in [DialogOptionEntry] is definitely 900 bytes. There are two
// possible explanations:
// - it's an array of 3 elements
// - there are three separate elements, two of which are not used, therefore
// they are not referenced anywhere, but they take up their space.
//
// See `_gdProcessChoice` for more info how this unreferenced range plays
// important role.
//
// 0x58F4E4 destination
static char dialogReplyText[900];

// 0x58FF70
static GameDialogOptionEntry dialogOptionEntries[DIALOG_OPTION_ENTRIES_CAPACITY];

// 0x596C30 talkOldFont
static int _talkOldFont;

// 0x596C34 fidgetTocksPerFrame
static unsigned int gameDialogFidgetUpdateDelay;

// 0x596C38 fidgetFrameCounter
static int gameDialogFidgetFrmCurrentFrame;

static FrmImage _reviewBackgroundFrmImage;
static FrmImage _reviewFrmImages[GAME_DIALOG_REVIEW_WINDOW_BUTTON_FRM_COUNT];

static FrmImage _lowerHighlightFrmImage;
static FrmImage _upperHighlightFrmImage;
static FrmImage _barterBackgroundFrmImage;

static int _gdialogReset();
static void gameDialogEndLips();
static void gameDialogRestoreCenterTile();
static int gdHide();
static int gdUnhide();
static int gameDialogAddMessageOption(int messageListId, int messageId, int reaction);
static int gameDialogAddTextOption(int messageListId, const char* text, int reaction);
static int gameDialogReviewWindowInit(int* win);
static int gameDialogReviewWindowFree(int* win);
static int gameDialogShowReview();
static void gameDialogReviewButtonOnMouseUp(int btn, int keyCode);
static void gameDialogReviewWindowUpdate(int win, int origin);
static void dialogReviewEntriesClear();
static int gameDialogAddReviewMessage(int messageListId, int messageId);
static int gameDialogAddReviewText(const char* text);
static int gameDialogSetReviewOptionMessage(int messageListId, int messageId);
static int gameDialogSetReviewOptionText(const char* text);
static int _gdProcessInit();
static void _gdProcessCleanup();
static int _gdProcessExit();
static void gameDialogRenderCaps();
static int gameDialogProcessUI();
static int _gdProcessChoice(int optionIndex);
static void gameDialogOptionOnMouseEnter(int optionIndex);
static void gameDialogOptionOnMouseExit(int optionIndex);
static void gameDialogRenderReply();
static void _gdProcessUpdate();
static int _gdCreateHeadWindow();
static void _gdDestroyHeadWindow();
static void _gdSetupFidget(const HeadFrmId& headFrmId, HeadFidget reaction);
static void gameDialogBlitIsoWindowToDisplayBuffer();
static void gameDialogRenderMapInDisplayBuffer();
static void gameDialogWaitForFidgetToComplete();
static void _gdPlayTransition(HeadAnimation animation);
static void _reply_arrow_up(int btn, int keyCode);
static void _reply_arrow_down(int btn, int keyCode);
static void _reply_arrow_restore(int btn, int keyCode);
static void _demo_copy_title(int win);
static void _demo_copy_options(int win);
static void _gDialogRefreshOptionsRect(int win, Rect* drawRect);
static void gameDialogTicker();
// Animates scroll up or down of a given dialog sub-window.
// If scrolling up - only uses subWindowFrmData to gradually fill the window (must be pre-filled with bg window contents).
// If scrolling down - uses both subWindowFrmData and bgWindowBuf to fill parts of window buffer.
static void _gdialog_scroll_subwin(int windowIdx, bool scrollUp, const unsigned char* subWindowFrmData, unsigned char* windowBuf, const unsigned char* bgWindowBuf, int bgWindowHeight, bool instantScrollUp = false);
static int _text_num_lines(const char* text, int maxWidth);
static int text_to_rect_wrapped(unsigned char* buffer, Rect* rect, const char* string, int* textOffset, int height, int pitch, ColorWithFlags color);
static int gameDialogDrawText(unsigned char* buffer, Rect* rect, const char* string, int* textOffset, int height, int pitch, ColorWithFlags color, int draw);
static int gameDialogCreateBarterWindow();
static void gameDialogDestroyBarterWindow();
static void gameDialogBarterCleanupTables();
static int partyMemberControlWindowInit();
static void partyMemberControlWindowFree();
static void partyMemberControlWindowUpdate();
static void gameDialogCombatControlButtonOnMouseUp(int btn, int keyCode);
static int _gdPickAIUpdateMsg(Object* critter);
static int _gdCanBarter();
static void partyMemberControlWindowHandleEvents();
static int partyMemberCustomizationWindowInit();
static void partyMemberCustomizationWindowFree();
static void partyMemberCustomizationWindowHandleEvents();
static void partyMemberCustomizationWindowUpdate();
static void _gdCustomSelectRedraw(unsigned char* dest, int pitch, int type, int selectedIndex);
static int _gdCustomSelect(int option);
static void _gdCustomUpdateSetting(int option, int value);
static void gameDialogBarterButtonUpMouseUp(int btn, int keyCode);
static int createDialogRedButton(int win, int x, int y, void (*mouseUp)(int, int), int keyCode = -1);
static int _gdialog_window_create();
static void _gdialog_window_destroy();
static int talk_to_create_background_window();
static int gameDialogWindowRenderBackground();
static int _talkToRefreshDialogWindowRect(Rect* rect);
static void gameDialogRenderHighlight(unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* dest, int x, int y, int destPitch, Color* blendTable, Color* unusedGrayTable);
static const char* expandedBarterFrmName();
static void gameDialogRenderTalkingHead(Art* headFrm, int frame);
static void gameDialogHighlightsInit();
static void gameDialogHighlightsExit();

static bool gameDialogFix;
static bool numberOptions;

// gdialog_init
// 0x444D1C
int gameDialogInit()
{
    // SFALL: Prevents from using 0 to escape from dialogue at any time.
    gameDialogFix = true;
    configGetBool(&gContentConfig, CONTENT_CONFIG_DIALOG_SECTION, "no_exit_hotkey", &gameDialogFix);

    // SFALL: Use numbers for replies (instead of default knobs).
    numberOptions = settings.ui.numbers_in_dialogue;

    return 0;
}

// 0x444D20
int _gdialogReset()
{
    gameDialogEndLips();
    return 0;
}

// NOTE: Uncollapsed 0x444D20.
int gameDialogReset()
{
    return _gdialogReset();
}

// NOTE: Uncollapsed 0x444D20.
int gameDialogExit()
{
    return _gdialogReset();
}

static void gameDialogRestoreCenterTile()
{
    if (gameDialogOldCenterTile == -1) {
        return;
    }

    if (gameDialogOldDudeTile != gDude->tile) {
        gameDialogOldCenterTile = gDude->tile;
    }

    if (_gdDialogWentOff) {
        _tile_scroll_to(gameDialogOldCenterTile, 2);
    }
}

// 0x444D2C
bool _gdialogActive()
{
    return _dialog_state_fix != 0;
}

// _gdialogActive() stays true for the whole talk_p_proc call, even if the
// script never opens a window (e.g. a flavor NPC that just floats a line
// and returns). This checks the actual window state instead, so callers can
// tell whether there's a head on screen to lip-sync against.
bool gameDialogWindowActive()
{
    return _gdialog_state == GAME_DIALOG_ACTIVE;
}

// gdialogEnter
// 0x444D3C
void gameDialogEnter(Object* speaker, int mode)
{
    if (speaker == nullptr) {
        debugPrint("\nError: gdialogEnter: target was NULL!");
        return;
    }

    _gdDialogWentOff = false;

    if (isInCombat()) {
        return;
    }

    if (speaker->sid == -1) {
        return;
    }

    if (objectTypeFromPid(speaker->pid) != OBJ_TYPE_ITEM && SID_TYPE(speaker->sid) != SCRIPT_TYPE_SPATIAL) {
        MessageListItem messageListItem;

        int rc = _action_can_talk_to(gDude, speaker);
        if (rc == -1) {
            // You can't see there.
            messageListItem.num = 660;
            if (messageListGetItem(&gProtoMessageList, &messageListItem)) {
                if (mode) { // never true
                    displayMonitorAddMessage(messageListItem.text);
                    debugPrint("Cannot see there ");
                } else {
                    debugPrint(messageListItem.text);
                }
            } else {
                debugPrint("\nError: gdialog: Can't find message!");
            }
            return;
        }

        if (rc == -2) {
            // Too far away.
            messageListItem.num = 661;
            if (messageListGetItem(&gProtoMessageList, &messageListItem)) {
                if (mode) { // never true
                    displayMonitorAddMessage(messageListItem.text);
                    debugPrint("Too far ");
                } else {
                    debugPrint(messageListItem.text);
                }
            } else {
                debugPrint("\nError: gdialog: Can't find message!");
            }
            return;
        }
    }

    gameDialogOldCenterTile = gCenterTile;
    gameDialogBarterModifier = 0;
    gameDialogOldDudeTile = gDude->tile;
    isoDisable();

    _dialog_state_fix = 1;
    gGameDialogSpeaker = speaker;
    gGameDialogSpeakerIsPartyMember = objectIsPartyMember(speaker);

    _dialogue_just_started = 1;

    // CE: Obtain and keep SID in a separate variable. This is needed because in
    // rare circumstates the speaker can destroy itself. So after executing it's
    // script |speaker| can point to freed memory. Dereferencing such pointer
    // can lead to crash depending on the environment (confirmed on Android and
    // MSVC debug builds).
    int sid = speaker->sid;
    if (sid != -1) {
        scriptExecProc(speaker->sid, SCRIPT_PROC_TALK);
    }

    Script* script;
    if (scriptGetScript(sid, &script) == -1) {
        gameMouseObjectsShow();
        isoEnable();
        scriptsExecMapUpdateProc();
        _dialog_state_fix = 0;
        return;
    }

    if (script->scriptOverrides || dialogMode != GAME_DIALOG_MODE_BARTER) {
        _dialogue_just_started = 0;
        isoEnable();
        scriptsExecMapUpdateProc();
        _dialog_state_fix = 0;
        return;
    }

    gameDialogEndLips();

    if (_gdialog_state == GAME_DIALOG_ACTIVE) {
        // TODO: Not sure about these conditions.
        if (dialogSwitchMode == GAME_DIALOG_MODE_SWITCH_TO_BARTER) {
            _gdialog_window_destroy();
        } else if (dialogSwitchMode == GAME_DIALOG_MODE_SWITCH_TO_PARTY_CONTROL) {
            _gdialog_window_destroy();
        } else if (dialogSwitchMode == GAME_DIALOG_MODE_SWITCH_TO_PARTY_CUSTOMIZATION) {
            _gdialog_window_destroy();
        } else {
            if (dialogSwitchMode == GAME_DIALOG_MODE_TALK) {
                gameDialogDestroyBarterWindow();
            } else if (dialogMode == GAME_DIALOG_MODE_TALK) {
                _gdialog_window_destroy();
            } else if (dialogMode == mode) {
                gameDialogDestroyBarterWindow();
            }
        }
        _gdialogExitFromScript();
    }

    _gdialog_state = GAME_DIALOG_INACTIVE;
    dialogMode = GAME_DIALOG_MODE_NONE;

    gameDialogRestoreCenterTile();

    isoEnable();
    scriptsExecMapUpdateProc();

    _dialog_state_fix = 0;
}

// 0x444FE4
void _gdialogSystemEnter()
{
    gameUpdateState();

    _gdDialogTurnMouseOff = true;

    soundContinueAll();
    gameDialogEnter(gGameDialogSpeaker, 0);
    soundContinueAll();

    gameDialogRestoreCenterTile();

    gameRequestState(GAME_STATE_2);

    gameUpdateState();
}

// 0x445050
void gameDialogStartLips(const char* audioFileName)
{
    if (audioFileName == nullptr) {
        debugPrint("\nGDialog: Bleep!");
        soundPlayFile("censor");
        return;
    }

    char name[16];
    if (artCopyFileName(gGameDialogHeadFrmId, name) == -1) {
        return;
    }

    if (lipsLoad(audioFileName, name) == -1 || lipsStart() == -1) {
        lipsFree();
        return;
    }

    gameDialogLipSyncStarted = true;

    debugPrint("Starting lipsynch speech");
}

// 0x4450C4
void gameDialogEndLips()
{
    if (gameDialogLipSyncStarted) {
        debugPrint("Ending lipsynch system");
        gameDialogLipSyncStarted = false;

        lipsFree();
    }
}

// 0x4450EC
int gameDialogEnable()
{
    tickersAdd(gameDialogTicker);
    return 0;
}

// 0x4450FC
int gameDialogDisable()
{
    tickersRemove(gameDialogTicker);
    return 0;
}

// 0x44510C
int _gdialogInitFromScript(const HeadFrmId& headFrmId, HeadFidget reaction)
{
    if (dialogMode == GAME_DIALOG_MODE_TALK) {
        return -1;
    }

    if (_gdialog_state == GAME_DIALOG_ACTIVE) {
        return 0;
    }

    animationStop();

    _boxesWereDisabled = indicatorBarHide();
    gGameDialogSpeakerIsPartyMember = objectIsPartyMember(gGameDialogSpeaker);
    _oldFont = fontGetCurrent();
    fontSetCurrent(101);
    dialogSetReplyWindow(135, 225, 379, 58, nullptr);
    dialogSetReplyColor(0.3f, 0.3f, 0.3f);
    dialogSetOptionWindow(127, 335, 393, 117, nullptr);
    dialogSetOptionColor(0.2f, 0.2f, 0.2f);
    dialogSetReplyTitle(nullptr);
    _dialogRegisterWinDrawCallbacks(_demo_copy_title, _demo_copy_options);
    gameDialogHighlightsInit();
    colorCycleDisable();
    if (_gdDialogTurnMouseOff) {
        _gmouse_disable(0);
    }
    gameMouseObjectsHide();
    gameMouseSetCursor(MOUSE_CURSOR_ARROW);
    textObjectsReset();

    // CE: Fix Barter button.
    _gdCreateHeadWindow();
    tickersAdd(gameDialogTicker);
    _gdSetupFidget(headFrmId, reaction);
    _gdialog_state = GAME_DIALOG_ACTIVE;
    _gmouse_disable_scrolling();

    if (!headFrmId.valid()) {
        // SFALL: Fix the music volume when entering the dialog.
        gameDialogOldMusicVolume = _gsound_background_volume_get_set(gMusicVolume / 2);
    } else {
        gameDialogOldMusicVolume = -1;
        backgroundSoundDelete();
    }

    _gdDialogWentOff = true;

    GameMode::enterGameMode(GameMode::kDialog);

    // Dialog is a UI screen with buttons that need direct touch-to-click.
    // On iPad with relative mouse mode, taps send zero-delta clicks at the
    // cursor position instead of at the finger — unusable without touchscreen
    // mode. Same pattern as inventory, skilldex, elevator, automap, etc.
    touch_set_touchscreen_mode(true);

    return 0;
}

// 0x445298
int _gdialogExitFromScript()
{
    if (dialogSwitchMode == GAME_DIALOG_MODE_SWITCH_TO_BARTER
        || dialogSwitchMode == GAME_DIALOG_MODE_SWITCH_TO_PARTY_CONTROL
        || dialogSwitchMode == GAME_DIALOG_MODE_SWITCH_TO_PARTY_CUSTOMIZATION) {
        return -1;
    }

    if (_gdialog_state == GAME_DIALOG_INACTIVE) {
        return 0;
    }

    gameDialogEndLips();
    dialogReviewEntriesClear();
    tickersRemove(gameDialogTicker);

    touch_set_touchscreen_mode(false);

    GameMode::exitGameMode(GameMode::kDialog);

    GameMode::enterGameMode(GameMode::kSpecial);
    _gdDestroyHeadWindow();
    GameMode::exitGameMode(GameMode::kSpecial);

    // CE: Fix Barter button.
    fontSetCurrent(_oldFont);

    if (gameDialogFidgetFrm != nullptr) {
        artUnlock(gameDialogFidgetFrmHandle);
        gameDialogFidgetFrm = nullptr;
    }

    if (_lipsKey != nullptr) {
        if (artUnlock(_lipsKey) == -1) {
            debugPrint("Failure unlocking lips frame!\n");
        }
        _lipsKey = nullptr;
        _lipsFp = nullptr;
        _lipsFrmId = HeadFrameId::None;
    }

    // NOTE: Uninline.
    gameDialogHighlightsExit();

    _gdialog_state = GAME_DIALOG_INACTIVE;
    dialogMode = GAME_DIALOG_MODE_NONE;

    colorCycleEnable();

    if (!gameUiIsDisabled()) {
        _gmouse_enable_scrolling();
    }

    if (gameDialogOldMusicVolume == -1) {
        backgroundSoundRestart(GSOUND_LIMIT_BEFORE);
    } else {
        backgroundSoundSetVolume(gameDialogOldMusicVolume);
    }

    if (_boxesWereDisabled) {
        indicatorBarShow();
    }

    _boxesWereDisabled = 0;

    if (_gdDialogTurnMouseOff) {
        if (!gameUiIsDisabled()) {
            _gmouse_enable();
        }

        _gdDialogTurnMouseOff = 0;
    }

    if (!gameUiIsDisabled()) {
        gameMouseObjectsShow();
    }

    _gdDialogWentOff = true;

    return 0;
}

// 0x445438
void gameDialogSetBackground(const BackgroundFrmId& background)
{
    if (background.valid()) {
        gameDialogBackgroundFrmId = background;
    }
}

// Renders supplementary message in reply area of the dialog.
//
// 0x445448
void gameDialogRenderSupplementaryMessage(const char* msg)
{
    if (_gd_replyWin == -1) {
        debugPrint("\nError: Reply window doesn't exist!");
        return;
    }

    _replyRect.left = 5;
    _replyRect.top = 10;
    _replyRect.right = 374;
    _replyRect.bottom = 58;
    _demo_copy_title(gameDialogReplyWindow);

    unsigned char* windowBuffer = windowGetBuffer(gameDialogReplyWindow);
    int lineHeight = fontGetLineHeight();

    int textOffset = 0;

    // NOTE: Uninline.
    text_to_rect_wrapped(windowBuffer,
        &_replyRect,
        msg,
        &textOffset,
        lineHeight,
        379,
        COLOR_GREEN | DRAW_TEXT_FLAG_NO_BG);

    windowShow(_gd_replyWin);
    windowRefresh(gameDialogReplyWindow);
}

// 0x4454FC
int _gdialogStart()
{
    gameDialogReviewEntriesLength = 0;
    gameDialogOptionEntriesLength = 0;
    return 0;
}

// 0x445510
int _gdialogSayMessage()
{
    mouseShowCursor();
    _gdialogGo();

    gameDialogOptionEntriesLength = 0;
    dialogReplyMessageListId = -1;

    return 0;
}

// NOTE: If you look at the scripts handlers, my best guess that their intention
// was to allow scripters to specify proc names instead of proc addresses. They
// dropped this idea, probably because they've updated their own compiler, or
// maybe there was not enough time to complete it. Any way, [procedure] is the
// identifier of the procedure in the script, but it is silently ignored.
//
// 0x445538
int gameDialogAddMessageOptionWithProcIdentifier(int messageListId, int messageId, const char* proc, int reaction)
{
    dialogOptionEntries[gameDialogOptionEntriesLength].proc = 0;

    return gameDialogAddMessageOption(messageListId, messageId, reaction);
}

// NOTE: If you look at the script handlers, my best guess that their intention
// was to allow scripters to specify proc names instead of proc addresses. They
// dropped this idea, probably because they've updated their own compiler, or
// maybe there was not enough time to complete it. Any way, [procedure] is the
// identifier of the procedure in the script, but it is silently ignored.
//
// 0x445578
int gameDialogAddTextOptionWithProcIdentifier(int messageListId, const char* text, const char* proc, int reaction)
{
    dialogOptionEntries[gameDialogOptionEntriesLength].proc = 0;

    return gameDialogAddTextOption(messageListId, text, reaction);
}

// 0x4455B8
int gameDialogAddMessageOptionWithProc(int messageListId, int messageId, int proc, int reaction)
{
    dialogOptionEntries[gameDialogOptionEntriesLength].proc = proc;

    return gameDialogAddMessageOption(messageListId, messageId, reaction);
}

// 0x4455FC
int gameDialogAddTextOptionWithProc(int messageListId, const char* text, int proc, int reaction)
{
    dialogOptionEntries[gameDialogOptionEntriesLength].proc = proc;

    return gameDialogAddTextOption(messageListId, text, reaction);
}

// 0x445640
int gameDialogSetMessageReply(Program* program, int messageListId, int messageId)
{
    gameDialogAddReviewMessage(messageListId, messageId);

    dialogReplyProgram = program;
    dialogReplyMessageListId = messageListId;
    dialogReplyMessageId = messageId;
    dialogReplyTextOffset = 0;
    dialogReplyText[0] = '\0';
    gameDialogOptionEntriesLength = 0;

    return 0;
}

// 0x44567C
int gameDialogSetTextReply(Program* program, int messageListId, const char* text)
{
    if (text == nullptr) {
        text = "";
    }

    gameDialogAddReviewText(text);

    dialogReplyProgram = program;
    dialogReplyTextOffset = 0;
    dialogReplyMessageListId = -4;
    dialogReplyMessageId = -4;

    snprintf(dialogReplyText, sizeof(dialogReplyText), "%s", text);

    gameDialogOptionEntriesLength = 0;

    return 0;
}

// 0x4456D8
int _gdialogGo()
{
    if (dialogReplyMessageListId == -1) {
        return 0;
    }

    int rc = 0;

    if (gameDialogOptionEntriesLength < 1) {
        dialogOptionEntries[gameDialogOptionEntriesLength].proc = 0;

        if (gameDialogAddMessageOption(-1, -1, 50) == -1) {
            programFatalError("Error setting option.");
            rc = -1;
        }
    }

    if (rc != -1) {
        rc = gameDialogProcessUI();
    }

    gameDialogOptionEntriesLength = 0;

    return rc;
}

// 0x445764
void _gdialogUpdatePartyStatus()
{
    if (dialogMode != GAME_DIALOG_MODE_TALK) {
        return;
    }

    bool isPartyMember = objectIsPartyMember(gGameDialogSpeaker);
    if (isPartyMember == gGameDialogSpeakerIsPartyMember) {
        return;
    }

    // NOTE: Uninline.
    gdHide();

    GameMode::enterGameMode(GameMode::kSpecial);

    _gdialog_window_destroy();

    gGameDialogSpeakerIsPartyMember = isPartyMember;

    GameMode::exitGameMode(GameMode::kSpecial);

    _gdialog_window_create();

    // NOTE: Uninline.
    gdUnhide();
}

// NOTE: Inlined.
//
// 0x4457EC
static int gdHide()
{
    if (_gd_replyWin != -1) {
        windowHide(_gd_replyWin);
    }

    if (_gd_optionsWin != -1) {
        windowHide(_gd_optionsWin);
    }

    return 0;
}

// NOTE: Inlined.
//
// 0x445818
static int gdUnhide()
{
    if (_gd_replyWin != -1) {
        windowShow(_gd_replyWin);
    }

    if (_gd_optionsWin != -1) {
        windowShow(_gd_optionsWin);
    }

    return 0;
}

// 0x44585C
int gameDialogAddMessageOption(int messageListId, int messageId, int reaction)
{
    if (gameDialogOptionEntriesLength >= DIALOG_OPTION_ENTRIES_CAPACITY) {
        debugPrint("\nError: dialog: Ran out of options!");
        return -1;
    }

    GameDialogOptionEntry* optionEntry = &(dialogOptionEntries[gameDialogOptionEntriesLength]);
    optionEntry->messageListId = messageListId;
    optionEntry->messageId = messageId;
    optionEntry->reaction = reaction;
    optionEntry->btn = -1;
    optionEntry->text[0] = '\0';

    gameDialogOptionEntriesLength++;

    return 0;
}

// 0x4458BC
int gameDialogAddTextOption(int messageListId, const char* text, int reaction)
{
    if (gameDialogOptionEntriesLength >= DIALOG_OPTION_ENTRIES_CAPACITY) {
        debugPrint("\nError: dialog: Ran out of options!");
        return -1;
    }

    GameDialogOptionEntry* optionEntry = &(dialogOptionEntries[gameDialogOptionEntriesLength]);
    optionEntry->messageListId = -4;
    optionEntry->messageId = -4;
    optionEntry->reaction = reaction;
    optionEntry->btn = -1;

    // SFALL
    if (numberOptions) {
        snprintf(optionEntry->text, sizeof(optionEntry->text), "%d. %s", gameDialogOptionEntriesLength + 1, text);
    } else {
        snprintf(optionEntry->text, sizeof(optionEntry->text), "%c %s", '\x95', text);
    }

    gameDialogOptionEntriesLength++;

    return 0;
}

// UI CODE STARTS HERE

static int createDialogRedButton(int win, int x, int y, void (*mouseUp)(int, int), int keyCode)
{
    int h = buttonCreateWithFrm(win, x, y, -1, -1, -1, keyCode,
        InterfaceFrameId::DialogRedButtonUp, InterfaceFrameId::DialogRedButtonDown,
        {}, BUTTON_FLAG_TRANSPARENT);
    if (h == -1) return -1;
    buttonSetCallbacks(h, _gsound_med_butt_press, _gsound_med_butt_release);
    if (mouseUp != nullptr) {
        buttonSetMouseCallbacks(h, nullptr, nullptr, nullptr, mouseUp);
    }
    return h;
}

static int createDialogReviewButton(int win)
{
    int h = buttonCreateWithFrm(win, 13, 154, -1, -1, -1, -1,
        InterfaceFrameId::DialogRestButtonUp, InterfaceFrameId::DialogRestButtonDown);
    if (h == -1) return -1;
    buttonSetMouseCallbacks(h, nullptr, nullptr, nullptr, gameDialogReviewButtonOnMouseUp);
    buttonSetCallbacks(h, _gsound_red_butt_press, _gsound_red_butt_release);
    return h;
}

static int createLittleRedButton(int win, int x, int y, int keyCode)
{
    int h = buttonCreateWithFrm(win, x, y, -1, -1, -1, keyCode,
        InterfaceFrameId::LittleRedButtonUp, InterfaceFrameId::LittleRedButtonDown,
        {}, BUTTON_FLAG_TRANSPARENT);
    if (h == -1) return -1;
    buttonSetCallbacks(h, _gsound_red_butt_press, _gsound_red_butt_release);
    return h;
}

// 0x445938
int gameDialogReviewWindowInit(int* win)
{
    if (gameDialogLipSyncStarted) {
        if (soundIsPlaying(gLipsData.sound)) {
            gameDialogEndLips();
        }
    }

    gameDialogReviewWindowOldFont = fontGetCurrent();

    if (win == nullptr) {
        return -1;
    }

    int reviewWindowX = (screenGetWidth() - GAME_DIALOG_REVIEW_WINDOW_WIDTH) / 2;
    int reviewWindowY = (screenGetHeight() - GAME_DIALOG_REVIEW_WINDOW_HEIGHT) / 2;
    *win = windowCreate(reviewWindowX,
        reviewWindowY,
        GAME_DIALOG_REVIEW_WINDOW_WIDTH,
        GAME_DIALOG_REVIEW_WINDOW_HEIGHT,
        static_cast<ColorWithFlags>(256),
        WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
    if (*win == -1) {
        return -1;
    }

    FrmImage backgroundFrmImage;
    if (!backgroundFrmImage.lock(InterfaceFrameId::DialogReviewBackground)) {
        windowDestroy(*win);
        *win = -1;
        return -1;
    }

    unsigned char* windowBuffer = windowGetBuffer(*win);
    blitBufferToBuffer(backgroundFrmImage.getData(),
        GAME_DIALOG_REVIEW_WINDOW_WIDTH,
        GAME_DIALOG_REVIEW_WINDOW_HEIGHT,
        GAME_DIALOG_REVIEW_WINDOW_WIDTH,
        windowBuffer,
        GAME_DIALOG_REVIEW_WINDOW_WIDTH);

    backgroundFrmImage.unlock();

    int index;
    for (index = 0; index < GAME_DIALOG_REVIEW_WINDOW_BUTTON_FRM_COUNT; index++) {
        const FrmId frmId = kGameDialogReviewWindowButtonFrmIds[index];
        if (!_reviewFrmImages[index].lock(frmId)) {
            break;
        }
    }

    if (index != GAME_DIALOG_REVIEW_WINDOW_BUTTON_FRM_COUNT) {
        gameDialogReviewWindowFree(win);
        return -1;
    }

    int upBtn = buttonCreate(*win,
        475,
        152,
        gameDialogReviewWindowButtonWidths[GAME_DIALOG_REVIEW_WINDOW_BUTTON_SCROLL_UP],
        gameDialogReviewWindowButtonHeights[GAME_DIALOG_REVIEW_WINDOW_BUTTON_SCROLL_UP],
        -1,
        -1,
        -1,
        KEY_ARROW_UP,
        _reviewFrmImages[GAME_DIALOG_REVIEW_WINDOW_BUTTON_FRM_ARROW_UP_NORMAL].getData(),
        _reviewFrmImages[GAME_DIALOG_REVIEW_WINDOW_BUTTON_FRM_ARROW_UP_PRESSED].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (upBtn == -1) {
        gameDialogReviewWindowFree(win);
        return -1;
    }

    buttonSetCallbacks(upBtn, _gsound_med_butt_press, _gsound_med_butt_release);

    int downBtn = buttonCreate(*win,
        475,
        191,
        gameDialogReviewWindowButtonWidths[GAME_DIALOG_REVIEW_WINDOW_BUTTON_SCROLL_DOWN],
        gameDialogReviewWindowButtonHeights[GAME_DIALOG_REVIEW_WINDOW_BUTTON_SCROLL_DOWN],
        -1,
        -1,
        -1,
        KEY_ARROW_DOWN,
        _reviewFrmImages[GAME_DIALOG_REVIEW_WINDOW_BUTTON_FRM_ARROW_DOWN_NORMAL].getData(),
        _reviewFrmImages[GAME_DIALOG_REVIEW_WINDOW_BUTTON_FRM_ARROW_DOWN_PRESSED].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (downBtn == -1) {
        gameDialogReviewWindowFree(win);
        return -1;
    }

    buttonSetCallbacks(downBtn, _gsound_med_butt_press, _gsound_med_butt_release);

    int doneBtn = buttonCreate(*win,
        499,
        398,
        gameDialogReviewWindowButtonWidths[GAME_DIALOG_REVIEW_WINDOW_BUTTON_DONE],
        gameDialogReviewWindowButtonHeights[GAME_DIALOG_REVIEW_WINDOW_BUTTON_DONE],
        -1,
        -1,
        -1,
        KEY_ESCAPE,
        _reviewFrmImages[GAME_DIALOG_REVIEW_WINDOW_BUTTON_FRM_DONE_NORMAL].getData(),
        _reviewFrmImages[GAME_DIALOG_REVIEW_WINDOW_BUTTON_FRM_DONE_PRESSED].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (doneBtn == -1) {
        gameDialogReviewWindowFree(win);
        return -1;
    }

    buttonSetCallbacks(doneBtn, _gsound_red_butt_press, _gsound_red_butt_release);

    fontSetCurrent(101);

    windowRefresh(*win);

    tickersRemove(gameDialogTicker);

    if (!_reviewBackgroundFrmImage.lock(InterfaceFrameId::DialogReviewBackground)) {
        gameDialogReviewWindowFree(win);
        return -1;
    }

    return 0;
}

// 0x445C18
int gameDialogReviewWindowFree(int* win)
{
    tickersAdd(gameDialogTicker);

    for (int index = 0; index < GAME_DIALOG_REVIEW_WINDOW_BUTTON_FRM_COUNT; index++) {
        _reviewFrmImages[index].unlock();
    }

    _reviewBackgroundFrmImage.unlock();

    fontSetCurrent(gameDialogReviewWindowOldFont);

    if (win == nullptr) {
        return -1;
    }

    windowDestroy(*win);
    *win = -1;

    return 0;
}

// 0x445CA0
int gameDialogShowReview()
{
    ScopedGameMode gm(GameMode::kDialogReview);

    int win;

    if (gameDialogReviewWindowInit(&win) == -1) {
        debugPrint("\nError initializing review window!");
        return -1;
    }

    // probably current top line or something like this, which is used to scroll
    int reviewStartIndex = 0;
    gameDialogReviewWindowUpdate(win, reviewStartIndex);

    while (true) {
        sharedFpsLimiter.mark();

        int keyCode = inputGetInput();
        if (keyCode == 17 || keyCode == 24 || keyCode == 324) {
            showQuitConfirmationDialog();
        }

        if (_game_user_wants_to_quit != GAME_QUIT_REQUEST_NONE || keyCode == KEY_ESCAPE) {
            break;
        }

        // likely scrolling
        if (keyCode == 328) {
            reviewStartIndex -= 1;
            if (reviewStartIndex >= 0) {
                gameDialogReviewWindowUpdate(win, reviewStartIndex);
            } else {
                reviewStartIndex = 0;
            }
        } else if (keyCode == 336) {
            reviewStartIndex += 1;
            if (reviewStartIndex <= gameDialogReviewEntriesLength - 1) {
                gameDialogReviewWindowUpdate(win, reviewStartIndex);
            } else {
                reviewStartIndex = gameDialogReviewEntriesLength - 1;
            }
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    if (gameDialogReviewWindowFree(&win) == -1) {
        return -1;
    }

    return 0;
}

// NOTE: Uncollapsed 0x445CA0 with different signature.
void gameDialogReviewButtonOnMouseUp(int btn, int keyCode)
{
    gameDialogShowReview();
}

// 0x445D44
void gameDialogReviewWindowUpdate(int win, int origin)
{
    Rect entriesRect;
    entriesRect.left = 113;
    entriesRect.top = 76;
    entriesRect.right = 422;
    entriesRect.bottom = 418;

    int entrySpacing = fontGetLineHeight() + 2;
    unsigned char* windowBuffer = windowGetBuffer(win);
    if (windowBuffer == nullptr) {
        debugPrint("\nError: gdialog: review: can't find buffer!");
        return;
    }

    int width = GAME_DIALOG_WINDOW_WIDTH;
    blitBufferToBuffer(
        _reviewBackgroundFrmImage.getData() + width * entriesRect.top + entriesRect.left,
        width,
        entriesRect.bottom - entriesRect.top + 15,
        width,
        windowBuffer + width * entriesRect.top + entriesRect.left,
        width);

    int y = 76;
    for (int index = origin; index < gameDialogReviewEntriesLength; index++) {
        GameDialogReviewEntry* dialogReviewEntry = &(dialogReviewEntries[index]);

        char name[60];
        snprintf(name, sizeof(name), "%s:", objectGetName(gGameDialogSpeaker));
        windowDrawText(win, name, 180, 88, y, COLOR_GREEN | DRAW_TEXT_FLAG_NO_BG);
        entriesRect.top += entrySpacing;

        char* replyText;
        if (dialogReviewEntry->replyMessageListId <= -3) {
            replyText = dialogReviewEntry->replyText;
        } else {
            replyText = _scr_get_msg_str(dialogReviewEntry->replyMessageListId, dialogReviewEntry->replyMessageId);
        }

        if (replyText == nullptr) {
            showMessageBox("\nGDialog::Error Grabbing text message!");
            exit(1);
        }

        // NOTE: Uninline.
        y = text_to_rect_wrapped(windowBuffer + 113,
            &entriesRect,
            replyText,
            nullptr,
            fontGetLineHeight(),
            640,
            COLOR_LIGHT_GREEN_3 | DRAW_TEXT_FLAG_NO_BG);

        // SFALL: Cosmetic fix to the dialog review interface to prevent the
        // player name from being displayed at the bottom of the window when the
        // text is longer than one screen.
        if (y >= 407) {
            break;
        }

        if (dialogReviewEntry->optionMessageListId != -3) {
            snprintf(name, sizeof(name), "%s:", objectGetName(gDude));
            windowDrawText(win, name, 180, 88, y, COLOR_LIGHT_GREY | DRAW_TEXT_FLAG_NO_BG);
            entriesRect.top += entrySpacing;

            char* optionText;
            if (dialogReviewEntry->optionMessageListId <= -3) {
                optionText = dialogReviewEntry->optionText;
            } else {
                optionText = _scr_get_msg_str(dialogReviewEntry->optionMessageListId, dialogReviewEntry->optionMessageId);
            }

            if (optionText == nullptr) {
                showMessageBox("\nGDialog::Error Grabbing text message!");
                exit(1);
            }

            // NOTE: Uninline.
            y = text_to_rect_wrapped(windowBuffer + 113,
                &entriesRect,
                optionText,
                nullptr,
                fontGetLineHeight(),
                640,
                COLOR_GREY_2 | DRAW_TEXT_FLAG_NO_BG);
        }

        if (y >= 407) {
            break;
        }
    }

    entriesRect.left = 88;
    entriesRect.top = 76;
    entriesRect.bottom += 14;
    entriesRect.right = 434;
    windowRefreshRect(win, &entriesRect);
}

// 0x445FDC
void dialogReviewEntriesClear()
{
    for (int index = 0; index < gameDialogReviewEntriesLength; index++) {
        GameDialogReviewEntry* entry = &(dialogReviewEntries[index]);
        entry->replyMessageListId = 0;
        entry->replyMessageId = 0;

        if (entry->replyText != nullptr) {
            internal_free(entry->replyText);
            entry->replyText = nullptr;
        }

        entry->optionMessageListId = 0;
        entry->optionMessageId = 0;
    }
}

// 0x446040
int gameDialogAddReviewMessage(int messageListId, int messageId)
{
    if (gameDialogReviewEntriesLength >= DIALOG_REVIEW_ENTRIES_CAPACITY) {
        debugPrint("\nError: Ran out of review slots!");
        return -1;
    }

    GameDialogReviewEntry* entry = &(dialogReviewEntries[gameDialogReviewEntriesLength]);
    entry->replyMessageListId = messageListId;
    entry->replyMessageId = messageId;

    // NOTE: I'm not sure why there are two consequtive assignments.
    entry->optionMessageListId = -1;
    entry->optionMessageId = -1;

    entry->optionMessageListId = -3;
    entry->optionMessageId = -3;

    gameDialogReviewEntriesLength++;

    return 0;
}

// 0x4460B4
int gameDialogAddReviewText(const char* string)
{
    if (gameDialogReviewEntriesLength >= DIALOG_REVIEW_ENTRIES_CAPACITY) {
        debugPrint("\nError: Ran out of review slots!");
        return -1;
    }

    GameDialogReviewEntry* entry = &(dialogReviewEntries[gameDialogReviewEntriesLength]);
    entry->replyMessageListId = -4;
    entry->replyMessageId = -4;

    if (entry->replyText != nullptr) {
        internal_free(entry->replyText);
        entry->replyText = nullptr;
    }

    entry->replyText = (char*)internal_malloc(strlen(string) + 1);
    strcpy(entry->replyText, string);

    entry->optionMessageListId = -3;
    entry->optionMessageId = -3;
    entry->optionText = nullptr;

    gameDialogReviewEntriesLength++;

    return 0;
}

// 0x4461A4
int gameDialogSetReviewOptionMessage(int messageListId, int messageId)
{
    if (gameDialogReviewEntriesLength >= DIALOG_REVIEW_ENTRIES_CAPACITY) {
        debugPrint("\nError: Ran out of review slots!");
        return -1;
    }

    GameDialogReviewEntry* entry = &(dialogReviewEntries[gameDialogReviewEntriesLength - 1]);
    entry->optionMessageListId = messageListId;
    entry->optionMessageId = messageId;
    entry->optionText = nullptr;

    return 0;
}

// 0x4461F0
int gameDialogSetReviewOptionText(const char* string)
{
    if (gameDialogReviewEntriesLength >= DIALOG_REVIEW_ENTRIES_CAPACITY) {
        debugPrint("\nError: Ran out of review slots!");
        return -1;
    }

    GameDialogReviewEntry* entry = &(dialogReviewEntries[gameDialogReviewEntriesLength - 1]);
    entry->optionMessageListId = -4;
    entry->optionMessageId = -4;

    entry->optionText = (char*)internal_malloc(strlen(string) + 1);
    strcpy(entry->optionText, string);

    return 0;
}

// Creates dialog interface.
//
// 0x446288
int _gdProcessInit()
{
    int upBtn;
    int downBtn;
    int optionsWindowX;
    int optionsWindowY;

    Rect bgRect;
    windowGetRect(gameDialogBackgroundWindow, &bgRect);
    int replyWindowX = bgRect.left + GAME_DIALOG_REPLY_WINDOW_X;
    int replyWindowY = bgRect.top + GAME_DIALOG_REPLY_WINDOW_Y;
    gameDialogReplyWindow = windowCreate(replyWindowX,
        replyWindowY,
        GAME_DIALOG_REPLY_WINDOW_WIDTH,
        GAME_DIALOG_REPLY_WINDOW_HEIGHT,
        static_cast<ColorWithFlags>(256),
        WINDOW_MOVE_ON_TOP);
    if (gameDialogReplyWindow == -1) {
        goto err;
    }

    // Top part of the reply window - scroll up.
    upBtn = buttonCreate(gameDialogReplyWindow, 1, 1, 377, 28, -1, -1, KEY_ARROW_UP, -1, nullptr, nullptr, nullptr, 32);
    if (upBtn == -1) {
        goto err_1;
    }

    buttonSetCallbacks(upBtn, _gsound_red_butt_press, _gsound_red_butt_release);
    buttonSetMouseCallbacks(upBtn, _reply_arrow_up, _reply_arrow_restore, nullptr, nullptr);

    // Bottom part of the reply window - scroll down.
    downBtn = buttonCreate(gameDialogReplyWindow, 1, 29, 377, 28, -1, -1, KEY_ARROW_DOWN, -1, nullptr, nullptr, nullptr, 32);
    if (downBtn == -1) {
        goto err_1;
    }

    buttonSetCallbacks(downBtn, _gsound_red_butt_press, _gsound_red_butt_release);
    buttonSetMouseCallbacks(downBtn, _reply_arrow_down, _reply_arrow_restore, nullptr, nullptr);

    optionsWindowX = bgRect.left + GAME_DIALOG_OPTIONS_WINDOW_X;
    optionsWindowY = bgRect.top + GAME_DIALOG_OPTIONS_WINDOW_Y;
    gameDialogOptionsWindow = windowCreate(optionsWindowX, optionsWindowY, GAME_DIALOG_OPTIONS_WINDOW_WIDTH, GAME_DIALOG_OPTIONS_WINDOW_HEIGHT, static_cast<ColorWithFlags>(256), WINDOW_MOVE_ON_TOP);
    if (gameDialogOptionsWindow == -1) {
        goto err_2;
    }

    // CE: Move red buttons init to `_gdialogInitFromScript`.

    _talkOldFont = fontGetCurrent();
    fontSetCurrent(101);

    return 0;

err_2:

    windowDestroy(gameDialogOptionsWindow);
    gameDialogOptionsWindow = -1;

err_1:

    windowDestroy(gameDialogReplyWindow);
    gameDialogReplyWindow = -1;

err:

    return -1;
}

// RELASE: Rename/comment.
// free dialog option buttons
// 0x446454
void _gdProcessCleanup()
{
    for (int index = 0; index < gameDialogOptionEntriesLength; index++) {
        GameDialogOptionEntry* optionEntry = &(dialogOptionEntries[index]);

        if (optionEntry->btn != -1) {
            buttonDestroy(optionEntry->btn);
            optionEntry->btn = -1;
        }
    }
}

// RELASE: Rename/comment.
// free dialog interface
// 0x446498
int _gdProcessExit()
{
    _gdProcessCleanup();

    // CE: Don't let a held-back reply speech leak into the next conversation if
    // the barter screen was never actually shown.
    gameDialogDeferReplySpeech = false;

    // CE: Move red buttons exit to `_gdialogExitFromScript`.

    windowDestroy(gameDialogReplyWindow);
    gameDialogReplyWindow = -1;

    windowDestroy(gameDialogOptionsWindow);
    gameDialogOptionsWindow = -1;

    fontSetCurrent(_talkOldFont);

    return 0;
}

// 0x446504 gdUpdateDudeCaps
void gameDialogRenderCaps()
{
    Rect rect;
    rect.left = 5;
    rect.right = 70;
    rect.top = 36;
    rect.bottom = fontGetLineHeight() + 36;

    _talkToRefreshDialogWindowRect(&rect);

    int oldFont = fontGetCurrent();
    fontSetCurrent(101);

    int caps = itemGetTotalCaps(gDude);
    char text[20];
    snprintf(text, sizeof(text), "$%d", caps);

    int width = fontGetStringWidth(text);
    if (width > 60) {
        width = 60;
    }

    windowDrawText(gameDialogWindow, text, width, 38 - width / 2, 36, COLOR_GREEN | (DRAW_TEXT_FLAG_REFRESH | DRAW_TEXT_FLAG_NO_BG | DRAW_TEXT_FLAG_OVERFLOW));

    fontSetCurrent(oldFont);
}

// 0x4465C0 gdProcess
int gameDialogProcessUI()
{
    if (_gdReenterLevel == 0) {
        if (_gdProcessInit() == -1) {
            return -1;
        }
    }

    _gdReenterLevel += 1;

    _gdProcessUpdate();

    bool autoAdvance = false;
    if (dialogReplyTextOffset != 0) {
        autoAdvance = true;
        _gdReplyTooBig = 1;
    }

    unsigned int tick = getTicks();
    int pageCount = 0;
    int pageIndex = 0;
    int pageOffsets[10];
    pageOffsets[0] = 0;
    for (;;) {
        sharedFpsLimiter.mark();

        int keyCode = inputGetInput();

        convertMouseWheelToArrowKey(&keyCode);

        if (keyCode == KEY_CTRL_Q || keyCode == KEY_CTRL_X || keyCode == KEY_F10) {
            showQuitConfirmationDialog();
        }

        if (_game_user_wants_to_quit != GAME_QUIT_REQUEST_NONE) {
            break;
        }

        if (keyCode == KEY_CTRL_B && !_mouse_click_in(135, 225, 514, 283)) {
            if (gameMouseGetCursor() != MOUSE_CURSOR_ARROW) {
                gameMouseSetCursor(MOUSE_CURSOR_ARROW);
            }
        } else {
            if (dialogSwitchMode == GAME_DIALOG_MODE_BARTER_ACTIVE) {
                dialogMode = GAME_DIALOG_MODE_BARTER;

                GameMode::exitGameModeQuietly(GameMode::kSpecial);

                barterProcessUI(gameDialogWindow, gGameDialogSpeaker, gameDialogPlayerTableObj, gameDialogBartererTableObj, gameDialogBarterModifier);
                gameDialogBarterCleanupTables();

                GameDialogMode dialogueState = dialogMode;
                gameDialogDestroyBarterWindow();
                dialogMode = dialogueState;

                if (dialogueState == GAME_DIALOG_MODE_BARTER) {
                    dialogSwitchMode = GAME_DIALOG_MODE_TALK;
                    dialogMode = GAME_DIALOG_MODE_TALK;
                }

                // Barter's _exit_inventory() disables touchscreen mode.
                // Re-enable it for the dialog UI.
                touch_set_touchscreen_mode(true);

                continue;
            } else if (dialogSwitchMode == GAME_DIALOG_MODE_PARTY_CONTROL_ACTIVE) {
                dialogMode = GAME_DIALOG_MODE_PARTY_CONTROL;
                partyMemberControlWindowHandleEvents();
                partyMemberControlWindowFree();

                // Party control may have changed touchscreen mode.
                // Re-enable it for the dialog UI.
                touch_set_touchscreen_mode(true);

                continue;
            } else if (dialogSwitchMode == GAME_DIALOG_MODE_PARTY_CUSTOMIZATION_ACTIVE) {
                dialogMode = GAME_DIALOG_MODE_PARTY_CUSTOMIZATION;
                partyMemberCustomizationWindowHandleEvents();
                partyMemberCustomizationWindowFree();

                // Party customization may have changed touchscreen mode.
                // Re-enable it for the dialog UI.
                touch_set_touchscreen_mode(true);

                continue;
            }

            if (keyCode == KEY_LOWERCASE_B) {
                gameDialogBarterButtonUpMouseUp(-1, -1);
            }
        }

        if (_gdReplyTooBig) {
            unsigned int now = _get_bk_time();
            if (autoAdvance) {
                if (getTicksBetween(now, tick) >= 10000 || keyCode == KEY_SPACE) {
                    pageCount++;
                    pageIndex++;
                    pageOffsets[pageCount] = dialogReplyTextOffset;
                    gameDialogRenderReply();
                    tick = now;
                    if (!dialogReplyTextOffset) {
                        autoAdvance = false;
                    }
                }
            }

            if (keyCode == KEY_ARROW_UP) {
                if (pageIndex > 0) {
                    pageIndex--;
                    dialogReplyTextOffset = pageOffsets[pageIndex];
                    autoAdvance = false;
                    gameDialogRenderReply();
                }
            } else if (keyCode == KEY_ARROW_DOWN) {
                if (pageIndex < pageCount) {
                    pageIndex++;
                    dialogReplyTextOffset = pageOffsets[pageIndex];
                    autoAdvance = false;
                    gameDialogRenderReply();
                } else {
                    if (dialogReplyTextOffset != 0) {
                        tick = now;
                        pageIndex++;
                        pageCount++;
                        pageOffsets[pageCount] = dialogReplyTextOffset;
                        autoAdvance = false;
                        gameDialogRenderReply();
                    }
                }
            }
        }

        if (keyCode != -1) {
            if (keyCode >= 1200 && keyCode <= 1250) {
                gameDialogOptionOnMouseEnter(keyCode - 1200);
            } else if (keyCode >= 1300 && keyCode <= 1330) {
                gameDialogOptionOnMouseExit(keyCode - 1300);
            } else if (keyCode >= 48 && keyCode <= 57) {
                // SFALL: Prevents from using 0 to escape from dialogue at any time.
                if (keyCode == KEY_0 && gameDialogFix) {
                    continue;
                }

                int optionIndex = keyCode - 49;
                if (optionIndex < gameDialogOptionEntriesLength) {
                    pageCount = 0;
                    pageIndex = 0;
                    pageOffsets[0] = 0;
                    _gdReplyTooBig = 0;

                    if (_gdProcessChoice(optionIndex) == -1) {
                        break;
                    }

                    tick = getTicks();

                    if (dialogReplyTextOffset) {
                        autoAdvance = true;
                        _gdReplyTooBig = 1;
                    } else {
                        autoAdvance = false;
                    }
                }
            }
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    _gdReenterLevel -= 1;

    if (_gdReenterLevel == 0) {
        if (_gdProcessExit() == -1) {
            return -1;
        }
    }

    return 0;
}

// 0x4468DC
int _gdProcessChoice(int optionIndex)
{
    // FIXME: There is a buffer underread bug when `optionIndex` is -1 (pressing 0 on the
    // keyboard, see `_gdProcess`). When it happens the game looks into unused
    // continuation of `dialogReplyText` (within 0x58F868-0x58FF70 range) which
    // is initialized to 0 according to C spec. I was not able to replicate the
    // same behaviour by extending dialogReplyText to 2700 bytes or introduce
    // new 1800 bytes buffer in between, at least not in debug builds. In order
    // to preserve original behaviour this dummy dialog option entry is used.
    GameDialogOptionEntry dummy;
    memset(&dummy, 0, sizeof(dummy));

    mouseHideCursor();
    _gdProcessCleanup();

    GameDialogOptionEntry* dialogOptionEntry = optionIndex != -1 ? &(dialogOptionEntries[optionIndex]) : &dummy;
    if (dialogOptionEntry->messageListId == -4) {
        gameDialogSetReviewOptionText(dialogOptionEntry->text);
    } else {
        gameDialogSetReviewOptionMessage(dialogOptionEntry->messageListId, dialogOptionEntry->messageId);
    }

    _can_start_new_fidget = false;

    gameDialogEndLips();

    int reaction = GAME_DIALOG_REACTION_NEUTRAL;
    switch (dialogOptionEntry->reaction) {
    case GAME_DIALOG_REACTION_GOOD:
        reaction = -1;
        break;
    case GAME_DIALOG_REACTION_NEUTRAL:
        reaction = 0;
        break;
    case GAME_DIALOG_REACTION_BAD:
        reaction = 1;
        break;
    default:
        // See 0x446907 in ecx but this branch should be unreachable. Due to the
        // bug described above, this code is reachable.
        reaction = GAME_DIALOG_REACTION_NEUTRAL;
        debugPrint("\nError: dialog: Empathy Perk: invalid reaction!");
        break;
    }

    _demo_copy_title(gameDialogReplyWindow);
    _demo_copy_options(gameDialogOptionsWindow);
    windowRefresh(gameDialogReplyWindow);
    windowRefresh(gameDialogOptionsWindow);

    gameDialogOptionOnMouseEnter(optionIndex);
    _talk_to_critter_reacts(reaction);

    gameDialogOptionEntriesLength = 0;

    if (_gdReenterLevel < 2) {
        if (dialogOptionEntry->proc != 0) {
            programExecuteProcedure(dialogReplyProgram, dialogOptionEntry->proc);
        }
    }

    mouseShowCursor();

    if (gameDialogOptionEntriesLength == 0) {
        return -1;
    }

    _gdProcessUpdate();

    return 0;
}

// 0x446A18
void gameDialogOptionOnMouseEnter(int index)
{
    // FIXME: See explanation in `_gdProcessChoice`.
    GameDialogOptionEntry dummy;
    memset(&dummy, 0, sizeof(dummy));

    GameDialogOptionEntry* dialogOptionEntry = index != -1 ? &(dialogOptionEntries[index]) : &dummy;
    if (dialogOptionEntry->btn == 0) {
        return;
    }

    _optionRect.left = 0;
    _optionRect.top = dialogOptionEntry->top;
    _optionRect.right = 391;
    _optionRect.bottom = dialogOptionEntry->bottom;
    _gDialogRefreshOptionsRect(gameDialogOptionsWindow, &_optionRect);

    _optionRect.left = 5;
    _optionRect.right = 388;

    ColorWithFlags color = COLOR_LIGHT_YELLOW | DRAW_TEXT_FLAG_NO_BG;
    if (perkHasRank(gDude, PERK_EMPATHY)) {
        color = COLOR_LIGHT_YELLOW | DRAW_TEXT_FLAG_NO_BG;
        switch (dialogOptionEntry->reaction) {
        case GAME_DIALOG_REACTION_GOOD:
            color = COLOR_MAGENTA | DRAW_TEXT_FLAG_NO_BG;
            break;
        case GAME_DIALOG_REACTION_NEUTRAL:
            break;
        case GAME_DIALOG_REACTION_BAD:
            color = COLOR_LIGHT_RED | DRAW_TEXT_FLAG_NO_BG;
            break;
        default:
            debugPrint("\nError: dialog: Empathy Perk: invalid reaction!");
            break;
        }
    }

    // NOTE: Uninline.
    text_to_rect_wrapped(windowGetBuffer(gameDialogOptionsWindow),
        &_optionRect,
        dialogOptionEntry->text,
        nullptr,
        fontGetLineHeight(),
        393,
        color);

    _optionRect.left = 0;
    _optionRect.right = 391;
    _optionRect.top = dialogOptionEntry->top;
    windowRefreshRect(gameDialogOptionsWindow, &_optionRect);
}

// 0x446B5C
void gameDialogOptionOnMouseExit(int index)
{
    GameDialogOptionEntry* dialogOptionEntry = &(dialogOptionEntries[index]);

    _optionRect.left = 0;
    _optionRect.top = dialogOptionEntry->top;
    _optionRect.right = 391;
    _optionRect.bottom = dialogOptionEntry->bottom;
    _gDialogRefreshOptionsRect(gameDialogOptionsWindow, &_optionRect);

    ColorWithFlags color = COLOR_GREEN | DRAW_TEXT_FLAG_NO_BG;
    if (perkGetRank(gDude, PERK_EMPATHY) != 0) {
        color = COLOR_LIGHT_YELLOW | DRAW_TEXT_FLAG_NO_BG;
        switch (dialogOptionEntry->reaction) {
        case GAME_DIALOG_REACTION_GOOD:
            color = COLOR_BLUE | DRAW_TEXT_FLAG_NO_BG;
            break;
        case GAME_DIALOG_REACTION_NEUTRAL:
            color = COLOR_GREEN | DRAW_TEXT_FLAG_NO_BG;
            break;
        case GAME_DIALOG_REACTION_BAD:
            color = COLOR_RED | DRAW_TEXT_FLAG_NO_BG;
            break;
        default:
            debugPrint("\nError: dialog: Empathy Perk: invalid reaction!");
            break;
        }
    }

    _optionRect.left = 5;
    _optionRect.right = 388;

    // NOTE: Uninline.
    text_to_rect_wrapped(windowGetBuffer(gameDialogOptionsWindow),
        &_optionRect,
        dialogOptionEntry->text,
        nullptr,
        fontGetLineHeight(),
        393,
        color);

    _optionRect.right = 391;
    _optionRect.top = dialogOptionEntry->top;
    _optionRect.left = 0;
    windowRefreshRect(gameDialogOptionsWindow, &_optionRect);
}

// 0x446C94
void gameDialogRenderReply()
{
    _replyRect.left = 5;
    _replyRect.top = 10;
    _replyRect.right = 374;
    _replyRect.bottom = 58;

    // NOTE: There is an unused if condition.
    perkGetRank(gDude, PERK_EMPATHY);

    _demo_copy_title(gameDialogReplyWindow);

    // NOTE: Uninline.
    text_to_rect_wrapped(windowGetBuffer(gameDialogReplyWindow),
        &_replyRect,
        dialogReplyText,
        &dialogReplyTextOffset,
        fontGetLineHeight(),
        379,
        COLOR_GREEN | DRAW_TEXT_FLAG_NO_BG);
    windowRefresh(gameDialogReplyWindow);
}

// 0x446D30
void _gdProcessUpdate()
{
    _replyRect.left = 5;
    _replyRect.top = 10;
    _replyRect.right = 374;
    _replyRect.bottom = 58;

    _optionRect.left = 5;
    _optionRect.top = 5;
    _optionRect.right = 388;
    _optionRect.bottom = 112;

    _demo_copy_title(gameDialogReplyWindow);
    _demo_copy_options(gameDialogOptionsWindow);

    if (dialogReplyMessageListId > 0) {
        // CE: Hold back the reply speech when this update runs while a switch to
        // the barter screen is already pending (the option's proc called
        // `gdialog_barter` before `Reply()`). `gameDialogTicker` starts it again
        // once the barter screen is dismissed. See `gameDialogDeferReplySpeech`.
        gameDialogDeferReplySpeech = dialogSwitchMode == GAME_DIALOG_MODE_SWITCH_TO_BARTER
            || dialogSwitchMode == GAME_DIALOG_MODE_BARTER_ACTIVE;

        char* s = _scr_get_msg_str_speech(dialogReplyMessageListId, dialogReplyMessageId, gameDialogDeferReplySpeech ? 0 : 1);
        if (s == nullptr) {
            showMessageBox("\n'GDialog::Error Grabbing text message!");
            exit(1);
        }

        strncpy(dialogReplyText, s, sizeof(dialogReplyText) - 1);
        *(dialogReplyText + sizeof(dialogReplyText) - 1) = '\0';
    }

    gameDialogRenderReply();

    ColorWithFlags color = COLOR_GREEN | DRAW_TEXT_FLAG_NO_BG;

    bool hasEmpathy = perkGetRank(gDude, PERK_EMPATHY) != 0;

    int width = _optionRect.right - _optionRect.left - 4;

    MessageListItem messageListItem;

    bool skippedOnce = false;

    for (int index = 0; index < gameDialogOptionEntriesLength; index++) {
        GameDialogOptionEntry* dialogOptionEntry = &(dialogOptionEntries[index]);

        if (hasEmpathy) {
            switch (dialogOptionEntry->reaction) {
            case GAME_DIALOG_REACTION_GOOD:
                color = COLOR_BLUE | DRAW_TEXT_FLAG_NO_BG;
                break;
            case GAME_DIALOG_REACTION_NEUTRAL:
                color = COLOR_GREEN | DRAW_TEXT_FLAG_NO_BG;
                break;
            case GAME_DIALOG_REACTION_BAD:
                color = COLOR_RED | DRAW_TEXT_FLAG_NO_BG;
                break;
            default:
                debugPrint("\nError: dialog: Empathy Perk: invalid reaction!");
                break;
            }
        }

        if (dialogOptionEntry->messageListId >= 0) {
            char* text = _scr_get_msg_str_speech(dialogOptionEntry->messageListId, dialogOptionEntry->messageId, 0);
            if (text == nullptr) {
                showMessageBox("\nGDialog::Error Grabbing text message!");
                exit(1);
            }

            // SFALL
            if (numberOptions) {
                snprintf(dialogOptionEntry->text, sizeof(dialogOptionEntry->text), "%d. %s", index + 1, text);
            } else {
                snprintf(dialogOptionEntry->text, sizeof(dialogOptionEntry->text), "%c %s", '\x95', text);
            }
        } else if (dialogOptionEntry->messageListId == -1) {
            if (index == 0) {
                // Go on
                messageListItem.num = 655;
                if (critterGetStat(gDude, STAT_INTELLIGENCE) < 4) {
                    if (messageListGetItem(&gProtoMessageList, &messageListItem)) {
                        // SFALL
                        if (numberOptions) {
                            snprintf(dialogOptionEntry->text, sizeof(dialogOptionEntry->text), "%d. %s", index + 1, messageListItem.text);
                        } else {
                            snprintf(dialogOptionEntry->text, sizeof(dialogOptionEntry->text), "%s", messageListItem.text);
                        }
                    } else {
                        debugPrint("\nError...can't find message!");
                        return;
                    }
                }
            } else {
                // TODO: Why only space?
                // SFALL
                if (numberOptions) {
                    snprintf(dialogOptionEntry->text, sizeof(dialogOptionEntry->text), "%d. %s", index + 1, " ");
                } else {
                    strcpy(dialogOptionEntry->text, " ");
                }
            }
        } else if (dialogOptionEntry->messageListId == -2) {
            // [Done]
            messageListItem.num = 650;
            if (messageListGetItem(&gProtoMessageList, &messageListItem)) {
                // SFALL
                if (numberOptions) {
                    snprintf(dialogOptionEntry->text, sizeof(dialogOptionEntry->text), "%d. %s", index + 1, messageListItem.text);
                } else {
                    snprintf(dialogOptionEntry->text, sizeof(dialogOptionEntry->text), "%c %s", '\x95', messageListItem.text);
                }
            } else {
                debugPrint("\nError...can't find message!");
                return;
            }
        }

        int estimate = _text_num_lines(dialogOptionEntry->text, _optionRect.right - _optionRect.left) * fontGetLineHeight() + _optionRect.top + 2;
        if (estimate < _optionRect.bottom) {
            int y = _optionRect.top;

            dialogOptionEntry->bottom = estimate;
            dialogOptionEntry->top = y;

            if (index == 0) {
                y = 0;
            }

            // NOTE: Uninline.
            text_to_rect_wrapped(windowGetBuffer(gameDialogOptionsWindow),
                &_optionRect,
                dialogOptionEntry->text,
                nullptr,
                fontGetLineHeight(),
                393,
                color);

            dialogOptionEntry->bottom = _optionRect.top;
            _optionRect.top += 2;

            dialogOptionEntry->btn = buttonCreate(gameDialogOptionsWindow, 2, y, width, _optionRect.top - y - 4, 1200 + index, 1300 + index, -1, 49 + index, nullptr, nullptr, nullptr, 0);
            if (dialogOptionEntry->btn != -1) {
                buttonSetCallbacks(dialogOptionEntry->btn, _gsound_red_butt_press, _gsound_red_butt_release);
            } else {
                debugPrint("\nError: Can't create button!");
            }
        } else {
            // TODO: seems odd that we would suppress the error the first time
            if (!skippedOnce) {
                skippedOnce = true;
            } else {
                debugPrint("Error: couldn't make button because it went below the window.\n");
            }
        }
    }

    gameDialogRenderCaps();
    windowRefresh(gameDialogReplyWindow);
    windowRefresh(gameDialogOptionsWindow);
}

// 0x44715C
int _gdCreateHeadWindow()
{
    dialogMode = GAME_DIALOG_MODE_TALK;

    int windowWidth = GAME_DIALOG_WINDOW_WIDTH;

    // NOTE: Uninline.
    if (talk_to_create_background_window() == -1 || gameDialogWindowRenderBackground() == -1) {
        _gdDestroyHeadWindow();
        return -1;
    }

    ConstBuffer2D backgroundBuf = windowGetBuffer2D(gameDialogBackgroundWindow);

    for (int index = 0; index < 8; index++) {
        soundContinueAll();

        Rect rect = gameDialogGetBackgroundRect(index);
        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;
        _backgrndBufs[index] = (unsigned char*)internal_malloc(static_cast<size_t>(width) * height);
        if (_backgrndBufs[index] == nullptr) {
            _gdDestroyHeadWindow();
            return -1;
        }

        Buffer2D savedBackgroundBuf { _backgrndBufs[index], width, height };
        blitBuffer2D(backgroundBuf, rect.left, rect.top, width, height, savedBackgroundBuf);
    }

    if (_gdialog_window_create() == -1) {
        _gdDestroyHeadWindow();
        return -1;
    }

    gameDialogDisplayBuffer = windowGetBuffer(gameDialogBackgroundWindow) + windowWidth * (14 + gameDialogHrArtYOffset()) + 126;

    // TODO: jnz at 0x447275 without cmp or test, not sure what that means.
    if (false) {
        _gdDestroyHeadWindow();
        return -1;
    }

    return 0;
}

// 0x447294
void _gdDestroyHeadWindow()
{
    if (gameDialogWindow != -1) {
        gameDialogDisplayBuffer = nullptr;
    }

    if (dialogMode == GAME_DIALOG_MODE_TALK) {
        _gdialog_window_destroy();
    } else if (dialogMode == GAME_DIALOG_MODE_BARTER) {
        gameDialogDestroyBarterWindow();
    }

    if (gameDialogBackgroundWindow != -1) {
        windowDestroy(gameDialogBackgroundWindow);
        gameDialogBackgroundWindow = -1;
    }

    gameDialogUseHrArt = false;
    expandedBarterEnabled = false;

    for (int index = 0; index < 8; index++) {
        internal_free(_backgrndBufs[index]);
        _backgrndBufs[index] = nullptr;
    }
}

// 0x447300
void _gdSetupFidget(const HeadFrmId& headFrmId, HeadFidget reaction)
{
    gameDialogFidgetFrmCurrentFrame = 0;

    if (!headFrmId.valid()) {
        gameDialogFidgetFrmId = HeadFrameId::Invalid;
        gameDialogFidgetFrm = nullptr;
        gameDialogFidgetFrmHandle = INVALID_CACHE_ENTRY;
        gameDialogFidgetReaction = FIDGET_INVALID;
        gameDialogFidgetUpdateDelay = 0;
        gameDialogFidgetLastUpdateTimestamp = 0;
        gameDialogRenderTalkingHead(nullptr, 0);
        _lipsFrmId = HeadFrameId::None;
        _lipsKey = nullptr;
        _lipsFp = nullptr;
        return;
    }

    HeadAnimation anim;
    switch (reaction) {
    case FIDGET_GOOD:
        anim = HeadAnimation::GoodPhonemes;
        break;
    case FIDGET_BAD:
        anim = HeadAnimation::BadPhonemes;
        break;
    default:
        anim = HeadAnimation::NeutralPhonemes;
        break;
    }

    if (_lipsFrmId != HeadFrameId::None) {
        if (anim != _phone_anim) {
            if (artUnlock(_lipsKey) == -1) {
                debugPrint("failure unlocking lips frame!\n");
            }
            _lipsKey = nullptr;
            _lipsFp = nullptr;
            _lipsFrmId = HeadFrameId::None;
        }
    }

    if (_lipsFp == nullptr) {
        _lipsFrmId = HeadFrameId::None;
    }

    if (_lipsFrmId == HeadFrameId::None) {
        _phone_anim = anim;
        _lipsFrmId = HeadFrmId(headFrmId.frameId().head, anim);
        _lipsFp = artLock(_lipsFrmId, &_lipsKey);
        if (_lipsFp == nullptr) {
            debugPrint("failure!\n");

            char stats[200];
            cachePrintStats(&gArtCache, stats, sizeof(stats));
            debugPrint("%s", stats);
        }
    }

    int fidgetCount = artGetFidgetCount(HeadFrmId(headFrmId.frameId().head, headAnimationFromHeadFidget(reaction)));
    if (fidgetCount == -1) {
        debugPrint("\tError - No available fidgets for given frame id\n");
        return;
    }

    int chance = randomBetween(1, 100) + _dialogue_seconds_since_last_input / 2;

    int fidget = fidgetCount;
    switch (fidgetCount) {
    case 1:
        fidget = 1;
        break;
    case 2:
        if (chance < 68) {
            fidget = 1;
        } else {
            fidget = 2;
        }
        break;
    case 3:
        _dialogue_seconds_since_last_input = 0;
        if (chance < 52) {
            fidget = 1;
        } else if (chance < 77) {
            fidget = 2;
        } else {
            fidget = 3;
        }
        break;
    }

    debugPrint("Choosing fidget %d out of %d\n", fidget, fidgetCount);

    if (gameDialogFidgetFrm != nullptr) {
        if (artUnlock(gameDialogFidgetFrmHandle) == -1) {
            debugPrint("failure!\n");
        }
    }

    gameDialogFidgetFrmId = HeadFrmId(headFrmId.frameId().head, headAnimationFromHeadFidget(reaction), fidget);
    gameDialogFidgetFrmCurrentFrame = 0;
    gameDialogFidgetFrm = artLock(gameDialogFidgetFrmId, &gameDialogFidgetFrmHandle);
    if (gameDialogFidgetFrm == nullptr) {
        debugPrint("failure!\n");

        char stats[200];
        cachePrintStats(&gArtCache, stats, sizeof(stats));
        debugPrint("%s", stats);
    }

    gameDialogFidgetLastUpdateTimestamp = 0;
    gameDialogFidgetReaction = reaction;
    gameDialogFidgetUpdateDelay = 1000 / artGetFramesPerSecond(gameDialogFidgetFrm);
}

static void gameDialogBlitIsoWindowToDisplayBuffer()
{
    unsigned char* src = windowGetBuffer(gIsoWindow);

    // Usually rendering functions use `screenGetWidth`/`screenGetHeight` to
    // determine rendering position. However in this case `windowGetHeight`
    // is a must because isometric window's height can either include
    // interface bar or not. Offset is updated accordingly (332 -> 232, the
    // missing 100 is interface bar height, which is already accounted for
    // when we're using `windowGetHeight`). `windowGetWidth` is used for
    // consistency.
    blitBufferToBuffer(
        src + ((windowGetHeight(gIsoWindow) - 232) / 2) * windowGetWidth(gIsoWindow) + (windowGetWidth(gIsoWindow) - 388) / 2,
        388,
        200,
        windowGetWidth(gIsoWindow),
        gameDialogDisplayBuffer,
        GAME_DIALOG_WINDOW_WIDTH);
}

static void gameDialogRenderMapInDisplayBuffer()
{
    int oldCenterTile = gCenterTile;
    bool changedCenter = false;
    bool mapRefreshed = false;

    if (gGameDialogSpeaker != nullptr
        && objectTypeFromPid(gGameDialogSpeaker->pid) != OBJ_TYPE_ITEM
        && gGameDialogSpeaker->elevation == gElevation
        && gGameDialogSpeaker->tile != oldCenterTile) {
        if (tileSetCenter(gGameDialogSpeaker->tile,
                TILE_SET_CENTER_REFRESH_WINDOW | TILE_SET_CENTER_FLAG_IGNORE_SCROLL_RESTRICTIONS)
            == 0) {
            mapRefreshed = true;
            changedCenter = gCenterTile != oldCenterTile;
        } else {
            // Legacy map borders cannot be bypassed. Preserve the old
            // progressive behavior and capture from the closest valid center.
            _tile_scroll_to(gGameDialogSpeaker->tile, 2);
            changedCenter = gCenterTile != oldCenterTile;
            mapRefreshed = changedCenter;
        }
    }

    if (!mapRefreshed) {
        tileWindowRefresh();
    }

    gameDialogBlitIsoWindowToDisplayBuffer();

    if (changedCenter) {
        tileSetCenter(oldCenterTile,
            TILE_SET_CENTER_REFRESH_WINDOW | TILE_SET_CENTER_FLAG_IGNORE_SCROLL_RESTRICTIONS);
    }
}

// 0x447598
void gameDialogWaitForFidgetToComplete()
{
    if (gameDialogFidgetFrm == nullptr) {
        return;
    }

    if (gameDialogWindow == -1) {
        return;
    }

    debugPrint("Waiting for fidget to complete...\n");

    while (artGetFrameCount(gameDialogFidgetFrm) > gameDialogFidgetFrmCurrentFrame) {
        sharedFpsLimiter.mark();

        if (getTicksSince(gameDialogFidgetLastUpdateTimestamp) >= gameDialogFidgetUpdateDelay) {
            gameDialogRenderTalkingHead(gameDialogFidgetFrm, gameDialogFidgetFrmCurrentFrame);
            gameDialogFidgetLastUpdateTimestamp = getTicks();
            gameDialogFidgetFrmCurrentFrame++;
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    gameDialogFidgetFrmCurrentFrame = 0;
}

// 0x447614
void _gdPlayTransition(HeadAnimation anim)
{
    if (gameDialogFidgetFrm == nullptr) {
        return;
    }

    if (gameDialogWindow == -1) {
        return;
    }

    mouseHideCursor();

    debugPrint("Starting transition...\n");

    gameDialogWaitForFidgetToComplete();

    if (gameDialogFidgetFrm != nullptr) {
        if (artUnlock(gameDialogFidgetFrmHandle) == -1) {
            debugPrint("\tError unlocking fidget in transition func...");
        }
        gameDialogFidgetFrm = nullptr;
    }

    CacheEntry* headFrmHandle;
    const HeadFrmId headFid = HeadFrmId(gGameDialogHeadFrmId.frameId().head, anim);
    Art* headFrm = artLock(headFid, &headFrmHandle);
    if (headFrm == nullptr) {
        debugPrint("\tError locking transition...\n");
    }

    unsigned int delay = 1000 / artGetFramesPerSecond(headFrm);

    int frame = 0;
    unsigned int time = 0;
    while (frame < artGetFrameCount(headFrm)) {
        sharedFpsLimiter.mark();

        if (getTicksSince(time) >= delay) {
            gameDialogRenderTalkingHead(headFrm, frame);
            time = getTicks();
            frame++;
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    if (artUnlock(headFrmHandle) == -1) {
        debugPrint("\tError unlocking transition...\n");
    }

    debugPrint("Finished transition...\n");
    mouseShowCursor();
}

// 0x447724
void _reply_arrow_up(int btn, int keyCode)
{
    if (_gdReplyTooBig) {
        gameMouseSetCursor(MOUSE_CURSOR_SMALL_ARROW_UP);
    }
}

// 0x447738
void _reply_arrow_down(int btn, int keyCode)
{
    if (_gdReplyTooBig) {
        gameMouseSetCursor(MOUSE_CURSOR_SMALL_ARROW_DOWN);
    }
}

// 0x44774C
void _reply_arrow_restore(int btn, int keyCode)
{
    gameMouseSetCursor(MOUSE_CURSOR_ARROW);
}

// demo_copy_title
// 0x447758
void _demo_copy_title(int win)
{
    _gd_replyWin = win;

    if (win == -1) {
        debugPrint("\nError: demo_copy_title: win invalid!");
        return;
    }

    int width = windowGetWidth(win);
    if (width < 1) {
        debugPrint("\nError: demo_copy_title: width invalid!");
        return;
    }

    int height = windowGetHeight(win);
    if (height < 1) {
        debugPrint("\nError: demo_copy_title: length invalid!");
        return;
    }

    if (gameDialogBackgroundWindow == -1) {
        debugPrint("\nError: demo_copy_title: dialogueBackWindow wasn't created!");
        return;
    }

    unsigned char* src = windowGetBuffer(gameDialogBackgroundWindow);
    if (src == nullptr) {
        debugPrint("\nError: demo_copy_title: couldn't get buffer!");
        return;
    }

    unsigned char* dest = windowGetBuffer(win);

    blitBufferToBuffer(src + 640 * 225 + 135, width, height, 640, dest, width);
}

// demo_copy_options
// 0x447818
void _demo_copy_options(int win)
{
    _gd_optionsWin = win;

    if (win == -1) {
        debugPrint("\nError: demo_copy_options: win invalid!");
        return;
    }

    int width = windowGetWidth(win);
    if (width < 1) {
        debugPrint("\nError: demo_copy_options: width invalid!");
        return;
    }

    int height = windowGetHeight(win);
    if (height < 1) {
        debugPrint("\nError: demo_copy_options: length invalid!");
        return;
    }

    if (gameDialogBackgroundWindow == -1) {
        debugPrint("\nError: demo_copy_options: dialogueBackWindow wasn't created!");
        return;
    }

    Rect windowRect;
    windowGetRect(gameDialogWindow, &windowRect);
    Rect bgRect;
    windowGetRect(gameDialogBackgroundWindow, &bgRect);
    windowRect.left -= bgRect.left;
    windowRect.top -= bgRect.top;

    unsigned char* src = windowGetBuffer(gameDialogWindow);
    if (src == nullptr) {
        debugPrint("\nError: demo_copy_options: couldn't get buffer!");
        return;
    }

    unsigned char* dest = windowGetBuffer(win);
    blitBufferToBuffer(src + 640 * (335 - windowRect.top) + 127, width, height, 640, dest, width);
}

// gDialogRefreshOptionsRect
// 0x447914
void _gDialogRefreshOptionsRect(int win, Rect* drawRect)
{
    if (drawRect == nullptr) {
        debugPrint("\nError: gDialogRefreshOptionsRect: drawRect NULL!");
        return;
    }

    if (win == -1) {
        debugPrint("\nError: gDialogRefreshOptionsRect: win invalid!");
        return;
    }

    if (gameDialogBackgroundWindow == -1) {
        debugPrint("\nError: gDialogRefreshOptionsRect: dialogueBackWindow wasn't created!");
        return;
    }

    Rect windowRect;
    windowGetRect(gameDialogWindow, &windowRect);
    Rect bgRect;
    windowGetRect(gameDialogBackgroundWindow, &bgRect);
    windowRect.left -= bgRect.left;
    windowRect.top -= bgRect.top;

    unsigned char* src = windowGetBuffer(gameDialogWindow);
    if (src == nullptr) {
        debugPrint("\nError: gDialogRefreshOptionsRect: couldn't get buffer!");
        return;
    }

    if (drawRect->top >= drawRect->bottom) {
        debugPrint("\nError: gDialogRefreshOptionsRect: Invalid Rect (too many options)!");
        return;
    }

    if (drawRect->left >= drawRect->right) {
        debugPrint("\nError: gDialogRefreshOptionsRect: Invalid Rect (too many options)!");
        return;
    }

    int destWidth = windowGetWidth(win);
    unsigned char* dest = windowGetBuffer(win);

    blitBufferToBuffer(
        src + (640 * (335 - windowRect.top) + 127) + (640 * drawRect->top + drawRect->left),
        drawRect->right - drawRect->left,
        drawRect->bottom - drawRect->top,
        640,
        dest + destWidth * drawRect->top,
        destWidth);
}

// 0x447A58
void gameDialogTicker()
{
    switch (dialogSwitchMode) {
    case GAME_DIALOG_MODE_SWITCH_TO_BARTER:
        _loop_cnt = -1;
        dialogSwitchMode = GAME_DIALOG_MODE_BARTER_ACTIVE;

        GameMode::enterGameMode(GameMode::kSpecial);

        _gdialog_window_destroy();
        gameDialogCreateBarterWindow();
        break;
    case GAME_DIALOG_MODE_TALK:
        _loop_cnt = -1;
        dialogSwitchMode = GAME_DIALOG_MODE_NONE;
        gameDialogDestroyBarterWindow();
        _gdialog_window_create();

        // NOTE: Uninline.
        gdUnhide();

        if (_gd_optionsWin != -1) {
            // SFALL: Fix for the player's money not being displayed in the
            // dialog window after leaving the barter/combat control interface.
            gameDialogRenderCaps();
        }

        // CE: Start the reply speech that was held back while the barter screen
        // was open (see `gameDialogDeferReplySpeech`), now that the dialog
        // window is back and the reply text is visible again.
        if (gameDialogDeferReplySpeech) {
            gameDialogDeferReplySpeech = false;
            if (dialogReplyMessageListId > 0) {
                _scr_get_msg_str_speech(dialogReplyMessageListId, dialogReplyMessageId, 1);
                // CE: Rewind to the first page so the redraw matches the speech,
                // which restarts from the top. `_gdProcessUpdate` already
                // advanced the offset when it drew the reply before barter.
                dialogReplyTextOffset = 0;
                gameDialogRenderReply();
            }
        }

        break;
    case GAME_DIALOG_MODE_SWITCH_TO_PARTY_CONTROL:
        _loop_cnt = -1;
        dialogSwitchMode = GAME_DIALOG_MODE_PARTY_CONTROL_ACTIVE;
        _gdialog_window_destroy();
        partyMemberControlWindowInit();
        break;
    case GAME_DIALOG_MODE_SWITCH_TO_PARTY_CUSTOMIZATION:
        _loop_cnt = -1;
        dialogSwitchMode = GAME_DIALOG_MODE_PARTY_CUSTOMIZATION_ACTIVE;
        _gdialog_window_destroy();
        partyMemberCustomizationWindowInit();
        break;
    default:
        break;
    }

    if (gameDialogFidgetFrm == nullptr) {
        return;
    }

    if (gameDialogLipSyncStarted) {
        lipsTicker();

        if (gLipsPhonemeChanged) {
            int phoneme = gLipsCurrentPhoneme < PHONEME_COUNT ? gLipsCurrentPhoneme : 0;
            gameDialogRenderTalkingHead(_lipsFp, _head_phoneme_lookup[phoneme]);
            gLipsPhonemeChanged = false;
        }

        if (!soundIsPlaying(gLipsData.sound)) {
            gameDialogEndLips();
            gameDialogRenderTalkingHead(_lipsFp, 0);
            _can_start_new_fidget = true;
            _dialogue_seconds_since_last_input = 3;
            gameDialogFidgetFrmCurrentFrame = 0;
        }
        return;
    }

    if (_can_start_new_fidget) {
        if (getTicksSince(gameDialogFidgetLastUpdateTimestamp) >= _tocksWaiting) {
            _can_start_new_fidget = false;
            _dialogue_seconds_since_last_input += _tocksWaiting / 1000;
            _tocksWaiting = 1000 * (randomBetween(0, 3) + 4);
            _gdSetupFidget(gameDialogFidgetFrmId, gameDialogFidgetFrmId.fidget());
        }
        return;
    }

    if (getTicksSince(gameDialogFidgetLastUpdateTimestamp) >= gameDialogFidgetUpdateDelay) {
        if (artGetFrameCount(gameDialogFidgetFrm) <= gameDialogFidgetFrmCurrentFrame) {
            gameDialogRenderTalkingHead(gameDialogFidgetFrm, 0);
            _can_start_new_fidget = true;
        } else {
            gameDialogRenderTalkingHead(gameDialogFidgetFrm, gameDialogFidgetFrmCurrentFrame);
            gameDialogFidgetLastUpdateTimestamp = getTicks();
            gameDialogFidgetFrmCurrentFrame += 1;
        }
    }
}

// FIXME: Due to the bug in `_gdProcessChoice` this function can receive invalid
// reaction value (50 instead of expected -1, 0, 1). It's handled gracefully by
// the game.
//
// 0x447CA0
void _talk_to_critter_reacts(int reaction)
{
    int reactionIndex = reaction + 1;

    debugPrint("Dialogue Reaction: ");
    if (reactionIndex < 3) {
        debugPrint("%s\n", _react_strs[reactionIndex]);
    }

    int reactionCode = reaction + 50;
    _dialogue_seconds_since_last_input = 0;

    switch (reactionCode) {
    case GAME_DIALOG_REACTION_GOOD:
        switch (gameDialogFidgetReaction) {
        case FIDGET_GOOD:
            _gdPlayTransition(HeadAnimation::VeryGoodReaction);
            _gdSetupFidget(gGameDialogHeadFrmId, FIDGET_GOOD);
            break;
        case FIDGET_NEUTRAL:
            _gdPlayTransition(HeadAnimation::NeutralToGood);
            _gdSetupFidget(gGameDialogHeadFrmId, FIDGET_GOOD);
            break;
        case FIDGET_BAD:
            _gdPlayTransition(HeadAnimation::BadToNeutral);
            _gdSetupFidget(gGameDialogHeadFrmId, FIDGET_NEUTRAL);
            break;
        default:
            break;
        }
        break;
    case GAME_DIALOG_REACTION_NEUTRAL:
        break;
    case GAME_DIALOG_REACTION_BAD:
        switch (gameDialogFidgetReaction) {
        case FIDGET_GOOD:
            _gdPlayTransition(HeadAnimation::GoodToNeutral);
            _gdSetupFidget(gGameDialogHeadFrmId, FIDGET_NEUTRAL);
            break;
        case FIDGET_NEUTRAL:
            _gdPlayTransition(HeadAnimation::NeutralToBad);
            _gdSetupFidget(gGameDialogHeadFrmId, FIDGET_BAD);
            break;
        case FIDGET_BAD:
            _gdPlayTransition(HeadAnimation::VeryBadReaction);
            _gdSetupFidget(gGameDialogHeadFrmId, FIDGET_BAD);
            break;
        default:
            break;
        }
        break;
    }
}

// 0x447D98
void _gdialog_scroll_subwin(int windowIdx, bool scrollUp, const unsigned char* windowFrmData, unsigned char* windowBuf, const unsigned char* bgWindowBuf, int bgWindowHeight, bool instantScrollUp)
{
    constexpr int stripHeight = 10;
    int windowHeight = windowGetHeight(windowIdx);
    int height = windowHeight;
    unsigned char* dest = windowBuf;
    Rect rect;

    const int delayMs = std::max(static_cast<int>(33.0 / settings.ui.anim_speed), 1);

    if (scrollUp) {
        rect.left = 0;
        rect.right = GAME_DIALOG_WINDOW_WIDTH - 1;
        rect.bottom = windowHeight - 1;

        int strips = windowHeight / stripHeight;
        if (instantScrollUp) {
            rect.top = stripHeight;
            strips = 0;
        } else {
            rect.top = strips * stripHeight;
            height = windowHeight % stripHeight;
            dest += GAME_DIALOG_WINDOW_WIDTH * rect.top;
        }

        for (; strips >= 0; strips--) {
            if (!instantScrollUp) {
                sharedFpsLimiter.mark();
            }

            soundContinueAll();
            blitBufferToBuffer(windowFrmData,
                GAME_DIALOG_WINDOW_WIDTH,
                height,
                GAME_DIALOG_WINDOW_WIDTH,
                dest,
                GAME_DIALOG_WINDOW_WIDTH);
            rect.top -= stripHeight;
            windowRefreshRect(windowIdx, &rect);
            height += stripHeight;
            dest -= stripHeight * (GAME_DIALOG_WINDOW_WIDTH);

            if (!instantScrollUp) {
                delay_ms(delayMs);
                renderPresent();
                sharedFpsLimiter.throttle();
            }
        }
    } else {
        rect.right = GAME_DIALOG_WINDOW_WIDTH - 1;
        rect.bottom = windowHeight - 1;
        rect.left = 0;
        rect.top = 0;

        int bgRowsRead = 0;
        for (int top = 0; top < windowHeight;) {
            sharedFpsLimiter.mark();

            soundContinueAll();

            int curStripHeight = std::min(stripHeight, windowHeight - top);

            // bg window overlap can be smaller than dialog; zero-fill the gap.
            int bgBlitHeight = std::min(curStripHeight, bgWindowHeight - bgRowsRead);
            if (bgBlitHeight > 0) {
                blitBufferToBuffer(bgWindowBuf,
                    GAME_DIALOG_WINDOW_WIDTH,
                    bgBlitHeight,
                    GAME_DIALOG_WINDOW_WIDTH,
                    dest,
                    GAME_DIALOG_WINDOW_WIDTH);
                bgWindowBuf += bgBlitHeight * GAME_DIALOG_WINDOW_WIDTH;
                bgRowsRead += bgBlitHeight;
            }
            int zeroHeight = curStripHeight - bgBlitHeight;
            if (zeroHeight > 0) {
                memset(dest + bgBlitHeight * GAME_DIALOG_WINDOW_WIDTH, 0,
                    zeroHeight * GAME_DIALOG_WINDOW_WIDTH);
            }

            top += curStripHeight;
            dest += curStripHeight * GAME_DIALOG_WINDOW_WIDTH;
            height -= curStripHeight;

            blitBufferToBuffer(windowFrmData,
                GAME_DIALOG_WINDOW_WIDTH,
                height,
                GAME_DIALOG_WINDOW_WIDTH,
                dest,
                GAME_DIALOG_WINDOW_WIDTH);

            windowRefreshRect(windowIdx, &rect);

            rect.top += curStripHeight;

            delay_ms(delayMs);

            renderPresent();
            sharedFpsLimiter.throttle();
        }
    }
}

// 0x447F64
int _text_num_lines(const char* text, int maxWidth)
{
    int width = fontGetStringWidth(text);

    int lineCount = 0;
    while (width > 0) {
        width -= maxWidth;
        lineCount++;
    }

    return lineCount;
}

// NOTE: Inlined.
//
// 0x447F80
static int text_to_rect_wrapped(unsigned char* buffer, Rect* rect, const char* string, int* textOffset, int height, int pitch, ColorWithFlags color)
{
    return gameDialogDrawText(buffer, rect, string, textOffset, height, pitch, color, 1);
}

// display_msg
// 0x447FA0
static int gameDialogDrawText(unsigned char* buffer, Rect* rect, const char* string, int* textOffset, int height, int pitch, ColorWithFlags color, int draw)
{
    if (string == nullptr) {
        if (textOffset != nullptr) {
            *textOffset = 0;
        }
        return rect->top;
    }

    std::string mutableString(string);
    char* mutableText = mutableString.data();

    char* start;
    if (textOffset != nullptr) {
        start = mutableText + *textOffset;
    } else {
        start = mutableText;
    }

    int maxWidth = rect->right - rect->left;
    char* end = nullptr;
    while (start != nullptr && *start != '\0') {
        if (fontGetStringWidth(start) > maxWidth) {
            end = start + 1;
            while (*end != '\0' && *end != ' ') {
                end++;
            }

            if (*end != '\0') {
                char* lookahead = end + 1;
                while (lookahead != nullptr) {
                    while (*lookahead != '\0' && *lookahead != ' ') {
                        lookahead++;
                    }

                    if (*lookahead == '\0') {
                        lookahead = nullptr;
                    } else {
                        *lookahead = '\0';
                        if (fontGetStringWidth(start) >= maxWidth) {
                            *lookahead = ' ';
                            lookahead = nullptr;
                        } else {
                            end = lookahead;
                            *lookahead = ' ';
                            lookahead++;
                        }
                    }
                }

                if (*end == ' ') {
                    *end = '\0';
                }
            } else {
                if (rect->bottom - fontGetLineHeight() < rect->top) {
                    return rect->top;
                }

                if (draw != 1 || start == mutableText) {
                    fontDrawText(buffer + pitch * rect->top + 10, start, maxWidth, pitch, color);
                } else {
                    fontDrawText(buffer + pitch * rect->top, start, maxWidth, pitch, color);
                }

                if (textOffset != nullptr) {
                    *textOffset += static_cast<int>(strlen(start)) + 1;
                }

                rect->top += height;
                return rect->top;
            }
        }

        if (fontGetStringWidth(start) > maxWidth) {
            debugPrint("\nError: display_msg: word too long!");
            break;
        }

        if (draw != 0) {
            if (rect->bottom - fontGetLineHeight() < rect->top) {
                if (end != nullptr && *end == '\0') {
                    *end = ' ';
                }
                return rect->top;
            }

            unsigned char* dest;
            if (draw != 1 || start == mutableText) {
                dest = buffer + 10;
            } else {
                dest = buffer;
            }
            fontDrawText(dest + pitch * rect->top, start, maxWidth, pitch, color);
        }

        if (textOffset != nullptr && end != nullptr) {
            *textOffset += static_cast<int>(strlen(start)) + 1;
        }

        rect->top += height;

        if (end != nullptr) {
            start = end + 1;
            if (*end == '\0') {
                *end = ' ';
            }
            end = nullptr;
        } else {
            start = nullptr;
        }
    }

    if (textOffset != nullptr) {
        *textOffset = 0;
    }

    return rect->top;
}

int gameDialogGetBarterModifier()
{
    return gameDialogBarterModifier;
}

// 0x448214
void gameDialogSetBarterModifier(int modifier)
{
    gameDialogBarterModifier = modifier;
}

// gdialog_barter
// 0x44821C
int gameDialogBarter(int modifier)
{
    if (!_dialog_state_fix) {
        return -1;
    }

    gameDialogBarterModifier = modifier;
    gameDialogBarterButtonUpMouseUp(-1, -1);
    dialogMode = GAME_DIALOG_MODE_BARTER;
    dialogSwitchMode = GAME_DIALOG_MODE_SWITCH_TO_BARTER;

    return 0;
}

// 0x448268 barter_end_to_talk_to
void gameDialogEndBarter()
{
    _dialogQuit();
    _dialogClose();
    _updatePrograms();
    scriptWindowUpdateAll();
    dialogMode = GAME_DIALOG_MODE_TALK;
    dialogSwitchMode = GAME_DIALOG_MODE_TALK;
}

// 0x448290 gdialog_barter_create_win
int gameDialogCreateBarterWindow()
{
    FrmImage backgroundFrmImage;
    barterWindowExpanded = expandedBarterEnabled && backgroundFrmImage.lock(OBJ_TYPE_INTERFACE, expandedBarterFrmName());

    if (!barterWindowExpanded) {
        const InterfaceFrmId backgroundFid = gGameDialogSpeakerIsPartyMember
            ? InterfaceFrameId::TradeWindow
            : InterfaceFrameId::BarterWindow;
        backgroundFrmImage.lock(backgroundFid);
    }

    dialogMode = GAME_DIALOG_MODE_BARTER;

    if (!backgroundFrmImage.isLocked()) {
        return -1;
    }

    _dialogue_subwin_len = backgroundFrmImage.getHeight();

    // Effective overlap with the 480-tall dialog background. The expanded
    // barter frame extends kExpandedBarterExtraHeight pixels below the
    // background, so the bg-copy and scroll operations must be clipped to
    // what the background actually covers.l
    int bgOverlapHeight = barterWindowExpanded
        ? _dialogue_subwin_len - kExpandedBarterExtraHeight
        : _dialogue_subwin_len;

    Rect bgRect;
    windowGetRect(gameDialogBackgroundWindow, &bgRect);
    UniqueWindow win(windowCreate(bgRect.left,
        bgRect.top + GAME_DIALOG_WINDOW_HEIGHT - bgOverlapHeight,
        GAME_DIALOG_WINDOW_WIDTH,
        _dialogue_subwin_len,
        static_cast<ColorWithFlags>(256),
        WINDOW_DONT_MOVE_TOP | WINDOW_TRANSPARENT));
    if (win.get() == -1) return -1;

    unsigned char* windowBuffer = windowGetBuffer(win.get());
    Buffer2D subWinBuf { windowBuffer, windowGetWidth(win.get()), windowGetHeight(win.get()) };
    ConstBuffer2D bgBuf { windowGetBuffer(gameDialogBackgroundWindow), GAME_DIALOG_WINDOW_WIDTH, GAME_DIALOG_WINDOW_HEIGHT };
    blitBuffer2D(bgBuf, 0, GAME_DIALOG_WINDOW_HEIGHT - bgOverlapHeight, GAME_DIALOG_WINDOW_WIDTH, bgOverlapHeight, subWinBuf);

    _gdialog_scroll_subwin(win.get(), true, backgroundFrmImage.getData(), windowBuffer, nullptr, bgOverlapHeight);

    // TRADE
    int tradeBtn = createDialogRedButton(win.get(), 40, 162, nullptr, KEY_LOWERCASE_M);
    if (tradeBtn == -1) return -1;

    // TALK
    int talkBtn = createDialogRedButton(win.get(), 583, 161, nullptr, KEY_LOWERCASE_T);
    if (talkBtn == -1) return -1;

    UniqueObject playerTableObj;
    if (objectCreateWithFrmIdPid(playerTableObj, FrmId::Empty(), -1) == -1) return -1;
    playerTableObj->flags |= OBJECT_HIDDEN;

    UniqueObject bartererTableObj;
    if (objectCreateWithFrmIdPid(bartererTableObj, FrmId::Empty(), -1) == -1) return -1;
    bartererTableObj->flags |= OBJECT_HIDDEN;

    UniqueObject bartererTempObj;
    if (objectCreateWithFrmIdPid(bartererTempObj, FrmId(gGameDialogSpeaker), -1) == -1) return -1;
    bartererTempObj->flags |= OBJECT_HIDDEN | OBJECT_NO_SAVE;
    bartererTempObj->sid = -1;

    _barterBackgroundFrmImage = std::move(backgroundFrmImage);
    _gdialog_buttons[0] = tradeBtn;
    _gdialog_buttons[1] = talkBtn;
    gameDialogPlayerTableObj = playerTableObj.release();
    gameDialogBartererTableObj = bartererTableObj.release();
    _barterer_temp_obj = bartererTempObj.release();
    gameDialogWindow = win.release();
    return 0;
}

// 0x44854C gdialog_barter_destroy_win
void gameDialogDestroyBarterWindow()
{
    if (gameDialogWindow == -1) {
        return;
    }

    objectDestroy(_barterer_temp_obj, nullptr);
    objectDestroy(gameDialogBartererTableObj, nullptr);
    objectDestroy(gameDialogPlayerTableObj, nullptr);

    for (int index = 0; index < 9; index++) {
        buttonDestroy(_gdialog_buttons[index]);
        _gdialog_buttons[index] = -1;
    }

    if (_barterBackgroundFrmImage.isLocked()) {
        int bgOverlapHeight = barterWindowExpanded
            ? _dialogue_subwin_len - kExpandedBarterExtraHeight
            : _dialogue_subwin_len;

        unsigned char* backgroundWindowBuffer = windowGetBuffer(gameDialogBackgroundWindow)
            + GAME_DIALOG_WINDOW_WIDTH * (GAME_DIALOG_WINDOW_HEIGHT - bgOverlapHeight);

        unsigned char* windowBuffer = windowGetBuffer(gameDialogWindow);
        _gdialog_scroll_subwin(gameDialogWindow, false, _barterBackgroundFrmImage.getData(), windowBuffer, backgroundWindowBuffer, bgOverlapHeight);

        _barterBackgroundFrmImage.unlock();
    }

    windowDestroy(gameDialogWindow);
    gameDialogWindow = -1;

    barterWindowExpanded = false;

    aiAttemptWeaponReload(gGameDialogSpeaker, 0);
}

bool gameDialogIsBarterWindowExpanded()
{
    return barterWindowExpanded;
}

int gameDialogGetWindow()
{
    return windowGetWindow(gameDialogWindow) != nullptr ? gameDialogWindow : -1;
}

int gameDialogGetBackgroundWindow()
{
    return windowGetWindow(gameDialogBackgroundWindow) != nullptr ? gameDialogBackgroundWindow : -1;
}

// 0x448660 gdialog_barter_cleanup_tables
void gameDialogBarterCleanupTables()
{
    Inventory* inventory;
    int length;

    inventory = &(gameDialogPlayerTableObj->data.inventory);
    length = inventory->length;
    for (int index = 0; index < length; index++) {
        Object* item = inventory->items->item;
        int quantity = itemGetQuantity(gameDialogPlayerTableObj, item);
        itemMoveForce(gameDialogPlayerTableObj, gDude, item, quantity);
    }

    inventory = &(gameDialogBartererTableObj->data.inventory);
    length = inventory->length;
    for (int index = 0; index < length; index++) {
        Object* item = inventory->items->item;
        int quantity = itemGetQuantity(gameDialogBartererTableObj, item);
        itemMoveForce(gameDialogBartererTableObj, gGameDialogSpeaker, item, quantity);
    }

    if (_barterer_temp_obj != nullptr) {
        inventory = &(_barterer_temp_obj->data.inventory);
        length = inventory->length;
        for (int index = 0; index < length; index++) {
            Object* item = inventory->items->item;
            int quantity = itemGetQuantity(_barterer_temp_obj, item);
            itemMoveForce(_barterer_temp_obj, gGameDialogSpeaker, item, quantity);
        }
    }
}

// 0x448740
int partyMemberControlWindowInit()
{
    FrmImage backgroundFrmImage;
    if (!backgroundFrmImage.lock(InterfaceFrameId::PartyControlInterface)) {
        return -1;
    }

    unsigned char* backgroundData = backgroundFrmImage.getData();
    if (backgroundData == nullptr) {
        partyMemberControlWindowFree();
        return -1;
    }

    _dialogue_subwin_len = backgroundFrmImage.getHeight();
    Rect bgRect;
    windowGetRect(gameDialogBackgroundWindow, &bgRect);
    int controlWindowX = bgRect.left;
    int controlWindowY = bgRect.top + GAME_DIALOG_WINDOW_HEIGHT - _dialogue_subwin_len;
    gameDialogWindow = windowCreate(controlWindowX,
        controlWindowY,
        GAME_DIALOG_WINDOW_WIDTH,
        _dialogue_subwin_len,
        static_cast<ColorWithFlags>(256),
        WINDOW_DONT_MOVE_TOP);
    if (gameDialogWindow == -1) {
        partyMemberControlWindowFree();
        return -1;
    }

    unsigned char* windowBuffer = windowGetBuffer(gameDialogWindow);
    unsigned char* src = windowGetBuffer(gameDialogBackgroundWindow);
    blitBufferToBuffer(src + (GAME_DIALOG_WINDOW_WIDTH) * (GAME_DIALOG_WINDOW_HEIGHT - _dialogue_subwin_len), GAME_DIALOG_WINDOW_WIDTH, _dialogue_subwin_len, GAME_DIALOG_WINDOW_WIDTH, windowBuffer, GAME_DIALOG_WINDOW_WIDTH);
    _gdialog_scroll_subwin(gameDialogWindow, true, backgroundData, windowBuffer, nullptr, _dialogue_subwin_len);
    backgroundFrmImage.unlock();

    // TALK
    _gdialog_buttons[0] = createDialogRedButton(gameDialogWindow, 593, 41, nullptr, KEY_ESCAPE);
    if (_gdialog_buttons[0] == -1) {
        partyMemberControlWindowFree();
        return -1;
    }

    // TRADE
    _gdialog_buttons[1] = createDialogRedButton(gameDialogWindow, 593, 97, nullptr, KEY_LOWERCASE_D);
    if (_gdialog_buttons[1] == -1) {
        partyMemberControlWindowFree();
        return -1;
    }

    // USE BEST WEAPON
    _gdialog_buttons[2] = createDialogRedButton(gameDialogWindow, 236, 15, nullptr, KEY_LOWERCASE_W);
    if (_gdialog_buttons[2] == -1) {
        partyMemberControlWindowFree();
        return -1;
    }

    // USE BEST ARMOR
    _gdialog_buttons[3] = createDialogRedButton(gameDialogWindow, 235, 46, nullptr, KEY_LOWERCASE_A);
    if (_gdialog_buttons[3] == -1) {
        partyMemberControlWindowFree();
        return -1;
    }

    _control_buttons_start = 4;

    int dispositionButtonIndex = 3;

    for (int index = 0; index < 5; index++) {
        GameDialogButtonData* buttonData = &(gameDialogDispositionButtonsData[index]);

        Art* upButtonFrm = artLock(buttonData->upFrmId, &(buttonData->upFrmHandle));
        if (upButtonFrm == nullptr) {
            partyMemberControlWindowFree();
            return -1;
        }

        int width = artGetWidth(upButtonFrm);
        int height = artGetHeight(upButtonFrm);
        unsigned char* upButtonFrmData = artGetFrameData(upButtonFrm);

        Art* downButtonFrm = artLock(buttonData->downFrmId, &(buttonData->downFrmHandle));
        if (downButtonFrm == nullptr) {
            partyMemberControlWindowFree();
            return -1;
        }

        unsigned char* downButtonFrmData = artGetFrameData(downButtonFrm);

        Art* disabledButtonFrm = artLock(buttonData->disabledFrmId, &(buttonData->disabledFrmHandle));
        if (disabledButtonFrm == nullptr) {
            partyMemberControlWindowFree();
            return -1;
        }

        unsigned char* disabledButtonFrmData = artGetFrameData(disabledButtonFrm);

        dispositionButtonIndex++;

        _gdialog_buttons[dispositionButtonIndex] = buttonCreate(gameDialogWindow,
            buttonData->x,
            buttonData->y,
            width,
            height,
            -1,
            -1,
            buttonData->keyCode,
            -1,
            upButtonFrmData,
            downButtonFrmData,
            nullptr,
            BUTTON_FLAG_TRANSPARENT | BUTTON_FLAG_NO_TOGGLE_OFF | BUTTON_FLAG_CHECKABLE);
        if (_gdialog_buttons[dispositionButtonIndex] == -1) {
            partyMemberControlWindowFree();
            return -1;
        }

        _win_register_button_disable(_gdialog_buttons[dispositionButtonIndex], disabledButtonFrmData, disabledButtonFrmData, disabledButtonFrmData);
        buttonSetCallbacks(_gdialog_buttons[dispositionButtonIndex], _gsound_med_butt_press, _gsound_med_butt_release);

        if (!partyMemberSupportsDisposition(gGameDialogSpeaker, static_cast<Disposition>(buttonData->value))) {
            buttonDisable(_gdialog_buttons[dispositionButtonIndex]);
        }
    }

    _win_group_radio_buttons(5, &(_gdialog_buttons[_control_buttons_start]));

    Disposition disposition = aiGetDisposition(gGameDialogSpeaker);
    _win_set_button_rest_state(_gdialog_buttons[_control_buttons_start + 4 - disposition], 1, 0);

    partyMemberControlWindowUpdate();

    dialogMode = GAME_DIALOG_MODE_PARTY_CONTROL;

    windowRefresh(gameDialogWindow);

    return 0;
}

// 0x448C10
void partyMemberControlWindowFree()
{
    if (gameDialogWindow == -1) {
        return;
    }

    for (int index = 0; index < 9; index++) {
        buttonDestroy(_gdialog_buttons[index]);
        _gdialog_buttons[index] = -1;
    }

    for (int index = 0; index < 5; index++) {
        GameDialogButtonData* buttonData = &(gameDialogDispositionButtonsData[index]);

        if (buttonData->upFrmHandle) {
            artUnlock(buttonData->upFrmHandle);
            buttonData->upFrmHandle = nullptr;
        }

        if (buttonData->downFrmHandle) {
            artUnlock(buttonData->downFrmHandle);
            buttonData->downFrmHandle = nullptr;
        }

        if (buttonData->disabledFrmHandle) {
            artUnlock(buttonData->disabledFrmHandle);
            buttonData->disabledFrmHandle = nullptr;
        }
    }

    FrmImage backgroundFrmImage;
    if (backgroundFrmImage.lock(InterfaceFrameId::PartyControlInterface)) {
        _gdialog_scroll_subwin(gameDialogWindow, false, backgroundFrmImage.getData(), windowGetBuffer(gameDialogWindow), windowGetBuffer(gameDialogBackgroundWindow) + (GAME_DIALOG_WINDOW_WIDTH) * (480 - _dialogue_subwin_len), _dialogue_subwin_len);
    }

    windowDestroy(gameDialogWindow);
    gameDialogWindow = -1;
}

// 0x448D30
void partyMemberControlWindowUpdate()
{
    int oldFont = fontGetCurrent();
    fontSetCurrent(101);

    unsigned char* windowBuffer = windowGetBuffer(gameDialogWindow);
    int windowWidth = windowGetWidth(gameDialogWindow);

    FrmImage backgroundFrmImage;
    if (backgroundFrmImage.lock(InterfaceFrameId::PartyControlInterface)) {
        int width = backgroundFrmImage.getWidth();
        unsigned char* buffer = backgroundFrmImage.getData();

        // Clear "Weapon Used:".
        blitBufferToBuffer(buffer + width * 20 + 112, 110, fontGetLineHeight(), width, windowBuffer + windowWidth * 20 + 112, windowWidth);

        // Clear "Armor Used:".
        blitBufferToBuffer(buffer + width * 49 + 112, 110, fontGetLineHeight(), width, windowBuffer + windowWidth * 49 + 112, windowWidth);

        // Clear character preview.
        blitBufferToBuffer(buffer + width * 84 + 8, 70, 98, width, windowBuffer + windowWidth * 84 + 8, windowWidth);

        // Clear ?
        blitBufferToBuffer(buffer + width * 80 + 232, 132, 106, width, windowBuffer + windowWidth * 80 + 232, windowWidth);

        backgroundFrmImage.unlock();
    }

    MessageListItem messageListItem;
    char* text;
    char formattedText[256];

    // Render item in right hand.
    Object* item2 = critterGetItem2(gGameDialogSpeaker);
    text = item2 != nullptr ? itemGetName(item2) : getmsg(&gProtoMessageList, &messageListItem, 10);
    snprintf(formattedText, sizeof(formattedText), "%s", text);
    fontDrawText(windowBuffer + windowWidth * 20 + 112, formattedText, 110, windowWidth, COLOR_GREEN);

    // Render armor.
    Object* armor = critterGetArmor(gGameDialogSpeaker);
    text = armor != nullptr ? itemGetName(armor) : getmsg(&gProtoMessageList, &messageListItem, 10);
    snprintf(formattedText, sizeof(formattedText), "%s", text);
    fontDrawText(windowBuffer + windowWidth * 49 + 112, formattedText, 110, windowWidth, COLOR_GREEN);

    // Render preview.
    CacheEntry* previewHandle;
    const FrmId previewFrmId = FrmId(gGameDialogSpeaker, ANIM_STAND, ROTATION_SW);
    Art* preview = artLock(previewFrmId, &previewHandle);
    if (preview != nullptr) {
        int width = artGetWidth(preview, 0, ROTATION_SW);
        int height = artGetHeight(preview, 0, ROTATION_SW);
        unsigned char* buffer = artGetFrameData(preview, 0, ROTATION_SW);
        blitBufferToBufferTrans(buffer, width, height, width, windowBuffer + windowWidth * (132 - height / 2) + 39 - width / 2, windowWidth);
        artUnlock(previewHandle);
    }

    // Render hit points.
    int maximumHitPoints = critterGetStat(gGameDialogSpeaker, STAT_MAXIMUM_HIT_POINTS);
    int hitPoints = critterGetStat(gGameDialogSpeaker, STAT_CURRENT_HIT_POINTS);
    snprintf(formattedText, sizeof(formattedText), "%d/%d", hitPoints, maximumHitPoints);
    fontDrawText(windowBuffer + windowWidth * 96 + 240, formattedText, 115, windowWidth, COLOR_GREEN);

    // Render best skill.
    Skill bestSkill = partyMemberGetBestSkill(gGameDialogSpeaker);
    text = skillGetName(bestSkill);
    snprintf(formattedText, sizeof(formattedText), "%s", text);
    fontDrawText(windowBuffer + windowWidth * 113 + 240, formattedText, 115, windowWidth, COLOR_GREEN);

    // Render weight summary.
    int inventoryWeight = objectGetInventoryWeight(gGameDialogSpeaker);
    int carryWeight = critterGetStat(gGameDialogSpeaker, STAT_CARRY_WEIGHT);
    snprintf(formattedText, sizeof(formattedText), "%d/%d ", inventoryWeight, carryWeight);
    fontDrawText(windowBuffer + windowWidth * 131 + 240, formattedText, 115, windowWidth, critterIsEncumbered(gGameDialogSpeaker) ? COLOR_RED : COLOR_GREEN);

    // Render melee damage.
    int meleeDamage = critterGetStat(gGameDialogSpeaker, STAT_MELEE_DAMAGE);
    snprintf(formattedText, sizeof(formattedText), "%d", meleeDamage);
    fontDrawText(windowBuffer + windowWidth * 148 + 240, formattedText, 115, windowWidth, COLOR_GREEN);

    int actionPoints;
    if (isInCombat()) {
        actionPoints = gGameDialogSpeaker->data.critter.combat.ap;
    } else {
        actionPoints = critterGetStat(gGameDialogSpeaker, STAT_MAXIMUM_ACTION_POINTS);
    }
    int maximumActionPoints = critterGetStat(gGameDialogSpeaker, STAT_MAXIMUM_ACTION_POINTS);
    snprintf(formattedText, sizeof(formattedText), "%d/%d ", actionPoints, maximumActionPoints);
    fontDrawText(windowBuffer + windowWidth * 167 + 240, formattedText, 115, windowWidth, COLOR_GREEN);

    if (settings.ui.party_member_extra_info) {
        int level = partyMemberGetCurrentLevel(gGameDialogSpeaker);
        snprintf(formattedText, sizeof(formattedText), "Lvl: %d", level);
        int width = fontGetStringWidth(formattedText);
        fontDrawText(windowBuffer + windowWidth * 96 + 350 - width, formattedText, width, windowWidth, COLOR_GREEN);

        int armorClass = critterGetStat(gGameDialogSpeaker, STAT_ARMOR_CLASS);
        snprintf(formattedText, sizeof(formattedText), "AC: %d", armorClass);
        int armorClassX = 350 - fontGetStringWidth("AC:") - 20;
        fontDrawText(windowBuffer + windowWidth * 167 + armorClassX, formattedText, fontGetStringWidth(formattedText), windowWidth, COLOR_GREEN);

        if (queueFindFirstEvent(gGameDialogSpeaker, EVENT_TYPE_WITHDRAWAL) != nullptr) {
            const char* addictText = "Addict";
            int addictTextWidth = fontGetStringWidth(addictText);
            fontDrawText(windowBuffer + windowWidth * 148 + 350 - addictTextWidth, addictText, addictTextWidth, windowWidth, COLOR_RED);
        }
    }

    fontSetCurrent(oldFont);
    windowRefresh(gameDialogWindow);
}

// 0x44928C
void gameDialogCombatControlButtonOnMouseUp(int btn, int keyCode)
{
    dialogSwitchMode = GAME_DIALOG_MODE_SWITCH_TO_PARTY_CONTROL;
    dialogMode = GAME_DIALOG_MODE_PARTY_CONTROL;

    // NOTE: Uninline.
    gdHide();
}

// 0x4492D0
int _gdPickAIUpdateMsg(Object* critter)
{
    auto it = partyMemberCcMsgIds.find(critter->pid);
    if (it != partyMemberCcMsgIds.end()) {
        return randomBetween(it->second.start, it->second.end);
    }

    int pids[3];
    memcpy(pids, _Dogs, sizeof(pids));

    for (int index = 0; index < 3; index++) {
        if (critter->pid == pids[index]) {
            return 677 + randomBetween(0, 1);
        }
    }

    return 670 + randomBetween(0, 4);
}

void gameDialogSetPartyMemberCcMsgIds(int pid, int startMsgId, int endMsgId)
{
    assert(startMsgId <= endMsgId);

    partyMemberCcMsgIds[pid] = { startMsgId, endMsgId };
}

void gameDialogResetPartyMemberCcMsgIds()
{
    partyMemberCcMsgIds.clear();
}

// 0x449330
int _gdCanBarter()
{
    if (objectTypeFromPid(gGameDialogSpeaker->pid) != OBJ_TYPE_CRITTER) {
        return 1;
    }

    Proto* proto;
    if (protoGetProto(gGameDialogSpeaker->pid, &proto) == -1) {
        return 1;
    }

    if ((proto->critter.data.flags & CRITTER_BARTER) != CRITTER_NONE) {
        return 1;
    }

    MessageListItem messageListItem;

    // This person will not barter with you.
    messageListItem.num = 903;
    if (gGameDialogSpeakerIsPartyMember) {
        // This critter can't carry anything.
        messageListItem.num = 913;
    }

    if (!messageListGetItem(&gProtoMessageList, &messageListItem)) {
        debugPrint("\nError: gdialog: Can't find message!");
        return 0;
    }

    gameDialogRenderSupplementaryMessage(messageListItem.text);

    return 0;
}

// 0x4493B8
void partyMemberControlWindowHandleEvents()
{
    MessageListItem messageListItem;

    bool done = false;
    while (!done) {
        sharedFpsLimiter.mark();

        int keyCode = inputGetInput();
        if (keyCode != -1) {
            if (keyCode == KEY_CTRL_Q || keyCode == KEY_CTRL_X || keyCode == KEY_F10) {
                showQuitConfirmationDialog();
            }

            if (_game_user_wants_to_quit != GAME_QUIT_REQUEST_NONE) {
                break;
            }

            if (keyCode == KEY_LOWERCASE_W) {
                inventoryUnequip(gGameDialogSpeaker, HAND_RIGHT);

                Object* weapon = _ai_search_inven_weap(gGameDialogSpeaker, 0, nullptr);
                if (weapon != nullptr) {
                    inventoryEquip(gGameDialogSpeaker, weapon, HAND_RIGHT);
                    aiAttemptWeaponReload(gGameDialogSpeaker, 0);
                }

                int num = _gdPickAIUpdateMsg(gGameDialogSpeaker);
                char* msg = getmsg(&gProtoMessageList, &messageListItem, num);
                gameDialogRenderSupplementaryMessage(msg);
                partyMemberControlWindowUpdate();
            } else if (keyCode == 2098) {
                aiSetDisposition(gGameDialogSpeaker, DISPOSITION_AGGRESSIVE);
            } else if (keyCode == 2099) {
                aiSetDisposition(gGameDialogSpeaker, DISPOSITION_NONE);
                dialogMode = GAME_DIALOG_MODE_PARTY_CUSTOMIZATION;
                dialogSwitchMode = GAME_DIALOG_MODE_SWITCH_TO_PARTY_CUSTOMIZATION;
                done = true;
            } else if (keyCode == 2102) {
                aiSetDisposition(gGameDialogSpeaker, DISPOSITION_COWARD);
            } else if (keyCode == 2103) {
                aiSetDisposition(gGameDialogSpeaker, DISPOSITION_DEFENSIVE);
            } else if (keyCode == 2111) {
                aiSetDisposition(gGameDialogSpeaker, DISPOSITION_CUSTOM);
            } else if (keyCode == KEY_ESCAPE) {
                dialogSwitchMode = GAME_DIALOG_MODE_TALK;
                dialogMode = GAME_DIALOG_MODE_TALK;
                return;
            } else if (keyCode == KEY_LOWERCASE_A) {
                if (partyMemberPidCanEquipArmor(gGameDialogSpeaker->pid)) {
                    Object* armor = _ai_search_inven_armor(gGameDialogSpeaker);
                    if (armor != nullptr) {
                        inventoryEquip(gGameDialogSpeaker, armor, HAND_LEFT);
                    }
                }

                int num = _gdPickAIUpdateMsg(gGameDialogSpeaker);
                char* msg = getmsg(&gProtoMessageList, &messageListItem, num);
                gameDialogRenderSupplementaryMessage(msg);
                partyMemberControlWindowUpdate();
            } else if (keyCode == KEY_LOWERCASE_D) {
                if (_gdCanBarter()) {
                    dialogSwitchMode = GAME_DIALOG_MODE_SWITCH_TO_BARTER;
                    dialogMode = GAME_DIALOG_MODE_BARTER;
                    return;
                }
            } else if (keyCode == -2) {
                // CE: Minor improvement - handle on mouse up (just like other
                // buttons). Also fixed active button area (in original code
                // it's slightly smaller than the button itself).
                if ((mouseGetEvent() & MOUSE_EVENT_LEFT_BUTTON_UP) != 0) {
                    if (mouseHitTestInWindow(gameDialogWindow, 438, 156, 438 + 109, 156 + 28)) {
                        aiSetDisposition(gGameDialogSpeaker, DISPOSITION_NONE);
                        dialogMode = GAME_DIALOG_MODE_PARTY_CUSTOMIZATION;
                        dialogSwitchMode = GAME_DIALOG_MODE_SWITCH_TO_PARTY_CUSTOMIZATION;
                        done = true;
                    }
                }
            }
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }
}

// 0x4496A0
int partyMemberCustomizationWindowInit()
{
    if (!messageListInit(&customMessageList)) {
        return -1;
    }

    if (!messageListLoad(&customMessageList, "game\\custom.msg")) {
        return -1;
    }
    messageListRepositorySetStandardMessageList(STANDARD_MESSAGE_LIST_CUSTOM, &customMessageList);

    FrmImage backgroundFrmImage;
    if (!backgroundFrmImage.lock(InterfaceFrameId::PartyCustomInterface)) {
        partyMemberCustomizationMessageListReset();
        return -1;
    }

    unsigned char* backgroundFrmData = backgroundFrmImage.getData();
    if (backgroundFrmData == nullptr) {
        // FIXME: Leaking background.
        partyMemberCustomizationWindowFree();
        return -1;
    }

    _dialogue_subwin_len = backgroundFrmImage.getHeight();

    Rect bgRect;
    windowGetRect(gameDialogBackgroundWindow, &bgRect);
    int customizationWindowX = bgRect.left;
    int customizationWindowY = bgRect.top + GAME_DIALOG_WINDOW_HEIGHT - _dialogue_subwin_len;
    gameDialogWindow = windowCreate(customizationWindowX,
        customizationWindowY,
        GAME_DIALOG_WINDOW_WIDTH,
        _dialogue_subwin_len,
        static_cast<ColorWithFlags>(256),
        WINDOW_DONT_MOVE_TOP);
    if (gameDialogWindow == -1) {
        partyMemberCustomizationWindowFree();
        return -1;
    }

    unsigned char* windowBuffer = windowGetBuffer(gameDialogWindow);
    unsigned char* parentWindowBuffer = windowGetBuffer(gameDialogBackgroundWindow);
    blitBufferToBuffer(parentWindowBuffer + (GAME_DIALOG_WINDOW_HEIGHT - _dialogue_subwin_len) * GAME_DIALOG_WINDOW_WIDTH,
        GAME_DIALOG_WINDOW_WIDTH,
        _dialogue_subwin_len,
        GAME_DIALOG_WINDOW_WIDTH,
        windowBuffer,
        GAME_DIALOG_WINDOW_WIDTH);

    _gdialog_scroll_subwin(gameDialogWindow, true, backgroundFrmData, windowBuffer, nullptr, _dialogue_subwin_len);
    backgroundFrmImage.unlock();

    _gdialog_buttons[0] = createDialogRedButton(gameDialogWindow, 593, 101, nullptr, 13);
    if (_gdialog_buttons[0] == -1) {
        partyMemberCustomizationWindowFree();
        return -1;
    }

    int optionButton = 0;
    _custom_buttons_start = 1;

    for (auto& buttonDataRef : _custom_button_info) {
        GameDialogButtonData* buttonData = &buttonDataRef;

        Art* upButtonFrm = artLock(buttonData->upFrmId, &(buttonData->upFrmHandle));
        if (upButtonFrm == nullptr) {
            partyMemberCustomizationWindowFree();
            return -1;
        }

        int width = artGetWidth(upButtonFrm);
        int height = artGetHeight(upButtonFrm);
        unsigned char* upButtonFrmData = artGetFrameData(upButtonFrm);

        Art* downButtonFrm = artLock(buttonData->downFrmId, &(buttonData->downFrmHandle));
        if (downButtonFrm == nullptr) {
            partyMemberCustomizationWindowFree();
            return -1;
        }

        unsigned char* downButtonFrmData = artGetFrameData(downButtonFrm);

        optionButton++;
        _gdialog_buttons[optionButton] = buttonCreate(gameDialogWindow,
            buttonData->x,
            buttonData->y,
            width,
            height,
            -1,
            -1,
            -1,
            buttonData->keyCode,
            upButtonFrmData,
            downButtonFrmData,
            nullptr,
            BUTTON_FLAG_TRANSPARENT);
        if (_gdialog_buttons[optionButton] == -1) {
            partyMemberCustomizationWindowFree();
            return -1;
        }

        buttonSetCallbacks(_gdialog_buttons[optionButton], _gsound_med_butt_press, _gsound_med_butt_release);
    }

    _custom_current_selected[PARTY_MEMBER_CUSTOMIZATION_OPTION_AREA_ATTACK_MODE] = aiGetAreaAttackMode(gGameDialogSpeaker);
    _custom_current_selected[PARTY_MEMBER_CUSTOMIZATION_OPTION_RUN_AWAY_MODE] = aiGetRunAwayMode(gGameDialogSpeaker);
    _custom_current_selected[PARTY_MEMBER_CUSTOMIZATION_OPTION_BEST_WEAPON] = aiGetBestWeapon(gGameDialogSpeaker);
    _custom_current_selected[PARTY_MEMBER_CUSTOMIZATION_OPTION_DISTANCE] = aiGetDistance(gGameDialogSpeaker);
    _custom_current_selected[PARTY_MEMBER_CUSTOMIZATION_OPTION_ATTACK_WHO] = aiGetAttackWho(gGameDialogSpeaker);
    _custom_current_selected[PARTY_MEMBER_CUSTOMIZATION_OPTION_CHEM_USE] = aiGetChemUse(gGameDialogSpeaker);

    dialogMode = GAME_DIALOG_MODE_PARTY_CUSTOMIZATION;

    partyMemberCustomizationWindowUpdate();

    return 0;
}

// 0x449A10
void partyMemberCustomizationWindowFree()
{
    if (gameDialogWindow == -1) {
        partyMemberCustomizationMessageListReset();
        return;
    }

    for (int index = 0; index < 9; index++) {
        buttonDestroy(_gdialog_buttons[index]);
        _gdialog_buttons[index] = -1;
    }

    for (int index = 0; index < PARTY_MEMBER_CUSTOMIZATION_OPTION_COUNT; index++) {
        GameDialogButtonData* buttonData = &(_custom_button_info[index]);

        if (buttonData->upFrmHandle != nullptr) {
            artUnlock(buttonData->upFrmHandle);
            buttonData->upFrmHandle = nullptr;
        }

        if (buttonData->downFrmHandle != nullptr) {
            artUnlock(buttonData->downFrmHandle);
            buttonData->downFrmHandle = nullptr;
        }

        if (buttonData->disabledFrmHandle != nullptr) {
            artUnlock(buttonData->disabledFrmHandle);
            buttonData->disabledFrmHandle = nullptr;
        }
    }

    FrmImage backgroundFrmImage;
    if (backgroundFrmImage.lock(InterfaceFrameId::PartyCustomInterface)) {
        _gdialog_scroll_subwin(gameDialogWindow, false, backgroundFrmImage.getData(), windowGetBuffer(gameDialogWindow), windowGetBuffer(gameDialogBackgroundWindow) + (GAME_DIALOG_WINDOW_WIDTH) * (480 - _dialogue_subwin_len), _dialogue_subwin_len);
    }

    windowDestroy(gameDialogWindow);
    gameDialogWindow = -1;

    partyMemberCustomizationMessageListReset();
}

// 0x449B3C
void partyMemberCustomizationWindowHandleEvents()
{
    bool done = false;
    while (!done) {
        sharedFpsLimiter.mark();

        unsigned int keyCode = inputGetInput();
        if (keyCode != -1) {
            if (keyCode == KEY_CTRL_Q || keyCode == KEY_CTRL_X || keyCode == KEY_F10) {
                showQuitConfirmationDialog();
            }

            if (_game_user_wants_to_quit != GAME_QUIT_REQUEST_NONE) {
                break;
            }

            if (keyCode <= 5) {
                _gdCustomSelect(keyCode);
                partyMemberCustomizationWindowUpdate();
            } else if (keyCode == KEY_RETURN || keyCode == KEY_ESCAPE) {
                done = true;
                dialogSwitchMode = GAME_DIALOG_MODE_SWITCH_TO_PARTY_CONTROL;
                dialogMode = GAME_DIALOG_MODE_PARTY_CONTROL;
            }
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }
}

// 0x449BB4
void partyMemberCustomizationWindowUpdate()
{
    int oldFont = fontGetCurrent();
    fontSetCurrent(101);

    unsigned char* windowBuffer = windowGetBuffer(gameDialogWindow);
    int windowWidth = windowGetWidth(gameDialogWindow);

    FrmImage backgroundFrmImage;
    if (!backgroundFrmImage.lock(InterfaceFrameId::PartyCustomInterface)) {
        return;
    }

    int backgroundWidth = backgroundFrmImage.getWidth();
    int backgroundHeight = backgroundFrmImage.getHeight();
    unsigned char* backgroundData = backgroundFrmImage.getData();
    blitBufferToBuffer(backgroundData, backgroundWidth, backgroundHeight, backgroundWidth, windowBuffer, GAME_DIALOG_WINDOW_WIDTH);

    backgroundFrmImage.unlock();

    MessageListItem messageListItem;
    int num;
    char* msg;

    // BURST
    if (_custom_current_selected[PARTY_MEMBER_CUSTOMIZATION_OPTION_AREA_ATTACK_MODE] == -1) {
        // Not Applicable
        num = 99;
    } else {
        debugPrint("\nburst: %d", _custom_current_selected[PARTY_MEMBER_CUSTOMIZATION_OPTION_AREA_ATTACK_MODE]);
        num = _custom_settings[PARTY_MEMBER_CUSTOMIZATION_OPTION_AREA_ATTACK_MODE][_custom_current_selected[PARTY_MEMBER_CUSTOMIZATION_OPTION_AREA_ATTACK_MODE]].messageId;
    }

    msg = getmsg(&customMessageList, &messageListItem, num);
    fontDrawText(windowBuffer + windowWidth * 20 + 232, msg, 248, windowWidth, COLOR_GREEN);

    // RUN AWAY
    msg = getmsg(&customMessageList, &messageListItem, _custom_settings[PARTY_MEMBER_CUSTOMIZATION_OPTION_RUN_AWAY_MODE][_custom_current_selected[PARTY_MEMBER_CUSTOMIZATION_OPTION_RUN_AWAY_MODE]].messageId);
    fontDrawText(windowBuffer + windowWidth * 48 + 232, msg, 248, windowWidth, COLOR_GREEN);

    // WEAPON PREF
    msg = getmsg(&customMessageList, &messageListItem, _custom_settings[PARTY_MEMBER_CUSTOMIZATION_OPTION_BEST_WEAPON][_custom_current_selected[PARTY_MEMBER_CUSTOMIZATION_OPTION_BEST_WEAPON]].messageId);
    fontDrawText(windowBuffer + windowWidth * 78 + 232, msg, 248, windowWidth, COLOR_GREEN);

    // DISTANCE
    msg = getmsg(&customMessageList, &messageListItem, _custom_settings[PARTY_MEMBER_CUSTOMIZATION_OPTION_DISTANCE][_custom_current_selected[PARTY_MEMBER_CUSTOMIZATION_OPTION_DISTANCE]].messageId);
    fontDrawText(windowBuffer + windowWidth * 108 + 232, msg, 248, windowWidth, COLOR_GREEN);

    // ATTACK WHO
    msg = getmsg(&customMessageList, &messageListItem, _custom_settings[PARTY_MEMBER_CUSTOMIZATION_OPTION_ATTACK_WHO][_custom_current_selected[PARTY_MEMBER_CUSTOMIZATION_OPTION_ATTACK_WHO]].messageId);
    fontDrawText(windowBuffer + windowWidth * 137 + 232, msg, 248, windowWidth, COLOR_GREEN);

    // CHEM USE
    msg = getmsg(&customMessageList, &messageListItem, _custom_settings[PARTY_MEMBER_CUSTOMIZATION_OPTION_CHEM_USE][_custom_current_selected[PARTY_MEMBER_CUSTOMIZATION_OPTION_CHEM_USE]].messageId);
    fontDrawText(windowBuffer + windowWidth * 166 + 232, msg, 248, windowWidth, COLOR_GREEN);

    windowRefresh(gameDialogWindow);
    fontSetCurrent(oldFont);
}

// 0x449E64
void _gdCustomSelectRedraw(unsigned char* dest, int pitch, int type, int selectedIndex)
{
    MessageListItem messageListItem;

    fontSetCurrent(101);

    for (int index = 0; index < 6; index++) {
        PartyMemberOptionSetting* ptr = &(_custom_settings[type][index]);
        if (ptr->messageId != -1) {
            bool enabled = false;
            switch (type) {
            case PARTY_MEMBER_CUSTOMIZATION_OPTION_AREA_ATTACK_MODE:
                enabled = partyMemberSupportsAreaAttackMode(gGameDialogSpeaker, ptr->areaAttackMode);
                break;
            case PARTY_MEMBER_CUSTOMIZATION_OPTION_RUN_AWAY_MODE:
                enabled = partyMemberSupportsRunAwayMode(gGameDialogSpeaker, ptr->runAwayMode);
                break;
            case PARTY_MEMBER_CUSTOMIZATION_OPTION_BEST_WEAPON:
                enabled = partyMemberSupportsBestWeapon(gGameDialogSpeaker, ptr->bestWeapon);
                break;
            case PARTY_MEMBER_CUSTOMIZATION_OPTION_DISTANCE:
                enabled = partyMemberSupportsDistance(gGameDialogSpeaker, ptr->distanceMode);
                break;
            case PARTY_MEMBER_CUSTOMIZATION_OPTION_ATTACK_WHO:
                enabled = partyMemberSupportsAttackWho(gGameDialogSpeaker, ptr->attackWho);
                break;
            case PARTY_MEMBER_CUSTOMIZATION_OPTION_CHEM_USE:
                enabled = partyMemberSupportsChemUse(gGameDialogSpeaker, ptr->chemUse);
                break;
            }

            Color color;
            if (enabled) {
                if (index == selectedIndex) {
                    color = COLOR_LIGHT_YELLOW;
                } else {
                    color = COLOR_GREEN;
                }
            } else {
                color = COLOR_GREY_2;
            }

            const char* msg = getmsg(&customMessageList, &messageListItem, ptr->messageId);
            fontDrawText(dest + pitch * (fontGetLineHeight() * index + 42) + 42, msg, pitch - 84, pitch, color);
        }
    }
}

// 0x449FC0
int _gdCustomSelect(int option)
{
    int oldFont = fontGetCurrent();

    FrmImage backgroundFrmImage;
    if (!backgroundFrmImage.lock(InterfaceFrameId::PartyCustomSelect)) {
        return -1;
    }

    int backgroundFrmWidth = backgroundFrmImage.getWidth();
    int backgroundFrmHeight = backgroundFrmImage.getHeight();

    int selectWindowX = (screenGetWidth() - backgroundFrmWidth) / 2;
    int selectWindowY = (screenGetHeight() - backgroundFrmHeight) / 2;
    int win = windowCreate(selectWindowX, selectWindowY, backgroundFrmWidth, backgroundFrmHeight, static_cast<ColorWithFlags>(256), WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
    if (win == -1) return -1;
    UniqueWindow winPtr(win);

    unsigned char* windowBuffer = windowGetBuffer(win);
    blitBufferToBuffer(backgroundFrmImage.getData(),
        backgroundFrmWidth,
        backgroundFrmHeight,
        backgroundFrmWidth,
        windowBuffer,
        backgroundFrmWidth);

    backgroundFrmImage.unlock();

    int btn1 = createLittleRedButton(win, 70, 164, 500);
    if (btn1 == -1) return -1;

    int btn2 = createLittleRedButton(win, 176, 163, KEY_ESCAPE);
    if (btn2 == -1) return -1;

    fontSetCurrent(103);

    MessageListItem messageListItem;
    const char* msg;

    msg = getmsg(&customMessageList, &messageListItem, option);
    fontDrawText(windowBuffer + backgroundFrmWidth * 15 + 40, msg, backgroundFrmWidth, backgroundFrmWidth, COLOR_DARK_YELLOW);

    msg = getmsg(&customMessageList, &messageListItem, 10);
    fontDrawText(windowBuffer + backgroundFrmWidth * 163 + 88, msg, backgroundFrmWidth, backgroundFrmWidth, COLOR_DARK_YELLOW);

    msg = getmsg(&customMessageList, &messageListItem, 11);
    fontDrawText(windowBuffer + backgroundFrmWidth * 162 + 193, msg, backgroundFrmWidth, backgroundFrmWidth, COLOR_DARK_YELLOW);

    int value = _custom_current_selected[option];
    _gdCustomSelectRedraw(windowBuffer, backgroundFrmWidth, option, value);
    windowRefresh(win);

    int minX = selectWindowX + 42;
    int minY = selectWindowY + 42;
    int maxX = selectWindowX + backgroundFrmWidth - 42;
    int maxY = selectWindowY + backgroundFrmHeight - 42;

    bool done = false;
    unsigned int lastSelectionTimestamp = 0;
    while (!done) {
        sharedFpsLimiter.mark();

        int keyCode = inputGetInput();
        if (keyCode != -1) {
            if (keyCode == KEY_CTRL_Q || keyCode == KEY_CTRL_X || keyCode == KEY_F10) {
                showQuitConfirmationDialog();
            }

            if (_game_user_wants_to_quit != GAME_QUIT_REQUEST_NONE) {
                break;
            }

            if (keyCode == KEY_RETURN || keyCode == 500) {
                PartyMemberOptionSetting* ptr = &(_custom_settings[option][value]);
                _custom_current_selected[option] = value;

                switch (option) {
                case PARTY_MEMBER_CUSTOMIZATION_OPTION_AREA_ATTACK_MODE:
                    aiSetAreaAttackMode(gGameDialogSpeaker, ptr->areaAttackMode);
                    break;
                case PARTY_MEMBER_CUSTOMIZATION_OPTION_RUN_AWAY_MODE:
                    aiSetRunAwayMode(gGameDialogSpeaker, ptr->runAwayMode);
                    break;
                case PARTY_MEMBER_CUSTOMIZATION_OPTION_BEST_WEAPON:
                    aiSetBestWeapon(gGameDialogSpeaker, ptr->bestWeapon);
                    break;
                case PARTY_MEMBER_CUSTOMIZATION_OPTION_DISTANCE:
                    aiSetDistance(gGameDialogSpeaker, ptr->distanceMode);
                    break;
                case PARTY_MEMBER_CUSTOMIZATION_OPTION_ATTACK_WHO:
                    aiSetAttackWho(gGameDialogSpeaker, ptr->attackWho);
                    break;
                case PARTY_MEMBER_CUSTOMIZATION_OPTION_CHEM_USE:
                    aiSetChemUse(gGameDialogSpeaker, ptr->chemUse);
                    break;
                }

                if (keyCode != 500) {
                    soundPlayFile("ib1p1xx1");
                }
                done = true;
            } else if (keyCode == KEY_ESCAPE) {
                done = true;
            } else if (keyCode == -2) {
                if ((mouseGetEvent() & MOUSE_EVENT_LEFT_BUTTON_UP) != 0) {
                    // No need to use mouseHitTestInWindow as these values are already
                    // in screen coordinates.
                    if (_mouse_click_in(minX, minY, maxX, maxY)) {
                        int mouseX;
                        int mouseY;
                        mouseGetPosition(&mouseX, &mouseY);

                        int lineHeight = fontGetLineHeight();
                        int newValue = (mouseY - minY) / lineHeight;
                        if (newValue < 6) {
                            unsigned int timestamp = getTicks();
                            if (newValue == value) {
                                if (getTicksBetween(timestamp, lastSelectionTimestamp) < 250) {
                                    _custom_current_selected[option] = newValue;
                                    _gdCustomUpdateSetting(option, newValue);
                                    done = true;
                                }
                            } else {
                                PartyMemberOptionSetting* ptr = &(_custom_settings[option][newValue]);
                                if (ptr->messageId != -1) {
                                    bool enabled = false;
                                    switch (option) {
                                    case PARTY_MEMBER_CUSTOMIZATION_OPTION_AREA_ATTACK_MODE:
                                        enabled = partyMemberSupportsAreaAttackMode(gGameDialogSpeaker, ptr->areaAttackMode);
                                        break;
                                    case PARTY_MEMBER_CUSTOMIZATION_OPTION_RUN_AWAY_MODE:
                                        enabled = partyMemberSupportsRunAwayMode(gGameDialogSpeaker, ptr->runAwayMode);
                                        break;
                                    case PARTY_MEMBER_CUSTOMIZATION_OPTION_BEST_WEAPON:
                                        enabled = partyMemberSupportsBestWeapon(gGameDialogSpeaker, ptr->bestWeapon);
                                        break;
                                    case PARTY_MEMBER_CUSTOMIZATION_OPTION_DISTANCE:
                                        enabled = partyMemberSupportsDistance(gGameDialogSpeaker, ptr->distanceMode);
                                        break;
                                    case PARTY_MEMBER_CUSTOMIZATION_OPTION_ATTACK_WHO:
                                        enabled = partyMemberSupportsAttackWho(gGameDialogSpeaker, ptr->attackWho);
                                        break;
                                    case PARTY_MEMBER_CUSTOMIZATION_OPTION_CHEM_USE:
                                        enabled = partyMemberSupportsChemUse(gGameDialogSpeaker, ptr->chemUse);
                                        break;
                                    }

                                    if (enabled) {
                                        value = newValue;
                                        _gdCustomSelectRedraw(windowBuffer, backgroundFrmWidth, option, newValue);
                                        windowRefresh(win);
                                    }
                                }
                            }
                            lastSelectionTimestamp = timestamp;
                        }
                    }
                }
            }
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    fontSetCurrent(oldFont);
    return 0;
}

// 0x44A4E0
void _gdCustomUpdateSetting(int option, int value)
{
    switch (option) {
    case PARTY_MEMBER_CUSTOMIZATION_OPTION_AREA_ATTACK_MODE:
        aiSetAreaAttackMode(gGameDialogSpeaker, static_cast<AreaAttackMode>(value));
        break;
    case PARTY_MEMBER_CUSTOMIZATION_OPTION_RUN_AWAY_MODE:
        aiSetRunAwayMode(gGameDialogSpeaker, static_cast<RunAwayMode>(value));
        break;
    case PARTY_MEMBER_CUSTOMIZATION_OPTION_BEST_WEAPON:
        aiSetBestWeapon(gGameDialogSpeaker, static_cast<BestWeapon>(value));
        break;
    case PARTY_MEMBER_CUSTOMIZATION_OPTION_DISTANCE:
        aiSetDistance(gGameDialogSpeaker, static_cast<DistanceMode>(value));
        break;
    case PARTY_MEMBER_CUSTOMIZATION_OPTION_ATTACK_WHO:
        aiSetAttackWho(gGameDialogSpeaker, static_cast<AttackWho>(value));
        break;
    case PARTY_MEMBER_CUSTOMIZATION_OPTION_CHEM_USE:
        aiSetChemUse(gGameDialogSpeaker, static_cast<ChemUse>(value));
        break;
    }
}

// 0x44A52C
void gameDialogBarterButtonUpMouseUp(int btn, int keyCode)
{
    if (objectTypeFromPid(gGameDialogSpeaker->pid) != OBJ_TYPE_CRITTER) {
        return;
    }

    Script* script;
    if (scriptGetScript(gGameDialogSpeaker->sid, &script) == -1) {
        return;
    }

    Proto* proto;
    protoGetProto(gGameDialogSpeaker->pid, &proto);
    if ((proto->critter.data.flags & CRITTER_BARTER) != CRITTER_NONE) {
        if (gameDialogLipSyncStarted) {
            if (soundIsPlaying(gLipsData.sound)) {
                gameDialogEndLips();
            }
        }

        dialogSwitchMode = GAME_DIALOG_MODE_SWITCH_TO_BARTER;
        dialogMode = GAME_DIALOG_MODE_BARTER;

        // NOTE: Uninline.
        gdHide();
    } else {
        MessageListItem messageListItem;
        // This person will not barter with you.
        messageListItem.num = 903;
        if (gGameDialogSpeakerIsPartyMember) {
            // This critter can't carry anything.
            messageListItem.num = 913;
        }

        if (messageListGetItem(&gProtoMessageList, &messageListItem)) {
            gameDialogRenderSupplementaryMessage(messageListItem.text);
        } else {
            debugPrint("\nError: gdialog: Can't find message!");
        }
    }
}

// 0x44A62C
int _gdialog_window_create()
{
    const int screenWidth = GAME_DIALOG_WINDOW_WIDTH;
    if (_gdialog_window_created) return -1;

    for (int& btn : _gdialog_buttons)
        btn = -1;

    FrmImage backgroundFrmImage;
    const InterfaceFrmId backgroundFrmId = gGameDialogSpeakerIsPartyMember ? InterfaceFrameId::DialogTalkSubwindowParty : InterfaceFrameId::DialogTalkSubwindow;
    if (!backgroundFrmImage.lock(backgroundFrmId)) return -1;

    unsigned char* backgroundFrmData = backgroundFrmImage.getData();
    if (backgroundFrmData == nullptr) return -1;

    _dialogue_subwin_len = backgroundFrmImage.getHeight();

    Rect bgRect;
    const int subWinRelativeY = GAME_DIALOG_WINDOW_HEIGHT - _dialogue_subwin_len;
    windowGetRect(gameDialogBackgroundWindow, &bgRect);
    int win = windowCreate(bgRect.left, bgRect.top + subWinRelativeY,
        GAME_DIALOG_WINDOW_WIDTH, _dialogue_subwin_len, static_cast<ColorWithFlags>(256), WINDOW_DONT_MOVE_TOP);
    if (win == -1) return -1;

    UniqueWindow winPtr(win);

    Buffer2D windowBuf = windowGetBuffer2D(win);
    Buffer2D bgWindowBuf = windowGetBuffer2D(gameDialogBackgroundWindow);
    blitBuffer2D(bgWindowBuf, 0, subWinRelativeY, windowBuf.width, windowBuf.height, windowBuf);

    bool instantScroll = _dialogue_just_started != 0;
    if (_dialogue_just_started) {
        windowRefresh(gameDialogBackgroundWindow);
        _dialogue_just_started = 0;
    }
    _gdialog_scroll_subwin(win, true, backgroundFrmData, windowBuf.data, nullptr, _dialogue_subwin_len, instantScroll);

    // BARTER/TRADE
    int barterBtn = createDialogRedButton(win, 593, 41, gameDialogBarterButtonUpMouseUp);
    if (barterBtn == -1) return -1;

    // REVIEW
    int reviewBtn = createDialogReviewButton(win);
    if (reviewBtn == -1) return -1;

    if (gGameDialogSpeakerIsPartyMember) {
        // COMBAT CONTROL
        int controlBtn = createDialogRedButton(win, 593, 116, gameDialogCombatControlButtonOnMouseUp);
        if (controlBtn == -1) return -1;

        _gdialog_buttons[2] = controlBtn;
    }

    _gdialog_buttons[0] = barterBtn;
    _gdialog_buttons[1] = reviewBtn;
    gameDialogWindow = winPtr.release();
    _gdialog_window_created = true;
    return 0;
}

// 0x44A9D8
void _gdialog_window_destroy()
{
    if (gameDialogWindow == -1) {
        return;
    }

    for (int& btn : _gdialog_buttons) {
        buttonDestroy(btn);
        btn = -1;
    }

    int offset = (GAME_DIALOG_WINDOW_WIDTH) * (480 - _dialogue_subwin_len);
    unsigned char* backgroundWindowBuffer = windowGetBuffer(gameDialogBackgroundWindow) + offset;

    const InterfaceFrmId backgroundFrmId = gGameDialogSpeakerIsPartyMember ? InterfaceFrameId::DialogTalkSubwindowParty : InterfaceFrameId::DialogTalkSubwindow;

    FrmImage backgroundFrmImage;
    if (backgroundFrmImage.lock(backgroundFrmId)) {
        unsigned char* windowBuffer = windowGetBuffer(gameDialogWindow);
        _gdialog_scroll_subwin(gameDialogWindow, false, backgroundFrmImage.getData(), windowBuffer, backgroundWindowBuffer, _dialogue_subwin_len);
        windowDestroy(gameDialogWindow);
        _gdialog_window_created = false;
        gameDialogWindow = -1;
    }
}

static const char* expandedBarterFrmName()
{
    return gGameDialogSpeakerIsPartyMember ? "trade_e.frm" : "barter_e.frm";
}

// NOTE: Inlined.
//
// 0x44AAD8
static int talk_to_create_background_window()
{
    gameDialogUseHrArt = false;

    expandedBarterEnabled = settings.ui.expand_barter_window
        && screenGetHeight() >= GAME_DIALOG_WINDOW_HEIGHT + kExpandedBarterExtraHeight
        && FrmImage().lock(OBJ_TYPE_INTERFACE, expandedBarterFrmName());

    int backgroundWindowX = (screenGetWidth() - GAME_DIALOG_WINDOW_WIDTH) / 2;
    int backgroundWindowY = gameDialogGetBackgroundWindowY();

    gameDialogBackgroundWindow = windowCreate(backgroundWindowX,
        backgroundWindowY,
        GAME_DIALOG_WINDOW_WIDTH,
        GAME_DIALOG_WINDOW_HEIGHT,
        static_cast<ColorWithFlags>(256),
        WINDOW_DONT_MOVE_TOP | WINDOW_MODAL);

    if (gameDialogBackgroundWindow != -1) {
        return 0;
    }

    return -1;
}

// 0x44AB18
int gameDialogWindowRenderBackground()
{
    FrmImage backgroundFrmImage;

    if (gameDialogShouldUseHrArt()) {
        if (backgroundFrmImage.lock(OBJ_TYPE_INTERFACE, "HR_ALLTLK.FRM")
            && backgroundFrmImage.getWidth() >= GAME_DIALOG_WINDOW_WIDTH
            && backgroundFrmImage.getHeight() >= GAME_DIALOG_WINDOW_HEIGHT) {
            gameDialogUseHrArt = true;
        } else {
            backgroundFrmImage.unlock();
        }
    }

    if (!backgroundFrmImage.isLocked()) {
        const InterfaceFrmId backgroundFrmId = InterfaceFrameId::DialogScreenBackground;
        if (!backgroundFrmImage.lock(backgroundFrmId)) {
            return -1;
        }
        gameDialogUseHrArt = false;
    }

    ConstBuffer2D backgroundFrmBuf = backgroundFrmImage.getBuffer();
    Buffer2D windowBuf = windowGetBuffer2D(gameDialogBackgroundWindow);
    blitBuffer2D(backgroundFrmBuf, 0, 0, GAME_DIALOG_WINDOW_WIDTH, GAME_DIALOG_WINDOW_HEIGHT, windowBuf);

    if (!_dialogue_just_started) {
        windowRefresh(gameDialogBackgroundWindow);
    }

    return 0;
}

// 0x44ABA8
int _talkToRefreshDialogWindowRect(Rect* rect)
{
    FrmImage backgroundFrmImage;
    const InterfaceFrmId backgroundFrmId = gGameDialogSpeakerIsPartyMember ? InterfaceFrameId::DialogTalkSubwindowParty : InterfaceFrameId::DialogTalkSubwindow;
    if (!backgroundFrmImage.lock(backgroundFrmId)) {
        return -1;
    }

    int offset = 640 * rect->top + rect->left;

    unsigned char* windowBuffer = windowGetBuffer(gameDialogWindow);
    blitBufferToBuffer(backgroundFrmImage.getData() + offset,
        rect->right - rect->left,
        rect->bottom - rect->top,
        GAME_DIALOG_WINDOW_WIDTH,
        windowBuffer + offset,
        GAME_DIALOG_WINDOW_WIDTH);

    windowRefreshRect(gameDialogWindow, rect);

    return 0;
}

// 0x44AC68
void gameDialogRenderHighlight(unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* dest, int destX, int destY, int destPitch, Color* blendTable, Color* unusedGrayTable)
{
    (void)unusedGrayTable;

    int srcStep = srcPitch - srcWidth;
    int destStep = destPitch - srcWidth;

    dest += destPitch * destY + destX;

    for (int y = 0; y < srcHeight; y++) {
        for (int x = 0; x < srcWidth; x++) {
            unsigned char alpha = *src++;
            if (alpha != 0) {
                alpha = (COLOR_COUNT - alpha) >> 4;
            }

            unsigned char destColor = *dest;
            *dest++ = blendTable[COLOR_COUNT * alpha + destColor];
        }
        src += srcStep;
        dest += destStep;
    }
}

// 0x44ACFC
void gameDialogRenderTalkingHead(Art* headFrm, int frame)
{
    if (gameDialogWindow == -1) {
        return;
    }

    if (headFrm != nullptr) {
        if (frame == 0) {
            _totalHotx = 0;
        }

        FrmImage backgroundFrmImage;
        if (!backgroundFrmImage.lock(gameDialogBackgroundFrmId)) {
            debugPrint("\tError locking background in display...\n");
        }

        unsigned char* backgroundFrmData = backgroundFrmImage.getData();
        if (backgroundFrmData != nullptr) {
            blitBufferToBuffer(backgroundFrmData, 388, 200, 388, gameDialogDisplayBuffer, GAME_DIALOG_WINDOW_WIDTH);
        } else {
            debugPrint("\tError getting background data in display...\n");
        }

        int width = artGetWidth(headFrm, frame);
        int height = artGetHeight(headFrm, frame);
        unsigned char* data = artGetFrameData(headFrm, frame);

        int rotationOffsetX;
        int rotationOffsetY;
        artGetRotationOffsets(headFrm, ROTATION_NE, &rotationOffsetX, &rotationOffsetY);

        int frameOffsetX;
        int frameOffsetY;
        artGetFrameOffsets(headFrm, frame, ROTATION_NE, &frameOffsetX, &frameOffsetY);
        (void)frameOffsetY;

        _totalHotx += frameOffsetX;
        rotationOffsetX += _totalHotx;

        if (data != nullptr) {
            int destWidth = GAME_DIALOG_WINDOW_WIDTH;
            int destOffset = destWidth * (200 - height) + rotationOffsetX + (388 - width) / 2;
            if (destOffset + destWidth * rotationOffsetY > 0) {
                destOffset += destWidth * rotationOffsetY;
            }

            blitBufferToBufferTrans(
                data,
                width,
                height,
                width,
                gameDialogDisplayBuffer + destOffset,
                destWidth);
        } else {
            debugPrint("\tError getting head data in display...\n");
        }
    } else {
        gameDialogRenderMapInDisplayBuffer();
    }

    int yOffset = gameDialogHrArtYOffset();

    Rect headRect;
    headRect.left = 126;
    headRect.top = 14 + yOffset;
    headRect.right = 514;
    headRect.bottom = 214 + yOffset;

    unsigned char* dest = windowGetBuffer(gameDialogBackgroundWindow);

    gameDialogRenderHighlight(_upperHighlightFrmImage.getData(),
        _upperHighlightFrmImage.getWidth(),
        _upperHighlightFrmImage.getHeight(),
        _upperHighlightFrmImage.getWidth(),
        dest,
        426,
        15 + yOffset,
        GAME_DIALOG_WINDOW_WIDTH,
        _light_BlendTable,
        _light_GrayTable);

    gameDialogRenderHighlight(_lowerHighlightFrmImage.getData(),
        _lowerHighlightFrmImage.getWidth(),
        _lowerHighlightFrmImage.getHeight(),
        _lowerHighlightFrmImage.getWidth(),
        dest,
        129,
        214 + yOffset - _lowerHighlightFrmImage.getHeight() - 2,
        GAME_DIALOG_WINDOW_WIDTH,
        _dark_BlendTable,
        _dark_GrayTable);

    for (int index = 0; index < 8; ++index) {
        Rect rect = gameDialogGetBackgroundRect(index);
        int width = rect.right - rect.left;

        blitBufferToBufferTrans(_backgrndBufs[index],
            width,
            rect.bottom - rect.top,
            width,
            dest + GAME_DIALOG_WINDOW_WIDTH * rect.top + rect.left,
            GAME_DIALOG_WINDOW_WIDTH);
    }

    windowRefreshRect(gameDialogBackgroundWindow, &headRect);
}

// 0x44B080
void gameDialogHighlightsInit()
{
    for (int color = COLOR_FIRST; color < COLOR_COUNT; color++) {
        int rgb = Color2RGB(static_cast<Color>(color & COLOR_LAST));
        int r = (rgb & 0x7C00) >> 10;
        int g = (rgb & 0x3E0) >> 5;
        int b = rgb & 0x1F;
        _light_GrayTable[color] = static_cast<Color>((((r + 2 * g + 2 * b) / 10) >> 2) & COLOR_LAST);
        _dark_GrayTable[color] = static_cast<Color>((((r + g + b) / 10) >> 2) & COLOR_LAST);
    }

    _light_GrayTable[0] = COLOR_FIRST;
    _dark_GrayTable[0] = COLOR_FIRST;

    _light_BlendTable = _getColorBlendTable(COLOR_GREY);
    _dark_BlendTable = _getColorBlendTable(COLOR_OLIVE);

    _upperHighlightFrmImage.lock(InterfaceFrameId::DialogueUpperHighlight);
    _lowerHighlightFrmImage.lock(InterfaceFrameId::DialogueLowerHighlight);
}

// NOTE: Inlined.
//
// 0x44B1D4
static void gameDialogHighlightsExit()
{
    _freeColorBlendTable(COLOR_GREY);
    _freeColorBlendTable(COLOR_OLIVE);

    _upperHighlightFrmImage.unlock();
    _lowerHighlightFrmImage.unlock();
}

} // namespace fallout
