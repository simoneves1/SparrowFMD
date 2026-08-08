cmake_minimum_required(VERSION 3.16)

# shared/shared-protocol/ lives outside main-esp/ (it's shared with
# touch-ui and ams-esp, whenever those get their own ESP-IDF projects),
# so it needs to be added to the component search path explicitly.
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../shared")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(sparrowfdm_main_esp)
