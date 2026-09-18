#include "platform/storage/PakFile.h"

#if PLATFORM_PS2
#include <fcntl.h>
#include <unistd.h>
#endif

#if PLATFORM_PS2

PakFile::PakFile()
    : fd_(-1)
{
}

PakFile::~PakFile()
{
    close();
}

bool PakFile::open(const std::string &path)
{
    close();
    fd_ = ::open(path.c_str(), O_RDONLY);
    return fd_ >= 0;
}

void PakFile::close()
{
    if (fd_ >= 0)
        ::close(fd_);
    fd_ = -1;
}

bool PakFile::isOpen() const
{
    return fd_ >= 0;
}

bool PakFile::readAt(std::uint32_t offset, void *dst, std::uint32_t length)
{
    if (fd_ < 0 || dst == nullptr)
        return false;
    const off_t target = static_cast<off_t>(offset);
    if (::lseek(fd_, target, SEEK_SET) != target)
        return false;
    // The IOP side may answer a large request in pieces; loop until the
    // whole range is in, and treat zero as the end of the file.
    unsigned char *out = static_cast<unsigned char *>(dst);
    std::uint32_t remaining = length;
    while (remaining > 0)
    {
        const int got = static_cast<int>(::read(fd_, out, remaining));
        if (got <= 0)
            return false;
        out += got;
        remaining -= static_cast<std::uint32_t>(got);
    }
    return true;
}

#else

PakFile::PakFile()
    : file_(nullptr)
{
}

PakFile::~PakFile()
{
    close();
}

bool PakFile::open(const std::string &path)
{
    close();
    file_ = std::fopen(path.c_str(), "rb");
    return file_ != nullptr;
}

void PakFile::close()
{
    if (file_ != nullptr)
        std::fclose(file_);
    file_ = nullptr;
}

bool PakFile::isOpen() const
{
    return file_ != nullptr;
}

bool PakFile::readAt(std::uint32_t offset, void *dst, std::uint32_t length)
{
    if (file_ == nullptr || dst == nullptr)
        return false;
    if (std::fseek(file_, static_cast<long>(offset), SEEK_SET) != 0)
        return false;
    return std::fread(dst, 1, length, file_) == length;
}

#endif
