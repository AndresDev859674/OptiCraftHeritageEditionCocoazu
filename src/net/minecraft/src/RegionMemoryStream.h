#pragma once

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <ios>
#include <utility>
#include <vector>

class RegionMemoryStream
{
public:
    RegionMemoryStream() = default;

    void replaceBuffer(std::vector<unsigned char> &&bytes)
    {
        bytes_ = std::move(bytes);
        getPos_ = 0;
        putPos_ = 0;
        good_ = true;
        lastRead_ = 0;
    }

    void clear()
    {
        good_ = true;
        lastRead_ = 0;
    }

    explicit operator bool() const
    {
        return good_;
    }

    std::streamsize gcount() const
    {
        return lastRead_;
    }

    std::streampos tellg() const
    {
        return good_ ? std::streampos(static_cast<std::streamoff>(getPos_)) : std::streampos(-1);
    }

    void seekg(std::streampos pos)
    {
        seekGet(static_cast<std::streamoff>(pos), std::ios::beg);
    }

    void seekg(std::streamoff off, std::ios::seekdir dir)
    {
        seekGet(off, dir);
    }

    void seekp(std::streampos pos)
    {
        seekPut(static_cast<std::streamoff>(pos), std::ios::beg);
    }

    void seekp(std::streamoff off, std::ios::seekdir dir)
    {
        seekPut(off, dir);
    }

    void read(char *dst, std::streamsize count)
    {
        lastRead_ = 0;
        if (!good_ || dst == nullptr || count < 0)
        {
            good_ = false;
            return;
        }

        const std::size_t requested = static_cast<std::size_t>(count);
        const std::size_t available = getPos_ < bytes_.size() ? bytes_.size() - getPos_ : 0;
        const std::size_t actual = std::min(requested, available);
        if (actual != 0)
            std::memcpy(dst, bytes_.data() + getPos_, actual);

        getPos_ += actual;
        lastRead_ = static_cast<std::streamsize>(actual);
        if (actual != requested)
            good_ = false;
    }

    void write(const char *src, std::streamsize count)
    {
        if (!good_ || count < 0 || (src == nullptr && count != 0))
        {
            good_ = false;
            return;
        }

        const std::size_t length = static_cast<std::size_t>(count);
        if (putPos_ > static_cast<std::size_t>(-1) - length)
        {
            good_ = false;
            return;
        }

        const std::size_t end = putPos_ + length;
        if (end > bytes_.size())
            bytes_.resize(end, 0);
        if (length != 0)
            std::memcpy(bytes_.data() + putPos_, src, length);
        putPos_ = end;
    }

    void flush() {}

    const unsigned char *data() const
    {
        return bytes_.empty() ? nullptr : bytes_.data();
    }

    std::size_t size() const
    {
        return bytes_.size();
    }

private:
    static bool resolvePosition(std::size_t current, std::size_t size,
                                std::streamoff off, std::ios::seekdir dir,
                                std::size_t &result)
    {
        std::streamoff base = 0;
        if (dir == std::ios::beg)
            base = 0;
        else if (dir == std::ios::cur)
            base = static_cast<std::streamoff>(current);
        else if (dir == std::ios::end)
            base = static_cast<std::streamoff>(size);
        else
            return false;

        const std::streamoff position = base + off;
        if (position < 0)
            return false;
        result = static_cast<std::size_t>(position);
        return true;
    }

    void seekGet(std::streamoff off, std::ios::seekdir dir)
    {
        std::size_t position = 0;
        if (!resolvePosition(getPos_, bytes_.size(), off, dir, position) || position > bytes_.size())
        {
            good_ = false;
            return;
        }
        getPos_ = position;
    }

    void seekPut(std::streamoff off, std::ios::seekdir dir)
    {
        std::size_t position = 0;
        if (!resolvePosition(putPos_, bytes_.size(), off, dir, position))
        {
            good_ = false;
            return;
        }
        putPos_ = position;
    }

    std::vector<unsigned char> bytes_;
    std::size_t getPos_ = 0;
    std::size_t putPos_ = 0;
    bool good_ = true;
    std::streamsize lastRead_ = 0;
};
