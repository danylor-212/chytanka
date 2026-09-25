set(DICTIONARY_LOOKUP_SOURCES
  ${REPO_ROOT}/src/util/Dictionary.cpp
  ${REPO_ROOT}/lib/Utf8/Utf8.cpp
)

set(DICTIONARY_LOOKUP_INCLUDES
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${REPO_ROOT}/src/util
  ${REPO_ROOT}/lib/Utf8
  ${REPO_ROOT}/lib/Memory
)

add_executable(DictionaryLookupTest
  DictionaryLookupTest.cpp
  ${DICTIONARY_LOOKUP_SOURCES}
)
target_include_directories(DictionaryLookupTest PRIVATE ${DICTIONARY_LOOKUP_INCLUDES})
target_link_libraries(DictionaryLookupTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)
gtest_discover_tests(DictionaryLookupTest)
