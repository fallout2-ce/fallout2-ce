#include "config.h"
#include "dfile.h"
#include "dictionary.h"
#include "interpreter.h"
#include "platform_compat.h"
#include "scan_unimplemented.h"
#include "scan_unimplemented_sfall.h"
#include "scan_unimplemented_utils.h"
#include "sfall_metarules.h"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <stdio.h>
#include <string.h>
#include <string>
#include <sys/stat.h>
#include <vector>

std::map<fallout::opcode_t, std::set<std::string>> unknown_opcodes;
std::map<std::string, std::set<std::string>> sus_strings;
std::map<int, std::set<std::string>> unknown_hooks;
std::map<fallout::opcode_t, std::map<std::string, std::set<std::string>>> offset_requests;

int checked_files = 0;

// right now registered hooks aren't all implemented, so this is a shortcut to list
// hooks that are implemented
bool is_implemented_hook(int hookId)
{
    switch (hookId) {
    case 0: // HOOK_TOHIT
    case 1: // HOOK_AFTERHITROLL
    case 2: // HOOK_CALCAPCOST
    case 4: // HOOK_DEATHANIM2
    case 5: // HOOK_COMBATDAMAGE
    case 6: // HOOK_ONDEATH
    case 8: // HOOK_USEOBJON
    case 9: // HOOK_REMOVEINVENOBJ
    case 10: // HOOK_BARTERPRICE
    case 11: // HOOK_MOVECOST
    case 16: // HOOK_ITEMDAMAGE
    case 17: // HOOK_AMMOCOST
    case 18: // HOOK_USEOBJ
    case 19: // HOOK_KEYPRESS
    case 20: // HOOK_MOUSECLICK
    case 21: // HOOK_USESKILL
    case 22: // HOOK_STEAL
    case 23: // HOOK_WITHINPERCEPTION
    case 24: // HOOK_INVENTORYMOVE
    case 25: // HOOK_INVENWIELD
    case 26: // HOOK_ADJUSTFID
    case 27: // HOOK_COMBATTURN
    case 30: // HOOK_RESTTIMER
    case 31: // HOOK_GAMEMODECHANGE
    case 33: // HOOK_EXPLOSIVETIMER
    case 34: // HOOK_DESCRIPTIONOBJ
    case 35: // HOOK_USESKILLON
    case 40: // HOOK_STDPROCEDURE
    case 41: // HOOK_STDPROCEDURE_END
    case 43: // HOOK_ENCOUNTER
    case 48: // HOOK_CANUSEWEAPON
        return true;
    default:
        return false;
    }
}

std::string get_hook_name(int hookId)
{
    static constexpr char const* hook_names[] = {
        "HOOK_TOHIT",
        "HOOK_AFTERHITROLL",
        "HOOK_CALCAPCOST",
        "HOOK_DEATHANIM1",
        "HOOK_DEATHANIM2",
        "HOOK_COMBATDAMAGE",
        "HOOK_ONDEATH",
        "HOOK_FINDTARGET",
        "HOOK_USEOBJON",
        "HOOK_REMOVEINVENOBJ",
        "HOOK_BARTERPRICE",
        "HOOK_MOVECOST",
        "HOOK_HEXMOVEBLOCKING",
        "HOOK_HEXAIBLOCKING",
        "HOOK_HEXSHOOTBLOCKING",
        "HOOK_HEXSIGHTBLOCKING",
        "HOOK_ITEMDAMAGE",
        "HOOK_AMMOCOST",
        "HOOK_USEOBJ",
        "HOOK_KEYPRESS",
        "HOOK_MOUSECLICK",
        "HOOK_USESKILL",
        "HOOK_STEAL",
        "HOOK_WITHINPERCEPTION",
        "HOOK_INVENTORYMOVE",
        "HOOK_INVENWIELD",
        "HOOK_ADJUSTFID",
        "HOOK_COMBATTURN",
        "HOOK_CARTRAVEL",
        "HOOK_SETGLOBALVAR",
        "HOOK_RESTTIMER",
        "HOOK_GAMEMODECHANGE",
        "HOOK_USEANIMOBJ",
        "HOOK_EXPLOSIVETIMER",
        "HOOK_DESCRIPTIONOBJ",
        "HOOK_USESKILLON",
        "HOOK_ONEXPLOSION",
        "HOOK_SUBCOMBATDAMAGE",
        "HOOK_SETLIGHTING",
        "HOOK_SNEAK",
        "HOOK_STDPROCEDURE",
        "HOOK_STDPROCEDURE_END",
        "HOOK_TARGETOBJECT",
        "HOOK_ENCOUNTER",
        "HOOK_ADJUSTPOISON",
        "HOOK_ADJUSTRADS",
        "HOOK_ROLLCHECK",
        "HOOK_BESTWEAPON",
        "HOOK_CANUSEWEAPON",
        // RESERVED 49 to 60
        nullptr, // 49
        nullptr, // 50
        nullptr, // 51
        nullptr, // 52
        nullptr, // 53
        nullptr, // 54
        nullptr, // 55
        nullptr, // 56
        nullptr, // 57
        nullptr, // 58
        nullptr, // 59
        nullptr, // 60
        "HOOK_BUILDSFXWEAPON",
        "HOOK_COUNT"
    };
    const size_t hook_names_count = sizeof(hook_names) / sizeof(hook_names[0]);
    if (hookId < 0 || hookId >= hook_names_count || hook_names[hookId] == nullptr) {
        return "Unknown hook";
    }
    return std::string(hook_names[hookId]);
}

int get_offset_request_argument_count(unsigned int opcodeIndex)
{
    switch (opcodeIndex) {
    case 0x156: // read_byte
    case 0x157: // read_short
    case 0x158: // read_int
    case 0x159: // read_string
    case 0x1d2: // call_offset_v0
        return 1;
    case 0x1cf: // write_byte
    case 0x1d0: // write_short
    case 0x1d1: // write_int
    case 0x1d3: // call_offset_v1
    case 0x21b: // write_string
        return 2;
    case 0x1d4: // call_offset_v2
        return 3;
    case 0x1d5: // call_offset_v3
        return 4;
    case 0x1d6: // call_offset_v4
        return 5;
    case 0x1d7: // call_offset_r0
        return 1;
    case 0x1d8: // call_offset_r1
        return 2;
    case 0x1d9: // call_offset_r2
        return 3;
    case 0x1da: // call_offset_r3
        return 4;
    case 0x1db: // call_offset_r4
        return 5;
    default:
        return 0;
    }
}

bool offset_request_returns_value(unsigned int opcodeIndex)
{
    switch (opcodeIndex) {
    case 0x156: // read_byte
    case 0x157: // read_short
    case 0x158: // read_int
    case 0x159: // read_string
    case 0x1d7: // call_offset_r0
    case 0x1d8: // call_offset_r1
    case 0x1d9: // call_offset_r2
    case 0x1da: // call_offset_r3
    case 0x1db: // call_offset_r4
        return true;
    default:
        return false;
    }
}

std::string offset_request_value_to_string(int value)
{
    return toHexString(static_cast<unsigned int>(value)) + " (" + std::to_string(value) + ")";
}

struct ScannedStackValue {
    bool isKnownInt = false;
    int value = 0;
};

ScannedStackValue unknownStackValue()
{
    return ScannedStackValue {};
}

ScannedStackValue knownIntStackValue(int value)
{
    ScannedStackValue stackValue;
    stackValue.isKnownInt = true;
    stackValue.value = value;
    return stackValue;
}

void pop_scanned_stack_values(std::vector<ScannedStackValue>& stack, int count)
{
    if (count <= 0) {
        return;
    }

    if (stack.size() < static_cast<size_t>(count)) {
        stack.clear();
        return;
    }

    stack.resize(stack.size() - static_cast<size_t>(count));
}

void push_unknown_stack_value(std::vector<ScannedStackValue>& stack)
{
    stack.push_back(unknownStackValue());
}

void simulate_unary_stack_operation(std::vector<ScannedStackValue>& stack, ScannedStackValue (*op)(ScannedStackValue))
{
    if (stack.empty()) {
        push_unknown_stack_value(stack);
        return;
    }

    auto value = stack.back();
    stack.pop_back();
    stack.push_back(op(value));
}

void simulate_binary_stack_operation(std::vector<ScannedStackValue>& stack, ScannedStackValue (*op)(ScannedStackValue, ScannedStackValue))
{
    if (stack.size() < 2) {
        stack.clear();
        push_unknown_stack_value(stack);
        return;
    }

    const auto right = stack.back();
    stack.pop_back();
    const auto left = stack.back();
    stack.pop_back();
    stack.push_back(op(left, right));
}

ScannedStackValue stack_add(ScannedStackValue left, ScannedStackValue right)
{
    if (left.isKnownInt && right.isKnownInt) {
        return knownIntStackValue(left.value + right.value);
    }
    return unknownStackValue();
}

ScannedStackValue stack_subtract(ScannedStackValue left, ScannedStackValue right)
{
    if (left.isKnownInt && right.isKnownInt) {
        return knownIntStackValue(left.value - right.value);
    }
    return unknownStackValue();
}

ScannedStackValue stack_multiply(ScannedStackValue left, ScannedStackValue right)
{
    if (left.isKnownInt && right.isKnownInt) {
        return knownIntStackValue(left.value * right.value);
    }
    return unknownStackValue();
}

ScannedStackValue stack_divide(ScannedStackValue left, ScannedStackValue right)
{
    if (left.isKnownInt && right.isKnownInt && right.value != 0) {
        return knownIntStackValue(left.value / right.value);
    }
    return unknownStackValue();
}

ScannedStackValue stack_modulo(ScannedStackValue left, ScannedStackValue right)
{
    if (left.isKnownInt && right.isKnownInt && right.value != 0) {
        return knownIntStackValue(left.value % right.value);
    }
    return unknownStackValue();
}

ScannedStackValue stack_bitwise_and(ScannedStackValue left, ScannedStackValue right)
{
    if (left.isKnownInt && right.isKnownInt) {
        return knownIntStackValue(left.value & right.value);
    }
    return unknownStackValue();
}

ScannedStackValue stack_bitwise_or(ScannedStackValue left, ScannedStackValue right)
{
    if (left.isKnownInt && right.isKnownInt) {
        return knownIntStackValue(left.value | right.value);
    }
    return unknownStackValue();
}

ScannedStackValue stack_bitwise_xor(ScannedStackValue left, ScannedStackValue right)
{
    if (left.isKnownInt && right.isKnownInt) {
        return knownIntStackValue(left.value ^ right.value);
    }
    return unknownStackValue();
}

ScannedStackValue stack_negate(ScannedStackValue value)
{
    if (value.isKnownInt) {
        return knownIntStackValue(-value.value);
    }
    return unknownStackValue();
}

ScannedStackValue stack_bitwise_not(ScannedStackValue value)
{
    if (value.isKnownInt) {
        return knownIntStackValue(~value.value);
    }
    return unknownStackValue();
}

void simulate_scanned_stack_opcode(
    std::vector<ScannedStackValue>& stack,
    fallout::opcode_t opcode,
    unsigned char* data,
    size_t opcodePos)
{
    const unsigned int opcodeIndex = opcode & 0x3FF;

    const int offsetArgCount = get_offset_request_argument_count(opcodeIndex);
    if (offsetArgCount > 0) {
        pop_scanned_stack_values(stack, offsetArgCount);
        if (offset_request_returns_value(opcodeIndex)) {
            push_unknown_stack_value(stack);
        }
        return;
    }

    switch (opcode) {
    case VALUE_TYPE_INT:
        stack.push_back(knownIntStackValue(fallout::stackReadInt32(data, opcodePos + 2)));
        break;
    case VALUE_TYPE_FLOAT:
    case VALUE_TYPE_STRING:
    case VALUE_TYPE_DYNAMIC_STRING:
    case VALUE_TYPE_PTR:
        push_unknown_stack_value(stack);
        break;
    case fallout::OPCODE_FETCH:
    case fallout::OPCODE_FETCH_GLOBAL:
    case fallout::OPCODE_FETCH_EXTERNAL:
        pop_scanned_stack_values(stack, 1);
        push_unknown_stack_value(stack);
        break;
    case fallout::OPCODE_STORE:
    case fallout::OPCODE_STORE_GLOBAL:
    case fallout::OPCODE_STORE_EXTERNAL:
        pop_scanned_stack_values(stack, 2);
        break;
    case fallout::OPCODE_POP:
        pop_scanned_stack_values(stack, 1);
        break;
    case fallout::OPCODE_DUP:
        if (!stack.empty()) {
            stack.push_back(stack.back());
        }
        break;
    case fallout::OPCODE_SWAP:
        if (stack.size() >= 2) {
            std::iter_swap(stack.end() - 1, stack.end() - 2);
        } else {
            stack.clear();
        }
        break;
    case fallout::OPCODE_ADD:
        simulate_binary_stack_operation(stack, stack_add);
        break;
    case fallout::OPCODE_SUB:
        simulate_binary_stack_operation(stack, stack_subtract);
        break;
    case fallout::OPCODE_MUL:
        simulate_binary_stack_operation(stack, stack_multiply);
        break;
    case fallout::OPCODE_DIV:
        simulate_binary_stack_operation(stack, stack_divide);
        break;
    case fallout::OPCODE_MOD:
        simulate_binary_stack_operation(stack, stack_modulo);
        break;
    case fallout::OPCODE_BITWISE_AND:
        simulate_binary_stack_operation(stack, stack_bitwise_and);
        break;
    case fallout::OPCODE_BITWISE_OR:
        simulate_binary_stack_operation(stack, stack_bitwise_or);
        break;
    case fallout::OPCODE_BITWISE_XOR:
        simulate_binary_stack_operation(stack, stack_bitwise_xor);
        break;
    case fallout::OPCODE_BITWISE_NOT:
        simulate_unary_stack_operation(stack, stack_bitwise_not);
        break;
    case fallout::OPCODE_NEGATE:
        simulate_unary_stack_operation(stack, stack_negate);
        break;
    case fallout::OPCODE_EQUAL:
    case fallout::OPCODE_NOT_EQUAL:
    case fallout::OPCODE_LESS_THAN_EQUAL:
    case fallout::OPCODE_GREATER_THAN_EQUAL:
    case fallout::OPCODE_LESS_THAN:
    case fallout::OPCODE_GREATER_THAN:
    case fallout::OPCODE_AND:
    case fallout::OPCODE_OR:
        pop_scanned_stack_values(stack, 2);
        push_unknown_stack_value(stack);
        break;
    case fallout::OPCODE_NOT:
    case fallout::OPCODE_FLOOR:
        pop_scanned_stack_values(stack, 1);
        push_unknown_stack_value(stack);
        break;
    case fallout::OPCODE_CALL:
        pop_scanned_stack_values(stack, 1);
        push_unknown_stack_value(stack);
        break;
    case fallout::OPCODE_CALL_AT:
    case fallout::OPCODE_CALL_WHEN:
    case fallout::OPCODE_SPAWN:
    case fallout::OPCODE_FORK:
    case fallout::OPCODE_EXEC:
        stack.clear();
        break;
    default:
        break;
    }
}

void track_offset_request(std::string fName, fallout::opcode_t opcode, const std::vector<ScannedStackValue>& stack)
{
    const unsigned int opcodeIndex = opcode & 0x3FF;
    const int argCount = get_offset_request_argument_count(opcodeIndex);
    if (argCount == 0) {
        return;
    }

    std::string offset = "<dynamic>";
    if (stack.size() >= static_cast<size_t>(argCount)) {
        const auto& firstArg = stack[stack.size() - static_cast<size_t>(argCount)];
        if (firstArg.isKnownInt) {
            offset = offset_request_value_to_string(firstArg.value);
        }
    }

    offset_requests[opcode][offset].insert(fName);
}

void check_int_data(
    std::string fName,
    unsigned char* data,
    size_t start_pos,
    size_t end_pos)
{
    size_t i = start_pos;
    std::vector<ScannedStackValue> stack;
    while (i < end_pos) {
        const size_t opcodePos = i;
        auto opcode = fallout::stackReadInt16(data, i);
        if (!((opcode >> 8) & 0x80)) {
            printf("ERROR: Wrong opcode %x in file %s at pos=0x%lx\n", opcode, fName.c_str(), i);
            return;
        };
        unsigned int opcodeIndex = opcode & 0x3FF;
        fallout::OpcodeHandler* handler = fallout::gInterpreterOpcodeHandlers[opcodeIndex];
        if (handler == NULL) {
            auto& set = unknown_opcodes[opcode];
            set.insert(fName);
        };

        track_offset_request(fName, opcode, stack);

        if (opcodeIndex == 0x207) { // register_hook
            if (i >= 6 && fallout::stackReadInt16(data, i - 6) == 0xC001) {
                auto hookId = fallout::stackReadInt32(data, i - 6 + 2);
                if (!is_implemented_hook(hookId)) {
                    unknown_hooks[hookId].insert(fName);
                }
            } else {
                printf("ERROR: Unknown usage of register_hook in file %s at pos=0x%lx\n", fName.c_str(), i);
                exit(1);
            }
        } else if (opcodeIndex == 0x262 || opcodeIndex == 0x27d) { // register_hook_proc / register_hook_proc_spec
            if (
                i >= 6 * 2 && fallout::stackReadInt16(data, i - 6) == 0xC001 && fallout::stackReadInt16(data, i - 6 * 2) == 0xC001) {
                auto hookId = fallout::stackReadInt32(data, i - 6 * 2 + 2);
                if (!is_implemented_hook(hookId)) {
                    unknown_hooks[hookId].insert(fName);
                }
            } else {
                printf("ERROR: Unknown usage of register_hook_proc in file %s at pos=0x%lx\n", fName.c_str(), i);
                exit(1);
            }
        }

        // printf("DEBUG: pos=0x%lx opcode=0x%x (%x) handler=%s\n", i, opcode, opcodeIndex, handler ? "yes": "no=======================");

        i += 2;

        if (opcodeIndex == (fallout::OPCODE_PUSH & 0x3FF)) {
            i += 4;
        }

        simulate_scanned_stack_opcode(stack, opcode, data, opcodePos);
    }
};

void check_file_data(unsigned char* data, int fileSize, std::string fName)
{

    auto script_strings = std::vector<std::string> {};

    check_int_data(fName, data, 0, 0x2A);

    auto identifiers_pos = 24 * fallout::stackReadInt32(data, 42) + 42 + 4;
    auto static_strings_pos = identifiers_pos + fallout::stackReadInt32(data, identifiers_pos) + 4 + 4;
    auto static_string_len = fallout::stackReadInt32(data, static_strings_pos);
    if (static_string_len == -1) {
        static_string_len = -4;
    }
    // printf("File %s identifiers_pos=%x static_strings_pos=%x static_string_len=%i\n", fName.c_str(), identifiers_pos, static_strings_pos, static_string_len);
    if (static_string_len > 0) {
        auto pos = static_strings_pos + 4;
        while (pos < static_strings_pos + 4 + static_string_len) {
            auto str_len = fallout::stackReadInt16(data, pos);
            pos += 2;
            auto str = &data[pos];
            auto str_len_actual = strlen((char*)str);
            if (str_len_actual <= str_len) {
                script_strings.push_back(std::string((char*)str, str_len_actual));
            }
            pos += str_len;
        }
    }
    auto code_pos = static_strings_pos + 4 + static_string_len + 4;

    // printf("File %s identifiers_pos=%x static_strings_pos=%x code_pos=%x\n", fName.c_str(), identifiers_pos,static_strings_pos, code_pos);

    check_int_data(fName, data, code_pos, fileSize);

    for (auto script_str : script_strings) {
        if (
            std::find(
                std::begin(sfall_metarules),
                std::end(sfall_metarules),
                script_str)
            != std::end(sfall_metarules)) {
            // That looks like a sFall metarule
            if (
                std::find_if(
                    fallout::kMetarules, fallout::kMetarules + fallout::kMetarulesCount,
                    [&script_str](auto rule) {
                        return std::string(rule.name) == script_str;
                    })
                == fallout::kMetarules + fallout::kMetarulesCount) {

                auto& set = sus_strings[script_str];
                set.insert(fName);

                // printf("WARNING: Found sFall metarule %s in file %s, but it is not defined in kMetarules\n",
                //   script_str.c_str(), fName.c_str());
                // sus_strings[s].insert(fName);
            }
        }
        // std::cout << "Script string: " << script_str << std::endl;
    }

    checked_files++;
}
void check_file_stream(fallout::File* stream, std::string fName)
{
    int fileSize = fallout::fileGetSize(stream);
    auto data = std::make_unique<unsigned char[]>(fileSize);
    fileRead(data.get(), 1, fileSize, stream);
    check_file_data(data.get(), fileSize, fName);
}

void check_file(std::string fName)
{
    fallout::File* stream = fallout::fileOpen(fName.c_str(), "rb");
    if (stream == NULL) {
        printf("Error opening %s\n", fName.c_str());
        exit(1);
    }

    check_file_stream(stream, fName);

    fileClose(stream);
};

void check_database(std::string dbFileName)
{
    std::cout << "Checking database file: " << dbFileName << std::endl;

    fallout::File* stream = fallout::fileOpen(dbFileName.c_str(), "rb");
    auto sizeOfFile = fallout::fileGetSize(stream);
    if (sizeOfFile <= 8) {
        std::cout << "  - File is too small, skipping" << std::endl;
        fallout::fileClose(stream);
        return;
    }
    fallout::fileSeek(stream, sizeOfFile - 4, SEEK_SET);
    unsigned int sizeInTheFile;
    fallout::fileReadUInt32(stream, &sizeInTheFile);
    sizeInTheFile = ((sizeInTheFile & 0xFF000000) >> 24) | ((sizeInTheFile & 0xFF0000) >> 8) | ((sizeInTheFile & 0xFF00) << 8) | ((sizeInTheFile & 0xFF) << 24);

    fallout::fileClose(stream);
    if (sizeOfFile != sizeInTheFile) {
        std::cout << "  - File does not look like database: file size: " << sizeOfFile << ", size in the file: " << sizeInTheFile << std::endl;
        return;
    }

    auto db = fallout::dbaseOpen(dbFileName.c_str());
    int intCount = 0;
    for (int i = 0; i < db->entriesLength; i++) {
        auto entryPath = std::string(db->entries[i].path);
        // std::cout << "  - Checking entry " << i << ": " << entryPath << std::endl;

        if (ends_with(entryPath, ".int")) {
            // std::cout << "    - This is script, checking..." << std::endl;
            fallout::DFile* dfile = fallout::dfileOpen(db, db->entries[i].path, "rb");
            if (dfile == NULL) {
                std::cout << "    - Error opening script file, skipping" << std::endl;
                continue;
            }

            auto fileSize = fallout::dfileGetSize(dfile);
            auto data = std::make_unique<unsigned char[]>(fileSize);
            fallout::dfileRead(data.get(), 1, fileSize, dfile);

            check_file_data(data.get(), fileSize, dbFileName + std::string(" -> ") + db->entries[i].path);
            intCount++;
            fallout::dfileClose(dfile);
        }
    }
    std::cout << "  - checked " << intCount << " scripts in database file" << std::endl;

    fallout::dbaseClose(db);
}

void scan_in_folder(std::string dirPath)
{
    // std::cout << "Scanning folder: " << dirPath << std::endl;
    for (auto dirEntry : directory_iterator(dirPath)) {
        if (dirEntry.is_directory()) {
            scan_in_folder(dirEntry.path());
            continue;
        } else if (dirEntry.is_regular_file()) {
            std::string filePath = dirEntry.path();
            if (
                ends_with(filePath, ".int") || ends_with(filePath, ".int.expected")) {
                // std::cout << "Scanning file: " << dirEntry.path() << std::endl;
                check_file(dirEntry.path());
            } else if (ends_with(filePath, ".dat")) {
                check_database(dirEntry.path());
            } else {
                // std::cout << "Skipping file with unsupported extension: " << dirEntry.path() << std::endl;
            }
        } else {
            std::cout << "Skipping non-regular file: " << dirEntry.path() << std::endl;
        }
    }
}

auto get_opcode_name(fallout::opcode_t opcode)
{
    auto sfallName = std::find_if(
        std::begin(opcodeInfoArray),
        std::end(opcodeInfoArray),
        [&opcode](const SfallOpcodeInfo& info) {
            return info.opcode == (opcode & 0x3FF);
        });

    if (sfallName != std::end(opcodeInfoArray)) {
        return sfallName->name;
    } else {
        return std::string("(check Opcodes.cpp)");
    }
}

void checkScriptsOpcodes()
{
    if (!gScanUnimplementedEnabled) {
        return;
    }

    unknown_opcodes.clear();
    sus_strings.clear();
    unknown_hooks.clear();
    offset_requests.clear();

    checked_files = 0;

    std::string folderName = ".";

    scan_in_folder(folderName);

    if (false) { // This is folder path stripping, not used now
        for (auto& [opcode, nameSet] : unknown_opcodes) {
            std::set<std::string> updatedNames;
            for (const auto& name : nameSet) {
                std::string trimmed = name;

                if (trimmed.rfind(folderName, 0) == 0) { // prefix match
                    trimmed.erase(0, folderName.length());
                }

                updatedNames.insert(std::move(trimmed));
            }
            nameSet = std::move(updatedNames);
        }
        for (auto& [opcode, offsetSet] : offset_requests) {
            for (auto& [offset, nameSet] : offsetSet) {
                std::set<std::string> updatedNames;
                for (const auto& name : nameSet) {
                    std::string trimmed = name;

                    if (trimmed.rfind(folderName, 0) == 0) { // prefix match
                        trimmed.erase(0, folderName.length());
                    }

                    updatedNames.insert(std::move(trimmed));
                }
                nameSet = std::move(updatedNames);
            }
        }
        for (auto& [opcode, nameSet] : sus_strings) {
            std::set<std::string> updatedNames;
            for (const auto& name : nameSet) {
                std::string trimmed = name;

                if (trimmed.rfind(folderName, 0) == 0) { // prefix match
                    trimmed.erase(0, folderName.length());
                }

                updatedNames.insert(std::move(trimmed));
            }
            nameSet = std::move(updatedNames);
        }
    }

    if (unknown_opcodes.size() == 0 && sus_strings.size() == 0 && unknown_hooks.size() == 0 && offset_requests.size() == 0) {
        printf("Everything is ok, all opcodes are known and no sus strings. Checked %i files\n", checked_files);
    } else {
        printf("\n\nChecked %i files and found those:\n", checked_files);
        // TODO: Sort
        for (auto iter : unknown_opcodes) {
            auto& opcode = iter.first;
            // https://github.com/sfall-team/sfall/blob/master/sfall/Modules/Scripting/Opcodes.cpp
            printf("OPCODE %s (0x%x - 0x%x - %i):\n",
                get_opcode_name(opcode).c_str(),
                opcode, opcode & 0x3FF, opcode & 0x3FF);
            for (auto fName : iter.second) {
                printf("  - %s\n", fName.c_str());
            }
        }
        for (auto iter : sus_strings) {
            printf("METARULE %s:\n", iter.first.c_str());
            for (auto fName : iter.second) {
                printf("  - %s\n", fName.c_str());
            }
        }
        for (const auto& [hookId, files] : unknown_hooks) {
            printf("HOOK %s (%i):\n",
                get_hook_name(hookId).c_str(),
                hookId);
            for (auto fName : files) {
                printf("  - %s\n", fName.c_str());
            }
        }
        for (const auto& [opcode, offsets] : offset_requests) {
            printf("OFFSET %s (0x%x - 0x%x - %i):\n",
                get_opcode_name(opcode).c_str(),
                opcode, opcode & 0x3FF, opcode & 0x3FF);
            for (const auto& [offset, files] : offsets) {
                printf("  %s:\n", offset.c_str());
                for (auto fName : files) {
                    printf("    - %s\n", fName.c_str());
                }
            }
        }

        printf("\nSame but per-file:\n");
        // TODO: Sort
        std::map<std::string, std::set<std::string>> files;
        for (auto iter : unknown_opcodes) {
            auto& opcode = iter.first;
            for (auto fName : iter.second) {
                std::string oss = "OPCODE " + get_opcode_name(opcode) + " "
                    + toHexString(opcode & 0x3FF)
                    + " (" + toHexString(opcode) + ")";

                files[fName].insert(oss);
            }
        }
        for (const auto& [opcode, offsets] : offset_requests) {
            for (const auto& [offset, offsetFiles] : offsets) {
                for (auto fName : offsetFiles) {
                    std::string oss = "OFFSET " + get_opcode_name(opcode) + " " + offset
                        + " " + toHexString(opcode & 0x3FF)
                        + " (" + toHexString(opcode) + ")";

                    files[fName].insert(oss);
                }
            }
        }
        for (auto iter : sus_strings) {
            for (auto fName : iter.second) {
                files[fName].insert(std::string("METARULE ") + iter.first);
            }
        }
        for (const auto& [hookId, hookFiles] : unknown_hooks) {
            for (auto fName : hookFiles) {
                std::string oss = "HOOK " + get_hook_name(hookId) + " "
                    + " (" + std::to_string(hookId) + ")";

                files[fName].insert(oss);
            }
        }
        for (auto iter : files) {
            printf("%s:\n", iter.first.c_str());
            for (auto s : iter.second) {
                printf("  - %s\n", s.c_str());
            }
        }
        // asdasd
    }
    printf("Done\n");

    printf("\n\n");
}
