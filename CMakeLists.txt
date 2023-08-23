cmake_minimum_required(VERSION 3.16.3)
project(simLuaCmd)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_MACOSX_RPATH 1)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)


set(Qt Qt5 CACHE STRING "Qt version to use (e.g. Qt5)")
set_property(CACHE Qt PROPERTY STRINGS Qt5 Qt6)  #

find_package(Boost REQUIRED)
find_package(${Qt} COMPONENTS Core Gui Widgets PrintSupport REQUIRED)

if(NOT COPPELIASIM_INCLUDE_DIR)
    if(DEFINED ENV{COPPELIASIM_ROOT_DIR})
        set(COPPELIASIM_INCLUDE_DIR $ENV{COPPELIASIM_ROOT_DIR}/programming/include)
    else()
        set(COPPELIASIM_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../include)
    endif()
endif()
list(APPEND CMAKE_MODULE_PATH
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules
    ${COPPELIASIM_INCLUDE_DIR}/cmake)
find_package(CoppeliaSim 4.5.1.7 REQUIRED)

include(ExternalProject)
set(REPLXX_PREFIX ${CMAKE_CURRENT_BINARY_DIR}/replxx-ext-prefix)
set(REPLXX_SRC_DIR ${REPLXX_PREFIX}/src/replxx-ext)
set(REPLXX_BUILD_DIR ${REPLXX_PREFIX}/src/replxx-ext-build)
ExternalProject_Add(replxx-ext
    GIT_REPOSITORY https://github.com/AmokHuginnsson/replxx
    GIT_TAG master
    CMAKE_ARGS -DCMAKE_C_FLAGS=-fPIC -DCMAKE_CXX_FLAGS=-fPIC -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE} -DREPLXX_BUILD_EXAMPLES=OFF -DREPLXX_BUILD_PACKAGE=OFF
    BUILD_COMMAND ${CMAKE_COMMAND} --build ${REPLXX_BUILD_DIR} --config ${CMAKE_BUILD_TYPE}
    INSTALL_COMMAND ""
)
include_directories(${REPLXX_SRC_DIR}/include)
if(WIN32)
    set(REPLXX_LIBRARY ${REPLXX_BUILD_DIR}/${CMAKE_BUILD_TYPE}/replxx-static.lib)
else()
    set(REPLXX_LIBRARY ${REPLXX_BUILD_DIR}/libreplxx.a)
endif()

include_directories(${CMAKE_CURRENT_BINARY_DIR}/generated)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external)

coppeliasim_generate_stubs(${CMAKE_CURRENT_BINARY_DIR}/generated XML_FILE ${CMAKE_CURRENT_SOURCE_DIR}/callbacks.xml LUA_FILE ${CMAKE_CURRENT_SOURCE_DIR}/simLuaCmd.lua)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

set(SOURCES
    SIM.cpp
    UI.cpp
    plugin.cpp
    PersistentOptions.cpp
    qluacommanderwidget.cpp
    qcommandedit.cpp
    ConsoleREPL.cpp
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
    Qt::Core
    Qt::Gui
    Qt::Widgets
    Qt::PrintSupport
    ${REPLXX_LIBRARY}
)

coppeliasim_add_plugin(simLuaCmd SOURCES ${SOURCES})
target_compile_definitions(simLuaCmd PRIVATE QT_COMPIL)
add_dependencies(simLuaCmd replxx-ext)
option(CUSTOM_TOOLTIP_WINDOW "use a custom tooltip window class instead of the default tooltip")
if(CUSTOM_TOOLTIP_WINDOW)
    target_compile_definitions(simLuaCmd PRIVATE CUSTOM_TOOLTIP_WINDOW)
endif()
option(USE_LUA_PARSER "use the lua-parser module to determine correct calltip")
if(USE_LUA_PARSER)
    target_compile_definitions(simLuaCmd PRIVATE USE_LUA_PARSER)
endif()
target_compile_definitions(simLuaCmd PRIVATE REPLXX_STATIC)
target_link_libraries(simLuaCmd PRIVATE ${LIBRARIES})
coppeliasim_add_lua(test/getCallContexts.lua)
