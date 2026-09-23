add_executable(Utf8BoundedTest
  Utf8BoundedTest.cpp
  ${REPO_ROOT}/lib/Utf8/Utf8.cpp
)

target_include_directories(Utf8BoundedTest PRIVATE
  ${REPO_ROOT}/lib/Utf8
)

target_link_libraries(Utf8BoundedTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(Utf8BoundedTest)
