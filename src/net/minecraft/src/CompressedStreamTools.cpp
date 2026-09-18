#include "CompressedStreamTools.h"

#include "NBTBase.h"
#include "NBTTagCompound.h"
#include <array>
#include <memory>
#include <sstream>
#include <streambuf>
#include <vector>
#include <stdexcept>
#include <string>
#include <zlib.h>

static std::vector<char> gzipDecompress(const std::vector<char> &compressed)
{
    // Compatibility path for callers that explicitly need a contiguous buffer.
    // Save/map NBT on PS2 no longer comes through here: readGzippedCompound()
    // parses directly from a streaming inflater below.
    const size_t maxOut = 1024 * 1024;
    std::vector<char> out;
    out.resize(64 * 1024);

    z_stream zs{};
    if (inflateInit2(&zs, 16 + MAX_WBITS) != Z_OK) // gzip mode
        throw std::runtime_error("gzip decompress failed");
    zs.next_in  = (Bytef*)compressed.data();
    zs.avail_in = (uInt)compressed.size();

    for (;;)
    {
        zs.next_out  = (Bytef*)(out.data() + zs.total_out);
        zs.avail_out = (uInt)(out.size() - (size_t)zs.total_out);
        int ret = inflate(&zs, Z_NO_FLUSH);
        if (ret == Z_STREAM_END)
            break;
        const bool outFull = (ret == Z_OK || ret == Z_BUF_ERROR) && zs.avail_out == 0;
        if (!outFull || out.size() >= maxOut)
        {
            inflateEnd(&zs);
            throw std::runtime_error("gzip decompress failed");
        }
        size_t next = out.size() * 2;
        if (next > maxOut)
            next = maxOut;
        out.resize(next);
    }

    uLong actual = zs.total_out;
    inflateEnd(&zs);
    out.resize(actual);
    return out;
}

#ifdef PS2_PLATFORM
namespace
{
class GzipInflateStreamBuf : public std::streambuf
{
public:
    explicit GzipInflateStreamBuf(std::istream &source)
        : source(source), initialized(false), finished(false), sourceExhausted(false)
    {
        const int ret = inflateInit2(&zs, 16 + MAX_WBITS);
        if (ret != Z_OK)
            throw std::runtime_error("gzip inflate init failed");
        initialized = true;
        setg(output.data(), output.data(), output.data());
    }

    ~GzipInflateStreamBuf() override
    {
        if (initialized)
            inflateEnd(&zs);
    }

    void finishAndVerify()
    {
        while (!finished)
        {
            if (gptr() < egptr())
                gbump(static_cast<int>(egptr() - gptr()));
            if (gptr() == egptr() && traits_type::eq_int_type(underflow(), traits_type::eof()))
                break;
        }
        if (!finished)
            throw std::runtime_error("gzip stream did not reach its checksum trailer");
    }

protected:
    int_type underflow() override
    {
        if (gptr() < egptr())
            return traits_type::to_int_type(*gptr());
        if (finished)
            return traits_type::eof();

        zs.next_out = reinterpret_cast<Bytef*>(output.data());
        zs.avail_out = static_cast<uInt>(output.size());

        while (zs.avail_out > 0 && !finished)
        {
            if (zs.avail_in == 0 && !sourceExhausted)
            {
                source.read(input.data(), static_cast<std::streamsize>(input.size()));
                const std::streamsize got = source.gcount();
                if (got > 0)
                {
                    zs.next_in = reinterpret_cast<Bytef*>(input.data());
                    zs.avail_in = static_cast<uInt>(got);
                    sourceExhausted = source.eof();
                }
                else
                {
                    if (source.bad())
                        throw std::runtime_error("gzip source read failed");
                    sourceExhausted = true;
                }
            }

            if (zs.avail_in == 0 && sourceExhausted)
                throw std::runtime_error("truncated gzip stream");

            const uInt beforeIn = zs.avail_in;
            const uInt beforeOut = zs.avail_out;
            const int ret = inflate(&zs, Z_NO_FLUSH);
            if (ret == Z_STREAM_END)
            {
                finished = true;
                break;
            }
            if (ret != Z_OK)
            {
                const std::string detail = zs.msg != nullptr ? zs.msg : "unknown zlib error";
                throw std::runtime_error("gzip inflate failed: " + detail);
            }
            if (beforeIn == zs.avail_in && beforeOut == zs.avail_out)
                throw std::runtime_error("gzip inflate made no progress");
        }

        const std::size_t produced = output.size() - static_cast<std::size_t>(zs.avail_out);
        if (produced == 0)
            return traits_type::eof();

        setg(output.data(), output.data(), output.data() + produced);
        return traits_type::to_int_type(*gptr());
    }

private:
    std::istream &source;
    z_stream zs{};
    bool initialized;
    bool finished;
    bool sourceExhausted;
    std::array<char, 16 * 1024> input{};
    std::array<char, 16 * 1024> output{};
};
}
#endif

static std::vector<char> gzipCompress(const std::vector<char> &raw)
{
    uLongf compLen = compressBound((uLong)raw.size()) + 64;
    std::vector<char> out(compLen);

    z_stream zs{};
    deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 16 + MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
    zs.next_in  = (Bytef*)raw.data();
    zs.avail_in = (uInt)raw.size();
    zs.next_out  = (Bytef*)out.data();
    zs.avail_out = (uInt)out.size();

    deflate(&zs, Z_FINISH);
    uLong actual = zs.total_out;
    deflateEnd(&zs);

    out.resize(actual);
    return out;
}

NBTTagCompound* CompressedStreamTools::readGzippedCompound(std::istream &is)
{
#ifdef PS2_PLATFORM
    // Parse directly from zlib output. This removes the old 256 KiB cap and,
    // more importantly on a 32 MiB PS2, avoids keeping a second full raw NBT
    // buffer (plus the std::string copy) alive while the compound is built.
    GzipInflateStreamBuf inflateBuffer(is);
    std::istream inflatedStream(&inflateBuffer);
    inflatedStream.exceptions(std::ios::badbit);
    std::unique_ptr<NBTTagCompound> compound(readCompound(inflatedStream));
    inflateBuffer.finishAndVerify();
    return compound.release();
#else
    std::vector<char> compressed((std::istreambuf_iterator<char>(is)), {});
    std::vector<char> raw = gzipDecompress(compressed);

    std::string s(raw.begin(), raw.end());
    std::istringstream rawStream(s, std::ios::binary);
    return readCompound(rawStream);
#endif
}

NBTTagCompound* CompressedStreamTools::readCompressed(std::istream &is)
{
    return readGzippedCompound(is);
}

void CompressedStreamTools::writeGzippedCompoundToOutputStream(NBTTagCompound *compound, std::ostream &os)
{
    const std::vector<char> compressed = compress(compound);
    os.write(compressed.data(), (std::streamsize)compressed.size());
}

std::vector<char> CompressedStreamTools::compress(NBTTagCompound *compound)
{
    if (compound == nullptr)
        return {};

    std::ostringstream rawStream(std::ios::binary);
    writeCompound(compound, rawStream);
    const std::string raw = rawStream.str();
    return gzipCompress(std::vector<char>(raw.begin(), raw.end()));
}

NBTTagCompound* CompressedStreamTools::decompress(const std::vector<char> &compressed)
{
    const std::vector<char> raw = gzipDecompress(compressed);
    std::string data(raw.begin(), raw.end());
    std::istringstream rawStream(data, std::ios::binary);
    return readCompound(rawStream);
}

NBTTagCompound* CompressedStreamTools::readCompound(std::istream &is)
{
    NBTBase *tag = NBTBase::readTag(is);
    NBTTagCompound *compound = dynamic_cast<NBTTagCompound*>(tag);
    if (compound == nullptr)
    {
        delete tag;
        throw std::runtime_error("Root tag must be a named compound tag");
    }
    return compound;
}

void CompressedStreamTools::writeCompound(NBTTagCompound *compound, std::ostream &os)
{
    NBTBase::writeTag(compound, os);
}
