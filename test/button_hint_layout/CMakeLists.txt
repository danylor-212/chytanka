add_executable(ButtonHintLayoutTest
  ButtonHintLayoutTest.cpp
)

target_include_directories(ButtonHintLayoutTest PRIVATE
  ${REPO_ROOT}/src
)

target_link_libraries(ButtonHintLayoutTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(ButtonHintLayoutTest)
