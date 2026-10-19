# https://github.com/cpm-cmake/CPM.cmake

set(CPMSRC "https://github.com/cpm-cmake/CPM.cmake/releases/download/v0.42.0/CPM.cmake")
set(CPMDST "${CMAKE_BINARY_DIR}/CPM.cmake")

if(NOT EXISTS "${CPMDST}")
  file(DOWNLOAD "${CPMSRC}" "${CPMDST}"
    EXPECTED_HASH SHA256=2020b4fc42dba44817983e06342e682ecfc3d2f484a581f11cc5731fbe4dce8a)
endif()

include("${CPMDST}")
