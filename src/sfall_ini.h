#ifndef FALLOUT_SFALL_INI_H_
#define FALLOUT_SFALL_INI_H_

#include <cstddef>
#include "dictionary.h"

namespace fallout {

/// Sets base directory to lookup .ini files.
void sfall_ini_set_base_path(const char* path);

/// Reads integer key identified by "fileName|section|key" triplet into `value`.
bool sfall_ini_get_int(const char* triplet, int* value);

/// Reads string key identified by "fileName|section|key" triplet into `value`.
bool sfall_ini_get_string(const char* triplet, char* value, size_t size);

/// Writes integer key identified by "fileName|section|key" triplet.
bool sfall_ini_set_int(const char* triplet, int value);

/// Writes string key identified by "fileName|section|key" triplet.
bool sfall_ini_set_string(const char* triplet, const char* value);

// Loads an INI file specified by 'ini_file_name' (e.g., "myconfig.ini" or "ddraw.ini")
// into the provided 'config_out' object.
// The 'config_out' object must be initialized by the caller (using configInit).
// The caller is also responsible for freeing 'config_out' (using configFree).
// Returns true if the file was successfully found and read, false otherwise.
bool sfall_load_named_ini_file(const char* ini_file_name, Config* config_out);

// Finds a section within a loaded Config object.
// Returns a pointer to the ConfigSection if found, otherwise nullptr.
// The ConfigSection pointer is valid as long as the parent Config object is valid and unmodified.
const ConfigSection* sfall_find_section_in_config(const Config* config, const char* section_name);

} // namespace fallout

#endif /* FALLOUT_SFALL_INI_H_ */
