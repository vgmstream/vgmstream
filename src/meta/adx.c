#ifdef _MSC_VER
#define _USE_MATH_DEFINES
#endif
#include <math.h>
#include <string.h>
#include <limits.h>
#include "meta.h"

#include "../coding/coding.h"
#include "../util.h"

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


    /* check extension, case insensitive */
    if (!check_extensions(streamFile,"adx")) goto fail;

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

/* guessadx stuff */

/* type 8 keys */
static struct {
    uint16_t start,mult,add;
} keys_8[] = {
    /* Clover Studio (GOD HAND, Okami) */
    /* Verified by VGAudio and the game's executable */
    /* Key string: karaage */
    {0x49e1,0x4a57,0x553d},

    /* Grasshopper Manufacture 0 (Blood+) */
    /* this is estimated */
    {0x5f5d,0x58bd,0x55ed},

    /* Grasshopper Manufacture 1 (Killer7) */
    /* this is estimated */
    {0x50fb,0x5803,0x5701},

    /* Grasshopper Manufacture 2 (Samurai Champloo) */
    /* confirmed unique with guessadx */
    {0x4f3f,0x472f,0x562f},

    /* Moss Ltd (Raiden III) */
    /* this is estimated */
    {0x66f5,0x58bd,0x4459},

    /* Sonic Team 0 (Phantasy Star Universe) */
    /* Verified by VGAudio and the game's executable */
    /* Key string: 3x5k62bg9ptbwy */
    {0x5deb,0x5f27,0x673f},

    /* G.rev 0 (Senko no Ronde) */
    /* this is estimated */
    {0x46d3,0x5ced,0x474d},

    /* Sonic Team 1 (NiGHTS: Journey of Dreams) */
    /* this seems to be dead on, but still estimated */
    {0x440b,0x6539,0x5723},

    /* from guessadx (unique?), unknown source */
    {0x586d,0x5d65,0x63eb},

    /* Navel (Shuffle! On the Stage) */
    /* 2nd key from guessadx */
    {0x4969,0x5deb,0x467f},

    /* Success (Aoishiro) */
    /* 1st key from guessadx */
    {0x4d65,0x5eb7,0x5dfd},

    /* Sonic Team 2 (Sonic and the Black Knight) */
    /* Verified by VGAudio and the game's executable */
    /* Key string: morio */
    {0x55b7,0x6191,0x5a77},

    /* Enterbrain (Amagami) */
    /* Verified by VGAudio and the game's executable */
    /* Key string: mituba */
    {0x5a17,0x509f,0x5bfd},

    /* Yamasa (Yamasa Digi Portable: Matsuri no Tatsujin) */
    /* confirmed unique with guessadx */
    {0x4c01,0x549d,0x676f},

    /* Kadokawa Shoten (Fragments Blue) */
    /* confirmed unique with guessadx */
    {0x5803,0x4555,0x47bf},

    /* Namco (Soulcalibur IV) */
    /* confirmed unique with guessadx */
    {0x59ed,0x4679,0x46c9},

    /* G.rev 1 (Senko no Ronde DUO) */
    /* from guessadx */
    {0x6157,0x6809,0x4045},

    /* ASCII Media Works 0 (Nogizaka Haruka no Himitsu: Cosplay Hajimemashita) */
    /* 2nd from guessadx, other was {0x45ad,0x5f27,0x10fd} */
    {0x45af,0x5f27,0x52b1},

    /* D3 Publisher 0 (Little Anchor) */
    /* confirmed unique with guessadx */
    {0x5f65,0x5b3d,0x5f65},

    /* Marvelous 0 (Hanayoi Romanesque: Ai to Kanashimi) */
    /* 2nd from guessadx, other was {0x5562,0x5047,0x1433} */
    {0x5563,0x5047,0x43ed},

	/* Capcom (Mobile Suit Gundam: Gundam vs. Gundam NEXT PLUS) */
    /* confirmed unique with guessadx */
    {0x4f7b,0x4fdb,0x5cbf},

	/* Developer: Bridge NetShop
	 * Publisher: Kadokawa Shoten (Shoukan Shoujo: Elemental Girl Calling) */
    /* confirmed unique with guessadx */
    {0x4f7b,0x5071,0x4c61},

	/* Developer: Net Corporation
	 * Publisher: Tecmo (Rakushou! Pachi-Slot Sengen 6: Rio 2 Cruising Vanadis) */
    /* confirmed unique with guessadx */
    {0x53e9,0x586d,0x4eaf},
	
	/* Developer: Aquaplus
	 * Tears to Tiara Gaiden Avalon no Kagi (PS3) */
	/* confirmed unique with guessadx */
	{0x47e1,0x60e9,0x51c1},

	/* Developer: Broccoli
	 * Neon Genesis Evangelion: Koutetsu no Girlfriend 2nd (PS2) */
    /* confirmed unique with guessadx */
	{0x481d,0x4f25,0x5243},

	/* Developer: Marvelous
	 * Futakoi Alternative (PS2) */
    /* confirmed unique with guessadx */
	{0x413b,0x543b,0x57d1},

	/* Developer: Marvelous
	 * Gakuen Utopia - Manabi Straight! KiraKira Happy Festa! (PS2)
	 * Second guess from guessadx, other was 
	 *   {0x440b,0x4327,0x564b} 
	 **/
	 {0x440d,0x4327,0x4fff},

	/* Developer: Datam Polystar
	 * Soshite Kono Uchuu ni Kirameku Kimi no Shi XXX (PS2) */
    /* confirmed unique with guessadx */
	{0x5f5d,0x552b,0x5507},

	/* Developer: Sega
	 * Sakura Taisen: Atsuki Chishio Ni (PS2) */
    /* confirmed unique with guessadx */
	{0x645d,0x6011,0x5c29},

	/* Developer: Sega
	 * Sakura Taisen 3 ~Paris wa Moeteiru ka~ (PS2) */
    /* confirmed unique with guessadx */
	{0x62ad,0x4b13,0x5957},

	/* Developer: Jinx
	 * Sotsugyou 2nd Generation (PS2)
     * First guess from guessadx, other was 
	 *   {0x6307,0x509f,0x2ac5} 
	 */
	{0x6305,0x509f,0x4c01},

    /*
     * La Corda d'Oro (2005)(-)(Koei)[PSP]
     * confirmed unique with guessadx */
    {0x55b7,0x67e5,0x5387},

    /*
     * Nanatsuiro * Drops Pure!! (2007)(Media Works)[PS2]
     * confirmed unique with guessadx */
    {0x6731,0x645d,0x566b},

    /*
     * Shakugan no Shana (2006)(Vridge)(Media Works)[PS2]
     * confirmed unique with guessadx */
    {0x5fc5,0x63d9,0x599f},

    /*
     * Uragiri wa Boku no Namae o Shitteiru (2010)(Kadokawa Shoten)[PS2]
     * confirmed unique with guessadx */
    {0x4c73,0x4d8d,0x5827},

	/*
     * StormLover Kai!! (2012)(D3 Publisher)[PSP]
     * confirmed unique with guessadx */
    {0x5a11,0x67e5,0x6751},
    
	/*
     * Sora no Otoshimono - DokiDoki Summer Vacation (2010)(Kadokawa Shoten)[PSP]
     * confirmed unique with guessadx */
    {0x5e75,0x4a89,0x4c61},
    
    /*
     * Boku wa Koukuu Kanseikan - Airport Hero Naha (2006)(Sonic Powered)(Electronic Arts)[PSP]
     * confirmed unique with guessadx */
    {0x64ab,0x5297,0x632f},
    
	/*
     * Lucky Star - Net Idol Meister (2009)(Kadokawa Shoten)[PSP]
     * confirmed unique with guessadx */
    {0x4d82,0x5243,0x685},
    
    /*
     * Ishin Renka: Ryouma Gaiden (2010-11-25)(-)(D3 Publisher)[PSP]
     */
    {0x54d1,0x526d,0x5e8b},
    
    /*
     * Lucky Star - Ryouou Gakuen Outousai Portable (2010-12-22)(-)(Kadokawa Shoten)[PSP]
     */
    {0x4d06,0x663b,0x7d09},

    /*
     * Marriage Royale - Prism Story (2010-04-28)(-)(ASCII Media Works)[PSP]
     */
    {0x40a9,0x46b1,0x62ad},

    /*
     * Nogizaka Haruka no Himitsu - Doujinshi Hajime Mashita (2010-10-28)(-)(ASCII Media Works)[PSP]
     */
    {0x4601,0x671f,0x0455},

    /*
     * Slotter Mania P - Mach Go Go Go III (2011-01-06)(-)(Dorart)[PSP]
     */
    {0x41ef,0x463d,0x5507},

    /*
     * Nichijou - Uchuujin (2011-07-28)(-)(Kadokawa Shoten)[PSP]
     */
    {0x4369,0x486d,0x5461},

    /*
     * R-15 Portable (2011-10-27)(-)(Kadokawa Shoten)[PSP]
     */
    {0x6809,0x5fd5,0x5bb1},

    /*
     * Suzumiya Haruhi-chan no Mahjong (2011-07-07)(-)(Kadokawa Shoten)[PSP]
     */
    {0x5c33,0x4133,0x4ce7},

	// Storm Lover Natsu Koi!! (2011-08-04)(Vridge)(D3 Publisher)
	{0x4133,0x5a01,0x5723},

};

/* type 9 keys (may not be autodetected correctly) */
static struct {
    uint16_t start,mult,add;
} keys_9[] = {
    /* Phantasy Star Online 2
     * guessed with degod */
    {0x07d2,0x1ec5,0x0c7f},

    /* Dragon Ball Z: Dokkan Battle
     * Verified by VGAudio 
     * Key code: 416383518 */
    {0x0003,0x0d19,0x043b},

    /* Kisou Ryouhei Gunhound EX (2013-01-31)(Dracue)[PSP]
     * Verified by VGAudio 
     * Key code: 683461999 */
    {0x0005,0x0bcd,0x1add},

    /* Raramagi [Android]
     * Verified by VGAudio 
     * Key code: 12160794 */
    {0x0000,0x0b99,0x1e33},

    /* Sonic runners [Android]
     * Verified by VGAudio 
     * Key code: 19910623 */
    {0x0000,0x12fd,0x1fbd},

};

static const int keys_8_count = sizeof(keys_8)/sizeof(keys_8[0]);
static const int keys_9_count = sizeof(keys_9)/sizeof(keys_9[0]);

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

        if ( read_key_file(keybuf, 6, file) ) {
            *xor_start = get_16bitBE(keybuf+0);
            *xor_mult = get_16bitBE(keybuf+2);
            *xor_add = get_16bitBE(keybuf+4);
            return 1;
        }
    }


    /* guess key from the tables above */
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
#define MAX_FRAMES (INT_MAX/0x8000)
        struct { uint16_t start, mult, add; } *keys = NULL;
        int keycount = 0, keymask = 0;
        int scales_to_do;
        int key_id;

        /* allocate storage for scales */
        scales_to_do = (bruteframecount > MAX_FRAMES ? MAX_FRAMES : bruteframecount);
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
            keys = &keys_8;
            keycount = keys_8_count;
            keymask = 0x6000;
        }
        else if (type == 9)
        {
            /* smarter XOR as seen in PSO2. The scale is technically 13 bits,
             * but the maximum value assigned by the encoder is 0x1000.
             * This is written to the ADX file as 0xFFF, leaving the high bit
             * empty, which is used to validate a key */
            keys = &keys_9;
            keycount = keys_9_count;
            keymask = 0x1000;
        }

        /* guess each of the keys */
        for (key_id=0;key_id<keycount;key_id++) {
            /* test pre-scales */
            uint16_t xor = keys[key_id].start;
            uint16_t mult = keys[key_id].mult;
            uint16_t add = keys[key_id].add;
            int i;

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

