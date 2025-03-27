cmake_minimum_required(VERSION 3.10)
project(samsung-cli)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add executable
add_executable(samsung-cli src/samsung-cli.cpp)

# Add compiler flags
target_compile_options(samsung-cli PRIVATE -Wall -Wextra)

# Installation rules
include(GNUInstallDirs)
install(TARGETS samsung-cli
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Create uninstall target
if(NOT TARGET uninstall)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
        IMMEDIATE @ONLY)

    add_custom_target(uninstall
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
endif() 