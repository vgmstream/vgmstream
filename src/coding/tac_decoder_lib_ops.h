#ifndef _TAC_DECODER_LIB_OPS_H_
#define _TAC_DECODER_LIB_OPS_H_

#include <math.h>
#include "tac_decoder_lib_ops.h"

/* The following ops are similar to VU1's ops, but not quite the same. For example VU1 has special op
 * registers like the ACC, and updates zero/neg/etc flags per op (plus added here a few helper ops).
 * Main reason to use them vs doing standard +*-/ in code is allowing to simulate PS2 floats.
 * See Nisto's decoder for actual emulation. */


/* PS2 floats are slightly different vs IEEE 754 floats:
 * - NaN and Inf (exp 255) don't exist on the PS2, meaning it has a bigger range of floats
 * - denormals (exp 0) don't exist either, and ops truncate to 0
 * - rounding on PS2 always rounds towards zero
 * The code below (partially) simulates this, but for audio it only means +-1 differences,
 * plus we can't fully emulate exact behaviour, so it's disabled for performance
 * (function call is optimized out by compiler). */
#define TAC_ENABLE_PS2_FLOATS  0

static inline void UPDATE_FLOATS(uint8_t dest, REG_VF *vf) {
#if TAC_ENABLE_PS2_FLOATS
    int i;

    for (i = 0; i < 4; i++) {
        int shift = 3 - i;
        if (dest & (1 << shift)) {

            if (vf->F[i] == 0.0) {
                uint32_t v = vf->UL[i];
                int exp = (v >> 23) & 0xff;
                uint32_t s = v & 0x80000000;

                switch (exp) {
                    case 0:
                        vf->UL[i] = s;
                        break;
                    case 255:
                        vf->UL[i] = s|0x7f7fffff; /* max allowed */
                        break;
                    default: /* standard */
                        vf->UL[i] = v;
                        break;
                }
            }
        }
    }
#endif
}

static inline void _DIV_INTERNAL(REG_VF *fd, const REG_VF *fs, const REG_VF *ft, int from) {
    float dividend = fs->F[from];
    float divisor = ft->F[from];

#if TAC_ENABLE_PS2_FLOATS
    if (divisor == 0.0) {
        if ((ft->UL[from] & 0x80000000) != (0x80000000 & fs->UL[from])) {
            fd->UL[from] = 0xFF7FFFFF;
        }
        else {
            fd->UL[from] = 0x7F7FFFFF;
        }
    }
    else {
        fd->F[from] = dividend / divisor;
    }
#else
    fd->F[from] = dividend / divisor;
#endif
}

///////////////////////////////////////////////////////////////////////////////

static inline void DIV(uint8_t dest, REG_VF *fd, const REG_VF *fs, const REG_VF *ft) {
    if (dest & _x___) _DIV_INTERNAL(fd, fs, ft, 0);
    if (dest & __y__) _DIV_INTERNAL(fd, fs, ft, 1);
    if (dest & ___z_) _DIV_INTERNAL(fd, fs, ft, 2);
    if (dest & ____w) _DIV_INTERNAL(fd, fs, ft, 3);
    UPDATE_FLOATS(dest, fd);
}

///////////////////////////////////////////////////////////////////////////////

static inline void ADD(uint8_t dest, REG_VF *fd, const REG_VF *fs, const REG_VF *ft) {
    if (dest & _x___) fd->f.x = fs->f.x + ft->f.x;
    if (dest & __y__) fd->f.y = fs->f.y + ft->f.y;
    if (dest & ___z_) fd->f.z = fs->f.z + ft->f.z;
    if (dest & ____w) fd->f.w = fs->f.w + ft->f.w;
    UPDATE_FLOATS(dest, fd);
}

static inline void ADDx(uint8_t dest, REG_VF *fd, const REG_VF *fs, const REG_VF *ft) {
    if (dest & _x___) fd->f.x = fs->f.x + ft->f.x;
    if (dest & __y__) fd->f.y = fs->f.y + ft->f.x;
    if (dest & ___z_) fd->f.z = fs->f.z + ft->f.x;
    if (dest & ____w) fd->f.w = fs->f.w + ft->f.x;
    UPDATE_FLOATS(dest, fd);
}

static inline void ADDy(uint8_t dest, REG_VF *fd, const REG_VF *fs, const REG_VF *ft) {
    if (dest & _x___) fd->f.x = fs->f.x + ft->f.y;
    if (dest & __y__) fd->f.y = fs->f.y + ft->f.y;
    if (dest & ___z_) fd->f.z = fs->f.z + ft->f.y;
    if (dest & ____w) fd->f.w = fs->f.w + ft->f.y;
    UPDATE_FLOATS(dest, fd);
}

static inline void ADDz(uint8_t dest, REG_VF *fd, const REG_VF *fs, const REG_VF *ft) {
    if (dest & _x___) fd->f.x = fs->f.x + ft->f.z;
    if (dest & __y__) fd->f.y = fs->f.y + ft->f.z;
    if (dest & ___z_) fd->f.z = fs->f.z + ft->f.z;
    if (dest & ____w) fd->f.w = fs->f.w + ft->f.z;
    UPDATE_FLOATS(dest, fd);
}

static inline void ADDw(uint8_t dest, REG_VF *fd, const REG_VF *fs, const REG_VF *ft) {
    if (dest & _x___) fd->f.x = fs->f.x + ft->f.w;
    if (dest & __y__) fd->f.y = fs->f.y + ft->f.w;
    if (dest & ___z_) fd->f.z = fs->f.z + ft->f.w;
    if (dest & ____w) fd->f.w = fs->f.w + ft->f.w;
    UPDATE_FLOATS(dest, fd);
}

///////////////////////////////////////////////////////////////////////////////

static inline void SUB(uint8_t dest, REG_VF *fd, const REG_VF *fs, const REG_VF *ft) {
    if (dest & _x___) fd->f.x = fs->f.x - ft->f.x;
    if (dest & __y__) fd->f.y = fs->f.y - ft->f.y;
    if (dest & ___z_) fd->f.z = fs->f.z - ft->f.z;
    if (dest & ____w) fd->f.w = fs->f.w - ft->f.w;
    UPDATE_FLOATS(dest, fd);
}

static inline void SUBx(uint8_t dest, REG_VF *fd, const REG_VF *fs, const REG_VF *ft) {
    if (dest & _x___) fd->f.x = fs->f.x - ft->f.x;
    if (dest & __y__) fd->f.y = fs->f.y - ft->f.x;
    if (dest & ___z_) fd->f.z = fs->f.z - ft->f.x;
    if (dest & ____w) fd->f.w = fs->f.w - ft->f.x;
    UPDATE_FLOATS(dest, fd);
}

static inline void SUBy(uint8_t dest, REG_VF *fd, const REG_VF *fs, const REG_VF *ft) {
    if (dest & _x___) fd->f.x = fs->f.x - ft->f.y;
    if (dest & __y__) fd->f.y = fs->f.y - ft->f.y;
    if (dest & ___z_) fd->f.z = fs->f.z - ft->f.y;
    if (dest & ____w) fd->f.w = fs->f.w - ft->f.y;
    UPDATE_FLOATS(dest, fd);
}

static inline void SUBz(uint8_t dest, REG_VF *fd, const REG_VF *fs, const REG_VF *ft) {
    if (dest & _x___) fd->f.x = fs->f.x - ft->f.z;
    if (dest & __y__) fd->f.y = fs->f.y - ft->f.z;
    if (dest & ___z_) fd->f.z = fs->f.z - ft->f.z;
    if (dest & ____w) fd->f.w = fs->f.w - ft->f.z;
    UPDATE_FLOATS(dest, fd);
}

static inline void SUBw(uint8_t dest, REG_VF *fd, const REG_VF *fs, const REG_VF *ft) {
    if (dest & _x___) fd->f.x = fs->f.x - ft->f.w;
    if (dest & __y__) fd->f.y = fs->f.y - ft->f.w;
    if (dest & ___z_) fd->f.z = fs->f.z - ft->f.w;
    if (dest & ____w) fd->f.w = fs->f.w - ft->f.w;
    UPDATE_FLOATS(dest, fd);
}

///////////////////////////////////////////////////////////////////////////////

static inline void MUL(uint8_t dest, REG_VF *fd, const REG_VF *fs, const REG_VF *ft) {
    if (dest & _x___) fd->f.x = fs->f.x * ft->f.x;
    if (dest & __y__) fd->f.y = fs->f.y * ft->f.y;
    if (dest & ___z_) fd->f.z = fs->f.z * ft->f.z;
    if (dest & ____w) fd->f.w = fs->f.w * ft->f.w;
    UPDATE_FLOATS(dest, fd);
}

static inline void MULx(uint8_t dest, REG_VF *fd, const REG_VF *fs, const REG_VF *ft) {
    if (dest & _x___) fd->f.x = fs->f.x * ft->f.x;
    if (dest & __y__) fd->f.y = fs->f.y * ft->f.x;
    if (dest & ___z_) fd->f.z = fs->f.z * ft->f.x;
    if (dest & ____w) fd->f.w = fs->f.w * ft->f.x;
    UPDATE_FLOATS(dest, fd);
}

static inline void MULy(uint8_t dest, REG_VF *fd, const REG_VF *fs, const REG_VF *ft) {
    if (dest & _x___) fd->f.x = fs->f.x * ft->f.y;
    if (dest & __y__) fd->f.y = fs->f.y * ft->f.y;
    if (dest & ___z_) fd->f.z = fs->f.z * ft->f.y;
    if (dest & ____w) fd->f.w = fs->f.w * ft->f.y;
    UPDATE_FLOATS(dest, fd);
}

static inline void MULz(uint8_t dest, REG_VF *fd, const REG_VF *fs, const REG_VF *ft) {
    if (dest & _x___) fd->f.x = fs->f.x * ft->f.z;
    if (dest & __y__) fd->f.y = fs->f.y * ft->f.z;
    if (dest & ___z_) fd->f.z = fs->f.z * ft->f.z;
    if (dest & ____w) fd->f.w = fs->f.w * ft->f.z;
    UPDATE_FLOATS(dest, fd);
}

static inline void MULw(uint8_t dest, REG_VF *fd, const REG_VF *fs, const REG_VF *ft) {
    if (dest & _x___) fd->f.x = fs->f.x * ft->f.w;
    if (dest & __y__) fd->f.y = fs->f.y * ft->f.w;
    if (dest & ___z_) fd->f.z = fs->f.z * ft->f.w;
    if (dest & ____w) fd->f.w = fs->f.w * ft->f.w;
    UPDATE_FLOATS(dest, fd);
}

///////////////////////////////////////////////////////////////////////////////

static inline void MADD(uint8_t dest, REG_VF *fd, const REG_VF *fs, const REG_VF *ft) {
    if (dest & _x___) fd->f.x = fd->f.x + (fs->f.x * ft->f.x);
    if (dest & __y__) fd->f.y = fd->f.y + (fs->f.y * ft->f.y);
    if (dest & ___z_) fd->f.z = fd->f.z + (fs->f.z * ft->f.z);
    if (dest & ____w) fd->f.w = fd->f.w + (fs->f.w * ft->f.w);
    UPDATE_FLOATS(dest, fd);
}

static inline void MADDx(uint8_t dest, REG_VF *fd, const REG_VF *fs, const REG_VF *ft) {
    if (dest & _x___) fd->f.x = fd->f.x + (fs->f.x * ft->f.x);
    if (dest & __y__) fd->f.y = fd->f.y + (fs->f.y * ft->f.x);
    if (dest & ___z_) fd->f.z = fd->f.z + (fs->f.z * ft->f.x);
    if (dest & ____w) fd->f.w = fd->f.w + (fs->f.w * ft->f.x);
    UPDATE_FLOATS(dest, fd);
}

static inline void MADDy(uint8_t dest, REG_VF *fd, const REG_VF *fs, const REG_VF *ft) {
    if (dest & _x___) fd->f.x = fd->f.x + (fs->f.x * ft->f.y);
    if (dest & __y__) fd->f.y = fd->f.y + (fs->f.y * ft->f.y);
    if (dest & ___z_) fd->f.z = fd->f.z + (fs->f.z * ft->f.y);
    if (dest & ____w) fd->f.w = fd->f.w + (fs->f.w * ft->f.y);
    UPDATE_FLOATS(dest, fd);
}

static inline void MADDz(uint8_t dest, REG_VF *fd, const REG_VF *fs, const REG_VF *ft) {
    if (dest & _x___) fd->f.x = fd->f.x + (fs->f.x * ft->f.z);
    if (dest & __y__) fd->f.y = fd->f.y + (fs->f.y * ft->f.z);
    if (dest & ___z_) fd->f.z = fd->f.z + (fs->f.z * ft->f.z);
    if (dest & ____w) fd->f.w = fd->f.w + (fs->f.w * ft->f.z);
    UPDATE_FLOATS(dest, fd);
}

static inline void MADDw(uint8_t dest, REG_VF *fd, const REG_VF *fs, const REG_VF *ft) {
    if (dest & _x___) fd->f.x = fd->f.x + (fs->f.x * ft->f.w);
    if (dest & __y__) fd->f.y = fd->f.y + (fs->f.y * ft->f.w);
    if (dest & ___z_) fd->f.z = fd->f.z + (fs->f.z * ft->f.w);
    if (dest & ____w) fd->f.w = fd->f.w + (fs->f.w * ft->f.w);
    UPDATE_FLOATS(dest, fd);
}

static inline void MSUBx(uint8_t dest, REG_VF *fd, const REG_VF *fs, const REG_VF *ft) {
    if (dest & _x___) fd->f.x = fd->f.x - (fs->f.x * ft->f.x);
    if (dest & __y__) fd->f.y = fd->f.y - (fs->f.y * ft->f.x);
    if (dest & ___z_) fd->f.z = fd->f.z - (fs->f.z * ft->f.x);
    if (dest & ____w) fd->f.w = fd->f.w - (fs->f.w * ft->f.x);
    UPDATE_FLOATS(dest, fd);
}

static inline void MSUBy(uint8_t dest, REG_VF *fd, const REG_VF *fs, const REG_VF *ft) {
    if (dest & _x___) fd->f.x = fd->f.x - (fs->f.x * ft->f.y);
    if (dest & __y__) fd->f.y = fd->f.y - (fs->f.y * ft->f.y);
    if (dest & ___z_) fd->f.z = fd->f.z - (fs->f.z * ft->f.y);
    if (dest & ____w) fd->f.w = fd->f.w - (fs->f.w * ft->f.y);
    UPDATE_FLOATS(dest, fd);
}

static inline void MSUBz(uint8_t dest, REG_VF *fd, const REG_VF *fs, const REG_VF *ft) {
    if (dest & _x___) fd->f.x = fd->f.x - (fs->f.x * ft->f.z);
    if (dest & __y__) fd->f.y = fd->f.y - (fs->f.y * ft->f.z);
    if (dest & ___z_) fd->f.z = fd->f.z - (fs->f.z * ft->f.z);
    if (dest & ____w) fd->f.w = fd->f.w - (fs->f.w * ft->f.z);
    UPDATE_FLOATS(dest, fd);
}

static inline void MSUBw(uint8_t dest, REG_VF *fd, const REG_VF *fs, const REG_VF *ft) {
    if (dest & _x___) fd->f.x = fd->f.x - (fs->f.x * ft->f.w);
    if (dest & __y__) fd->f.y = fd->f.y - (fs->f.y * ft->f.w);
    if (dest & ___z_) fd->f.z = fd->f.z - (fs->f.z * ft->f.w);
    if (dest & ____w) fd->f.w = fd->f.w - (fs->f.w * ft->f.w);
    UPDATE_FLOATS(dest, fd);
}

///////////////////////////////////////////////////////////////////////////////

static inline void FMUL(uint8_t dest, REG_VF *fd, const REG_VF *fs, const float I_F) {
    if (dest & _x___) fd->f.x = fs->f.x * I_F;
    if (dest & __y__) fd->f.y = fs->f.y * I_F;
    if (dest & ___z_) fd->f.z = fs->f.z * I_F;
    if (dest & ____w) fd->f.w = fs->f.w * I_F;
    UPDATE_FLOATS(dest, fd);
}

static inline void FMULf(uint8_t dest, REG_VF *fd, const float fs) {
    if (dest & _x___) fd->f.x = fd->f.x * fs;
    if (dest & __y__) fd->f.y = fd->f.y * fs;
    if (dest & ___z_) fd->f.z = fd->f.z * fs;
    if (dest & ____w) fd->f.w = fd->f.w * fs;
    UPDATE_FLOATS(dest, fd);
}

///////////////////////////////////////////////////////////////////////////////

static inline void ABS(uint8_t dest, REG_VF *ft, const REG_VF *fs) {
    if (dest & _x___) ft->f.x = fabsf(fs->f.x);
    if (dest & __y__) ft->f.y = fabsf(fs->f.y);
    if (dest & ___z_) ft->f.z = fabsf(fs->f.z);
    if (dest & ____w) ft->f.w = fabsf(fs->f.w);
}

static inline void FTOI0(uint8_t dest, REG_VF *ft, const REG_VF *fs) {
    if (dest & _x___) ft->SL[0] = (int32_t)fs->f.x;
    if (dest & __y__) ft->SL[1] = (int32_t)fs->f.y;
    if (dest & ___z_) ft->SL[2] = (int32_t)fs->f.z;
    if (dest & ____w) ft->SL[3] = (int32_t)fs->f.w;
}

static inline void ITOF0(uint8_t dest, REG_VF *ft, const REG_VF *fs) {
    if (dest & _x___) ft->f.x = (float)fs->SL[0];
    if (dest & __y__) ft->f.y = (float)fs->SL[1];
    if (dest & ___z_) ft->f.z = (float)fs->SL[2];
    if (dest & ____w) ft->f.w = (float)fs->SL[3];
}

static inline void MR32(uint8_t dest, REG_VF *ft, const REG_VF *fs) {
    float x = fs->f.x;
    if (dest & _x___) ft->f.x = fs->f.y;
    if (dest & __y__) ft->f.y = fs->f.z;
    if (dest & ___z_) ft->f.z = fs->f.w;
    if (dest & ____w) ft->f.w = x;
}

///////////////////////////////////////////////////////////////////////////////

static inline void LOAD(uint8_t dest, REG_VF *ft, REG_VF* src, int pos) {
    if (dest & _x___) ft->f.x = src[pos].f.x;
    if (dest & __y__) ft->f.y = src[pos].f.y;
    if (dest & ___z_) ft->f.z = src[pos].f.z;
    if (dest & ____w) ft->f.w = src[pos].f.w;
}

static inline void STORE(uint8_t dest, REG_VF* dst, const REG_VF *fs, int pos) {
    if (dest & _x___) dst[pos].f.x = fs->f.x;
    if (dest & __y__) dst[pos].f.y = fs->f.y;
    if (dest & ___z_) dst[pos].f.z = fs->f.z;
    if (dest & ____w) dst[pos].f.w = fs->f.w;
}

static inline void MOVE(uint8_t dest, REG_VF *fd, const REG_VF *fs) {
    if (dest & _x___) fd->f.x = fs->f.x;
    if (dest & __y__) fd->f.y = fs->f.y;
    if (dest & ___z_) fd->f.z = fs->f.z;
    if (dest & ____w) fd->f.w = fs->f.w;
}

static inline void MOVEx(uint8_t dest, REG_VF *fd, const REG_VF *fs) {
    if (dest & _x___) fd->f.x = fs->f.x;
    if (dest & __y__) fd->f.y = fs->f.x;
    if (dest & ___z_) fd->f.z = fs->f.x;
    if (dest & ____w) fd->f.w = fs->f.x;
}

static inline void SIGN(uint8_t dest, REG_VF *fd, const REG_VF *fs) {
    if (dest & _x___) if (fs->f.x < 0) fd->f.x = -fd->f.x;
    if (dest & __y__) if (fs->f.y < 0) fd->f.y = -fd->f.y;
    if (dest & ___z_) if (fs->f.z < 0) fd->f.z = -fd->f.z;
    if (dest & ____w) if (fs->f.w < 0) fd->f.w = -fd->f.w;
}

static inline void COPY(uint8_t dest, REG_VF *fd, const int16_t* buf) {
    if (dest & _x___) fd->f.x = buf[0];
    if (dest & __y__) fd->f.y = buf[1];
    if (dest & ___z_) fd->f.z = buf[2];
    if (dest & ____w) fd->f.w = buf[3];
}

#endif /* _TAC_DECODER_LIB_OPS_H_ */
