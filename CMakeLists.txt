cmake_minimum_required(VERSION 3.24)

project(srpy VERSION 1.0 DESCRIPTION "Python bindings for sigrok C++ extension" LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
#set(CMAKE_CXX_FLAGS "-O3")
set(CMAKE_CXX_FLAGS "-Og")

#set(CMAKE_CXX_FLAGS "-flto=auto")

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_BUILD_TYPE Debug)
#set(CMAKE_EXPORT_COMPILE_COMMANDS ON) #gen compile_commands.json

if(POLICY CMP0054)
	cmake_policy(SET CMP0054 NEW)
endif()

cmake_policy(SET CMP0057 NEW)

include(CheckIPOSupported)
check_ipo_supported(RESULT supported OUTPUT error)
if( supported )
    message(STATUS "IPO / LTO enabled")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
    message(STATUS "IPO / LTO not supported: <${error}>")
endif()

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_THREAD_LIBS_INIT}
)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

#find_package(Snappy CONFIG REQUIRED)

#set (PYBIND11_PYTHON_VERSION "3.11")
find_package(pybind11 CONFIG REQUIRED)

pkg_search_module(UUID REQUIRED uuid)

#-------------------------------#
    #list(APPEND PKGDEPS glibmm)
#-------------------------------#

list(APPEND PKGDEPS libsigrokcxx)

find_package(PkgConfig)
pkg_check_modules(PKGDEPS REQUIRED ${PKGDEPS})

set(CMAKE_REQUIRED_INCLUDES "${PKGDEPS_INCLUDE_DIRS}")
set(CMAKE_REQUIRED_LIBRARIES "libsigrokcxx")

include_directories(${PKGDEPS_INCLUDE_DIRS})

foreach (LPATH ${PKGDEPS_LIBRARY_DIRS})
    list(APPEND CMAKE_REQUIRED_LINK_OPTIONS "-L${LPATH}")
endforeach ()

link_directories(${PKGDEPS_LIBRARY_DIRS})
list(APPEND LINK_LIBS ${PKGDEPS_LIBRARIES})



find_package (Python ${PYBIND11_PYTHON_VERSION} EXACT COMPONENTS Interpreter)
message("PYTHON_SITEARCH----------------->:${Python_SITEARCH}")



set(SRC_FILES
    srcxx/srpsamples.cpp
    srcxx/srpconfig.cpp
    srcxx/srpchannels.cpp
    srcxx/srpdevice.cpp
    srcxx/srpdriver.cpp
    srcxx/srpsession.cpp
    srcxx/srpmanager.cpp
    srpy/srpy.cpp
)

set(HDR_FILES
    srcxx/srpsamples.hpp
    srcxx/srpconfig.hpp
    srcxx/srpchannels.hpp
    srcxx/srpdevice.hpp
    srcxx/srpdriver.hpp
    srcxx/srpsession.hpp
    srcxx/srpmanager.hpp
    srcxx/utils.hpp
    srcxx/srpcxx.hpp
)

pybind11_add_module(${PROJECT_NAME} MODULE
    ${SRC_FILES}
    ${HDR_FILES}
)

target_link_libraries(${PROJECT_NAME} PUBLIC ${LINK_LIBS} Threads::Threads uuid)


install(TARGETS srpy
   COMPONENT python
   LIBRARY DESTINATION "${Python_SITEARCH}"
)

# install(TARGETS srpy
#   COMPONENT python
#   RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
#   LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
#   ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
#)
 
