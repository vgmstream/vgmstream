#ifndef _HCA_KEYS_H_
#define _HCA_KEYS_H_

#include "hca_keys_awb.h"

typedef struct {
    uint64_t key;               /* hca key or seed ('user') key */
#if 0
    const uint16_t* subkeys;    /* scramble subkey table for seed key */
    size_t subkeys_size;        /* size of the derivation subkey table */
#endif
} hcakey_info;


/**
 * List of known keys, extracted from the game files (several found in 2ch.net, others from data analisys).
 * CRI's tools expect an unsigned 64 bit number string, but keys are commonly found online in hex form.
 * Keys only use 56 bits though, so the upper 8 bits can be ignored.
 *
 * Some ACB+AWB after mid 2018 use a user seed key + a scramble subkey in the AWB (normally 16b LE at 0x0e)
 * to create the final HCA key, which means there is one key per AWB (so most HCA have a unique key).
 * vgmstream derives the key if subkey table is provided.
 */
static const hcakey_info hcakey_list[] = {

        // CRI HCA decoder default
        {9621963164387704},         // CF222F1FE0748978

        // Phantasy Star Online 2 (multi?)
        // used by most console games
        {14723751768204501419u},    // CC55463930DBE1AB

        // Old Phantasy Star Online 2 (multi?)
        {61891147883431481},        // 30DBE1ABCC554639

        // Jojo All Star Battle (PS3)
        {19700307},                 // 00000000012C9A53

        // Ro-Kyu-Bu! Himitsu no Otoshimono (PSP)
        {2012082716},               // 0000000077EDF21C

        // VRIDGE Inc. games:
        // - HatsuKare * Renai Debut Sengen! (PSP)
        // - Seitokai no Ichizon Lv. 2 Portable (PSP)
        // - Koi wa Kousoku ni Shibararenai! (PSP)
        // - StormLover 2nd (PSP)
        // - Prince of Stride (PSVita)
        // - Ro-Kyu-Bu! Naisho no Shutter Chance (PSVita)
        {1234253142},               // 0000000049913556

        // THE iDOLM@STER Cinderella Girls: Starlight Stage (iOS/Android)
        // Shadowverse (iOS/Android)
        {59751358413602},           // 00003657F27E3B22

        // Grimoire ~Shiritsu Grimoire Mahou Gakuen~ (iOS/Android)
        {5027916581011272},         // 0011DCDD0DC57F48

        // Idol Connect (iOS/Android)
        {2424},                     // 0000000000000978

        // Kamen Rider Battle Rush (iOS/Android)
        {29423500797988784},        // 00688884A11CCFB0

        // SD Gundam Strikers (iOS/Android)
        {30260840980773},           // 00001B85A6AD6125

        // Sonic Runners (iOS/Android)
        {19910623},                 // 00000000012FCFDF

        // Fate/Grand Order (iOS/Android)
        {12345},                    // 0000000000003039 - base assets
        {9117927877783581796},      // 7E89631892EBF464 - downloaded assets *unconfirmed

        // Raramagi (iOS/Android)
        {45719322},                 // 0000000002B99F1A

        // THE iDOLM@STER Million Live! (iOS/Android)
        // THE iDOLM@STER SideM GROWING STARS (Android)
        {765765765765765},          // 0002B875BC731A85

        // Kurokishi to Shiro no Maou (iOS/Android)
        {3003875739822025258},      // 29AFE911F5816A2A

        // Puella Magi Madoka Magica Side Story: Magia Record (iOS/Android)
        // Hortensia Saga (iOS/Android)
        {20536401},                 // 0000000001395C51

        // The Tower of Princess (iOS/Android)
        {9101518402445063},         // 002055C8634B5F07

        // Fallen Princess (iOS/Android)
        {145552191146490718},       // 02051AF25990FB5E

        // Diss World (iOS/Android)
        {9001712656335836006},      // 7CEC81F7C3091366

        // Ikemen Vampire - Ijin-tachi to Koi no Yuuwaku (iOS/Android)
        {45152594117267709},        // 00A06A0B8D0C10FD

        // Super Robot Wars X-Omega (iOS/Android)
        {165521992944278},          // 0000968A97978A96

        // BanG Dream! Girls Band Party! (iOS/Android)
        {8910},                     // 00000000000022CE

        // Tokyo 7th Sisters (iOS/Android) *unconfirmed
        {18279639311550860193u},    // FDAE531AAB414BA1

        // One Piece Dance Battle (iOS/Android)
        {1905818},                  // 00000000001D149A

        // Derby Stallion Masters (iOS/Android)
        {19840202},                 // 00000000012EBCCA

        // World Chain (iOS/Android)
        {4892292804961027794},      // 43E4EA62B8E6C6D2

        // Yuuki Yuuna wa Yuusha de aru - Hanayui no Kirameki / Yuyuyui (iOS/Android)
        {4867249871962584729},      // 438BF1F883653699

        // Tekken Mobile (iOS/Android)
        {18446744073709551615u},    // FFFFFFFFFFFFFFFF

        // Tales of the Rays (iOS/Android)
        {9516284},                  // 00000000009134FC

        // Skylock - Kamigami to Unmei no Itsutsuko (iOS/Android)
        {49160768297},              // 0000000B7235CB29

        // Tokyo Ghoul: Re Invoke (iOS/Android)
        {6929101074247145},         // 00189DFB1024ADE9

        // Azur Lane (iOS/Android)
        {621561580448882},          // 0002354E95356C72

        // One Piece Treasure Cruise (iOS/Android)
        {1224},                     // 00000000000004C8

        // Schoolgirl Strikers ~Twinkle Melodies~ (iOS/Android)
        {15806334760965177344u},    // DB5B61B8343D0000

        // Bad Apple Wars (PSVita)
        {241352432},                // 000000000E62BEF0

        // Koi to Senkyo to Chocolate Portable (PSP)
        {243812156},                // 000000000E88473C

        // Custom Drive (PSP)
        {2012062010},               // 0000000077EDA13A

        // Root Letter (PSVita)
        {1547531215412131},         // 00057F78B05F9BA3

        // Pro Evolution Soccer 2018 / Winning Eleven 2018 (Android)
        {14121473},                 // 0000000000D77A01

        // Kirara Fantasia (Android/iOS)
        {51408295487268137},        // 00B6A3928706E529

        // A3! (iOS/Android)
        {914306251},                // 00000000367F34CB

        // Weekly Shonen Jump: Ore Collection! (iOS/Android)
        {11708691},                 // 0000000000B2A913

        // Monster Gear Versus (iOS/Android)
        {12818105682118423669u},    // B1E30F346415B475

        // Yumeiro Cast (iOS/Android)
        {14418},                    // 0000000000003852

        // Ikki Tousen: Straight Striker (iOS/Android)
        {1000},                     // 00000000000003E8

        // Zero kara Hajimeru Mahou no Sho (iOS/Android)
        {15197457305692143616u},    // D2E836E662F20000

        // Soul Reverse Zero (iOS/Android)
        {2873513618},               // 00000000AB465692

        // Jojo's Bizarre Adventure: Diamond Records (iOS/Android) [additional data]
        {9368070542905259486u},     // 820212864CAB35DE

        // HUNTER x HUNTER: World Hunt (iOS/Android)
        {71777214294589695},        // 00FF00FF00FF00FF

        // \Comepri/ Comedy Prince (iOS/Android)
        {201537197868},             // 0000002EEC8D972C

        // Puzzle of Empires (iOS/Android)
        {13687846},                 // 0000000000D0DC26

        // Aozora Under Girls! (iOS/Android)
        {4988006236073},            // 000004895C56FFA9

        // Castle & Dragon (iOS/Android)
        // Gunbit (Android)
        {20140528},                 // 00000000013351F0

        // Uta no Prince sama Shining Live (iOS/Android)
        {2122831366},               // 000000007E87D606

        // Sevens Story (iOS/Android)
        {629427372852},             // 000000928CCB8334

        // MinGol: Everybody's Golf (iOS/Android)
        {1430028151061218},         // 0005149A5FF67AE2

        // AKB48 Group Tsui ni Koushiki Otoge demashita. (iOS/Android)
        {831021912315111419},       // 0B886206BC1BA7FB

        // Sen no Kaizoku (iOS/Android)
        {81368371967},              // 00000012F1EED2FF

        // I Chu (iOS/Android)
        {13456},                    // 0000000000003490

        // Shinobi Nightmare (iOS/Android)
        {369481198260487572},       // 0520A93135808594

        // Bungo Stray Dogs: Mayoi Inu Kaikitan (iOS/Android)
        {1655728931134731873},      // 16FA54B0C09F7661

        // Super Sentai Legend Wars (iOS/Android)
        {4017992759667450},         // 000E4657D7266AFA

        // Metal Saga: The Ark of Wastes (iOS/Android)
        {100097101118097115},       // 01639DC87B30C6DB

        // Taga Tame no Alchemist (iOS/Android)
        {5047159794308},            // 00000497222AAA84

        // Shin Tennis no Ouji-sama: Rising Beat (iOS/Android) voices?
        // UNI'S ON AIR (iOS/Android)
        {4902201417679},            // 0000047561F95FCF

        // Kai-ri-Sei Million Arthur (Vita)
        {1782351729464341796},      // 18BC2F7463867524

        // Dx2 Shin Megami Tensei Liberation (iOS/Android)
        {118714477},                // 000000000713706D

        // Oira (Cygames) [iOS/Android]
        {46460622},                 // 0000000002C4EECE

        // Dragon Ball Legends (Bandai Namco) [iOS/Android]
        {7335633962698440504},      // 65CD683924EE7F38

        // Princess Connect Re:Dive (iOS/Android/PC)
        {3201512},                  // 000000000030D9E8

        // PriPara: All Idol Perfect Stage (Takara Tomy) [Switch]
        {217735759},                // 000000000CFA624F

        // Space Invaders Extreme (Taito Corporation, Backbone Entertainment) [PC]
        {91380310056},              // 0000001546B0E028

        // CR Another God Hades Advent (Universal Entertainment Corporation) [iOS/Android]
        {64813795},                 // 0000000003DCFAE3

        // Onsen Musume: Yunohana Kore Kushon (Android) voices
        {6667},                     // 0000000000001A0B

        /* Libra of Precatus (Android) */
        {7894523655423589588},      // 6D8EFB700870FCD4

        /* Mashiro Witch (Android) */
        {6183755869466481156},      // 55D11D3349495204

        /* Iris Mysteria! (Android) */
        {62049655719861786},        // 00DC71D5479E1E1A

        /* Kotodaman (Android) */
        {19850716},                 // 00000000012EE5DC

        /* Puchiguru Love Live! (Android) */
        {355541041372},             // 00000052C7E5C0DC

        /* Dolls Order (Android) */
        {153438415134838},          // 00008B8D2A3AA076

        /* Fantasy Life Online (Android) */
        {123456789},                // 00000000075BCD15

        /* Wonder Gravity (Android) */
        {30623969886430861},        // 006CCC569EB1668D

        /* Ryu ga Gotoku Online (Android) */
        {59361939},                 // 000000000389CA93

        /* Sengoku BASARA Battle Party (Android) */
        {836575858265},             // 000000C2C7CE8E59

        /* DAME x PRINCE (Android) */
        {217019410378917901},       // 030302010100080D

        /* Uta Macross SmaPho De Culture (Android) */
        {396798934275978741},       // 0581B68744C5F5F5

        /* Touhou Cannonball (Android) */
        {5465717035832233},         // 00136B0A6A5D13A9

        /* Love Live! School idol festival ALL STARS (Android) */
        {6498535309877346413},      // 5A2F6F6F0192806D

        /* BLACKSTAR -Theater Starless- (Android) */
        {121837007188},             // 0000001C5E0D3154

        /* Nogizaka46 Rhythm Festival (Android) */
        {5613126134333697},         // 0013F11BC5510101

        /* IDOLiSH7 (Android) */
        {8548758374946935437},      // 76A34A72E15B928D

        /* Phantom of the Kill (Android) */
        {33624594140214547},        // 00777563E571B513

        /* Dankira!!! Boys, be DANCING! (Android) */
        {3957325206121219506},      // 36EB3E4EE38E05B2

        /* Idola: Phantasy Star Saga (Android) */
        {12136065386219383975u},    // A86BF72B4C852CA7

        /* Arca Last (Android) */
        {612310807},                // 00000000247F1F17

        /* ArkResona (Android) */
        {564321654321},             // 0000008364311631

        /* Kemono Friends 3 (Android) */
        {3315495188},               // 00000000C59E7114

        /* Inazuma Eleven SD (Android) */
        {14138734607940803423u},    // C436E03737D55B5F

        /* Detective Conan Runner / Case Closed Runner (Android) */
        {1175268187653273344},      // 104f643098e3f700

        /* I Chu EtoileStage (Android) */
        {1433227444226663680},      // 13E3D8C45778A500

        /* 22/7 Ongaku no Jikan (Android) */
        {20190906},                 // 00000000013416BA

        /* Cardcaptor Sakura: Happiness Memories (Android) */
        {625144437747651},          // 00023890C8252FC3

        /* Digimon Story: Cyber Sleuth (PC) */
        {2897314143465725881},      // 283553DCE3FD5FB9

        /* Alice Re:Code (Android) */
        {9422596198430275382u},     // 82C3C951C561F736

        /* Tokyo 7th Sisters (Android) */
        {18279639311550860193u},    // FDAE531AAB414BA1

        /* High School Fleet: Kantai Battle de Pinch! (Mobile) */
        {43472919336422565},        // 009A7263CA658CA5

        /* Disney's Twisted Wonderland (Android) */
        {2895000877},               // 00000000AC8E352D 

        /* B-PROJECT Kaikan Everyday (Android) */
        {12316546176516217334u},    // AAED297DDEF1D9F6

        /* HELIOS Rising Heroes (Android) */
        {311981570940334162},       // 04546195F85DF052

        /* World Ends's Club (iOS) */
        {50979632184989243},        // 00B51DB4932A963B

        /* Kandagawa Jet Girls (PC) */
        {6235253715273671},         // 001626EE22C887C7

        /* Re:Zero - Lost in Memories (Android) */
        {1611432018519751642},      // 165CF4E2138F7BDA

        /* Toji no Miko: Kizamishi Issen no Tomoshibi (Android) */
        {62057514034227932},        // 00DC78FAEFA76ADC

        /* Readyyy! (Android) */
        {1234567890987654321},      // 112210F4B16C1CB1

        /* HoneyWorks Premium Live (Android) */
        {20200401000000},           // 0000125F45B9D640

        /* Assault Lily Last Bullet (Android) */
        {6349046567469313},         // 00168E6C99510101

        /* Sakura Kakumei (iOS/Android) */
        {382759},                   // 000000000005D727

        /* Uma Musume (Android) */
        {75923756697503},           // 0000450D608C479F

        /* Fantasia Re: Build (Android) */
        {8430978314079461325},      // 7500DA1B7FBA5FCD

        /* SaGa Frontier Remastered (multi) */
        {5935495141785029458},      // 525F1D6244274B52

        /* Mushoku Tensei: Game ni Nattemo Honki Dasu (Android) */
        {12281329554827291428u},    // AA700C292CFCAB24

        /* Dragalia Lost (iOS/Android) */
        {2967411924141},            // 000002B2E7889CAD

        /* maimai DX Splash (AC) */
        {9170825592834449000},      // 7F4551499DF55E68

        /* Dragon Quest Tact (Android) */
        {3234477171400153310},      // 2CE32BD9B36A98DE

        /* Alchemy Stars (Android) */
        {1564789515523},            // 0000016C54B92503

        /* D4DJ Groovy Mix (Android) [base files] */
        {393410674916959300},       // 0575ACECA945A444
        /* D4DJ Groovy Mix (Android) [music_* files, per-song later mixed with subkey] */
        {0x59f449354d063308},	//music_0000001
        {0xf19d4cb84172f7ab},	//music_0000004
        {0x52d065d9ccdb8696},	//music_0110001
        {0xba26e58923a5da5d},	//music_0110002
        {0x5b877af6e52af19b},	//music_0110003
        {0x55b7b25821375a02},	//music_0110004
        {0x87025d78a57af15b},	//music_0110005
        {0x8815d2dffd77a71e},	//music_0110006
        {0xb9cedc6d6738d481},	//music_0110008
        {0x8efa09c6df3991a4},	//music_0110009
        {0xc05f8e4ea8c3e487},	//music_0110010
        {0xbf7712e175c0b265},	//music_0110011
        {0xf74cf8d4a5d008ce},	//music_0110012
        {0xfcce3164db70522d},	//music_0110013
        {0x47f52330df2ead11},	//music_0110014
        {0x2f778c736a8a4597},	//music_0110015
        {0xa90c8ebf8463d05},	//music_0110016
        {0x750beaf22ddc700b},	//music_0110018
        {0x16ccc93f976a8329},	//music_0110019
        {0x9f7a0810034669fe},	//music_0110020
        {0xe8333d53d2779e38},	//music_0110021
        {0x2cdcac4f44f67075},	//music_0110022
        {0xfb647d074e53fab6},	//music_0120001
        {0xc24049b9f7ed3105},	//music_0120002
        {0xdc128f2fd48bf4b},	//music_0120003
        {0xaef2954dc3657336},	//music_0120004
        {0x567d295828f1b08a},	//music_0120005
        {0x1bbad843d5971358},	//music_0120006
        {0xf6b0dc07ea8ebeb7},	//music_0120007
        {0xdb03ecca6a0151e2},	//music_0120008
        {0x260a354b925afeaf},	//music_0120009
        {0x8032f83cbf0076a1},	//music_0120010
        {0xe4a1737fa3d34ccb},	//music_0120011
        {0xd0ed8e940d8ed705},	//music_0120012
        {0x6ba36cadf1e045cf},	//music_0120013
        {0xb96786621e27daf3},	//music_0120014
        {0xa2c543b227b8e5e2},	//music_0120015
        {0x845437ec4e367a13},	//music_0120016
        {0xadfecfaf25cfe2ce},	//music_0120017
        {0x3674aba8da7bc84b},	//music_0120018
        {0xfd61f2c3b89f3888},	//music_0120019
        {0x4fffee4065d22bec},	//music_0210001
        {0x7678588b0adf59df},	//music_0210002
        {0xa0316b536c8b7540},	//music_0210003
        {0x76254d1ef50c004c},	//music_0210004
        {0x22ef086d7d6ce520},	//music_0210005
        {0x2f2c0ff3ff235bd6},	//music_0210006
        {0x6867cc75639ee0c3},	//music_0210007
        {0x73621a0d321e60c2},	//music_0210008
        {0xff04547fe629c8bf},	//music_0210009
        {0x5ef795cdbcdcba91},	//music_0210010
        {0x868acc0102c59a38},	//music_0210011
        {0x6dc5ff77263450a5},	//music_0210012
        {0x15bb78c31db0a0b6},	//music_0220001
        {0x59b1257242c40109},	//music_0220002
        {0xdb402bd08d522f34},	//music_0220003
        {0xa76262c2557be76f},	//music_0220004
        {0xa139c29e97fcefb4},	//music_0220005
        {0xb18fb83ee356fb94},	//music_0220006
        {0xd2cb7692d690b3a7},	//music_0220007
        {0x2b4a83e7d54d0554},	//music_0220008
        {0xa691936caf4d91d0},	//music_0220009
        {0xd40ccba5e10385a},	//music_0220010
        {0xf0c624dc0385adae},	//music_0220011
        {0xce0796d2a956dc5a},	//music_0220012
        {0xf9d6fb07c0b4e967},	//music_0220013
        {0x4aa31e0c4f787a8},	//music_0220014
        {0x94466db0d3c10f4b},	//music_0220015
        {0xe6d1fd6effa46736},	//music_0220017
        {0xd23bdacd616fc4c9},	//music_0220018
        {0xfceaa73248868ec5},	//music_0220019
        {0x6a15a9610d10d210},	//music_0310001
        {0x57111c24801b44a1},	//music_0310002
        {0x40443974a0a86b8b},	//music_0310003
        {0xcede847721873fc2},	//music_0310004
        {0xc40c398f7e80d184},	//music_0310005
        {0x68d576c631e61265},	//music_0310006
        {0x217782495c8b2972},	//music_0310007
        {0x6699616be2c50115},	//music_0310008
        {0xb7a5297198a73155},	//music_0310009
        {0xdd9ca800a7123d6f},	//music_0310010
        {0xc86f8564e0b9078c},	//music_0310011
        {0xcc5610c09f472ce9},	//music_0310012
        {0xd447a497c5547a1c},	//music_0310013
        {0x227b85948bb3d899},	//music_0310014
        {0xb921c3992807dadd},	//music_0320001
        {0x38ad99a045dc971f},	//music_0320002
        {0xf616642579ba5850},	//music_0320003
        {0x6aa0ff881da270e7},	//music_0320004
        {0x5089e16d7a676ab1},	//music_0320005
        {0x8ad213dddedc9022},	//music_0320006
        {0x5205a666f976d42f},	//music_0320007
        {0xcccb5077d978def4},	//music_0320008
        {0x290fbc93e184af1e},	//music_0320009
        {0x230c9509bbc3df0d},	//music_0320010
        {0x5771a2c76f36c898},	//music_0320011
        {0x244a92885ab77b7c},	//music_0320012
        {0xfc3fa77fc33460d4},	//music_0320013
        {0x26ee13598091b548},	//music_0320014
        {0xf06a6bfdd00c8286},	//music_0320015
        {0x2df608ef06aca41c},	//music_0320016
        {0x776c4aded0bca5d1},	//music_0410001
        {0xb7bff4fbf66be43f},	//music_0410002
        {0x904f50c5ce8ec6e4},	//music_0410003
        {0x465e30321a4091f2},	//music_0410004
        {0x7c7dd6d9f3761102},	//music_0410005
        {0xc222e70e4a79f7c3},	//music_0410006
        {0x8463554672bfb716},	//music_0410007
        {0x1111d6c10e509824},	//music_0410008
        {0x2e107d849959c430},	//music_0410009
        {0x75859a7a2b1ed37d},	//music_0410010
        {0x2e5f57a6c6e9c97f},	//music_0410011
        {0xa144f6d7de02e000},	//music_0410012
        {0x5d1f3fdbbb036f8d},	//music_0420001
        {0xc04264e8f34ad5c0},	//music_0420002
        {0x8f0e96b4f71f724f},	//music_0420003
        {0x79c5f00d243e3097},	//music_0420004
        {0x889d47adc9595ffa},	//music_0420005
        {0x3f25fe3395b3154c},	//music_0420006
        {0x212bbee264be5b06},	//music_0420007
        {0x867d47a7d8376402},	//music_0420008
        {0xf7e11ec9c94402f1},	//music_0420009
        {0x1bb363adcf4eb3f8},	//music_0420010
        {0xd80d3dcc7c75cea},	//music_0420011
        {0x52723f026d5238e8},	//music_0420012
        {0xd13a315c0005f0},	//music_0420013
        {0x35f2d3cec84aba1},	//music_0420014
        {0xdad11fe0e397ede},	//music_0420015
        {0xc94236c936f50cc},	//music_0420016
        {0xdf31e26a7b036a2},	//music_0510001
        {0xb2770dced3cfd9a7},	//music_0510002
        {0x6c6c1fd51e28a1e7},	//music_0510003
        {0xdcd2a403fb01e164},	//music_0510004
        {0x984363837811b08a},	//music_0510005
        {0x9f6881f6d7a91658},	//music_0510006
        {0x3804d53c43293080},	//music_0510007
        {0x298a0fa05c3f355f},	//music_0510008
        {0x9ebb560685327081},	//music_0510009
        {0xd45e8ba374b45ff7},	//music_0510010
        {0xc5c9bf138c9e28ce},	//music_0510011
        {0x1980271cfe0da9bd},	//music_0510012
        {0x75c5bd4e3a01a8a4},	//music_0510013
        {0xec5f5fbe92bbb771},	//music_0510014
        {0xb8c3233338ad8e0},	//music_0510015
        {0x15f82c1617013c36},	//music_0520001
        {0xc7da8e6f0e2fe399},	//music_0520002
        {0xe350bffcdc9cb686},	//music_0520003
        {0xe77aa2f3c90a4e84},	//music_0520004
        {0x57bdc58e4c06fc76},	//music_0520005
        {0xd4c36ab962153420},	//music_0520006
        {0x9de6ace9a0e62f44},	//music_0520007
        {0x35128087963cd5be},	//music_0520008
        {0xdf30ed86c3d00ffb},	//music_0520009
        {0xde4959221bc2675},	//music_0520010
        {0xeeaf8d2458ccdb36},	//music_0520011
        {0xb140168a47d55b92},	//music_0520012
        {0x2e8d1134ce415f8c},	//music_0520013
        {0x1bf43def1e4b103a},	//music_0520014
        {0x6721ad5109e4840d},	//music_0520015
        {0xd2ce91dbfc209b10},	//music_0610001
        {0xa662be1601e49476},	//music_0610002
        {0xe5e83d31e64273f8},	//music_0610003
        {0xaf9d7a05b0fc3d9e},	//music_0610004
        {0xcee66d585d689851},	//music_0610005
        {0x65c2f8500bc12c8},	//music_0610006
        {0x7148dda3afa76439},	//music_0610007
        {0x42548fe4544c2ed7},	//music_0610008
        {0x9e68da734cc472f},	//music_0610009
        {0xa01c597d1aa13358},	//music_0610010
        {0x6492e7708204838},	//music_0610011
        {0x957e4d3948427952},	//music_0610012
        {0x7081f083ac3d6f0a},	//music_0610013
        {0x8258ddd6a1d0849b},	//music_0620001
        {0x1dd21a1244ca12f1},	//music_0620002
        {0xfdec74b23d8b494b},	//music_0620003
        {0x6f9735c02faf6aae},	//music_0620004
        {0xe978d394512cfd},	//music_0620005
        {0xaba147637d52efbe},	//music_0620006
        {0xe67f4da6012c5d24},	//music_0620007
        {0xc352bbf3d519256e},	//music_0620008
        {0x37d1452c192b1e6},	//music_0620009
        {0xf7e53533d82d48dd},	//music_0620010
        {0x33848be13a2884a3},	//music_0620011
        {0xfab3596f11cc4d7a},	//music_0620012
        {0xe35d52b6d2c094fb},	//music_0620013
        {0xcdb9bc2ad7024ca2},	//music_0620014
        {0x5de2b0a34eee1c89},	//music_0620016
        {0x2a47feac8dc3ca9c},	//music_3010001
        {0x9ebbaf63ffe9d9ef},	//music_3010002
        {0xe553dba6592293d8},	//music_3010003
        {0x31e072678ad18a3},	//music_3010004
        {0x4ba9a9471f49b74e},	//music_3010005
        {0xc917cb864231982},	//music_3010006
        {0x7a708e291692abb9},	//music_3010007
        {0x1ab266a4cbb5133a},	//music_3010008
        {0x7d4719615fbb2f4d},	//music_3010009
        {0x28aa75a01f26a853},	//music_3010010
        {0x7555feeaa2a8fac4},	//music_3010011
        {0xa42de67a89fb3175},	//music_3010012
        {0xbdd0c58062c675d4},	//music_3010014
        {0xfd3ea450350d666f},	//music_3020001
        {0x5e91a3790c32e2b3},	//music_3020002
        {0x358adfd1bbd3a95e},	//music_3020003
        {0x1948edf7ff41e79b},	//music_3020004
        {0x100293729f35b4de},	//music_3020005
        {0x140ac59d2b870a13},	//music_3020006
        {0x402b13df5481d4e6},	//music_3020007
        {0x729efd67aede1a40},	//music_3020008
        {0xb7b9a143742fa51e},	//music_3020009
        {0xdfad847a86a126bb},	//music_5030001
        {0x711ef85045b8c26e},	//music_5030002
        {0xff7640b46d72b337},	//music_5030003
        {0x420d4dd413053980},	//music_5030004
        {0x84dc42f5a05f77cf},	//music_5030005
        {0xcb60232f2f27ace5},	//music_5030006
        {0xd9a00c9bc93014a8},	//music_5030007
        {0xe0b8bb03c74bb3d0},	//music_5030008
        {0xcb3d9329d40490b2},	//music_5030009
        {0x7ce69eed81f01019},	//music_5030010
        {0xfd9fa5bcb347c01b},	//music_5030011
        {0x4a4462cb0375001e},	//music_5030012
        {0xa3711cc06f9b86c2},	//music_5030013
        {0xaebfdf85aae4424},	//music_5030014
        {0x1ed521f6dd691255},	//music_5030015
        {0xb2bd99fa559b9062},	//music_5030016
        {0xaff9df030e63e5ba},	//music_5030017
        {0xb30acd0a43754e5c},	//music_5030018
        {0xa6cefd4472568948},	//music_5030019
        {0x447d08ca3148599d},	//music_5030020
        {0xfe31517282d40690},	//music_5030021
        {0xa6a15cc9722257d},	//music_5030022
        {0x55912db4388961ac},	//music_5030023
        {0x8f5f05c835f7280e},	//music_5030024
        {0x6750f4d05183bc01},	//music_5030025
        {0xda65af760e02c6ee},	//music_5030026
        {0xf4093992cadd3708},	//music_5030027
        {0xf965a1086b3179c3},	//music_5030028
        {0x24c0b49097e9ebff},	//music_5030029
        {0x2ecdf66c680f3a45},	//music_5030030
        {0x54aaada4a1b8deef},	//music_5030031
        {0x46bed365593c560c},	//music_5030032
        {0xa954b315630e3ed0},	//music_5030033
        {0x8328668369631cc1},	//music_5030034
        {0xa5c1adeb7919845f},	//music_5030035
        {0x8e35d68632fc0d77},	//music_5030036
        {0x4fbc9cabd12f75a1},	//music_5030037
        {0xd27146e6de40209a},	//music_5030038
        {0x6abcc90be62f2cec},	//music_5030039
        {0x7f617e396e9a1e5c},	//music_5030040
        {0xd0471c163265ca1b},	//music_5030041
        {0xd689966609595d7d},	//music_5030042
        {0x172171a4ff10fdc1},	//music_5030043
        {0x53c2bddb0a15d322},	//music_5030044
        {0xcb2c44d594252491},	//music_5030045
        {0xbdc220ba31087591},	//music_5030046
        {0xe2346e5f5d18228e},	//music_5030047
        {0x458b73844ed5219e},	//music_5030048
        {0x7d83b8da9023ef26},	//music_5030049
        {0x32cb728ddab4d956},	//music_5030050
        {0x52c5dfb61fe4c87a},	//music_5030054
        {0x3ebbccab07c9a9ba},	//music_5030055
        {0x7a5e0865ba8cafa7},	//music_5030061
        {0x7679587f7292b057},	//music_5030062
        {0xc9c804e6fed3387c},	//music_5030063
        {0x444dda6d55d76095},	//music_5040001
        {0xcbf4f1324081e0a6},	//music_5040002
        {0xf1db3c1d9542063a},	//music_5040003
        {0x114245b98dcb75bf},	//music_5040004
        {0x6139edfb8889032d},	//music_5040005
        {0x9ce13dcb2fb389cc},	//music_5040006
        {0x67b89634319c1d36},	//music_5040007
        {0xf877dea1180b9b90},	//music_5040008
        {0xcd3fb92065d9f373},	//music_5040009
        {0xee8da2806a13eecf},	//music_5040010
        {0x46fd87a21859ac},	//music_5040011
        {0x90fefcd350bd2cb8},	//music_5040012
        {0xf7edc5d72fdd6ceb},	//music_5040013
        {0x4c7d7c251c6bfa95},	//music_5040014
        {0x2f3528a4b9eaa0f7},	//music_5040015
        {0x529969b7e1e9ac18},	//music_5040016
        {0xbb7be9c7c620f504},	//music_5040018
        {0x7ed1fa0b6ec8f9b3},	//music_5040020
        {0xa4481f97a8d4d01c},	//music_5040021
        {0x7465c7c473e53a40},	//music_5040022
        {0xfadb1b0f28e951e1},	//music_5040023
        {0x5e3eba376e0b3dd},	//music_5050001
        {0xa8ee7a3a20ce822},	//music_5050002
        {0xf42d31b5ecd1aec1},	//music_5050003
        {0x56ecfc7ef4c65be8},	//music_5050004
        {0xad071dce0c070e65},	//music_5050005
        {0x98178a7b6ac7327b},	//music_5050006
        {0xb65d86624a857788},	//music_5050007
        {0x9fbd8a172d5ba3e3},	//music_5050008
        {0xdc2680acfd1b9b64},	//music_5050009
        {0xd0d8557c8ef44dd4},	//music_5050010
        {0x945cdb3cf1f29e52},	//music_5050011
        {0x6461fe08c7744918},	//music_5050012
        {0xe27f90cf77f17dec},	//music_5050013
        {0xbf5902d516db6ed5},	//music_5050015
        {0xeb8aac34dc178f65},	//music_5050016
        {0xd4d2a706a06377ef},	//music_5050017
        {0xdce3cd3ffe2d4144},	//music_5050018
        {0xdaaa3a987e3aa3ca},	//music_5050019
        {0x6cd32143f1a4a2aa},	//music_5050020
        {0x5b92c17283e2b9a0},	//music_5050021
        {0x141e0174df535976},	//music_5050022
        {0x73667711348f833f},	//music_5050023
        {0xc9f159f60b065f91},	//music_5050024
        {0x2638971e9d063b5f},	//music_5050025
        {0x6bdf5832eb19fcdf},	//music_5050026
        {0x7f0feac6be7def5b},	//music_5050027
        {0x8cc0aa89c75bb821},	//music_5050028
        {0x917e7dd2c5287edd},	//music_5050029
        {0x71b5fa3761d6726d},	//music_5050030
        {0xe4e11a71fe620e3a},	//music_5050031
        {0xc28331aab2612584},	//music_5050032
        {0xff05b24da2980c99},	//music_5050033
        {0xa7ce246e536b0941},	//music_5050034
        {0xfa842bc07360137d},	//music_5050035
        {0xf8d72c405d3f0456},	//music_5050036
        {0xd4d5fa6c87342e6b},	//music_5050037
        {0xd8cbc946fa660944},	//music_5050038
        {0xfac398719cd9e4a},	//music_5050039
        {0x9c4ba796548a019},	//music_5050040
        {0x7e7c462ba7d473cf},	//music_5050041
        {0x8a9a7af1379840fb},	//music_5050042
        {0xa0aa0097e5631019},	//music_5050043
        {0xe278eccf08eb2565},	//music_5050044
        {0x1cf133b26d8160d1},	//music_5050045
        {0xda08e9d3961c93f2},	//music_5050046
        {0x58d97e6f3d1aee86},	//music_5050047
        {0x57353b771188635e},	//music_5050048
        {0xeb9ad1180d7e1b53},	//music_5050049
        {0xaec8dbd5f5337a9e},	//music_5050050
        {0x49d08922136334ce},	//music_5050051
        {0x138df0b866e902e0},	//music_5050052
        {0xc076e8604740ff5f},	//music_5050053
        {0x69fe38ae5970d450},	//music_5050054
        {0x414200bd8ac11b40},	//music_5050055
        {0xbce9e85d31089fb2},	//music_5050056
        {0x817b919679c96d7},	//music_5050057
        {0x3e0e51043bd7d5e5},	//music_5050058
        {0x86d17e28b2f2b91c},	//music_5050059
        {0x115f906b6b7fb845},	//music_5050060
        {0xa8d5e9b1c6cf1505},	//music_5050061
        {0x69ffd3fefdf7ee71},	//music_5050062
        {0x571e646778541f4d},	//music_5050063
        {0xe8b5323ec07608e7},	//music_5050064
        {0x27992dd621b8a07e},	//music_5050065
        {0x8e2a8439f5628513},	//music_5050066
        {0x8b5be21e70a84eed},	//music_5050067
        {0x227297416c6ccc7c},	//music_5050068
        {0xb544dc8524419109},	//music_5050069
        {0x6c2d9160672cbf95},	//music_5050070
        {0x7ff6630286d2d93b},	//music_5050071
        {0xc6deecd2d1391713},	//music_5050072
        {0x78bec41dd27d8788},	//music_5050074
        {0xf86991a3b9aec2b},	//music_5050075
        {0x8f750fabaa794130},	//music_5050076
        {0x3c68e8102dbec720},	//music_5050077
        {0xf653b47bc8d4d1cd},	//music_5050079
        {0xb50f482149140fda},	//music_5050080
        {0xd61cc4e14e7073f4},	//music_5050081
        {0xfba77b717e43a39a},	//music_5050082
        {0x85a236b5270bac29},	//music_5050083
        {0x818d37d319d4c177},	//music_5050084
        {0xc16fb31c74eb5e59},	//music_5050085
        {0x598e133e0673b1e6},	//music_5050086
        {0x4cb2e8101df88d6f},	//music_5050087
        {0x3f8abfcd47711be2},	//music_5050088
        {0xcdb3f9edbd51012f},	//music_5050089
        {0xa28c9867b32a60e1},	//music_5050090
        {0xb1e06cf5f6a790c2},	//music_5050091
        {0xcfb9a7e64443e95c},	//music_5050093
        {0xf9ef74ac89bdfb7d},	//music_5050094
        {0x561e1e17dfb055ce},	//music_5050095
        {0x46967c2bc5d4d050},	//music_5050096
        {0xdcdaeb3067868ad9},	//music_5050097
        {0x18fab58c80c85580},	//music_5050098
        {0xba4484d824fb61af},	//music_5050099
        {0xb70fe5c5e12c7a1c},	//music_5050100
        {0x7f5d26ba72161054},	//music_5050101
        {0x79c1f27fa0f8c937},	//music_5050103
        {0xe1e4f9125646aa8a},	//music_5050104
        {0xd5cf3ce581c59e40},	//music_5050105
        {0x5ecb21ac94aa4b8f},	//music_5050107
        {0x3786b3940e98628a},	//music_5050108
        {0x52c250eade92393b},	//music_9010001
        {0xf66e6bb5b0599b07},	//music_9010002
        {0xfea0d6adff136868},	//music_9050001

        // Mini 4WD Hyper Dash Grand Prix (Android)
        {7957824642808300098},  // 6E6FDF59AB704242

        // m HOLD'EM (Android)
        {369211553984367},      // 00014FCBC385AF6F

        // Sonic Colors Ultimate (multi)
        {1991062320101111},     // 000712DC5250B6F7

        // ALTDEUS: Beyond Chronos (PC) [base-string 14238637353525924 + mods)]
        {14238637353525934},    // 003295F7198AE2AE - bgm
        {14238637353525954},    // 003295F7198AE2C2 - se
        {14238637353525944},    // 003295F7198AE2B8 - voice

        // SHOW BY ROCK!! Fes A Live (Android)
        {54605542411982574},    // 00C1FF73963BD6EE

        // Touhou Danmaku Kagura (Android)
        {5654863921795627},     // 001417119B4FD22B

        // Nogizaka 46 Fractal (Android)
        {984635491346198130},   // 0DAA20C336EEAE72

        // NEO: The World Ends With You (PC)
        {53346933792338754},    // 00BD86C0EE8C7342

        // THE iDOLM@STER Starlit Season (PS4/PC)
        {0x1e03b570b6145d1d},   // BGM
        {0x1da915aaa181a461},   // SE
        {0x1c82b6ab7487a5ec},   // Voice
        {0x6d275d3666c2f9c8},   // Sng001
        {0x0f53815df3044e6d},   // Sng002
        {0x158778e2e2fab347},   // Sng003
        {0x16b75e8b5247d46b},   // Sng004
        {0x157df8a6047048fc},   // Sng005
        {0x184d358b50b658d0},   // Sng006
        {0x157fb75af4ddd983},   // Sng007
        {0x404ba38c3e470827},   // Sng008
        {0x01d0b788a3b60d48},   // Sng009
        {0x021718d55d0960c9},   // Sng010
        {0x0021c5993d2b901c},   // Sng011
        {0x08237bcb9b711087},   // Sng012
        {0x01af60402e1228a5},   // Sng013
        {0x4eec18ab73a1a634},   // Sng014
        {0x1855099898b11ad9},   // Sng015
        {0x57ef8f2ea5d54db5},   // Sng016
        {0x17cc6975d67e2a1f},   // Sng017
        {0x0a5d0fc8cc5c4502},   // Sng018
        {0x198ea1a17416050b},   // Sng019
        {0x2aa3b8abad207a1e},   // Sng020
        {0x1175edbbacc1fc18},   // Sng024
        {0x0e14d06d7f7a6c8c},   // Sng025
        {0x33d98a3a9f9bfdef},   // Sng026
        {0x2284fd5ca82c78f4},   // Sng027
        {0x178a76b6436d20f0},   // Sng028
        {0x3ff99f2fed65a1ed},   // Sng030

        // Ulala: Idle Adventure (Android)
        {20191022},             // 000000000134172E

        // Girls' Frontline: Project Neural Cloud (Android)
        {210222522032314},      // 0000BF323EBFE0BA

        // Super Robot Wars 30 (PC)
        {6734488621090458},     // 0017ECFB5201069A

        // CHUNITHM NEW (AC)
        {32931609366120192},	// 0074FF1FCE264700

        // Shaman King: Funbari Chronicle (Android)
        {1620612098671},        // 0000017954022A6F

};

#endif/*_HCA_KEYS_H_*/
