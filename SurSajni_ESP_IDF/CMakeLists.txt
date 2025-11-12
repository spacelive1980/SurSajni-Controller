# ESP-IDF Project CMakeLists.txt
# Sursajni Wireless Pump Controller - ESP-IDF Version

cmake_minimum_required(VERSION 3.16)

# Set project name
set(PROJECT_NAME "sursajni_controller")

# Include ESP-IDF CMake
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Define the project
project(${PROJECT_NAME})
