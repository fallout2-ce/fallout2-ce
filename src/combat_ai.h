#ifndef COMBAT_AI_H
#define COMBAT_AI_H

#include "color.h"
#include "combat_ai_defs.h"
#include "combat_defs.h"
#include "db.h"
#include "obj_types.h"
#include "sfall_script_hooks.h"
#include "skill_defs.h"

namespace fallout {

enum AiMessageType : int {
    AI_MESSAGE_TYPE_RUN,
    AI_MESSAGE_TYPE_MOVE,
    AI_MESSAGE_TYPE_ATTACK,
    AI_MESSAGE_TYPE_MISS,
    AI_MESSAGE_TYPE_HIT,
};

extern const char* gAreaAttackModeKeys[AREA_ATTACK_MODE_COUNT];
extern const char* gAttackWhoKeys[ATTACK_WHO_COUNT];
extern const char* gBestWeaponKeys[BEST_WEAPON_COUNT];
extern const char* gChemUseKeys[CHEM_USE_COUNT];
extern const char* gDistanceModeKeys[DISTANCE_COUNT];
extern const char* gRunAwayModeKeys[RUN_AWAY_MODE_COUNT];
extern const char* gDispositionKeys[DISPOSITION_COUNT];
extern const char* gHurtTooMuchKeys[HURT_COUNT];

struct AiMessageRange {
    int start;
    int end;
};

#define AI_PACKET_CHEM_PRIMARY_DESIRE_COUNT (3)

struct AiPacket {
    char* name;
    int packet_num;
    int max_dist;
    int min_to_hit;
    int min_hp;
    int aggression;
    Dam hurt_too_much;
    int secondary_freq;
    int called_freq;
    int font;
    ColorWithFlags color;
    ColorWithFlags outline_color;
    int chance;
    AiMessageRange run;
    AiMessageRange move;
    AiMessageRange attack;
    AiMessageRange miss;
    AiMessageRange hit[HIT_LOCATION_SPECIFIC_COUNT];
    AreaAttackMode area_attack_mode;
    RunAwayMode run_away_mode;
    BestWeapon best_weapon;
    DistanceMode distance;
    AttackWho attack_who;
    ChemUse chem_use;
    int chem_primary_desire[AI_PACKET_CHEM_PRIMARY_DESIRE_COUNT];
    Disposition disposition;
    char* body_type;
    char* general_type;
};

int aiInit();
void aiReset();
int aiExit();
int aiLoad(File* stream);
int aiSave(File* stream);
int combat_ai_num();
char* combat_ai_name(int packet_num);
AreaAttackMode aiGetAreaAttackMode(Object* obj);
RunAwayMode aiGetRunAwayMode(Object* obj);
BestWeapon aiGetBestWeapon(Object* obj);
DistanceMode aiGetDistance(Object* obj);
AttackWho aiGetAttackWho(Object* obj);
ChemUse aiGetChemUse(Object* obj);
AiPacket* aiGetPacket(Object* obj);
int aiSetAreaAttackMode(Object* critter, AreaAttackMode areaAttackMode);
int aiSetRunAwayMode(Object* obj, RunAwayMode run_away_mode);
int aiSetBestWeapon(Object* critter, BestWeapon bestWeapon);
int aiSetDistance(Object* critter, DistanceMode distance);
int aiSetAttackWho(Object* critter, AttackWho attackWho);
int aiSetChemUse(Object* critter, ChemUse chemUse);
bool aiIsBurstDisabled(Object* critter);
void aiSetBurstDisabled(Object* critter, bool disable);
void aiRemoveBurstDisabled(Object* critter);
Disposition aiGetDisposition(Object* obj);
int aiSetDisposition(Object* obj, Disposition disposition);
int _caiSetupTeamCombat(Object* attackerTeam, Object* defenderTeam);
int _caiTeamCombatInit(Object** crittersList, int crittersListLength);
void _caiTeamCombatExit();
Object* _ai_search_inven_weap(Object* critter, bool checkRequiredActionPoints, Object* defender);
Object* _ai_search_inven_armor(Object* critter);
int _cAIPrepWeaponItem(Object* critter, Object* item);
void aiAttemptWeaponReload(Object* critter, int animate);
void _combat_ai_begin(int a1, void* a2);
void _combat_ai_over();
int _cai_perform_distance_prefs(Object* a1, Object* a2);
void _combat_ai(Object* a1, Object* a2);
bool _combatai_want_to_join(Object* a1);
bool _combatai_want_to_stop(Object* a1);
int critterSetTeam(Object* obj, int team);
int critterSetAiPacket(Object* object, int aiPacket);
int _combatai_msg(Object* critter, Attack* attack, AiMessageType type, int delay);
Object* _combat_ai_random_target(Attack* attack);
void _combatai_check_retaliation(Object* a1, Object* a2);
// Use this when the caller needs hook-specific context or must distinguish
// PERCEPTION_FORCE from the normal in-range/out-of-range result.
PerceptionResult isWithinPerceptionDetailed(Object* critter, Object* target, PerceptionType type = PERCEPTION_OTHER);
bool isWithinPerception(Object* watcher, Object* target);
void aiMessageListReloadIfNeeded();
void _combatai_notify_onlookers(Object* a1);
void _combatai_notify_friends(Object* a1);
void _combatai_delete_critter(Object* obj);

} // namespace fallout

#endif /* COMBAT_AI_H */
