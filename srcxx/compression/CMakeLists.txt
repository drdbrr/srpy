#https://github.com/weidenka/cmake-subdirectory-dependencies/tree/master
#https://evileg.com/en/post/436/

find_package(Blosc2 REQUIRED)
if (Blosc2_FOUND)
    message(STATUS "Found Blosc2")
endif()

add_library(compression compress.cpp)

target_sources(compression
  PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/compress.cpp
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/compress.hpp
  )
target_include_directories(compression
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
  )
