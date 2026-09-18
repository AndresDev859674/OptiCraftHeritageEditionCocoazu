#pragma once

// Sequential read-only file for the PS2 audio stream thread.
//
// Reads go through the libcglue POSIX layer (open/read/lseek) instead of stdio:
// stdio takes the process-wide newlib lock (Ps2LibcLocks) for the whole read,
// which on this platform blocks inside an IOP RPC. Holding that lock from the
// stream thread would stall every malloc/fopen on the main thread for the
// duration of a USB read, and waiting for it behind a main-thread read is what
// starves the audio ring buffer.
//
// A "pak://" path (AssetPak) opens the pak itself on a handle of its own and
// confines every offset to the entry's byte range, so the stream thread never
// shares a file position with the main thread's loader.
class Ps2StreamFile
{
public:
    static constexpr int kBufferBytes = 8192;

    Ps2StreamFile();
    ~Ps2StreamFile();
    Ps2StreamFile(const Ps2StreamFile &) = delete;
    Ps2StreamFile &operator=(const Ps2StreamFile &) = delete;

    bool open(const char *path);
    void close();
    bool isOpen() const;

    // Bytes in the file or pak entry.
    long size() const;

    // Absolute seek within the file/entry; discards buffered data.
    bool seek(long offset);

    // Makes sure the buffer holds data. Returns the number of buffered bytes,
    // 0 at end of file, negative on read error.
    int fill();
    const unsigned char *data() const;
    void consume(int bytes);

    // Reads exactly `bytes` into `dst`, refilling as needed.
    bool readExact(void *dst, int bytes);

private:
    int fd_;
    int pos_;
    int len_;
    bool failed_;
    long base_;      // byte offset of the entry inside the handle (0 for a loose file)
    long size_;      // bytes readable from base_
    long filePos_;   // next byte fill() reads, relative to base_
    // 64-byte alignment lets fioRead DMA straight into the buffer instead of
    // bouncing through its unaligned-copy path.
    alignas(64) unsigned char buffer_[kBufferBytes];
};
