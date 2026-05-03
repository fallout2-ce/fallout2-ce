#!/usr/bin/env python3
"""
map_function_names.py - Map original Fallout 2 function names to CE renamed equivalents.

Scans all .cc/.h files in src/ for offset comments (// 0xABCD1234),
looks each address up in Fallout2.lst to find the original function name,
and outputs a mapping: original_name -> ce_name

Usage: python tools/map_function_names.py [--src <src_dir>] [--lst <fallout2.lst>] [--out <output.txt>]
"""

import os
import re
import sys
import argparse

DEFAULT_SRC = os.path.join(os.path.dirname(__file__), "..", "src")
DEFAULT_LST = r"d:\GAMES\!Arhives\Fallout\Modding\EXE\Fallout2.lst"
DEFAULT_OUT = os.path.join(os.path.dirname(__file__), "..", "docs", "mapper", "function_name_map.txt")


def load_lst_index(lst_path):
    """Build address -> original_name index from Fallout2.lst.
    Lines look like: 00485DD0 gnw_main_      proc near
    Returns dict: addr_upper_hex (e.g. '485DD0') -> name (e.g. 'gnw_main_')
    """
    print(f"Indexing {lst_path} ...")
    proc_pat = re.compile(r'^([0-9A-Fa-f]{8})\s+(\w+)\s+proc\s+(near|far)', re.IGNORECASE)
    index = {}
    with open(lst_path, 'r', encoding='utf-8', errors='replace') as f:
        for line in f:
            m = proc_pat.match(line)
            if m:
                addr = m.group(1).upper().lstrip('0') or '0'
                name = m.group(2)
                index[addr] = name
    print(f"  Found {len(index)} proc definitions in LST.")
    return index


def scan_src(src_dir):
    """Scan all .cc and .h files for offset comments + following function name.
    Returns list of (file, lineno, hex_addr, ce_function_name)
    """
    offset_pat = re.compile(r'//\s*0x([0-9A-Fa-f]+)')
    # Match a C/C++ function definition line - crude but good enough:
    # looks for lines like: 'int funcName(' or 'void funcName(' or 'static int funcName('
    func_name_pat = re.compile(r'(?:static\s+)?(?:\w+\s+)+(\w+)\s*\(')

    results = []
    for root, dirs, files in os.walk(src_dir):
        # Skip build dirs
        dirs[:] = [d for d in dirs if d not in ('CMakeFiles', 'build', '.git')]
        for fname in files:
            if not (fname.endswith('.cc') or fname.endswith('.h')):
                continue
            fpath = os.path.join(root, fname)
            try:
                with open(fpath, 'r', encoding='utf-8', errors='replace') as f:
                    lines = f.readlines()
            except Exception:
                continue

            for i, line in enumerate(lines):
                m = offset_pat.search(line)
                if not m:
                    continue
                addr = m.group(1).upper().lstrip('0') or '0'
                # Search forward for the function definition (within next 5 lines)
                ce_name = None
                for j in range(i + 1, min(i + 6, len(lines))):
                    fm = func_name_pat.match(lines[j].strip())
                    if fm:
                        ce_name = fm.group(1)
                        break
                if ce_name:
                    rel_path = os.path.relpath(fpath, src_dir)
                    results.append((rel_path, i + 1, addr, ce_name))

    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--src', default=DEFAULT_SRC)
    parser.add_argument('--lst', default=DEFAULT_LST)
    parser.add_argument('--out', default=DEFAULT_OUT)
    args = parser.parse_args()

    lst_index = load_lst_index(args.lst)
    print(f"Scanning {args.src} ...")
    entries = scan_src(args.src)
    print(f"  Found {len(entries)} offset-annotated functions in CE source.")

    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)

    mapped = 0
    unmapped = 0
    lines_out = []

    for rel_path, lineno, addr, ce_name in sorted(entries, key=lambda x: x[2]):
        orig_name = lst_index.get(addr)
        if orig_name:
            lines_out.append(f"{orig_name} -> {ce_name}  [{rel_path}:{lineno}]")
            mapped += 1
        else:
            lines_out.append(f"??? (0x{addr}) -> {ce_name}  [{rel_path}:{lineno}]")
            unmapped += 1

    with open(args.out, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines_out) + '\n')

    print(f"  Mapped: {mapped}, Unmapped: {unmapped}")
    print(f"Output written to: {args.out}")


if __name__ == '__main__':
    main()
