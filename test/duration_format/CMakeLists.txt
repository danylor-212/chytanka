add_executable(DurationFormatTest
  DurationFormatTest.cpp
)

target_include_directories(DurationFormatTest PRIVATE
  ${REPO_ROOT}/src
)

target_compile_definitions(DurationFormatTest PRIVATE
  TRANSLATIONS_DIR="${REPO_ROOT}/lib/I18n/translations"
)

target_link_libraries(DurationFormatTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(DurationFormatTest)
