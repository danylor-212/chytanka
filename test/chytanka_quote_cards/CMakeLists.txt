# Fork-only («Читанка»): embedded quote card decoder and shuffle-bag picker.
# Set CHYTANKA_BRAND_DIR (the brand repo's brand/ directory) when running the
# test to also compare the decoded cards with the source BMPs pixel by pixel.
add_executable(ChytankaQuoteCardsTest
  ChytankaQuoteCardsTest.cpp
  ${REPO_ROOT}/src/activities/boot_sleep/ChytankaQuoteDecoder.cpp
  ${REPO_ROOT}/src/images/ChytankaQuoteCards.cpp
  ${REPO_ROOT}/lib/uzlib/src/tinflate.c
  ${REPO_ROOT}/lib/uzlib/src/uzlib_checksums.c
)

target_include_directories(ChytankaQuoteCardsTest PRIVATE
  ${REPO_ROOT}/src
  ${REPO_ROOT}/src/activities/boot_sleep
  ${REPO_ROOT}/lib/uzlib/src
  ${REPO_ROOT}/lib/GfxRenderer
)

target_compile_definitions(ChytankaQuoteCardsTest PRIVATE CHYTANKA=1)

target_link_libraries(ChytankaQuoteCardsTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(ChytankaQuoteCardsTest)
