#ifndef SKILL_H
#define SKILL_H

#include <array>
#include <cstddef>
#include <map>
#include <optional>

#include "art.h"
#include "db.h"
#include "enums.h"
#include "obj_types.h"
#include "proto_types.h"

namespace fallout {

extern int _gIsSteal;
extern int _gStealCount;
extern int _gStealSize;

enum class SkillStealResult {
    Caught = 0,
    Success = 1,
    Fail = 2,
};

int skillsInit();
void skillsReset();
void skillsExit();
int skillsLoad(File* stream);
int skillsSave(File* stream);
void protoCritterDataResetSkills(CritterProtoData* data);
void skillsSetTagged(Skill* skills, int count);
void skillsGetTagged(Skill* skills, int count);
bool skillIsTagged(Skill skill);
int skillGetValue(Object* critter, Skill skill);
void skillSetMaximum(int maximum);
int skillGetDefaultValue(Skill skill);
int skillAdd(Object* critter, Skill skill);
int skillAddForce(Object* critter, Skill skill);
int skillsGetCost(int skillValue);
int skillSub(Object* critter, Skill skill);
int skillSubForce(Object* critter, Skill skill);
Roll skillRoll(Object* critter, Skill skill, int modifier, int* howMuch);
char* skillGetName(Skill skill);
char* skillGetDescription(Skill skill);
char* skillGetAttributes(Skill skill);
SkillDexFrmId skillGetFrmId(Skill skill);
int skillUse(Object* obj, Object* target, Skill skill, int skillBonus);
SkillStealResult skillsPerformStealing(Object* thief, Object* target, Object* item, int quantity, bool isPlanting, int* xpOverride);

inline constexpr size_t kSkillMaxUsesPerDay = 3;
inline constexpr size_t kDailyLimitedSkillCount = 3;

constexpr std::optional<size_t> dailyLimitedSkillIndex(Skill skill)
{
    switch (skill) {
    case SKILL_FIRST_AID:
        return 0;
    case SKILL_DOCTOR:
        return 1;
    case SKILL_REPAIR:
        return 2;
    default:
        return std::nullopt;
    }
}

constexpr bool isDailyLimitedSkill(Skill skill)
{
    return dailyLimitedSkillIndex(skill).has_value();
}

// Rolling 24-hour usage timestamps for a single daily-limited skill.
class SkillUsageSlots {
public:
    using Storage = std::array<int, kSkillMaxUsesPerDay>;

    SkillUsageSlots()
    {
        clear();
    }

    std::optional<size_t> findFreeUsageSlot(unsigned int now) const;
    bool hasFreeUsageSlot(unsigned int now) const;
    bool recordUse(unsigned int now);

    bool hasActiveTimestamp(unsigned int now) const;
    bool hasAnyTimestamp() const;
    void clear()
    {
        _data.fill(0);
    }

    int* data()
    {
        return _data.data();
    }

    const int* data() const
    {
        return _data.data();
    }

    Storage::iterator begin()
    {
        return _data.begin();
    }

    Storage::iterator end()
    {
        return _data.end();
    }

    Storage::const_iterator begin() const
    {
        return _data.begin();
    }

    Storage::const_iterator end() const
    {
        return _data.end();
    }

    static bool isTimestampActive(int timestamp, unsigned int now);

private:
    Storage _data;
};

static_assert(sizeof(SkillUsageSlots) == sizeof(int) * kSkillMaxUsesPerDay);
static_assert(sizeof(std::array<SkillUsageSlots, SKILL_COUNT>) == sizeof(int) * SKILL_COUNT * kSkillMaxUsesPerDay);

// Tracks the player's daily skill usage slots (serialized in SAVE.DAT).
class PlayerSkillUsage {
public:
    const SkillUsageSlots& slots(Skill skill) const
    {
        return _slots[skill];
    }

    std::optional<size_t> findFreeSlot(Skill skill) const;
    bool hasFreeSlot(Skill skill) const;
    bool recordUse(Skill skill);
    void clear();

    int save(File* stream) const;
    int load(File* stream);

private:
    std::array<SkillUsageSlots, SKILL_COUNT> _slots;
};

using CritterSkillUsageTable = std::array<SkillUsageSlots, kDailyLimitedSkillCount>;

// Tracks daily First Aid, Doctor, and Repair usage per non-player critter (serialized in ce.sav).
class CritterSkillUsage {
public:
    using TableMap = std::map<int, CritterSkillUsageTable>;

    const TableMap& tables() const
    {
        return _tables;
    }

    bool hasFreeSlot(int pid, Skill skill) const;
    bool recordUse(int pid, Skill skill);
    void pruneExpired();
    void importTables(TableMap&& tables);
    void clear();

private:
    // Keyed by critter proto ID so companion quotas persist across maps and save/load.
    TableMap _tables;
};

extern PlayerSkillUsage playerSkillUsage;
extern CritterSkillUsage critterSkillUsage;

int skillGetGameDifficultyModifier(Skill skill);
int skillsUsageSave(File* stream);
int skillsUsageLoad(File* stream);
char* skillsGetGenericResponse(Object* critter, bool isDude);

} // namespace fallout

#endif /* SKILL_H */
