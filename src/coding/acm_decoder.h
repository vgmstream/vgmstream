/*
 * libacm - Interplay ACM audio decoder.
 *
 * Copyright (c) 2004-2008, Marko Kreen
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef __LIBACM_H
#define __LIBACM_H

#include "../streamfile.h"

#define LIBACM_VERSION "0.9.2"

#define ACM_ID		0x032897
#define ACM_WORD	2

#define ACM_OK			 0
#define ACM_ERR_OTHER		-1
#define ACM_ERR_OPEN		-2
#define ACM_ERR_NOT_ACM		-3
#define ACM_ERR_READ_ERR	-4
#define ACM_ERR_BADFMT		-5
#define ACM_ERR_CORRUPT		-6
#define ACM_ERR_UNEXPECTED_EOF	-7
#define ACM_ERR_NOT_SEEKABLE	-8

typedef struct ACMInfo {
	int channels;
	int rate;
	int acm_id;
	int acm_version;
	int acm_level;
	int acm_cols;		/* 1 << acm_level */
	int acm_rows;
} ACMInfo;

struct ACMStream {
	ACMInfo info;
	int total_values;

	/* acm data stream */
    STREAMFILE *streamfile;
	int data_len;

	/* acm stream buffer */
	int bit_avail;
	unsigned bit_data;
	unsigned buf_start_ofs;

	/* block lengths (in samples) */
	int block_len;
	int wrapbuf_len;
	/* buffers */
	int *block;
	int *wrapbuf;
	int *ampbuf;
	int *midbuf;			/* pointer into ampbuf */
	/* result */
	int block_ready;
	int stream_pos;			/* in words. absolute */
	int block_pos;			/* in words, relative */
};
typedef struct ACMStream ACMStream;

/* decode.c */
int acm_open_decoder(ACMStream **res, STREAMFILE *facilitator_file, const char *const filename);
int acm_read(ACMStream *acm, char *buf, int nbytes,
		int bigendianp, int wordlen, int sgned);
void acm_close(ACMStream *acm);

/* util.c */
const ACMInfo *acm_info(ACMStream *acm);
int acm_seekable(ACMStream *acm);
int acm_bitrate(ACMStream *acm);
int acm_raw_total(ACMStream *acm);
int acm_raw_tell(ACMStream *acm);
int acm_pcm_total(ACMStream *acm);
int acm_pcm_tell(ACMStream *acm);
int acm_time_total(ACMStream *acm);
int acm_time_tell(ACMStream *acm);
int acm_read_loop(ACMStream *acm, char *dst, int len,
		int bigendianp, int wordlen, int sgned);
int acm_seek_pcm(ACMStream *acm, int pcm_pos);
int acm_seek_time(ACMStream *acm, int pos_ms);
const char *acm_strerror(int err);

#endif

