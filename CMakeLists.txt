cmake_minimum_required(VERSION 3.15)
project(starter_clap_plugin)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Apple universal binary support
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15")
endif()

# Optimization flags
set(CMAKE_CXX_FLAGS "-g -O2")
set(CMAKE_C_FLAGS "-g -O2")

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS_RELEASE "-g -O2 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g -O2 -DNDEBUG")
elseif(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Zi /DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/O2 /Zi /DNDEBUG")
endif()

# Fetch CLAP headers
include(FetchContent)

FetchContent_Declare(
  clap
  GIT_REPOSITORY https://github.com/free-audio/clap.git
  GIT_TAG        1.2.6
)

FetchContent_MakeAvailable(clap)

# Create the plugin library
add_library(starter_clap_plugin SHARED
    plugin.h
    plugin.cpp
    dsp.cpp
    params.cpp
)

# Link CLAP headers
target_include_directories(starter_clap_plugin PRIVATE ${clap_SOURCE_DIR}/include)

# Set proper library properties for CLAP plugin
set_target_properties(starter_clap_plugin PROPERTIES
    PREFIX ""
    SUFFIX ".clap"
    OUTPUT_NAME "EvansPluginTemplate"
)
