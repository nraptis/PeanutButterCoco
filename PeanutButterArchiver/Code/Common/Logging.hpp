#pragma once

#include <cstdint>
#include <string>

namespace peanutbutter {

enum class LogLevelV2 {
  kInfo = 0,
  kWarning = 1,
  kError = 2,
};

struct LoggingStatV2 {
  std::uint64_t mArchivesCompleted = 0u;
  std::uint64_t mArchivesTotal = 0u;
  std::uint64_t mFilesCompleted = 0u;
  std::uint64_t mFilesTotal = 0u;
  std::uint64_t mFoldersCompleted = 0u;
  std::uint64_t mFoldersTotal = 0u;
  std::uint64_t mBytesCompleted = 0u;
  std::uint64_t mBytesTotal = 0u;
};

struct LogEntryV2 {
  LogLevelV2 mLevel = LogLevelV2::kInfo;
  std::string mMessage;
  LoggingStatV2 mStat{};
};

}  // namespace peanutbutter
