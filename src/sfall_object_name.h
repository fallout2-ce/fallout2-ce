#ifndef FALLOUT_SFALL_OBJECT_NAME_H_
#define FALLOUT_SFALL_OBJECT_NAME_H_

#include "obj_types.h"

namespace fallout {

void sfallObjectNameSet(int sid, const char* name);
char* sfallObjectNameGet(Object* object);
void sfallObjectNameReset();

} // namespace fallout

#endif /* FALLOUT_SFALL_OBJECT_NAME_H_ */
