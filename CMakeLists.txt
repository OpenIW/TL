cmake_minimum_required(VERSION 3.22)
project(TL LANGUAGES CXX)
file(GLOB_RECURSE TL RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp *.h *.inl)

add_library(TL
  ${TL}
)

target_include_directories(TL
PUBLIC
  base/include physics/include
  # jobqueue.h is not in the public header directory but is in the source
  # directory so we need to include it here to make sure it is included in the
  # target header file list (otherwise it will be included in the target source file list)
  #
  # This is a hack to get around the fact that the CMake build system does not yet support the
  # ability to specify a list of files to include in a target.
  # See https://gitlab.kitware.com/cmake/cmake/issues/16053 for more details.
  jobqueue
)

target_compile_definitions(TL
PRIVATE
  "_CRT_SECURE_NO_WARNINGS"
)
