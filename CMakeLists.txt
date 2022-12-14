cmake_minimum_required(VERSION 3.3 FATAL_ERROR)
project(uringpp)
set(CMAKE_CXX_STANDARD 20)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(LibUring 2.0 REQUIRED)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif ()

string(LENGTH "${CMAKE_SOURCE_DIR}/" SOURCE_PATH_LENGTH)
add_definitions("-DSOURCE_PATH_LENGTH=${SOURCE_PATH_LENGTH}")

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  option(URINGPP_BUILD_EXAMPLES "Build examples" ON)
else()
  option(URINGPP_BUILD_EXAMPLES "Build examples" OFF)
endif()

set(URINGPP_SOURCE_FILES
  src/event_loop.cc
)

add_library(uringpp STATIC ${URINGPP_SOURCE_FILES})
set(URINGPP_LINK_LIBRARIES URING::uring)
list(APPEND
  URINGPP_COMPILE_OPTIONS
  PUBLIC
  -flto
  PRIVATE
  -fno-rtti
  -Wall 
  -Wextra 
  -pedantic 
  -Werror
)
list(APPEND
  URINGPP_LINK_OPTIONS
  PUBLIC
  -flto
  PRIVATE
  -fno-rtti
)

if (CMAKE_CXX_COMPILER_ID MATCHES Clang)
  list(APPEND
    URINGPP_COMPILE_OPTIONS
    PUBLIC
    -stdlib=libc++
    PUBLIC
    -fcoroutines-ts
  )
  list(APPEND
    URINGPP_LINK_OPTIONS
    PUBLIC
    -stdlib=libc++
  )
elseif (CMAKE_CXX_COMPILER_ID MATCHES GNU)
  if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS "11") 
    list(APPEND
      URINGPP_COMPILE_OPTIONS
      PUBLIC
      -fcoroutines
    )
  endif ()
endif ()
if (URINGPP_COMPILE_OPTIONS)
  target_compile_options(uringpp ${URINGPP_COMPILE_OPTIONS})
endif ()
if (URINGPP_LINK_OPTIONS)
  target_link_options(uringpp ${URINGPP_LINK_OPTIONS})
endif ()
target_link_libraries(uringpp ${URINGPP_LINK_LIBRARIES})
target_include_directories(uringpp PUBLIC include)

set(URINGPP_EXAMPLES helloworld)
if (URINGPP_BUILD_EXAMPLES)
  foreach (EXAMPLE ${URINGPP_EXAMPLES})
    add_executable(${EXAMPLE} examples/${EXAMPLE}.cc)
    target_link_libraries(${EXAMPLE} uringpp) 
  endforeach ()
endif ()

include(GNUInstallDirs)
install(TARGETS uringpp EXPORT uringpp ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/uringpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})