#include "FileSystem.hpp"

#include <algorithm>
#include <cstring>

namespace peanutbutter {

bool FileSystemV2::ReadFile(const std::string& pPath,
                            ByteBufferV2& pContents) const {
  pContents.Clear();

  std::unique_ptr<FileReadStreamV2> aRead = OpenReadStream(pPath);
  if (aRead == nullptr || !aRead->IsReady()) {
    return false;
  }

  const std::size_t aLength = aRead->GetLength();
  if (!pContents.Resize(aLength)) {
    return false;
  }
  if (aLength == 0u) {
    return true;
  }

  return aRead->Read(0u, pContents.Data(), aLength);
}

bool FileSystemV2::ReadTextFile(const std::string& pPath,
                                std::string& pContents) const {
  ByteBufferV2 aBuffer;
  if (!ReadFile(pPath, aBuffer)) {
    return false;
  }

  pContents.assign(reinterpret_cast<const char*>(aBuffer.Data()), aBuffer.Size());
  return true;
}

bool FileSystemV2::WriteFile(const std::string& pPath,
                             const ByteBufferV2& pContents) {
  return WriteFile(pPath, pContents.Data(), pContents.Size());
}

bool FileSystemV2::WriteFile(const std::string& pPath,
                             const unsigned char* pContents,
                             std::size_t pLength) {
  std::unique_ptr<FileWriteStreamV2> aWrite = OpenWriteStream(pPath);
  if (aWrite == nullptr || !aWrite->IsReady()) {
    return false;
  }

  bool aSucceeded = true;

  if (pLength > 0) {
    if (pContents == nullptr) {
      return false; // invalid input
    }
    aSucceeded = aWrite->Write(pContents, pLength);
  }

  if (aSucceeded) {
    aSucceeded = aWrite->Close();
  }

  return aSucceeded;
}

bool FileSystemV2::WriteTextFile(const std::string& pPath,
                                 const std::string& pContents) {
  return WriteFile(pPath,
                   reinterpret_cast<const unsigned char*>(pContents.data()),
                   pContents.size());
}

bool FileSystemV2::AppendTextFile(const std::string& pPath,
                                  const std::string& pContents) {
  return AppendFile(pPath,
                    reinterpret_cast<const unsigned char*>(pContents.data()),
                    pContents.size());
}

}  // namespace peanutbutter
