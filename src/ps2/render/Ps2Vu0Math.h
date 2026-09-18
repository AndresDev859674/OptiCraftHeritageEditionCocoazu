#pragma once

#ifdef PS2_PLATFORM

#include <cstdint>

#include <libvux.h>

// ---- COP2 (VU0 macro mode) float kernels for terrain management ----
//
// These are the non-drawing counterparts of ps2_vu0_xform3/4 in Ps2ClipGuard.h:
// the same accumulator-free idiom and the same hardcoded register convention,
// applied to the float work the terrain path does BETWEEN draws -- per-section
// matrix setup and per-cluster visibility classification.
//
// Macro mode issues COP2 instructions from the EE pipeline, so nothing here runs
// in parallel with the EE. The win is SIMD width (four lanes per instruction)
// plus hoisting an operand set that is constant for a whole frame out of the
// per-cluster loop. Register use is hardcoded exactly as libvux and
// Ps2ClipGuard.h do it: GCC's R5900 backend does not allocate COP2 registers for
// ordinary C, so vf16-vf31 are free, and each block is self-contained -- it
// loads its operands, computes and stores -- so nothing depends on a vf register
// surviving across statements.
//
// VU floats are not IEEE-754: denormals flush to zero and an overflow clamps to
// the maximum magnitude instead of producing an infinity. Values fed through
// these kernels must therefore stay inside ordinary float range, which is why
// the neutral plane below is a large finite number and not a sentinel infinity.

// Four planes transposed so one pass produces four results at once.
//
// Testing one plane against an AABB needs two dot products:
//
//     distance = plane . (center, 1)
//     radius   = |plane.xyz| . extent
//
// Per plane those are horizontal sums, which COP2 has no single instruction for.
// Transposing the plane set turns both into the vertical multiply-accumulate
// ps2_vu0_xform4 already uses: coeff[0..3] holds the x coefficient of all four
// planes, coeff[4..7] the y, coeff[8..11] the z and coeff[12..15] the constant
// term, so
//
//     out = coeffX*center.x + coeffY*center.y + coeffZ*center.z + coeffW*center.w
//
// lands four planes' distances in the four lanes of one register.
//
// absCoeff carries |x|,|y|,|z| of the same four planes. The absolute value is a
// property of the plane and not of the cluster, so folding it in at build time
// removes three fabsf per plane per cluster from the classification loop. There
// is no w row: the projected radius has no constant term.
struct alignas(16) Ps2Vu0PlaneBlock
{
    float coeff[16];
    float absCoeff[12];
};

// Transpose up to four row-major planes into one block.
//
// Lanes past planeCount get a neutral plane: distance far positive, radius zero.
// A caller's reject test (distance + radius < 0) and its "touches the plane"
// test (distance - radius <= epsilon) both pass such a lane, so padding can
// neither reject an AABB nor demote it from fully-inside, and the caller does
// not have to mask the tail.
void ps2_vu0_plane_block_build(Ps2Vu0PlaneBlock& out,
                               const float planes[][4],
                               int planeCount);

// Signed center distance and projected radius of one AABB against the block's
// four planes. center->w must be 1.0 and extent->w 0.0. distance and radius are
// four floats each and must be 16-byte aligned.
//
// Every multiply issues before the first add that consumes it, which is the same
// reason ps2_vu0_xform4 interleaves four vertices: a COP2 result is not ready in
// the next issue slot, and the two independent chains (distance, radius) give
// the scheduler something to put in between.
static inline void ps2_vu0_plane_block_eval(const Ps2Vu0PlaneBlock& block,
                                            const VU_VECTOR* center,
                                            const VU_VECTOR* extent,
                                            float* distance,
                                            float* radius)
{
    __asm__ __volatile__ (
        "lqc2       $vf16, 0x00(%[c])       \n"
        "lqc2       $vf17, 0x10(%[c])       \n"
        "lqc2       $vf18, 0x20(%[c])       \n"
        "lqc2       $vf19, 0x30(%[c])       \n"
        "lqc2       $vf20, 0x00(%[a])       \n"
        "lqc2       $vf21, 0x10(%[a])       \n"
        "lqc2       $vf22, 0x20(%[a])       \n"
        "lqc2       $vf23, 0x00(%[ct])      \n"
        "lqc2       $vf24, 0x00(%[ex])      \n"
        "vmulx.xyzw $vf25, $vf16, $vf23x    \n"
        "vmulx.xyzw $vf26, $vf20, $vf24x    \n"
        "vmuly.xyzw $vf27, $vf17, $vf23y    \n"
        "vmuly.xyzw $vf28, $vf21, $vf24y    \n"
        "vmulz.xyzw $vf29, $vf18, $vf23z    \n"
        "vmulz.xyzw $vf30, $vf22, $vf24z    \n"
        "vmulw.xyzw $vf31, $vf19, $vf23w    \n"
        "vadd.xyzw  $vf25, $vf25, $vf27     \n"
        "vadd.xyzw  $vf26, $vf26, $vf28     \n"
        "vadd.xyzw  $vf25, $vf25, $vf29     \n"
        "vadd.xyzw  $vf26, $vf26, $vf30     \n"
        "vadd.xyzw  $vf25, $vf25, $vf31     \n"
        "sqc2       $vf25, 0x00(%[d])       \n"
        "sqc2       $vf26, 0x00(%[r])       \n"
        :
        : [c]  "r" (block.coeff),
          [a]  "r" (block.absCoeff),
          [ct] "r" (center),
          [ex] "r" (extent),
          [d]  "r" (distance),
          [r]  "r" (radius)
        : "memory");
}

// out = mvp * translate(tx, ty, tz).
//
// For M*T only the fourth column changes, so this is col3 += col0*tx + col1*ty +
// col2*tz -- the same transform ps2_vu0_xform4 applies to a vertex, minus the w
// round. translate->w is ignored. out may alias mvp; both must be 16-byte
// aligned, because lqc2/sqc2 fault otherwise.
static inline void ps2_vu0_mvp_translate(float* out, const float* mvp,
                                         const VU_VECTOR* translate)
{
    __asm__ __volatile__ (
        "lqc2       $vf16, 0x00(%[m])       \n"
        "lqc2       $vf17, 0x10(%[m])       \n"
        "lqc2       $vf18, 0x20(%[m])       \n"
        "lqc2       $vf19, 0x30(%[m])       \n"
        "lqc2       $vf20, 0x00(%[t])       \n"
        "vmulx.xyzw $vf24, $vf16, $vf20x    \n"
        "vmuly.xyzw $vf25, $vf17, $vf20y    \n"
        "vmulz.xyzw $vf26, $vf18, $vf20z    \n"
        "vadd.xyzw  $vf24, $vf24, $vf25     \n"
        "vadd.xyzw  $vf19, $vf19, $vf26     \n"
        "vadd.xyzw  $vf19, $vf19, $vf24     \n"
        "sqc2       $vf16, 0x00(%[o])       \n"
        "sqc2       $vf17, 0x10(%[o])       \n"
        "sqc2       $vf18, 0x20(%[o])       \n"
        "sqc2       $vf19, 0x30(%[o])       \n"
        :
        : [m] "r" (mvp),
          [t] "r" (translate),
          [o] "r" (out)
        : "memory");
}

// lqc2/sqc2 fault on an unaligned address. Callers that cannot guarantee 16-byte
// storage themselves test with this and take their scalar path instead, the same
// way ps2_matrix_multiply guards multiplyVu0.
static inline bool ps2_vu0_is_aligned16(const void* pointer)
{
    return (reinterpret_cast<std::uintptr_t>(pointer) & 0xFu) == 0u;
}

#endif // PS2_PLATFORM
