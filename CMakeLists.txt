cmake_minimum_required(VERSION 3.24)

project(srpy VERSION 1.0 DESCRIPTION "Python bindings for sigrok C++ extension" LANGUAGES CXX)

#set(CMAKE_C_COMPILER clang)
#set(CMAKE_CXX_COMPILER clang++)

include(GNUInstallDirs)
include(CheckCXXCompilerFlag)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS " -O3 -ggdb3 -march=native")# -save-temps ")# -ffast-math -fomit-frame-pointer -fstrict-aliasing -ftree-vectorize -msse2 -fPIC ")
#set(CMAKE_CXX_FLAGS "-O3")
#set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING
    "Choose the type of build (None, Debug, Release, RelWithDebInfo, MinSizeRel)."
    FORCE)
endif()

#set(CMAKE_EXPORT_COMPILE_COMMANDS ON) #gen compile_commands.json

include(CheckIPOSupported)
check_ipo_supported(RESULT supported OUTPUT error)
if( supported )
    message(STATUS "IPO / LTO enabled")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
    message(STATUS "IPO / LTO not supported: <${error}>")
endif()

#add_subdirectory(c-blosc2)


#find_package(Blosc2 REQUIRED)
#list(APPEND LINK_LIBS blosc2)

find_package(OpenMP REQUIRED)
list(APPEND LINK_LIBS OpenMP::OpenMP_CXX)


find_package(Threads REQUIRED)
list(APPEND LINK_LIBS Threads::Threads)

find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)
find_package(pybind11 CONFIG REQUIRED)
list(APPEND LINK_LIBS pybind11::headers)


find_package(PkgConfig REQUIRED)
#list(APPEND PKGDEPS glib-2.0)
#list(APPEND PKGDEPS glibmm-2.4)
list(APPEND PKGDEPS libzstd)
list(APPEND PKGDEPS uuid)
list(APPEND PKGDEPS libsigrokcxx)
list(APPEND PKGDEPS libsigrokdecode)
pkg_check_modules(PKGDEPS REQUIRED ${PKGDEPS})

list(APPEND LINK_LIBS ${PKGDEPS_LIBRARIES})

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PKGDEPS_INCLUDE_DIRS}
)

link_directories(${PKGDEPS_LIBRARY_DIRS})

set(SRC_FILES
    #srcxx/srpsamples.cpp
    srcxx/srpsamples_segmented.cpp
    srcxx/srpconfig.cpp
    srcxx/srpchannels.cpp
    srcxx/srpdevice.cpp
    srcxx/srpdriver.cpp
    srcxx/srpsession.cpp
    srcxx/srpmanager.cpp
    srpy/srpy.cpp
)

set(HDR_FILES
    #srcxx/srpsamples.hpp
    srcxx/srpsamples_segmented.hpp
    srcxx/srpconfig.hpp
    srcxx/srpchannels.hpp
    srcxx/srpdevice.hpp
    srcxx/srpdriver.hpp
    srcxx/srpsession.hpp
    srcxx/srpmanager.hpp
    srcxx/utils.hpp
    srcxx/srpcxx.hpp
)

# pybind11_add_module(${PROJECT_NAME} MODULE
#     ${SRC_FILES}
#     ${HDR_FILES}
# )

Python_add_library(${PROJECT_NAME} MODULE ${SRC_FILES} WITH_SOABI)
target_link_libraries(${PROJECT_NAME} PUBLIC ${LINK_LIBS})

install(TARGETS srpy
   COMPONENT python
   #LIBRARY DESTINATION ${Python_SITELIB}
   LIBRARY DESTINATION ${PYTHON_SITE}
)
