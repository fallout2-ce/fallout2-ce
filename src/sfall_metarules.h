#ifndef FALLOUT_SFALL_METARULES_H_
#define FALLOUT_SFALL_METARULES_H_

#include "interpreter.h"

namespace fallout {

typedef void(MetaruleHandler)(Program* program, int args);


// The type of argument, not the same as actual data type. Useful for validation.
enum OpcodeArgumentType {
    ARG_ANY = 0, // no validation (default)
    ARG_INT,     // integer only
    ARG_OBJECT,  // integer that is not 0
    ARG_STRING,  // string only
    ARG_INTSTR,  // integer OR string
    ARG_NUMBER,  // float OR integer
};

constexpr size_t METARULE_MAX_ARGS = 8;

// Simplified cousin of `SfallMetarule` from Sfall.
typedef struct MetaruleInfo {
    const char* name;
    MetaruleHandler* handler;
    int minArgs;
    int maxArgs;
    int errorReturn;
    OpcodeArgumentType argumentTypes[METARULE_MAX_ARGS];
} MetaruleInfo;

extern const MetaruleInfo kMetarules[];
extern const std::size_t kMetarulesCount;

void sfall_metarule(Program* program, int args);

void sprintf_lite(Program* program, int args, const char* infoOpcodeName);

} // namespace fallout

#endif /* FALLOUT_SFALL_METARULES_H_ */
