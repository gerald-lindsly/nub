IF(LZO_FOUND)
	add_executable (test_res test_res.cpp)
	target_link_libraries(test_res nub ${LZO_LIBRARIES})
ENDIF(LZO_FOUND)
