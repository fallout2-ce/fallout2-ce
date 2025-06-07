#ifndef CHECK_SCRIPTS_OPCODES_SCAN_H
#define CHECK_SCRIPTS_OPCODES_SCAN_H

#include "config.h"
#include <map>
#include <set>
#include <string>

void checkScriptsOpcodes();

using ConfigMap = std::map<std::string, std::map<std::string, std::string>>;

class ConfigChecker {
public:
    ConfigChecker(fallout::Config& configDefaults, std::string configFileName);
    ConfigChecker(ConfigMap configDefaults, std::string configFileName);

    void check(fallout::Config& readedConfig);

private:
    ConfigMap defaultsMap;
    std::string configFileName;
};

#endif /* CHECK_SCRIPTS_OPCODES_SCAN_H */