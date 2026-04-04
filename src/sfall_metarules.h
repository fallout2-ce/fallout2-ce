#ifndef FALLOUT_SFALL_METARULES_H_
#define FALLOUT_SFALL_METARULES_H_

#include "script_call_defs.h"

namespace fallout {

class OpcodeContext;

typedef void(MetaruleHandler)(OpcodeContext& ctx);

// The type of argument, not the same as actual data type. Useful for validation.
enum OpcodeArgumentType {
    ARG_ANY = 0, // no validation (default)
    ARG_INT, // integer only
    ARG_OBJECT, // non-null pointer/object
    ARG_STRING, // string only
    ARG_INTSTR, // integer OR string
    ARG_NUMBER, // float OR integer
};

constexpr std::size_t METARULE_MAX_ARGS = SCRIPT_CALL_MAX_ARGS;

struct MetaruleInfo {
    const char* name;
    MetaruleHandler* handler;
    int minArgs;
    int maxArgs;
    int errorReturn;
    OpcodeArgumentType argumentTypes[METARULE_MAX_ARGS];
};

extern const MetaruleInfo kMetarules[];
extern const std::size_t kMetarulesCount;

class Program;

void sfall_metarule(Program* program, int args);

void sprintf_lite(Program* program, int args, const char* infoOpcodeName);

} // namespace fallout

#endif /* FALLOUT_SFALL_METARULES_H_ */
