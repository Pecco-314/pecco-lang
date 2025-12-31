if (BUILD_TESTING)
	include(FetchContent)

	FetchContent_Declare(
		googletest
		URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
	)

	set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
	FetchContent_MakeAvailable(googletest)

	add_executable(pecco_lexer_tests
		${CMAKE_CURRENT_SOURCE_DIR}/lexer_tests.cpp
	)

	target_link_libraries(pecco_lexer_tests
		PRIVATE
			pecco_lib
			GTest::gtest_main
	)

	target_compile_features(pecco_lexer_tests PRIVATE cxx_std_20)

	include(GoogleTest)
	gtest_discover_tests(pecco_lexer_tests)

	add_executable(pecco_driver_tests
		${CMAKE_CURRENT_SOURCE_DIR}/driver_tests.cpp
	)

	target_link_libraries(pecco_driver_tests
		PRIVATE
			GTest::gtest_main
	)

	target_compile_features(pecco_driver_tests PRIVATE cxx_std_20)

	target_compile_definitions(pecco_driver_tests PRIVATE
		PLC_BINARY="${CMAKE_BINARY_DIR}/src/plc"
		TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
	)

	gtest_discover_tests(pecco_driver_tests)
endif()
