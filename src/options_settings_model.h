#ifndef FALLOUT_OPTIONS_SETTINGS_MODEL_H_
#define FALLOUT_OPTIONS_SETTINGS_MODEL_H_

#include <string>
#include <vector>

#include "settings.h"

namespace fallout {

struct OptionsSettingEdit {
    const SettingDescriptor* descriptor;
    SettingValue originalValue;
    SettingValue value;
};

class OptionsSettingsModel {
public:
    OptionsSettingsModel();

    const std::vector<OptionsSettingEdit>& edits() const;
    bool setValue(const std::string& id, const SettingValue& value, std::string* error = nullptr);
    bool resetToDefault(const std::string& id, std::string* error = nullptr);
    bool isDirty() const;
    bool isDirty(const std::string& id) const;
    void rollback();
    bool commit(std::string* error = nullptr);

private:
    OptionsSettingEdit* findEdit(const std::string& id);
    const OptionsSettingEdit* findEdit(const std::string& id) const;

    std::vector<OptionsSettingEdit> edits_;
};

} // namespace fallout

#endif /* FALLOUT_OPTIONS_SETTINGS_MODEL_H_ */
