/* Originally from nwatowav.cc (2007.7.28 version) by jagarl.
 * - http://www.creator.club.ne.jp/~jagarl/
 *
 * Converted to .c by hcs (redone as a lib without RIFF/main handling), some cleanup by bnnm.
 *
 * nwa format (abridged from the original)
 *   NWA Header
 *   data offset index
 *   data block<0>
 *   data block<1>
 *   ...
 *   data block<N>
 *
 * - NWA header: 0x2c with nwa info (channels, compression level, etc), no magic number
 * - data offset index: pointers to data blocks
 * - data block: variable sized DPCM blocks to fixed size PCM (a,b,c compresses to (a),b-a,c-b),
 *   DPCM codes use variable bits. Usually for 16-bit PCM ends ups using 6-8 bits.
 * - Block format:
 *   - mono: initial PCM (8 or 16-bit) then bitstream
 *   - stereo: initial PCM for left + right channel then bitstream
 *   Differential accuracy isn't high so initial PCM is used to correct data in each block (?)
 * - bitstream: Roughly each code has an 'exponent' (2 bits) + 'mantissa' (variable bits).
 *   Depending on compression level + type it configures final shift value and matissa bits.
 *   There is a run length encoding mode in some cases (Tomoyo After voice files).
 *   Bitstream bytes follow little endian.
 *   (some examples here in the original, see decoder).
 *
 * Original copyright:
 */

/*
 * Copyright 2001-2007  jagarl / Kazunori Ueno <jagarl@creator.club.ne.jp>
 * All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 *
 * このプログラムの作者は jagarl です。
 *
 * このプログラム、及びコンパイルによって生成したバイナリは
 * プログラムを変更する、しないにかかわらず再配布可能です。
 * その際、上記 Copyright 表示を保持するなどの条件は課しま
 * せん。対応が面倒なのでバグ報告を除き、メールで連絡をする
 * などの必要もありません。ソースの一部を流用することを含め、
 * ご自由にお使いください。
 *
 * THIS SOFTWARE IS PROVIDED BY KAZUNORI 'jagarl' UENO ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL KAZUNORI UENO BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * 
 */

#include <stdlib.h>
#include "nwa_decoder.h"


//NWAInfo::UseRunLength
static int is_use_runlength(NWAData* nwa) {
    if (nwa->channels == 2 && nwa->bps == 16 && nwa->complevel == 2) {
        return 0; /* sw2 */
    }

    if (nwa->complevel == 5) {
        if (nwa->channels == 2)
            return 0; // BGM*.nwa in Little Busters!
        return 1; // Tomoyo After (.nwk koe file)
    }

    return 0;
}

NWAData* nwalib_open(STREAMFILE* sf) {
    uint8_t header[0x2c] = {0};
    int i;
    NWAData* const nwa = malloc(sizeof(NWAData));
    if (!nwa) goto fail;

    //NWAData::ReadHeader

    read_streamfile(header, 0x00, sizeof(header), sf);
    nwa->channels = get_s16le(header+0x00);
    nwa->bps = get_s16le(header+0x02);
    nwa->freq = get_s32le(header+0x04);
    nwa->complevel = get_s32le(header+0x08);
    nwa->dummy = get_s32le(header+0x0c);
    nwa->blocks = get_s32le(header+0x10);
    nwa->datasize = get_s32le(header+0x14);
    nwa->compdatasize = get_s32le(header+0x18);
    nwa->samplecount = get_s32le(header+0x1c);
    nwa->blocksize = get_s32le(header+0x20);
    nwa->restsize = get_s32le(header+0x24);
    nwa->dummy2 = get_s32le(header+0x28);

    nwa->offsets = NULL;
    nwa->outdata = NULL;
    nwa->outdata_readpos = NULL;
    nwa->tmpdata = NULL;
    nwa->filesize = get_streamfile_size(sf);


    if (nwa->blocks <= 0 || nwa->blocks > 1000000)
        /* １時間を超える曲ってのはないでしょ*/ //surely there won't be songs over 1 hour
        goto fail;

    // NWAData::CheckHeader:

    if (nwa->channels != 1 && nwa->channels != 2)
        goto fail;

    if (nwa->bps != 8 && nwa->bps != 16)
        goto fail;

    // (PCM not handled)

    if (nwa->complevel < 0 || nwa->complevel > 5)
        goto fail;

    if (nwa->filesize != nwa->compdatasize)
        goto fail;


    if (nwa->datasize != nwa->samplecount * (nwa->bps / 8))
        goto fail;

    if (nwa->samplecount != (nwa->blocks - 1) * nwa->blocksize + nwa->restsize)
        goto fail;

    /* offset index 読み込み */ //read offset index
    nwa->offsets = malloc(sizeof(off_t) * nwa->blocks);
    if (!nwa->offsets) goto fail;

    for (i = 0; i < nwa->blocks; i++) {
        int32_t o = read_s32le(0x2c + i*4, sf);
        if (o < 0) goto fail;
        nwa->offsets[i] = o;
    }

    if (nwa->offsets[nwa->blocks-1] >= nwa->compdatasize)
        goto fail;

    nwa->use_runlength = is_use_runlength(nwa);
    nwa->curblock = 0;


    //extra
    if (nwa->restsize > nwa->blocksize) {
        nwa->outdata = malloc(sizeof(int16_t) * nwa->restsize);
    }
    else {
        nwa->outdata = malloc(sizeof(int16_t) * nwa->blocksize);
    }
    if (!nwa->outdata)
        goto fail;

    /* これ以上の大きさはないだろう、、、 */ //probably not over this size
    nwa->tmpdata = malloc(sizeof(uint8_t) * nwa->blocksize * (nwa->bps / 8) * 2);
    if (!nwa->tmpdata)
        goto fail;

    nwa->outdata_readpos = nwa->outdata;
    nwa->samples_in_buffer = 0;

    return nwa;
fail:
    nwalib_close(nwa);
    return NULL;
}

void nwalib_close(NWAData * nwa) {
    if (!nwa) return;

    free(nwa->offsets);
    free(nwa->outdata);
    free(nwa->tmpdata);
    free(nwa);
}

//NWAData::Rewind
void nwalib_reset(NWAData* nwa) {
    nwa->curblock = 0;
    nwa->outdata_readpos = nwa->outdata;
    nwa->samples_in_buffer = 0;
}

// can serve up 8 bits at a time
static int getbits(const uint8_t** p_data, int* shift, int bits) {
    int ret;
    if (*shift > 8) {
        (*p_data)++;
        *shift -= 8;
    }

    ret = get_s16le(*p_data) >> *shift;
    *shift += bits;
    return ret & ((1 << bits) - 1); /* mask */
}

// NWADecode
static void decode_block(NWAData* nwa, const uint8_t* data, int outdatasize) {
    sample d[2];
    int i;
    int shift = 0;

    int dsize = outdatasize / (nwa->bps / 8);
    int flip_flag = 0; /* stereo 用 */ //for stereo
    int runlength = 0;

    /* 最初のデータを読み込む */ //read initial data
    for (i = 0; i < nwa->channels; i++) {
        if (nwa->bps == 8) {
            d[i] = get_s8(data);
            data += 1;
        }
        else { /* nwa->bps == 16 */
            d[i] = get_s16le(data);
            data += 2;
        }
    }

    for (i = 0; i < dsize; i++) {
        if (runlength == 0) { /* コピーループ中でないならデータ読み込み */ //read data if not in the copy loop
            int type = getbits(&data, &shift, 3);

            /* type により分岐：0, 1-6, 7 */ //fork depending on type
            if (type == 7) {
                /* 7 : 大きな差分 */ //big diff
                /* RunLength() 有効時（CompLevel==5, 音声ファイル) では無効 */ //invalid when using RLE (comp=5, voice file)
                if (getbits(&data, &shift, 1) == 1) {
                    d[flip_flag] = 0;    /* 未使用 */ //unused
                }
                else {
                    int BITS, SHIFT;
                    if (nwa->complevel >= 3) {
                        BITS = 8;
                        SHIFT = 9;
                    }
                    else {
                        BITS = 8 - nwa->complevel;
                        SHIFT = 2 + 7 + nwa->complevel;
                    }

                    {
                        const int MASK1 = (1 << (BITS - 1));
                        const int MASK2 = (1 << (BITS - 1)) - 1;
                        int b = getbits(&data, &shift, BITS);
                        if (b & MASK1)
                            d[flip_flag] -= (b & MASK2) << SHIFT;
                        else
                            d[flip_flag] += (b & MASK2) << SHIFT;
                    }
                }
            }
            else if (type != 0) {
                /* 1-6 : 通常の差分 */ //normal diff
                int BITS, SHIFT;
                if (nwa->complevel >= 3) {
                    BITS = nwa->complevel + 3;
                    SHIFT = 1 + type;
                }
                else {
                    BITS = 5 - nwa->complevel;
                    SHIFT = 2 + type + nwa->complevel;
                }
                {
                    const int MASK1 = (1 << (BITS - 1));
                    const int MASK2 = (1 << (BITS - 1)) - 1;
                    int b = getbits(&data, &shift, BITS);
                    if (b & MASK1)
                        d[flip_flag] -= (b & MASK2) << SHIFT;
                    else
                        d[flip_flag] += (b & MASK2) << SHIFT;
                }
            }
            else { /* type == 0 */
                /* ランレングス圧縮なしの場合はなにもしない */ //does nothing in case of no RLE compression
                if (nwa->use_runlength) {
                    /* ランレングス圧縮ありの場合 */ //in case of RLE compression
                    runlength = getbits(&data, &shift, 1);
                    if (runlength == 1) {
                        runlength = getbits(&data, &shift, 2);
                        if (runlength == 3) {
                            runlength = getbits(&data, &shift, 8);
                        }
                    }
                }
            }
        }
        else {
            runlength--;
        }

        if (nwa->bps == 8) {
            nwa->outdata[i] = d[flip_flag] * 256; //extra (original outputs 8-bit)
        }
        else {
            nwa->outdata[i] = d[flip_flag];
        }

        if (nwa->channels == 2)
            flip_flag ^= 1;  /* channel 切り替え */ //channel swap
    }

    nwa->samples_in_buffer = dsize;
}

//NWAData::Decode
int nwalib_decode(STREAMFILE* sf, NWAData* nwa) {
    /* some wav/pcm handling skipped here */

    /* 今回読み込む／デコードするデータの大きさを得る */ //get current read/decode data size
    int curblocksize, curcompsize;
    if (nwa->curblock != nwa->blocks - 1) {
        curblocksize = nwa->blocksize * (nwa->bps / 8);
        curcompsize = nwa->offsets[nwa->curblock + 1] - nwa->offsets[nwa->curblock];
        if (curblocksize >= nwa->blocksize * (nwa->bps / 8) * 2) {
            return -1; // Fatal error
        }
    }
    else { //last block
        curblocksize = nwa->restsize * (nwa->bps / 8);
        curcompsize = nwa->blocksize * (nwa->bps / 8) * 2;
    }
    // (in practice compsize is ~200-400 and blocksize ~0x800, but last block can be different)

    /* データ読み込み */ //data read (may read less on last block?)
    read_streamfile(nwa->tmpdata, nwa->offsets[nwa->curblock], curcompsize, sf);

    nwa->samples_in_buffer = 0;
    nwa->outdata_readpos = nwa->outdata;

    decode_block(nwa, nwa->tmpdata, curblocksize);

    nwa->curblock++; //todo check not over max blocks?

    return 0;
}

//NWAFILE::Seek (not too similar)
void nwalib_seek(STREAMFILE* sf, NWAData* nwa, int32_t seekpos) {
    int dest_block = seekpos / (nwa->blocksize / nwa->channels);
    int32_t remainder = seekpos % (nwa->blocksize / nwa->channels);

    nwa->curblock = dest_block;

    nwalib_decode(sf, nwa);

    nwa->outdata_readpos = nwa->outdata + remainder * nwa->channels;
    nwa->samples_in_buffer -= remainder*nwa->channels;
}

/* ****************************************************************** */

#include "coding.h"


struct nwa_codec_data {
    STREAMFILE* sf;
    NWAData* nwa;
};

/* interface to vgmstream */
void decode_nwa(nwa_codec_data* data, sample_t* outbuf, int32_t samples_to_do) {
    NWAData* nwa = data->nwa;

    while (samples_to_do > 0) {
        if (nwa->samples_in_buffer > 0) {
            int32_t samples_to_read = nwa->samples_in_buffer / nwa->channels;
            if (samples_to_read > samples_to_do)
                samples_to_read = samples_to_do;

            memcpy(outbuf, nwa->outdata_readpos, sizeof(sample_t) * samples_to_read * nwa->channels);

            nwa->outdata_readpos += samples_to_read * nwa->channels;
            nwa->samples_in_buffer -= samples_to_read * nwa->channels;
            outbuf += samples_to_read * nwa->channels;
            samples_to_do -= samples_to_read;
        }
        else {
            int err = nwalib_decode(data->sf, nwa);
            if (err < 0) {
                VGM_LOG("NWA: decoding error\n");
                return;
            }
        }
    }
}


nwa_codec_data* init_nwa(STREAMFILE* sf) {
    nwa_codec_data* data = NULL;

    data = malloc(sizeof(nwa_codec_data));
    if (!data) goto fail;

    data->nwa = nwalib_open(sf);
    if (!data->nwa) goto fail;

    data->sf = reopen_streamfile(sf, 0);
    if (!data->sf) goto fail;

    return data;

fail:
    free_nwa(data);
    return NULL;
}

void seek_nwa(nwa_codec_data* data, int32_t sample) {
    if (!data) return;

    nwalib_seek(data->sf, data->nwa, sample);
}

void reset_nwa(nwa_codec_data* data) {
    if (!data) return;

    nwalib_reset(data->nwa);
}

void free_nwa(nwa_codec_data* data) {
    if (!data) return;

    close_streamfile(data->sf);
    nwalib_close(data->nwa);
    free(data);
}

STREAMFILE* nwa_get_streamfile(nwa_codec_data* data) {
     if (!data) return NULL;

     return data->sf;
 }
