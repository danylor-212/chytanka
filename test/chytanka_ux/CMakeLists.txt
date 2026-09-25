# Fork-only («Читанка»): first-run welcome decision and the "currently
# reading" line on the quote sleep card.
add_executable(ChytankaUxTest ChytankaUxTest.cpp)

target_include_directories(ChytankaUxTest PRIVATE
  ${REPO_ROOT}/src/activities/home
  ${REPO_ROOT}/src/activities/boot_sleep
)

target_link_libraries(ChytankaUxTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(ChytankaUxTest)
