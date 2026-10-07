#ifndef FALLOUT_OPTIONS_SCHEMA_H_
#define FALLOUT_OPTIONS_SCHEMA_H_

#include <string>
#include <vector>

#include "config.h"
#include "settings.h"

namespace fallout {

// Parses a single schema section (e.g. "preferences.combat_speed") from a Config object into a SettingDescriptor.
// Leaves outDescriptor unchanged on failure.
bool optionsSchemaParseSection(Config* config, const char* sectionName, SettingDescriptor* outDescriptor);

// Parses all schema sections matching [section.key] in the given Config.
bool optionsSchemaParse(Config* config, std::vector<SettingDescriptor>* outDescriptors);

} // namespace fallout

#endif /* FALLOUT_OPTIONS_SCHEMA_H_ */
