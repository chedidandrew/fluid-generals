add_library(core_config INTERFACE)

include(${CMAKE_CURRENT_LIST_DIR}/config-build.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/config-macros.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/config-retail.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/config-debug.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/config-memory.cmake)

option(FLUID_ENABLE_DEVELOPMENT_PROBES "Enable local synthetic game-scene diagnostics" OFF)
if(FLUID_ENABLE_DEVELOPMENT_PROBES)
    target_compile_definitions(core_config INTERFACE FLUID_ENABLE_DEVELOPMENT_PROBES)
endif()
