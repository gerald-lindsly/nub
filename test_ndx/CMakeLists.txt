IF(CMAKE_COMPILER_IS_GNUCXX)
   SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wdeprecated-declarations-")
ENDIF(CMAKE_COMPILER_IS_GNUCXX)

add_executable (test_ndx test_ndx.cpp)