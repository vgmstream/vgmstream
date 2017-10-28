#ifndef _ADX_KEYS_H_
#define _ADX_KEYS_H_

typedef struct {
    uint16_t start,mult,add;    /* XOR values derived from the actual key */
    //char* key8;               /* key in type 8 format (string) */
    //uint64_t key9;            /* key in type 9 format (64b number) */
} adxkey_info;

/**
 * List of known keys, cracked from the sound files.
 * Key string (type-8) and key codes (type-9) from VGAudio / game's executable.
 * For key derivation code see VGAudio.
 */
static const adxkey_info adxkey8_list[] = {
        /* Clover Studio (GOD HAND, Okami) */
        {0x49e1,0x4a57,0x553d},     // Key string: "karaage"

        /* Grasshopper Manufacture 0 (Blood+) */
        {0x5f5d,0x58bd,0x55ed},     // estimated

        /* Grasshopper Manufacture 1 (Killer7) */
        {0x50fb,0x5803,0x5701},     // estimated

        /* Grasshopper Manufacture 2 (Samurai Champloo) */
        {0x4f3f,0x472f,0x562f},     // confirmed unique with guessadx

        /* Moss Ltd (Raiden III) */
        {0x66f5,0x58bd,0x4459},     // estimated

        /* Sonic Team 0 (Phantasy Star Universe) */
        {0x5deb,0x5f27,0x673f},     // Key string: "3x5k62bg9ptbwy"

        /* G.rev 0 (Senko no Ronde) */
        {0x46d3,0x5ced,0x474d},     // estimated

        /* Sonic Team 1 (NiGHTS: Journey of Dreams) */
        {0x440b,0x6539,0x5723},     // this seems to be dead on, but still estimated

        /* unknown source */
        {0x586d,0x5d65,0x63eb},     // from guessadx (unique?)

        /* Navel (Shuffle! On the Stage) */
        {0x4969,0x5deb,0x467f},     // 2nd key from guessadx

        /* Success (Aoishiro) */
        {0x4d65,0x5eb7,0x5dfd},     // 1st key from guessadx

        /* Sonic Team 2 (Sonic and the Black Knight) */
        {0x55b7,0x6191,0x5a77},     // Key string: "morio"

        /* Enterbrain (Amagami) */
        {0x5a17,0x509f,0x5bfd},     // Key string: "mituba"

        /* Yamasa (Yamasa Digi Portable: Matsuri no Tatsujin) */
        {0x4c01,0x549d,0x676f},     // confirmed unique with guessadx

        /* Kadokawa Shoten (Fragments Blue) */
        {0x5803,0x4555,0x47bf},     // confirmed unique with guessadx

        /* Namco (Soulcalibur IV) */
        {0x59ed,0x4679,0x46c9},     // confirmed unique with guessadx

        /* G.rev 1 (Senko no Ronde DUO) */
        {0x6157,0x6809,0x4045},     // from guessadx

        /* ASCII Media Works 0 (Nogizaka Haruka no Himitsu: Cosplay Hajimemashita) */
        {0x45af,0x5f27,0x52b1},     // 2nd from guessadx, other was {0x45ad,0x5f27,0x10fd}

        /* D3 Publisher 0 (Little Anchor) */
        {0x5f65,0x5b3d,0x5f65},     // confirmed unique with guessadx

        /* Marvelous 0 (Hanayoi Romanesque: Ai to Kanashimi) */
        {0x5563,0x5047,0x43ed},     // 2nd from guessadx, other was {0x5562,0x5047,0x1433}

        /* Capcom (Mobile Suit Gundam: Gundam vs. Gundam NEXT PLUS) */
        {0x4f7b,0x4fdb,0x5cbf},     // confirmed unique with guessadx

        /* Developer: Bridge NetShop
         * Publisher: Kadokawa Shoten (Shoukan Shoujo: Elemental Girl Calling) */
        {0x4f7b,0x5071,0x4c61},     // confirmed unique with guessadx

        /* Developer: Net Corporation
         * Publisher: Tecmo (Rakushou! Pachi-Slot Sengen 6: Rio 2 Cruising Vanadis) */
        {0x53e9,0x586d,0x4eaf},     // confirmed unique with guessadx

        /* Developer: Aquaplus
         * Tears to Tiara Gaiden Avalon no Kagi (PS3) */
        {0x47e1,0x60e9,0x51c1},     // confirmed unique with guessadx

        /* Developer: Broccoli
         * Neon Genesis Evangelion: Koutetsu no Girlfriend 2nd (PS2) */
        {0x481d,0x4f25,0x5243},     // confirmed unique with guessadx

        /* Developer: Marvelous
         * Futakoi Alternative (PS2) */
        {0x413b,0x543b,0x57d1},     // confirmed unique with guessadx

        /* Developer: Marvelous
         * Gakuen Utopia - Manabi Straight! KiraKira Happy Festa! (PS2) */
         {0x440d,0x4327,0x4fff},    // Second guess from guessadx, other was {0x440b,0x4327,0x564b}

        /* Developer: Datam Polystar
         * Soshite Kono Uchuu ni Kirameku Kimi no Shi XXX (PS2) */
        {0x5f5d,0x552b,0x5507},     // confirmed unique with guessadx

        /* Developer: Sega
         * Sakura Taisen: Atsuki Chishio Ni (PS2) */
        {0x645d,0x6011,0x5c29},     // confirmed unique with guessadx

        /* Developer: Sega
         * Sakura Taisen 3 ~Paris wa Moeteiru ka~ (PS2) */
        {0x62ad,0x4b13,0x5957},     // confirmed unique with guessadx

        /* Developer: Jinx
         * Sotsugyou 2nd Generation (PS2) */
        {0x6305,0x509f,0x4c01},     // First guess from guessadx, other was {0x6307,0x509f,0x2ac5}

        /* La Corda d'Oro (2005)(-)(Koei)[PSP] */
        {0x55b7,0x67e5,0x5387},     // confirmed unique with guessadx

        /* Nanatsuiro * Drops Pure!! (2007)(Media Works)[PS2] */
        {0x6731,0x645d,0x566b},     // confirmed unique with guessadx

        /* Shakugan no Shana (2006)(Vridge)(Media Works)[PS2] */
        {0x5fc5,0x63d9,0x599f},     // confirmed unique with guessadx

        /* Uragiri wa Boku no Namae o Shitteiru (2010)(Kadokawa Shoten)[PS2] */
        {0x4c73,0x4d8d,0x5827},     // confirmed unique with guessadx

        /* StormLover Kai!! (2012)(D3 Publisher)[PSP] */
        {0x5a11,0x67e5,0x6751},     // confirmed unique with guessadx

        /* Sora no Otoshimono - DokiDoki Summer Vacation (2010)(Kadokawa Shoten)[PSP] */
        {0x5e75,0x4a89,0x4c61},     // confirmed unique with guessadx

        /* Boku wa Koukuu Kanseikan - Airport Hero Naha (2006)(Sonic Powered)(Electronic Arts)[PSP] */
        {0x64ab,0x5297,0x632f},     // confirmed unique with guessadx

        /* Lucky Star - Net Idol Meister (2009)(Kadokawa Shoten)[PSP] */
        {0x4d82,0x5243,0x685},      // confirmed unique with guessadx

        /* Ishin Renka: Ryouma Gaiden (2010-11-25)(-)(D3 Publisher)[PSP] */
        {0x54d1,0x526d,0x5e8b},     // ?

        /* Lucky Star - Ryouou Gakuen Outousai Portable (2010-12-22)(-)(Kadokawa Shoten)[PSP] */
        {0x4d06,0x663b,0x7d09},     // ?

        /* Marriage Royale - Prism Story (2010-04-28)(-)(ASCII Media Works)[PSP] */
        {0x40a9,0x46b1,0x62ad},     // ?

        /* Nogizaka Haruka no Himitsu - Doujinshi Hajime Mashita (2010-10-28)(-)(ASCII Media Works)[PSP] */
        {0x4601,0x671f,0x0455},     // ?

        /* Slotter Mania P - Mach Go Go Go III (2011-01-06)(-)(Dorart)[PSP] */
        {0x41ef,0x463d,0x5507},     // ?

        /* Nichijou - Uchuujin (2011-07-28)(-)(Kadokawa Shoten)[PSP] */
        {0x4369,0x486d,0x5461},     // ?

        /* R-15 Portable (2011-10-27)(-)(Kadokawa Shoten)[PSP] */
        {0x6809,0x5fd5,0x5bb1},

        /* Suzumiya Haruhi-chan no Mahjong (2011-07-07)(-)(Kadokawa Shoten)[PSP] */
        {0x5c33,0x4133,0x4ce7},     // ?

        /* Storm Lover Natsu Koi!! (2011-08-04)(Vridge)(D3 Publisher) */
        {0x4133,0x5a01,0x5723},     // ?

};

static const adxkey_info adxkey9_list[] = {

        /* Phantasy Star Online 2 */
        {0x07d2,0x1ec5,0x0c7f},     // guessed with degod

        /* Dragon Ball Z: Dokkan Battle */
        {0x0003,0x0d19,0x043b},     // Key code: 416383518

        /* Kisou Ryouhei Gunhound EX (2013-01-31)(Dracue)[PSP] */
        {0x0005,0x0bcd,0x1add},     // Key code: 683461999

        /* Raramagi (Android) */
        {0x0000,0x0b99,0x1e33},     // Key code: 12160794

        /* Sonic Runners (Android) */
        {0x0000,0x12fd,0x1fbd},     // Key code: 19910623

};

static const int adxkey8_list_count = sizeof(adxkey8_list) / sizeof(adxkey8_list[0]);
static const int adxkey9_list_count = sizeof(adxkey9_list) / sizeof(adxkey9_list[0]);


#endif/*_ADX_KEYS_H_*/
