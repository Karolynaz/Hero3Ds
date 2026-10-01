###########################################################################
#   fheroes2: https://github.com/ihhub/fheroes2                           #
#   Copyright (C) 2026                                                    #
#                                                                         #
#   This program is free software; you can redistribute it and/or modify  #
#   it under the terms of the GNU General Public License as published by  #
#   the Free Software Foundation; either version 2 of the License, or     #
#   (at your option) any later version.                                   #
#                                                                         #
#   This program is distributed in the hope that it will be useful,       #
#   but WITHOUT ANY WARRANTY; without even the implied warranty of        #
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         #
#   GNU General Public License for more details.                          #
#                                                                         #
#   You should have received a copy of the GNU General Public License     #
#   along with this program; if not, write to the                         #
#   Free Software Foundation, Inc.,                                       #
#   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             #
###########################################################################

# Native engine build. The separate Makefile remains a hardware probe only.
cmake_minimum_required(VERSION 3.24)
project(Hero3DS LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT DEFINED ENV{DEVKITPRO})
    message(FATAL_ERROR "Install devkitPro devkitARM, libctru, 3ds-zlib, 3ds-libvorbisidec and devkitarm-cmake, 3ds-cmake; set DEVKITPRO.")
endif()
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Nintendo3DS")
    message(FATAL_ERROR "Configure with -DCMAKE_TOOLCHAIN_FILE=$ENV{DEVKITPRO}/cmake/3DS.cmake")
endif()
include(Platform/Nintendo3DS)
set(FHEROES2_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
file(GLOB ENGINE_SOURCES CONFIGURE_DEPENDS "${FHEROES2_ROOT}/src/engine/*.cpp")
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS "${FHEROES2_ROOT}/src/fheroes2/*.cpp")
add_library(smacker STATIC "${FHEROES2_ROOT}/src/thirdparty/libsmacker/smacker.c")
target_include_directories(smacker PUBLIC "${FHEROES2_ROOT}/src/thirdparty/libsmacker")
add_library(engine STATIC ${ENGINE_SOURCES})
target_compile_definitions(engine PUBLIC TARGET_NINTENDO_3DS)
target_include_directories(engine PUBLIC "${FHEROES2_ROOT}/src/engine" "$ENV{DEVKITPRO}/libctru/include" "$ENV{DEVKITPRO}/portlibs/3ds/include")
target_link_directories(engine PUBLIC "$ENV{DEVKITPRO}/libctru/lib" "$ENV{DEVKITPRO}/portlibs/3ds/lib")
target_link_libraries(engine PUBLIC smacker vorbisidec ogg ctru z m)
add_executable(Hero3DS ${GAME_SOURCES})
foreach(DIRECTORY agg ai army audio battle campaign castle dialog editor game gui h2d heroes image kingdom maps monster resource spell system world)
    target_include_directories(Hero3DS PRIVATE "${FHEROES2_ROOT}/src/fheroes2/${DIRECTORY}")
endforeach()
target_compile_definitions(Hero3DS PRIVATE FHEROES2_DATA=sdmc:/3ds/fheroes2)
target_link_libraries(Hero3DS PRIVATE engine)
ctr_generate_smdh(Hero3DS.smdh NAME "Heroes II 3DS" DESCRIPTION "fheroes2 Nintendo 3DS port" AUTHOR "fheroes2 / Hero3DS")
ctr_create_3dsx(Hero3DS SMDH Hero3DS.smdh)
