#ifndef _AHX_KEYS_H_
#define _AHX_KEYS_H_

#include <stdint.h>
#include <string.h>

typedef struct {
    const char* key8;           /* keystring used by type 8 encryption */
    uint64_t key9;              /* reserved (not seen) */
} ahxkey_info;

/**
 * List of known keys, from exes. Generally same as ADX keys as CRI's key init seems shared.
 */
static const ahxkey_info ahxkey8_list[] = {

        /* Amagami (PS2) [Enterbrain] */
        {"mituba",0},

        /* StormLover!! (PSP), StormLover Kai!! (PSP) [Vridge] */
        {"HEXDPFMDKPQW",0},

        /* Lucky Star: Net Idol Meister (PSP) [Vridge, Kadokawa Shoten] */
        /* Baka to Test to Shoukanjuu Portable (PSP) */
        {"JJOLIFJLE",0},

        /* Ishin Renka: Ryouma Gaiden (PSP) [Vridge] */
        {"LQAFJOIEJ",0},

        /* Lucky Star: Ryouou Gakuen Outousai Portable (PSP) [Vridge] */
        {"IUNOIRU",0},

        /* Marriage Royale: Prism Story (PSP) [Vridge] */
        {"ROYMAR",0},

        /* Nogizaka Haruka no Himitsu: Doujinshi Hajimemashita (PSP) [Vridge] */
        {"CLKMEOUHFLIE",0},

        /* Nichijou: Uchuujin (PSP) [Vridge] */
        {"LJLOUHIU787",0},

        /* StormLover Natsu Koi!! (PSP) [Vridge] */
        {"LIKDFJUIDJOQ",0},

        /* Corpse Party: Book of Shadows (PSP) */
        {"\x83\x76\x83\x89\x83\x60\x83\x69Lovers_Day",0}, // "プラチナLovers_Day" in SHIFT-JIS

};

static const int ahxkey8_list_count = sizeof(ahxkey8_list) / sizeof(ahxkey8_list[0]);

#endif /* _AHX_KEYS_H_ */
