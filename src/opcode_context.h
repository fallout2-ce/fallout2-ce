#ifndef FALLOUT_OPCODE_CONTEXT_H_
#define FALLOUT_OPCODE_CONTEXT_H_

#include <array>

#include "interpreter.h"
#include "sfall_arrays.h"

namespace fallout {

struct MetaruleInfo;

constexpr std::size_t METARULE_MAX_ARGS = 8;

// Generic scripting call context. This is metarule-only for now, but it is
// intentionally named to make later opcode adoption smaller.
class OpcodeContext {
public:
    OpcodeContext(Program* program, const MetaruleInfo* metaruleInfo, int numArgs, const ProgramValue* args);

    Program* program() const;
    const MetaruleInfo* metaruleInfo() const;
    const char* name() const;
    int numArgs() const;

    const ProgramValue& arg(int index) const;
    const char* stringArg(int index) const;

    void setReturn(const ProgramValue& value);
    void setReturn(std::nullptr_t);
    void setReturn(int value);
    void setReturn(ArrayId value);
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

} // namespace fallout

#endif /* FALLOUT_OPCODE_CONTEXT_H_ */
