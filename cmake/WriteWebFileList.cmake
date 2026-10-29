# Web build: writes files.txt into OUTPUT_DIR, the list of game files the browser client mounts
# (src/source/Core/Platform/web/WebFileSystem.cpp), one path per line, relative to OUTPUT_DIR.
# tools/web/make_startup_pack.py runs next: it reads the list and adds the packs and the gzip copy of
# Main.wasm it writes.
#
#   cmake -DOUTPUT_DIR=<web build output> -P cmake/WriteWebFileList.cmake

if(NOT OUTPUT_DIR)
  message(FATAL_ERROR "WriteWebFileList.cmake needs -DOUTPUT_DIR=<web build output>")
endif()

file(GLOB_RECURSE files LIST_DIRECTORIES FALSE RELATIVE "${OUTPUT_DIR}"
  "${OUTPUT_DIR}/Data/*"
  "${OUTPUT_DIR}/fonts/*"
  "${OUTPUT_DIR}/shaders/*")
# The page's manifest, icons and service worker ride along, so scripts/publish-web.py uploads them
# with the rest. The game skips them (WebFileSystem.cpp): only the page reads them.
foreach(single IN ITEMS config.ini MuMainIcon.png manifest.webmanifest icon-192.png icon-512.png sw.js)
  if(EXISTS "${OUTPUT_DIR}/${single}")
    list(APPEND files "${single}")
  endif()
endforeach()
list(SORT files)
list(LENGTH files count)
string(JOIN "\n" content ${files})
file(WRITE "${OUTPUT_DIR}/files.txt" "${content}\n")
message(STATUS "Web build: files.txt lists ${count} files")
