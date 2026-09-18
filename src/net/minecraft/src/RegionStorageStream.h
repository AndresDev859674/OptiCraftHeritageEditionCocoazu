#pragma once

#include <cstddef>
#include <ios>
#include <string>
#include <vector>

#include "RegionMemoryStream.h"

class RegionStorageStream
{
public:
    RegionStorageStream() = default;

    bool open(const std::string &path);
    bool materializeForWrite();

    void clear();
    explicit operator bool() const;
    std::streamsize gcount() const;
    std::streampos tellg() const;
    void seekg(std::streampos pos);
    void seekg(std::streamoff off, std::ios::seekdir dir);
    void seekp(std::streampos pos);
    void seekp(std::streamoff off, std::ios::seekdir dir);
    void read(char *dst, std::streamsize count);
    void write(const char *src, std::streamsize count);
    void flush();
    const unsigned char *data() const;
    std::size_t size() const;

private:
    bool materialize();
    static bool resolvePosition(std::size_t current, std::size_t size,
                                std::streamoff off, std::ios::seekdir dir,
                                std::size_t &result);
    void seekGet(std::streamoff off, std::ios::seekdir dir);
    void seekPut(std::streamoff off, std::ios::seekdir dir);

    RegionMemoryStream memory_;
    std::vector<unsigned char> header_;
    std::string filePath_;
    std::size_t storageSize_ = 0;
    std::size_t getPos_ = 0;
    std::size_t putPos_ = 0;
    bool good_ = true;
    bool materialized_ = false;
    bool exists_ = false;
    std::streamsize lastRead_ = 0;
};
