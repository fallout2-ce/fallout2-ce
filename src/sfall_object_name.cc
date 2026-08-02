#include "sfall_object_name.h"

#include <string>
#include <unordered_map>

#include "proto.h"

namespace fallout {

static std::unordered_map<int, std::string> overrideScrName;

void sfallObjectNameSet(int sid, const char* name)
{
    if (sid == -1 || overrideScrName.find(sid) != overrideScrName.end()) {
        return;
    }

    overrideScrName.emplace(sid, name != nullptr ? name : "");
}

char* sfallObjectNameGet(Object* object)
{
    if (object == nullptr || object->sid == -1) {
        return nullptr;
    }

    const auto it = overrideScrName.find(object->sid);
    if (it == overrideScrName.end()) {
        return nullptr;
    }

    if (it->second.empty()) {
        return protoGetName(object->pid);
    }

    return const_cast<char*>(it->second.c_str());
}

void sfallObjectNameReset()
{
    overrideScrName.clear();
}

} // namespace fallout
