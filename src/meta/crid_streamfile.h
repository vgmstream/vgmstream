#ifndef _CRID_STREAMFILE_H_
#define _CRID_STREAMFILE_H_


typedef struct {
    bool is_encrypted;
    uint8_t audio_mask[0x20];
    int track_number;

    /* state */
    off_t logical_offset;           /* offset that corresponds to physical_offset */
    off_t physical_offset;          /* actual file offset */

    size_t block_size;              /* current block size */
    size_t data_size;
    size_t skip_size;

    size_t logical_size;
    size_t physical_size;

} crid_io_data;


static void decrypt_init(crid_io_data* io_data, uint64_t keycode) {
    if (!keycode)
        return;

    uint8_t key[0x08];
    put_u64le(key, keycode);

    // from crid.exe
    uint8_t t[0x20];
	t[0x00] = key[0];
	t[0x01] = key[1];
	t[0x02] = key[2];
	t[0x03] = key[3] - 0x34;
	t[0x04] = key[4] + 0xF9;
	t[0x05] = key[5] ^ 0x13;
	t[0x06] = key[6] + 0x61;
	t[0x07] = t[0x00] ^ 0xFF;
	t[0x08] = t[0x02] + t[0x01];
	t[0x09] = t[0x01] - t[0x07];
	t[0x0A] = t[0x02] ^ 0xFF;
	t[0x0B] = t[0x01] ^ 0xFF;
	t[0x0C] = t[0x0B] + t[0x09];
	t[0x0D] = t[0x08] - t[0x03];
	t[0x0E] = t[0x0D] ^ 0xFF;
	t[0x0F] = t[0x0A] - t[0x0B];
	t[0x10] = t[0x08] - t[0x0F];
	t[0x11] = t[0x10] ^ t[0x07];
	t[0x12] = t[0x0F] ^ 0xFF;
	t[0x13] = t[0x03] ^ 0x10;
	t[0x14] = t[0x04] - 0x32;
	t[0x15] = t[0x05] + 0xED;
	t[0x16] = t[0x06] ^ 0xF3;
	t[0x17] = t[0x13] - t[0x0F];
	t[0x18] = t[0x15] + t[0x07];
	t[0x19] = 0x21    - t[0x13];
	t[0x1A] = t[0x14] ^ t[0x17];
	t[0x1B] = t[0x16] + t[0x16];
	t[0x1C] = t[0x17] + 0x44;
	t[0x1D] = t[0x03] + t[0x04];
	t[0x1E] = t[0x05] - t[0x16];
	t[0x1F] = t[0x1D] ^ t[0x13];

	uint8_t t2[4] = {'U','R','U','C'};
	for (int i = 0; i < 0x20; i++) {
		io_data->audio_mask[i] = (i&1) ? t2[(i >> 1) & 3] : t[i] ^ 0xFF;
	}

    io_data->is_encrypted = true;
}

static void decrypt_callback(uint8_t* dst, crid_io_data* data, size_t block_pos, size_t read_size) {
    if (!data->is_encrypted)
        return;
    if (!data->data_size) // implicit but...
        return;

    // encryption starts at 0x140 from audio payload
	for(int i = 0; i < read_size; i++){
        if (block_pos >= 0x140) {
		    dst[i] ^= data->audio_mask[block_pos & 0x1F];
        }
        block_pos++;
	}
}

static void block_callback(STREAMFILE* sf, crid_io_data* data) {
    uint32_t offset = data->physical_offset;

    /* 00: header id
     * 04: data size
     * 08: header size (- 0x08)
     * 0a: padding size after data
     * 0c: stream number
     * 0d: empty
     * 0e: chunk type (0=data, 1=header, 2=comment, 3=seek), possibly a bitflag?
     * 10: frame time? (typically 0)
     * 14: frame rate? (typically 30)
    */

    uint32_t id   = read_u32be(offset + 0x00, sf);
    uint32_t size = read_u32be(offset + 0x04, sf);
    uint16_t head = read_u16be(offset + 0x08, sf);
    uint16_t padd = read_u16be(offset + 0x0a, sf);
    uint8_t  chno = read_u8   (offset + 0x0c, sf);
    uint16_t type = read_u16be(offset + 0x0e, sf);

    data->block_size = size + 0x08;
    data->data_size = size - head - padd;
    data->skip_size = head + 0x08;

    if (type != 0)
        data->data_size = 0;
    if (chno != data->track_number)
        data->data_size = 0;
    if (id != get_id32be("@SFA"))
        data->data_size = 0;
}

static size_t crid_io_read(STREAMFILE* sf, uint8_t *dest, off_t offset, size_t length, crid_io_data* data) {
    size_t total_read = 0;

    /* re-start when previous offset (can't map logical<>physical offsets) */
    if (data->logical_offset < 0 || offset < data->logical_offset) {
        data->physical_offset = 0x00;
        data->logical_offset = 0x00;
        data->data_size = 0;
    }

    /* read blocks */
    while (length > 0) {

        /* ignore EOF */
        if (offset < 0 ||
                (data->physical_offset >= data->physical_size) ||
                (data->logical_size > 0 && offset > data->logical_size)) {
            break;
        }

        /* process new block */
        if (data->data_size == 0) {
            block_callback(sf, data);

            if (data->block_size <= 0) {
                VGM_LOG("CRID: block size not set at %x\n", (int)data->physical_offset);
                break;
            }
        }

        /* move to next block */
        if (data->data_size == 0 || offset >= data->logical_offset + data->data_size) {
            data->physical_offset += data->block_size;
            data->logical_offset += data->data_size;
            data->data_size = 0;
            continue;
        }

        /* read block data */
        {
            size_t bytes_consumed, bytes_done, to_read;

            bytes_consumed = offset - data->logical_offset;
            to_read = data->data_size - bytes_consumed;
            if (to_read > length)
                to_read = length;
            bytes_done = read_streamfile(dest, data->physical_offset + data->skip_size + bytes_consumed, to_read, sf);

            decrypt_callback(dest, data, bytes_consumed, bytes_done);

            total_read += bytes_done;
            dest += bytes_done;
            offset += bytes_done;
            length -= bytes_done;

            if (bytes_done != to_read || bytes_done == 0) {
                break; /* error/EOF */
            }
        }
    }

    return total_read;
}

static size_t crid_io_size(STREAMFILE* sf, crid_io_data* data) {
    uint8_t buf[0x04];

    if (data->logical_size)
        return data->logical_size;

    /* force a fake read at max offset, to get max logical_offset (will be reset next read) */
    crid_io_read(sf, buf, 0x7FFFFFFF, 1, data);
    data->logical_size = data->logical_offset;
    
    return data->logical_size;
}


/* Extract audio payload from CRID (.usm) @SFA chunks 
 * Some info from:
 * - crid.exe by Nyaga: https://github.com/bnnm/vgm-tools/tree/master/misc/crid-mod (encryption)
 * - hcs64's usm_deinterleave https://github.com/hcs64/vgm_ripping/tree/master/multi/utf_tab
 */
static STREAMFILE* setup_crid_streamfile(STREAMFILE* sf, const char* ext, uint64_t keycode, int target_subsong) {
    STREAMFILE* new_sf = NULL;
    crid_io_data io_data = {0};

    decrypt_init(&io_data, keycode);
    io_data.track_number = target_subsong - 1;
    io_data.logical_offset = -1; /* read reset */
    io_data.physical_size = get_streamfile_size(sf);

    new_sf = open_wrap_streamfile(sf);
    new_sf = open_io_streamfile_f(new_sf, &io_data, sizeof(crid_io_data), crid_io_read, crid_io_size);
    new_sf = open_fakename_streamfile_f(new_sf, NULL, ext);
    return new_sf;
}

#endif
