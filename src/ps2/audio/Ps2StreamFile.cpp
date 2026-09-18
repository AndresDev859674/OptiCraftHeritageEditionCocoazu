#ifdef PS2_PLATFORM

#include "ps2/audio/Ps2StreamFile.h"

#include "platform/storage/AssetPak.h"

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>

Ps2StreamFile::Ps2StreamFile()
    : fd_(-1), pos_(0), len_(0), failed_(false), base_(0), size_(0), filePos_(0)
{
}

Ps2StreamFile::~Ps2StreamFile()
{
    close();
}

bool Ps2StreamFile::open(const char *path)
{
    close();
    if (path == nullptr)
        return false;

    const std::string spelled(path);
    if (AssetPak::isPakPath(spelled))
    {
        std::uint32_t dataOffset = 0;
        std::uint32_t entrySize = 0;
        if (!AssetPak::locate(AssetPak::keyOf(spelled), &dataOffset, &entrySize))
            return false;
        fd_ = ::open(AssetPak::archivePath().c_str(), O_RDONLY);
        if (fd_ < 0)
            return false;
        base_ = static_cast<long>(dataOffset);
        size_ = static_cast<long>(entrySize);
    }
    else
    {
        fd_ = ::open(path, O_RDONLY);
        if (fd_ < 0)
            return false;
        base_ = 0;
        const off_t end = ::lseek(fd_, 0, SEEK_END);
        size_ = end >= 0 ? static_cast<long>(end) : 0;
    }
    return seek(0);
}

void Ps2StreamFile::close()
{
    if (fd_ >= 0)
        ::close(fd_);
    fd_ = -1;
    pos_ = 0;
    len_ = 0;
    failed_ = false;
    base_ = 0;
    size_ = 0;
    filePos_ = 0;
}

long Ps2StreamFile::size() const
{
    return fd_ >= 0 ? size_ : -1L;
}

bool Ps2StreamFile::isOpen() const
{
    return fd_ >= 0;
}

bool Ps2StreamFile::seek(long offset)
{
    if (fd_ < 0 || offset < 0 || offset > size_)
        return false;
    pos_ = 0;
    len_ = 0;
    const off_t target = static_cast<off_t>(base_ + offset);
    if (::lseek(fd_, target, SEEK_SET) != target)
        return false;
    filePos_ = offset;
    return true;
}

int Ps2StreamFile::fill()
{
    if (fd_ < 0 || failed_)
        return -1;
    if (pos_ < len_)
        return len_ - pos_;

    pos_ = 0;
    len_ = 0;
    const long remaining = size_ - filePos_;
    if (remaining <= 0)
        return 0;
    const int want = static_cast<int>(std::min<long>(remaining, kBufferBytes));
    const int got = static_cast<int>(::read(fd_, buffer_, static_cast<std::size_t>(want)));
    if (got < 0)
    {
        failed_ = true;
        return -1;
    }
    len_ = got;
    filePos_ += got;
    return got;
}

const unsigned char *Ps2StreamFile::data() const
{
    return buffer_ + pos_;
}

void Ps2StreamFile::consume(int bytes)
{
    pos_ = std::min(len_, pos_ + std::max(0, bytes));
}

bool Ps2StreamFile::readExact(void *dst, int bytes)
{
    unsigned char *out = static_cast<unsigned char *>(dst);
    int remaining = bytes;
    while (remaining > 0)
    {
        const int available = fill();
        if (available <= 0)
            return false;
        const int piece = std::min(remaining, available);
        std::memcpy(out, data(), static_cast<std::size_t>(piece));
        consume(piece);
        out += piece;
        remaining -= piece;
    }
    return true;
}

#endif // PS2_PLATFORM
