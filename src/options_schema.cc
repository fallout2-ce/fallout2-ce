#include "options_schema.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <utility>

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

    std::string readOptionalText(Config* config, const char* sectionName, const char* key, const std::string& fallback = {})
    {
        char* raw = nullptr;
        if (!configGetString(config, sectionName, key, &raw) || raw == nullptr || *raw == '\0') {
            return fallback;
        }
        return trim(raw);
    }

    bool parseInteger(const std::string& raw, int* value)
    {
        std::string trimmed = trim(raw);
        char* end;
        errno = 0;
        long number = std::strtol(trimmed.c_str(), &end, 0);
        if (errno != 0 || end == trimmed.c_str() || *end != '\0' || number < INT_MIN || number > INT_MAX) return false;
        *value = static_cast<int>(number);
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
            int number;
            if (!parseInteger(raw, &number)) return false;
            *value = number;
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
            int value;
            if (labelStr.empty() || !parseInteger(valStr, &value)) return false;
            choices->push_back({ value, labelStr });
        }
        return !choices->empty();
    }

    bool parseDescriptorValue(const std::string& raw, const SettingDescriptor& descriptor, SettingValue* value)
    {
        if (!parseSettingValue(raw, descriptor.valueType, value)) return false;
        if (descriptor.valueType != SettingValueType::Choice) return true;

        int choiceValue = *std::get_if<int>(value);
        return std::any_of(descriptor.choices.begin(), descriptor.choices.end(), [choiceValue](const SettingChoice& choice) {
            return choice.value == choiceValue;
        });
    }

    bool parseMessageId(Config* config, const char* sectionName, const char* key, int* messageId)
    {
        char* raw = nullptr;
        if (!configGetString(config, sectionName, key, &raw)) return true;
        if (raw == nullptr) return false;

        return parseInteger(raw, messageId);
    }

} // namespace

bool optionsSchemaParseSection(Config* config, const char* sectionName, SettingDescriptor* outDescriptor)
{
    if (config == nullptr || sectionName == nullptr || outDescriptor == nullptr) {
        return false;
    }

    SettingDescriptor descriptor;

    std::string secNameStr(sectionName);
    size_t dotPos = secNameStr.find('.');
    if (dotPos == std::string::npos || dotPos == 0 || dotPos == secNameStr.length() - 1) {
        return false;
    }

    descriptor.id = secNameStr;
    descriptor.section = secNameStr.substr(0, dotPos);
    descriptor.key = secNameStr.substr(dotPos + 1);

    descriptor.source = readOptionalText(config, sectionName, "file", "fallout2.cfg");

    char* typeStr = nullptr;
    if (!configGetString(config, sectionName, "type", &typeStr)
        || typeStr == nullptr
        || !parseSettingValueType(typeStr, &descriptor.valueType)) return false;

    char* catStr = nullptr;
    if (!configGetString(config, sectionName, "category", &catStr)
        || catStr == nullptr
        || !parseSettingCategory(catStr, &descriptor.category)) return false;

    descriptor.subsection = readOptionalText(config, sectionName, "subsection");

    char* choicesStr = nullptr;
    if (configGetString(config, sectionName, "choices", &choicesStr) && choicesStr != nullptr) {
        if (!parseChoices(choicesStr, &descriptor.choices)) return false;
    }
    if (descriptor.valueType == SettingValueType::Choice && descriptor.choices.empty()) return false;
    if (descriptor.valueType != SettingValueType::Choice && !descriptor.choices.empty()) return false;

    char* defStr = nullptr;
    if (!configGetString(config, sectionName, "default", &defStr)
        || defStr == nullptr
        || !parseDescriptorValue(defStr, descriptor, &descriptor.defaultValue)) return false;

    char* vanStr = nullptr;
    if (configGetString(config, sectionName, "vanilla", &vanStr) && vanStr != nullptr) {
        SettingValue vanillaValue;
        if (!parseDescriptorValue(vanStr, descriptor, &vanillaValue)) return false;
        descriptor.vanillaValue = std::move(vanillaValue);
    }

    if (!parseMessageId(config, sectionName, "label_id", &descriptor.labelMessageId)) return false;

    descriptor.fallbackLabel = readOptionalText(config, sectionName, "label", humanize(descriptor.key));

    if (!parseMessageId(config, sectionName, "desc_id", &descriptor.descriptionMessageId)) return false;

    descriptor.fallbackDescription = readOptionalText(config, sectionName, "description");

    descriptor.asset = readOptionalText(config, sectionName, "asset");

    char* applyStr = nullptr;
    if (configGetString(config, sectionName, "apply", &applyStr) && applyStr != nullptr) {
        if (!parseSettingApplyPolicy(applyStr, &descriptor.applyPolicy)) return false;
    }

    *outDescriptor = std::move(descriptor);
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
