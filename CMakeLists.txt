cmake_minimum_required(VERSION 3.0.0)
project(clalua)

set(CMAKE_CXX_STANDARD 20)
set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug/lib)
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug/lib)
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug/bin)
set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release/lib)
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release/lib)
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release/bin)

set(EXTERNAL_DIR ${CMAKE_CURRENT_LIST_DIR}/_external)
subdirs(fmt clang lualib lua luafilesystem lrdb_server clalua)
