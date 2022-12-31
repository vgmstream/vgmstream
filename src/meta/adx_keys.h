#ifndef _ADX_KEYS_H_
#define _ADX_KEYS_H_

#include <stdint.h>
#include <string.h>

typedef struct {
    uint16_t start, mult, add;  /* XOR values derived from the actual key */
    const char* key8;           /* keystring used by type 8 encryption */
    uint64_t key9;              /* keycode used by type 9 encryption */
} adxkey_info;

/**
 * List of known keys, cracked from the sound files.
 * Keystrings (type 8) and keycodes (type 9) from executables / VGAudio / game's executables / 2ch.net.
 * Multiple keys may work for a game due to how they are derived.
 * start/mult/add are optional (0,0,0) if key8/9 are provided, but take priority if given.
 */
static const adxkey_info adxkey8_list[] = {

        /* GOD HAND (PS2), Okami (PS2) [Clover Studio] */
        {0x49e1,0x4a57,0x553d, "karaage",0},

        /* Blood+ (PS2) [Grasshopper Manufacture] */
        {0x5f5d,0x58bd,0x55ed, "LOVELOVE",0}, // obfuscated keystring is "KNUDKNUD", adds +1 to chars to get final key

        /* Killer7 (PS2) [Grasshopper Manufacture] */
        {0x50fb,0x5803,0x5701, "GHM",0},

        /* Samurai Champloo (PS2) [Grasshopper Manufacture] */
        {0x4f3f,0x472f,0x562f, "GHMSC",0},

        /* Raiden III (PS2) [Moss] */
        {0x66f5,0x58bd,0x4459, "(C)2005 MOSS LTD. BMW Z4",0},

        /* Phantasy Star Universe (PC), Phantasy Star Universe: Ambition of the Illuminus (PS2) [Sonic Team] */
        {0x5deb,0x5f27,0x673f, "3x5k62bg9ptbwy",0},

        /* Senko no Ronde Rev.X (X360) [G.rev] */
        {0x46d3,0x5ced,0x474d, "ranatus",0},

        /* NiGHTS: Journey of Dreams (Wii) [Sonic Team] */
        {0x440b,0x6539,0x5723, "sakakit4649",0},

        /* The iDOLM@STER: Live For You (X360) [Bandai Namco] (.aix) */
        {0x586d,0x5d65,0x63eb, "75_501NO_003B",0},

        /* Shuffle! On the Stage (PS2) [Navel] */
        {0x4969,0x5deb,0x467f, "SHUF",0},

        /* Aoishiro (PS2) [Success] */
        {0x4d65,0x5eb7,0x5dfd, "wakasugi",0},

        /* Sonic and the Black Knight (Wii) [Sonic Team] */
        {0x55b7,0x6191,0x5a77, "morio",0},

        /* Amagami (PS2) [Enterbrain] */
        {0x5a17,0x509f,0x5bfd, "mituba",0},

        /* Yamasa Digi Portable: Matsuri no Tatsujin (PSP) [Yamasa] */
        {0x4c01,0x549d,0x676f, "7fa0xB9tw3",0},

        /* Fragments Blue (PS2) [Kadokawa Shoten] */
        {0x5803,0x4555,0x47bf, "PIETA",0},

        /* Soulcalibur IV (PS3) [Namco] */
        {0x59ed,0x4679,0x46c9, "SC4Test",0},

        /* Senko no Ronde DUO (X360/AC) [G.rev] */
        {0x6157,0x6809,0x4045, "yomesokushitsu",0},

        /* Nogizaka Haruka no Himitsu: Cosplay Hajimemashita (PS2) [Vridge] */
        {0x45af,0x5f27,0x52b1, "SKFHSIA",0},

        /* Little Anchor (PS2) [Vridge] */
        {0x5f65,0x5b3d,0x5f65, "KHNUJYTG",0},

        /* Hanayoi Romanesque: Ai to Kanashimi (PS2) [Marvelous] */
        {0x5563,0x5047,0x43ed, "HANAOTM",0},

        /* Mobile Suit Gundam: Gundam vs. Gundam NEXT PLUS (PSP) [Capcom] */
        {0x4f7b,0x4fdb,0x5cbf, "CS-GGNX+",0},

        /* Shoukan Shoujo: Elemental Girl Calling (PS2) [Bridge NetShop] */
        {0x4f7b,0x5071,0x4c61, "ELEMENGAL",0},

        /* Rakushou! Pachi-Slot Sengen 6: Rio 2 Cruising Vanadis (PS2) [Net Corporation] */
        {0x53e9,0x586d,0x4eaf, "waksde",0},

        /* Tears to Tiara Gaiden: Avalon no Nazo (PS3) [Aquaplus] */
        {0x47e1,0x60e9,0x51c1, "Hello TtT world!",0}, // obfuscated keystring xors 0xF0 to chars to get final key

        /* Neon Genesis Evangelion: Koutetsu no Girlfriend 2nd (PS2) [Broccoli] */
        {0x481d,0x4f25,0x5243, "eva2",0},

        /* Futakoi Alternative (PS2) [Marvelous] */
        {0x413b,0x543b,0x57d1, "LOVLOV",0},

        /* Gakuen Utopia: Manabi Straight! KiraKira Happy Festa! (PS2) [Marvelous] */
        {0x440b,0x4327,0x564b, "MANABIST",0},

        /* Soshite Kono Uchuu ni Kirameku Kimi no Shi XXX (PS2) [Datam Polystar] */
        {0x5f5d,0x552b,0x5507, "DATAM-KK2",0},

        /* Sakura Taisen: Atsuki Chishio ni (PS2) [Sega] */
        {0x645d,0x6011,0x5c29, "[Seq][ADX] illegal cri or libsd status.",0}, // actual keystring (obfuscation probably)

        /* Sakura Taisen Monogatari: Mysterious Paris (PS2) [Sega] */
        {0x62ad,0x4b13,0x5957, "inoue4126",0},

        /* Sotsugyou 2nd Generation (PS2) [Jinx] */
        {0x6305,0x509f,0x4c01, "MUSUMEG",0},

        /* Kin'iro no Corda -La Corda d'Oro- (PSP) [Koei] */
        {0x55b7,0x67e5,0x5387, "neo3corda",0}, // keystring as code, char by char

        /* Nanatsuiro * Drops Pure!! (PS2) [Media Works] */
        {0x6731,0x645d,0x566b, "NANAT",0},

        /* Shakugan no Shana (PS2) [Vridge] */
        {0x5fc5,0x63d9,0x599f, "FUZETSU",0},

        /* Uragiri wa Boku no Namae o Shitteiru (PS2) [Kadokawa Shoten] */
        {0x4c73,0x4d8d,0x5827, "URABOKU-penguin",0},

        /* StormLover!! (PSP), StormLover Kai!! (PSP) [Vridge] */
        {0x5a11,0x67e5,0x6751, "HEXDPFMDKPQW",0},

        /* Sora no Otoshimono: DokiDoki Summer Vacation (PSP) [Kadokawa Shoten] */
        {0x5e75,0x4a89,0x4c61, "funen-gomi",0},

        /* Boku wa Koukuu Kanseikan: Airport Hero Naha/Narita/Shinchitose/Haneda/Kankuu (PSP) [Sonic Powered] */
        {0x64ab,0x5297,0x632f, "sonic",0},

        /* Lucky Star: Net Idol Meister (PSP) [Vridge, Kadokawa Shoten] */
        /* Baka to Test to Shoukanjuu Portable (PSP) */
        {0x4d81,0x5243,0x58c7, "JJOLIFJLE",0},

        /* Ishin Renka: Ryouma Gaiden (PSP) [Vridge] */
        {0x54d1,0x526d,0x5e8b, "LQAFJOIEJ",0},

        /* Lucky Star: Ryouou Gakuen Outousai Portable (PSP) [Vridge] */
        {0x4d05,0x663b,0x6343, "IUNOIRU",0},

        /* Marriage Royale: Prism Story (PSP) [Vridge] */
        {0x40a9,0x46b1,0x62ad, "ROYMAR",0},

        /* Nogizaka Haruka no Himitsu: Doujinshi Hajimemashita (PSP) [Vridge] */
        {0x4609,0x671f,0x4b65, "CLKMEOUHFLIE",0},

        /* Slotter Mania P: Mach Go Go Go III (PSP) [Dorart] */
        {0x41ef,0x463d,0x5507, "SGGK",0},

        /* Nichijou: Uchuujin (PSP) [Vridge] */
        {0x4369,0x486d,0x5461, "LJLOUHIU787",0},

        /* R-15 Portable (PSP) [Kadokawa Shoten] */
        {0x6809,0x5fd5,0x5bb1, "R-15(Heart)Love",0},

        /* Suzumiya Haruhi-chan no Mahjong (PSP) [Kadokawa Shoten] */
        {0x5c33,0x4133,0x4ce7, "bi88a#fas",0},

        /* StormLover Natsu Koi!! (PSP) [Vridge] */
        {0x4133,0x5a01,0x5723, "LIKDFJUIDJOQ",0},

        /* Shounen Onmyouji: Tsubasa yo Ima, Sora e Kaere (PS2) [Kadokawa Shoten] */
        {0x55d9,0x46d3,0x5b01, "SONMYOJI",0},

        /* Girls Bravo: Romance 15's (PS2) [Kadokawa Shoten] */
        {0x658f,0x4a89,0x5213, "GBRAVO",0},

        /* Kashimashi! Girl Meets Girl: Hajimete no Natsu Monogatari (PS2) [Vridge] */
        {0x6109,0x5135,0x673f, "KASHIM",0},

        /* Bakumatsu Renka: Karyuu Kenshiden (PS2) [Vridge] */
        {0x4919,0x612d,0x4919, "RENRENKA22",0},

        /* Tensei Hakkenshi: Fuumaroku (PS2) [Vridge] */
        {0x5761,0x6283,0x4531, "HAKKEN",0},

        /* Lucky Star: Ryouou Gakuen Outousai (PS2) [Vridge] */
        {0x481D,0x44F9,0x4E35, "LSTARPS2",0},

        /* Bakumatsu Renka: Shinsengumi (PS2) [Vridge] */
        {0x5381,0x5701,0x665B, "SHINN",0},

        /* Gintama Gin-san to Issho! Boku no Kabukichou Nikki (PS2) [Bandai Namco?] */
        {0x67CD,0x5CA7,0x655F, "gt25809",0},

        /* Lucky Star: RAvish Romance (PS2) [Vridge] */
        {0x5347,0x4FB7,0x6415, "LUCKYSRARPS2",0},

        /* Katekyoo Hitman Reborn! Nerae! Ring x Bongole Trainers (PS2) */
        {0x61C7,0x4549,0x4337, "ORN2HITMAN",0},

        /* Katekyoo Hitman Reborn! Let's Ansatsu! Nerawareta 10-daime! (PS2) */
        {0x5381,0x52E5,0x53E9, "REBHITMAN",0},

        /* 428: Fuusasareta Shibuya de (PS3) */
        {0x52ff,0x649f,0x448f, "hj1kviaqqdzUacryoacwmscfvwtlfkVbbbqpqmzqnbile2euljywazejgyxxvqlf",0},

        /* Mirai Nikki: 13-ninme no Nikki Shoyuusha Re-Write (PSP) */
        {0x58a3,0x66f5,0x599f, "FDRW17th",0},
    
        /* Shoujo Yoshitsune-den Ni - Toki wo Koeru Chigiri (PS2) */
        {0x62d7,0x483d,0x4fb7, "YOSHI2",0},

};

static const adxkey_info adxkey9_list[] = {

        /* Phantasy Star Online 2 */
        {0x07d2,0x1ec5,0x0c7f, NULL,0},                     // guessed with degod

        /* Dragon Ball Z: Dokkan Battle (Android/iOS) */
        {0x0003,0x0d19,0x043b, NULL,416383518},             // 0000000018D1821E

        /* Kisou Ryouhei Gunhound EX (PSP) */
        {0x0005,0x0bcd,0x1add, NULL,683461999},             // 0000000028BCCD6F

        /* Raramagi (Android) */
        {0x0000,0x2b99,0x3e33, NULL,45719322},              // 0000000002B99F1A (12160794 also works)

        /* Sonic Runners (Android) */
        {0x0000,0x12fd,0x1fbd, NULL,19910623},              // 00000000012FCFDF

        /* Fallen Princess (iOS/Android) */
        {0x5e4b,0x190d,0x76bb, NULL,145552191146490718},    // 02051AF25990FB5E

        /* Yuuki Yuuna wa Yuusha de aru: Hanayui no Kirameki / Yuyuyui (iOS/Android) */
        {0x3f10,0x3651,0x6d31, NULL,4867249871962584729},   // 438BF1F883653699

        /* Super Robot Wars X-Omega (iOS/Android) voices */
        {0x5152,0x7979,0x152b, NULL,165521992944278},       // 0000968A97978A96

        /* AKA to BLUE (Android) */
        {0x03fc,0x0749,0x12EF, NULL,0},                     // guessed with VGAudio (possible key: 1FE0748978 / 136909719928)
      //{0x0c03,0x0749,0x1459, NULL,0},                     // 2nd guess (possible key: 6018748A2D / 412727151149)

        /* Mashiro Witch (Android) */
        {0x2669,0x1495,0x2407, NULL,0x55D11D3349495204},    // 55D11D3349495204

        /* Nogizaka46 Rhythm Festival (Android) */
        {0x2378,0x5511,0x0201, NULL,5613126134333697},      // 0013F11BC5510101

        /* Detective Conan Runner / Case Closed Runner (Android) */
        {0x0613,0x0e3d,0x6dff, NULL,1175268187653273344},   // 104f643098e3f700

        /* Persona 5 Royal (PS4) (asia?) **verified */
        {0x0000,0x1c85,0x7043, NULL,29915170},              // 0000000001C87822

        /* Persona 5 Royal (PS4) (japan?) **not verified */
        {0x0000,0x0000,0x0000, NULL,10882899},              // 0000000000A60F53

        /* Persona 5 Royal (PS4) (us/eu?) **not verified */
        {0x0000,0x0000,0x0000, NULL,22759300},              // 00000000015B4784

        /* Assault Lily Last Bullet (Android) */
        {0x0000,0x0000,0x0000, NULL,6349046567469313},      // 00168E6C99510101 (+ AWB subkeys)

        /* maimai DX Splash (AC) */
        {0x0000,0x0000,0x0000, NULL,9170825592834449000},   // 7F4551499DF55E68

        /* Sonic Colors Ultimate (multi) */
        {0x0000,0x0000,0x0000, NULL,1991062320101111},      // 000712DC5250B6F7

        /* Shin Megami Tensei V (Switch) */
        {0x0000,0x0000,0x0000, NULL,1731948526},            // 00000000673B6FEE

        /* Persona 5 Royal (PC) */
        {0x0000,0x0000,0x0000, NULL,9923540143823782},      // 002341683D2FDBA6

};

static const int adxkey8_list_count = sizeof(adxkey8_list) / sizeof(adxkey8_list[0]);
static const int adxkey9_list_count = sizeof(adxkey9_list) / sizeof(adxkey9_list[0]);

#endif/*_ADX_KEYS_H_*/
