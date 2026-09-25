add_executable(UkrainianTextTest
  UkrainianTextTest.cpp
  ${REPO_ROOT}/lib/Epub/Epub/text/UkrainianText.cpp
)

target_include_directories(UkrainianTextTest PRIVATE
  ${REPO_ROOT}/lib/Epub/Epub/text
  ${REPO_ROOT}/lib/Epub/Epub
)

target_link_libraries(UkrainianTextTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(UkrainianTextTest)
