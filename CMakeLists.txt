include(FetchContent)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
cmake_minimum_required(VERSION 3.31)

set(CMAKE_C_COMPILER gcc)
set(CMAKE_CXX_COMPILER g++)
set(CMAKE_BUILD_TOOL make)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)

set(BIN_NAME bin.out)
set(COMMON_COMPILE_OPTIONS -Wall -Wextra)
set(INCLUDE_DIR
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/extern/openfhe/src/pke/include
  ${CMAKE_CURRENT_SOURCE_DIR}/extern/openfhe/src/core/include
  ${CMAKE_CURRENT_SOURCE_DIR}/extern/openfhe/src/binfhe/include
  ${CMAKE_CURRENT_BINARY_DIR}/extern/openfhe/src/core

  ${CMAKE_CURRENT_SOURCE_DIR}/extern/openfhe/third-party/cereal/include
)
set(COMMON_LIBS
  OPENFHEcore
  OPENFHEpke
  OPENFHEbinfhe
  # openfhe
)

set(SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/bfv.cxx
  ${CMAKE_CURRENT_SOURCE_DIR}/src/FileWriter.cxx
)

set(BUILD_UNITTESTS OFF CACHE BOOL "Disable OpenFHE unit tests")
set(BUILD_EXAMPLES OFF CACHE BOOL "Disable OpenFHE examples")
set(BUILD_BENCHMARKS OFF CACHE BOOL "Disable OpenFHE benchmarks")
set(BUILD_SHARED ON CACHE BOOL "Build shared OpenFHE libraries")
set(BUILD_STATIC OFF CACHE BOOL "Don't build static libraries")
set(BUILD_EXTRAS OFF CACHE BOOL "Disable extras")

# FetchContent_Declare(
#   openfhe 
#   URL https://github.com/openfheorg/openfhe-development/archive/refs/tags/v1.4.2.tar.gz
#   DOWNLOAD_EXTRACT_TIMESTAMP true
#   SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/extern/openfhe
#   BINARY_DIR ${CMAKE_BINARY_DIR}/extern/openfhe
# )

FetchContent_Declare(
  openfhe 
  GIT_REPOSITORY https://github.com/openfheorg/openfhe-development
  GIT_TAG v1.4.2
  GIT_SHALLOW TRUE
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/extern/openfhe
  BINARY_DIR ${CMAKE_BINARY_DIR}/extern/openfhe
)

FetchContent_MakeAvailable(openfhe)

include_directories(${INCLUDE_DIR})

project(openfhe_imp LANGUAGES C CXX VERSION 0.1.2)

add_executable(${BIN_NAME} ${SOURCES})
target_link_libraries(${BIN_NAME} PRIVATE ${COMMON_LIBS})
target_compile_options(${BIN_NAME} PRIVATE ${COMMON_COMPILE_OPTIONS})
