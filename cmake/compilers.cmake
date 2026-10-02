
# Print some information
message(STATUS "CMAKE_CXX_COMPILER: ${CMAKE_CXX_COMPILER}")
message(STATUS "CMAKE_CXX_COMPILER_ID: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "CMAKE_CXX_COMPILER_VERSION: ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
if (DEFINED MSVC_VERSION)
    message(STATUS "MSVC_VERSION: ${MSVC_VERSION}")
endif()

# TheSuperHackers @build JohnsterID 05/01/2026 Add MinGW-w64 detection and configure compiler flags
# Detect MinGW-w64
if(MINGW)
    message(STATUS "MinGW-w64 detected")
    set(IS_MINGW_BUILD TRUE)
else()
    set(IS_MINGW_BUILD FALSE)
endif()

# Set variable for VS6 to handle special cases.
if (DEFINED MSVC_VERSION AND MSVC_VERSION LESS 1300)
    set(IS_VS6_BUILD TRUE)
else()
    set(IS_VS6_BUILD FALSE)
endif()

# Make release builds have debug information too.
if(MSVC)
    # Create PDB for Release as long as debug info was generated during compile.
    string(APPEND CMAKE_EXE_LINKER_FLAGS_RELEASE " /DEBUG /OPT:REF /OPT:ICF")
    string(APPEND CMAKE_SHARED_LINKER_FLAGS_RELEASE " /DEBUG /OPT:REF /OPT:ICF")
    
    # /INCREMENTAL:NO prevents PDB size bloat in Debug configuration(s).
    add_link_options("/INCREMENTAL:NO")
else()
    # We go a bit wild here and assume any other compiler we are going to use supports -g for debug info.
    # Add debug symbols to Release builds for crash dump analysis, profiling, and post-mortem debugging.
    # For MinGW, symbols will be stripped to separate .debug files (matching MSVC PDB workflow).
    string(APPEND CMAKE_CXX_FLAGS_RELEASE " -g")
    string(APPEND CMAKE_C_FLAGS_RELEASE " -g")
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # Ensures only ISO features are used

if (NOT IS_VS6_BUILD)
    if (MSVC)
        # Multithreaded build.
        add_compile_options(/MP)
        # Enforce strict __cplusplus version
        add_compile_options(/Zc:__cplusplus)
        # Prevent FMA contraction to avoid cross-platform divergence
        add_compile_options(/fp:precise)
    else()
        add_compile_options(-Wsuggest-override)
        # GeneralsX @build fbraz 03/05/2026 Disable FMA contraction to avoid
        # cross-platform rounding divergence in deterministic math paths.
        # Upstream reference: Okladnoj, PR #2670
        # https://github.com/TheSuperHackers/GeneralsGameCode/pull/2670
        add_compile_options(-ffp-contract=off)
    endif()
else()
    if(RTS_BUILD_OPTION_VC6_FULL_DEBUG)
        set_property(GLOBAL PROPERTY JOB_POOLS compile=1 link=1)
    else()
        # Define two pools: 'compile' with plenty of slots, 'link' with just one
        set_property(GLOBAL PROPERTY JOB_POOLS compile=0 link=1)
    endif()

    # Tell CMake that all compile steps go into 'compile'
    set(CMAKE_JOB_POOL_COMPILE compile)
    # and all link steps go into 'link' (so only one link ever runs since vc6 can't handle multithreaded linking)
    set(CMAKE_JOB_POOL_LINK link)
endif()

if(RTS_BUILD_OPTION_ASAN)
    if(MSVC)
        set(ENV{ASAN_OPTIONS} "shadow_scale=2")
        add_compile_options(/fsanitize=address)
        add_link_options(/fsanitize=address)
    else()
        add_compile_options(-fsanitize=address)
        add_link_options(-fsanitize=address)
    endif()
endif()

# GeneralsX @feature cemlyn007 29/09/2026 RTS_SANITIZE for instrumented embedding builds (PLAN-023 Phase 5b item 5)
# RTS_SANITIZE=thread|address adds -fsanitize=<value> to the compile and link lines of every target
# configured after this point: the engine, its Core libraries and the FetchContent dependencies built
# with it (SDL3, SDL3_image, gamespy, GameMath, lzhl). vcpkg ports and DXVK (built by their own
# toolchains) are not instrumented. A host links the resulting library into an executable built with
# the same sanitiser (rlgenerals' --config=tsan / --config=asan). Empty (the default) changes nothing.
set(RTS_SANITIZE "" CACHE STRING "Sanitizer for every engine target: empty, thread or address (GCC/Clang only)")
set_property(CACHE RTS_SANITIZE PROPERTY STRINGS "" thread address)
if(RTS_SANITIZE)
    if(MSVC)
        message(FATAL_ERROR "RTS_SANITIZE is for GCC/Clang; use RTS_BUILD_OPTION_ASAN with MSVC")
    endif()
    if(NOT RTS_SANITIZE MATCHES "^(thread|address)$")
        message(FATAL_ERROR "RTS_SANITIZE must be empty, 'thread' or 'address', not '${RTS_SANITIZE}'")
    endif()
    if(RTS_SANITIZE STREQUAL "address" AND RTS_BUILD_OPTION_ASAN)
        message(STATUS "RTS_SANITIZE=address: RTS_BUILD_OPTION_ASAN adds the same flag")
    endif()
    if(RTS_SANITIZE STREQUAL "thread" AND RTS_BUILD_OPTION_ASAN)
        # GeneralsX @bugfix cemlyn007 02/10/2026 GCC/Clang refuse to link -fsanitize=thread together with
        # -fsanitize=address, but without this check configuration succeeds and the first compile fails with
        # an error that names neither cache option (found in review).
        message(FATAL_ERROR "RTS_SANITIZE=thread cannot be combined with RTS_BUILD_OPTION_ASAN=ON (ThreadSanitizer and AddressSanitizer cannot be linked together)")
    endif()
    add_compile_options(-fsanitize=${RTS_SANITIZE} -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=${RTS_SANITIZE})
    # The static libraries linked into a shared engine library must then be
    # position independent: ASan's instrumentation refers to runtime data (__asan_option_*) that a
    # non-PIC object cannot reach from a shared object (GameMath overrides it: see gamemath.cmake).
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    message(STATUS "RTS_SANITIZE: engine targets built with -fsanitize=${RTS_SANITIZE}")
endif()
