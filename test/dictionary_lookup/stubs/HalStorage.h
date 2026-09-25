#pragma once

// Host HalStorage for Dictionary tests: plain POSIX files, plus counters that
// model the device's SD cost (file opens, seeks, 512-byte sectors fetched).

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

struct HalStorageStats {
  uint64_t opens = 0;
  uint64_t seeks = 0;
  uint64_t bytesRead = 0;
  uint64_t sectorLoads = 0;  // sector changes through one shared cache, like SdFat's volume cache
  void reset() { *this = HalStorageStats{}; }
};

inline HalStorageStats halStorageStats;

class HalFile {
 public:
  HalFile() = default;
  HalFile(const HalFile&) = delete;
  HalFile& operator=(const HalFile&) = delete;
  ~HalFile() { close(); }

  bool openRaw(const char* path, const char* mode) {
    close();
    fp_ = std::fopen(path, mode);
    if (!fp_) return false;
    std::fseek(fp_, 0, SEEK_END);
    size_ = static_cast<size_t>(std::ftell(fp_));
    std::fseek(fp_, 0, SEEK_SET);
    pos_ = 0;
    id_ = ++nextId();
    halStorageStats.opens++;
    return true;
  }

  size_t fileSize() const { return size_; }
  bool seekSet(const size_t offset) {
    if (!fp_ || offset > size_) return false;
    halStorageStats.seeks++;
    pos_ = offset;
    return std::fseek(fp_, static_cast<long>(offset), SEEK_SET) == 0;
  }
  int available() const { return fp_ && pos_ < size_ ? static_cast<int>(size_ - pos_) : 0; }
  size_t position() const { return pos_; }

  int read(void* buf, const size_t count) {
    if (!fp_) return -1;
    touch(count);
    const size_t n = std::fread(buf, 1, count, fp_);
    pos_ += n;
    halStorageStats.bytesRead += n;
    return static_cast<int>(n);
  }
  int read() {
    if (!fp_ || pos_ >= size_) return -1;
    touch(1);
    const int c = std::fgetc(fp_);
    if (c < 0) return -1;
    pos_++;
    halStorageStats.bytesRead++;
    return c;
  }
  size_t write(const void* buf, const size_t count) {
    if (!fp_) return 0;
    const size_t n = std::fwrite(buf, 1, count, fp_);
    pos_ += n;
    if (pos_ > size_) size_ = pos_;
    return n;
  }
  size_t write(const uint8_t* buf, const size_t count) { return write(static_cast<const void*>(buf), count); }
  size_t write(const uint8_t b) { return write(&b, 1); }

  bool close() {
    if (fp_) std::fclose(fp_);
    fp_ = nullptr;
    return true;
  }
  bool isOpen() const { return fp_ != nullptr; }
  explicit operator bool() const { return isOpen(); }

 private:
  static uint64_t& nextId() {
    static uint64_t id = 0;
    return id;
  }
  static uint64_t& cachedKey() {
    static uint64_t key = UINT64_MAX;
    return key;
  }
  void touch(const size_t count) {
    const size_t last = pos_ + (count ? count - 1 : 0);
    for (size_t sector = pos_ / 512; sector <= last / 512; sector++) {
      const uint64_t key = (id_ << 40) | sector;
      if (key != cachedKey()) {
        cachedKey() = key;
        halStorageStats.sectorLoads++;
      }
    }
  }

  std::FILE* fp_ = nullptr;
  size_t size_ = 0;
  size_t pos_ = 0;
  uint64_t id_ = 0;
};

class HalStorage {
 public:
  bool exists(const char* path) {
    std::FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    std::fclose(f);
    return true;
  }
  bool remove(const char* path) { return std::remove(path) == 0; }
  bool rename(const char* from, const char* to) { return std::rename(from, to) == 0; }
  bool openFileForRead(const char*, const char* path, HalFile& file) { return file.openRaw(path, "rb"); }
  bool openFileForRead(const char* tag, const std::string& path, HalFile& file) {
    return openFileForRead(tag, path.c_str(), file);
  }
  bool openFileForWrite(const char*, const char* path, HalFile& file) { return file.openRaw(path, "w+b"); }
  bool openFileForWrite(const char* tag, const std::string& path, HalFile& file) {
    return openFileForWrite(tag, path.c_str(), file);
  }
};

inline HalStorage Storage;
