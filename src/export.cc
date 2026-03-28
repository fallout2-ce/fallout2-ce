#include "export.h"

#include <ctype.h>
#include <string.h>

#include "interpreter_lib.h"
#include "memory_manager.h"
#include "platform_compat.h"

namespace fallout {

typedef struct ExternalVariable {
    char name[32];
    char* programName;
    ProgramValue value;
    char* stringValue;
} ExternalVariable;

typedef struct ExternalProcedure {
    char name[32];
    Program* program;
    int argumentCount;
    int address;
} ExternalProcedure;

static unsigned int _hashName(const char* identifier);
static ExternalProcedure* externalProcedureFind(const char* identifier);
static ExternalProcedure* externalProcedureAdd(const char* identifier);
static ExternalVariable* externalVariableFind(const char* identifier);
static ExternalVariable* externalVariableAdd(const char* identifier);
static void _removeProgramReferences(Program* program);

// 0x570C00
static ExternalProcedure gExternalProcedures[1013];

// 0x57BA1C
static ExternalVariable gExternalVariables[1013];

// NOTE: Inlined.
//
// 0x440F10 hashName_
static unsigned int _hashName(const char* identifier)
{
    unsigned int h = 0;
    const char* p = identifier;
    while (*p != '\0') {
        int ch = *p & 0xFF;
        h += (tolower(ch) & 0xFF) + (h * 8) + (h >> 29);
        p++;
    }

    h = h % 1013;
    return h;
}

// 0x440F58 findProc_
static ExternalProcedure* externalProcedureFind(const char* identifier)
{
    // NOTE: Uninline.
    unsigned int idx = _hashName(identifier);
    unsigned int start = idx;

    ExternalProcedure* externalProcedure = &(gExternalProcedures[idx]);
    if (externalProcedure->program != nullptr) {
        if (compat_stricmp(externalProcedure->name, identifier) == 0) {
            return externalProcedure;
        }
    }

    do {
        idx += 7;
        if (idx >= 1013) {
            idx -= 1013;
        }

        externalProcedure = &(gExternalProcedures[idx]);
        if (externalProcedure->program != nullptr) {
            if (compat_stricmp(externalProcedure->name, identifier) == 0) {
                return externalProcedure;
            }
        }
    } while (idx != start);

    return nullptr;
}

// 0x441018 findEmptyProc_
static ExternalProcedure* externalProcedureAdd(const char* identifier)
{
    // NOTE: Uninline.
    unsigned int idx = _hashName(identifier);
    unsigned int start = idx;

    ExternalProcedure* externalProcedure = &(gExternalProcedures[idx]);
    if (externalProcedure->name[0] == '\0') {
        return externalProcedure;
    }

    do {
        idx += 7;
        if (idx >= 1013) {
            idx -= 1013;
        }

        externalProcedure = &(gExternalProcedures[idx]);
        if (externalProcedure->name[0] == '\0') {
            return externalProcedure;
        }
    } while (idx != start);

    return nullptr;
}

// 0x4410AC findVar_
static ExternalVariable* externalVariableFind(const char* identifier)
{
    // NOTE: Uninline.
    unsigned int idx = _hashName(identifier);
    unsigned int start = idx;

    ExternalVariable* exportedVariable = &(gExternalVariables[idx]);
    if (compat_stricmp(exportedVariable->name, identifier) == 0) {
        return exportedVariable;
    }

    do {
        exportedVariable = &(gExternalVariables[idx]);
        if (exportedVariable->name[0] == '\0') {
            break;
        }

        idx += 7;
        if (idx >= 1013) {
            idx -= 1013;
        }

        exportedVariable = &(gExternalVariables[idx]);
        if (compat_stricmp(exportedVariable->name, identifier) == 0) {
            return exportedVariable;
        }
    } while (idx != start);

    return nullptr;
}

// 0x44118C findEmptyVar_
static ExternalVariable* externalVariableAdd(const char* identifier)
{
    // NOTE: Uninline.
    unsigned int idx = _hashName(identifier);
    unsigned int start = idx;

    ExternalVariable* exportedVariable = &(gExternalVariables[idx]);
    if (exportedVariable->name[0] == '\0') {
        return exportedVariable;
    }

    do {
        idx += 7;
        if (idx >= 1013) {
            idx -= 1013;
        }

        exportedVariable = &(gExternalVariables[idx]);
        if (exportedVariable->name[0] == '\0') {
            return exportedVariable;
        }
    } while (idx != start);

    return nullptr;
}

// 0x44127C exportStoreVariable_
int externalVariableSetValue(Program* program, const char* name, ProgramValue& programValue)
{
    ExternalVariable* exportedVariable = externalVariableFind(name);
    if (exportedVariable == nullptr) {
        return 1;
    }

    if ((exportedVariable->value.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        internal_free_safe(exportedVariable->stringValue, __FILE__, __LINE__); // "..\\int\\EXPORT.C", 169
    }

    if ((programValue.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        if (program != nullptr) {
            const char* stringValue = programGetString(program, programValue.opcode, programValue.integerValue);
            exportedVariable->value.opcode = VALUE_TYPE_DYNAMIC_STRING;

            exportedVariable->stringValue = (char*)internal_malloc_safe(strlen(stringValue) + 1, __FILE__, __LINE__); // "..\\int\\EXPORT.C", 175
            strcpy(exportedVariable->stringValue, stringValue);
        }
    } else {
        exportedVariable->value = programValue;
    }

    return 0;
}

// 0x4413D4 exportFetchVariable_
int externalVariableGetValue(Program* program, const char* name, ProgramValue& value)
{
    ExternalVariable* exportedVariable = externalVariableFind(name);
    if (exportedVariable == nullptr) {
        return 1;
    }

    if ((exportedVariable->value.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        value.opcode = exportedVariable->value.opcode;
        value.integerValue = programPushString(program, exportedVariable->stringValue);
    } else {
        value = exportedVariable->value;
    }

    return 0;
}

// 0x4414B8 exportExportVariable_
int externalVariableCreate(Program* program, const char* identifier)
{
    const char* programName = program->name;
    ExternalVariable* exportedVariable = externalVariableFind(identifier);

    if (exportedVariable != nullptr) {
        if (compat_stricmp(exportedVariable->programName, programName) != 0) {
            return 1;
        }

        if ((exportedVariable->value.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
            internal_free_safe(exportedVariable->stringValue, __FILE__, __LINE__); // "..\\int\\EXPORT.C", 234
        }
    } else {
        exportedVariable = externalVariableAdd(identifier);
        if (exportedVariable == nullptr) {
            return 1;
        }

        strncpy(exportedVariable->name, identifier, 31);

        exportedVariable->programName = (char*)internal_malloc_safe(strlen(programName) + 1, __FILE__, __LINE__); // // "..\\int\\EXPORT.C", 243
        strcpy(exportedVariable->programName, programName);
    }

    exportedVariable->value.opcode = VALUE_TYPE_INT;
    exportedVariable->value.integerValue = 0;

    return 0;
}

// 0x4414FC removeProgramReferences_
static void _removeProgramReferences(Program* program)
{
    for (int index = 0; index < 1013; index++) {
        ExternalProcedure* externalProcedure = &(gExternalProcedures[index]);
        if (externalProcedure->program == program) {
            externalProcedure->name[0] = '\0';
            externalProcedure->program = nullptr;
        }
    }
}

// 0x44152C initExport_
void _initExport()
{
    intLibRegisterProgramDeleteCallback(_removeProgramReferences);
}

// 0x441538 exportClose_
void externalVariablesClear()
{
    for (int index = 0; index < 1013; index++) {
        ExternalVariable* exportedVariable = &(gExternalVariables[index]);

        if (exportedVariable->name[0] != '\0') {
            internal_free_safe(exportedVariable->programName, __FILE__, __LINE__); // ..\\int\\EXPORT.C, 274
        }

        if (exportedVariable->value.opcode == VALUE_TYPE_DYNAMIC_STRING) {
            internal_free_safe(exportedVariable->stringValue, __FILE__, __LINE__); // ..\\int\\EXPORT.C, 276
        }
    }
}

// 0x44158C exportFindProcedure_
Program* externalProcedureGetProgram(const char* identifier, int* addressPtr, int* argumentCountPtr)
{
    ExternalProcedure* externalProcedure = externalProcedureFind(identifier);
    if (externalProcedure == nullptr) {
        return nullptr;
    }

    if (externalProcedure->program == nullptr) {
        return nullptr;
    }

    *addressPtr = externalProcedure->address;
    *argumentCountPtr = externalProcedure->argumentCount;

    return externalProcedure->program;
}

// 0x4415B0 exportExportProcedure_
int externalProcedureCreate(Program* program, const char* identifier, int address, int argumentCount)
{
    ExternalProcedure* externalProcedure = externalProcedureFind(identifier);
    if (externalProcedure != nullptr) {
        if (program != externalProcedure->program) {
            return 1;
        }
    } else {
        externalProcedure = externalProcedureAdd(identifier);
        if (externalProcedure == nullptr) {
            return 1;
        }

        strncpy(externalProcedure->name, identifier, 31);
    }

    externalProcedure->argumentCount = argumentCount;
    externalProcedure->address = address;
    externalProcedure->program = program;

    return 0;
}

// 0x441824 exportClearAllVariables_
void _exportClearAllVariables()
{
    for (int index = 0; index < 1013; index++) {
        ExternalVariable* exportedVariable = &(gExternalVariables[index]);
        if (exportedVariable->name[0] != '\0') {
            if ((exportedVariable->value.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
                if (exportedVariable->stringValue != nullptr) {
                    internal_free_safe(exportedVariable->stringValue, __FILE__, __LINE__); // "..\\int\\EXPORT.C", 387
                }
            }

            if (exportedVariable->programName != nullptr) {
                internal_free_safe(exportedVariable->programName, __FILE__, __LINE__); // "..\\int\\EXPORT.C", 393
                exportedVariable->programName = nullptr;
            }

            exportedVariable->name[0] = '\0';
            exportedVariable->value.opcode = 0;
        }
    }
}

} // namespace fallout
