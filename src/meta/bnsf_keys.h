#ifndef _BNSF_KEYS_H_
#define _BNSF_KEYS_H_

typedef struct {
    const char* key;
} bnsfkey_info;

/* Known keys, from games' exe (iM@S, near "nus" strings) or files (Tales, config in audio bigfiles).
 *
 * In memdumps, first 16 chars of key can be found XORed with "Ua#oK3P94vdxX,ft" after AES 'Td'
 * mix tables (that end with 8D4697A3 A38D4697 97A38D46 4697A38D), then can be cross referenced
 * with other strings (max 24 chars) in the memdump. */
static const bnsfkey_info s14key_list[] = {

        /* THE iDOLM@STER 2 (PS3/X360) */
        {"haruka17imas"},

        /* Tales of Zestiria (PS3) */
        {"TO12_SPSLoc"},

        /* Tales of Berseria (PS3) */
        {"SPSLOC13"},

        /* THE iDOLM@STER: One For All (PS3) */
        {"86c215d7655eefb5c77ae92c"},

};

#endif /*_BNSF_KEYS_H_*/
