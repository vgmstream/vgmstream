#ifdef _MSC_VER
#define _USE_MATH_DEFINES
#endif
#include <math.h>
#include <limits.h>

#include "meta.h"
#include "adx_keys.h"
#include "../coding/coding.h"

#define MAX_TEST_FRAMES (INT_MAX/0x8000)

static int find_key(STREAMFILE *file, uint8_t type, uint16_t *xor_start, uint16_t *xor_mult, uint16_t *xor_add);

/* ADX - CRI Middleware format */
VGMSTREAM * init_vgmstream_adx(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, hist_offset = 0;
    int loop_flag = 0, channel_count;
    int32_t loop_start_sample = 0, loop_end_sample = 0;
    uint16_t version_signature;
    uint8_t encoding_type;
    uint8_t frame_size;

    meta_t header_type;
    coding_t coding_type;
    int16_t coef1, coef2;
    uint16_t xor_start=0,xor_mult=0,xor_add=0;


    /* checks*/
    /* .adx: standard, .adp: Headhunter (DC) */
    if (!check_extensions(streamFile,"adx,adp"))
        goto fail;

    /* check first 2 bytes */
    if ((uint16_t)read_16bitBE(0x00,streamFile)!=0x8000) goto fail;

    /* get stream offset, check for CRI signature just before */
    start_offset = (uint16_t)read_16bitBE(0x02,streamFile) + 4;
    if ((uint16_t)read_16bitBE(start_offset-6,streamFile)!=0x2863 ||   /* "(c" */
        (uint32_t)read_32bitBE(start_offset-4,streamFile)!=0x29435249  /* ")CRI" */
       ) goto fail;

    /* check for encoding type */
    /* 0x02 is for some unknown fixed filter, 0x03 is standard ADX, 0x04 is
     * ADX with exponential scale, 0x10 is AHX for DC, 0x11 is AHX */
    encoding_type = read_8bit(0x04, streamFile);

    switch (encoding_type) {
        case 2:
            coding_type = coding_CRI_ADX_fixed;
            break;
        case 3:
            coding_type = coding_CRI_ADX;
            break;
        case 4:
            coding_type = coding_CRI_ADX_exp;
            break;
        default:
            goto fail;
    }

    frame_size = read_8bit(0x05, streamFile);

    /* check for bits per sample? (only 4 makes sense for ADX) */
    if (read_8bit(0x06,streamFile) != 4) goto fail;

    /* older ADX (adxencd) up to 2ch, newer ADX (criatomencd) up to 8 */
    channel_count = read_8bit(0x07,streamFile);

    /* check version signature, read loop info */
    version_signature = read_16bitBE(0x12,streamFile);


    /* encryption */
    if (version_signature == 0x0408) {
        if (find_key(streamFile, 8, &xor_start, &xor_mult, &xor_add)) {
            coding_type = coding_CRI_ADX_enc_8;
            version_signature = 0x0400;
        }
    }
    else if (version_signature == 0x0409) {
        if (find_key(streamFile, 9, &xor_start, &xor_mult, &xor_add)) {
            coding_type = coding_CRI_ADX_enc_9;
            version_signature = 0x0400;
        }
    }


    /* version + extra data */
    if (version_signature == 0x0300) {  /* early ADX (~2004?) */
        size_t base_size = 0x14, loops_size = 0x18;

        header_type = meta_ADX_03;

        /* no sample history */

        if (start_offset - 6 >= base_size + loops_size) { /* enough space for loop info? */
            off_t loops_offset = base_size;

            /* off+0x00 (2): initial loop padding (the encoder adds a few blank samples so loop start is block-aligned; max 31)
             *  ex. loop_start=12: enc_start=32, padding=20 (32-20=12); loop_start=35: enc_start=64, padding=29 (64-29=35)
             * off+0x02 (2): loop sample(?) flag (always 1) */
            loop_flag           = read_32bitBE(loops_offset+0x04,streamFile) != 0; /* loop offset(?) flag (always 1) */
            loop_start_sample   = read_32bitBE(loops_offset+0x08,streamFile);
            //loop_start_offset = read_32bitBE(loops_offset+0x0c,streamFile);
            loop_end_sample     = read_32bitBE(loops_offset+0x10,streamFile);
            //loop_end_offset   = read_32bitBE(loops_offset+0x14,streamFile);
        }
    }
    else if (version_signature == 0x0400) {  /* common */
        size_t base_size = 0x18, hist_size, ainf_size = 0, loops_size = 0x18;
        off_t ainf_offset;

        header_type = meta_ADX_04;

        hist_offset = base_size; /* always present but often blank */
        hist_size = (channel_count > 1 ? 4*channel_count : 4+4); /* min is 8, even in 1ch files */

        ainf_offset = base_size + hist_size + 0x4; /* not seen with >2ch though */
        if ((uint32_t)read_32bitBE(ainf_offset+0x00,streamFile) == 0x41494E46) /* "AINF" */
            ainf_size = read_32bitBE(ainf_offset+0x04,streamFile);

        if (start_offset - ainf_size - 6 >= hist_offset + hist_size + loops_size) {  /* enough space for loop info? */
            off_t loops_offset = base_size + hist_size;

            /* off+0x00 (2): initial loop padding (the encoder adds a few blank samples so loop start is block-aligned; max 31)
             *  ex. loop_start=12: enc_start=32, padding=20 (32-20=12); loop_start=35: enc_start=64, padding=29 (64-29=35)
             * off+0x02 (2): loop sample(?) flag (always 1) */
            loop_flag           = read_32bitBE(loops_offset+0x04,streamFile) != 0; /* loop offset(?) flag (always 1) */
            loop_start_sample   = read_32bitBE(loops_offset+0x08,streamFile);
            //loop_start_offset = read_32bitBE(loops_offset+0x0c,streamFile);
            loop_end_sample     = read_32bitBE(loops_offset+0x10,streamFile);
            //loop_end_offset   = read_32bitBE(loops_offset+0x14,streamFile);
        }

        /* AINF header info (may be inserted by CRI's tools but is rarely used)
         *  Can also start right after the loop points (base_size + hist_size + loops_size)
         * 0x00 (4): "AINF";  0x04 (4): size;  0x08 (10): str_id
         * 0x18 (2): volume (0=base/max?, negative=reduce)
         * 0x1c (2): pan l;   0x1e (2): pan r (0=base, max +-128) */
    }
    else if (version_signature == 0x0500) {  /* found in some SFD: Buggy Heat, appears to have no loop */
        header_type = meta_ADX_05;
    }
    else { /* not a known/supported version signature */
        goto fail;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->num_samples = read_32bitBE(0xc,streamFile);
    vgmstream->sample_rate = read_32bitBE(0x8,streamFile);
    vgmstream->loop_start_sample = loop_start_sample;
    vgmstream->loop_end_sample = loop_end_sample;

    vgmstream->coding_type = coding_type;
    vgmstream->layout_type = channel_count==1 ? layout_none : layout_interleave;
    vgmstream->interleave_block_size = frame_size;
    vgmstream->meta_type = header_type;


    /* calculate filter coefficients */
    if (coding_type == coding_CRI_ADX_fixed) {
        int i;
        for (i = 0; i < channel_count; i++) {
            vgmstream->ch[i].adpcm_coef[0] = 0x0000;
            vgmstream->ch[i].adpcm_coef[1] = 0x0000;
            vgmstream->ch[i].adpcm_coef[2] = 0x0F00;
            vgmstream->ch[i].adpcm_coef[3] = 0x0000;
            vgmstream->ch[i].adpcm_coef[4] = 0x1CC0;
            vgmstream->ch[i].adpcm_coef[5] = 0xF300;
            vgmstream->ch[i].adpcm_coef[6] = 0x1880;
            vgmstream->ch[i].adpcm_coef[7] = 0xF240;
        }
    }
    else {
        double x,y,z,a,b,c;
        int i;
        /* high-pass cutoff frequency, always 500 that I've seen */
        uint16_t cutoff = (uint16_t)read_16bitBE(0x10,streamFile);

        x = cutoff;
        y = vgmstream->sample_rate;
        z = cos(2.0*M_PI*x/y);

        a = M_SQRT2-z;
        b = M_SQRT2-1.0;
        c = (a-sqrt((a+b)*(a-b)))/b;

        coef1 = (short)(c*8192);
        coef2 = (short)(c*c*-4096);

        for (i = 0; i < channel_count; i++) {
            vgmstream->ch[i].adpcm_coef[0] = coef1;
            vgmstream->ch[i].adpcm_coef[1] = coef2;
        }
    }

    /* init decoder */
    {
        int i;

        for (i=0;i<channel_count;i++) {
            /* 2 hist shorts per ch, corresponding to the very first original sample repeated (verified with CRI's encoders).
             * Not vital as their effect is small, after a few samples they don't matter, and most songs start in silence. */
            if (hist_offset) {
                vgmstream->ch[i].adpcm_history1_32 = read_16bitBE(hist_offset + i*4 + 0x00,streamFile);
                vgmstream->ch[i].adpcm_history2_32 = read_16bitBE(hist_offset + i*4 + 0x02,streamFile);
            }

            if (coding_type == coding_CRI_ADX_enc_8 || coding_type == coding_CRI_ADX_enc_9) {
                int j;
                vgmstream->ch[i].adx_channels = channel_count;
                vgmstream->ch[i].adx_xor = xor_start;
                vgmstream->ch[i].adx_mult = xor_mult;
                vgmstream->ch[i].adx_add = xor_add;

                for (j=0;j<i;j++)
                    adx_next_key(&vgmstream->ch[i]);
            }
        }
    }


    if ( !vgmstream_open_stream(vgmstream,streamFile,start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


/* return 0 if not found, 1 if found and set parameters */
static int find_key(STREAMFILE *file, uint8_t type, uint16_t *xor_start, uint16_t *xor_mult, uint16_t *xor_add)
{
    uint16_t * scales = NULL;
    uint16_t * prescales = NULL;
    int bruteframe=0,bruteframecount=-1;
    int startoff, endoff;
    int rc = 0;


    /* try to find key in external file first */
    {
        uint8_t keybuf[6];

        if (read_key_file(keybuf, 6, file) == 6) {
            *xor_start = get_16bitBE(keybuf+0);
            *xor_mult = get_16bitBE(keybuf+2);
            *xor_add = get_16bitBE(keybuf+4);
            return 1;
        }
    }


    /* guess key from the tables */
    startoff=read_16bitBE(2, file)+4;
    endoff=(read_32bitBE(12, file)+31)/32*18*read_8bit(7, file)+startoff;

    /* how many scales? */
    {
        int framecount=(endoff-startoff)/18;
        if (framecount<bruteframecount || bruteframecount<0)
            bruteframecount=framecount;
    }

    /* find longest run of nonzero frames */
    {
        int longest=-1,longest_length=-1;
        int i;
        int length=0;
        for (i=0;i<bruteframecount;i++) {
            static const unsigned char zeroes[18]={0};
            unsigned char buf[18];
            read_streamfile(buf, startoff+i*18, 18, file);
            if (memcmp(zeroes,buf,18)) length++;
            else length=0;
            if (length > longest_length) {
                longest_length=length;
                longest=i-length+1;
                if (longest_length >= 0x8000) break;
            }
        }
        if (longest==-1) {
            goto find_key_cleanup;
        }
        bruteframecount = longest_length;
        bruteframe = longest;
    }

    {
        /* try to guess key */
        const adxkey_info * keys = NULL;
        int keycount = 0, keymask = 0;
        int scales_to_do;
        int key_id;

        /* allocate storage for scales */
        scales_to_do = (bruteframecount > MAX_TEST_FRAMES ? MAX_TEST_FRAMES : bruteframecount);
        scales = malloc(scales_to_do*sizeof(uint16_t));
        if (!scales) {
            goto find_key_cleanup;
        }
        /* prescales are those scales before the first frame we test
         * against, we use these to compute the actual start */
        if (bruteframe > 0) {
            int i;
            /* allocate memory for the prescales */
            prescales = malloc(bruteframe*sizeof(uint16_t));
            if (!prescales) {
                goto find_key_cleanup;
            }
            /* read the prescales */
            for (i=0; i<bruteframe; i++) {
                prescales[i] = read_16bitBE(startoff+i*18, file);
            }
        }

        /* read in the scales */
        {
            int i;
            for (i=0; i < scales_to_do; i++) {
                scales[i] = read_16bitBE(startoff+(bruteframe+i)*18, file);
            }
        }

        if (type == 8)
        {
            keys = adxkey8_list;
            keycount = adxkey8_list_count;
            keymask = 0x6000;
        }
        else if (type == 9)
        {
            /* smarter XOR as seen in PSO2. The scale is technically 13 bits,
             * but the maximum value assigned by the encoder is 0x1000.
             * This is written to the ADX file as 0xFFF, leaving the high bit
             * empty, which is used to validate a key */
            keys = adxkey9_list;
            keycount = adxkey9_list_count;
            keymask = 0x1000;
        }

        /* guess each of the keys */
        for (key_id=0;key_id<keycount;key_id++) {
            /* test pre-scales */
            uint16_t xor = keys[key_id].start;
            uint16_t mult = keys[key_id].mult;
            uint16_t add = keys[key_id].add;
            int i;

#ifdef ADX_VERIFY_DERIVED_KEYS
            {
                uint16_t test_start, test_mult, test_add;
                if (type == 8 && keys[key_id].key8) {
                    process_cri_key8(keys[key_id].key8, &test_start, &test_mult, &test_add);
                    VGM_LOG("key8: pre=%04x %04x %04x vs calc=%04x %04x %04x = %s (\"%s\")\n",
                            xor,mult,add, test_start,test_mult,test_add, xor==test_start && mult==test_mult && add==test_add ? "ok" : "ko", keys[key_id].key8);
                }
                else if (type == 9 && keys[key_id].key9) {
                    process_cri_key9(keys[key_id].key9, &test_start, &test_mult, &test_add);
                    VGM_LOG("key9: pre=%04x %04x %04x vs calc=%04x %04x %04x = %s (%"PRIu64")\n",
                            xor,mult,add, test_start,test_mult,test_add, xor==test_start && mult==test_mult && add==test_add ? "ok" : "ko", keys[key_id].key9);
                }
                continue;
            }
#endif

            for (i=0;i<bruteframe &&
                ((prescales[i]&keymask)==(xor&keymask) ||
                    prescales[i]==0);
                i++) {
                xor = xor * mult + add;
            }

            if (i == bruteframe)
            {
                /* test */
                for (i=0;i<scales_to_do &&
                    (scales[i]&keymask)==(xor&keymask);i++) {
                    xor = xor * mult + add;
                }
                if (i == scales_to_do)
                {
                    *xor_start = keys[key_id].start;
                    *xor_mult = keys[key_id].mult;
                    *xor_add = keys[key_id].add;

                    rc = 1;
                    goto find_key_cleanup;
                }
            }
        }
    }

find_key_cleanup:
    if (scales) free(scales);
    if (prescales) free(prescales);
    return rc;
}

