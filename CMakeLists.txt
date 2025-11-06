cmake_minimum_required(VERSION 3.20)

project(gc-server)

set(CMAKE_CXX_STANDARD 20)

if (CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(IS_32BIT ON)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    if (IS_32BIT)
        set(GC_LIB_DIR ".")
        set(GC_EXE_SUFFIX "_osx")
    else()
        set(GC_LIB_DIR "osx64")
        set(GC_EXE_SUFFIX "_osx64")
    endif()
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if (IS_32BIT)
        set(GC_LIB_DIR ".")
        set(GC_EXE_SUFFIX "_linux")
    else()
        set(GC_LIB_DIR "linux64")
        set(GC_LIB_SUFFIX "_client")
        set(GC_EXE_SUFFIX "_linux64")
    endif()
elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if (IS_32BIT)
        set(GC_LIB_DIR ".")
    else()
        set(GC_LIB_DIR "x64")
        set(GC_EXE_SUFFIX "_win64")
    endif()
else()
    message(FATAL_ERROR "Unsupported system $[CMAKE_SYSTEM_NAME}")
endif()

if (MSVC)
    # statically linked crt
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    add_compile_options(-fpic -fvisibility=hidden)

    if (NOT APPLE)
        add_link_options(-Wl,--no-undefined)
    endif()
endif()

include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)

#-------------------------------------------------
# cryptopp-cmake
#-------------------------------------------------

# Try to find system cryptopp first
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(CRYPTOPP QUIET libcrypto++)
endif()

# If system package found, create an imported target
if(CRYPTOPP_FOUND)
    message(STATUS "Found system cryptopp: ${CRYPTOPP_VERSION}")
    add_library(cryptopp INTERFACE IMPORTED)
    set_target_properties(cryptopp PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${CRYPTOPP_INCLUDE_DIRS}"
        INTERFACE_LINK_LIBRARIES "${CRYPTOPP_LIBRARIES}"
    )
else()
    # Fall back to FetchContent
    message(STATUS "System cryptopp not found, using FetchContent")
    option(CRYPTOPP_BUILD_TESTING "Build library tests" OFF)
    option(CRYPTOPP_INSTALL "Generate the install target for this project." OFF)
    
    FetchContent_Declare(cryptopp-cmake 
        GIT_REPOSITORY https://github.com/abdes/cryptopp-cmake 
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(cryptopp-cmake)
endif()

#-------------------------------------------------
# mariadb
#-------------------------------------------------

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if (IS_32BIT)
        set(MARIADB_LIB_DIR "${CMAKE_SOURCE_DIR}/mariadb/lib32")
        set(MARIADB_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/mariadb/include32")
    else()
        set(MARIADB_LIB_DIR "${CMAKE_SOURCE_DIR}/mariadb/lib64")
        set(MARIADB_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/mariadb/include64")
    endif()
    find_library(MARIADB_LIBRARY
        NAMES libmariadb mariadb
        PATHS ${MARIADB_LIB_DIR}
        NO_DEFAULT_PATH
    )
else()
    # linux/macOS
    if(IS_32BIT)
        find_library(MARIADB_LIBRARY 
            NAMES libmariadb.a mariadb
            PATHS /usr/lib/i386-linux-gnu
            NO_DEFAULT_PATH
            REQUIRED
        )
        set(MARIADB_INCLUDE_DIRS /usr/include/mariadb)
    else()
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(MARIADB REQUIRED mariadb)
    endif()
endif()

if(NOT MARIADB_LIBRARY AND NOT MARIADB_FOUND)
    message(FATAL_ERROR "MariaDB not found. Please install libmariadb-dev (Linux) or set up MariaDB libraries manually (Windows)")
endif()

#-------------------------------------------------
# protobuf
#-------------------------------------------------

option(protobuf_INSTALL "Install protobuf binaries and files" OFF)
option(protobuf_BUILD_TESTS "Build tests" OFF)
option(protobuf_BUILD_PROTOC_BINARIES "Build libprotoc and protoc compiler" OFF)
option(protobuf_BUILD_SHARED_LIBS "Build Shared Libraries" OFF)
option(protobuf_WITH_ZLIB "Build with zlib support" OFF)

FetchContent_Declare(protobuf GIT_REPOSITORY https://github.com/protocolbuffers/protobuf GIT_TAG 21.x GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(protobuf)

add_definitions(-DSTEAM_API_EXPORTS)

if (MSVC)
    # statically linked crt
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    
    # Add Steam SDK path for Windows
    if (IS_32BIT)
        set(STEAM_SDK_LIB_PATH "${CMAKE_SOURCE_DIR}/steamworks/sdk/redistributable_bin")
    else()
        set(STEAM_SDK_LIB_PATH "${CMAKE_SOURCE_DIR}/steamworks/sdk/redistributable_bin/win64")
    endif()
    
    # Export this variable so subprojects can use it
    set(STEAM_SDK_LIB_PATH ${STEAM_SDK_LIB_PATH} CACHE PATH "Path to Steam SDK libraries")
    
    # Add linker options for Windows
    add_link_options(/SUBSYSTEM:WINDOWS)
else()
    add_compile_options(-fpic -fvisibility=hidden)
    if (NOT APPLE)
        add_link_options(-Wl,--no-undefined)
    endif()
endif()
