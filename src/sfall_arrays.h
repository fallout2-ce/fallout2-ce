#ifndef FALLOUT_SFALL_ARRAYS_H_
#define FALLOUT_SFALL_ARRAYS_H_

#include "interpreter.h"

struct XFile;
typedef XFile File;

namespace fallout {

#define SFALL_ARRAYFLAG_ASSOC (1) // is map
#define SFALL_ARRAYFLAG_CONSTVAL (2) // don't update value of key if the key exists in map
#define SFALL_ARRAYFLAG_RESERVED (4) // has no significance in sfall or CE
#define SFALL_ARRAYFLAG_EXPR_PUSH (32) // is created as part of array sub-expression
#define SFALL_ARRAYFLAG_EXPR_POP (64) // is used to indicate end of array sub-expression, not used in actual array

using ArrayId = unsigned int;

enum class SaveArrayResult {
    OK = 0,
    InvalidId,
    InvalidKeyType,
    ReservedKey,
};

bool sfallArraysInit();
void sfallArraysReset();
void sfallArraysExit();
ArrayId CreateArray(int len, unsigned int flags);
ArrayId CreateTempArray(int len, unsigned int flags);
ProgramValue GetArrayKey(ArrayId arrayId, int index, Program* program);
int LenArray(ArrayId arrayId);
ProgramValue GetArray(ArrayId arrayId, const ProgramValue& key, Program* program);
void SetArray(ArrayId arrayId, const ProgramValue& key, const ProgramValue& val, bool allowUnset, Program* program);
void FreeArray(ArrayId arrayId);
void FixArray(ArrayId id);
void ResizeArray(ArrayId arrayId, int newLen);
void DeleteAllTempArrays();
void SetArrayFromExpression(const ProgramValue& key, const ProgramValue& val, Program* program);
void PopExpressionArray();
ProgramValue ScanArray(ArrayId arrayId, const ProgramValue& val, Program* program);
ArrayId ListAsArray(int type);

ArrayId StringSplit(const char* str, const char* split);

SaveArrayResult SaveArray(const ProgramValue& key, ArrayId arrayId, Program* program);
ArrayId LoadArray(const ProgramValue& key, Program* program);
bool sfallArraysSave(File* stream);
bool sfallArraysLoad(File* stream);

} // namespace fallout

#endif /* FALLOUT_SFALL_ARRAYS_H_ */
