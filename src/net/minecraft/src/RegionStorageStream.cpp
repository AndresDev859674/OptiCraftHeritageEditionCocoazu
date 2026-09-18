#include "RegionStorageStream.h"
#include "platform/PlatformConfig.h"

// Also the pak-backed half of RegionFileStream on the stream platforms.
#if PLATFORM_REGION_RANDOM_ACCESS || !PLATFORM_REGION_WHOLE_FILE_BUFFER

#include "platform/Storage.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <utility>

bool RegionStorageStream::open(const std::string &path)
{
    filePath_ = path;
    header_.clear();
    getPos_ = 0;
    putPos_ = 0;
    lastRead_ = 0;
    good_ = true;
    materialized_ = false;
    exists_ = false;
    storageSize_ = 0;

    const std::int64_t fileSize = PlatformStorage::getFileSize(path);
    if (fileSize >= 0 && static_cast<std::uint64_t>(fileSize) <=
                         static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
    {
        exists_ = true;
        storageSize_ = static_cast<std::size_t>(fileSize);
        const std::size_t headerSize = std::min<std::size_t>(storageSize_, 8192u);
        header_.resize(headerSize);
        if (headerSize == 0 || PlatformStorage::readFileRange(path, 0, header_.data(), headerSize))
            return true;
    }

    std::vector<unsigned char> existing;
    if (PlatformStorage::readFile(path, existing))
    {
        exists_ = true;
        storageSize_ = existing.size();
        memory_.replaceBuffer(std::move(existing));
        materialized_ = true;
        header_.clear();
        return true;
    }

    memory_.replaceBuffer(std::vector<unsigned char>());
    materialized_ = true;
    storageSize_ = 0;
    header_.clear();
    return false;
}

bool RegionStorageStream::materializeForWrite()
{
    if (materialized_)
        return good_;
    if (!materialize())
    {
        good_ = false;
        return false;
    }
    return true;
}

void RegionStorageStream::clear()
{
    good_ = true;
    lastRead_ = 0;
    if (materialized_)
        memory_.clear();
}

RegionStorageStream::operator bool() const
{
    return good_;
}

std::streamsize RegionStorageStream::gcount() const
{
    return lastRead_;
}

std::streampos RegionStorageStream::tellg() const
{
    return good_ ? std::streampos(static_cast<std::streamoff>(getPos_)) : std::streampos(-1);
}

void RegionStorageStream::seekg(std::streampos pos)
{
    seekGet(static_cast<std::streamoff>(pos), std::ios::beg);
}

void RegionStorageStream::seekg(std::streamoff off, std::ios::seekdir dir)
{
    seekGet(off, dir);
}

void RegionStorageStream::seekp(std::streampos pos)
{
    seekPut(static_cast<std::streamoff>(pos), std::ios::beg);
}

void RegionStorageStream::seekp(std::streamoff off, std::ios::seekdir dir)
{
    seekPut(off, dir);
}

void RegionStorageStream::read(char *dst, std::streamsize count)
{
    lastRead_ = 0;
    if (!good_ || dst == nullptr || count < 0)
    {
        good_ = false;
        return;
    }

    const std::size_t requested = static_cast<std::size_t>(count);
    const std::size_t totalSize = size();
    const std::size_t available = getPos_ < totalSize ? totalSize - getPos_ : 0;
    const std::size_t actual = std::min(requested, available);

    if (actual != 0)
    {
        if (materialized_)
        {
            memory_.clear();
            memory_.seekg(static_cast<std::streamoff>(getPos_), std::ios::beg);
            memory_.read(dst, static_cast<std::streamsize>(actual));
            if (!memory_ || memory_.gcount() != static_cast<std::streamsize>(actual))
            {
                good_ = false;
                return;
            }
        }
        else if (getPos_ <= header_.size() && actual <= header_.size() - getPos_)
        {
            std::memcpy(dst, header_.data() + getPos_, actual);
        }
        else if (!PlatformStorage::readFileRange(filePath_, getPos_, dst, actual))
        {
            if (!materialize())
            {
                good_ = false;
                return;
            }
            memory_.clear();
            memory_.seekg(static_cast<std::streamoff>(getPos_), std::ios::beg);
            memory_.read(dst, static_cast<std::streamsize>(actual));
            if (!memory_ || memory_.gcount() != static_cast<std::streamsize>(actual))
            {
                good_ = false;
                return;
            }
        }
    }

    getPos_ += actual;
    lastRead_ = static_cast<std::streamsize>(actual);
    if (actual != requested)
        good_ = false;
}

void RegionStorageStream::write(const char *src, std::streamsize count)
{
    if (!good_ || count < 0 || (src == nullptr && count != 0))
    {
        good_ = false;
        return;
    }
    if (!materializeForWrite())
        return;

    const std::size_t length = static_cast<std::size_t>(count);
    if (putPos_ > std::numeric_limits<std::size_t>::max() - length)
    {
        good_ = false;
        return;
    }

    memory_.clear();
    memory_.seekp(static_cast<std::streamoff>(putPos_), std::ios::beg);
    memory_.write(src, count);
    if (!memory_)
    {
        good_ = false;
        return;
    }

    putPos_ += length;
    storageSize_ = memory_.size();
}

void RegionStorageStream::flush()
{
    if (materialized_)
        memory_.flush();
}

const unsigned char *RegionStorageStream::data() const
{
    return materialized_ ? memory_.data() : nullptr;
}

std::size_t RegionStorageStream::size() const
{
    return materialized_ ? memory_.size() : storageSize_;
}

bool RegionStorageStream::materialize()
{
    if (materialized_)
        return true;
    if (!exists_)
        return false;

    std::vector<unsigned char> existing;
    if (!PlatformStorage::readFile(filePath_, existing) || existing.size() != storageSize_)
        return false;

    memory_.replaceBuffer(std::move(existing));
    materialized_ = true;
    header_.clear();
    return true;
}

bool RegionStorageStream::resolvePosition(std::size_t current, std::size_t streamSize,
                                          std::streamoff off, std::ios::seekdir dir,
                                          std::size_t &result)
{
    std::streamoff base = 0;
    if (dir == std::ios::beg)
        base = 0;
    else if (dir == std::ios::cur)
        base = static_cast<std::streamoff>(current);
    else if (dir == std::ios::end)
        base = static_cast<std::streamoff>(streamSize);
    else
        return false;

    const std::streamoff position = base + off;
    if (position < 0)
        return false;
    result = static_cast<std::size_t>(position);
    return true;
}

void RegionStorageStream::seekGet(std::streamoff off, std::ios::seekdir dir)
{
    std::size_t position = 0;
    if (!resolvePosition(getPos_, size(), off, dir, position) || position > size())
    {
        good_ = false;
        return;
    }
    getPos_ = position;
}

void RegionStorageStream::seekPut(std::streamoff off, std::ios::seekdir dir)
{
    std::size_t position = 0;
    if (!resolvePosition(putPos_, size(), off, dir, position))
    {
        good_ = false;
        return;
    }
    putPos_ = position;
}

#endif // PLATFORM_REGION_RANDOM_ACCESS || !PLATFORM_REGION_WHOLE_FILE_BUFFER
