#!/usr/bin/env python3
"""Packs a game data/ tree into a single assets.pak read by PakArchive.

Usage:
    make_pak.py                         opens a folder picker for the data/ dir
    make_pak.py <data dir> [output.pak] no window; output defaults to
                                        <data dir>/../assets.pak

Every regular file under the chosen directory becomes one entry keyed by its
path relative to that directory, forward slashes, no leading slash
("assets/gui/items.png", "resources/newsound/step/grass1.adp"). The runtime
looks entries up case-insensitively, so two files that differ only by case
are rejected here rather than silently shadowing each other.

assets/legacy/tutorial goes in too: it is a world save, and the game opens it
read-only in place from the pak (PlatformStorage and RegionFile understand
"pak://" paths). Only its session lock and the pre-conversion level.dat
backups are left out, since a read-only open never needs them.

Layout (all integers big-endian u32; see src/platform/storage/PakArchive.h):

    header   32 bytes: 'MCPK', version 1, entryCount, tableOffset,
                       namesOffset, namesBytes, dataAlign, reserved
    table    entryCount x 16 bytes: hash, nameOffset, dataOffset, size,
             sorted by (hash, name) so the reader can bisect on the hash
    names    NUL-terminated keys, referenced by nameOffset
    data     each file at a dataAlign boundary (64: the PS2 DMA / Wii cache
             line), uncompressed -- PNG and ADPCM/OGG do not shrink

The hash is 32-bit FNV-1a over the lowercase key.
"""

import os
import struct
import sys

MAGIC = b"MCPK"
VERSION = 1
HEADER_BYTES = 32
ENTRY_BYTES = 16
DATA_ALIGN = 64


def fnv1a32(text):
    value = 0x811C9DC5
    for byte in text.encode("utf-8"):
        value ^= byte
        value = (value * 0x01000193) & 0xFFFFFFFF
    return value


EXCLUDED_PREFIXES = (
    "assets/legacy/tutorial/session.lock",
    "assets/legacy/tutorial/level.dat_old",
    "assets/legacy/tutorial/level.dat_mcr",
)


def collect_files(root):
    entries = []
    seen = {}
    for directory, _, names in os.walk(root):
        for name in names:
            full = os.path.join(directory, name)
            key = os.path.relpath(full, root).replace(os.sep, "/")
            if key.lower().startswith(EXCLUDED_PREFIXES):
                continue
            lowered = key.lower()
            if lowered in seen:
                raise SystemExit(
                    "case collision: %s and %s would be the same entry" % (seen[lowered], key))
            seen[lowered] = key
            entries.append((key, full, os.path.getsize(full)))
    return entries


def align(value, alignment):
    return (value + alignment - 1) // alignment * alignment


def build(root, output):
    files = collect_files(root)
    if not files:
        raise SystemExit("no files under %s" % root)

    names = bytearray()
    records = []
    for key, full, size in files:
        name_offset = len(names)
        names += key.encode("utf-8") + b"\0"
        records.append([fnv1a32(key.lower()), name_offset, 0, size, key, full])
    records.sort(key=lambda r: (r[0], r[4]))

    table_offset = HEADER_BYTES
    names_offset = table_offset + len(records) * ENTRY_BYTES
    data_offset = align(names_offset + len(names), DATA_ALIGN)

    cursor = data_offset
    for record in records:
        record[2] = cursor
        cursor = align(cursor + record[3], DATA_ALIGN)

    with open(output, "wb") as out:
        out.write(struct.pack(">4sIIIIIII", MAGIC, VERSION, len(records), table_offset,
                              names_offset, len(names), DATA_ALIGN, 0))
        for hash_value, name_offset, offset, size, _, _ in records:
            out.write(struct.pack(">IIII", hash_value, name_offset, offset, size))
        out.write(bytes(names))
        out.write(b"\0" * (data_offset - out.tell()))
        for _, _, offset, size, key, full in records:
            out.write(b"\0" * (offset - out.tell()))
            with open(full, "rb") as src:
                copied = 0
                while True:
                    chunk = src.read(1 << 20)
                    if not chunk:
                        break
                    out.write(chunk)
                    copied += len(chunk)
            if copied != size:
                raise SystemExit("%s changed size while packing" % key)
        out.write(b"\0" * (align(out.tell(), DATA_ALIGN) - out.tell()))
        total = out.tell()
    return len(records), total


def pick_directory():
    try:
        import tkinter
        from tkinter import filedialog
    except ImportError:
        raise SystemExit("tkinter is not available; pass the data directory on the command line")
    window = tkinter.Tk()
    window.withdraw()
    chosen = filedialog.askdirectory(title="Select the data/ folder to pack")
    window.destroy()
    return chosen


def main(argv):
    if len(argv) >= 2:
        root = argv[1]
    else:
        root = pick_directory()
        if not root:
            return 1
    root = os.path.abspath(root)
    if not os.path.isdir(root):
        raise SystemExit("not a directory: %s" % root)

    if len(argv) >= 3:
        output = argv[2]
    else:
        output = os.path.join(os.path.dirname(root), "assets.pak")

    count, total = build(root, output)
    print("%s: %d entries, %d bytes" % (output, count, total))
    if len(argv) < 2:
        try:
            import tkinter
            from tkinter import messagebox
            window = tkinter.Tk()
            window.withdraw()
            messagebox.showinfo("make_pak", "%s\n%d entries, %.1f MB" % (output, count, total / 1048576.0))
            window.destroy()
        except ImportError:
            pass
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
