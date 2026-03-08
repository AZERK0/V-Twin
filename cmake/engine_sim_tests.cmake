if (NOT ENGINE_SIM_BUILD_TESTS)
    return()
endif()

enable_testing()

add_executable(engine-sim-test
    ${ENGINE_SIM_TEST_SOURCES}
)

target_link_libraries(engine-sim-test
    gtest_main
    engine-sim
)

include(GoogleTest)
gtest_discover_tests(engine-sim-test)
