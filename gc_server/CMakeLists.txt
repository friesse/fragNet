project(gc-server)

add_executable(gc-server WIN32
    main.cpp
    networking.cpp
    networking_users.cpp
    networking_inventory.cpp
    networking_matchmaking.cpp
    gc_message.cpp
    steam_network_message.cpp
    logger.cpp
    tcp_networking.cpp
    
    inventory.cpp
    item_schema.cpp
    keyvalue.cpp
    keyvalue_english.cpp
    random.cpp
    
    # Matchmaking system
    matchmaking_manager.cpp
    mysql_database.cpp
    gameserver_manager.cpp)

target_precompile_headers(gc-server PRIVATE stdafx.h)
set_target_properties(gc-server PROPERTIES PREFIX "")
set_target_properties(gc-server PROPERTIES OUTPUT_NAME gc-server${GC_EXE_SUFFIX})
if(UNIX AND NOT APPLE)
    set_target_properties(gc-server PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

# Only set subsystem and Windows-specific options for MSVC
if (MSVC)
    target_link_options(gc-server PRIVATE /SUBSYSTEM:CONSOLE)
    target_compile_options(gc-server PRIVATE /W4 /wd4244)
    target_compile_definitions(gc-server PRIVATE *CRT*SECURE_NO_WARNINGS)
endif()

file(GLOB PROTOBUFS ../protobufs/*.cc)

if(IS_32BIT)
    set(MARIADB_LIBRARY "/usr/lib/i386-linux-gnu/libmariadb.a")
endif()

target_include_directories(gc-server PRIVATE
    ${protobuf_SOURCE_DIR}/src
    ../protobufs
    ${MARIADB_INCLUDE_DIRS}
    ${MARIADB_INCLUDE_DIR} # for windows
)

target_sources(gc-server PRIVATE ${PROTOBUFS})
source_group("Protobufs" FILES ${PROTOBUFS})

target_link_libraries(gc-server PRIVATE
    protobuf::libprotobuf
    steam_api
    ${MARIADB_LIBRARIES}
    ${MARIADB_LIBRARY}
)

# Link cryptopp (from system, vcpkg, or FetchContent)
message(STATUS "Searching for cryptopp library...")

# First check if we have the target from the main CMakeLists
if(TARGET cryptopp::cryptopp)
    message(STATUS "Linking cryptopp::cryptopp")
    target_link_libraries(gc-server PRIVATE cryptopp::cryptopp)
elseif(TARGET cryptopp)
    message(STATUS "Linking cryptopp target")
    target_link_libraries(gc-server PRIVATE cryptopp)
elseif(TARGET cryptopp-static)
    message(STATUS "Linking cryptopp-static")
    target_link_libraries(gc-server PRIVATE cryptopp-static)
else()
    # Try to find cryptopp using find_package as a last resort
    message(STATUS "Trying to find cryptopp using find_package...")
    find_package(cryptopp QUIET)
    
    if(TARGET cryptopp::cryptopp)
        message(STATUS "Found cryptopp::cryptopp via find_package")
        target_link_libraries(gc-server PRIVATE cryptopp::cryptopp)
    else()
        # If we still don't have a target, try to find the library directly
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(CRYPTOPP QUIET libcrypto++)
            if(CRYPTOPP_FOUND)
                message(STATUS "Found cryptopp via pkg-config: ${CRYPTOPP_LIBRARIES}")
                target_include_directories(gc-server PRIVATE ${CRYPTOPP_INCLUDE_DIRS})
                target_link_libraries(gc-server PRIVATE ${CRYPTOPP_LIBRARIES})
                return()
            endif()
        endif()
        
        # If we get here, we couldn't find cryptopp
        message(STATUS "Available targets in current scope:")
        get_property(targets DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY BUILDSYSTEM_TARGETS)
        foreach(target ${targets})
            message(STATUS "  - ${target}")
        endforeach()
        
        message(FATAL_ERROR "cryptopp library not found! Please ensure cryptopp is installed or use FetchContent.")
    endif()
endif()

# Add include directories if they're not already included
if(CRYPTOPP_INCLUDE_DIRS)
    target_include_directories(gc-server PRIVATE ${CRYPTOPP_INCLUDE_DIRS})
endif()

# Platform-specific libraries
if (MSVC OR WIN32)
    target_link_libraries(gc-server PRIVATE
        ws2_32    # Winsock
        winmm     # Windows multimedia
    )
else()
    # Linux/Unix libraries
    target_link_libraries(gc-server PRIVATE
        pthread
        m
        dl
        z
        ssl
        crypto
    )
endif()
   
# apple message box
if (APPLE)
    target_link_libraries(gc-server PRIVATE "-framework Foundation")
endif()

if (MSVC)
    # we need large address awareness and a larger stack to not crash
    target_link_options(gc-server PRIVATE /LARGEADDRESSAWARE /STACK:1572864)
endif()

if (OUTDIR)
    add_custom_command(TARGET gc-server POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:gc-server>
        ${OUTDIR}/gc-server/${GC_LIB_DIR}/$<TARGET_FILE_NAME:gc-server>)
endif()