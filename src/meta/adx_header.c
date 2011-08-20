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

VGMSTREAM * init_vgmstream_adx(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t stream_offset;
    size_t filesize;
    uint16_t version_signature;
    int loop_flag=0;
    int channel_count;
    int32_t loop_start_offset=0;
    int32_t loop_end_offset=0;
    int32_t loop_start_sample=0;
    int32_t loop_end_sample=0;
    meta_t header_type;
    int16_t coef1, coef2;
    uint16_t cutoff;
    char filename[260];
    int coding_type = coding_CRI_ADX;
    uint16_t xor_start=0,xor_mult=0,xor_add=0;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("adx",filename_extension(filename))) goto fail;

    filesize = get_streamfile_size(streamFile);

    /* check first 2 bytes */
    if ((uint16_t)read_16bitBE(0,streamFile)!=0x8000) goto fail;

    /* get stream offset, check for CRI signature just before */
    stream_offset = (uint16_t)read_16bitBE(2,streamFile) + 4;
    if ((uint16_t)read_16bitBE(stream_offset-6,streamFile)!=0x2863 ||/* "(c" */
        (uint32_t)read_32bitBE(stream_offset-4,streamFile)!=0x29435249 /* ")CRI" */
       ) goto fail;

    /* check for encoding type */
    /* 2 is for some unknown fixed filter, 3 is standard ADX, 4 is
     * ADX with exponential scale, 0x11 is AHX */
    if (read_8bit(4,streamFile) != 3) goto fail;

    /* check for frame size (only 18 is supported at the moment) */
    if (read_8bit(5,streamFile) != 18) goto fail;

    /* check for bits per sample? (only 4 makes sense for ADX) */
    if (read_8bit(6,streamFile) != 4) goto fail;

    /* check version signature, read loop info */
    version_signature = read_16bitBE(0x12,streamFile);
    /* encryption */
    if (version_signature == 0x0408) {
        if (find_key(streamFile, 8, &xor_start, &xor_mult, &xor_add))
        {
            coding_type = coding_CRI_ADX_enc_8;
            version_signature = 0x0400;
        }
    }
    else if (version_signature == 0x0409) {
        if (find_key(streamFile, 9, &xor_start, &xor_mult, &xor_add))
        {
            coding_type = coding_CRI_ADX_enc_9;
            version_signature = 0x0400;
        }
    }

    if (version_signature == 0x0300) {      /* type 03 */
        header_type = meta_ADX_03;
        if (stream_offset-6 >= 0x2c) {   /* enough space for loop info? */
            loop_flag = (read_32bitBE(0x18,streamFile) != 0);
            loop_start_sample = read_32bitBE(0x1c,streamFile);
            loop_start_offset = read_32bitBE(0x20,streamFile);
            loop_end_sample = read_32bitBE(0x24,streamFile);
            loop_end_offset = read_32bitBE(0x28,streamFile);
        }
    } else if (version_signature == 0x0400) {

        off_t	ainf_info_length=0;

        if((uint32_t)read_32bitBE(0x24,streamFile)==0x41494E46) /* AINF Header */
            ainf_info_length = (off_t)read_32bitBE(0x28,streamFile);

        header_type = meta_ADX_04;
        if (stream_offset-ainf_info_length-6 >= 0x38) {   /* enough space for loop info? */
		if (read_32bitBE(0x24,streamFile) == 0xFFFEFFFE)
			loop_flag = 0;
		else
			loop_flag = (read_32bitBE(0x24,streamFile) != 0);

            loop_start_sample = read_32bitBE(0x28,streamFile);
            loop_start_offset = read_32bitBE(0x2c,streamFile);
            loop_end_sample = read_32bitBE(0x30,streamFile);
            loop_end_offset = read_32bitBE(0x34,streamFile);
        }
    } else if (version_signature == 0x0500) {			 /* found in some SFD : Buggy Heat, appears to have no loop */
        header_type = meta_ADX_05;
    } else goto fail;   /* not a known/supported version signature */

    /* At this point we almost certainly have an ADX file,
     * so let's build the VGMSTREAM. */

    /* high-pass cutoff frequency, always 500 that I've seen */
    cutoff = (uint16_t)read_16bitBE(0x10,streamFile);

    channel_count = read_8bit(7,streamFile);
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->num_samples = read_32bitBE(0xc,streamFile);
    vgmstream->sample_rate = read_32bitBE(8,streamFile);
    /* channels and loop flag are set by allocate_vgmstream */
    vgmstream->loop_start_sample = loop_start_sample;
    vgmstream->loop_end_sample = loop_end_sample;

    vgmstream->coding_type = coding_type;
    if (channel_count==1)
        vgmstream->layout_type = layout_none;
    else
        vgmstream->layout_type = layout_interleave;
    vgmstream->meta_type = header_type;

    vgmstream->interleave_block_size=18;

    /* calculate filter coefficients */
    {
        double x,y,z,a,b,c;

        x = cutoff;
        y = vgmstream->sample_rate;
        z = cos(2.0*M_PI*x/y);

        a = M_SQRT2-z;
        b = M_SQRT2-1.0;
        c = (a-sqrt((a+b)*(a-b)))/b;

        coef1 = floor(c*8192);
        coef2 = floor(c*c*-4096);
    }

    {
        int i;
        STREAMFILE * chstreamfile;
       
        /* ADX is so tightly interleaved that having two buffers is silly */
        chstreamfile = streamFile->open(streamFile,filename,18*0x400);
        if (!chstreamfile) goto fail;

        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = chstreamfile;

            vgmstream->ch[i].channel_start_offset=
                vgmstream->ch[i].offset=
                stream_offset+18*i;

            vgmstream->ch[i].adpcm_coef[0] = coef1;
            vgmstream->ch[i].adpcm_coef[1] = coef2;

            if (coding_type == coding_CRI_ADX_enc_8 ||
                coding_type == coding_CRI_ADX_enc_9)
            {
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

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}

/* guessadx stuff */

/* type 8 keys */
static struct {
    uint16_t start,mult,add;
} keys_8[] = {
    /* Clover Studio (GOD HAND, Okami) */
    /* I'm pretty sure this is right, based on a decrypted version of some GOD HAND tracks. */
    /* Also it is the 2nd result from guessadx */
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
    /* this is estimated */
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
    /* confirmed unique with guessadx */
    {0x55b7,0x6191,0x5a77},

    /* Enterbrain (Amagami) */
    /* one of 32 from guessadx */
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
};

/* type 9 keys */
static struct {
    uint16_t start,mult,add;
} keys_9[] = {
    /* Phantasy Star Online 2
     * guessed with degod */
    {0x07d2,0x1ec5,0x0c7f},
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
            /* guess each of the keys */
            for (key_id=0;key_id<keys_8_count;key_id++) {
                /* test pre-scales */
                uint16_t xor = keys_8[key_id].start;
                uint16_t mult = keys_8[key_id].mult;
                uint16_t add = keys_8[key_id].add;
                int i;

                for (i=0;i<bruteframe &&
                        ((prescales[i]&0x6000)==(xor&0x6000) ||
                         prescales[i]==0);
                        i++) {
                    xor = xor * mult + add;
                }

                if (i == bruteframe)
                {
                    /* test */
                    for (i=0;i<scales_to_do &&
                            (scales[i]&0x6000)==(xor&0x6000);i++) {
                        xor = xor * mult + add;
                    }
                    if (i == scales_to_do)
                    {
                        *xor_start = keys_8[key_id].start;
                        *xor_mult = keys_8[key_id].mult;
                        *xor_add = keys_8[key_id].add;

                        rc = 1;
                        goto find_key_cleanup;
                    }
                }
            }
        }
        else if (type == 9)
        {
            /* smarter XOR as seen in PSO2, can't do an exact match so we
             * have to search for the lowest */
            long best_score = MAX_FRAMES * 0x1fff;

            /* guess each of the keys */
            for (key_id=0;key_id<keys_9_count;key_id++) {
                /* run past pre-scales */
                uint16_t xor = keys_9[key_id].start;
                uint16_t mult = keys_9[key_id].mult;
                uint16_t add = keys_9[key_id].add;
                int i;
                long total_score = 0;

                for (i=0;i<bruteframe;i++) {
                    xor = xor * mult + add;
                }

                if (i == bruteframe)
                {
                    /* test */
                    for (i=0;i<scales_to_do && total_score < best_score;i++) {
                        xor = xor * mult + add;
                        total_score += (scales[i]^xor)&0x1fff;
                    }

                    if (total_score < best_score)
                    {
                        *xor_start = keys_9[key_id].start;
                        *xor_mult = keys_9[key_id].mult;
                        *xor_add = keys_9[key_id].add;

                        best_score = total_score;
                    }
                }
            }

            /* arbitrarily decide if we have won? */
            if (best_score < scales_to_do * 0x1000)
            {
                rc = 1;
            }
        }
    }

find_key_cleanup:
    if (scales) free(scales);
    if (prescales) free(prescales);
    return rc;
}

