#include "opcode_context.h"

#include <cstdarg>
#include <cstdio>

#include "sfall_metarules.h"

#include <assert.h>

namespace fallout {

OpcodeContext::OpcodeContext(Program* program, const MetaruleInfo* metaruleInfo, int numArgs)
    : _program(program)
    , _metaruleInfo(metaruleInfo)
    , _numArgs(numArgs)
    , _returnValue(0)
    , _hasReturnValue(false)
{
    assert(numArgs >= 0 && numArgs <= static_cast<int>(METARULE_MAX_ARGS));

    for (int index = _numArgs - 1; index >= 0; index--) {
        _args[index] = programStackPopValue(_program);
    }
}

OpcodeContext::OpcodeContext(Program* program, const MetaruleInfo* metaruleInfo, int numArgs, const ProgramValue* args)
    : _program(program)
    , _metaruleInfo(metaruleInfo)
    , _numArgs(numArgs)
    , _returnValue(0)
    , _hasReturnValue(false)
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

const char* OpcodeContext::stringArg(int index) const
{
    const ProgramValue& value = arg(index);
    if (!value.isString()) {
        programFatalError("OpcodeContext::stringArg: string expected, got %x", value.opcode);
    }

    return programGetString(_program, value.opcode, value.integerValue);
}

void* OpcodeContext::pointerArg(int index) const
{
    const ProgramValue& value = arg(index);
    if (value.opcode == VALUE_TYPE_INT && value.integerValue == 0) {
        return nullptr;
    }

    if (!value.isPointer()) {
        programFatalError("OpcodeContext::pointerArg: pointer expected, got %x", value.opcode);
    }

    return value.pointerValue;
}

void OpcodeContext::setReturn(const ProgramValue& value)
{
    _returnValue = value;
    _hasReturnValue = true;
}

void OpcodeContext::setReturn(std::nullptr_t)
{
    setReturn(static_cast<void*>(nullptr));
}

void OpcodeContext::setReturn(int value)
{
    setReturn(ProgramValue(value));
}

void OpcodeContext::setReturn(float value)
{
    ProgramValue programValue;
    programValue.opcode = VALUE_TYPE_FLOAT;
    programValue.floatValue = value;
    setReturn(programValue);
}

void OpcodeContext::setReturn(const char* value)
{
    ProgramValue programValue;
    programValue.opcode = VALUE_TYPE_DYNAMIC_STRING;
    programValue.integerValue = programPushString(_program, value);
    setReturn(programValue);
}

void OpcodeContext::setReturn(void* value)
{
    ProgramValue programValue;
    programValue.opcode = VALUE_TYPE_PTR;
    programValue.pointerValue = value;
    setReturn(programValue);
}

void OpcodeContext::setReturn(Object* value)
{
    setReturn(static_cast<void*>(value));
}

void OpcodeContext::setReturn(Attack* value)
{
    setReturn(static_cast<void*>(value));
}

void OpcodeContext::pushReturnValue() const
{
    programStackPushValue(_program, _hasReturnValue ? _returnValue : ProgramValue(0));
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
