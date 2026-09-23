#include <gtest/gtest.h>

#include "UrlUtils.h"

using UrlUtils::buildUrl;

namespace {
constexpr const char* kFeed = "https://example.org/catalog/opds/index.xml";
}

TEST(UrlUtilsBuildUrl, KeepsAbsoluteUrls) {
  EXPECT_EQ(buildUrl(kFeed, "https://cdn.example.net/a.epub"), "https://cdn.example.net/a.epub");
  EXPECT_EQ(buildUrl(kFeed, "http://other.test/x?y=1"), "http://other.test/x?y=1");
}

TEST(UrlUtilsBuildUrl, SchemeRelativeTakesBaseScheme) {
  EXPECT_EQ(buildUrl(kFeed, "//mirror.example.net/books/a.epub"), "https://mirror.example.net/books/a.epub");
  EXPECT_EQ(buildUrl("http://example.org/opds", "//cdn.test/a"), "http://cdn.test/a");
}

TEST(UrlUtilsBuildUrl, RootRelativeUsesHostRoot) {
  EXPECT_EQ(buildUrl(kFeed, "/books/a.epub"), "https://example.org/books/a.epub");
  EXPECT_EQ(buildUrl("https://example.org:8080/opds?page=2", "/opds/new"), "https://example.org:8080/opds/new");
}

TEST(UrlUtilsBuildUrl, PathRelativeResolvesAgainstBaseDirectory) {
  // The reported bug: the file name of the feed must not become a directory.
  EXPECT_EQ(buildUrl(kFeed, "books/a.epub"), "https://example.org/catalog/opds/books/a.epub");
  EXPECT_EQ(buildUrl(kFeed, "./page-2.xml"), "https://example.org/catalog/opds/page-2.xml");
  EXPECT_EQ(buildUrl(kFeed, "../books/a.epub"), "https://example.org/catalog/books/a.epub");
  EXPECT_EQ(buildUrl(kFeed, "../../../../a.epub"), "https://example.org/a.epub");
  EXPECT_EQ(buildUrl("https://example.org/opds/", "new"), "https://example.org/opds/new");
  EXPECT_EQ(buildUrl("https://example.org", "opds"), "https://example.org/opds");
  EXPECT_EQ(buildUrl("https://example.org/a/b/c", ".."), "https://example.org/a/");
  EXPECT_EQ(buildUrl("https://example.org/a/b/c", "."), "https://example.org/a/b/");
}

TEST(UrlUtilsBuildUrl, KeepsQueryStrings) {
  EXPECT_EQ(buildUrl(kFeed, "search?q=Шевченко&page=2"),
            "https://example.org/catalog/opds/search?q=%D0%A8%D0%B5%D0%B2%D1%87%D0%B5%D0%BD%D0%BA%D0%BE&page=2");
  EXPECT_EQ(buildUrl("https://example.org/opds?page=1", "?page=2"), "https://example.org/opds?page=2");
  // The base's own query never leaks into a resolved path.
  EXPECT_EQ(buildUrl("https://example.org/opds/root.xml?auth=1", "books/a.epub"),
            "https://example.org/opds/books/a.epub");
  EXPECT_EQ(buildUrl(kFeed, "books/a.epub#part"), "https://example.org/catalog/opds/books/a.epub");
}

TEST(UrlUtilsBuildUrl, EmptyPathAndSchemeLessBase) {
  EXPECT_EQ(buildUrl(kFeed, ""), kFeed);
  EXPECT_EQ(buildUrl("example.org/opds/index.xml", "books/a.epub"), "http://example.org/opds/books/a.epub");
  EXPECT_EQ(buildUrl("example.org", "/opds"), "http://example.org/opds");
}
