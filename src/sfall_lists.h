#ifndef FALLOUT_SFALL_LISTS_H_
#define FALLOUT_SFALL_LISTS_H_

#include <vector>

#include "obj_types.h"

namespace fallout {

enum ListType : int {
    LIST_CRITTERS = 0,
    LIST_ITEMS = 1,
    LIST_SCENERY = 2,
    LIST_WALLS = 3,
    LIST_TILES = 4,
    LIST_MISC = 5,
    LIST_SPATIAL = 6,
    LIST_ALL = 9,
};

constexpr inline bool listTypeIsValid(int listType)
{
    return listType >= LIST_CRITTERS && listType <= LIST_SPATIAL || listType == LIST_ALL;
}

bool sfallListsInit();
void sfallListsReset();
void sfallListsExit();
int sfallListsCreate(ListType listType);
Object* sfallListsGetNext(int listId);
void sfallListsDestroy(int listId);
void sfall_lists_fill(ListType type, std::vector<Object*>& objects);

} // namespace fallout

#endif /* FALLOUT_SFALL_LISTS_H_ */
