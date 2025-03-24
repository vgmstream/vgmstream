/*****************************************************************************
                         E R I S A - L i b r a r y
 -----------------------------------------------------------------------------
        Copyright (C) 2002-2003 Leshade Entis, Entis-soft. All rights reserved.
 *****************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mio_xerisa.h"
#include "../../util/reader_get.h"

#define MIO_PACKET_BUFFER_MAX  0x20000 /* observed max is ~0x1d000 */
#define MIO_PACKET_HEADER_SIZE 0x08
#define MIO_PACKET_BYTES_MAX  (32768 * 2 * 2) /* observed max: samples * channels * pcm16 */


static inline /*const*/ uint64_t get_id64le(const char* s) {
    return (uint64_t)(
            ((uint64_t)s[7] << 56) |
            ((uint64_t)s[6] << 48) |
            ((uint64_t)s[5] << 40) |
            ((uint64_t)s[4] << 32) |
            ((uint64_t)s[3] << 24) |
            ((uint64_t)s[2] << 16) |
            ((uint64_t)s[1] << 8) |
            ((uint64_t)s[0] << 0)
    );
}

MIOFile* MIOFile_Open() {
    MIOFile* mf = calloc(1, sizeof(MIOFile));
    if (!mf)
        return NULL;

    return mf;
}

void MIOFile_Close(MIOFile* mf) {
    if (!mf) return;

    free(mf->ptrWaveBuf);
    free(mf->desc);
    free(mf->buf);
    free(mf);
}


static int read_chunk(EMC_RECORD_HEADER* rh, io_callback_t* file) {
    uint8_t chunk[0x10];

    int len = file->read(chunk, 1, 0x10, file->arg);
    if (len != 0x10) goto fail;

    rh->nRecordID    = get_u64le(chunk + 0x00);
    rh->nRecLength   = get_u64le(chunk + 0x08);

    return 1;
fail:
    return 0;
}

/* reads string (possibly utf16) in the form of "KEY\r\nVAL\r\n" */
static int read_tag(char* tag, int tag_len, char* buf, int buf_len, int is_utf16le) {
    int step = is_utf16le ? 2 : 1;
    
    int buf_pos = 0;
    int tag_pos = 0;

    tag[0] = '\0';
    while (1) {
        if (buf_pos + step >= buf_len)
            break;
        if (tag_pos + 1 >= tag_len)
            break;

        char elem = buf[buf_pos++];
        if (is_utf16le) /* ignore high byte for now (only seen simple tags) */
            buf_pos++;

        if (elem == '\0')
            break;

        tag[tag_pos++] = elem;
    }

    tag[tag_pos] = '\0';
    return buf_pos;
}

static int read_int(const char* params, UDWORD *value) {
    int n,m;
    int temp;

    m = sscanf(params, " %d%n", &temp,&n);
    if (m != 1 || temp < 0)
        return 0;

    *value = temp;
    return n;
}

/* Originally this is parsed into a list then returned when requested (ex. ERIFile::ETagInfo::GetRewindPoint)
 * but pre-read to simplify. Tags format:
 * - optional BOM (text is char16, possibly SHIFT-JIS), only seen files with utf-16
 * - key\r\nvalue\r\n (keys usually start with '#') xN (probably null-separated but only seen 1 tag)
 * - null padding (tag block is usually 0x80 even if only one short string is used) */
static void read_tags(MIOFile* mf) {
    char tag[128];
    int tag_len = sizeof(tag);
    char* desc = mf->desc;
    int desc_len = mf->desc_len;
    int is_utf16le = 0;

    if (desc[0] == '\xff' && desc[1] == '\xfe') {
        is_utf16le = 1;
    }

    desc += 2;
    desc_len -= 2;

    while (1) {
        int read = read_tag(tag, tag_len, desc, desc_len, is_utf16le);
        if (read <= 0)
            break;

        if (memcmp(tag, "#rewind-point\r\n", 15) == 0)
            read_int(tag + 15, &mf->mioih.rewindPoint);

        desc += read;
        desc_len -= read;
    }
}

ESLError MIOFile_Initialize(MIOFile* mf, io_callback_t* file) {
    EMC_RECORD_HEADER rh;
    uint8_t buf[0x40];
    int len, ok;
    int size, to_read;

    file->seek(file->arg, 0, IO_CALLBACK_SEEK_SET);

    /* read base chunk */
    {
        to_read = 0x40;

        len = file->read(buf, 1, to_read, file->arg);
        if (len < to_read) goto fail;

        mf->emcfh.cHeader      = get_u64le(buf + 0x00);
        mf->emcfh.dwFileID     = get_u32le(buf + 0x08);
        mf->emcfh.dwReserved   = get_u32le(buf + 0x0c);
        memcpy(mf->emcfh.cFormatDesc, buf + 0x10, 0x20);
        /* 0x30: extra info 0x10 */

        if (mf->emcfh.cHeader != get_id64le("Entis\x1a\x00\x00"))
            goto fail;

        /* each format has a fixed description, not checked in OG lib though */
        if (memcmp(mf->emcfh.cFormatDesc, "Music Interleaved and Orthogonal", 0x20) != 0)
            goto fail;
        /* older files end with " transformed\0\0\0\0", and newer with null + data size */
    }


    /* read header chunks */
    {
        ok = read_chunk(&rh, file);
        if (!ok) goto fail;

        if (rh.nRecordID != get_id64le("Header  "))
            goto fail;

        size = rh.nRecLength;
        while (size > 0) {

            ok = read_chunk(&rh, file);
            if (!ok) goto fail;

            size -= 0x10;

            /* common info */
            if (rh.nRecordID == get_id64le("FileHdr ")) {
                to_read = rh.nRecLength;
                if (to_read > sizeof(buf) || to_read != 0x14) goto fail;

                len = file->read(buf, 1, to_read, file->arg);
                if (len != to_read) goto fail;

                mf->erifh.dwVersion        = get_u32le(buf + 0x00);
                mf->erifh.dwContainedFlag  = get_u32le(buf + 0x04);
                mf->erifh.dwKeyFrameCount  = get_u32le(buf + 0x08);
                mf->erifh.dwFrameCount     = get_u32le(buf + 0x0c);
                mf->erifh.dwAllFrameTime   = get_u32le(buf + 0x10);
            }

            /* audio header */
            if (rh.nRecordID == get_id64le("SoundInf")) {
                to_read = rh.nRecLength;
                if (to_read > sizeof(buf) || to_read != 0x28) goto fail;

                len = file->read(buf, 1, to_read, file->arg);
                if (len != to_read) goto fail;

                mf->mioih.dwVersion        = get_u32le(buf + 0x00);
                mf->mioih.fdwTransformation= get_u32le(buf + 0x04);
                mf->mioih.dwArchitecture   = get_u32le(buf + 0x08);
                mf->mioih.dwChannelCount   = get_u32le(buf + 0x0c);

                mf->mioih.dwSamplesPerSec  = get_u32le(buf + 0x10);
                mf->mioih.dwBlocksetCount  = get_u32le(buf + 0x14);
                mf->mioih.dwSubbandDegree  = get_u32le(buf + 0x18);
                mf->mioih.dwAllSampleCount = get_u32le(buf + 0x1c);

                mf->mioih.dwLappedDegree   = get_u32le(buf + 0x20);
                mf->mioih.dwBitsPerSample  = get_u32le(buf + 0x24);
            }

            /* other defaults */
            mf->mioih.rewindPoint = -1; /* not looped (since #rewind-point 0 means loop from the beginning) */

            /* tags */
            if (rh.nRecordID == get_id64le("descript")) {
                /* OG lib supports: 
                   "title", "vocal-player", "composer", "arranger", "source", "track", "release-date", "genre",
                   "rewind-point" (loop start), "hot-spot", "resolution", "comment", "words" (lyrics)
                   Only seen loops though.
                 */

                /* sometimes chunk exists with just size 0x02 (empty but probably for BOM) */
                to_read = rh.nRecLength;
                if (to_read < 0x02 || to_read > 0x10000) goto fail;

                mf->desc = calloc(1, to_read + 2);
                if (!mf->desc) goto fail;

                len = file->read(mf->desc, 1, to_read, file->arg);
                if (len != to_read) goto fail;

                /* doesn't always end with null */
                mf->desc[to_read+0] = '\0';
                mf->desc[to_read+1] = '\0';
                mf->desc_len = to_read;

                read_tags(mf);
            }

            /* other chunks (not seen / for other non-MIO formats?):
             * - "PrevwInf" (image preview)
             * - "ImageInf" (image header)
             * - "Sequence" (image?)
             * - "cpyright" (text info) 
             */

            size -= rh.nRecLength;
        }

        if (mf->erifh.dwVersion > 0x00020100) {
            goto fail;
        }
    }


    /* read file data */
    {
        ok = read_chunk(&rh, file);
        if (!ok) goto fail;

        if (rh.nRecordID != get_id64le("Stream  "))
            goto fail;

        //printf("stream chunk reached\n");
        /* packets are in "SoundStm" chunks ("ImageFrm" in images) */
        mf->start = file->tell(file->arg);
    }

    return eslErrSuccess;
fail:
    return eslErrGeneral;
}

ESLError MIOFile_Reset(MIOFile* mf, io_callback_t* file) {
    if (!mf) goto fail;

    file->seek(file->arg, mf->start, IO_CALLBACK_SEEK_SET);

    return eslErrSuccess;
fail:
    return eslErrGeneral;
}

static int parse_packet_header(uint8_t* buf, int buf_size, MIO_DATA_HEADER* dh) {
    if (buf_size < MIO_PACKET_HEADER_SIZE)
        goto fail;

    dh->bytVersion      = get_u8(buf + 0x00);
    dh->bytFlags        = get_u8(buf + 0x01);
    dh->bytReserved1    = get_u8(buf + 0x02);
    dh->bytReserved2    = get_u8(buf + 0x03);
    dh->dwSampleCount   = get_u32le(buf + 0x04);

    return 1;
fail:
    return 0;
}

ESLError MIOFile_NextPacket(MIOFile* mf, io_callback_t* file) {
    EMC_RECORD_HEADER rh;
    int ok, len;

    ok = read_chunk(&rh, file);
    if (!ok) return eslErrEof;

    if (rh.nRecordID != get_id64le("SoundStm"))
        goto fail;

    /* prepare buf */
    if (mf->buf_size < rh.nRecLength) {
        if (rh.nRecLength > MIO_PACKET_BUFFER_MAX)
            goto fail;
        if (rh.nRecLength <= MIO_PACKET_HEADER_SIZE)
            goto fail;

        free(mf->buf);
        mf->buf_size = rh.nRecLength;
        mf->buf = malloc(mf->buf_size);
        if (!mf->buf) goto fail;
    }

    len = file->read(mf->buf, 1, rh.nRecLength, file->arg);
    if (len != rh.nRecLength) goto fail;

    ok = parse_packet_header(mf->buf, rh.nRecLength, &mf->miodh);
    if (!ok) goto fail;

    mf->packet = mf->buf + MIO_PACKET_HEADER_SIZE;
    mf->packet_size = rh.nRecLength - MIO_PACKET_HEADER_SIZE;

    return eslErrSuccess;
fail:
    return eslErrGeneral;
}


int MIOFile_GetTagLoop(MIOFile* mf, const char* tag) {
    return mf->mioih.rewindPoint;
}

void* MIOFile_GetCurrentWaveBuffer(MIOFile* mf) {
    int channels = mf->mioih.dwChannelCount;
    int bps = mf->mioih.dwBitsPerSample;
    int bytes_per_sample = channels * (bps / 8);
    int chunk_samples = mf->miodh.dwSampleCount;

    /* usually same for all except less in last packet */
    UDWORD dwBytesAudio = chunk_samples * bytes_per_sample;
    if (dwBytesAudio > MIO_PACKET_BYTES_MAX) goto fail;

    if (dwBytesAudio > mf->ptrWaveBuf_len) {
        free(mf->ptrWaveBuf);
        mf->ptrWaveBuf = malloc(dwBytesAudio);
        if (!mf->ptrWaveBuf) goto fail;
        mf->ptrWaveBuf_len = dwBytesAudio;
    }

    return mf->ptrWaveBuf;
fail:
    return NULL;
}

int MIOFile_GetCurrentWaveBufferCount(MIOFile* mf) {
    int channels = mf->mioih.dwChannelCount;
    int chunk_samples = mf->miodh.dwSampleCount;
    return chunk_samples * channels;
}
