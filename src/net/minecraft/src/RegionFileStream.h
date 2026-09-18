#pragma once

#include <fstream>
#include <ios>
#include <string>

#include "RegionStorageStream.h"
#include "platform/storage/AssetPak.h"

// RegionFile's handle on platforms that keep region files open as real
// streams (PC, Wii). A path on disk is a std::fstream exactly as before; a
// "pak://" path -- a region of the tutorial world shipped inside assets.pak --
// is read through RegionStorageStream, which keeps only the header resident
// and fetches sector ranges on demand from the pak. Pak regions are read-only:
// opening one for writing fails, and RegionFile never asks for that on a
// read-only save. Only the subset of the std::fstream interface RegionFile
// uses is forwarded.
class RegionFileStream
{
public:
    void open(const std::string &path, std::ios::openmode mode)
    {
        close();
        if (AssetPak::isPakPath(path))
        {
            if ((mode & std::ios::out) != 0)
                return;
            pakOpen_ = pak_.open(path);
            return;
        }
        file_.open(path, mode);
    }

    bool is_open() const { return pakOpen_ || file_.is_open(); }

    void close()
    {
        if (file_.is_open())
            file_.close();
        pakOpen_ = false;
    }

    void clear()
    {
        if (pakOpen_) pak_.clear(); else file_.clear();
    }

    explicit operator bool() const
    {
        return pakOpen_ ? static_cast<bool>(pak_) : static_cast<bool>(file_);
    }

    bool operator!() const { return !static_cast<bool>(*this); }

    std::streamsize gcount() const { return pakOpen_ ? pak_.gcount() : file_.gcount(); }

    std::streampos tellg() { return pakOpen_ ? pak_.tellg() : file_.tellg(); }

    void seekg(std::streampos pos)
    {
        if (pakOpen_) pak_.seekg(pos); else file_.seekg(pos);
    }

    void seekg(std::streamoff off, std::ios::seekdir dir)
    {
        if (pakOpen_) pak_.seekg(off, dir); else file_.seekg(off, dir);
    }

    void seekp(std::streampos pos)
    {
        if (pakOpen_) pak_.seekp(pos); else file_.seekp(pos);
    }

    void seekp(std::streamoff off, std::ios::seekdir dir)
    {
        if (pakOpen_) pak_.seekp(off, dir); else file_.seekp(off, dir);
    }

    void read(char *dst, std::streamsize count)
    {
        if (pakOpen_) pak_.read(dst, count); else file_.read(dst, count);
    }

    void write(const char *src, std::streamsize count)
    {
        if (pakOpen_) pak_.write(src, count); else file_.write(src, count);
    }

    void flush()
    {
        if (pakOpen_) pak_.flush(); else file_.flush();
    }

private:
    std::fstream file_;
    RegionStorageStream pak_;
    bool pakOpen_ = false;
};
