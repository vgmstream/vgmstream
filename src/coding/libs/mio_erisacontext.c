/*****************************************************************************
                         E R I S A - L i b r a r y
 -----------------------------------------------------------------------------
   Copyright (C) 2002-2003 Leshade Entis, Entis-soft. All rights reserved.
 *****************************************************************************/

#include <stdlib.h>
#include "mio_xerisa.h"

#define MIO_ERISA_SYMBOL_ERROR -1

/************************/
/* huffman tree helpers */
/************************/

static void EHT_Initialize(ERINA_HUFFMAN_TREE* tree) {
    for (int i = 0; i < MIO_HUFFMAN_SYMBOLS; i++) {
        tree->m_iSymLookup[i] = ERINA_HUFFMAN_NULL;
    }
    tree->m_iEscape = ERINA_HUFFMAN_NULL;
    tree->m_iTreePointer = ERINA_HUFFMAN_ROOT;
    tree->m_hnTree[ERINA_HUFFMAN_ROOT].weight = 0;
    tree->m_hnTree[ERINA_HUFFMAN_ROOT].parent = ERINA_HUFFMAN_NULL;
    tree->m_hnTree[ERINA_HUFFMAN_ROOT].child_code = ERINA_HUFFMAN_NULL;
}

static void EHT_RecountOccuredCount(ERINA_HUFFMAN_TREE* tree, int iParent) {
    int iChild = tree->m_hnTree[iParent].child_code;
    tree->m_hnTree[iParent].weight = tree->m_hnTree[iChild].weight + tree->m_hnTree[iChild + 1].weight;
}

static void EHT_Normalize(ERINA_HUFFMAN_TREE* tree, int iEntry) {
    while (iEntry < ERINA_HUFFMAN_ROOT) {
        // find swap entry
        int iSwap = iEntry + 1;
        WORD weight = tree->m_hnTree[iEntry].weight;
        while (iSwap < ERINA_HUFFMAN_ROOT) {
            if (tree->m_hnTree[iSwap].weight >= weight) break;
            ++iSwap;
        }
        if (iEntry == --iSwap) {
            iEntry = tree->m_hnTree[iEntry].parent;
            EHT_RecountOccuredCount(tree, iEntry);
            continue;
        }

        // swap
        int iChild, nCode;
        if (!(tree->m_hnTree[iEntry].child_code & ERINA_CODE_FLAG)) {
            iChild = tree->m_hnTree[iEntry].child_code;
            tree->m_hnTree[iChild].parent = iSwap;
            tree->m_hnTree[iChild + 1].parent = iSwap;
        }
        else {
            nCode = tree->m_hnTree[iEntry].child_code & ~ERINA_CODE_FLAG;
            if (nCode != ERINA_HUFFMAN_ESCAPE)
                tree->m_iSymLookup[nCode & 0xFF] = iSwap;
            else
                tree->m_iEscape = iSwap;
        }
        if (!(tree->m_hnTree[iSwap].child_code & ERINA_CODE_FLAG)) {
            int iChild = tree->m_hnTree[iSwap].child_code;
            tree->m_hnTree[iChild].parent = iEntry;
            tree->m_hnTree[iChild + 1].parent = iEntry;
        }
        else {
            int nCode = tree->m_hnTree[iSwap].child_code & ~ERINA_CODE_FLAG;
            if (nCode != ERINA_HUFFMAN_ESCAPE)
                tree->m_iSymLookup[nCode & 0xFF] = iEntry;
            else
                tree->m_iEscape = iEntry;
        }

        ERINA_HUFFMAN_NODE node;
        WORD iEntryParent = tree->m_hnTree[iEntry].parent;
        WORD iSwapParent = tree->m_hnTree[iSwap].parent;
        node = tree->m_hnTree[iSwap];
        tree->m_hnTree[iSwap] = tree->m_hnTree[iEntry];
        tree->m_hnTree[iEntry] = node;
        tree->m_hnTree[iSwap].parent = iSwapParent;
        tree->m_hnTree[iEntry].parent = iEntryParent;

         // recalc parent weight
        EHT_RecountOccuredCount(tree, iSwapParent);
        iEntry = iSwapParent;
    }
}

static void EHT_AddNewEntry(ERINA_HUFFMAN_TREE* tree, int nNewCode) {
    if (tree->m_iTreePointer > 0) {
        // reserve 2 areas
        int i = tree->m_iTreePointer = tree->m_iTreePointer - 2;

        // setup new entry
        ERINA_HUFFMAN_NODE* phnNew = &tree->m_hnTree[i];
        phnNew->weight = 1;
        phnNew->child_code = ERINA_CODE_FLAG | nNewCode;
        tree->m_iSymLookup[nNewCode & 0xFF] = i;

        ERINA_HUFFMAN_NODE* phnRoot = &tree->m_hnTree[ERINA_HUFFMAN_ROOT];
        if (phnRoot->child_code != ERINA_HUFFMAN_NULL) {
            // add new entry
            ERINA_HUFFMAN_NODE* phnParent = &tree->m_hnTree[i + 2];
            ERINA_HUFFMAN_NODE* phnChild = &tree->m_hnTree[i + 1];
            tree->m_hnTree[i + 1] = tree->m_hnTree[i + 2];

            if (phnChild->child_code & ERINA_CODE_FLAG) {
                int nCode = phnChild->child_code & ~ERINA_CODE_FLAG;
                if (nCode != ERINA_HUFFMAN_ESCAPE)
                    tree->m_iSymLookup[nCode & 0xFF] = i + 1;
                else
                    tree->m_iEscape = i + 1;
            }

            phnParent->weight = phnNew->weight + phnChild->weight;
            phnParent->parent = phnChild->parent;
            phnParent->child_code = i;

            phnNew->parent = phnChild->parent = i + 2;

            // fix parent entry
            EHT_Normalize(tree, i + 2);
        }
        else {
            // create initial tree state
            phnNew->parent = ERINA_HUFFMAN_ROOT;

            ERINA_HUFFMAN_NODE* phnEscape = &tree->m_hnTree[tree->m_iEscape = i + 1];
            phnEscape->weight = 1;
            phnEscape->parent = ERINA_HUFFMAN_ROOT;
            phnEscape->child_code = ERINA_CODE_FLAG | ERINA_HUFFMAN_ESCAPE;

            phnRoot->weight = 2;
            phnRoot->child_code = i;
        }
    }
    else {
        // replace least ocurring symbol with new symbol
        int i = tree->m_iTreePointer;
        ERINA_HUFFMAN_NODE* phnEntry = &tree->m_hnTree[i];
        if (phnEntry->child_code == (ERINA_CODE_FLAG | ERINA_HUFFMAN_ESCAPE)) {
            phnEntry = &tree->m_hnTree[i + 1];
        }
        phnEntry->child_code = ERINA_CODE_FLAG | nNewCode;
    }
}

static void EHT_HalfAndRebuild(ERINA_HUFFMAN_TREE* tree) {
    // halve ocurrence count and rebuild the tree
    int iNextEntry = ERINA_HUFFMAN_ROOT;
    for (int i = ERINA_HUFFMAN_ROOT - 1; i >= tree->m_iTreePointer; i--) {
        if (tree->m_hnTree[i].child_code & ERINA_CODE_FLAG) {
            tree->m_hnTree[i].weight = (tree->m_hnTree[i].weight + 1) >> 1;
            tree->m_hnTree[iNextEntry--] = tree->m_hnTree[i];
        }
    }
    ++iNextEntry;

    // rebuild tree
    int iChild, nCode;
    int i = tree->m_iTreePointer;
    for (;;) {
        // put the smallest 2 entries into the huffman tree
        tree->m_hnTree[i] = tree->m_hnTree[iNextEntry];
        tree->m_hnTree[i + 1] = tree->m_hnTree[iNextEntry + 1];
        iNextEntry += 2;
        ERINA_HUFFMAN_NODE* phnChild1 = &tree->m_hnTree[i];
        ERINA_HUFFMAN_NODE* phnChild2 = &tree->m_hnTree[i + 1];

        if (!(phnChild1->child_code & ERINA_CODE_FLAG)) {
            iChild = phnChild1->child_code;
            tree->m_hnTree[iChild].parent = i;
            tree->m_hnTree[iChild + 1].parent = i;
        }
        else {
            nCode = phnChild1->child_code & ~ERINA_CODE_FLAG;
            if (nCode == ERINA_HUFFMAN_ESCAPE)
                tree->m_iEscape = i;
            else
                tree->m_iSymLookup[nCode & 0xFF] = i;
        }

        if (!(phnChild2->child_code & ERINA_CODE_FLAG)) {
            iChild = phnChild2->child_code;
            tree->m_hnTree[iChild].parent = i + 1;
            tree->m_hnTree[iChild + 1].parent = i + 1;
        }
        else {
            nCode = phnChild2->child_code & ~ERINA_CODE_FLAG;
            if (nCode == ERINA_HUFFMAN_ESCAPE)
                tree->m_iEscape = i + 1;
            else
                tree->m_iSymLookup[nCode & 0xFF] = i + 1;
        }

        WORD weight = phnChild1->weight + phnChild2->weight;

        // include parent entry in list
        if (iNextEntry <= ERINA_HUFFMAN_ROOT) {
            int j = iNextEntry;
            for (;;) {
                if (weight <= tree->m_hnTree[j].weight) {
                    tree->m_hnTree[j - 1].weight = weight;
                    tree->m_hnTree[j - 1].child_code = i;
                    break;
                }
                tree->m_hnTree[j - 1] = tree->m_hnTree[j];
                if (++j > ERINA_HUFFMAN_ROOT) {
                    tree->m_hnTree[ERINA_HUFFMAN_ROOT].weight = weight;
                    tree->m_hnTree[ERINA_HUFFMAN_ROOT].child_code = i;
                    break;
                }
            }
            --iNextEntry;
        }
        else {
            tree->m_hnTree[ERINA_HUFFMAN_ROOT].weight = weight;
            tree->m_hnTree[ERINA_HUFFMAN_ROOT].parent = ERINA_HUFFMAN_NULL;
            tree->m_hnTree[ERINA_HUFFMAN_ROOT].child_code = i;
            phnChild1->parent = ERINA_HUFFMAN_ROOT;
            phnChild2->parent = ERINA_HUFFMAN_ROOT;
            break;
        }

        i += 2;
    }
}

static void EHT_IncreaseOccuredCount(ERINA_HUFFMAN_TREE* tree, int iEntry) {
    tree->m_hnTree[iEntry].weight++;
    EHT_Normalize(tree, iEntry);

    if (tree->m_hnTree[ERINA_HUFFMAN_ROOT].weight >= ERINA_HUFFMAN_MAX) {
        EHT_HalfAndRebuild(tree);
    }
}

/*****************************/
/* arithmetic coding helpers */
/*****************************/

static void EPM_Initialize(ERISA_PROB_MODEL* prob) {
    prob->dwTotalCount = ERISA_SYMBOL_SORTS;
    prob->dwSymbolSorts = ERISA_SYMBOL_SORTS;

    for (int i = 0; i < 0x100; i++) {
        prob->acsSymTable[i].wOccured = 1;
        prob->acsSymTable[i].wSymbol = (SWORD)(BYTE)i;
    }
    prob->acsSymTable[0x100].wOccured = 1;
    prob->acsSymTable[0x100].wSymbol = (SWORD)ERISA_ESCAPE_CODE;

    for (int i = 0; i < ERISA_SUB_SORT_MAX; i++) {
        prob->acsSubModel[i].wOccured = 0;
        prob->acsSubModel[i].wSymbol = (SWORD)-1;
    }
}

static void EPM_HalfOccuredCount(ERISA_PROB_MODEL* prob) {
    UDWORD i;
    
    prob->dwTotalCount = 0;
    for (i = 0; i < prob->dwSymbolSorts; i++) {
        prob->dwTotalCount += prob->acsSymTable[i].wOccured = ((prob->acsSymTable[i].wOccured + 1) >> 1);
    }
    for (i = 0; i < ERISA_SUB_SORT_MAX; i++) {
        prob->acsSubModel[i].wOccured >>= 1;
    }
}

static int EPM_IncreaseSymbol(ERISA_PROB_MODEL* prob, int index) {
    WORD wOccured = ++prob->acsSymTable[index].wOccured;
    SWORD wSymbol = prob->acsSymTable[index].wSymbol;

    while (--index >= 0) {
        if (prob->acsSymTable[index].wOccured >= wOccured)
            break;
        prob->acsSymTable[index + 1] = prob->acsSymTable[index];
    }
    prob->acsSymTable[++index].wOccured = wOccured;
    prob->acsSymTable[index].wSymbol = wSymbol;

    if (++prob->dwTotalCount >= ERISA_TOTAL_LIMIT) {
        EPM_HalfOccuredCount(prob);
    }

    return index;
}

/**************************/
/* general decode context */
/**************************/

ULONG MIOContext_DecodeSymbolBytes(MIOContext* context, SBYTE* ptrDst, ULONG nCount) {

    /* (assert) */
    if (context->m_pfnDecodeSymbolBytes == NULL)
        return 0;

    return (context->m_pfnDecodeSymbolBytes)(context, ptrDst, nCount);
}


static const SBYTE /*BYTE*/ nGammaCodeLookup[512] = {
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, 2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, 2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, 2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4, 4,  4,
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  8,  6,  8,  6,  8,  6,  8,  6,  16, 8,  -1, -1, 17, 8,  -1, -1, 9,  6,  9,  6,  9,  6,  9, 6,  18,
    8,  -1, -1, 19, 8,  -1, -1, 5,  4,  5,  4,  5,  4,  5,  4,  5,  4,  5,  4,  5,  4,  5,  4,  5,  4,  5,  4,  5,  4,  5,  4,  5,  4,  5,  4, 5,  4,
    5,  4,  10, 6,  10, 6,  10, 6,  10, 6,  20, 8,  -1, -1, 21, 8,  -1, -1, 11, 6,  11, 6,  11, 6,  11, 6,  22, 8,  -1, -1, 23, 8,  -1, -1, 3, 2,  3,
    2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2, 3,  2,
    3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  3, 2,  3,
    2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2, 3,  2,
    3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  6,  4,  6,  4,  6,  4,  6,  4,  6,  4,  6,  4,  6,  4,  6,  4,  6,  4,  6,  4,  6, 4,  6,
    4,  6,  4,  6,  4,  6,  4,  6,  4,  12, 6,  12, 6,  12, 6,  12, 6,  24, 8,  -1, -1, 25, 8,  -1, -1, 13, 6,  13, 6,  13, 6,  13, 6,  26, 8, -1, -1,
    27, 8,  -1, -1, 7,  4,  7,  4,  7,  4,  7,  4,  7,  4,  7,  4,  7,  4,  7,  4,  7,  4,  7,  4,  7,  4,  7,  4,  7,  4,  7,  4,  7,  4,  7, 4,  14,
    6,  14, 6,  14, 6,  14, 6,  28, 8,  -1, -1, 29, 8,  -1, -1, 15, 6,  15, 6,  15, 6,  15, 6,  30, 8,  -1, -1, 31, 8,  -1, -1
};


static ESLError MIOContext_PrefetchBuffer(MIOContext* context) {
    if (context->m_nIntBufCount != 0)
        return eslErrSuccess;

    if (context->m_pFilePos >= context->m_pFileLength)
        return eslErrGeneral;

    // read next int32 BE
    int dwLeft = context->m_pFileLength - context->m_pFilePos;
    if (dwLeft < 4) {
        // read partially at buffer edge
        context->m_dwIntBuffer = 0;
        for (int i = 0; i < dwLeft; i++) {
            context->m_dwIntBuffer |= (UDWORD)context->m_pFileBuf[context->m_pFilePos + i] << (24 - 8*i);
        }
        context->m_pFilePos += dwLeft;
    }
    else {
        context->m_dwIntBuffer = 
            ((UDWORD)context->m_pFileBuf[context->m_pFilePos + 0] << 24) | 
            ((UDWORD)context->m_pFileBuf[context->m_pFilePos + 1] << 16) | 
            ((UDWORD)context->m_pFileBuf[context->m_pFilePos + 2] << 8) |
            ((UDWORD)context->m_pFileBuf[context->m_pFilePos + 3]);
        context->m_pFilePos += 4;  
    }

    // get next int32
    context->m_nIntBufCount = 32;

    return eslErrSuccess;
}

int MIOContext_GetABit(MIOContext* context) {
    if (MIOContext_PrefetchBuffer(context)) {
        return 1; // on error does return 1
    }

    // returns one bit ("0 or -1" according to original comment)
    int nValue = (int)(((SDWORD)context->m_dwIntBuffer) >> 31);
    --context->m_nIntBufCount;
    context->m_dwIntBuffer <<= 1;
    return nValue;
}

UINT MIOContext_GetNBits(MIOContext* context, int n) {
    UINT nCode = 0;
    while (n != 0) {
        if (MIOContext_PrefetchBuffer(context))
            break;

        int nCopyBits = n;
        if (nCopyBits > context->m_nIntBufCount)
            nCopyBits = context->m_nIntBufCount;

        nCode = (nCode << nCopyBits) | (context->m_dwIntBuffer >> (32 - nCopyBits));
        n -= nCopyBits;
        context->m_nIntBufCount -= nCopyBits;
        context->m_dwIntBuffer <<= nCopyBits;
    }
    return nCode;
}

static int MIOContext_GetGammaCode(MIOContext* context) {
    // test 1
    if (MIOContext_PrefetchBuffer(context)) {
        return 0;
    }

    /*register*/ UDWORD dwIntBuf;
    context->m_nIntBufCount--;
    dwIntBuf = context->m_dwIntBuffer;
    context->m_dwIntBuffer <<= 1;
    if (!(dwIntBuf & 0x80000000)) {
        return 1;
    }


    // test end code
    if (MIOContext_PrefetchBuffer(context)) {
        return 0;
    }

    if ((~context->m_dwIntBuffer & 0x55000000) && (context->m_nIntBufCount >= 8)) {
        int i = (context->m_dwIntBuffer >> 24) << 1;
        int nCode = nGammaCodeLookup[i];
        int nBitCount = nGammaCodeLookup[i + 1];
        
        /* (assert) */
        if (nBitCount > context->m_nIntBufCount || nCode <= 0)
            return 0;

        context->m_nIntBufCount -= nBitCount;
        context->m_dwIntBuffer <<= nBitCount;
        return nCode;
    }

    // regular routine
    int nCode = 0, nBase = 2;
    for (;;) {
        if (context->m_nIntBufCount >= 2) {
            // process 2 bits
            dwIntBuf = context->m_dwIntBuffer;
            context->m_dwIntBuffer <<= 2;
            nCode = (nCode << 1) | (dwIntBuf >> 31);
            context->m_nIntBufCount -= 2;
            if (!(dwIntBuf & 0x40000000)) {
                return nCode + nBase;
            }
            nBase <<= 1;
        }
        else {
            // extract 1-bit
            if (MIOContext_PrefetchBuffer(context)) {
                return 0;
            }
            nCode = (nCode << 1) | (context->m_dwIntBuffer >> 31);
            context->m_nIntBufCount--;
            context->m_dwIntBuffer <<= 1;

            // test end code
            if (MIOContext_PrefetchBuffer(context)) {
                return 0;
            }

            dwIntBuf = context->m_dwIntBuffer;
            context->m_nIntBufCount--;
            context->m_dwIntBuffer <<= 1;
            if (!(dwIntBuf & 0x80000000)) {
                return nCode + nBase;
            }
            nBase <<= 1;
        }
    }
}

/* original clones this into 2 functions with different escape codes but here we use a flag */
static int MIOContext_GetHuffmanCommon(MIOContext* context, ERINA_HUFFMAN_TREE* tree, int escapeGamma) {
    // get one huffman code
    int nCode;
    if (tree->m_iEscape != ERINA_HUFFMAN_NULL) {
        int iEntry = ERINA_HUFFMAN_ROOT;
        int iChild = tree->m_hnTree[ERINA_HUFFMAN_ROOT].child_code;

        // decode codes
        do {
            if (MIOContext_PrefetchBuffer(context)) {
                return ERINA_HUFFMAN_ESCAPE;
            }

            // extract 1-bit
            iEntry = iChild + (context->m_dwIntBuffer >> 31);
            --context->m_nIntBufCount;
            iChild = tree->m_hnTree[iEntry].child_code;
            context->m_dwIntBuffer <<= 1;
        } while (!(iChild & ERINA_CODE_FLAG));

        // increase code occurence
        if ((context->m_dwERINAFlags != ERINAEncodingFlag_efERINAOrder0) || (tree->m_hnTree[ERINA_HUFFMAN_ROOT].weight < ERINA_HUFFMAN_MAX - 1)) {
            EHT_IncreaseOccuredCount(tree, iEntry);
        }

        // regular code
        nCode = iChild & ~ERINA_CODE_FLAG;
        if (nCode != ERINA_HUFFMAN_ESCAPE) {
            return nCode;
        }
    }

    if (escapeGamma) {
        // escape code: gamma code
        nCode = MIOContext_GetGammaCode(context);
        if (nCode == -1) {
            return ERINA_HUFFMAN_ESCAPE;
        }
    }
    else {
        // escape code: 8-bit code
        nCode = MIOContext_GetNBits(context, 8);
    }
    EHT_AddNewEntry(tree, nCode);

    return nCode;
}

// get one "regular huffman" code
static int MIOContext_GetHuffmanCode(MIOContext* context, ERINA_HUFFMAN_TREE* tree) {
    return MIOContext_GetHuffmanCommon(context, tree, 0);
}

// get one "length huffman" code
static int MIOContext_GetHuffmanLength(MIOContext* context, ERINA_HUFFMAN_TREE* tree) {
    return MIOContext_GetHuffmanCommon(context, tree, 1);
}

//////////////////////////////////////////////////////////////////////////////

/* decode ERINA (huffman-coded) */
ULONG MIOContext_DecodeERINACodeBytes(MIOContext* context, SBYTE* ptrDst, ULONG nCount) {
    ERINA_HUFFMAN_TREE* tree = context->m_pLastHuffmanTree;
    int symbol, length;
    ULONG i = 0;
    if (context->m_nLength > 0) {
        length = context->m_nLength;
        if (length > (int)nCount) {
            length = nCount;
        }
        context->m_nLength -= length;
        do {
            ptrDst[i++] = 0;
        } while (--length);
    }
    while (i < nCount) {
        symbol = MIOContext_GetHuffmanCode(context, tree);
        if (symbol == ERINA_HUFFMAN_ESCAPE) {
            break;
        }
        ptrDst[i++] = (SBYTE)symbol;

        if (symbol == 0) {
            length = MIOContext_GetHuffmanLength(context, context->m_ppHuffmanTree[0x100]);
            if (length == ERINA_HUFFMAN_ESCAPE) {
                break;
            }
            if (--length) {
                context->m_nLength = length;
                if (i + length > nCount) {
                    length = nCount - i;
                }
                context->m_nLength -= length;
                if (length > 0) {
                    do {
                        ptrDst[i++] = 0;
                    } while (--length);
                }
            }
        }
        tree = context->m_ppHuffmanTree[symbol & 0xFF];
    }
    context->m_pLastHuffmanTree = tree;

    return i;
}

static int MIOContext_DecodeERISACodeIndex(MIOContext* context, ERISA_PROB_MODEL* pModel) {
    // serach via index
    UDWORD dwAcc = context->m_dwCodeRegister * pModel->dwTotalCount / context->m_dwAugendRegister;
    if (dwAcc >= ERISA_TOTAL_LIMIT) {
        return MIO_ERISA_SYMBOL_ERROR;
    }

    int iSym = 0;
    WORD wAcc = (WORD)dwAcc;
    WORD wFs = 0;
    WORD wOccured;
    for (;;) {
        wOccured = pModel->acsSymTable[iSym].wOccured;
        if (wAcc < wOccured) {
            break;
        }
        wAcc -= wOccured;
        wFs += wOccured;
        if ((UDWORD)++iSym >= pModel->dwSymbolSorts) {
            return MIO_ERISA_SYMBOL_ERROR;
        }
    }

    // update code and augend registers
    context->m_dwCodeRegister -= (context->m_dwAugendRegister * wFs + pModel->dwTotalCount - 1) / pModel->dwTotalCount;
    context->m_dwAugendRegister = context->m_dwAugendRegister * wOccured / pModel->dwTotalCount;

    /* (assert) */
    if (context->m_dwAugendRegister == 0)
        return MIO_ERISA_SYMBOL_ERROR;

    // normalize augend register and load into code register
    while (!(context->m_dwAugendRegister & 0x8000)) {
        int nNextBit = MIOContext_GetABit(context);
        if (nNextBit == 1) {
            if ((++context->m_nPostBitCount) >= 256) {
                return MIO_ERISA_SYMBOL_ERROR;
            }
            nNextBit = 0;
        }
        context->m_dwCodeRegister = (context->m_dwCodeRegister << 1) | (nNextBit & 0x01);

        context->m_dwAugendRegister <<= 1;
    }


    /* (assert) */
    if ((context->m_dwAugendRegister & 0x8000) == 0)
        return MIO_ERISA_SYMBOL_ERROR;

    context->m_dwCodeRegister &= 0xFFFF;

    return iSym;
}

/* decode ERISA (arithmetic-coded, using designated statistical model) */
int MIOContext_DecodeERISACode(MIOContext* context, ERISA_PROB_MODEL* pModel) {
    int iSym = MIOContext_DecodeERISACodeIndex(context, pModel);
    int nSymbol = ERISA_ESCAPE_CODE;
    if (iSym >= 0) {
        nSymbol = pModel->acsSymTable[iSym].wSymbol;
        EPM_IncreaseSymbol(pModel, iSym);
    }
    return nSymbol;
}

//////////////////////////////////////////////////////////////////////////////

MIOContext* MIOContext_Open() {
    MIOContext* context = malloc(sizeof(MIOContext));
    if (!context) return NULL;

    context->m_nIntBufCount = 0;

    context->m_pfnDecodeSymbolBytes = NULL;

    context->m_ppHuffmanTree = NULL;
    context->m_pPhraseLenProb = NULL;
    context->m_pPhraseIndexProb = NULL;
    context->m_pRunLenProb = NULL;
    context->m_ppTableERISA = NULL;

    return context;
}

void MIOContext_Close(MIOContext* context) {
    if (!context) return;

    free(context->m_ppHuffmanTree);
    free(context->m_pPhraseLenProb);
    free(context->m_pPhraseIndexProb);
    free(context->m_pRunLenProb);
    free(context->m_ppTableERISA);

    free(context);
}

void MIOContext_AttachInputFile(MIOContext* context, uint8_t* pFileBuf, int pFileLength) {
    context->m_pFileBuf = pFileBuf;
    context->m_pFileLength = pFileLength;
    context->m_pFilePos = 0;
}

void MIOContext_FlushBuffer(MIOContext* context) {
    context->m_nIntBufCount = 0;
}

//////////////////////////////////////////////////////////////////////////////

ESLError MIOContext_PrepareToDecodeERINACode(MIOContext* context, UDWORD dwFlags) {
    if (context->m_ppHuffmanTree == NULL) {
        UDWORD dwSize = (sizeof(ERINA_HUFFMAN_TREE*) + sizeof(ERINA_HUFFMAN_TREE)) * 0x101;
        dwSize = (dwSize + 0x0F) & ~0x0F;
        context->m_ppHuffmanTree = (ERINA_HUFFMAN_TREE**)malloc(dwSize);
        if (!context->m_ppHuffmanTree)
            return eslErrGeneral;

        BYTE* ptrBuf = (BYTE*)(context->m_ppHuffmanTree + 0x101);
        for (int i = 0; i < 0x101; i++) {
            void* ptrTmp = ptrBuf;
            context->m_ppHuffmanTree[i] = ptrTmp;
            ptrBuf += sizeof(ERINA_HUFFMAN_TREE);
        }
    }

    // init huffman
    context->m_dwERINAFlags = dwFlags;
    context->m_nLength = 0;
    if (dwFlags == ERINAEncodingFlag_efERINAOrder0) { // not used in audio actually
        EHT_Initialize(context->m_ppHuffmanTree[0]);
        EHT_Initialize(context->m_ppHuffmanTree[0x100]);
        for (int i = 1; i < 0x100; i++) {
            context->m_ppHuffmanTree[i] = context->m_ppHuffmanTree[0];
        }
    }
    else {
        for (int i = 0; i < 0x101; i++) {
            EHT_Initialize(context->m_ppHuffmanTree[i]);
        }
    }

    context->m_pLastHuffmanTree = context->m_ppHuffmanTree[0];

    context->m_pfnDecodeSymbolBytes = &MIOContext_DecodeERINACodeBytes;

    return eslErrSuccess;
}

ESLError MIOContext_PrepareToDecodeERISACode(MIOContext* context) {
    // init memory
    if (context->m_ppTableERISA == NULL) { //TODO improve
        UDWORD dwBytes = sizeof(ERISA_PROB_MODEL*) * 0x104 + sizeof(ERISA_PROB_MODEL) * 0x101;
        dwBytes = (dwBytes + 0x100F) & (~0xFFF);
        context->m_ppTableERISA = (ERISA_PROB_MODEL**)malloc(dwBytes);
        if (!context->m_ppTableERISA)
            return eslErrGeneral;
    }

    if (context->m_pPhraseLenProb == NULL)
        context->m_pPhraseLenProb = malloc(sizeof(ERISA_PROB_MODEL));
    if (context->m_pPhraseIndexProb == NULL)
        context->m_pPhraseIndexProb = malloc(sizeof(ERISA_PROB_MODEL));
    if (context->m_pRunLenProb == NULL)
        context->m_pRunLenProb = malloc(sizeof(ERISA_PROB_MODEL));
    if (!context->m_pPhraseLenProb || !context->m_pPhraseIndexProb || !context->m_pRunLenProb)
        return eslErrGeneral;

    // init probability model
    ERISA_PROB_MODEL* pNextProb = (ERISA_PROB_MODEL*)(context->m_ppTableERISA + 0x104);
    context->m_pLastERISAProb = pNextProb;
    for (int i = 0; i < 0x101; i++) {
        EPM_Initialize(pNextProb);
        context->m_ppTableERISA[i] = pNextProb;
        pNextProb++;
    }
    EPM_Initialize(context->m_pPhraseLenProb);
    EPM_Initialize(context->m_pPhraseIndexProb);
    EPM_Initialize(context->m_pRunLenProb);

    // init register
    context->m_nLength = 0;
    context->m_dwCodeRegister = MIOContext_GetNBits(context, 32);
    context->m_dwAugendRegister = 0xFFFF;
    context->m_nPostBitCount = 0;

    context->m_pfnDecodeSymbolBytes = &MIOContext_DecodeERISACodeBytes;

    return eslErrSuccess;
}

// init "arithtmetic code"
void MIOContext_InitializeERISACode(MIOContext* context) {
    context->m_nLength = 0;
    context->m_dwCodeRegister = MIOContext_GetNBits(context, 32);
    context->m_dwAugendRegister = 0xFFFF;
    context->m_nPostBitCount = 0;
}

ULONG MIOContext_DecodeERISACodeBytes(MIOContext* context, SBYTE* ptrDst, ULONG nCount) {
    ERISA_PROB_MODEL* pProb = context->m_pLastERISAProb;
    int nSymbol, iSym;
    int i = 0;

    while ((ULONG)i < nCount) {
        if (context->m_nLength > 0) {
            // zero-length
            ULONG nCurrent = nCount - i;
            if (nCurrent > context->m_nLength)
                nCurrent = context->m_nLength;

            context->m_nLength -= nCurrent;
            for (ULONG j = 0; j < nCurrent; j++) {
                ptrDst[i++] = 0;
            }

            continue;
        }

        // decode next arithmetic code
        iSym = MIOContext_DecodeERISACodeIndex(context, pProb);
        if (iSym < 0)
            break;
        nSymbol = pProb->acsSymTable[iSym].wSymbol;
        EPM_IncreaseSymbol(pProb, iSym);
        ptrDst[i++] = (SBYTE)nSymbol;

        if (nSymbol == 0) {
            // get zero-length
            iSym = MIOContext_DecodeERISACodeIndex(context, context->m_pRunLenProb);
            if (iSym < 0)
                break;
            context->m_nLength = context->m_pRunLenProb->acsSymTable[iSym].wSymbol;
            EPM_IncreaseSymbol(context->m_pRunLenProb, iSym);
        }

        pProb = context->m_ppTableERISA[(nSymbol & 0xFF)];
    }
    context->m_pLastERISAProb = pProb;

    return i;
}

ULONG MIOContext_DecodeERISACodeWords(MIOContext* context, SWORD* ptrDst, ULONG nCount) {
    ERISA_PROB_MODEL* pProb = context->m_pLastERISAProb;
    int nSymbol, iSym;
    int i = 0;

    while ((ULONG)i < nCount) {
        if (context->m_nLength > 0) {
            // zero-length
            ULONG nCurrent = nCount - i;
            if (nCurrent > context->m_nLength)
                nCurrent = context->m_nLength;

            context->m_nLength -= nCurrent;
            for (ULONG j = 0; j < nCurrent; j++) {
                ptrDst[i++] = 0;
            }

            continue;
        }

        // decode next arithmetic code
        iSym = MIOContext_DecodeERISACodeIndex(context, pProb);
        if (iSym < 0)
            break;
        nSymbol = pProb->acsSymTable[iSym].wSymbol;
        EPM_IncreaseSymbol(pProb, iSym);


        if (nSymbol == ERISA_ESCAPE_CODE) {
            iSym = MIOContext_DecodeERISACodeIndex(context, context->m_pPhraseIndexProb);
            if (iSym < 0)
                break;
            nSymbol = context->m_pPhraseIndexProb->acsSymTable[iSym].wSymbol;
            EPM_IncreaseSymbol(context->m_pPhraseIndexProb, iSym);

            iSym = MIOContext_DecodeERISACodeIndex(context, context->m_pPhraseLenProb);
            if (iSym < 0)
                break;
            nSymbol = (nSymbol << 8) | (context->m_pPhraseLenProb->acsSymTable[iSym].wSymbol & 0xFF);
            EPM_IncreaseSymbol(context->m_pPhraseLenProb, iSym);

            ptrDst[i++] = (SWORD)nSymbol;
            pProb = context->m_ppTableERISA[0x100];
        }
        else {
            ptrDst[i++] = (SWORD)(SBYTE)nSymbol;
            pProb = context->m_ppTableERISA[(nSymbol & 0xFF)];

            if (nSymbol == 0) {
                // get zero-length
                iSym = MIOContext_DecodeERISACodeIndex(context, context->m_pRunLenProb);
                if (iSym < 0)
                    break;
                context->m_nLength = context->m_pRunLenProb->acsSymTable[iSym].wSymbol;
                EPM_IncreaseSymbol(context->m_pRunLenProb, iSym);
            }
        }
    }

    context->m_pLastERISAProb = pProb;

    return i;
}
