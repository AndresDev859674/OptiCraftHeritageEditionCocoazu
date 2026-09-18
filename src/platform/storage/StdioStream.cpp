#include "platform/storage/StdioStream.h"

#include <climits>
#include <cstdio>
#include <streambuf>

namespace
{
class FileInputBuffer : public std::streambuf
{
public:
    explicit FileInputBuffer(FILE* file) : file_(file)
    {
        setg(buffer_, buffer_, buffer_);
    }

protected:
    int_type underflow() override
    {
        if (gptr() < egptr())
            return traits_type::to_int_type(*gptr());

        const std::size_t count = std::fread(buffer_, 1, sizeof(buffer_), file_);
        if (count == 0)
            return traits_type::eof();

        setg(buffer_, buffer_, buffer_ + count);
        return traits_type::to_int_type(*gptr());
    }

    pos_type seekoff(off_type offset, std::ios_base::seekdir direction, std::ios_base::openmode mode) override
    {
        if ((mode & std::ios_base::in) == 0)
            return pos_type(off_type(-1));

        long baseOffset = 0;
        int origin = SEEK_SET;
        if (direction == std::ios_base::cur)
        {
            const off_type unread = static_cast<off_type>(egptr() - gptr());
            offset -= unread;
            origin = SEEK_CUR;
        }
        else if (direction == std::ios_base::end)
        {
            origin = SEEK_END;
        }
        else if (direction != std::ios_base::beg)
        {
            return pos_type(off_type(-1));
        }

        if (offset < static_cast<off_type>(LONG_MIN) || offset > static_cast<off_type>(LONG_MAX))
            return pos_type(off_type(-1));
        baseOffset = static_cast<long>(offset);

        if (std::fseek(file_, baseOffset, origin) != 0)
            return pos_type(off_type(-1));
        const long position = std::ftell(file_);
        if (position < 0)
            return pos_type(off_type(-1));

        setg(buffer_, buffer_, buffer_);
        return pos_type(static_cast<off_type>(position));
    }

    pos_type seekpos(pos_type position, std::ios_base::openmode mode) override
    {
        return seekoff(static_cast<off_type>(position), std::ios_base::beg, mode);
    }

private:
    FILE* file_;
    char buffer_[4096];
};

class FileInputStream : public std::istream
{
public:
    explicit FileInputStream(FILE* file) : std::istream(nullptr), file_(file), buffer_(file)
    {
        rdbuf(&buffer_);
    }

    ~FileInputStream() override
    {
        std::fclose(file_);
    }

private:
    FILE* file_;
    FileInputBuffer buffer_;
};

class FileOutputBuffer : public std::streambuf
{
public:
    explicit FileOutputBuffer(FILE* file) : file_(file) {}

protected:
    std::streamsize xsputn(const char* data, std::streamsize length) override
    {
        if (length <= 0)
            return 0;
        return static_cast<std::streamsize>(std::fwrite(data, 1, static_cast<std::size_t>(length), file_));
    }

    int_type overflow(int_type ch) override
    {
        if (traits_type::eq_int_type(ch, traits_type::eof()))
            return traits_type::not_eof(ch);
        return std::fputc(traits_type::to_char_type(ch), file_) == EOF ? traits_type::eof() : ch;
    }

    int sync() override
    {
        return std::fflush(file_) == 0 ? 0 : -1;
    }

    pos_type seekoff(off_type offset, std::ios_base::seekdir direction, std::ios_base::openmode mode) override
    {
        if ((mode & std::ios_base::out) == 0)
            return pos_type(off_type(-1));

        int origin = SEEK_SET;
        if (direction == std::ios_base::cur)
            origin = SEEK_CUR;
        else if (direction == std::ios_base::end)
            origin = SEEK_END;
        else if (direction != std::ios_base::beg)
            return pos_type(off_type(-1));

        if (offset < static_cast<off_type>(LONG_MIN) || offset > static_cast<off_type>(LONG_MAX))
            return pos_type(off_type(-1));
        if (std::fseek(file_, static_cast<long>(offset), origin) != 0)
            return pos_type(off_type(-1));
        const long position = std::ftell(file_);
        return position >= 0 ? pos_type(static_cast<off_type>(position)) : pos_type(off_type(-1));
    }

    pos_type seekpos(pos_type position, std::ios_base::openmode mode) override
    {
        return seekoff(static_cast<off_type>(position), std::ios_base::beg, mode);
    }

private:
    FILE* file_;
};

class FileOutputStream : public std::ostream
{
public:
    explicit FileOutputStream(FILE* file) : std::ostream(nullptr), file_(file), buffer_(file)
    {
        rdbuf(&buffer_);
    }

    ~FileOutputStream() override
    {
        flush();
        std::fclose(file_);
    }

private:
    FILE* file_;
    FileOutputBuffer buffer_;
};

class DiscardBuffer : public std::streambuf
{
protected:
    std::streamsize xsputn(const char*, std::streamsize length) override
    {
        return length;
    }

    int_type overflow(int_type ch) override
    {
        return traits_type::eq_int_type(ch, traits_type::eof()) ? traits_type::not_eof(ch) : ch;
    }
};

class DiscardOutputStream : public std::ostream
{
public:
    DiscardOutputStream() : std::ostream(nullptr)
    {
        rdbuf(&buffer_);
    }

private:
    DiscardBuffer buffer_;
};
}

namespace PlatformStorage
{
std::unique_ptr<std::istream> openStdioInputStream(const std::string& path)
{
    FILE* file = std::fopen(path.c_str(), "rb");
    if (file == nullptr)
        return nullptr;
    return std::unique_ptr<std::istream>(new FileInputStream(file));
}

std::unique_ptr<std::ostream> openStdioOutputStream(const std::string& path, bool append)
{
    FILE* file = std::fopen(path.c_str(), append ? "ab" : "wb");
    if (file == nullptr)
        return nullptr;
    return std::unique_ptr<std::ostream>(new FileOutputStream(file));
}

std::unique_ptr<std::ostream> openDiscardOutputStream()
{
    return std::unique_ptr<std::ostream>(new DiscardOutputStream());
}
}
