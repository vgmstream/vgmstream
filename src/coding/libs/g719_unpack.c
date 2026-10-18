#include "g719_internal.h"

//-------------------------------------------------------------------------------------------------
// UNPACK

#define MAX16B    (short)0x7fff
#define MIN16B    (short)0x8000
#define NUM_MAP_BANDS               20


#define NORM_LIMIT      39 // EXTRA: max dicn/dicnlg2 table max index

#define  SFM_G1         16
//#define  SFM_G2       8
#define  SFM_G1G2       24
//#define  SFM_G3       12
#define  SFM_N          36
//#define  SFM_GX       8

#define  NB_VECT1       1
#define  NB_VECT2       2
#define  NB_VECT3       3
#define  NB_VECTX       4

#define  QBIT_MAX1      5
#define  QBIT_MAX2      9
#define  OFFSETf        0.015625f
#define  FCT_LVQ1f      1.1f
#define  FCT_LVQ2f      6.0f
#define  FCT_LVQ2fQ13   1365.333333f
//#define  MAX_DIST     100000000.0f
//#define  N_LEADER1    10

#define  FLAGS_BITS     3
#define  NORM0_BITS     5
#define  NORMI_BITS     5
#define  NUMNRMIBITS    215

#define  NOALLGROUPS    0
#define  ALLGROUPS      1

#define  NOHUFCODE      0
#define  HUFCODE        1


/*--------------------------------------------------------------------------*/
/*  Conversion from ITU bit stream into norm index                          */
/*--------------------------------------------------------------------------*/
/*  short       *y   i)  ITU bitstream                                      */
/*  short       N   (i)  bits per norm                                      */
/*  short       *x  (o)  index of quantized norm                            */
/*--------------------------------------------------------------------------*/
void /*inline*/ bits2idxn(bitstream_t* is, short N, short *x) {
    short i;

    int pos = bg_tell(is); //OG: skips handled by caller

    *x = 0;
    for (i = 0; i < N; i++) {
        *x <<= 1;
        if (bg_read1(is) == 1) {
            *x += 1;
        }
    }

    bg_seek(is, pos);
    return;
}

/*--------------------------------------------------------------------------*/
/*  Conversion from ITU bit stream into coefficient index                   */
/*--------------------------------------------------------------------------*/
/*  short       *y  (i)  ITU bitstream                                      */
/*  short       N   (i)  vector dimensions                                  */
/*  short       L   (i)  bits per coefficient                               */
/*  short       *x  (o)  index of quantized coefficient                     */
/*--------------------------------------------------------------------------*/
void bits2idxc(bitstream_t* is, short N, short L, short *x) {
    short i, k, m, n;
    short temp;

    int pos = bg_tell(is); //OG: skips handled by caller

    if (L == 1) {
        n = 1;
        m = N;
    }
    else {
        n = N;
        m = L;
    }

    for (k = 0; k < n; k++) {
        x[k] = 0;
        for (i = 0; i < m; i++) {
            temp = x[k] << 1;
            if (bg_read1(is) == 1) {
                temp += 1;
            }
            x[k] = temp;
        }
    }

    bg_seek(is, pos);
    return;
}

/*---------------------------------------------------------------------------*/
/*  De-quantization for sub-vectors originally allocated with 0 bits         */
/*---------------------------------------------------------------------------*/
/*  short     *y           (i)   indices of the selected codevectors         */
/*  short     *pbits       (i)   pointer to bitstream                        */
/*  short     idxnrm       (i)   indices of quantized norms                  */
/*  short     nb_vecs      (i)   number of 8-D vectors in current sub-vector */
/*  float     *coefs       (o)   MLT coefficients                            */
/*  float     *coefs_norm  (o)   normalized MLT coefficients                 */
/*---------------------------------------------------------------------------*/
static void dprocnf(short *y, bitstream_t* is, short idxnrm, short nb_vecs, float *coefs, float *coefs_norm) {
    short i, j;
    short pre_idx;
    float normq;

    int pos = bg_tell(is); //OG: skips handled by caller

    normq = dicn[idxnrm];
    pre_idx = MAX16B;
    for (i = 0; i < nb_vecs; i++) {
        bits2idxc(is, 8, 1, y);
        bg_skip(is, 8);
        if ((pre_idx < 128) && (*y < 16)) {
            for (j = 0; j < 8; j++) {
                *coefs_norm = OFFSETf;
                *coefs++ = (*coefs_norm++) * normq;
            }
        }
        else {
            for (j = 0; j < 8; j++) {
                *coefs_norm = (float)dic4[*y][j] / FCT_LVQ1f + OFFSETf;
                *coefs++ = (*coefs_norm++) * normq;
            }
        }
        pre_idx = *y;
        y += 8;
    }

    bg_seek(is, pos);
    return;
}

/*--------------------------------------------------------------------------*/
/*  Decoding for sub-vectors originally allocated with 0 bits               */
/*--------------------------------------------------------------------------*/
/*  short     *R            (i/o) number of bits per coefficinet            */
/*  short     *idx          (i)   reordered sub-vector indices              */
/*  short     *ynrm         (i)   quantization indices for norms            */
/*  short     *ycof         (i)   quantization indices for MLT coefficients */
/*  short     **ppbits      (i/o) pointer to bitstream                      */
/*  float     *coefsq       (o)   reconstructed MLT coefficients            */
/*  float     *coefsq_norm  (o)   reconstructed normalized MLT coefficients */
/*  short     nb_sfm        (i)   number of sub-vectors to be coded         */
/*  short     diff          (i)   number of available bits                  */
/*--------------------------------------------------------------------------*/
static void dprocnobitsbfm(short *R, short *idx, short *ynrm, short *ycof, bitstream_t* is,
                           float *coefsq, float *coefsq_norm, short nb_sfm, short diff) {
    short i, k, m, im, offset;

    //int pos = bg_tell(is); // OG: bug? see flvqdec end

    im = MAX16B;
    k = nb_sfm - 1;
    for (i = k; i > 0; i--) {
        if (R[idx[i]] == 0) {
            im = i;
        }
    }
    for (i = im; i < nb_sfm; i++) {
        m = idx[i];
        if (R[m] == 0) {
            if (m < SFM_G1) {
                if (diff >= WID_G1) {
                    R[m] = 1;
                    offset = m * WID_G1;
                    dprocnf(&ycof[offset], is, ynrm[m], NB_VECT1, &coefsq[offset], &coefsq_norm[offset]);
                    bg_skip(is, WID_G1);
                    diff -= WID_G1;
                }
            }
            else if (m < SFM_G1G2) {
                if (diff >= WID_G2) {
                    R[m] = 1;
                    offset = NUMC_G1 + (m - SFM_G1) * WID_G2;
                    dprocnf(&ycof[offset], is, ynrm[m], NB_VECT2, &coefsq[offset], &coefsq_norm[offset]);
                    bg_skip(is, WID_G2);
                    diff -= WID_G2;
                }
            }
            else if (m < SFM_N) {
                if (diff >= WID_G3) {
                    R[m] = 1;
                    offset = NUMC_G1G2 + (m - SFM_G1G2) * WID_G3;
                    dprocnf(&ycof[offset], is, ynrm[m], NB_VECT3, &coefsq[offset], &coefsq_norm[offset]);
                    bg_skip(is, WID_G3);
                    diff -= WID_G3;
                }
            }
            else {
                if (diff >= WID_GX) {
                    R[m] = 1;
                    offset = NUMC_N + (m - SFM_N) * WID_GX;
                    dprocnf(&ycof[offset], is, ynrm[m], NB_VECTX, &coefsq[offset], &coefsq_norm[offset]);
                    bg_skip(is, WID_GX);
                    diff -= WID_GX;
                }
            }
        }
    }

    //bg_seek(is, pos); // OG: bug? see flvqdec end
    return;
}

/*--------------------------------------------------------------------------*/
/*  Finding the closest point of lattice                                    */
/*--------------------------------------------------------------------------*/
/*  float       *x  (i)  arbitrary vector in Qv                             */
/*  short       *C  (o)  point of lattice                                   */
/*  Word16      R   (i)  number of bits per coefficient                     */
/*--------------------------------------------------------------------------*/
static void codesearch(short *x, short *C, short R) {
    short i, j, sum;
    short e[8], em, temp;

    sum = 0;
    for (i = 0; i < 8; i++) {
        temp = x[i] & FacLVQ2Mask[R];
        C[i] = x[i] >> FacLVQ2Qv[R]; // TO-DO EXTRA: possible negative shift? (UB)
        if ((temp > FacLVQ2HalfQv[R]) ||
            ((temp == FacLVQ2HalfQv[R]) && (x[i] < 0))) {
            C[i] += 1;
        }
        sum += C[i];
    }
    if (sum & 1) {
        j = 0;
        em = 0;
        for (i = 0; i < 8; i++) {
            temp = C[i] << FacLVQ2Qv[R]; // TO-DO EXTRA: possible negative shift? (UB)
            e[i] = x[i] - temp;
            temp = e[i];
            if (e[i] < 0) {
                temp = -e[i];
            }
            if (em < temp) {
                em = temp;
                j = i;
            }
        }
        if (e[j] >= 0) {
            C[j] += 1;
        }
        else {
            C[j] -= 1;
        }
    }

    return;
}

/*--------------------------------------------------------------------------*/
/*  Finding a codevector from its index                                     */
/*--------------------------------------------------------------------------*/
/*  short     *k  (i)   index of the codevector                             */
/*  short     *y  (o)   codevector                                          */
/*  short     R   (i)   number of bits per coefficient                      */
/*--------------------------------------------------------------------------*/
static void idx2code(short *k, short *y, short R) {
    short i, m, tmp;
    short v[8], z[8];

    tmp = FacLVQ2Qv[R] - R;
    m = k[0] << 1;
    for (i = 1; i < 8; i++) {
        m += k[i];
    }
    if (tmp < 0) {
        z[0] = m >> (-tmp); // TO-DO EXTRA: possible negative shift? (UB)
    }
    else {
        z[0] = m << tmp;
    }
    for (i = 1; i < 8; i++) {
        if (tmp < 0) {
            z[i] = k[i] >> (-tmp);
        }
        else {
            z[i] = k[i] << tmp;
        }
    }
    codesearch(z, v, R);
    y[0] = m - (v[0] << R); // TO-DO EXTRA: possible negative shift? (UB)
    for (i = 1; i < 8; i++) {
        y[i] = k[i] - (v[i] << R); // TO-DO EXTRA: possible negative shift? (UB)
    }

    return;
}

/*--------------------------------------------------------------------------*/
/*  Vector de-quantization for normalized MLT coefficients                  */
/*--------------------------------------------------------------------------*/
/*  short     *y           (i)   indices of the selected codevectors        */
/*  short     *idxnrm      (i)   indices of quantized norms                 */
/*  short     *R           (i)   number of bits per coefficient             */
/*  short     N1           (i)   beginning sub-vector's number in the group */
/*  short     N2           (i)   ending sub-vector's number in the group    */
/*  short     L            (i)   number of coefficients in each sub-vector  */
/*  float     *coefs       (o)   MLT coefficients                           */
/*  float     *coefs_norm  (o)   normalized MLT coefficients                */
/*--------------------------------------------------------------------------*/
static void dqcoefs(short *y, short *idxnrm, short *R, short N1, short N2,
                    short L, float *coefs, float *coefs_norm) {
    short i, j, n, v, rv;
    short nb_vecs, pre_idx;
    short x[8]; // OG: *not* arena alloc'd unlike other big arrays
    float normq, factor;
    short *pidx;
    float *pcoefs, *pcoefs_norm;

    pidx = y;
    pcoefs = coefs;
    pcoefs_norm = coefs_norm;
    nb_vecs = L >> 3;
    for (n = N1; n < N2; n++) {
        normq = dicn[idxnrm[n]];
        v = R[n];
        if (v > 1) {
            rv = RV[v];
            factor = FCT_LVQ2f / (float)rv;
            for (i = 0; i < nb_vecs; i++) {
                idx2code(pidx, x, v);
                for (j = 0; j < 8; j++) {
                    *pcoefs_norm = x[j] * factor + OFFSETf;
                    *pcoefs++ = (*pcoefs_norm++) * normq;
                }
                pidx += 8;
            }
        }
        else if (v == 1) {
            pre_idx = MAX16B;
            for (i = 0; i < nb_vecs; i++) {
                if ((pre_idx < 128) && (*pidx < 16)) {
                    for (j = 0; j < 8; j++) {
                        *pcoefs_norm = OFFSETf;
                        *pcoefs++ = (*pcoefs_norm++) * normq;
                    }
                }
                else {
                    for (j = 0; j < 8; j++) {
                        *pcoefs_norm = (float)dic4[*pidx][j] / FCT_LVQ1f + OFFSETf;
                        *pcoefs++ = (*pcoefs_norm++) * normq;
                    }
                }
                pre_idx = *pidx;
                pidx += 8;
            }
        }
        else {
            for (i = 0; i < L; i++) {
                *pcoefs_norm++ = 0.0;
                *pcoefs++ = 0.0;
            }
            pidx += L;
        }
    }

    return;
}

/*--------------------------------------------------------------------------*/
/*  Huffman decoding for LVQ2 quantization indices                          */
/*--------------------------------------------------------------------------*/
/*  short       *bitstream  (i)    Huffman code                             */
/*  short       N           (i)    number of coefficients                   */
/*  short       *index      (o)    LVQ2 quantization indices                */
/*--------------------------------------------------------------------------*/
static void hdec2blvq(bitstream_t* is, short N, short *index) {
    short i;
    short temp;
    short *pidx;

    int pos = bg_tell(is); //OG: skips handled by caller
    pidx = index;

    for (i = 0; i < N; i++) {
        temp = 0;
        if (bg_read1(is) == 1) {
            temp = 3;
            if (bg_read1(is) == 1) {
                temp = 1;
                if (bg_read1(is) == 1) {
                    temp++;
                }
            }
        }
        *pidx++ = temp;
    }

    bg_seek(is, pos);
    return;
}

/*--------------------------------------------------------------------------*/
/*  Huffman decoding for indices of 3-bit LVQ2                              */
/*--------------------------------------------------------------------------*/
/*  short       *bitstream  (i)    Huffman code                             */
/*  short       N           (i)    number of coefficients                   */
/*  short       *index      (o)    LVQ2 quantization indices                */
/*--------------------------------------------------------------------------*/
static void hdec3blvq(bitstream_t* is, short N, short *index) {
    short i, j, k, m;
    short temp;
    short *pidx;

    int pos = bg_tell(is); //OG: skips handled by caller
    pidx = index;

    for (i = 0; i < N; i++) {
        j = 0;
        if (bg_read1(is) == 1) {
            j = 1;
        }
        k = j * 2;
        if (bg_read1(is) == 1) {
            k++;
        }
        temp = j * 4 + k;
        if (k == 2) {
            j = 0;
            if (bg_read1(is) == 1) {
                j = 1;
            }
            m = j * 2;
            if (bg_read1(is) == 1) {
                m++;
            }
            temp = j * 2 + m + 1;
            if (m == 0) {
                temp = 3;
                if (bg_read1(is) == 1) {
                    temp++;
                }
            }
        }
        *pidx++ = temp;
    }

    bg_seek(is, pos);
    return;
}

/*--------------------------------------------------------------------------*/
/*  Huffman decoding for indices of 4-bit LVQ2                              */
/*--------------------------------------------------------------------------*/
/*  short       *bitstream  (i)    Huffman code                             */
/*  short       N           (i)    number of coefficients                   */
/*  short       *index      (o)    LVQ2 quantization indices                */
/*--------------------------------------------------------------------------*/
static void hdec4blvq(bitstream_t* is, short N, short *index) {
    short i, j, k, m;
    short temp;
    short *pidx;

    int pos = bg_tell(is); //OG: skips handled by caller
    pidx = index;

    for (i = 0; i < N; i++) {
        k = 0;
        if (bg_read1(is) == 1) {
            k = 2;
        }
        if (bg_read1(is) == 1) {
            k++;
        }
        temp = 0;
        if (k != 0) {
            j = 0;
            if (bg_read1(is) == 1) {
                j = 1;
            }
            temp = 1;
            if (j != 0) {
                temp = 15;
            }
            if (k != 3) {
                m = j * 2;
                if (bg_read1(is) == 1) {
                    m++;
                }
                temp = m;
                if (j == 0) {
                    temp = m + 13;
                }
                if (k != 1) {
                    m = m * 2;
                    if (bg_read1(is) == 1) {
                        m++;
                    }
                    temp = m;
                    if (j == 0) {
                        temp = m + 9;
                    }
                    if (m == 7) {
                        temp = m;
                        if (bg_read1(is) == 1) {
                            temp = m + 1;
                        }
                    }
                }
            }
        }
        *pidx++ = temp;
    }

    bg_seek(is, pos);
    return;
}

/*--------------------------------------------------------------------------*/
/*  Huffman decoding for indices of 5-bit LVQ2                              */
/*--------------------------------------------------------------------------*/
/*  short       *bitstream  (i)    Huffman code                             */
/*  short       N           (i)    number of coefficients                   */
/*  short       *index      (o)    LVQ2 quantization indices                */
/*--------------------------------------------------------------------------*/
static void hdec5blvq(bitstream_t* is, short N, short *index) {
    short i, j, k, m, n;
    short temp;
    short *pidx;

    int pos = bg_tell(is); //OG: skips handled by caller
    pidx = index;

    for (i = 0; i < N; i++) {
        n = 0;
        if (bg_read1(is) == 1) {
            n = 2;
        }
        if (bg_read1(is) == 1) {
            n++;
        }
        temp = 0;
        if (n != 0) {
            j = 0;
            if (bg_read1(is) == 1) {
                j = 1;
            }
            temp = 1;
            if (j != 0) {
                temp = 31;
            }
            if (n != 1) {
                k = 0;
                if (bg_read1(is) == 1) {
                    k = 1;
                }
                if (n == 2) {
                    temp = 2;
                    if (k != 0) {
                        temp = 30;
                    }
                    if (j != 0) {
                        temp = 3;
                        if (bg_read1(is) == 1) {
                            temp++;
                        }
                        if (k != 0) {
                            temp += 25;
                        }
                    }
                }
                else {
                    m = 0;
                    if (bg_read1(is) == 1) {
                        m = 2;
                    }
                    if (bg_read1(is) == 1) {
                        m++;
                    }
                    temp = m + 5;
                    if (k != 0) {
                        temp += 19;
                    }
                    if (j != 0) {
                        m = k * 4 + m;
                        temp = 23;
                        if (m != 7) {
                            m *= 2;
                            if (bg_read1(is) == 1) {
                                m++;
                            }
                            temp = m + 9;
                        }
                    }
                }
            }
        }
        *pidx++ = temp;
    }

    bg_seek(is, pos);
    return;
}

/*--------------------------------------------------------------------------*/
/*  Huffman decoding and unpacking indices for quantized coefficients       */
/*--------------------------------------------------------------------------*/
/*  short     *R      (i)   number of bits per coefficinet                  */
/*  short     *pbits  (i)   pointer to bitstream                            */
/*  short     flag    (i)   Huffman code flag                               */
/*  short     rv      (i)   offset for index of quantized coefficients      */
/*  short     N1      (i)   beginning sub-vector's number in the group      */
/*  short     N2      (i)   ending sub-vector's number in the group         */
/*  short     L       (o)   number of coefficients in each sub-vector       */
/*  short     *y      (o)   indices of the selected codevectors             */
/*--------------------------------------------------------------------------*/
/*  short     return  (o)   length of Huffman code                          */
/*--------------------------------------------------------------------------*/
static short unpackc(short *R, bitstream_t* is, short flag, short rv, short N1, short N2, short L, short *y) {
    short i, j, k, n, r, v, hcode_l, offset, sum;
    short nb_vecs, length;

    int pos = bg_tell(is); //OG: skips handled by caller

    nb_vecs = L >> 3;
    length = 0;
    if (flag == NOHUFCODE) {
        for (n = N1; n < N2; n++) {
            v = R[n];
            if (v > 1) {
                bits2idxc(is, L, v, &y[rv]);
                sum = v * L;
                bg_skip(is, sum);
                length += sum;
            }
            else if (v == 1) {
                k = rv;
                for (i = 0; i < nb_vecs; i++) {
                    bits2idxc(is, 8, 1, &y[k]);
                    bg_skip(is, 8);
                    k += 8;
                }
                length += L;
            }
            rv += L;
        }
    }
    else {
        r = 0;
        hcode_l = 0;
        for (n = N1; n < N2; n++) {
            v = R[n];
            if (v > QBIT_MAX1) {
                bits2idxc(is, L, v, &y[rv]);
                sum = v * L;
                bg_skip(is, sum);
                r += sum;
            }
            else if (v > 1) {
                if (v == 2) {
                    hdec2blvq(is, L, &y[rv]);
                }
                else if (v == 3) {
                    hdec3blvq(is, L, &y[rv]);
                }
                else if (v == 4) {
                    hdec4blvq(is, L, &y[rv]);
                }
                else {
                    hdec5blvq(is, L, &y[rv]);
                }
                offset = huffoffset[v];
                for (i = 0; i < L; i++) {
                    k = rv + i;
                    j = offset + y[k];
                    hcode_l += huffsizc[j];
                    bg_skip(is, huffsizc[j]);
                }
            }
            else if (v == 1) {
                k = rv;
                for (i = 0; i < nb_vecs; i++) {
                    bits2idxc(is, 8, 1, &y[k]);
                    bg_skip(is, 8);
                    k += 8;
                }
                r += L;
            }
            rv += L;
        }
        length = hcode_l + r;
    }

    bg_seek(is, pos);
    return (length);
}

/*--------------------------------------------------------------------------*/
/*  Adaptive bit allocation for 20kHz audio codec                           */
/*--------------------------------------------------------------------------*/
/*  short       *y     (i)    reordered norm of sub-vectors                 */
/*  short       *idx   (i)    reordered sub-vector indices                  */
/*  short       sum    (i)    number of available bits                      */
/*  short       N      (i)    number of norms                               */
/*  short       M      (i)    maximum number of bits per dimension          */
/*  short       *r     (o)    bit-allacation vector                         */
/*--------------------------------------------------------------------------*/
static void bitalloc(short *y, short *idx, short sum, short N, short M, short *r) {
    short i, j, k, n, m, v, im;
    short diff, temp;

    im = 1;
    diff = sum;
    n = sum >> 3;
    for (i = 0; i < n; i++) {
        k = 0;
        temp = y[0];
        for (m = 1; m <= im; m++) {

            // EXTRA: don't read past 'y' (wnorm), not sure if actually possible but hard to assess
            if (m >= NB_SFM)
                break;

            if (temp < y[m]) {
                temp = y[m];
                k = m;
            }
        }
        if (k == im) {
            im++;
        }
        j = idx[k];
        if ((sum >= sfmsize[j]) && (r[j] < M)) {
            y[k] -= 2;
            r[j]++;
            if (r[j] >= M) {
                y[k] = MIN16B;
            }
            sum -= sfmsize[j];
        }
        else {
            y[k] = MIN16B;
            k++;
            if (k == im) {
                im++;
            }
        }
        if ((sum < WID_G1) || (diff == sum)) {
            break;
        }
        diff = sum;
        v = N - 2;
        if (k > v) {
            for (i = 0; i < N; i++) {
                if (y[i] > MIN16B) {
                    im = i + 1;
                    break;
                }
            }
        }
    }
    if (sum >= WID_G2) {
        for (i = 0; i < N; i++) {
            j = idx[i];
            if ((j >= SFM_G1) && (j < SFM_G1G2) && (r[j] == 0)) {
                r[j] = 1;
                sum -= WID_G2;
                if (sum < WID_G2) {
                    break;
                }
            }
        }
    }
    if (sum >= WID_G2) {
        for (i = 0; i < N; i++) {
            j = idx[i];
            if ((j >= SFM_G1) && (j < SFM_G1G2) && (r[j] == 1)) {
                r[j] = 2;
                sum -= WID_G2;
                if (sum < WID_G2) {
                    break;
                }
            }
        }
    }
    if (sum >= WID_G1) {
        for (i = 0; i < N; i++) {
            j = idx[i];
            if ((j < SFM_G1) && (r[j] == 0)) {
                r[j] = 1;
                sum -= WID_G1;
                if (sum < WID_G1) {
                    break;
                }
            }
        }
    }
    if (sum >= WID_G1) {
        for (i = 0; i < N; i++) {
            j = idx[i];
            if ((j < SFM_G1) && (r[j] == 1)) {
                r[j] = 2;
                sum -= WID_G1;
                if (sum < WID_G1) {
                    break;
                }
            }
        }
    }

    return;
}

/*--------------------------------------------------------------------------*/
/*  Rearrange a vector in decreasing order                                  */
/*--------------------------------------------------------------------------*/
/*  short     *y    (i/o) vector to rearrange                               */
/*  short     N     (i)   dimensions                                        */
/*  short     *idx  (o)   reordered vector index                            */
/*--------------------------------------------------------------------------*/
static void reordvct(short *y, short N, short *idx) {
    short i, j, k, n, im, temp;

    n = N - 1;
    for (i = 0; i < n; i++) {
        im = i;
        k = i + 1;
        for (j = k; j < N; j++) {
            temp = y[im] - y[j];
            if (temp < 0) {
                im = j;
            }
        }
        temp = y[i];
        y[i] = y[im];
        y[im] = temp;
        j = idx[i];
        idx[i] = idx[im];
        idx[im] = j;
    }

    return;
}

/*--------------------------------------------------------------------------*/
/*  Map sub-vectors to pbands                                               */
/*--------------------------------------------------------------------------*/
/*  short    spe[]    (i)   sub-vectors                                     */
/*  short    spe2q[]  (o)   pbands                                          */
/*--------------------------------------------------------------------------*/
static void sfm2mqb(short spe[], short spe2q[]) {
    short tmp, i;

    /* short groups */
    spe2q[0] = spe[0] + 3;
    spe2q[1] = spe[1] + 3;
    spe2q[2] = spe[2] + 3;
    spe2q[3] = spe[3] + 3;
    spe2q[4] = spe[4] + 3;
    spe2q[5] = spe[5] + 3;
    spe2q[6] = spe[6] + 3;
    spe2q[7] = spe[7] + 3;
    spe2q[8] = spe[8] + 3;
    spe2q[9] = spe[9] + 3;

    spe2q[10] = ((spe[10] + spe[11]) >> 1) + 4;
    spe2q[11] = ((spe[12] + spe[13]) >> 1) + 4;
    spe2q[12] = ((spe[14] + spe[15]) >> 1) + 4;

    spe2q[13] = ((spe[16] + spe[17]) >> 1) + 5;
    spe2q[14] = ((spe[18] + spe[19]) >> 1) + 5;

    tmp = 0;
    for (i = 20; i < 24; i++) {
        tmp += spe[i];
    }
    spe2q[15] = (short)(((int)tmp * 8192L) >> 15) + 6;

    tmp = 0;
    for (i = 24; i < 27; i++) {
        tmp += spe[i];
    }
    spe2q[16] = (short)(((int)tmp * 10923L) >> 15) + 6;

    tmp = 0;
    for (i = 27; i < 30; i++) {
        tmp += spe[i];
    }
    spe2q[17] = (short)(((int)tmp * 10923L) >> 15) + 6;

    tmp = 0;
    for (i = 30; i < 35; i++) {
        tmp += spe[i];
    }
    spe2q[18] = (short)(((int)tmp * 6553L) >> 15) + 7;

    tmp = 0;
    for (i = 35; i < 44; i++) {
        tmp += spe[i];
    }
    spe2q[19] = (short)(((int)tmp * 3641L) >> 15) + 8;
}

/*--------------------------------------------------------------------------*/
/*  Map pbands to sub-vectors                                               */
/*--------------------------------------------------------------------------*/
/*  short    spe2q[]  (i)   pbands                                          */
/*  short    spe[]    (o)   sub-vectors                                     */
/*--------------------------------------------------------------------------*/
static void mqb2sfm(short spe2q[], short spe[]) {
    short i;

    spe[0] = spe2q[0];
    spe[1] = spe2q[1];
    spe[2] = spe2q[2];
    spe[3] = spe2q[3];
    spe[4] = spe2q[4];
    spe[5] = spe2q[5];
    spe[6] = spe2q[6];
    spe[7] = spe2q[7];
    spe[8] = spe2q[8];
    spe[9] = spe2q[9];

    spe[10] = spe2q[10];
    spe[11] = spe2q[10];

    spe[12] = spe2q[11];
    spe[13] = spe2q[11];

    spe[14] = spe2q[12];
    spe[15] = spe2q[12];

    spe[16] = spe2q[13];
    spe[17] = spe2q[13];

    spe[18] = spe2q[14];
    spe[19] = spe2q[14];

    for (i = 20; i < 24; i++) {
        spe[i] = spe2q[15];
    }

    for (i = 24; i < 27; i++) {
        spe[i] = spe2q[16];
    }

    for (i = 27; i < 30; i++) {
        spe[i] = spe2q[17];
    }

    for (i = 30; i < 35; i++) {
        spe[i] = spe2q[18];
    }

    for (i = 35; i < 44; i++) {
        spe[i] = spe2q[19];
    }
}

/*--------------------------------------------------------------------------*/
/*  Calculate the quantization weights                                      */
/*--------------------------------------------------------------------------*/
/*  short    normqlg2[]    (i)   quantized norms                            */
/*  short    wnorm[]       (o)   weighted norm                              */
/*  short    is_transient  (i)   transient flag                             */
/*--------------------------------------------------------------------------*/
static void map_quant_weight(short normqlg2[], short wnorm[], short is_transient) {
    short sfm;
    short tmp16;
    short spe2q[NUM_MAP_BANDS];
    short spe[NB_SFM];

    short spe2q_max;
    short spe2q_min;
    short norm_max;
    short shift;
    short sum;
    short k;

    if (is_transient) {
        for (sfm = 0; sfm < NB_SFM; sfm += 4) {
            sum = 0;
            for (k = 0; k < 4; k++) {
                sum = sum + normqlg2[sfm + k];
            }
            sum = sum >> 2;

            for (k = 0; k < 4; k++) {
                spe[sfm + k] = sum;
            }
        }
    }
    else {
        for (sfm = 0; sfm < NB_SFM; sfm++) {
            spe[sfm] = normqlg2[sfm];
        }
    }

    sfm2mqb(spe, spe2q);

    for (sfm = 0; sfm < NUM_MAP_BANDS; sfm++) {
        spe2q[sfm] = spe2q[sfm] - 10;
    }

    /* spectral smoothing */
    for (sfm = 1; sfm < NUM_MAP_BANDS; sfm++) {
        tmp16 = spe2q[sfm - 1] - 4;
        if (spe2q[sfm] < tmp16) {
            spe2q[sfm] = tmp16;
        }
    }

    for (sfm = NUM_MAP_BANDS - 2; sfm >= 0; sfm--) {
        tmp16 = spe2q[sfm + 1] - 8;
        if (spe2q[sfm] < tmp16) {
            spe2q[sfm] = tmp16;
        }
    }
    for (sfm = 0; sfm < NUM_MAP_BANDS; sfm++) {
        if (spe2q[sfm] < sfm_a[sfm]) {
            spe2q[sfm] = sfm_a[sfm];
        }
    }

    /* Saturate by the Absolute Threshold of Hearing */
    spe2q_max = MIN16B;
    spe2q_min = MAX16B;

    for (sfm = 0; sfm < NUM_MAP_BANDS; sfm++) {
        spe2q[sfm] = sfm_width[sfm] - spe2q[sfm];
        if (spe2q_max < spe2q[sfm]) {
            spe2q_max = spe2q[sfm];
        }
        if (spe2q_min > spe2q[sfm]) {
            spe2q_min = spe2q[sfm];
        }
    }

    for (sfm = 0; sfm < NUM_MAP_BANDS; sfm++) {
        spe2q[sfm] = spe2q[sfm] - spe2q_min;
    }

    spe2q_max = spe2q_max - spe2q_min;

    if (spe2q_max == 0) {
        norm_max = 0;
    }
    else {
        if (spe2q_max < 0) {
            spe2q_max = ~spe2q_max;
        }
        for (norm_max = 0; spe2q_max < 0x4000; norm_max++) {
            spe2q_max <<= 1;
        }
    }

    shift = norm_max - 13;

    for (sfm = 0; sfm < NUM_MAP_BANDS; sfm++) {
        if (shift < 0) {
            spe2q[sfm] = spe2q[sfm] >> (-shift);
        }
        else {
            spe2q[sfm] = spe2q[sfm] << shift;
        }
    }

    mqb2sfm(spe2q, spe);

    if (is_transient) {
        for (sfm = 0; sfm < NB_SFM; sfm += 4) {
            sum = 0;
            for (k = 0; k < 4; k++) {
                sum = sum + spe[sfm + k];
            }
            sum = sum >> 2;

            for (k = 0; k < 4; k++) {
                spe[sfm + k] = sum;
            }
        }
    }

    /* modify the norms for bit-allocation */
    for (sfm = 0; sfm < NB_SFM; sfm++) {
        wnorm[sfm] = spe[sfm] + normqlg2[sfm];
    }
}

/*--------------------------------------------------------------------------*/
/*  Recover reordered quantization indices and norms                        */
/*--------------------------------------------------------------------------*/
/*  short     *idxbuf    (i)   reordered quantization indices               */
/*  short     *ynrm      (o)   recovered quantization indices               */
/*  short     *normqlg2  (o)   recovered quantized norms                    */
/*--------------------------------------------------------------------------*/
static void recovernorm(short *idxbuf, short *ynrm, short *normqlg2) {
    short i;
    short *pidx, *pnormq;

    for (i = 0; i < 2; i++) {
        pidx = ynrm + i;
        pnormq = normqlg2 + i;
        *pidx = idxbuf[i];
        *pnormq = dicnlg2[*pidx];
        pidx += 2;
        pnormq += 2;
        *pidx = idxbuf[21 - i];
        *pnormq = dicnlg2[*pidx];
        pidx += 2;
        pnormq += 2;
        *pidx = idxbuf[22 + i];
        *pnormq = dicnlg2[*pidx];
        pidx += 2;
        pnormq += 2;
        *pidx = idxbuf[43 - i];
        *pnormq = dicnlg2[*pidx];
        pidx += 2;
        pnormq += 2;
        *pidx = idxbuf[i + 2];
        *pnormq = dicnlg2[*pidx];
        pidx += 2;
        pnormq += 2;
        *pidx = idxbuf[19 - i];
        *pnormq = dicnlg2[*pidx];
        pidx += 2;
        pnormq += 2;
        *pidx = idxbuf[24 + i];
        *pnormq = dicnlg2[*pidx];
        pidx += 2;
        pnormq += 2;
        *pidx = idxbuf[41 - i];
        *pnormq = dicnlg2[*pidx];
    }

    pidx = ynrm + 16;
    pnormq = normqlg2 + 16;
    for (i = 4; i < (NB_SFM / 4); i++) {
        *pidx = idxbuf[i];
        *pnormq++ = dicnlg2[*pidx++];
        *pidx = idxbuf[21 - i];
        *pnormq++ = dicnlg2[*pidx++];
        *pidx = idxbuf[22 + i];
        *pnormq++ = dicnlg2[*pidx++];
        *pidx = idxbuf[43 - i];
        *pnormq++ = dicnlg2[*pidx++];
    }

    return;
}

/*--------------------------------------------------------------------------*/
/*  Huffman decoding for indices of quantized norms                         */
/*--------------------------------------------------------------------------*/
/*  short       *bitstream  (i)    Huffman code                             */
/*  short       N           (i)    number of norms                          */
/*  short       *index      (o)    indices of quantized norms               */
/*--------------------------------------------------------------------------*/
static void hdecnrm(bitstream_t* is, short N, short *index) {
    short i, j, k, n, m;
    short temp;
    short *pidx;

    int pos = bg_tell(is); //OG: skips handled by caller

    pidx = index;

    m = N - 1;
    for (i = 0; i < m; i++) {
        j = 0;
        k = 0;
        if (bg_read1(is) == 1) {
            j = 1;
        }
        if (bg_read1(is) == 1) {
            k = 1;
        }
        n = j * 2 + k;
        j = j * 4;
        temp = 16 + n - j;
        if (bg_read1(is) == 1) {
            temp = 12 + n + j;
            if (bg_read1(is) == 1) {
                j = 0;
                if (bg_read1(is) == 1) {
                    j = 1;
                }
                temp = 8 + n;
                if (j != 0) {
                    temp += 12;
                }
                if (bg_read1(is) == 1) {
                    temp = n;
                    if (bg_read1(is) == 1) {
                        temp = n + 4;
                    }
                    if (j != 0) {
                        temp += 24;
                    }
                }
            }
        }
        *pidx++ = temp;
    }

    bg_seek(is, pos);
    return;
}

/*--------------------------------------------------------------------------*/
/*  Decoding of Fast Lattice Vector Quantization (FLVQ)                     */
/*--------------------------------------------------------------------------*/
/*  short     **bitstream     (i)   bit-stream vector                       */
/*  float     *coefsq         (o)   MLT coefficient vector                  */
/*  float     *coefsq_norm    (o)   normalized MLT coefficient vector       */
/*  short     R               (o)   bit-allocation vector                   */
/*  short     NumSpectumBits  (i)   number of available bits                */
/*  short     *ynrm           (o)   norm quantization index vector          */
/*  short     is_transient    (i)   transient flag                          */
/*--------------------------------------------------------------------------*/
void flvqdec(bitstream_t* is, float *coefsq, float *coefsq_norm, short *R,
             short NumSpectumBits, short *ynrm, short is_transient) {
    short i, j, k, v, nb_sfm;
    short diff;
    short hcode_l, FlagL, FlagN, FlagC;
    short idx[NB_SFM], normqlg2[NB_SFM], wnorm[NB_SFM], idxbuf[NB_SFM]; // OG: arena alloc
    short ycof[STOP_BAND];

    //int pos = bg_tell(is); // OG: bug? see end

    /*** Unpacking bit stream to flags ***/
    FlagL = 0;
    if (bg_read1(is) == 1) {
        FlagL = 1;
    }
    FlagN = 0;
    if (bg_read1(is) == 1) {
        FlagN = 1;
    }
    FlagC = 0;
    if (bg_read1(is) == 1) {
        FlagC = 1;
    }

    /*** Unpacking bit stream and Huffman decoding for indices of quantized
     * norms ***/
    if (FlagL == NOALLGROUPS) {
        nb_sfm = SFM_N;
    }
    else {
        nb_sfm = NB_SFM;
    }
    bits2idxn(is, NORM0_BITS, ynrm);
    bg_skip(is, NORM0_BITS);
    if (FlagN == HUFCODE) {
        hdecnrm(is, NB_SFM, &ynrm[1]);
        hcode_l = 0;
        for (i = 1; i < NB_SFM; i++) {
            hcode_l += huffsizn[ynrm[i]];
        }
        bg_skip(is, hcode_l);
    }
    else {
        for (i = 1; i < NB_SFM; i++) {
            // OG: inline'd
            bits2idxn(is, NORMI_BITS, &ynrm[i]);
            bg_skip(is, NORMI_BITS);
        }
        hcode_l = NUMNRMIBITS;
    }

    /*** De-quantization of norms ***/
    /* First sub-frame */
    normqlg2[0] = dicnlg2[ynrm[0]];
    /* Other sub-frames */
    if (is_transient) {
        /* Recover quantization indices and quantized norms */
        idxbuf[0] = ynrm[0];
        for (i = 1; i < NB_SFM; i++) {
            idxbuf[i] = ynrm[i] + idxbuf[i - 1] - 15;

            // EXTRA: clamp as this seems possible on bad data 
            if (idxbuf[i] < 0)
                idxbuf[i] = 0;
            else if (idxbuf[i] > NORM_LIMIT)
                idxbuf[i] = NORM_LIMIT;
        }
        recovernorm(idxbuf, ynrm, normqlg2);
    }
    else {
        for (i = 1; i < NB_SFM; i++) {
            j = i - 1;
            k = ynrm[j] - 15;
            ynrm[i] += k;

            // EXTRA: clamp, same as above
            if (ynrm[i] < 0)
                ynrm[i] = 0;
            if (ynrm[i] > NORM_LIMIT)
                ynrm[i] = NORM_LIMIT;

            normqlg2[i] = dicnlg2[ynrm[i]];
        }
    }

    /*** Bit allocation ***/
    for (i = 0; i < nb_sfm; i++) {
        idx[i] = i;
    }
    map_quant_weight(normqlg2, wnorm, is_transient);
    reordvct(wnorm, nb_sfm, idx);
    for (i = 0; i < NB_SFM; i++) {
        R[i] = 0;
    }
    diff = NumSpectumBits - FLAGS_BITS - NORM0_BITS;
    v = diff - hcode_l;
    diff = v;
    bitalloc(wnorm, idx, diff, nb_sfm, QBIT_MAX2, R);

    /*** Unpacking bit stream and Huffman decoding for indices of quantized
     * coefficients ***/
    /* First group */
    hcode_l = unpackc(R, is, FlagC, 0, 0, SFM_G1, WID_G1, ycof);
    bg_skip(is, hcode_l);

    /* Second group */
    k = unpackc(R, is, FlagC, NUMC_G1, SFM_G1, SFM_G1G2, WID_G2, ycof);
    bg_skip(is, k);
    hcode_l += k;

    /* Third group */
    k = unpackc(R, is, FlagC, NUMC_G1G2, SFM_G1G2, SFM_N, WID_G3, ycof);
    bg_skip(is, k);
    hcode_l += k;

    /* Forth group */
    if (nb_sfm > SFM_N) {
        k = unpackc(R, is, FlagC, NUMC_N, SFM_N, NB_SFM, WID_GX, ycof);
        bg_skip(is, k);
        hcode_l += k;
    }
    diff = v - hcode_l;

    /*** Lattice Vector De-quantization for normalized MLT coefficients ***/
    /* First group */
    dqcoefs(&ycof[0], ynrm, R, 0, SFM_G1, WID_G1, &coefsq[0], &coefsq_norm[0]);

    /* Second group */
    dqcoefs(&ycof[NUMC_G1], ynrm, R, SFM_G1, SFM_G1G2, WID_G2, &coefsq[NUMC_G1], &coefsq_norm[NUMC_G1]);

    /* Third group */
    dqcoefs(&ycof[NUMC_G1G2], ynrm, R, SFM_G1G2, SFM_N, WID_G3, &coefsq[NUMC_G1G2], &coefsq_norm[NUMC_G1G2]);

    /* Forth group */
    dqcoefs(&ycof[NUMC_N], ynrm, R, SFM_N, NB_SFM, WID_GX, &coefsq[NUMC_N], &coefsq_norm[NUMC_N]);

    /*** Processing for noise-filling sub-vectors ***/
    dprocnobitsbfm(R, idx, ynrm, ycof, is, coefsq, coefsq_norm, nb_sfm, diff);

    // OG: actually seeks back to the start 'pos' here, something also done in dprocnobitsbfm.
    // This seems like a new bug vs ref., but since unpacking is done at this point it shouldn't matter
    //bg_seek(is, pos);
    return;
}
