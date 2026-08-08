cmake_minimum_required(VERSION 3.16)

# shared/shared-protocol/ lives outside touch-ui/ (shared with main-esp
# and, eventually, ams-esp) -- same EXTRA_COMPONENT_DIRS setup as
# main-esp/CMakeLists.txt, see that file's comment for why.
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../shared")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(sparrowfdm_touch_ui)
