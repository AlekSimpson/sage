cmake_minimum_required(VERSION 3.16)
project(sage)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable debug information for debugging in CLion
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")

# Add compile flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

# Include directories
include_directories(include/)

# Find LLVM
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# Set LLVM definitions and include directories
add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})

# LLVM is normally built without RTTI. Be consistent with that.
if(NOT LLVM_ENABLE_RTTI)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif()

# Find Boost
find_package(Boost REQUIRED COMPONENTS system algorithm)
if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    message(STATUS "Found Boost ${Boost_VERSION}")
endif()

# Collect all source files
file(GLOB_RECURSE SOURCES "src/*.cpp")

# Create executable
add_executable(sage ${SOURCES})

# Get LLVM libraries to link
llvm_map_components_to_libnames(llvm_libs 
    core 
    support 
    remarks 
    debuginfodwarf
)

# Link libraries
target_link_libraries(sage ${llvm_libs} ${Boost_LIBRARIES})

# Platform-specific configurations
if(APPLE)
    # macOS specific configurations
    target_compile_definitions(sage PRIVATE 
        __STDC_CONSTANT_MACROS 
        __STDC_FORMAT_MACROS 
        __STDC_LIMIT_MACROS
    )
    
    # Add macOS specific library paths if needed
    if(EXISTS "/opt/homebrew/lib")
        target_link_directories(sage PRIVATE /opt/homebrew/lib)
    endif()
    
    # Link additional libraries for macOS
    target_link_libraries(sage m)
    
elseif(UNIX AND NOT APPLE)
    # Linux specific configurations
    target_link_libraries(sage m tinfo)
endif()

# Set output directory
set_target_properties(sage PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Create build directories
file(MAKE_DIRECTORY "./build/bin")
file(MAKE_DIRECTORY "./build/objects")

# Add custom target for running with a sample file
add_custom_target(run
    COMMAND sage sage_modules/add.sage
    DEPENDS sage
    WORKING_DIRECTORY ./build/bin
    COMMENT "Running sage with add.sage"
)

# Print some useful information
message(STATUS "Project: ${PROJECT_NAME}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Source files: ${SOURCES}") 