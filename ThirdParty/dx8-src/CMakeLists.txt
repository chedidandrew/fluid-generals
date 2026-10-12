add_library(d3d8lib INTERFACE)

# Common libraries for all compilers
target_link_libraries(d3d8lib INTERFACE d3d8 dinput8 dxguid)

# MSVC-specific configuration
if(MSVC)
    # Use bundled MSVC-compiled .lib files
    target_link_libraries(d3d8lib INTERFACE d3dx8)
    target_link_directories(d3d8lib BEFORE INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_options(d3d8lib INTERFACE /NODEFAULTLIB:libci.lib)
    
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "12.0.8804")
        # Modern MSVC (VS 2013+) has complete DirectX headers in Windows SDK
        target_link_libraries(d3d8lib INTERFACE legacy_stdio_definitions)
        target_link_options(d3d8lib INTERFACE /SAFESEH:NO)
    else()
        # VC6 and older MSVC need extra headers - their DirectX SDK is missing newer definitions
        target_include_directories(d3d8lib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/extra)
    endif()
endif()

# MinGW-specific configuration
if(MINGW)
    # MinGW-w64 DirectX 8 support varies by architecture:
    # i686 (32-bit): libd3d8.a + libd3dx8d.a (debug only, no release version)
    # x86_64 (64-bit): libd3d8thk.a only (no libd3dx8 libraries at all)
    target_link_libraries(d3d8lib INTERFACE d3dx8d)
endif()

target_compile_definitions(d3d8lib INTERFACE -DBUILD_WITH_D3D8)
target_include_directories(d3d8lib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})