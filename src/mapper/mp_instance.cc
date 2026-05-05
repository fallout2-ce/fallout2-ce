#include "mapper/mp_instance.h"

#include <stdio.h>
#include <string.h>

#include "art.h"
#include "color.h"
#include "combat_ai.h"
#include "critter.h"
#include "debug.h"
#include "draw.h"
#include "game_sound.h"
#include "input.h"
#include "inventory.h"
#include "item.h"
#include "kb.h"
#include "mapper/mp_proto.h"
#include "mapper/mp_scrpt.h"
#include "memory.h"
#include "object.h"
#include "proto.h"
#include "proto_instance.h"
#include "scripts.h"
#include "settings.h"
#include "svga.h"
#include "text_font.h"
#include "tile.h"
#include "window_manager.h"
#include "window_manager_private.h"
#include "worldmap.h"

namespace fallout {

// proto_inst_who_obj
Object* proto_inst_who_obj = nullptr;

// edit_window_color
extern int edit_window_color;

constexpr int kInstMaxLightDistance = 8;
constexpr int kInstMaxLightPct = 100;
constexpr int kInstLightScale = 0x10000;

// Key codes for instance editor buttons
constexpr int kInstKeyFlags = 'f';
constexpr int kInstKeyLight = 'l';
constexpr int kInstKeyNewScript = 's';
constexpr int kInstKeyName = 'n';
constexpr int kInstKeyAddToInven = 'i';
constexpr int kInstKeyViewInven = 'v';
constexpr int kInstKeyClearInven = 'c';
constexpr int kInstKeyAiPacket = 'A';
constexpr int kInstKeyTeamNum = 'T';
constexpr int kInstKeyDone = KEY_ESCAPE;

static int protoInstItemEdit(Object* obj);
static int protoInstCritterEdit(Object* obj);
static int protoInstSceneryEdit(Object* obj);
static int protoInstWallEdit(Object* obj);
static int protoInstTileEdit(Object* obj);
static void protoInstMiscEdit(Object* obj);
static int protoInstSetupEdit(int* pWinId, Object* obj, int* pObjType, int* pObjProtoOff, int* pBufOff, const char* title);
static bool regModFlagsDialog(int* flags, int objectType);
static int regModInstFlags(Object* obj);

// proto_inst_edit_
void protoInstEdit(Object* obj)
{
    if (obj == nullptr) return;

    switch (PID_TYPE(obj->pid)) {
    case OBJ_TYPE_ITEM:
        protoInstItemEdit(obj);
        break;
    case OBJ_TYPE_CRITTER:
        protoInstCritterEdit(obj);
        break;
    case OBJ_TYPE_SCENERY:
        protoInstSceneryEdit(obj);
        break;
    case OBJ_TYPE_WALL:
        protoInstWallEdit(obj);
        break;
    case OBJ_TYPE_TILE:
        protoInstTileEdit(obj);
        break;
    case OBJ_TYPE_MISC:
        protoInstMiscEdit(obj);
        break;
    }
}

// proto_inst_setup_edit_
static int protoInstSetupEdit(int* pWinId, Object* obj, int* pObjType, int* pObjProtoOff, int* pBufOff, const char* title)
{
    Proto* proto;
    if (protoGetProto(obj->pid, &proto) == -1) {
        return -1;
    }

    constexpr int kWinWidth = 520;
    constexpr int kWinHeight = 350;
    constexpr int kWinX = 60;
    constexpr int kWinY = 10;
    constexpr int kArtWidth = 64;
    constexpr int kArtHeight = 48;

    int win = windowCreate(kWinX, kWinY, kWinWidth, kWinHeight, edit_window_color, WINDOW_MOVE_ON_TOP);
    if (win == -1) return -1;

    *pWinId = win;
    *pObjType = PID_TYPE(obj->pid);
    *pObjProtoOff = 0;

    windowDrawBorder(win);

    // Title
    int titleWidth = fontGetStringWidth(title);
    windowDrawText(win, title, titleWidth, (kWinWidth - titleWidth) / 2, 18, _colorTable[32767] | FONT_SHADOW);

    // Object type name
    const char* typeName = artGetObjectTypeName(*pObjType);
    int typeNameWidth = fontGetStringWidth(typeName);
    windowDrawText(win, typeName, typeNameWidth, (kWinWidth - typeNameWidth) / 2, 28, _colorTable[21140] | FONT_SHADOW);

    // Name button + value
    _win_register_text_button(win, 10, 44, -1, -1, -1, kInstKeyName, "Name", 0);
    const char* objName = objectGetName(obj);
    windowDrawText(win, objName, 130, 90, 48, _colorTable[32747] | FONT_SHADOW);

    // Flags button
    _win_register_text_button(win, 10, 65, -1, -1, -1, kInstKeyFlags, "Flags", 0);

    // Light Level button
    _win_register_text_button(win, 10, 86, -1, -1, -1, kInstKeyLight, "Light Level", 0);

    // Inventory buttons (only for items with container type 1 = misc)
    int y = 107;
    if (*pObjType == OBJ_TYPE_ITEM && itemGetType(obj) == ITEM_TYPE_MISC) {
        _win_register_text_button(win, 98, y, -1, -1, -1, kInstKeyAddToInven, "Add to Inven", 0);
        _win_register_text_button(win, 188, y, -1, -1, -1, kInstKeyViewInven, "View Inven", 0);
        _win_register_text_button(win, 298, y, -1, -1, -1, kInstKeyClearInven, "Clear Inven", 0);
    }

    // Art rendering area bounds
    *pBufOff = 13420;
    unsigned char* buf = windowGetBuffer(win);
    bufferDrawRect(buf + 12899, kWinWidth, 0, 0, 65, 49, _colorTable[0]);
    bufferFill(buf + *pBufOff, kArtWidth, kArtHeight, kWinWidth, _colorTable[21]);
    artRender(obj->fid, buf + *pBufOff, kArtWidth, kArtHeight, kWinWidth);

    // Script section
    int scriptY = y + 83;
    _win_register_text_button(win, 10, scriptY, -1, -1, -1, kInstKeyNewScript, "New Script", 0);

    char scriptName[64];
    scriptName[0] = '\0';
    int scriptColor = _colorTable[32747];

    Script* script;
    if (scriptGetScript(obj->sid, &script) != -1) {
        if (scriptsGetFileName(script->index, scriptName, sizeof(scriptName)) == -1) {
            strcpy(scriptName, "Error: Bad script index!");
            scriptColor = _colorTable[31744];
        }
    } else {
        strcpy(scriptName, "None");
    }
    windowDrawText(win, scriptName, 130, 100, scriptY + 4, scriptColor | FONT_SHADOW);

    // Done button
    y += 26;
    _win_register_text_button(win, 10, y + 80, -1, -1, -1, kInstKeyDone, "Done", 0);

    windowRefresh(win);
    return 0;
}

// reg_mod_flags_
// Original single-column vertical flag editor matching vanilla reg_mod_flags_ layout.
// Flags are stored in *flags as a 32-bit ObjectFlags bitfield (obj->flags).
static bool regModFlagsDialog(int* flags, int objectType)
{
    constexpr int kDlgWidth = 220;
    constexpr int kDlgHeight = 380;
    constexpr int kDlgX = 320;
    constexpr int kDlgY = 185;

    int win = windowCreate(kDlgX, kDlgY, kDlgWidth, kDlgHeight, edit_window_color, WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
    if (win == -1) return false;

    windowDrawBorder(win);

    struct FlagButton {
        int key;
        const char* name;
        int bit;
        bool showForAll; // false = depends on objectType
    };

    constexpr int kMaxFlags = 16;
    const FlagButton btnDefs[kMaxFlags] = {
        { '1', "Flat", OBJECT_FLAT, true },
        { '2', "No Block", OBJECT_NO_BLOCK, true },
        { '3', "MultiHex", OBJECT_MULTIHEX, true },
        { '4', "Trans None", OBJECT_TRANS_NONE, true },
        { '5', "Trans Wall", OBJECT_TRANS_WALL, true },
        { '6', "Trans Glass", OBJECT_TRANS_GLASS, true },
        { '7', "Trans Steam", OBJECT_TRANS_STEAM, true },
        { '8', "Trans Energy", OBJECT_TRANS_ENERGY, true },
        { '9', "Trans Red", OBJECT_TRANS_RED, true },
        { '0', "Shoot Thru", static_cast<int>(OBJECT_SHOOT_THRU), true },
        { '-', "Light Thru", OBJECT_LIGHT_THRU, true },
        { '_', "Wall Trans End", OBJECT_WALL_TRANS_END, false }, // wall/scenery only
        { '!', "No Highlight", OBJECT_NO_HIGHLIGHT, false }, // item only
        { '=', "Wall Light", -1, false }, // wall/scenery: pseudo-flag, handled elsewhere
        { '@', "Hidden Item", -1, false }, // item proto extended flag, not object-level
    };
    constexpr int kCommonCount = 11;

    // Build visible flag list based on object type
    int visibleIndex[kMaxFlags];
    int visibleCount = 0;
    for (int i = 0; i < kMaxFlags; i++) {
        bool visible = btnDefs[i].showForAll;
        if (!visible) {
            bool isWallScenery = (objectType == OBJ_TYPE_WALL || objectType == OBJ_TYPE_SCENERY);
            bool isItem = (objectType == OBJ_TYPE_ITEM);
            if (isWallScenery && (btnDefs[i].key == '_' || btnDefs[i].key == '=')) visible = true;
            if (isItem && btnDefs[i].key == '!') visible = true;
            // Hidden Item ('@') requires proto access, skip for simplicity
        }
        if (visible) {
            visibleIndex[visibleCount++] = i;
        }
    }

    int flagValues[kMaxFlags];
    constexpr int kBtnX = 10;
    constexpr int kValX = 130;
    constexpr int kItemH = 21;
    int yPos = 21;

    for (int vi = 0; vi < visibleCount; vi++) {
        int i = visibleIndex[vi];
        int bit = btnDefs[i].bit;
        if (bit != -1) {
            flagValues[i] = (*flags & bit) ? 1 : 0;
        } else {
            flagValues[i] = 0;
        }

        _win_register_text_button(win, kBtnX, yPos, -1, -1, -1, btnDefs[i].key, btnDefs[i].name, 0);

        windowDrawText(win,
            flagValues[i] ? "YES" : "NO",
            50,
            kValX,
            yPos + 4,
            flagValues[i] ? (_colorTable[32747] | FONT_SHADOW) : (_colorTable[992] | FONT_SHADOW));

        yPos += kItemH;
    }

    // Done button
    constexpr int kDoneKey = KEY_ESCAPE;
    _win_register_text_button(win, kBtnX, yPos + 10, -1, -1, -1, kDoneKey, "Done", 0);

    windowRefresh(win);

    while (true) {
        sharedFpsLimiter.mark();

        int key = inputGetInput();

        if (key == KEY_ESCAPE || key == KEY_RETURN) {
            // Write back only the flags that were visible (prevents stomping unrelated bits)
            int preserved = *flags;
            for (int vi = 0; vi < visibleCount; vi++) {
                int i = visibleIndex[vi];
                int bit = btnDefs[i].bit;
                if (bit != -1) {
                    if (flagValues[i])
                        preserved |= bit;
                    else
                        preserved &= ~bit;
                }
            }
            *flags = preserved;
            windowDestroy(win);
            return true;
        }

        for (int vi = 0; vi < visibleCount; vi++) {
            int i = visibleIndex[vi];
            if (key != btnDefs[i].key) continue;

            flagValues[i] = !flagValues[i];
            int rowY = 21 + vi * kItemH;
            int valColor = flagValues[i] ? (_colorTable[32747] | FONT_SHADOW) : (_colorTable[992] | FONT_SHADOW);

            unsigned char* buf = windowGetBuffer(win);
            int pitch = windowGetWidth(win);
            bufferFill(buf + pitch * (rowY + 4) + kValX, 50, fontGetLineHeight(), pitch, edit_window_color);
            windowDrawText(win, flagValues[i] ? "YES" : "NO", 50, kValX, rowY + 4, valColor);

            Rect refreshRect = { kValX, rowY + 4, kValX + 50, rowY + 4 + fontGetLineHeight() };
            windowRefreshRect(win, &refreshRect);
            break;
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }
}

// reg_mod_inst_flags_
// Reads obj->flags into a copy, shows the flag dialog, and applies changes back.
// Flat uses _obj_toggle_flat for proper side effects; all others are applied via bit ops.
static int regModInstFlags(Object* obj)
{
    int flags = obj->flags;
    int oldFlat = flags & OBJECT_FLAT;
    int objectType = PID_TYPE(obj->pid);

    if (regModFlagsDialog(&flags, objectType)) {
        bool flatChanged = ((oldFlat != 0) != ((flags & OBJECT_FLAT) != 0));
        obj->flags = flags;

        if (flatChanged) {
            _obj_toggle_flat(obj, nullptr);
        }

        Rect rect;
        objectGetRect(obj, &rect);
        tileWindowRefreshRect(&rect, obj->elevation);
        return 1;
    }
    return 0;
}

static int applyLightEdit(Object* obj)
{
    int lightDist = obj->lightDistance;
    if (win_get_num_i(&lightDist, 0, kInstMaxLightDistance, false, "Light distance in hexes (0-8)", 100, 100) == -1) {
        return 0;
    }

    int lightPct = (obj->lightIntensity * 100) / kInstLightScale;
    if (win_get_num_i(&lightPct, 0, kInstMaxLightPct, false, "Light intensity (0-100 percent)", 100, 100) == -1) {
        return 0;
    }

    int lightIntensity = (lightPct * kInstLightScale) / 100;
    objectSetLight(obj, lightDist, lightIntensity, nullptr);
    return 1;
}

static void applyNewScript(Object* obj, int scriptType, int winId, int scriptNameY)
{
    int scriptIndex = scr_choose(scriptType);
    if (scriptIndex != -1) {
        if (obj->sid != -1) {
            scriptRemove(obj->sid);
            obj->sid = -1;
        }
    }
    if (scriptIndex >= 0) {
        objectSetScript(obj, scriptIndex >> 24, scriptIndex & 0xFFFFFF);
    }

    char scriptName[64];
    Script* script;
    if (scriptGetScript(obj->sid, &script) != -1) {
        scriptsGetFileName(script->index, scriptName, sizeof(scriptName));
    } else {
        strcpy(scriptName, "None");
    }
    windowDrawText(winId, scriptName, 130, 90, scriptNameY, _colorTable[32747] | FONT_SHADOW);
    windowRefresh(winId);
}

// proto_inst_item_edit_
static int protoInstItemEdit(Object* obj)
{
    int winId;
    int objType;
    int objProtoOff;
    int bufOff;

    if (protoInstSetupEdit(&winId, obj, &objType, &objProtoOff, &bufOff, "Item Instance Editor") == -1) {
        return -1;
    }

    while (true) {
        sharedFpsLimiter.mark();

        int key = inputGetInput();

        if (key == KEY_ESCAPE || key == KEY_RETURN) {
            windowDestroy(winId);
            return 0;
        }

        if (key == kInstKeyFlags) {
            if (regModInstFlags(obj)) windowRefresh(winId);
        } else if (key == kInstKeyLight) {
            if (applyLightEdit(obj)) windowRefresh(winId);
        } else if (key == 'S' && obj->sid != -1) {
            scriptRemove(obj->sid);
            obj->sid = -1;
            windowRefresh(winId);
        } else if (key == kInstKeyNewScript) {
            applyNewScript(obj, 3, winId, 194);
        } else if (key == kInstKeyClearInven && itemGetType(obj) == ITEM_TYPE_MISC) {
            _obj_inven_free(&obj->data.inventory);
            windowRefresh(winId);
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }
}

// proto_inst_add_to_inven
static int protoInstAddToInven(int pid, int count)
{
    if (proto_inst_who_obj == nullptr) return -1;

    Proto* proto;
    if (protoGetProto(pid, &proto) == -1) return 0;

    Object* newObj;
    if (objectCreateWithFidPid(&newObj, proto->fid, pid) == -1) return 0;

    objectSetLocation(newObj, 0, 0, nullptr);

    if (itemAdd(proto_inst_who_obj, newObj, count) != 0) {
        win_timed_msg("Error adding obj to critter!", _colorTable[32747] | FONT_SHADOW);
        return 0;
    }

    _obj_disconnect(newObj, nullptr);
    return 0;
}

// proto_choose_pid_inven_fid
static int protoInstChoosePidInvenFid(Proto* proto)
{
    if (PID_TYPE(proto->pid) != 0) return -1;
    if (proto->item.inventoryFid == -1) return proto->fid;
    return proto->item.inventoryFid;
}

// proto_inst_add_to_inven - grid-based proto picker (original implementation)
static void protoInstChooseItemsForInvenGrid(Object* obj)
{
    proto_inst_who_obj = obj;
    protoChooseMultiPids(OBJ_TYPE_ITEM, protoInstChoosePidInvenFid, protoInstAddToInven);
    proto_inst_who_obj = nullptr;
}

// proto_inst_add_to_inven - list-based proto picker (simpler alternative)
static void protoInstChooseItemsForInvenList(Object* obj)
{
    proto_inst_who_obj = obj;

    constexpr int kMaxItems = 200;
    char** names = static_cast<char**>(internal_malloc(sizeof(char*) * kMaxItems));
    int* pids = static_cast<int*>(internal_malloc(sizeof(int) * kMaxItems));
    int count = 0;

    for (int pid = 0x00000001; count < kMaxItems; pid++) {
        Proto* proto;
        if (protoGetProto(pid, &proto) == -1) break;
        if (PID_TYPE(pid) != OBJ_TYPE_ITEM) continue;

        names[count] = static_cast<char*>(internal_malloc(64));
        snprintf(names[count], 64, "%s", protoGetName(pid));
        pids[count] = pid;
        count++;
    }

    int selection = _win_list_select("Pick item to add", names, count, nullptr, 80, 200, _colorTable[32747] | FONT_SHADOW);
    if (selection != -1) {
        int quantity = 1;
        win_get_num_i(&quantity, 1, 32000, false, "How many?", 100, 100);
        protoInstAddToInven(pids[selection], quantity);
    }

    for (int i = 0; i < count; i++) {
        internal_free(names[i]);
    }
    internal_free(names);
    internal_free(pids);
    proto_inst_who_obj = nullptr;
}

// proto_inst_critter_edit_
static int protoInstCritterEdit(Object* obj)
{
    int winId;
    int objType;
    int objProtoOff;
    int bufOff;

    for (;;) {
        if (protoInstSetupEdit(&winId, obj, &objType, &objProtoOff, &bufOff, "Critter Instance Editor") == -1) {
            return -1;
        }

        // Critter-specific buttons
        constexpr int kBaseY = 107;
        _win_register_text_button(winId, 10, kBaseY, -1, -1, -1, kInstKeyAddToInven, "Add to Inventory", 0);
        _win_register_text_button(winId, 10, kBaseY + 21, -1, -1, -1, kInstKeyViewInven, "View Inventory List", 0);
        _win_register_text_button(winId, 10, kBaseY + 42, -1, -1, -1, kInstKeyClearInven, "Clear Inventory", 0);

        // AI Packet
        constexpr int kAiY = kBaseY + 63;
        _win_register_text_button(winId, 10, kAiY, -1, -1, -1, kInstKeyAiPacket, "AI Packet", 0);
        const char* aiName = combat_ai_name(obj->data.critter.combat.aiPacket);
        if (aiName == nullptr) aiName = "<Error>";
        windowDrawText(winId, aiName, 80, 100, kAiY + 4, _colorTable[32747] | FONT_SHADOW);

        // Team Num
        constexpr int kTeamY = kAiY;
        _win_register_text_button(winId, 240, kTeamY, -1, -1, -1, kInstKeyTeamNum, "Team Num", 0);
        char teamStr[16];
        snprintf(teamStr, sizeof(teamStr), "%d", obj->data.critter.combat.team);
        windowDrawText(winId, teamStr, 80, 320, kTeamY + 4, _colorTable[32747] | FONT_SHADOW);

        windowRefresh(winId);

        bool needRedraw = false;
        while (true) {
            sharedFpsLimiter.mark();

            if (needRedraw) {
                windowRefresh(winId);
                needRedraw = false;
            }

            int key = inputGetInput();

            if (key == KEY_ESCAPE || key == KEY_RETURN) {
                windowDestroy(winId);
                return 0;
            }

            if (key == kInstKeyFlags) {
                if (regModInstFlags(obj)) needRedraw = true;
            } else if (key == kInstKeyLight) {
                if (applyLightEdit(obj)) needRedraw = true;
            } else if (key == 'S' && obj->sid != -1) {
                scriptRemove(obj->sid);
                obj->sid = -1;
                needRedraw = true;
            } else if (key == kInstKeyNewScript) {
                applyNewScript(obj, 4, winId, 194);
            } else if (key == kInstKeyAiPacket) {
                proto_pick_ai_packet(&obj->data.critter.combat.aiPacket);
                const char* newAiName = combat_ai_name(obj->data.critter.combat.aiPacket);
                if (newAiName == nullptr) newAiName = "<Error>";
                windowDrawText(winId, newAiName, 80, 100, kAiY + 4, _colorTable[32747] | FONT_SHADOW);
                needRedraw = true;
            } else if (key == kInstKeyTeamNum) {
                int team = obj->data.critter.combat.team;
                if (win_get_num_i(&team, 0, 32000, false, "Team Num", 100, 100) != -1) {
                    obj->data.critter.combat.team = static_cast<char>(team);
                    snprintf(teamStr, sizeof(teamStr), "%d", obj->data.critter.combat.team);
                    windowDrawText(winId, teamStr, 80, 240, kTeamY + 4, _colorTable[32747] | FONT_SHADOW);
                    needRedraw = true;
                }
            } else if (key == kInstKeyClearInven) {
                _obj_inven_free(&obj->data.inventory);
                needRedraw = true;
            } else if (key == kInstKeyAddToInven) {
                if (settings.mapper.use_grid_item_picker) {
                    protoInstChooseItemsForInvenGrid(obj);
                } else {
                    protoInstChooseItemsForInvenList(obj);
                }
                needRedraw = true;
            } else if (key == kInstKeyViewInven) {
                windowDestroy(winId);

                inventorySetDude(obj, obj->pid);
                inventoryOpen();
                inventoryResetDude();

                Object* rightHandItem = critterGetItem2(obj);
                int animCode = 0;
                if (rightHandItem != nullptr && itemGetType(rightHandItem) == ITEM_TYPE_WEAPON) {
                    animCode = weaponGetAnimationCode(rightHandItem);
                }
                obj->fid = buildFid(FID_TYPE(obj->fid), obj->fid & 0xFFF, obj->frame + 1, animCode, 0);
                tileWindowRefresh();

                break;
            }

            renderPresent();
            sharedFpsLimiter.throttle();
        }
    }
}

// proto_inst_wall_edit_
static int protoInstWallEdit(Object* obj)
{
    int winId;
    int objType;
    int objProtoOff;
    int bufOff;

    if (protoInstSetupEdit(&winId, obj, &objType, &objProtoOff, &bufOff, "Wall Instance Editor") == -1) {
        return -1;
    }

    while (true) {
        sharedFpsLimiter.mark();

        int key = inputGetInput();

        if (key == KEY_ESCAPE || key == KEY_RETURN) {
            windowDestroy(winId);
            return 0;
        }

        if (key == kInstKeyFlags) {
            if (regModInstFlags(obj)) windowRefresh(winId);
        } else if (key == kInstKeyLight) {
            if (applyLightEdit(obj)) windowRefresh(winId);
        } else if (key == kInstKeyNewScript) {
            applyNewScript(obj, 3, winId, 194);
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }
}

// proto_inst_tile_edit_
static int protoInstTileEdit(Object* obj)
{
    int winId;
    int objType;
    int objProtoOff;
    int bufOff;

    if (protoInstSetupEdit(&winId, obj, &objType, &objProtoOff, &bufOff, "Tile Instance Editor") == -1) {
        return -1;
    }

    while (true) {
        sharedFpsLimiter.mark();

        int key = inputGetInput();

        if (key == KEY_ESCAPE || key == KEY_RETURN) {
            windowDestroy(winId);
            return 0;
        }

        if (key == kInstKeyFlags) {
            if (regModInstFlags(obj)) windowRefresh(winId);
        } else if (key == kInstKeyLight) {
            if (applyLightEdit(obj)) windowRefresh(winId);
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }
}

// proto_inst_misc_edit_
static void protoInstMiscEdit(Object* obj)
{
    int winId;
    int objType;
    int objProtoOff;
    int bufOff;

    if (protoInstSetupEdit(&winId, obj, &objType, &objProtoOff, &bufOff, "Misc Instance Editor") == -1) {
        return;
    }

    while (true) {
        sharedFpsLimiter.mark();

        int key = inputGetInput();

        if (key == KEY_ESCAPE || key == KEY_RETURN) {
            windowDestroy(winId);
            return;
        }

        if (key == kInstKeyFlags) {
            if (regModInstFlags(obj)) windowRefresh(winId);
        } else if (key == kInstKeyLight) {
            if (applyLightEdit(obj)) windowRefresh(winId);
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }
}

// proto_inst_scenery_edit_
static int protoInstSceneryEdit(Object* obj)
{
    int winId;
    int objType;
    int objProtoOff;
    int bufOff;

    Proto* proto;
    if (protoGetProto(obj->pid, &proto) == -1) {
        return -1;
    }

    if (protoInstSetupEdit(&winId, obj, &objType, &objProtoOff, &bufOff, "Scenery Instance Editor") == -1) {
        return -1;
    }

    // Scenery-specific buttons
    int sceneryType = proto->scenery.type;
    int y = 107;
    if (sceneryType != SCENERY_TYPE_GENERIC && sceneryType != SCENERY_TYPE_ELEVATOR) {
        _win_register_text_button(winId, 10, y, -1, -1, -1, 'd', "Dest Tile", 0);
        y += 21;
        _win_register_text_button(winId, 10, y, -1, -1, -1, 'e', "Dest Elev", 0);
        y += 21;
        _win_register_text_button(winId, 10, y, -1, -1, -1, 'm', "Dest Map", 0);
        y += 21;

        if (sceneryType == SCENERY_TYPE_LADDER_DOWN || sceneryType == SCENERY_TYPE_LADDER_UP) {
            _win_register_text_button(winId, 10, y, -1, -1, -1, 't', "Ladder Type", 0);
        }
    }
    windowRefresh(winId);

    while (true) {
        sharedFpsLimiter.mark();

        int key = inputGetInput();

        if (key == KEY_ESCAPE || key == KEY_RETURN) {
            windowDestroy(winId);
            return 0;
        }

        if (key == kInstKeyFlags) {
            if (regModInstFlags(obj)) windowRefresh(winId);
        } else if (key == kInstKeyLight) {
            if (applyLightEdit(obj)) windowRefresh(winId);
        } else if (key == 'S' && obj->sid != -1) {
            scriptRemove(obj->sid);
            obj->sid = -1;
            windowRefresh(winId);
        } else if (key == kInstKeyNewScript) {
            applyNewScript(obj, 4, winId, 194);
        } else if (key == 'd' && sceneryType != SCENERY_TYPE_GENERIC && sceneryType != SCENERY_TYPE_ELEVATOR) {
            int destTile = (obj->data.scenery.ladder.destinationBuiltTile != -1)
                ? builtTileGetTile(obj->data.scenery.ladder.destinationBuiltTile)
                : 0;
            if (win_get_num_i(&destTile, 0, 40000, false, "Destination Tile", 100, 100) != -1) {
                int elev = (obj->data.scenery.ladder.destinationBuiltTile != -1)
                    ? builtTileGetElevation(obj->data.scenery.ladder.destinationBuiltTile)
                    : 0;
                obj->data.scenery.ladder.destinationBuiltTile = builtTileCreate(destTile, elev);
                windowRefresh(winId);
            }
        } else if (key == 'e' && sceneryType != SCENERY_TYPE_GENERIC && sceneryType != SCENERY_TYPE_ELEVATOR) {
            int destElev = (obj->data.scenery.ladder.destinationBuiltTile != -1)
                ? builtTileGetElevation(obj->data.scenery.ladder.destinationBuiltTile)
                : 0;
            if (win_get_num_i(&destElev, 0, 3, false, "Destination Elevation", 100, 100) != -1) {
                int tile = (obj->data.scenery.ladder.destinationBuiltTile != -1)
                    ? builtTileGetTile(obj->data.scenery.ladder.destinationBuiltTile)
                    : 0;
                obj->data.scenery.ladder.destinationBuiltTile = builtTileCreate(tile, destElev);
                windowRefresh(winId);
            }
        } else if (key == 'm' && sceneryType != SCENERY_TYPE_GENERIC && sceneryType != SCENERY_TYPE_ELEVATOR) {
            int destMap = obj->data.scenery.ladder.destinationMap;
            if (win_get_num_i(&destMap, 0, wmMapMaxCount(), false, "Destination Map", 100, 100) != -1) {
                obj->data.scenery.ladder.destinationMap = destMap;
                windowRefresh(winId);
            }
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }
}

} // namespace fallout
