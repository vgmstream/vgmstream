#ifndef _HCA_KEYS_H_
#define _HCA_KEYS_H_
#include <stdint.h>
//#include "hca_keys_awb.h"

typedef struct {
    uint64_t key;               /* hca key or seed ('user') key */
#if 0
    const uint16_t* subkeys;    /* scramble subkey table for seed key */
    size_t subkeys_size;        /* size of the derivation subkey table */
#endif
} hcakey_info;


/**
 * List of known keys, extracted from the game files (most from data analysis, some from 2ch.net/forums).
 * CRI's tools expect an unsigned 64 bit numberic string, but keys are commonly found online in hex form.
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
    // - Prince of Stride (Vita)
    // - Ro-Kyu-Bu! Naisho no Shutter Chance (Vita)
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
    // Ikemen Villains (Android)
    // Ikemen Prince (Android)
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

    // Bad Apple Wars (Vita)
    {241352432},                // 000000000E62BEF0

    // Koi to Senkyo to Chocolate Portable (PSP)
    {243812156},                // 000000000E88473C

    // Custom Drive (PSP)
    {2012062010},               // 0000000077EDA13A

    // Root Letter (Vita)
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

    /* D4DJ Groovy Mix (Android) [music_* files, per-song also mixed with subkey] */
    {0x59f449354d063308},       // music_0000001
    {0xf19d4cb84172f7ab},       // music_0000004
    {0xc4276ffaee4aadec},       // music_0000005
    {0x683d739be9679e61},       // music_0000006
    {0xa3adc531c32bb128},       // music_0000007
    {0x0bb8f42a806d1fa6},       // music_0000008
    {0xfd6e5a721921d936},       // music_0000009
    {0x7749847207a5f0da},       // music_0000010
    {0x5fcc1b142892c55c},       // music_0000011
    {0x52d065d9ccdb8696},       // music_0110001
    {0xba26e58923a5da5d},       // music_0110002
    {0x5b877af6e52af19b},       // music_0110003
    {0x55b7b25821375a02},       // music_0110004
    {0x87025d78a57af15b},       // music_0110005
    {0x8815d2dffd77a71e},       // music_0110006
    {0xb9cedc6d6738d481},       // music_0110008
    {0x8efa09c6df3991a4},       // music_0110009
    {0xc05f8e4ea8c3e487},       // music_0110010
    {0xbf7712e175c0b265},       // music_0110011
    {0xf74cf8d4a5d008ce},       // music_0110012
    {0xfcce3164db70522d},       // music_0110013
    {0x47f52330df2ead11},       // music_0110014
    {0x2f778c736a8a4597},       // music_0110015
    {0x0a90c8ebf8463d05},       // music_0110016
    {0x750beaf22ddc700b},       // music_0110018
    {0x16ccc93f976a8329},       // music_0110019
    {0x9f7a0810034669fe},       // music_0110020
    {0xe8333d53d2779e38},       // music_0110021
    {0x2cdcac4f44f67075},       // music_0110022
    {0x670c738c8171210b},       // music_0110023
    {0xd7359fa9ec49e2c2},       // music_0110024
    {0xd91127e6c2f22539},       // music_0110025
    {0x120db58e6c835175},       // music_0110026
    {0x3613e2a7fdfc0784},       // music_0110027
    {0xc239d244b6d3b722},       // music_0110028
    {0x2575c136a260e723},       // music_0110029
    {0x22c6b455883b1c28},       // music_0110030
    {0xff2ae68fa067f80a},       // music_0110031
    {0x422ca19b0fa5c7c0},       // music_0110032
    {0x3b11592a696da747},       // music_0110033
    {0x4ad3b8c0afc555bf},       // music_0110034
    {0xc642361141842f89},       // music_0110035
    {0x9eb82f449eb4f3f6},       // music_0110036
    {0x417822c4c107541c},       // music_0110037
    {0x9e55fe333fe182dd},       // music_0110038
    {0x86c5e597cdcf2c4e},       // music_0110039
    {0x2dd99d6e648d8fd3},       // music_0110040
    {0xfb647d074e53fab6},       // music_0120001
    {0xc24049b9f7ed3105},       // music_0120002
    {0x0dc128f2fd48bf4b},       // music_0120003
    {0xaef2954dc3657336},       // music_0120004
    {0x567d295828f1b08a},       // music_0120005
    {0x1bbad843d5971358},       // music_0120006
    {0xf6b0dc07ea8ebeb7},       // music_0120007
    {0xdb03ecca6a0151e2},       // music_0120008
    {0x260a354b925afeaf},       // music_0120009
    {0x8032f83cbf0076a1},       // music_0120010
    {0xe4a1737fa3d34ccb},       // music_0120011
    {0xd0ed8e940d8ed705},       // music_0120012
    {0x6ba36cadf1e045cf},       // music_0120013
    {0xb96786621e27daf3},       // music_0120014
    {0xa2c543b227b8e5e2},       // music_0120015
    {0x845437ec4e367a13},       // music_0120016
    {0xadfecfaf25cfe2ce},       // music_0120017
    {0x3674aba8da7bc84b},       // music_0120018
    {0xfd61f2c3b89f3888},       // music_0120019
    {0x514fa9879fd07278},       // music_0120021
    {0x3841dd3467659916},       // music_0120022
    {0xe6b3826413d0dbfc},       // music_0120023
    {0x01cc0a87f19dd7ca},       // music_0120024
    {0x480905d4a216f5a6},       // music_0120025
    {0x253fe8e15f9682b6},       // music_0140001
    {0x4fffee4065d22bec},       // music_0210001
    {0x7678588b0adf59df},       // music_0210002
    {0xa0316b536c8b7540},       // music_0210003
    {0x76254d1ef50c004c},       // music_0210004
    {0x22ef086d7d6ce520},       // music_0210005
    {0x2f2c0ff3ff235bd6},       // music_0210006
    {0x6867cc75639ee0c3},       // music_0210007
    {0x73621a0d321e60c2},       // music_0210008
    {0xff04547fe629c8bf},       // music_0210009
    {0x5ef795cdbcdcba91},       // music_0210010
    {0x868acc0102c59a38},       // music_0210011
    {0x6dc5ff77263450a5},       // music_0210012
    {0x01dca436afdd18d9},       // music_0210013
    {0xaecee65d0f181d3b},       // music_0210014
    {0x2822bba0a5c4f18c},       // music_0210015
    {0xff579d3fcfa8453a},       // music_0210016
    {0x3caa61e8b958f6d8},       // music_0210017
    {0xe38b758bcadfc621},       // music_0210018
    {0x3399e970670db2ba},       // music_0210019
    {0x1cb76530af356c05},       // music_0210020
    {0x2ae40b627c7125d7},       // music_0210021
    {0xcc7e1e54049f4719},       // music_0210022
    {0x4c06e4b2e072abaa},       // music_0210023
    {0xc60bc5aeec280870},       // music_0210024
    {0x0c59613fc788cec7},       // music_0210025
    {0xf61a0cfac4072903},       // music_0210026
    {0xa316e41cc9303921},       // music_0210027
    {0x61933afefe5f14c3},       // music_0210028
    {0x15bb78c31db0a0b6},       // music_0220001
    {0x59b1257242c40109},       // music_0220002
    {0xdb402bd08d522f34},       // music_0220003
    {0xa76262c2557be76f},       // music_0220004
    {0xa139c29e97fcefb4},       // music_0220005
    {0xb18fb83ee356fb94},       // music_0220006
    {0xd2cb7692d690b3a7},       // music_0220007
    {0x2b4a83e7d54d0554},       // music_0220008
    {0xa691936caf4d91d0},       // music_0220009
    {0x0d40ccba5e10385a},       // music_0220010
    {0xf0c624dc0385adae},       // music_0220011
    {0xce0796d2a956dc5a},       // music_0220012
    {0xf9d6fb07c0b4e967},       // music_0220013
    {0x04aa31e0c4f787a8},       // music_0220014
    {0x94466db0d3c10f4b},       // music_0220015
    {0xe6d1fd6effa46736},       // music_0220017
    {0xd23bdacd616fc4c9},       // music_0220018
    {0xfceaa73248868ec5},       // music_0220019
    {0x3b6b0023a2dd8c3d},       // music_0220020
    {0x318f65f564daa549},       // music_0220021
    {0x3dac12b26997f628},       // music_0220022
    {0xdd4dc0afe201fd53},       // music_0240001
    {0x6a15a9610d10d210},       // music_0310001
    {0x57111c24801b44a1},       // music_0310002
    {0x40443974a0a86b8b},       // music_0310003
    {0xcede847721873fc2},       // music_0310004
    {0xc40c398f7e80d184},       // music_0310005
    {0x68d576c631e61265},       // music_0310006
    {0x217782495c8b2972},       // music_0310007
    {0x6699616be2c50115},       // music_0310008
    {0xb7a5297198a73155},       // music_0310009
    {0xdd9ca800a7123d6f},       // music_0310010
    {0xc86f8564e0b9078c},       // music_0310011
    {0xcc5610c09f472ce9},       // music_0310012
    {0xd447a497c5547a1c},       // music_0310013
    {0x227b85948bb3d899},       // music_0310014
    {0xfff671f0ddb660b1},       // music_0310015
    {0x22e33db9b5625a96},       // music_0310016
    {0xb78070802414de7a},       // music_0310017
    {0x572aeeed86655119},       // music_0310018
    {0x86ca86794400f49d},       // music_0310019
    {0x7a2bbc195b9bfac1},       // music_0310020
    {0x1c1f029bb47594b1},       // music_0310021
    {0xa3aad605838eefd7},       // music_0310022
    {0xf950f96b2066b16e},       // music_0310023
    {0x9e3d6943ba67b424},       // music_0310024
    {0xb58259c9d1f9ebc1},       // music_0310025
    {0xbd9e17f5262e3f09},       // music_0310026
    {0xba8c9e65cf055de},        // music_0310027
    {0xf8cad906044f1b35},       // music_0310028
    {0xb921c3992807dadd},       // music_0320001
    {0x38ad99a045dc971f},       // music_0320002
    {0xf616642579ba5850},       // music_0320003
    {0x6aa0ff881da270e7},       // music_0320004
    {0x5089e16d7a676ab1},       // music_0320005
    {0x8ad213dddedc9022},       // music_0320006
    {0x5205a666f976d42f},       // music_0320007
    {0xcccb5077d978def4},       // music_0320008
    {0x290fbc93e184af1e},       // music_0320009
    {0x230c9509bbc3df0d},       // music_0320010
    {0x5771a2c76f36c898},       // music_0320011
    {0x244a92885ab77b7c},       // music_0320012
    {0xfc3fa77fc33460d4},       // music_0320013
    {0x26ee13598091b548},       // music_0320014
    {0xf06a6bfdd00c8286},       // music_0320015
    {0x2df608ef06aca41c},       // music_0320016
    {0x641af19c287d4a2e},       // music_0320017
    {0xa9e5ea218873f8db},       // music_0320018
    {0x82de7b71b30d7bc2},       // music_0320019
    {0x100b7ca3075996fe},       // music_0320020
    {0x4d1f0819b42520fc},       // music_0320021
    {0xd8126b12e1ea087d},       // music_0340001
    {0x776c4aded0bca5d1},       // music_0410001
    {0xb7bff4fbf66be43f},       // music_0410002
    {0x904f50c5ce8ec6e4},       // music_0410003
    {0x465e30321a4091f2},       // music_0410004
    {0x7c7dd6d9f3761102},       // music_0410005
    {0xc222e70e4a79f7c3},       // music_0410006
    {0x8463554672bfb716},       // music_0410007
    {0x1111d6c10e509824},       // music_0410008
    {0x2e107d849959c430},       // music_0410009
    {0x75859a7a2b1ed37d},       // music_0410010
    {0x2e5f57a6c6e9c97f},       // music_0410011
    {0xa144f6d7de02e000},       // music_0410012
    {0x1da4370c9c20319c},       // music_0410013
    {0xd8cdd53589ad3634},       // music_0410014
    {0x88007190e0bfa1ce},       // music_0410015
    {0x6fccdd5c3d0d6e3e},       // music_0410016
    {0x9d8037d7bfb3fc1d},       // music_0410017
    {0x685b5601a43b6c60},       // music_0410018
    {0x75927596a180f3e3},       // music_0410019
    {0xc3d36676d54255c5},       // music_0410020
    {0x41283c6f06db3cdd},       // music_0410021
    {0x0198c53b5d345997},       // music_0410022
    {0xd5dcbaceb12dd205},       // music_0410023
    {0x4b71388640b83c6c},       // music_0410024
    {0x5b7c2a41095c7b76},       // music_0410025
    {0xea8a072379174ae7},       // music_0410026
    {0x5d1f3fdbbb036f8d},       // music_0420001
    {0xc04264e8f34ad5c0},       // music_0420002
    {0x8f0e96b4f71f724f},       // music_0420003
    {0x79c5f00d243e3097},       // music_0420004
    {0x889d47adc9595ffa},       // music_0420005
    {0x3f25fe3395b3154c},       // music_0420006
    {0x212bbee264be5b06},       // music_0420007
    {0x867d47a7d8376402},       // music_0420008
    {0xf7e11ec9c94402f1},       // music_0420009
    {0x1bb363adcf4eb3f8},       // music_0420010
    {0x0d80d3dcc7c75cea},       // music_0420011
    {0x52723f026d5238e8},       // music_0420012
    {0x00d13a315c0005f0},       // music_0420013
    {0x035f2d3cec84aba1},       // music_0420014
    {0x0dad11fe0e397ede},       // music_0420015
    {0x0c94236c936f50cc},       // music_0420016
    {0xbf8a2d951bc01dff},       // music_0420017
    {0xb945505638f972e4},       // music_0420018
    {0xe00590890f7f3219},       // music_0420019
    {0x8f7e24e32cbceb6a},       // music_0440001
    {0x0df31e26a7b036a2},       // music_0510001
    {0xb2770dced3cfd9a7},       // music_0510002
    {0x6c6c1fd51e28a1e7},       // music_0510003
    {0xdcd2a403fb01e164},       // music_0510004
    {0x984363837811b08a},       // music_0510005
    {0x9f6881f6d7a91658},       // music_0510006
    {0x3804d53c43293080},       // music_0510007
    {0x298a0fa05c3f355f},       // music_0510008
    {0x9ebb560685327081},       // music_0510009
    {0xd45e8ba374b45ff7},       // music_0510010
    {0xc5c9bf138c9e28ce},       // music_0510011
    {0x1980271cfe0da9bd},       // music_0510012
    {0x75c5bd4e3a01a8a4},       // music_0510013
    {0xec5f5fbe92bbb771},       // music_0510014
    {0x0b8c3233338ad8e0},       // music_0510015
    {0xda4ce04dbda1bd7e},       // music_0510016
    {0x07878df60f0549c4},       // music_0510017
    {0x8e5b7068022828e0},       // music_0510018
    {0xb069a5c5e2d93edf},       // music_0510019
    {0x30f9dcefa450733a},       // music_0510020
    {0xf85695960e2dcb7f},       // music_0510021
    {0x212604f36a4ddb0c},       // music_0510022
    {0xef7ee9169b194770},       // music_0510023
    {0x0f3bc872fcb7784b},       // music_0510024
    {0x8e779c6c28690617},       // music_0510025
    {0x468e4ffeae5afa0e},       // music_0510027
    {0x15f82c1617013c36},       // music_0520001
    {0xc7da8e6f0e2fe399},       // music_0520002
    {0xe350bffcdc9cb686},       // music_0520003
    {0xe77aa2f3c90a4e84},       // music_0520004
    {0x57bdc58e4c06fc76},       // music_0520005
    {0xd4c36ab962153420},       // music_0520006
    {0x9de6ace9a0e62f44},       // music_0520007
    {0x35128087963cd5be},       // music_0520008
    {0xdf30ed86c3d00ffb},       // music_0520009
    {0x0de4959221bc2675},       // music_0520010
    {0xeeaf8d2458ccdb36},       // music_0520011
    {0xb140168a47d55b92},       // music_0520012
    {0x2e8d1134ce415f8c},       // music_0520013
    {0x1bf43def1e4b103a},       // music_0520014
    {0x6721ad5109e4840d},       // music_0520015
    {0x0c488dd62fc89090},       // music_0520016
    {0xd3d24f1db0b74363},       // music_0520019
    {0xbc99855ebbfa8e97},       // music_0520020
    {0xb2b54877e3fa1bc6},       // music_0520021
    {0xc38d718006196625},       // music_0520022
    {0x207ae64e50eeba80},       // music_0540001
    {0xd2ce91dbfc209b10},       // music_0610001
    {0xa662be1601e49476},       // music_0610002
    {0xe5e83d31e64273f8},       // music_0610003
    {0xaf9d7a05b0fc3d9e},       // music_0610004
    {0xcee66d585d689851},       // music_0610005
    {0x065c2f8500bc12c8},       // music_0610006
    {0x7148dda3afa76439},       // music_0610007
    {0x42548fe4544c2ed7},       // music_0610008
    {0x09e68da734cc472f},       // music_0610009
    {0xa01c597d1aa13358},       // music_0610010
    {0x06492e7708204838},       // music_0610011
    {0x957e4d3948427952},       // music_0610012
    {0x7081f083ac3d6f0a},       // music_0610013
    {0xfcfa4dbd1ec6cfcb},       // music_0610014
    {0x750ab11487f9d6e8},       // music_0610015
    {0xd22171fda74c5615},       // music_0610016
    {0x3aa02e0a37543b5c},       // music_0610017
    {0xc451f1deddae08ca},       // music_0610018
    {0xc0fa6669d9904919},       // music_0610019
    {0x4ad7fd8dafaa58a9},       // music_0610020
    {0x7aece54359beac21},       // music_0610021
    {0x7c9332be25e5c95a},       // music_0610022
    {0x415eef25f84e8c2e},       // music_0610023
    {0xfd2237157af70152},       // music_0610024
    {0xf6c91cba66c3fe68},       // music_0610025
    {0xfdb798f20efbf4ac},       // music_0610026
    {0x9f37cb27968428fa},       // music_0610027
    {0x36024c4a109520ec},       // music_0610028
    {0x758ad666ba171bd7},       // music_0610029
    {0xe3c90dcaed524e49},       // music_0610030
    {0x8258ddd6a1d0849b},       // music_0620001
    {0x1dd21a1244ca12f1},       // music_0620002
    {0xfdec74b23d8b494b},       // music_0620003
    {0x6f9735c02faf6aae},       // music_0620004
    {0x00e978d394512cfd},       // music_0620005
    {0xaba147637d52efbe},       // music_0620006
    {0xe67f4da6012c5d24},       // music_0620007
    {0xc352bbf3d519256e},       // music_0620008
    {0x037d1452c192b1e6},       // music_0620009
    {0xf7e53533d82d48dd},       // music_0620010
    {0x33848be13a2884a3},       // music_0620011
    {0xfab3596f11cc4d7a},       // music_0620012
    {0xe35d52b6d2c094fb},       // music_0620013
    {0xcdb9bc2ad7024ca2},       // music_0620014
    {0xcf27380a5a949dc1},       // music_0620015
    {0x5de2b0a34eee1c89},       // music_0620016
    {0x5c1195d8afcb1901},       // music_0620017
    {0x1ad8db767d9ba4a7},       // music_0620018
    {0x9bc820aa161b0f08},       // music_0620019
    {0x7d1d8c5dd43cabfc},       // music_0620020
    {0x5a216f2660d870c0},       // music_0620021
    {0x6cccc0684c8b2664},       // music_0640001
    {0x85f26fa7befc2b5a},       // music_0810000
    {0xd1df27a57399613e},       // music_0810001
    {0xd37ec4cb304e16ae},       // music_0810002
    {0x4582fedda6a62aa8},       // music_0810003
    {0xb31ea911ea8aba81},       // music_0810004
    {0xd63e9ce2913178a4},       // music_0810005
    {0xef287bc5146b1743},       // music_0810006
    {0x1f3c1d0817b3d4be},       // music_0810008
    {0x2e5c9e00274e0f2a},       // music_0810009
    {0xfd59b4043bf88390},       // music_0810010
    {0xad90dc83d09aeaf1},       // music_0810011
    {0x960722a792155aae},       // music_0810012
    {0x1e99d14d97ab82c5},       // music_0820001
    {0x5bf7cefecda8bcb2},       // music_0820002
    {0x9cf7ab0ccafa374e},       // music_0820003
    {0xbaac3fa8d2d4b6cb},       // music_0820004
    {0x9fd373d1412b4d27},       // music_0840001
    {0x7a9e4b710716a8e2},       // music_0910000
    {0x972b52f3dfaa387a},       // music_0910001
    {0x407a4b6c3dcf2509},       // music_0910002
    {0x838be1aa4dc372bb},       // music_0910003
    {0xc03bac003c24471d},       // music_0910004
    {0xf864df0abe22ec49},       // music_0910005
    {0x66cdc54206ac6583},       // music_0910006
    {0xef56bb830245a94d},       // music_0910007
    {0x6cb33fe1e8506d0f},       // music_0910008
    {0xfaf9830ee551c6c4},       // music_0910009
    {0xf7c51ef55106e4a0},       // music_0910010
    {0xcd35d39900ebaedc},       // music_0910011
    {0x4683c57919dbdeee},       // music_0920001
    {0x126d0d20ad7f0401},       // music_0920002
    {0x1652eb8bf3cea8f5},       // music_0920003
    {0x7bd591da4640bd99},       // music_0920004
    {0x3ac823b053f954ab},       // music_0920005
    {0x353e9b0a47ed61bc},       // music_0940001
    {0x1b06e24d34d67ac8},       // music_1010001
    {0xab02c0d6f229df05},       // music_1010002
    {0x56a96f773f447a7e},       // music_1010003
    {0x2a47feac8dc3ca9c},       // music_3010001
    {0x9ebbaf63ffe9d9ef},       // music_3010002
    {0xe553dba6592293d8},       // music_3010003
    {0x031e072678ad18a3},       // music_3010004
    {0x4ba9a9471f49b74e},       // music_3010005
    {0x0c917cb864231982},       // music_3010006
    {0x7a708e291692abb9},       // music_3010007
    {0x1ab266a4cbb5133a},       // music_3010008
    {0x7d4719615fbb2f4d},       // music_3010009
    {0x28aa75a01f26a853},       // music_3010010
    {0x7555feeaa2a8fac4},       // music_3010011
    {0xa42de67a89fb3175},       // music_3010012
    {0xbdd0c58062c675d4},       // music_3010014
    {0xef257f41a265a0af},       // music_3010015
    {0x5e23d8a2488bc715},       // music_3010016
    {0x198cc607e20dd264},       // music_3010017
    {0x31a9ab25b5dff424},       // music_3010018
    {0x86e823895bb0bec6},       // music_3010019
    {0x17f53cfa841f41b5},       // music_3010020
    {0x4992a33be3ba81dd},       // music_3010021
    {0x86d0ab836f09a599},       // music_3010022
    {0x2a5eefc1987bf7fb},       // music_3010023
    {0x157f20b466d75228},       // music_3010024
    {0x13cada13f45f432b},       // music_3010025
    {0xe0f08378be17633b},       // music_3010026
    {0xaf81e107cd20ba90},       // music_3010027
    {0x4efe51d362dcb6b1},       // music_3010028
    {0xc054cfc97a5cc13d},       // music_3010029
    {0x521db4dcc2aa84fd},       // music_3010030
    {0x2898fbc6d7a571ff},       // music_3010031
    {0x1deb4339b0961fb1},       // music_3010032
    {0x2ceae501b33507b4},       // music_3010033
    {0x46e91ff3014e07f5},       // music_3010034
    {0xf1895742954806b8},       // music_3010035
    {0x189b84400394043d},       // music_3010036
    {0x0637e592d471df60},       // music_3010037
    {0xa633022c4198673a},       // music_3010038
    {0x8d410b922905a207},       // music_3010039
    {0x385562787c40d11c},       // music_3010040
    {0xfd3ea450350d666f},       // music_3020001
    {0x5e91a3790c32e2b3},       // music_3020002
    {0x358adfd1bbd3a95e},       // music_3020003
    {0x1948edf7ff41e79b},       // music_3020004
    {0x100293729f35b4de},       // music_3020005
    {0x140ac59d2b870a13},       // music_3020006
    {0x402b13df5481d4e6},       // music_3020007
    {0x729efd67aede1a40},       // music_3020008
    {0xb7b9a143742fa51e},       // music_3020009
    {0x0c7750328bcd329f},       // music_3020010
    {0xc3dba12eeb4cc308},       // music_3020011
    {0x5761fe7462b17a3b},       // music_3020012
    {0x98c7a1d1c45df640},       // music_3020013
    {0x3d01826fe053ddda},       // music_3020014
    {0xa6a6426caed68f7c},       // music_3020015
    {0x34cc16f635101f02},       // music_3020016
    {0xc92c94fdc988b8fa},       // music_3020017
    {0x4429ed54ca45a36d},       // music_3020018
    {0xcc935f3ebbb7bb94},       // music_3020019
    {0x4a1d57f0db140c12},       // music_3020020
    {0xbd9651254d9f6f95},       // music_3020021
    {0x8ddea25a12f93099},       // music_3020022
    {0xf754248dcd46287e},       // music_3020023
    {0xa3c06ffa7775a274},       // music_3020024
    {0x427be43aa233382c},       // music_3020025
    {0x4ec96dd7db2adfd7},       // music_3020026
    {0x4876142f5d34d944},       // music_3020027
    {0x9f12a5003f112aaa},       // music_3020028
    {0x0c8c46d327667edc},       // music_3020029
    {0x38edf73f19401e1c},       // music_3020030
    {0xfa2c72797fa3e388},       // music_3020031
    {0x3966399f31d9de81},       // music_3020032
    {0xe9df82eee5800cde},       // music_3020033
    {0x9faba30aa051f691},       // music_3020034
    {0xaa06f3d3301a97d2},       // music_3020037
    {0x5b6e11b92acc7903},       // music_3020038
    {0x044c84f5efae5311},       // music_3020039
    {0x49ade60a55212fc5},       // music_3020040
    {0xabd145c0bc3a69dc},       // music_3020041
    {0xd8883291d950e928},       // music_3040001
    {0xdfad847a86a126bb},       // music_5030001
    {0x711ef85045b8c26e},       // music_5030002
    {0xff7640b46d72b337},       // music_5030003
    {0x420d4dd413053980},       // music_5030004
    {0x84dc42f5a05f77cf},       // music_5030005
    {0xcb60232f2f27ace5},       // music_5030006
    {0xd9a00c9bc93014a8},       // music_5030007
    {0xe0b8bb03c74bb3d0},       // music_5030008
    {0xcb3d9329d40490b2},       // music_5030009
    {0x7ce69eed81f01019},       // music_5030010
    {0xfd9fa5bcb347c01b},       // music_5030011
    {0x4a4462cb0375001e},       // music_5030012
    {0xa3711cc06f9b86c2},       // music_5030013
    {0x0aebfdf85aae4424},       // music_5030014
    {0x1ed521f6dd691255},       // music_5030015
    {0xb2bd99fa559b9062},       // music_5030016
    {0xaff9df030e63e5ba},       // music_5030017
    {0xb30acd0a43754e5c},       // music_5030018
    {0xa6cefd4472568948},       // music_5030019
    {0x447d08ca3148599d},       // music_5030020
    {0xfe31517282d40690},       // music_5030021
    {0x0a6a15cc9722257d},       // music_5030022
    {0x55912db4388961ac},       // music_5030023
    {0x8f5f05c835f7280e},       // music_5030024
    {0x6750f4d05183bc01},       // music_5030025
    {0xda65af760e02c6ee},       // music_5030026
    {0xf4093992cadd3708},       // music_5030027
    {0xf965a1086b3179c3},       // music_5030028
    {0x24c0b49097e9ebff},       // music_5030029
    {0x2ecdf66c680f3a45},       // music_5030030
    {0x54aaada4a1b8deef},       // music_5030031
    {0x46bed365593c560c},       // music_5030032
    {0xa954b315630e3ed0},       // music_5030033
    {0x8328668369631cc1},       // music_5030034
    {0xa5c1adeb7919845f},       // music_5030035
    {0x8e35d68632fc0d77},       // music_5030036
    {0x4fbc9cabd12f75a1},       // music_5030037
    {0xd27146e6de40209a},       // music_5030038
    {0x6abcc90be62f2cec},       // music_5030039
    {0x7f617e396e9a1e5c},       // music_5030040
    {0xd0471c163265ca1b},       // music_5030041
    {0xd689966609595d7d},       // music_5030042
    {0x172171a4ff10fdc1},       // music_5030043
    {0x53c2bddb0a15d322},       // music_5030044
    {0xcb2c44d594252491},       // music_5030045
    {0xbdc220ba31087591},       // music_5030046
    {0xe2346e5f5d18228e},       // music_5030047
    {0x458b73844ed5219e},       // music_5030048
    {0x7d83b8da9023ef26},       // music_5030049
    {0x32cb728ddab4d956},       // music_5030050
    {0xbd128c4a4d1f565b},       // music_5030051
    {0x363aa084f3bf7af1},       // music_5030052
    {0xdfe954b617357381},       // music_5030053
    {0x52c5dfb61fe4c87a},       // music_5030054
    {0x3ebbccab07c9a9ba},       // music_5030055
    {0x50a0167818f4d51b},       // music_5030056
    {0xceaed3996196281b},       // music_5030057
    {0xe0590c04ec4906f1},       // music_5030058
    {0x400b11e930008a58},       // music_5030059
    {0x15aaecc4725a5f70},       // music_5030060
    {0x7a5e0865ba8cafa7},       // music_5030061
    {0x7679587f7292b057},       // music_5030062
    {0xc9c804e6fed3387c},       // music_5030063
    {0xc72eb23bcdc43f42},       // music_5030064
    {0x0f7cedd212a06307},       // music_5030065
    {0x850ad05f415d6018},       // music_5030066
    {0x36f62d41aa4203c9},       // music_5030067
    {0x2174d57bfeafc637},       // music_5030068
    {0x143a7405ef56e4df},       // music_5030069
    {0x4a193ed26d1d10f3},       // music_5030070
    {0x884385ad03f2bd62},       // music_5030071
    {0x9c220b56ab1d56b1},       // music_5030072
    {0x5ed84aea8ad0d05b},       // music_5030073
    {0xf5074d6db1cec319},       // music_5030074
    {0xae83f69d4d7ff064},       // music_5030075
    {0x29a3d7b03fe66d8f},       // music_5030076
    {0xb30288656e565401},       // music_5030077
    {0x5a420864cd1fa3fb},       // music_5030078
    {0xe1c992cc05d8a203},       // music_5030079
    {0x79ad5329b9b46034},       // music_5030080
    {0x1dd99ac6f1a07f00},       // music_5030081
    {0x2fd298ade03f7f0f},       // music_5030082
    {0xbeb0df818f88b99c},       // music_5030083
    {0x0886df2bdcc645cc},       // music_5030084
    {0x06420fc8d2d67ef1},       // music_5030085
    {0x29d61d19434b640b},       // music_5030086
    {0xf4d7569bb4fa06ab},       // music_5030087
    {0x1de64ce0c1054a5a},       // music_5030088
    {0xd51e8d88fff1aa5c},       // music_5030089
    {0xa2ea81812297bf73},       // music_5030090
    {0x0276f1e23c97cf8d},       // music_5030091
    {0xb59a0df1213c36c9},       // music_5030092
    {0xebf7bfebb82a4411},       // music_5030093
    {0xb991fcb8b74f46d7},       // music_5030094
    {0x435e2e064bd48ad5},       // music_5030095
    {0xa9824e9baaa354e2},       // music_5030096
    {0xc3a72539b831ea6e},       // music_5030097
    {0x8bc6b7b7a2d2bba3},       // music_5030098
    {0xdbb0f41e90b30452},       // music_5030099
    {0x2eb141954bb3bd25},       // music_5030101
    {0xaee2837e71b2bb97},       // music_5030102
    {0xa4b16fbd969bf071},       // music_5030103
    {0x9b1fdeafe8007f41},       // music_5030104
    {0xace06e1ff0e92427},       // music_5030105
    {0x5b6614302dee85c2},       // music_5030106
    {0xa54a0ae6b0c3b101},       // music_5030107
    {0x444dda6d55d76095},       // music_5040001
    {0xcbf4f1324081e0a6},       // music_5040002
    {0xf1db3c1d9542063a},       // music_5040003
    {0x114245b98dcb75bf},       // music_5040004
    {0x6139edfb8889032d},       // music_5040005
    {0x9ce13dcb2fb389cc},       // music_5040006
    {0x67b89634319c1d36},       // music_5040007
    {0xf877dea1180b9b90},       // music_5040008
    {0xcd3fb92065d9f373},       // music_5040009
    {0xee8da2806a13eecf},       // music_5040010
    {0x0046fd87a21859ac},       // music_5040011
    {0x90fefcd350bd2cb8},       // music_5040012
    {0xf7edc5d72fdd6ceb},       // music_5040013
    {0x4c7d7c251c6bfa95},       // music_5040014
    {0x2f3528a4b9eaa0f7},       // music_5040015
    {0x529969b7e1e9ac18},       // music_5040016
    {0xbb7be9c7c620f504},       // music_5040018
    {0x7ed1fa0b6ec8f9b3},       // music_5040020
    {0xa4481f97a8d4d01c},       // music_5040021
    {0x7465c7c473e53a40},       // music_5040022
    {0xfadb1b0f28e951e1},       // music_5040023
    {0x2f1a04b23ceef9d4},       // music_5040024
    {0x05e3eba376e0b3dd},       // music_5050001
    {0x0a8ee7a3a20ce822},       // music_5050002
    {0xf42d31b5ecd1aec1},       // music_5050003
    {0x56ecfc7ef4c65be8},       // music_5050004
    {0xad071dce0c070e65},       // music_5050005
    {0x98178a7b6ac7327b},       // music_5050006
    {0xb65d86624a857788},       // music_5050007
    {0x9fbd8a172d5ba3e3},       // music_5050008
    {0xdc2680acfd1b9b64},       // music_5050009
    {0xd0d8557c8ef44dd4},       // music_5050010
    {0x945cdb3cf1f29e52},       // music_5050011
    {0x6461fe08c7744918},       // music_5050012
    {0xe27f90cf77f17dec},       // music_5050013
    {0xbf5902d516db6ed5},       // music_5050015
    {0xeb8aac34dc178f65},       // music_5050016
    {0xd4d2a706a06377ef},       // music_5050017
    {0xdce3cd3ffe2d4144},       // music_5050018
    {0xdaaa3a987e3aa3ca},       // music_5050019
    {0x6cd32143f1a4a2aa},       // music_5050020
    {0x5b92c17283e2b9a0},       // music_5050021
    {0x141e0174df535976},       // music_5050022
    {0x73667711348f833f},       // music_5050023
    {0xc9f159f60b065f91},       // music_5050024
    {0x2638971e9d063b5f},       // music_5050025
    {0x6bdf5832eb19fcdf},       // music_5050026
    {0x7f0feac6be7def5b},       // music_5050027
    {0x8cc0aa89c75bb821},       // music_5050028
    {0x917e7dd2c5287edd},       // music_5050029
    {0x71b5fa3761d6726d},       // music_5050030
    {0xe4e11a71fe620e3a},       // music_5050031
    {0xc28331aab2612584},       // music_5050032
    {0xff05b24da2980c99},       // music_5050033
    {0xa7ce246e536b0941},       // music_5050034
    {0xfa842bc07360137d},       // music_5050035
    {0xf8d72c405d3f0456},       // music_5050036
    {0xd4d5fa6c87342e6b},       // music_5050037
    {0xd8cbc946fa660944},       // music_5050038
    {0x0fac398719cd9e4a},       // music_5050039
    {0x09c4ba796548a019},       // music_5050040
    {0x7e7c462ba7d473cf},       // music_5050041
    {0x8a9a7af1379840fb},       // music_5050042
    {0xa0aa0097e5631019},       // music_5050043
    {0xe278eccf08eb2565},       // music_5050044
    {0x1cf133b26d8160d1},       // music_5050045
    {0xda08e9d3961c93f2},       // music_5050046
    {0x58d97e6f3d1aee86},       // music_5050047
    {0x57353b771188635e},       // music_5050048
    {0xeb9ad1180d7e1b53},       // music_5050049
    {0xaec8dbd5f5337a9e},       // music_5050050
    {0x49d08922136334ce},       // music_5050051
    {0x138df0b866e902e0},       // music_5050052
    {0xc076e8604740ff5f},       // music_5050053
    {0x69fe38ae5970d450},       // music_5050054
    {0x414200bd8ac11b40},       // music_5050055
    {0xbce9e85d31089fb2},       // music_5050056
    {0x0817b919679c96d7},       // music_5050057
    {0x3e0e51043bd7d5e5},       // music_5050058
    {0x86d17e28b2f2b91c},       // music_5050059
    {0x115f906b6b7fb845},       // music_5050060
    {0xa8d5e9b1c6cf1505},       // music_5050061
    {0x69ffd3fefdf7ee71},       // music_5050062
    {0x571e646778541f4d},       // music_5050063
    {0xe8b5323ec07608e7},       // music_5050064
    {0x27992dd621b8a07e},       // music_5050065
    {0x8e2a8439f5628513},       // music_5050066
    {0x8b5be21e70a84eed},       // music_5050067
    {0x227297416c6ccc7c},       // music_5050068
    {0xb544dc8524419109},       // music_5050069
    {0x6c2d9160672cbf95},       // music_5050070
    {0x7ff6630286d2d93b},       // music_5050071
    {0xc6deecd2d1391713},       // music_5050072
    {0x78bec41dd27d8788},       // music_5050074
    {0x0f86991a3b9aec2b},       // music_5050075
    {0x8f750fabaa794130},       // music_5050076
    {0x3c68e8102dbec720},       // music_5050077
    {0xf653b47bc8d4d1cd},       // music_5050079
    {0xb50f482149140fda},       // music_5050080
    {0xd61cc4e14e7073f4},       // music_5050081
    {0xfba77b717e43a39a},       // music_5050082
    {0x85a236b5270bac29},       // music_5050083
    {0x818d37d319d4c177},       // music_5050084
    {0xc16fb31c74eb5e59},       // music_5050085
    {0x598e133e0673b1e6},       // music_5050086
    {0x4cb2e8101df88d6f},       // music_5050087
    {0x3f8abfcd47711be2},       // music_5050088
    {0xcdb3f9edbd51012f},       // music_5050089
    {0xa28c9867b32a60e1},       // music_5050090
    {0xb1e06cf5f6a790c2},       // music_5050091
    {0xcfb9a7e64443e95c},       // music_5050093
    {0xf9ef74ac89bdfb7d},       // music_5050094
    {0x561e1e17dfb055ce},       // music_5050095
    {0x46967c2bc5d4d050},       // music_5050096
    {0xdcdaeb3067868ad9},       // music_5050097
    {0x18fab58c80c85580},       // music_5050098
    {0xba4484d824fb61af},       // music_5050099
    {0xb70fe5c5e12c7a1c},       // music_5050100
    {0x7f5d26ba72161054},       // music_5050101
    {0x79c1f27fa0f8c937},       // music_5050103
    {0xe1e4f9125646aa8a},       // music_5050104
    {0xd5cf3ce581c59e40},       // music_5050105
    {0x5509e33bc008bf09},       // music_5050106
    {0x5ecb21ac94aa4b8f},       // music_5050107
    {0x3786b3940e98628a},       // music_5050108
    {0x54de39434bfe4f07},       // music_5050109
    {0x919d24cc51244387},       // music_5050110
    {0x9d020e395e34dc9d},       // music_5050111
    {0x35cd0df418d118d7},       // music_5050112
    {0xf8c3ee3e6a9034f1},       // music_5050113
    {0x77878d37ce7af3de},       // music_5050114
    {0xc017329b350ece41},       // music_5050115
    {0xf593a4edff474f67},       // music_5050116
    {0xe1972fbb1f007ddb},       // music_5050117
    {0xfed8de6081bb79a4},       // music_5050118
    {0x4877977d05b312f3},       // music_5050119
    {0x7eaa69de94e0eff1},       // music_5050120
    {0xee1270b0ecf19fa9},       // music_5050121
    {0x194e2cf79077c167},       // music_5050122
    {0x9dbfafd78a550632},       // music_5050123
    {0xf95b5bbf7e04f6f7},       // music_5050124
    {0x72eab426f4800cdf},       // music_5050125
    {0x0fbaaa063a92e3fa},       // music_5050126
    {0x1b837c9a98b7d123},       // music_5050127
    {0x94f28e181640e219},       // music_5050128
    {0x5d931d29d1432b4c},       // music_5050129
    {0xc50b5e9a5adcaae4},       // music_5050130
    {0xeb7796684409adfa},       // music_5050131
    {0x13ef11289cf31dad},       // music_5050132
    {0xd446d0ea96dfdf76},       // music_5050133
    {0x04c56ec4e341a717},       // music_5050134
    {0x82523f6386d6a38a},       // music_5050135
    {0x520868bafa84e471},       // music_5050136
    {0x7aab43829c6be9be},       // music_5050137
    {0xa00273b3d953e84d},       // music_5050138
    {0x6b3eeb4debf6a39c},       // music_5050139
    {0xa679e86ed8e96691},       // music_5050140
    {0x399025390570ea40},       // music_5050141
    {0xb8489d1921c79d9e},       // music_5050142
    {0xb47249ccc6c7528b},       // music_5050143
    {0xec5880daa44d87ce},       // music_5050145
    {0xbda2730cb81e55e1},       // music_5050146
    {0x1be3e8255dde31e3},       // music_5050147
    {0x58b735f1a68c9a81},       // music_5050148
    {0x5b3cb281d89019db},       // music_5050149
    {0x74f39448d2b75d4d},       // music_5050150
    {0x2b1a0269a3d890d4},       // music_5050151
    {0x74e058fd628364c9},       // music_5050152
    {0x3ade68d4857395c0},       // music_5050153
    {0x68f7f556af1c2fc6},       // music_5050154
    {0xaf67059daac6c89d},       // music_5050155
    {0xc557fa24c5c887e8},       // music_5050156
    {0xbad0b0f6dc3b004c},       // music_5050157
    {0x1838300a8bb9b8ff},       // music_5050158
    {0xffb4afe38bb4a3dd},       // music_5050159
    {0x22cfb3e29832a25a},       // music_5050160
    {0xa86bbfe35949b53e},       // music_5050161
    {0x6f321adde08396e3},       // music_5050163
    {0x58afa6381eeb1425},       // music_5050164
    {0x751daf7d1a5401cb},       // music_5050165
    {0x0ff2ea77b81c3ff6},       // music_5050166
    {0xbd6b66823f854f68},       // music_5050167
    {0xceb902b93eba45d8},       // music_5050168
    {0x2550e145b93723ae},       // music_5050169
    {0xb512188a55b8b698},       // music_5050170
    {0x26e5bf2c66a9898d},       // music_5050171
    {0x12c07d8b1b9c6e8e},       // music_5050172
    {0xe2b8148cc265cda2},       // music_5050173
    {0x5275e4ca06cb46ca},       // music_5050174
    {0x851f3b41203ff5ed},       // music_5050175
    {0xd93d0a1764e85d4d},       // music_5050176
    {0xff8c5c13833b1049},       // music_5050177
    {0x59efd868058a402e},       // music_5050178
    {0x93075c0fbb31f463},       // music_5050179
    {0xdedb6ae518faac3d},       // music_5050180
    {0xe997edae32a5fa3d},       // music_5050181
    {0xe19e00aebc8b309b},       // music_5050182
    {0x10574a2abfe8ab99},       // music_5050183
    {0xbd2f4b5ab481d300},       // music_5050184
    {0x1a253c2f40a38917},       // music_5050185
    {0xc735e9f28243ea8a},       // music_5050186
    {0x8137f876a1a8850b},       // music_5050187
    {0x82bd2fef6ad49c01},       // music_5050188
    {0xe77f67dd5b7aed71},       // music_5050189
    {0x2896fe0212b06d81},       // music_5050190
    {0xe60abdb973ad274a},       // music_5050191
    {0xe84e1005c5eeab22},       // music_5050192
    {0x1374b00b87c43440},       // music_5050193
    {0xf9b45750656b28bd},       // music_5050194
    {0x32f55efe2bd0a4fd},       // music_5050195
    {0x9092a8ef4ebacb7f},       // music_5050196
    {0x39a04e2ebc026177},       // music_5050197
    {0x1c848089b2d1a7b6},       // music_5050198
    {0x24feeee9ce287093},       // music_5050199
    {0x4226eb6f52ad4637},       // music_5050200
    {0x59ab358c5cded7c4},       // music_5050201
    {0x29ca209dae13020a},       // music_5050202
    {0x7fe1bc7cbda4a457},       // music_5050203
    {0x928964b93fce5bc9},       // music_5050204
    {0x58a46c0179f0d6ff},       // music_5050205
    {0x0929ca6a5e8e0445},       // music_5050206
    {0xf0de6097ea78513c},       // music_5050207
    {0xcde0de0e1216bdea},       // music_5050208
    {0xb496e9301bfc57c4},       // music_5050209
    {0xff4db63086831e8f},       // music_5050210
    {0x21cd7a36d33daae9},       // music_5050211
    {0x88fe1d5592979a66},       // music_5050212
    {0x1782668d98097f3b},       // music_5050213
    {0xb174215f3ed746e3},       // music_5050214
    {0x445aa44e5029d76b},       // music_5050215
    {0x39b3d29fc04211ed},       // music_5050216
    {0x6856a8773f3e7712},       // music_5050217
    {0x7f08330d7d971292},       // music_5050218
    {0x6ac19245cd7e8de0},       // music_5050219
    {0x7a0ed12fd8b5a9e8},       // music_5050220
    {0xc13b8037e2be01bf},       // music_5050221
    {0x5856576aa5c0a59f},       // music_5050222
    {0xe8286a95b8f38dd1},       // music_5050223
    {0x48015b291de44ff3},       // music_5050224
    {0x7b1a2559ece20f11},       // music_5050225
    {0x4521644fd1682bc0},       // music_5050226
    {0x808e307ec28b9ef0},       // music_5050227
    {0x6a4493f6e7dcce6c},       // music_5050228
    {0xd6bc23de632b00e0},       // music_5050229
    {0x7268ea33e33cbcfa},       // music_5050230
    {0x2de61d98bc2cea48},       // music_5050231
    {0x249fb9a25c75632b},       // music_5050232
    {0x60cf8b7ef1f1ba8c},       // music_5050233
    {0x0e42f74d5567bd5e},       // music_5050234
    {0x3e2b8152d40c4111},       // music_5050235
    {0x812e320bf455509e},       // music_5050236
    {0x55882d342d85c5bf},       // music_5050237
    {0x7731e49facd28485},       // music_5050238
    {0x2555ab1e06fdb167},       // music_5050239
    {0x3890abb5245d6c6f},       // music_5050240
    {0x2107ffff2a8fd88a},       // music_5050241
    {0xd8bdb31ab72c355b},       // music_5050242
    {0x2ade6f09d4c00bdf},       // music_5050245
    {0xa92c33355f232b5a},       // music_5050246
    {0xa0a3629f5d35eef4},       // music_5050247
    {0x07e30b53c3bc4efe},       // music_5050248
    {0x3fcd37732e64f44a},       // music_5050249
    {0x8e42f8b651700386},       // music_5050250
    {0x51e07b700d1bc711},       // music_5050251
    {0x7271a9fca4d4a183},       // music_5050252
    {0x457e9e3b040915bb},       // music_5050253
    {0xfc5419996922fc03},       // music_5050254
    {0x6d2e7b90423ac71d},       // music_5050255
    {0xa4b16fbd969bf071},       // music_5050256
    {0xca4073d3b6fafc65},       // music_5050257
    {0x7ce2075bcd63e8ee},       // music_5050258
    {0x532d1f45db13c367},       // music_5050259
    {0xb86e633c85e819c4},       // music_5050260
    {0x717100eb7bac3d19},       // music_5050261
    {0xdd1041820d825da4},       // music_5050262
    {0x11e9578c3c12a4ad},       // music_5050263
    {0xc74ff984f50113d3},       // music_5050264
    {0x84d2e4313779511b},       // music_5050265
    {0x780e8260c79e77b9},       // music_5050268
    {0xf72c77898bfc4678},       // music_5050269
    {0xbe0d5c4c95e909ff},       // music_5050270
    {0x1f74bfe24e614687},       // music_5050271
    {0x02b24e5c7c7b6e9d},       // music_5050272
    {0xf48f21a99fda09cf},       // music_5050273
    {0x5ef1786f364bce23},       // music_5050274
    {0x9577c9645a6ffd5e},       // music_5050275
    {0xafc13a64a56884e8},       // music_5050276
    {0x76dfb4c3728fe8d9},       // music_5050277
    {0x071a776b3ed5ab17},       // music_5050278
    {0x17ae871f1b26d068},       // music_5050279
    {0x5d3eadc8aecfa00c},       // music_5050280
    {0xc3552716a8bde9fe},       // music_5050281
    {0x880a35323f69b612},       // music_5050284
    {0x5c8623402d1c822d},       // music_5050286
    {0xca545af62852a7b7},       // music_5050287
    {0x518c6e5c4a268161},       // music_5050288
    {0x3b03fdfe671ddf68},       // music_5050289
    {0x7566fe3ae0662094},       // music_5050290
    {0x51d39f5ef693843c},       // music_5050291
    {0xb259f525f1f359d0},       // music_5050292
    {0xd2f2d004121c4118},       // music_5050293
    {0x775610e63d2b622b},       // music_5050294
    {0xcc73017924bfaaae},       // music_5050295
    {0xd2197c8cf0ea08f9},       // music_5050296
    {0xad6592b9a7f59dd4},       // music_5050298
    {0x26577d8c85702e9},        // music_5050299
    {0x8886b6ab9e5642a6},       // music_5050300
    {0xf02f820d32f0abe7},       // music_5050301
    {0x67d0a863e87d513a},       // music_5050302
    {0x481c17fb41e25dbb},       // music_5050303
    {0xe259362b1d601f93},       // music_5050304
    {0x7698628d25ad406b},       // music_5050305
    {0x34c0f6db642145a0},       // music_5050307
    {0xb7ecea9165c448da},       // music_5050308
    {0xa5e9bd945c5caf2c},       // music_5050309
    {0x8f88e97a742ec2b6},       // music_5050310
    {0xdafc7d4d918a6282},       // music_5050311
    {0x9260652a706b3616},       // music_5050312
    {0x91ff4aedae9ce2c3},       // music_5050313
    {0xeaaa417505d65dd1},       // music_5050322
    {0x591899d025c3beb7},       // music_5050323
    {0xa57678c62ef99124},       // music_5050324
    {0x925f360a8ccb4c32},       // music_5050325
    {0x2a9281f77161e068},       // music_5050326
    {0x3a00b50e407febcb},       // music_5050327
    {0x83c86bfce70eebaa},       // music_5050328
    {0x250a01be07e87ea4},       // music_5050329
    {0x664c54cd6651be76},       // music_5050330
    {0x1b51c4bfabf7a714},       // music_5050331
    {0xf61a55108c357816},       // music_5050332
    {0xdd901ab93ca2d5f7},       // music_5050333
    {0xec02897a679b5774},       // music_5050334
    {0x14682028545766d3},       // music_5050335
    {0xfed08d44b53c83e9},       // music_5050336
    {0xb6d6c6c134d3e63b},       // music_5050337
    {0x6ee22156d4de16d6},       // music_5050338
    {0x9adcbd66f3b2c042},       // music_5050339
    {0x333c0ee92e117284},       // music_5050340
    {0x52c250eade92393b},       // music_9010001
    {0xf66e6bb5b0599b07},       // music_9010002
    {0x8582b5a60dbbf948},       // music_9010003
    {0x5bb84b8a5677046f},       // music_9010004
    {0xfea0d6adff136868},       // music_9050001
    {0x19480b946279507a},       // music_9050002

    // Mini 4WD Hyper Dash Grand Prix (Android)
    {7957824642808300098},      // 6E6FDF59AB704242

    // m HOLD'EM (Android)
    {369211553984367},          // 00014FCBC385AF6F

    // Sonic Colors Ultimate (multi)
    {1991062320101111},         // 000712DC5250B6F7

    // ALTDEUS: Beyond Chronos (PC) [base-string 14238637353525924 + mods)]
    {14238637353525934},        // 003295F7198AE2AE - bgm
    {14238637353525954},        // 003295F7198AE2C2 - se
    {14238637353525944},        // 003295F7198AE2B8 - voice

    // SHOW BY ROCK!! Fes A Live (Android)
    {54605542411982574},        // 00C1FF73963BD6EE

    // Touhou Danmaku Kagura (Android)
    {5654863921795627},         // 001417119B4FD22B

    // Nogizaka 46 Fractal (Android)
    {984635491346198130},       // 0DAA20C336EEAE72

    // NEO: The World Ends With You (PC)
    {53346933792338754},        // 00BD86C0EE8C7342

    // THE iDOLM@STER Starlit Season (PS4/PC)
    {0x1e03b570b6145d1d},       // BGM
    {0x1da915aaa181a461},       // SE
    {0x1c82b6ab7487a5ec},       // Voice
    {0x6d275d3666c2f9c8},       // Sng001
    {0x0f53815df3044e6d},       // Sng002
    {0x158778e2e2fab347},       // Sng003
    {0x16b75e8b5247d46b},       // Sng004
    {0x157df8a6047048fc},       // Sng005
    {0x184d358b50b658d0},       // Sng006
    {0x157fb75af4ddd983},       // Sng007
    {0x404ba38c3e470827},       // Sng008
    {0x01d0b788a3b60d48},       // Sng009
    {0x021718d55d0960c9},       // Sng010
    {0x0021c5993d2b901c},       // Sng011
    {0x08237bcb9b711087},       // Sng012
    {0x01af60402e1228a5},       // Sng013
    {0x4eec18ab73a1a634},       // Sng014
    {0x1855099898b11ad9},       // Sng015
    {0x57ef8f2ea5d54db5},       // Sng016
    {0x17cc6975d67e2a1f},       // Sng017
    {0x0a5d0fc8cc5c4502},       // Sng018
    {0x198ea1a17416050b},       // Sng019
    {0x2aa3b8abad207a1e},       // Sng020
    {0x4ee10a3e3bb19e57},       // Sng021
    {0x08ad2fe12c79bca9},       // Sng022
    {0x18488992b1632ef5},       // Sng023
    {0x1175edbbacc1fc18},       // Sng024
    {0x0e14d06d7f7a6c8c},       // Sng025
    {0x33d98a3a9f9bfdef},       // Sng026
    {0x2284fd5ca82c78f4},       // Sng027
    {0x178a76b6436d20f0},       // Sng028
    {0x3ff99f2fed65a1ed},       // Sng030

    // Ulala: Idle Adventure (Android)
    {20191022},                 // 000000000134172E

    // Girls' Frontline: Project Neural Cloud (Android)
    {210222522032314},          // 0000BF323EBFE0BA

    // Super Robot Wars 30 (PC)
    {6734488621090458},         // 0017ECFB5201069A

    // CHUNITHM NEW (AC)
    {32931609366120192},        // 0074FF1FCE264700

    // Shaman King: Funbari Chronicle (Android)
    {1620612098671},            // 0000017954022A6F

    // Heaven Burns Red (Android)
    {7355875239102698567},      // 6615518E8ECED447

    // Digimon ReArise (Android)
    {34810080072368384},        // 007BAB9559510100

    // ANNO: Mutationem (PC)
    {351185040111633400},       // 04DFA8EEEE4903F8

    // Priconne! Grand Masters (iOS/Android)
    {185705658241},             // 0000002B3CEB7781

    // Sonic Origins (multi)
    {1991062320220623},         // 000712DC525289CF

    // Pachislot Gyakuten Saiban (iOS/Android)
    {2220022040477322},         // 0007E319291DBE8A

    // Alice Fiction (Android)
    {112089817},                // 0000000006AE5AD9

    // Taiko no Tatsujin: Rhythm Festival (Switch)
    {52539816150204134},        // 00baa8af36327ee6

    // Fairy Fencer F: Refrain Chord (multi)
    {348693553056839375},       // 04D6CEF0656BF6CF

    // Echoes of Mana (Android)
    {1090221945250803295},      // 0F213F153D026E5F

    // Persona 5 Royal (Switch)
    {9923540143823782},         // 002341683D2FDBA6

    // P Sengoku Otome 6 ~Akatsuki no Sekigahara~ (Android)
    {97648135},                 // 0000000005d1fe07

    // CHUNITHM International Version (AC)
    {33426922444908636},        // 0076C19BDE43685C

    // Star Ocean: The Divine Force (PC)
    {68308868861462528},        // 00f2ae8de77f0800

    // Sin Chronicle (Android)
    {4385672148314579020},      // 3CDD0995259D604C

    // The Eminence in Shadow: Master of Garden (Android)
    {8115775984160473168},      // 70A1074224880050

    // Punishing: Gray Raven (Android)-newer assets
    {62855594017927612},        // 00DF4ED46995B1BC 

    // Dragon Quest The Adventure of Dai: A Hero's Bonds (Android)
    {503876638109847402},       // 06FE2121F927CF6A (in criware decryptor init code, xor of 2 strings)

    // Love Live! School Idol Festival 2 MIRACLE LIVE (Android)
    {5067530812966687744},      // 46537c6ceb39d400

    // 404 Game RE:SET (Android)
    {21588207721978971},        // 004CB25C4C9B1C5B

    // Disney Star Smash (Android)
    {3941111674189632},         // 000E006B915C3740

    // Final Fantasy VII: Ever Crisis (beta) (Android)
    {19629307353822},           // 000011DA4DE45ADE

    // Tower of Sky (Android)
    {5893902754391672},         // 0014F0792C042678

    // The Irregular at Magic High School: Reloaded Memory (Android)
    {7181280307767738368},      // 63A9086451010000

    // BlazBlue Entropy Effect (Early Access) (PC)
    {29814655674508831},        // 0069EC457894661F

    // Star Ocean: The Second Story R (PC, Switch)
    {533948357975462459},       // 0768F733DD87D23B

    // Girls' Frontline 2: Exilium (Android)
    {8930254087621254},         // 001FBA04CEA58A86

    // Sonic Superstars (Switch)
    {1991062320230623},         // 000712DC5252B0DF

    // Persona 5 Tactica (Switch)
    {48319776512953016},        // 00ABAA94AAAE4AB8

    // THE IDOLM@STER Shiny Colors Song For Prism (PC)
    {156967709847897761},       // 022DA94CEAB0C6A1

    // Dokapon Kingdom Connect (PC, Switch)
    {104863924750642073},       // 01748d2f1883eb99

    // Girls' Frontline 2: Exilium (PC)
    {7152097263845921},         // 001968CB68CF8221

    // SD Shin Kamen Rider Rumble (PC)
    {4079616028775461768},      // 389DB529D726B388

    // Persona 3 Reload (PC)
    {11918920},                 // 0000000000B5DE48

    // IDOL FANTASY (Android)
    {4308390175392019587},      // 3BCA7A09A6063483

    // re:colloquial (Android)
    {9863154698521},            // 000008F871D50919

    // Ojisama to Nego Super Miracle Puzzle (Android)
    {5783843453453438113},      // 504456FA0F8F8CA1

    // ARGONAVIS from BanG Dream! AAside (Android)
    {1403972877097251940},      // 137BE9E4A5AA3464

    // ARGONAVIS -Kimi ga Mita Stage e- (Android)
    {301179795002661},          // 000111EBE2B1D525

    // YuYuYui (Switch)
    {3733383634313939},         // 000D437E1D5562D3

    // Pachislot Valvrave (iOS)
    {2753732797542815},         // 0009C8816134319F

    // Jujutsu Kaisen: Cursed Clash (multi)
    {984703514306706654},       // 0DAA5EA10B547CDE

    // Puyo Puyo Puzzle Pop (iOS)
    {9999},                     // 000000000000270F

    // Penny Blood: Hellbound (PC)
    {845873498572},             // 000000C4F1FD49CC

    // Sonic Rumble (Android)
    {6834182874188563},         // 001847A7328BCB13

    // P A Certain Magical Index 1/2 (Android)
    {5963},                     // 000000000000174B

    // Reynatis (Switch)
    {5963496778882477625},      // 52C298B97479EE39

    // Buddysma!! ~Smartphone Start! Buddyfight (Android)
    {8132462270},               // 00000001E4BB86BE

    // Code Geass: Lost Stories (Android)
    {9182735170},               // 0000000223556B42

    // Super Robot Wars DD (Android)
    {464113616464131416},       // 0670DCE00CC43558

    // Ange Re:Link (Android)
    {9666854456},               // 0000000240307E38

    // Metaphor: ReFantazio Demo (PC)
    {572184256205070254},       // 07F0CE8CA92C9FAE

    // DanMachi BATTLE CHRONICLE (Android)
    {8893372566804407},         // 001F9879A8B317B7

    // DRAGON BALL: Sparking! ZERO (multi)
    {13238534807163085345u},    // B7B8B9442F99A221

    // TOUHOU GENSOU MAHJONG (PC)
    {7757726886},               // 00000001CE6584A6

    // NARUTO X BORUTO NINJA VOLTAGE (Android)
    {15607939238842976091u},    // D89A8A0B171DC75B

    // Kingdom Hearts 3 (Steam)
    {10095514444067761537u},    // 8C1A78E6077BED81

    // Muv-Luv Dimensions (Android)
    {8848},                     // 0000000000002290

    // Tales of Graces f Remastered (PC)
    {51485416730473395},        // 00B6E9B6B75533B3

    // Freedom Wars Remastered (Switch)
    {3258660547165106863},      // 2D391680A55B32AF

    // Suikoden I & II HD Remaster (PC)
    {14510296783270449627u},    // C95EEE0BA85411DB

    // CHUNITHM Chinese Version (AC)
    {30194896045700459},        // 006B461914D5756B

    // Raidou Remastered: The Mystery of the Soulless Army (PS4)
    {954534454324},             // 000000DE3EAFE434

    // Sand Land (multi)
    {910990237314908160},       // 0CA47CCB51010000

    // Patapon 1 & 2 Reloaded (PC)
    {3316332033470531258},      // 2E05FA69EC4286BA

    // Aether Gazer (multi)
    {1578660190369042886},      // 15E887143BFF51C6

    // Sonic Racing CrossWorlds (multi)
    {32105414741057402},        // 00720FB46101DF7A

    // Ride Kamens (iOS/Android)
    {20000608},                 // 0000000001312F60

    // The Touhou Empires - Demo (PC)
    {946783},                   // 00000000000E725F

    // Pac-Man World 2 Re-Pac (PC)
    {5646981318476813},         // 00140FE64B8A840D

    // Towa and the Guardians of the Sacred Tree (multi)
    {2024060613531654235},      // 1C16E8677169D85B

};

#endif
