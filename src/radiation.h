
#ifndef RADIATION_H
#define RADIATION_H

#include "db.h"
#include "obj_types.h"

namespace fallout {

// The number of effects caused by radiation.
//
// A radiation effect is an identifier and does not have its own name. Its
// stat is specified in [gRadiationEffectStats], and its amount is specified
// in [gRadiationEffectPenalties] for every [RadiationLevel].
constexpr int RADIATION_EFFECT_COUNT = 8;

// Denotes how many primary stats at the top of [gRadiationEffectStats] array.
// These stats are used to determine if critter is alive after applying
// radiation effects.
constexpr int RADIATION_EFFECT_PRIMARY_STAT_COUNT = 6;

// Radiation levels.
//
// The names of levels are taken from Fallout 3, comments from Fallout 2.
typedef enum RadiationLevel {
    // Very nauseous.
    RADIATION_LEVEL_NONE,

    // Slightly fatigued.
    RADIATION_LEVEL_MINOR,

    // Vomiting does not stop.
    RADIATION_LEVEL_ADVANCED,

    // Hair is falling out.
    RADIATION_LEVEL_CRITICAL,

    // Skin is falling off.
    RADIATION_LEVEL_DEADLY,

    // Intense agony.
    RADIATION_LEVEL_FATAL,

    // The number of radiation levels.
    RADIATION_LEVEL_COUNT,
} RadiationLevel;

// Radiation for Critter
int critterGetRadiation(Object* obj);
int critterCheckRadiationEvent(Object* obj);
int critterAdjustRadiation(Object* obj, int amount);

// Radiation for Events
void radiationProcess(Object* obj, int radiationLevel, bool isHealing);
int radiationEventProcess(Object* obj, void* data);
int radiationEventRead(File* stream, void** dataPtr);
int radiationEventWrite(File* stream, void* data);

} // namespace fallout

#endif /* RADIATION_H */
