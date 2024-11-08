cmake_minimum_required(VERSION 3.12 FATAL_ERROR)

project(mmap_hdf5)

set(CMAKE_VERBOSE_MAKEFILE TRUE)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0")
# Add HDF5
include_directories($ENV{HDF5_HOME}/include)
link_directories($ENV{HDF5_HOME}/lib)

set(SOURCES
  source/main.c
)
add_executable(mmap_hdf5 ${SOURCES})

target_link_libraries(mmap_hdf5 PRIVATE hdf5)
