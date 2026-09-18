# ps2_resource_manifest.cmake — build-time replacement for a live PS2 disc
# directory listing.
#
# ps2sdk gives PS2 two ways to enumerate a directory: fileXio's dopen/dread,
# and newlib's POSIX opendir/readdir glue. On this project's cdrom0: disc
# filesystem, both are unreliable for a directory this size (data/resources,
# ~700 files / ~20 subfolders): fileXioDread() never returns (blocks the EE
# thread forever), and readdir() returns cleanly but with zero entries --
# even though the directory is provably well-formed (mounting the built ISO
# with Windows' own ISO9660 driver shows every file). Direct open-by-name
# (fopen) is completely reliable on the same disc; only *enumeration* is
# broken. So: never enumerate cdrom0: at runtime. Generate the file list here,
# at data-staging time, when the real tree is just sitting on disk -- then
# the runtime reads one small text file (a plain fopen, proven reliable) and
# reconstructs the tree from it instead of asking the disc what's there.
#
# Invocation (see the ps2-data target in ps2.cmake):
#   cmake -D ROOT=<staged resources dir> -D OUTPUT=<manifest file path>
#         -P cmake/ps2_resource_manifest.cmake

if(NOT DEFINED ROOT)
    message(FATAL_ERROR "ps2_resource_manifest.cmake: ROOT not set")
endif()
if(NOT DEFINED OUTPUT)
    message(FATAL_ERROR "ps2_resource_manifest.cmake: OUTPUT not set")
endif()

file(GLOB_RECURSE _ps2_manifest_files RELATIVE "${ROOT}" "${ROOT}/*")
list(SORT _ps2_manifest_files)

set(_ps2_manifest_content "")
foreach(_ps2_manifest_entry IN LISTS _ps2_manifest_files)
    string(APPEND _ps2_manifest_content "${_ps2_manifest_entry}\n")
endforeach()

file(WRITE "${OUTPUT}" "${_ps2_manifest_content}")
list(LENGTH _ps2_manifest_files _ps2_manifest_count)
message(STATUS "PS2 build: resource manifest -> ${OUTPUT} (${_ps2_manifest_count} files)")
