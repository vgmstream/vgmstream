#include "meta.h"
#include "../coding/coding.h"

typedef enum { NONE, VAG, DOUBLE_VAG, VPK, RIFF, XVAG } skx_codec;
typedef struct {
	/* main header */
	uint16_t version1;
	uint16_t version2;
	uint32_t table_offset;
	uint32_t table_size;
	uint32_t table_entries;

	/* stream table header */
	uint32_t stream_offset;
	uint32_t stream_codec;
	skx_codec codec;

	/* other stuff */
	uint32_t little_endian;
	uint32_t table_is_present_in_skx;
	uint32_t table_entry_size;
	//uint32_t table_ver;
} skx_header;

/* .skx - Sony "SKEX" container format [MLB 2004 (PS2), NBA ShootOut 2004 (PS2)] */
VGMSTREAM* init_vgmstream_skx(STREAMFILE* sf)
{
	VGMSTREAM* vgmstream = NULL;
	STREAMFILE* temp_sf = NULL;
	skx_header skx = { 0 };
	int target_subsong = sf->stream_index;
	uint32_t(*read_u32)(off_t, STREAMFILE*) = NULL;
	uint16_t(*read_u16)(off_t, STREAMFILE*) = NULL;

	/* checks */
	if (!is_id32be(0, sf, "SKEX") && /* PS2, PSP */
		!is_id32le(0, sf, "SKEX")) /* PS3 */
		goto fail;

	/* .skx - standard */
	if (!check_extensions(sf, "skx"))
		goto fail;

	skx.little_endian = is_id32be(0, sf, "SKEX");
	if (skx.little_endian) {
		read_u32 = read_u32le;
		read_u16 = read_u16le;
	}
	else {
		read_u32 = read_u32be;
		read_u16 = read_u16be;
	}

	/* read main header */
	skx.version1 = read_u16(4, sf);
	skx.version2 = read_u16(6, sf);
	skx.table_offset = read_u32(0x10, sf);
	skx.table_size = read_u32(0x14, sf);
	skx.table_entries = read_u32(0x18, sf);
	//skx.table_is_present_in_skx = (skx.table_offset && skx.table_size) != 0;
	skx.table_is_present_in_skx = skx.table_offset != 0;

	if (target_subsong == 0) target_subsong = 1; /* auto: default to 1 */
	if (target_subsong < 0 || target_subsong > skx.table_entries || skx.table_entries < 1) goto fail;

	/* step 2 */
	if (skx.version1 == 0x1070) {
		skx.table_entry_size = 0x0c;
	}
	else if (skx.version1 >= 0x2010) {
		skx.table_entry_size = 0x08;
	}

	/* step 3 */
	if (skx.table_is_present_in_skx != 0) {
		{

			uint32_t current_table_offset = skx.table_offset + skx.table_entry_size + (target_subsong - 1);
			/*
			for (int i = 0; i < skx.table_entries; i++)
			{
				skx.stream_offset = read_u32(skx.table_offset + 0, sf);
				skx.stream_codec = read_u8(skx.table_offset + 4, sf);
				skx.table_offset += skx.table_entry_size;
			}
			*/
			skx.stream_offset = read_u32(current_table_offset + 0, sf);
			if (skx.version1 == 0x1070) {
				skx.stream_codec = read_u8(current_table_offset + 4, sf);
			}
			else if (skx.version1 >= 0x2010) {
				skx.stream_codec = read_u8(current_table_offset + 8, sf);
			}
			switch (skx.stream_codec) {
				case 0x00: skx.codec = NONE; break;
				case 0x05: skx.codec = VAG; break;
				case 0x09: skx.codec = RIFF; break; // ATRAC3plus RIFF wrapper
				case 0x0b: skx.codec = VPK; break;
				case 0x0c: skx.codec = skx.version1 == 0x1070 ? DOUBLE_VAG : VPK; break;
				default: break;
			}
		}
	}
	else {
		VGM_LOG("SKEX: stream table not found.");
		goto fail;
	}

fail:
	close_vgmstream(vgmstream);
	return NULL;
}