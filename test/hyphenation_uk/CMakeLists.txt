add_executable(HyphenationUkrainianTest
  HyphenationUkrainianTest.cpp
  ${REPO_ROOT}/lib/Epub/Epub/hyphenation/Hyphenator.cpp
  ${REPO_ROOT}/lib/Epub/Epub/hyphenation/LanguageRegistry.cpp
  ${REPO_ROOT}/lib/Epub/Epub/hyphenation/LiangHyphenation.cpp
  ${REPO_ROOT}/lib/Epub/Epub/hyphenation/HyphenationCommon.cpp
  ${REPO_ROOT}/lib/Utf8/Utf8.cpp
)

target_include_directories(HyphenationUkrainianTest PRIVATE
  ${REPO_ROOT}/lib/Epub
  ${REPO_ROOT}/lib/Utf8
)

target_link_libraries(HyphenationUkrainianTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(HyphenationUkrainianTest)
