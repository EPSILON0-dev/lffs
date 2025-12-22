cmake_minimum_required(VERSION 3.10)

# Project name
project(fs C)

# Set C standard
set(CMAKE_C_STANDARD 11)

# Export compile commands
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add source files
add_executable(fs src/main.c src/fs.c)

target_include_directories(fs PRIVATE src)
target_link_libraries(fs PRIVATE m)
target_compile_options(fs PRIVATE -fsanitize=address -O0 -g -Wall -Wextra -Wpedantic)
target_link_options(fs PRIVATE -fsanitize=address)

# Create link to fs in the build directory
add_custom_command(TARGET fs POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${CMAKE_CURRENT_BINARY_DIR}/fs
    ${CMAKE_CURRENT_SOURCE_DIR}/fs
)
