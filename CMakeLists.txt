cmake_minimum_required (VERSION 2.8.10)
project (nub)

IF(CMAKE_COMPILER_IS_GNUCXX)
   SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -D_FILE_OFFSET_BITS=64")
   add_library (nub STATIC src/FileSystem.cpp src/ResourceGet.cpp  
                           src/Index.cpp src/ResourcePut.cpp src/imemstream.cpp
                           src/ResourceFile.cpp src/UniIndex.cpp)

ELSE
	add_library (nub STATIC src/FileSystem.cpp src/ResourceGet.cpp  
	                        src/Index.cpp src/ResourcePut.cpp src/imemstream.cpp
	                        src/ResourceFile.cpp src/UniIndex.cpp src/istreamBMP.cpp)
ENDIF(CMAKE_COMPILER_IS_GNUCXX)


include_directories (include)

add_subdirectory (test_ndx)
add_subdirectory (test_res)
add_subdirectory (test_gen1000x16)
