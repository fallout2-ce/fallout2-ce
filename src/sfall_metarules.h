#ifndef FALLOUT_SFALL_METARULES_H_
#define FALLOUT_SFALL_METARULES_H_

#include <array>

#include "interpreter.h"

namespace fallout {

class MetaruleContext;

typedef void(MetaruleHandler)(MetaruleContext& ctx);

// The type of argument, not the same as actual data type. Useful for validation.
enum OpcodeArgumentType {
    ARG_ANY = 0, // no validation (default)
    ARG_INT, // integer only
    ARG_OBJECT, // integer that is not 0
    ARG_STRING, // string only
    ARG_INTSTR, // integer OR string
    ARG_NUMBER, // float OR integer
};

constexpr size_t METARULE_MAX_ARGS = 8;

struct MetaruleInfo {
    const char* name;
    MetaruleHandler* handler;
    int minArgs;
    int maxArgs;
    int errorReturn;
    OpcodeArgumentType argumentTypes[METARULE_MAX_ARGS];
};

class MetaruleContext {
public:
    MetaruleContext(Program* program, const MetaruleInfo* metaruleInfo, int numArgs);
    MetaruleContext(Program* program, const MetaruleInfo* metaruleInfo, int numArgs, const ProgramValue* args);

    Program* program() const;
    const MetaruleInfo* metaruleInfo() const;
    const char* name() const;
    int numArgs() const;

    const ProgramValue& arg(int index) const;
    int intArg(int index) const;
    float floatArg(int index) const;
    const char* stringArg(int index) const;
    void* pointerArg(int index) const;

    void setReturn(const ProgramValue& value);
    void setReturn(int value);
    void setReturn(float value);
    void setReturn(const char* value);
    void setReturn(void* value);
    void setReturn(Object* value);
    void setReturn(Attack* value);

    void pushReturnValue() const;
    void printError(const char* format, ...) const;
    bool validateArguments() const;

private:
    Program* _program;
    const MetaruleInfo* _metaruleInfo;
    int _numArgs;
    std::array<ProgramValue, METARULE_MAX_ARGS> _args;
    ProgramValue _returnValue;
    bool _hasReturnValue;
};

extern const MetaruleInfo kMetarules[];
extern const std::size_t kMetarulesCount;

void sfall_metarule(Program* program, int args);

void sprintf_lite(Program* program, int args, const char* infoOpcodeName);

} // namespace fallout

#endif /* FALLOUT_SFALL_METARULES_H_ */
