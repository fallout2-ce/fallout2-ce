#include "opcode_context.h"

#include <cstdarg>
#include <cstdio>

#include "sfall_metarules.h"

#include <assert.h>

namespace fallout {

OpcodeContext::OpcodeContext(Program* program, const MetaruleInfo* metaruleInfo, int numArgs, const ProgramValue* args)
    : _program(program)
    , _metaruleInfo(metaruleInfo)
    , _numArgs(numArgs)
    , _returnValue(0)
{
    assert(numArgs >= 0 && numArgs <= static_cast<int>(METARULE_MAX_ARGS));

    for (int index = 0; index < _numArgs; index++) {
        _args[index] = args[_numArgs - index - 1];
    }
}

Program* OpcodeContext::program() const
{
    return _program;
}

const MetaruleInfo* OpcodeContext::metaruleInfo() const
{
    return _metaruleInfo;
}

const char* OpcodeContext::name() const
{
    return _metaruleInfo->name;
}

int OpcodeContext::numArgs() const
{
    return _numArgs;
}

const ProgramValue& OpcodeContext::arg(int index) const
{
    assert(index >= 0 && index < _numArgs);
    return _args[index];
}

// TODO: when we have a better way of shlepping strings around, use arg(i).asString() instead
const char* OpcodeContext::stringArg(int index) const
{
    const ProgramValue& value = arg(index);
    assert(value.isString());

    return value.asString(_program);
}

void OpcodeContext::setReturn(const ProgramValue& value)
{
    _returnValue = value;
}

void OpcodeContext::setReturn(std::nullptr_t)
{
    setReturn(ProgramValue(0));
}

void OpcodeContext::setReturn(int value)
{
    setReturn(ProgramValue(value));
}

void OpcodeContext::setReturn(const char* value)
{
    ProgramValue programValue;
    programValue.opcode = VALUE_TYPE_DYNAMIC_STRING;
    programValue.integerValue = programPushString(_program, value);
    setReturn(programValue);
}

void OpcodeContext::pushReturnValue() const
{
    // sfall_funcX calls are expression-style; handlers that do not call
    // setReturn() implicitly return 0.
    programStackPushValue(_program, _returnValue);
}

void OpcodeContext::printError(const char* format, ...) const
{
    va_list args;
    va_start(args, format);
    char message[1024];
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    programPrintError("%s", message);
}

bool OpcodeContext::validateArguments() const
{
    if (_numArgs < _metaruleInfo->minArgs || _numArgs > _metaruleInfo->maxArgs) {
        printError("%s() - invalid number of arguments (%d), must be from %d to %d.",
            name(),
            _numArgs,
            _metaruleInfo->minArgs,
            _metaruleInfo->maxArgs);
        return false;
    }

    for (int index = 0; index < _numArgs; index++) {
        const ProgramValue& value = arg(index);
        switch (_metaruleInfo->argumentTypes[index]) {
        case ARG_ANY:
            continue;
        case ARG_INT:
            if (!value.isInt()) {
                printError("%s() - argument #%d is not an integer.", name(), index + 1);
                return false;
            }
            break;
        case ARG_OBJECT:
            if (!value.isPointer()) {
                printError("%s() - argument #%d is not an object.", name(), index + 1);
                return false;
            }
            if (value.pointerValue == nullptr) {
                printError("%s() - argument #%d is null.", name(), index + 1);
                return false;
            }
            break;
        case ARG_STRING:
            if (!value.isString()) {
                printError("%s() - argument #%d is not a string.", name(), index + 1);
                return false;
            }
            break;
        case ARG_INTSTR:
            if (!value.isInt() && !value.isString()) {
                printError("%s() - argument #%d is not an integer or a string.", name(), index + 1);
                return false;
            }
            break;
        case ARG_NUMBER:
            if (!value.isInt() && !value.isFloat()) {
                printError("%s() - argument #%d is not a number.", name(), index + 1);
                return false;
            }
            break;
        }
    }

    return true;
}

} // namespace fallout
