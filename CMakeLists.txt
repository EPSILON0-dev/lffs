cmake_minimum_required(VERSION 3.10)

# Project name
project(fs C)

# Set C standard
set(CMAKE_C_STANDARD 11)

# Export compile commands
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable AddressSanitizer and debugging flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -O0 -g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -O0 -g")

# Add source files
add_executable(fs src/main.c src/fs.c)

target_include_directories(fs PRIVATE src)