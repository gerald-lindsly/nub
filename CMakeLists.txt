cmake_minimum_required (VERSION 2.8.10)
project (nub)

include_directories (include/nub)

add_library (nub STATIC src/FileSystem.cpp src/ResourceGet.cpp  
src/Index.cpp src/ResourcePut.cpp  src/imemstream.cpp
src/ResourceFile.cpp  src/UniIndex.cpp     src/istreamBMP.cpp)

add_subdirectory (test_ndx)
add_subdirectory (test_res)
