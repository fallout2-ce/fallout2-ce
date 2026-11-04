#!/usr/bin/env python3
"""Regression checks for script save/load extent metadata.

Run: python3 tests/script_serialization_test.py
Requires a C++17 compiler (CXX, or c++). No game assets or SDL required.
The actual production function bodies run against in-memory I/O stubs.
These checks cover filtering, extent lengths, recovery bounds, and live-list
immutability; they do not replace a full savegame integration test.
"""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "src/scripts.cc"


def function_body(text, signature):
    start = text.index(signature + "\n{")
    opening = text.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end] + "\n"


WRITER_PREAMBLE = r'''

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
constexpr int SCRIPT_LIST_EXTENT_SIZE = 16;
enum ScriptType { SCRIPT_TYPE_FIRST = 0,
    SCRIPT_TYPE_COUNT = 5 };
ScriptType operator++(ScriptType& e, int)
{
    auto old = e;
    e = static_cast<ScriptType>(e + 1);
    return old;
}
constexpr int SCRIPT_FLAG_NO_SAVE = 8, SCRIPT_FLAG_NONE = 0;
struct Object {
    bool savable = true;
};
bool objectIsSavable(Object* object)
{
    return object != nullptr && object->savable;
}
struct Script {
    int sid = 0;
    int flags = 0;
    Object* owner = nullptr;
};
struct ScriptListExtent {
    Script scripts[16];
    int length = 0;
    ScriptListExtent* next = nullptr;
};
struct ScriptList {
    ScriptListExtent* head = nullptr;
    ScriptListExtent* tail = nullptr;
    int length = 0;
};
ScriptList gScriptLists[5];
struct File {
    std::vector<int> words;
};
int fileWriteInt32(File* f, int x)
{
    f->words.push_back(x);
    return 0;
}
int scriptWrite(Script* s, File* f)
{
    f->words.push_back(s->sid);
    f->words.push_back(s->flags);
    return 0;
}

'''

WRITER_CHECKS = r'''
int main()
{
    int cases = 0;
    for (int n = 0; n <= 96; n++)
        for (int pattern = 0; pattern < 8; pattern++)
          for (int owners = 0; owners < 3; owners++) {
            Object savableOwner;
            Object temporaryOwner { false };
            std::vector<ScriptListExtent> ext((n + 15) / 16);
            std::vector<int> expected;
            for (int i = 0; i < n; i++) {
                bool skip = pattern == 1 ? i % 2 == 0 : pattern == 2 ? i >= n - 1
                    : pattern == 3                                   ? i >= n - 8
                    : pattern == 4                                   ? i >= n - 20
                    : pattern == 5                                   ? true
                    : pattern == 6                                   ? i < 20
                    : pattern == 7                                   ? i % 3 == 1
                                                                     : false;
                Object* owner = owners == 0 ? nullptr
                    : owners == 1 ? &savableOwner
                    : i % 3 == 0 ? &temporaryOwner : nullptr;
                ext[i / 16].scripts[i % 16] = { i, skip ? 8 : 0, owner };
                ext[i / 16].length++;
                if (!skip && !(owners == 2 && i % 3 == 0)) expected.push_back(i);
            }
            for (size_t i = 0; i + 1 < ext.size(); i++)
                ext[i].next = &ext[i + 1];
            gScriptLists[3].head = ext.empty() ? nullptr : &ext[0];
            gScriptLists[3].tail = ext.empty() ? nullptr : &ext.back();
            gScriptLists[3].length = static_cast<int>(ext.size());
            auto before = ext;
            File f;
            assert(scriptSaveAll(&f) == 0);
            size_t pos = 0;
            for (int type = 0; type < 5; type++) {
                int count = f.words[pos++];
                assert(count == (type == 3 ? int(expected.size()) : 0));
                int consumed = 0;
                for (int first = 0; first < count; first += 16) {
                    int len = std::min(16, count - first);
                    for (int j = 0; j < 16; j++) {
                        int sid = f.words[pos++], flags = f.words[pos++];
                        if (j < len) {
                            assert(sid == expected[consumed++]);
                            assert(!(flags & 8));
                        } else
                            assert(sid == expected.back() && !(flags & 8));
                    }
                    assert(f.words[pos++] == len);
                    assert(f.words[pos++] == 0);
                }
            }
            assert(pos == f.words.size());
            for (size_t i = 0; i < ext.size(); i++)
                assert(std::memcmp(&ext[i], &before[i], sizeof(ScriptListExtent)) == 0);
            cases++;
        }
    std::printf("PASS: %d save/parse cases; partial extents, full runtime-only tails, mixed flags, ownerless/savable/temporary owners, empty lists; live state unchanged.\n", cases);
}
'''

READER_PREAMBLE = r'''

#include <cassert>
#include <cstdio>
#include <vector>
constexpr int SCRIPT_LIST_EXTENT_SIZE = 16, SCRIPT_FLAG_NO_SAVE = 8, SCRIPT_FLAG_NONE = 0;
struct Script {
    int sid = 0, flags = 0;
};
struct ScriptListExtent {
    Script scripts[16];
    int length = 0;
};
struct File {
    std::vector<int> words;
    size_t pos = 0;
};
int fileReadInt32(File* f, int* out)
{
    if (f->pos >= f->words.size()) return -1;
    *out = f->words[f->pos++];
    return 0;
}
int scriptRead(Script* s, File* f)
{
    if (fileReadInt32(f, &s->sid)) return -1;
    return fileReadInt32(f, &s->flags);
}
void debugPrint(const char*, ...) { }
'''

READER_CHECKS = r'''
int main()
{
    int cases = 0;
    for (int expected = 1; expected <= 16; expected++)
        for (int actual = 0; actual <= 18; actual++)
            for (int bad = 0; bad < 2; bad++) {
                File f;
                for (int i = 0; i < 16; i++) {
                    f.words.push_back(i);
                    f.words.push_back(i >= expected && !(bad && i == expected) ? 8 : 0);
                }
                f.words.push_back(actual);
                f.words.push_back(0);
                ScriptListExtent e;
                bool valid = actual == expected || (actual > expected && actual <= 16 && !bad);
                int rc = scriptListExtentRead(&e, expected, &f);
                assert((rc == 0) == valid);
                if (valid) {
                    assert(e.length == expected);
                    assert(f.pos == f.words.size());
                }
                cases++;
            }
    std::printf("PASS: %d reader cases; recover NO_SAVE-only surplus; reject persistent surplus, invalid and short lengths.\n", cases);
}
'''

def main():
    source = SOURCE.read_text()
    checks = [
        ("writer", WRITER_PREAMBLE
         + function_body(source, "bool scriptIsSavable(const Script* script)"),
         "int scriptSaveAll(File* stream)", WRITER_CHECKS),
        ("reader", READER_PREAMBLE,
         "static int scriptListExtentRead(ScriptListExtent* scriptExtent, int expectedLength, File* stream)",
         READER_CHECKS),
    ]
    compiler = os.environ.get("CXX", "c++")
    with tempfile.TemporaryDirectory(prefix="fallout-script-tests-") as tmp:
        for name, preamble, signature, assertions in checks:
            cpp = Path(tmp) / (name + ".cc")
            binary = Path(tmp) / name
            cpp.write_text(preamble + function_body(source, signature) + assertions)
            subprocess.run([compiler, "-std=c++17", "-O1", "-fsanitize=address,undefined",
                            str(cpp), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
