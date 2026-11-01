// clang++ -std=c++17 -ffunction-sections -fdata-sections -Isrc
// -I/opt/homebrew/include/SDL2 -Ithird_party/lodepng tests/npc_level_test.cc
// -Wl,-dead_strip -o /tmp/npc_level_test
#include "../src/party_member.cc"

#include <cassert>
#include <cstdio>
#include <strings.h>

namespace fallout {
MessageList gMiscMessageList {};
Color _colorTable[COLOR_PALETTE_SIZE_15BIT] {};
static Proto liveProto {};
static Proto stages[2] {};
static int playerLevel;
static int messages;
static int rolls;
static bool failLoad;

int compat_stricmp(const char* a, const char* b) { return strcasecmp(a, b); }
char* critterGetName(Object* obj) { return const_cast<char*>(obj->pid == 0x01000001 ? "Vic" : "Other"); }
int debugPrint(const char*, ...) { return 0; }
int pcGetStat(PcStat) { return playerLevel; }
int randomBetween(int, int)
{
    rolls++;
    return 0;
}
int protoGetProto(const ProtoId& id, Proto** result)
{
    if (failLoad) return -1;
    if (id.pid() == 0x01000001)
        *result = &liveProto;
    else if (id.pid() == 0x01000002)
        *result = &stages[0];
    else if (id.pid() == 0x01000003)
        *result = &stages[1];
    else
        return -1;
    return 0;
}
Object* critterGetItem2(Object*) { return nullptr; }
Object* critterGetArmor(Object*) { return nullptr; }
int inventoryUnequipFunc(Object*, Hand, bool) { return 0; }
void adjustCritterStatsOnArmorChange(Object*, Object*, Object*) { }
int itemRemoveQuietly(Object*, Object*, int) { return 0; }
int critterGetStat(Object*, Stat stat) { return liveProto.critter.data.baseStats[stat]; }
int critterAdjustHitPoints(Object*, int) { return 0; }
int itemAdd(Object*, Object*, int) { return 0; }
int inventoryEquip(Object*, Object*, Hand) { return 0; }
int inventoryEquipFunc(Object*, Object*, Hand, bool) { return 0; }
char* getmsg(MessageList*, MessageListItem*, int) { return const_cast<char*>("%s leveled"); }
void displayMonitorAddMessage(const char*) { messages++; }
bool messageListGetItem(MessageList*, MessageListItem*) { return false; }
int textObjectAdd(Object*, char*, int, ColorWithFlags, ColorWithFlags, Rect*) { return 0; }
void tileWindowRefreshRect(Rect*, MapElevation) { }
} // namespace fallout

int main()
{
    using namespace fallout;
    Object player {};
    player.pid = 0x01000000;
    Object npc {};
    npc.pid = 0x01000001;
    Object other {};
    other.pid = 0x01000004;
    gPartyMemberProtoIds = { ProtoId(&player), ProtoId(&npc), ProtoId(&other) };
    gPartyMemberDescriptionsLength = 3;
    gPartyMemberDescriptions.resize(3);
    _partyMemberLevelUpInfoList.resize(3);
    gPartyMembers.resize(3);
    gPartyMembers[0].object = &player;
    gPartyMembers[1].object = &npc;
    gPartyMembers[2].object = &other;
    gPartyMembersLength = 3;
    auto& desc = gPartyMemberDescriptions[1];
    desc.level_up_every = 3;
    desc.level_minimum = 20;
    desc.level_pids_num = 2;
    desc.level_pids[0] = 0x01000002;
    desc.level_pids[1] = 0x01000003;
    stages[0].critter.data.baseStats[STAT_STRENGTH] = 7;
    stages[1].critter.data.baseStats[STAT_STRENGTH] = 9;
    stages[0].critter.data.baseStats[STAT_MAXIMUM_HIT_POINTS] = 40;
    stages[1].critter.data.baseStats[STAT_MAXIMUM_HIT_POINTS] = 60;
    stages[0].critter.data.skills[SKILL_SMALL_GUNS] = 80;
    stages[1].critter.data.skills[SKILL_SMALL_GUNS] = 100;
    auto& info = _partyMemberLevelUpInfoList[1];

    assert(partyMemberGetNpcLevel(0) == -1);
    assert(partyMemberGetNpcLevel(-1) == -1);
    assert(partyMemberGetNpcLevel(0, "") == -1);
    assert(partyMemberGetNpcLevel(0, "missing") == -1);
    assert(partyMemberGetNpcLevel(npc.pid) == 0);
    assert(partyMemberGetNpcLevel(0, "vIC") == 0);
    assert(partyMemberIncNpcLevel(0) == -1);
    assert(info.numLevelUps == 0);

    // Force ignores the player level and automatic-leveling switch, but uses
    // stage zero and updates the normal counters, HP, skills and messages.
    partyMemberSetEngineLevelUpEnabled(false);
    info.isEarly = 1;
    assert(partyMemberIncNpcLevel(0, "VIC") == 0);
    assert(info.level == 1 && info.numLevelUps == 1 && info.isEarly == 1);
    assert(liveProto.critter.data.baseStats[STAT_STRENGTH] == 7);
    assert(liveProto.critter.data.skills[SKILL_SMALL_GUNS] == 80);
    assert(npc.data.critter.hp == 40);
    assert(messages == 1 && rolls == 0);
    assert(_partyMemberLevelUpInfoList[2].numLevelUps == 0);

    // A forced early gain suppresses the following automatic gain until the
    // current three-level cycle has completed.
    playerLevel = 20;
    _partyMemberIncLevels();
    assert(info.level == 1 && info.numLevelUps == 2 && info.isEarly == 1);
    _partyMemberIncLevels();
    assert(info.level == 1 && info.numLevelUps == 3 && info.isEarly == 0);
    _partyMemberIncLevels();
    assert(info.level == 2 && info.numLevelUps == 4 && rolls == 1);
    assert(npc.data.critter.hp == 60);
    assert(liveProto.critter.data.skills[SKILL_SMALL_GUNS] == 100);
    partyMemberIncNpcLevel(npc.pid);
    assert(info.level == 2 && info.numLevelUps == 4 && messages == 2);

    // One-stage definitions must reach their sole stage, then stop.
    partyMembersReset();
    assert(partyMemberEngineLevelUpEnabled());
    desc.level_pids_num = 1;
    partyMemberIncNpcLevel(npc.pid);
    assert(info.level == 1 && npc.data.critter.hp == 40);
    partyMemberIncNpcLevel(npc.pid);
    assert(info.level == 1 && info.numLevelUps == 1);

    // The expanded sfall limit accepts every stage through level ten.
    partyMembersReset();
    desc.level_pids_num = PARTY_MEMBER_MAX_LEVEL;
    for (int index = 0; index < PARTY_MEMBER_MAX_LEVEL; index++) {
        desc.level_pids[index] = 0x01000002 + index % 2;
        partyMemberIncNpcLevel(npc.pid);
        assert(info.level == index + 1);
    }
    partyMemberIncNpcLevel(npc.pid);
    assert(info.level == PARTY_MEMBER_MAX_LEVEL && info.numLevelUps == PARTY_MEMBER_MAX_LEVEL);
    partyMembersReset();
    desc.level_pids_num = 1;
    partyMemberIncNpcLevel(npc.pid);

    // Dismissed NPCs remain configured, but cannot be queried or advanced.
    gPartyMembersLength = 1;
    assert(partyMemberGetNpcLevel(npc.pid) == -1);
    assert(partyMemberGetNpcLevel(0, "Vic") == -1);
    assert(partyMemberIncNpcLevel(npc.pid) == -1);
    assert(info.level == 1 && info.numLevelUps == 1);
    gPartyMembersLength = 3;

    partyMembersReset();
    desc.level_up_every = 0;
    partyMemberIncNpcLevel(npc.pid);
    assert(info.level == 0 && info.numLevelUps == 0);
    desc.level_up_every = 3;
    failLoad = true;
    assert(partyMemberIncNpcLevel(npc.pid) == -1);
    assert(info.level == 0);
    failLoad = false;
    info.level = -1;
    partyMemberIncNpcLevel(npc.pid);
    assert(info.level == -1);
    puts("NPC level tests passed.");
}
