cmake_minimum_required(VERSION 2.8.11)
project(simExtLuaCmd)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_MACOSX_RPATH 1)

find_package(Boost REQUIRED)
find_package(Qt5 COMPONENTS Core Gui Widgets PrintSupport REQUIRED)

if(NOT LIBPLUGIN_DIR)
    if(DEFINED ENV{COPPELIASIM_ROOT_DIR})
        set(LIBPLUGIN_DIR $ENV{COPPELIASIM_ROOT_DIR}/programming/libPlugin)
    else()
        set(LIBPLUGIN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../libPlugin)
    endif()
endif()
list(APPEND CMAKE_MODULE_PATH
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules
    ${LIBPLUGIN_DIR}/cmake)
find_package(CoppeliaSim 4.5.0.0 REQUIRED)

include_directories(${CMAKE_CURRENT_BINARY_DIR}/generated)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external)
add_definitions(-DQT_COMPIL)

coppeliasim_generate_stubs(${CMAKE_CURRENT_BINARY_DIR}/generated XML_FILE ${CMAKE_CURRENT_SOURCE_DIR}/callbacks.xml LUA_FILE ${CMAKE_CURRENT_SOURCE_DIR}/simLuaCmd.lua)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

set(SOURCES
    UIFunctions.cpp
    UIProxy.cpp
    plugin.cpp
    PersistentOptions.cpp
    qluacommanderwidget.cpp
    qcommandedit.cpp
    ${COPPELIASIM_INCLUDE_DIR}/simStack/stackObject.cpp
    ${COPPELIASIM_INCLUDE_DIR}/simStack/stackNull.cpp
    ${COPPELIASIM_INCLUDE_DIR}/simStack/stackBool.cpp
    ${COPPELIASIM_INCLUDE_DIR}/simStack/stackNumber.cpp
    ${COPPELIASIM_INCLUDE_DIR}/simStack/stackString.cpp
    ${COPPELIASIM_INCLUDE_DIR}/simStack/stackArray.cpp
    ${COPPELIASIM_INCLUDE_DIR}/simStack/stackMap.cpp
)

set(LIBRARIES
    Boost::boost
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::PrintSupport
)

coppeliasim_add_plugin(simExtLuaCmd SOURCES ${SOURCES})
option(CUSTOM_TOOLTIP_WINDOW "use a custom tooltip window class instead of the default tooltip")
if(CUSTOM_TOOLTIP_WINDOW)
    target_compile_definitions(simExtLuaCmd PRIVATE -DCUSTOM_TOOLTIP_WINDOW)
endif()
option(USE_LUA_PARSER "use the lua-parser module to determine correct calltip")
if(USE_LUA_PARSER)
    target_compile_definitions(simExtLuaCmd PRIVATE -DUSE_LUA_PARSER)
endif()
target_link_libraries(simExtLuaCmd ${LIBRARIES})
coppeliasim_add_lua(test/getCallContexts.lua)
