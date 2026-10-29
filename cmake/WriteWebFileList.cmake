# Web build: writes files.txt into OUTPUT_DIR, the list of game files the browser client mounts
# (src/source/Core/Platform/web/WebFileSystem.cpp), one path per line, relative to OUTPUT_DIR.
#
#   cmake -DOUTPUT_DIR=<web build output> -P cmake/WriteWebFileList.cmake

if(NOT OUTPUT_DIR)
  message(FATAL_ERROR "WriteWebFileList.cmake needs -DOUTPUT_DIR=<web build output>")
endif()

file(GLOB_RECURSE files LIST_DIRECTORIES FALSE RELATIVE "${OUTPUT_DIR}"
  "${OUTPUT_DIR}/Data/*"
  "${OUTPUT_DIR}/fonts/*"
  "${OUTPUT_DIR}/shaders/*")
# The page's manifest and icons, and the gzip copies of startup.pack and Main.wasm
# (tools/web/make_startup_pack.py), ride along, so scripts/publish-web.py uploads them with the rest.
foreach(single IN ITEMS config.ini MuMainIcon.png manifest.webmanifest icon-192.png icon-512.png
    startup.pack.gz Main.wasm.gz)
  if(EXISTS "${OUTPUT_DIR}/${single}")
    list(APPEND files "${single}")
  endif()
endforeach()
list(SORT files)
list(LENGTH files count)
string(JOIN "\n" content ${files})
file(WRITE "${OUTPUT_DIR}/files.txt" "${content}\n")
message(STATUS "Web build: files.txt lists ${count} files")
