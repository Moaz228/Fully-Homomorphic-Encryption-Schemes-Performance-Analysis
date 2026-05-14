include(FetchContent)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
cmake_minimum_required(VERSION 3.31)
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
set(CMAKE_BUILD_TOOL make)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

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
  # OPENFHEcore
  # OPENFHEpke
  # OPENFHEbinfhe
  # openfhe
  OPENFHEpke_static
  OPENFHEcore_static
  OPENFHEbinfhe_static
  Threads::Threads
  OpenMP::OpenMP_CXX
)

set(SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/bfv.cxx
  ${CMAKE_CURRENT_SOURCE_DIR}/src/FileWriter.cxx
)

set(BUILD_UNITTESTS OFF CACHE BOOL "Disable OpenFHE unit tests")
set(BUILD_EXAMPLES OFF CACHE BOOL "Disable OpenFHE examples")
set(BUILD_BENCHMARKS OFF CACHE BOOL "Disable OpenFHE benchmarks")
set(BUILD_SHARED OFF CACHE BOOL "Build shared OpenFHE libraries")
set(BUILD_STATIC ON CACHE BOOL "Don't build static libraries")
set(BUILD_EXTRAS OFF CACHE BOOL "Disable extras")
set(NATIVE_PYTHON OFF CACHE BOOL "" FORCE)


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

find_package(Threads REQUIRED)
find_package(OpenMP REQUIRED)


# --- BFV Binary ---
add_executable(bfv_bin.out src/bfv.cxx src/FileWriter.cxx)
target_include_directories(bfv_bin.out PRIVATE ${INCLUDE_DIRS})
target_link_libraries(bfv_bin.out PRIVATE ${COMMON_LIBS})

# --- BGV Binary ---
add_executable(bgv_bin.out src/bgv.cxx src/FileWriter.cxx)
target_include_directories(bgv_bin.out PRIVATE ${INCLUDE_DIRS})
target_link_libraries(bgv_bin.out PRIVATE ${COMMON_LIBS})

# --- CKKS Binary ---
add_executable(ckks_bin.out src/ckks.cxx src/FileWriter.cxx)
target_include_directories(ckks_bin.out PRIVATE ${INCLUDE_DIRS})
target_link_libraries(ckks_bin.out PRIVATE ${COMMON_LIBS})

# --- Cloud Math Binary ---
add_executable(cloud_math.out src/cloud_math.cxx src/FileWriter.cxx)
target_include_directories(cloud_math.out PRIVATE ${INCLUDE_DIRS})
target_link_libraries(cloud_math.out PRIVATE ${COMMON_LIBS})
