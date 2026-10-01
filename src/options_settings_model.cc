#include "options_settings_model.h"

#include <algorithm>

namespace fallout {

OptionsSettingsModel::OptionsSettingsModel()
{
    const auto& descriptors = settingsGetDescriptors();
    edits_.reserve(descriptors.size());
    for (const auto& descriptor : descriptors) {
        SettingValue value = settingsGetConfiguredValue(descriptor);
        edits_.push_back({ &descriptor, value, std::move(value) });
    }
}

const std::vector<OptionsSettingEdit>& OptionsSettingsModel::edits() const
{
    return edits_;
}

bool OptionsSettingsModel::setValue(const std::string& id, const SettingValue& value, std::string* error)
{
    OptionsSettingEdit* edit = findEdit(id);
    if (edit == nullptr) {
        if (error != nullptr) *error = "Setting is not registered.";
        return false;
    }
    if (edit->descriptor->readOnly) {
        if (error != nullptr) *error = "Setting is read-only.";
        return false;
    }
    if (edit->descriptor->commandLineOverride) {
        if (error != nullptr) *error = "Setting is overridden by the command line.";
        return false;
    }
    if (!settingsValidateValue(*edit->descriptor, value, error)) {
        return false;
    }

    edit->value = value;
    return true;
}

bool OptionsSettingsModel::resetToDefault(const std::string& id, std::string* error)
{
    OptionsSettingEdit* edit = findEdit(id);
    if (edit == nullptr) {
        if (error != nullptr) *error = "Setting is not registered.";
        return false;
    }
    return setValue(id, edit->descriptor->defaultValue, error);
}

bool OptionsSettingsModel::isDirty() const
{
    return std::any_of(edits_.begin(), edits_.end(), [](const OptionsSettingEdit& edit) {
        return edit.value != edit.originalValue;
    });
}

bool OptionsSettingsModel::isDirty(const std::string& id) const
{
    const OptionsSettingEdit* edit = findEdit(id);
    return edit != nullptr && edit->value != edit->originalValue;
}

void OptionsSettingsModel::rollback()
{
    for (auto& edit : edits_) {
        edit.value = edit.originalValue;
    }
}

bool OptionsSettingsModel::commit(std::string* error)
{
    for (const auto& edit : edits_) {
        if (!settingsValidateValue(*edit.descriptor, edit.value, error)) {
            return false;
        }
    }

    for (const auto& edit : edits_) {
        if (edit.value == edit.originalValue) continue;
        if (!settingsSetValue(*edit.descriptor, edit.value, error)) {
            for (const auto& rollbackEdit : edits_) {
                if (rollbackEdit.value == rollbackEdit.originalValue) continue;
                settingsSetValue(*rollbackEdit.descriptor, rollbackEdit.originalValue);
            }
            return false;
        }
    }

    if (!settingsSave()) {
        for (const auto& edit : edits_) {
            if (edit.value == edit.originalValue) continue;
            settingsSetValue(*edit.descriptor, edit.originalValue);
        }
        settingsWriteToConfig();
        if (error != nullptr) *error = "Unable to save the configuration file.";
        return false;
    }

    for (auto& edit : edits_) {
        edit.originalValue = settingsGetConfiguredValue(*edit.descriptor);
        edit.value = edit.originalValue;
    }
    return true;
}

OptionsSettingEdit* OptionsSettingsModel::findEdit(const std::string& id)
{
    auto it = std::find_if(edits_.begin(), edits_.end(), [&id](const OptionsSettingEdit& edit) {
        return edit.descriptor->id == id;
    });
    return it != edits_.end() ? &*it : nullptr;
}

const OptionsSettingEdit* OptionsSettingsModel::findEdit(const std::string& id) const
{
    auto it = std::find_if(edits_.begin(), edits_.end(), [&id](const OptionsSettingEdit& edit) {
        return edit.descriptor->id == id;
    });
    return it != edits_.end() ? &*it : nullptr;
}

} // namespace fallout
