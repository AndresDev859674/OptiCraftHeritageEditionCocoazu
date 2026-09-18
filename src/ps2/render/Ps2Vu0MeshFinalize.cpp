#include "ps2/render/Ps2Vu0MeshFinalize.h"

#ifdef PS2_PLATFORM

#if defined(PS2_ENABLE_VU0_MESH_FINALIZE)

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <dma.h>
#include <ee_regs.h>

#include "platform/Log.h"
#include "ps2/render/Ps2CaptureLayout.h"

extern "C" {
extern unsigned char ps2Vu0MeshFinalize_CodeStart[];
extern unsigned char ps2Vu0MeshFinalize_CodeEnd[];
}

namespace
{
    static const unsigned int kVifNop = 0x00;
    static const unsigned int kVifStcycl = 0x01;
    static const unsigned int kVifFlushe = 0x10;
    static const unsigned int kVifMscal = 0x14;
    static const unsigned int kVifMpg = 0x4a;
    static const unsigned int kVifUnpackV4_32 = 0x6c;

    static const int kBatchVertices = 160;
    static const int kContextQwords = 6;
    static const int kVu0DataQwords = 256;
    static const std::uintptr_t kVu0DataAddress = 0x11004000u;
    static const unsigned int kDmaRunning = 0x100u;
    static const unsigned int kVifBusyMask = 0x7u; // VPS[1:0] | VEW
    static const unsigned int kVifWaitCycles = 294912u * 300u;

    static unsigned int s_microPacket[1024] __attribute__((aligned(64)));
    static unsigned int s_batchPacket[1024] __attribute__((aligned(64)));
    static bool s_dmaInitialized = false;
    static bool s_programResident = false;
    static bool s_inFlight = false;
    static bool s_disabled = false;
    static unsigned int s_inFlightBeginCycles = 0;

    static_assert((kBatchVertices & 3) == 0,
                  "VU0 mesh-finalize batches must contain whole quads");
    static_assert(kContextQwords +
                      (kBatchVertices * Ps2CaptureLayout::Slots) / 4 <=
                      kVu0DataQwords,
                  "VU0 mesh-finalize batch exceeds VU0 data memory");

    static inline unsigned int vifCode(unsigned int command, unsigned int num,
                                       unsigned int immediate)
    {
        return (command << 24) | ((num & 0xffu) << 16) | (immediate & 0xffffu);
    }

    static inline unsigned int floatBits(float value)
    {
        unsigned int bits;
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    }

    static inline unsigned int eeCount()
    {
        unsigned int cycles;
        __asm__ __volatile__("mfc0 %0, $9" : "=r"(cycles));
        return cycles;
    }

    static inline volatile unsigned int* ucabWords(unsigned int* packet)
    {
        return reinterpret_cast<volatile unsigned int*>(
            reinterpret_cast<std::uintptr_t>(packet) | 0x30000000u);
    }

    static inline void flushUcab()
    {
        __asm__ __volatile__("sync.l; sync.p" ::: "memory");
    }

    static void disableAfterFailure(const char* site)
    {
        *R_EE_D0_CHCR = 0;
        *R_EE_VIF0_FBRST = 0x2; // FBK: stop a running VU0 microprogram.
        *R_EE_VIF0_FBRST = 0x1; // RST: reset VIF0 and its FIFO.
        *R_EE_VIF0_ERR = 0;
        *R_EE_D_STAT = 0x1; // Write-one-to-clear channel-0 status.
        flushUcab();

        dma_channel_initialize(DMA_CHANNEL_VIF0, nullptr, 0);
        s_dmaInitialized = true;
        s_programResident = false;
        s_inFlight = false;
        s_disabled = true;

        MC_LOG_ERROR("render",
            "[PS2] VU0 mesh finalize disabled for this session (%s); using CPU packing\n",
            site ? site : "?");
    }

    static Ps2Vu0MeshFinalizePollResult pollVif0(const char* site)
    {
        if (!s_inFlight)
            return s_disabled ? Ps2Vu0MeshFinalizePollResult::Failed
                              : Ps2Vu0MeshFinalizePollResult::Idle;

        const unsigned int chcr = (unsigned int)*R_EE_D0_CHCR;
        const unsigned int stat = (unsigned int)*R_EE_VIF0_STAT;
        if ((chcr & kDmaRunning) == 0u && (stat & kVifBusyMask) == 0u)
        {
            flushUcab();
            s_inFlight = false;
            return Ps2Vu0MeshFinalizePollResult::Complete;
        }

        if (eeCount() - s_inFlightBeginCycles < kVifWaitCycles)
            return Ps2Vu0MeshFinalizePollResult::Pending;

        MC_LOG_ERROR("render",
            "[PS2] VIF0 stalled in %s: CHCR=%08x MADR=%08x QWC=%08x "
            "STAT=%08x ERR=%08x CODE=%08x NUM=%08x D_STAT=%08x\n",
            site ? site : "?", chcr,
            (unsigned int)*R_EE_D0_MADR, (unsigned int)*R_EE_D0_QWC,
            stat, (unsigned int)*R_EE_VIF0_ERR,
            (unsigned int)*R_EE_VIF0_CODE, (unsigned int)*R_EE_VIF0_NUM,
            (unsigned int)*R_EE_D_STAT);
        disableAfterFailure(site);
        return Ps2Vu0MeshFinalizePollResult::Failed;
    }

    static bool waitForVif0(const char* site)
    {
        for (;;)
        {
            const Ps2Vu0MeshFinalizePollResult result = pollVif0(site);
            if (result == Ps2Vu0MeshFinalizePollResult::Pending)
                continue;
            return result == Ps2Vu0MeshFinalizePollResult::Complete ||
                   result == Ps2Vu0MeshFinalizePollResult::Idle;
        }
    }

    static bool ensureDma()
    {
        if (s_dmaInitialized)
            return true;
        if (dma_channel_initialize(DMA_CHANNEL_VIF0, nullptr, 0) < 0)
        {
            s_disabled = true;
            return false;
        }
        s_dmaInitialized = true;
        return true;
    }

    static bool sendAndWait(volatile unsigned int* packet, int words,
                            const char* site)
    {
        if (words <= 0)
            return false;
        while ((words & 3) != 0)
            packet[words++] = vifCode(kVifNop, 0, 0);

        flushUcab();
        if (dma_channel_send_normal_ucab(DMA_CHANNEL_VIF0,
                (void*)packet, words / 4, 0) < 0)
            return false;

        s_inFlight = true;
        s_inFlightBeginCycles = eeCount();
        return waitForVif0(site);
    }

    static bool ensureProgram()
    {
        if (s_programResident)
            return true;
        if (s_disabled || !ensureDma())
            return false;

        const unsigned char* begin = ps2Vu0MeshFinalize_CodeStart;
        const unsigned char* end = ps2Vu0MeshFinalize_CodeEnd;
        const int byteCount = (int)(end - begin);
        if (byteCount <= 0 || (byteCount & 7) != 0)
        {
            s_disabled = true;
            return false;
        }

        const int instructionCount = byteCount / 8;
        if (instructionCount <= 0 || instructionCount > 512)
        {
            s_disabled = true;
            return false;
        }

        volatile unsigned int* packet = ucabWords(s_microPacket);
        const int capacityWords = (int)(sizeof(s_microPacket) / sizeof(s_microPacket[0]));
        int words = 0;
        int uploaded = 0;
        while (uploaded < instructionCount)
        {
            const int batch = (instructionCount - uploaded > 255) ?
                255 : instructionCount - uploaded;
            const int codeWords = batch * 2;
            if (words + 1 + codeWords + 3 > capacityWords)
            {
                s_disabled = true;
                return false;
            }

            packet[words++] = vifCode(kVifMpg, (unsigned int)batch,
                                      (unsigned int)uploaded);
            for (int i = 0; i < codeWords; ++i)
            {
                unsigned int word;
                std::memcpy(&word,
                    begin + ((std::size_t)uploaded * 2u + (std::size_t)i) * 4u,
                    sizeof(word));
                packet[words++] = word;
            }
            uploaded += batch;
        }

        if (!sendAndWait(packet, words, "mesh-mpg"))
        {
            if (!s_disabled)
                disableAfterFailure("mesh-mpg-send");
            return false;
        }

        s_programResident = true;
        return true;
    }
}

bool ps2_vu0_mesh_finalize_begin(const int_t* raw, int_t vertexCount)
{
    if (raw == nullptr || vertexCount <= 0 || vertexCount > kBatchVertices ||
        (vertexCount & 3) != 0 || s_inFlight || !ensureProgram())
        return false;

    const int rawWords = vertexCount * Ps2CaptureLayout::Slots;
    if ((rawWords & 3) != 0)
        return false;
    const int rawQwords = rawWords / 4;
    const int unpackQwords = kContextQwords + rawQwords;
    if (unpackQwords > 255 || unpackQwords > kVu0DataQwords)
        return false;

    volatile unsigned int* packet = ucabWords(s_batchPacket);
    const int capacityWords = (int)(sizeof(s_batchPacket) / sizeof(s_batchPacket[0]));
    int words = 0;
    packet[words++] = vifCode(kVifStcycl, 0, 0x0101);
    packet[words++] = vifCode(kVifUnpackV4_32,
                              (unsigned int)unpackQwords, 0);

    const unsigned int bias = floatBits(-8.0f);
    const unsigned int positionScale = floatBits(1024.0f);
    const unsigned int uvScale = floatBits(4096.0f);
    const unsigned int minimum = floatBits(-32768.0f);
    const unsigned int maximum = floatBits(32767.0f);
    for (int lane = 0; lane < 4; ++lane) packet[words++] = bias;
    for (int lane = 0; lane < 4; ++lane) packet[words++] = positionScale;
    for (int lane = 0; lane < 4; ++lane) packet[words++] = uvScale;
    for (int lane = 0; lane < 4; ++lane) packet[words++] = minimum;
    for (int lane = 0; lane < 4; ++lane) packet[words++] = maximum;
    packet[words++] = (unsigned int)(vertexCount / 2);
    packet[words++] = 0;
    packet[words++] = 0;
    packet[words++] = 0;

    if (words + rawWords + 4 > capacityWords)
        return false;
    for (int i = 0; i < rawWords; ++i)
        packet[words++] = (unsigned int)raw[i];

    packet[words++] = vifCode(kVifMscal, 0, 0);
    packet[words++] = vifCode(kVifFlushe, 0, 0);
    while ((words & 3) != 0)
        packet[words++] = vifCode(kVifNop, 0, 0);

    flushUcab();
    if (dma_channel_send_normal_ucab(DMA_CHANNEL_VIF0,
            (void*)packet, words / 4, 0) < 0)
    {
        disableAfterFailure("mesh-submit");
        return false;
    }

    s_inFlight = true;
    s_inFlightBeginCycles = eeCount();
    return true;
}

Ps2Vu0MeshFinalizePollResult ps2_vu0_mesh_finalize_poll()
{
    return pollVif0("mesh-finalize");
}

bool ps2_vu0_mesh_finalize_wait()
{
    if (!s_inFlight)
        return !s_disabled;
    return waitForVif0("mesh-finalize");
}

bool ps2_vu0_mesh_finalize_drain()
{
    return ps2_vu0_mesh_finalize_wait();
}

const volatile int_t* ps2_vu0_mesh_finalize_result()
{
    if (s_inFlight || s_disabled)
        return nullptr;
    return reinterpret_cast<const volatile int_t*>(
        kVu0DataAddress + (std::uintptr_t)kContextQwords * 16u);
}

int_t ps2_vu0_mesh_finalize_batch_capacity()
{
    return kBatchVertices;
}

#else

bool ps2_vu0_mesh_finalize_begin(const int_t*, int_t) { return false; }
Ps2Vu0MeshFinalizePollResult ps2_vu0_mesh_finalize_poll() { return Ps2Vu0MeshFinalizePollResult::Idle; }
bool ps2_vu0_mesh_finalize_wait() { return true; }
bool ps2_vu0_mesh_finalize_drain() { return true; }
const volatile int_t* ps2_vu0_mesh_finalize_result() { return nullptr; }
int_t ps2_vu0_mesh_finalize_batch_capacity() { return 0; }

#endif // PS2_ENABLE_VU0_MESH_FINALIZE
#endif // PS2_PLATFORM
