cmake_minimum_required(VERSION 3.15)
project(egi_amp_cli LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define Windows target version to avoid warnings
if(WIN32)
  add_definitions(-D_WIN32_WINNT=0x0A00)  # Windows 10
endif()

# Use modern Boost config (disable deprecated FindBoost warnings)
cmake_policy(SET CMP0167 NEW)

# Find Boost using modern CMake approach
find_package(Boost REQUIRED)
if(NOT Boost_FOUND)
  message(FATAL_ERROR "Could not find Boost")
endif()

message(STATUS "Boost version: ${Boost_VERSION}")
message(STATUS "Boost include dirs: ${Boost_INCLUDE_DIRS}")

# For modern Boost (1.69+), system is header-only, so just use Boost::boost
set(BOOST_TARGETS Boost::boost)

# --- LSL ---
find_package(LSL CONFIG REQUIRED)
if(NOT LSL_FOUND)
  message(FATAL_ERROR "Could not find liblsl")
endif()

add_library(egi_client src/EGIClient.cpp)
target_include_directories(egi_client PUBLIC include)
target_link_libraries(egi_client PUBLIC ${BOOST_TARGETS} LSL::lsl)

add_executable(egi_amp_cli src/main.cpp)
target_include_directories(egi_amp_cli PRIVATE include)
target_link_libraries(egi_amp_cli PRIVATE egi_client ${BOOST_TARGETS} LSL::lsl)

if(UNIX AND NOT APPLE)
  target_link_libraries(egi_amp_cli PRIVATE pthread)
endif()
