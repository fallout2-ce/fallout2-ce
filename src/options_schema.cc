#include "options_schema.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace fallout {

namespace {

    std::string trim(const std::string& str)
    {
        auto isNotSpace = [](unsigned char ch) { return !std::isspace(ch); };
        auto start = std::find_if(str.begin(), str.end(), isNotSpace);
        if (start == str.end()) return "";
        auto end = std::find_if(str.rbegin(), str.rend(), isNotSpace).base();
        return std::string(start, end);
    }

    std::string toLower(std::string str)
    {
        std::transform(str.begin(), str.end(), str.begin(), [](unsigned char ch) { return std::tolower(ch); });
        return str;
    }

    std::string humanize(const std::string& key)
    {
        std::string result;
        bool capitalizeNext = true;
        for (char ch : key) {
            if (ch == '_') {
                result += ' ';
                capitalizeNext = true;
            } else if (capitalizeNext) {
                result += static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
                capitalizeNext = false;
            } else {
                result += ch;
            }
        }
        return result;
    }

    bool parseSettingCategory(const std::string& raw, SettingCategory* category)
    {
        std::string lower = toLower(trim(raw));
        if (lower == "screen" || lower == "display")
            *category = SettingCategory::Screen;
        else if (lower == "interface" || lower == "ui")
            *category = SettingCategory::Interface;
        else if (lower == "audio" || lower == "sound")
            *category = SettingCategory::Audio;
        else if (lower == "gameplay")
            *category = SettingCategory::Gameplay;
        else if (lower == "preferences")
            *category = SettingCategory::Preferences;
        else if (lower == "qualityoflife" || lower == "quality_of_life" || lower == "qol")
            *category = SettingCategory::QualityOfLife;
        else if (lower == "system")
            *category = SettingCategory::System;
        else if (lower == "debug")
            *category = SettingCategory::Debug;
        else if (lower == "combatai" || lower == "combat_ai" || lower == "combat")
            *category = SettingCategory::CombatAi;
        else if (lower == "mapper")
            *category = SettingCategory::Mapper;
        else
            return false;
        return true;
    }

    bool parseSettingValueType(const std::string& raw, SettingValueType* valueType)
    {
        std::string lower = toLower(trim(raw));
        if (lower == "bool" || lower == "boolean")
            *valueType = SettingValueType::Boolean;
        else if (lower == "int" || lower == "integer")
            *valueType = SettingValueType::Integer;
        else if (lower == "real" || lower == "float" || lower == "double")
            *valueType = SettingValueType::Real;
        else if (lower == "text" || lower == "string")
            *valueType = SettingValueType::Text;
        else if (lower == "choice" || lower == "enum")
            *valueType = SettingValueType::Choice;
        else if (lower == "key" || lower == "key_binding" || lower == "keybinding")
            *valueType = SettingValueType::KeyBinding;
        else
            return false;
        return true;
    }

    bool parseSettingApplyPolicy(const std::string& raw, SettingApplyPolicy* applyPolicy)
    {
        std::string lower = toLower(trim(raw));
        if (lower == "on_close" || lower == "onclose")
            *applyPolicy = SettingApplyPolicy::OnClose;
        else if (lower == "next_game" || lower == "nextgame")
            *applyPolicy = SettingApplyPolicy::NextGame;
        else if (lower == "restart")
            *applyPolicy = SettingApplyPolicy::Restart;
        else
            return false;
        return true;
    }

    bool parseSettingValue(const std::string& raw, SettingValueType type, SettingValue* value)
    {
        std::string trimmed = trim(raw);
        switch (type) {
        case SettingValueType::Boolean: {
            std::string lower = toLower(trimmed);
            if (lower == "1" || lower == "true" || lower == "yes" || lower == "on")
                *value = true;
            else if (lower == "0" || lower == "false" || lower == "no" || lower == "off")
                *value = false;
            else
                return false;
            return true;
        }
        case SettingValueType::Integer:
        case SettingValueType::Choice:
        case SettingValueType::KeyBinding: {
            char* end;
            errno = 0;
            long number = std::strtol(trimmed.c_str(), &end, 0);
            if (errno != 0 || end == trimmed.c_str() || *end != '\0' || number < INT_MIN || number > INT_MAX) return false;
            *value = static_cast<int>(number);
            return true;
        }
        case SettingValueType::Real: {
            char* end;
            errno = 0;
            double number = std::strtod(trimmed.c_str(), &end);
            if (errno != 0 || end == trimmed.c_str() || *end != '\0' || !std::isfinite(number)) return false;
            *value = number;
            return true;
        }
        case SettingValueType::Text:
        default:
            *value = trimmed;
            return true;
        }
    }

    bool parseChoices(const std::string& raw, std::vector<SettingChoice>* choices)
    {
        std::stringstream ss(raw);
        std::string item;
        while (std::getline(ss, item, ',')) {
            item = trim(item);
            if (item.empty()) continue;
            size_t colon = item.find(':');
            if (colon == std::string::npos) return false;
            std::string valStr = trim(item.substr(0, colon));
            std::string labelStr = trim(item.substr(colon + 1));
            SettingValue value;
            if (labelStr.empty() || !parseSettingValue(valStr, SettingValueType::Integer, &value)) return false;
            choices->push_back({ std::get<int>(value), labelStr });
        }
        return !choices->empty();
    }

} // namespace

bool optionsSchemaParseSection(Config* config, const char* sectionName, SettingDescriptor* outDescriptor)
{
    if (config == nullptr || sectionName == nullptr || outDescriptor == nullptr) {
        return false;
    }

    *outDescriptor = SettingDescriptor();

    std::string secNameStr(sectionName);
    size_t dotPos = secNameStr.find('.');
    if (dotPos == std::string::npos || dotPos == 0 || dotPos == secNameStr.length() - 1) {
        return false;
    }

    outDescriptor->id = secNameStr;
    outDescriptor->section = secNameStr.substr(0, dotPos);
    outDescriptor->key = secNameStr.substr(dotPos + 1);

    char* fileStr = nullptr;
    if (configGetString(config, sectionName, "file", &fileStr) && fileStr != nullptr && *fileStr != '\0') {
        outDescriptor->source = trim(fileStr);
    } else {
        outDescriptor->source = "fallout2.cfg";
    }

    char* typeStr = nullptr;
    if (!configGetString(config, sectionName, "type", &typeStr)
        || typeStr == nullptr
        || !parseSettingValueType(typeStr, &outDescriptor->valueType)) return false;

    char* catStr = nullptr;
    if (!configGetString(config, sectionName, "category", &catStr)
        || catStr == nullptr
        || !parseSettingCategory(catStr, &outDescriptor->category)) return false;

    char* subsectionStr = nullptr;
    if (configGetString(config, sectionName, "subsection", &subsectionStr)
        && subsectionStr != nullptr
        && *subsectionStr != '\0') {
        outDescriptor->subsection = trim(subsectionStr);
    }

    char* choicesStr = nullptr;
    if (configGetString(config, sectionName, "choices", &choicesStr) && choicesStr != nullptr) {
        if (!parseChoices(choicesStr, &outDescriptor->choices)) return false;
    }
    if (outDescriptor->valueType == SettingValueType::Choice && outDescriptor->choices.empty()) return false;
    if (outDescriptor->valueType != SettingValueType::Choice && !outDescriptor->choices.empty()) return false;

    char* defStr = nullptr;
    if (!configGetString(config, sectionName, "default", &defStr)
        || defStr == nullptr
        || !parseSettingValue(defStr, outDescriptor->valueType, &outDescriptor->defaultValue)) return false;
    if (outDescriptor->valueType == SettingValueType::Choice) {
        int defaultValue = std::get<int>(outDescriptor->defaultValue);
        auto it = std::find_if(outDescriptor->choices.begin(), outDescriptor->choices.end(), [defaultValue](const SettingChoice& choice) {
            return choice.value == defaultValue;
        });
        if (it == outDescriptor->choices.end()) return false;
    }

    char* vanStr = nullptr;
    if (configGetString(config, sectionName, "vanilla", &vanStr) && vanStr != nullptr) {
        SettingValue vanillaValue;
        if (!parseSettingValue(vanStr, outDescriptor->valueType, &vanillaValue)) return false;
        outDescriptor->vanillaValue = std::move(vanillaValue);
    }

    int labelId = -1;
    if (configGetInt(config, sectionName, "label_id", &labelId)) {
        outDescriptor->labelMessageId = labelId;
    }

    char* labelStr = nullptr;
    if (configGetString(config, sectionName, "label", &labelStr) && labelStr != nullptr && *labelStr != '\0') {
        outDescriptor->fallbackLabel = trim(labelStr);
    } else {
        outDescriptor->fallbackLabel = humanize(outDescriptor->key);
    }

    int descId = -1;
    if (configGetInt(config, sectionName, "desc_id", &descId)) {
        outDescriptor->descriptionMessageId = descId;
    }

    char* descStr = nullptr;
    if (configGetString(config, sectionName, "description", &descStr) && descStr != nullptr && *descStr != '\0') {
        outDescriptor->fallbackDescription = trim(descStr);
    }

    char* assetStr = nullptr;
    if (configGetString(config, sectionName, "asset", &assetStr) && assetStr != nullptr && *assetStr != '\0') {
        outDescriptor->asset = trim(assetStr);
    }

    char* applyStr = nullptr;
    if (configGetString(config, sectionName, "apply", &applyStr) && applyStr != nullptr) {
        if (!parseSettingApplyPolicy(applyStr, &outDescriptor->applyPolicy)) return false;
    }

    return true;
}

bool optionsSchemaParse(Config* config, std::vector<SettingDescriptor>* outDescriptors)
{
    if (config == nullptr || outDescriptors == nullptr) {
        return false;
    }

    std::vector<SettingDescriptor> descriptors;
    descriptors.reserve(config->entriesLength);
    for (int i = 0; i < config->entriesLength; i++) {
        const char* sec = config->entries[i].key;
        SettingDescriptor desc;
        if (!optionsSchemaParseSection(config, sec, &desc)) return false;
        descriptors.push_back(std::move(desc));
    }
    *outDescriptors = std::move(descriptors);
    return true;
}

} // namespace fallout
