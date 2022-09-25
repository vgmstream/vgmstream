#include "meta.h"
#include "../coding/coding.h"
#include "../util/chunks.h"
#include "../util/endianness.h"

/* xbb defines, made up for easier identification. */
#define xbb_has_riff_chunk    0x01
#define xbb_has_wave_chunk    0x02
#define xbb_has_fmt_chunk     0x04
#define xbb_has_data_chunk    0x08
#define xbb_has_external_data 0x10

typedef struct {
	/* RIFF chunks */
	uint32_t riff_offset;
	uint32_t riff_size;
	uint32_t fmt_offset;
	uint32_t fmt_size;
	uint32_t data_offset;
	uint32_t data_size;

	/* RIFF WAVE format header */
	uint16_t codec;
	uint16_t channels;
	uint32_t sample_rate;
	uint32_t avg_bps; // "average_bytes_per_second"
	uint16_t blkalgn; // "block_align"
	uint16_t bps; // "bytes_per_second"
	uint16_t out_bps_pch; // "out_bytes_per_second_per_channel"
	uint16_t out_blkalgn_pch; // "out_block_align_per_channel"

	/* RIFF data header */
	uint32_t external_data_offset;
	uint32_t external_data_size;

	/* other stuff */
	uint8_t xbb_flags;
	uint32_t stream_offset;
	uint32_t stream_size;
} xbb_header;

/* .xbb+.xsb - from Gladius [Xbox] */
VGMSTREAM* init_vgmstream_xbb_xsb(STREAMFILE* sf)
{
	VGMSTREAM* vgmstream = NULL;
	//STREAMFILE* sf_xbb = NULL, *sf_xsb = NULL, *sf_h = NULL, *sf_b = NULL;
	STREAMFILE* sf_h = NULL, *sf_b = NULL, *sf_data = NULL;
	xbb_header xbb = { 0 };
	uint32_t xbb_size, xbb_entry_offset, xbb_entries;
	uint32_t xbb_entry;
	int target_subsong = sf->stream_index;
	int xbb_real_size;

	/* checks */
	if (!check_extensions(sf, "xbb")) goto fail;

	sf_h = sf;

	xbb_size = read_u32le(0, sf_h);
	if (xbb_size < 4) goto fail;
	xbb_real_size = get_streamfile_size(sf_h);
	if (xbb_real_size != (xbb_size + 4)) goto fail;

	xbb_entries = read_u32le(4, sf_h);
	if (target_subsong == 0) target_subsong = 1;
	if (target_subsong < 0 || target_subsong > xbb_entries || xbb_entries < 1) goto fail;

	xbb_entry_offset = 8;
	xbb_entry = 0;
	{
		chunk_t rc = { 0 };
		rc.current = xbb_entry_offset;
		
		if (xbb_entry != (target_subsong - 1)) goto fail;

		xbb.xbb_flags = 0;
		while (next_chunk(&rc, sf_h))
		{
			switch (rc.type) {
				case 0x52494646: /* "RIFF" */
					xbb.xbb_flags |= xbb_has_riff_chunk;
					xbb.riff_offset = rc.offset;
					xbb.riff_size = rc.size;
					break;
				case 0x57415645: /* "WAVE" */
					xbb.xbb_flags |= xbb_has_wave_chunk;
					break;
				case 0x666d7420: /* "fmt " */
					xbb.xbb_flags |= xbb_has_fmt_chunk;
					xbb.fmt_offset = rc.offset;
					xbb.fmt_size = rc.size;
					break;
				case 0x64617461: /* "data" */
					xbb.xbb_flags |= xbb_has_data_chunk;
					if (rc.size == 0xfffffff8) {
						xbb.xbb_flags |= xbb_has_external_data;
						rc.size = 0;
					} else {
						xbb.data_size = rc.size;
					}
					xbb.data_offset = rc.offset;
					break;
				default: /* not many chunks beyond that though. */
					break;
			}
		}

		xbb_entry += 1;
		xbb_entry_offset += 8;
		xbb_entry_offset += xbb.riff_size;
	}

	/* check if "RIFF" chunk exists, unlikely. */
	if (!(xbb.xbb_flags & xbb_has_riff_chunk)) goto fail;
	/* check if "WAVE" chunk exists, unlikely. */
	if (!(xbb.xbb_flags & xbb_has_wave_chunk)) goto fail;
	/* check if "fmt " chunk exists, unlikely. */
	if (!(xbb.xbb_flags & xbb_has_fmt_chunk)) goto fail;
	/* read all "fmt " info there is. */
	if (xbb.fmt_size == 0x14) {
		/* usual Xbox IMA ADPCM codec header data */
		xbb.codec = read_u16le(xbb.fmt_offset + 0, sf_h);
		xbb.channels = read_u16le(xbb.fmt_offset + 2, sf_h);
		xbb.sample_rate = read_u32le(xbb.fmt_offset + 4, sf_h);
		xbb.avg_bps = read_u32le(xbb.fmt_offset + 8, sf_h);
		xbb.blkalgn = read_u16le(xbb.fmt_offset + 12, sf_h);
		xbb.bps = read_u16le(xbb.fmt_offset + 14, sf_h);
		xbb.out_bps_pch = read_u16le(xbb.fmt_offset + 16, sf_h);
		xbb.out_blkalgn_pch = read_u16le(xbb.fmt_offset + 18, sf_h);
		/* do some checks against existing fmt values */
		if (xbb.blkalgn != (0x24 * xbb.channels)) goto fail;
		if (xbb.bps != 4) goto fail;
		if (xbb.out_bps_pch != 2) goto fail;
		if (xbb.out_blkalgn_pch != 0x40) goto fail;
	}
	else {
		goto fail;
	}
	/* check if "data" chunk exists, unlikely. */
	if (!(xbb.xbb_flags & xbb_has_data_chunk)) goto fail;
	/* read all "data" info there is. */
	if (xbb.xbb_flags & xbb_has_external_data) {
		/* if some sound has external data, read offset and size values from internal XBB entry info */
		xbb.external_data_offset = read_u32le(xbb.data_offset + 0, sf);
		xbb.external_data_size = read_u32le(xbb.data_offset + 4, sf);
		xbb.stream_offset = xbb.external_data_offset;
		xbb.stream_size = xbb.external_data_size;
		/* open external .xsb file */
		sf_b = open_streamfile_by_ext(sf, "xsb");
		if (!sf_b) goto fail;
		sf_data = sf_b;
	}
	else {
		xbb.stream_offset = xbb.data_offset;
		xbb.stream_size = xbb.data_size;
		sf_data = sf_h;
	}

	/* build and allocate the vgmstream */
	vgmstream = allocate_vgmstream(xbb.channels, 0);
	if (!vgmstream) goto fail;

	vgmstream->meta_type = meta_XBB_XSB;
	vgmstream->sample_rate = xbb.sample_rate;
	vgmstream->num_streams = xbb_entries;
	vgmstream->stream_size = xbb.stream_size;

	switch (xbb.codec) {
		case 0x0069:
			vgmstream->coding_type = coding_XBOX_IMA;
			vgmstream->layout_type = layout_none;
			vgmstream->num_samples = xbox_ima_bytes_to_samples(xbb.stream_size,xbb.channels);
			break;
	}

	/* open the file for reading */
	if (!vgmstream_open_stream(vgmstream,sf_data,xbb.stream_offset)) goto fail;
	if (sf_h) close_streamfile(sf_h);
	if (sf_b) close_streamfile(sf_b);
	return vgmstream;
fail:
	if (sf_h) close_streamfile(sf_h);
	if (sf_b) close_streamfile(sf_b);
	close_vgmstream(vgmstream);
	return NULL;
}
