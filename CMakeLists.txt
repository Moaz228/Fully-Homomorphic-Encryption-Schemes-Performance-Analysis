cmake_minimum_required(VERSION 3.20)
project(openfhe_imp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find the system-wide OpenFHE engine
find_package(OpenFHE REQUIRED)

# The official OpenFHE method for including all the cryptography headers
include_directories(
    ${OPENMP_INCLUDES}
    ${OpenFHE_INCLUDE}
    ${OpenFHE_INCLUDE}/third-party/include
    ${OpenFHE_INCLUDE}/core
    ${OpenFHE_INCLUDE}/pke
    ${OpenFHE_INCLUDE}/binfhe
)

set(BIN_NAME bin.out)

set(SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/bfv.cxx
)

add_executable(${BIN_NAME} ${SOURCES})

# Point it to your friend's custom include folder for EncDataContainer
target_include_directories(${BIN_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Link using the official OpenFHE bundled variable
target_link_libraries(${BIN_NAME} PRIVATE ${OpenFHE_LIBRARIES})

# Add the BGV executable
add_executable(bgv_bin.out src/bgv.cxx)
target_include_directories(bgv_bin.out PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(bgv_bin.out PRIVATE ${OpenFHE_LIBRARIES})

# Add the CKKS executable
add_executable(ckks_bin.out src/ckks.cxx)
target_include_directories(ckks_bin.out PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(ckks_bin.out PRIVATE ${OpenFHE_LIBRARIES})