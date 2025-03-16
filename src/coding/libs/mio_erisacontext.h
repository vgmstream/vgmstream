/* Handles bitreading from blocks and code unpacking (huffman/arithmetical decoding).
 */
#ifndef _MIO_ERISA_CONTEXT_H_
#define _MIO_ERISA_CONTEXT_H_

/* tree for huffman coding ("ERINA" codes) */

#define ERINA_CODE_FLAG 0x80000000u
#define ERINA_HUFFMAN_ESCAPE 0x7FFFFFFF
#define ERINA_HUFFMAN_NULL 0x8000u
#define ERINA_HUFFMAN_MAX 0x4000
#define ERINA_HUFFMAN_ROOT 0x200

#define MIO_HUFFMAN_SYMBOLS 256

typedef struct {
    uint16_t weight;
    uint16_t parent;
    uint32_t child_code;
} ERINA_HUFFMAN_NODE;

typedef struct {
    ERINA_HUFFMAN_NODE m_hnTree[0x201];
    int m_iSymLookup[MIO_HUFFMAN_SYMBOLS];
    int m_iEscape;
    int m_iTreePointer;
} ERINA_HUFFMAN_TREE;


/* probability model for arithmetic coding ("ERISA" codes) */

#define ERISA_TOTAL_LIMIT 0x2000  // parameter limit
#define ERISA_SYMBOL_SORTS 0x101  // max types
#define ERISA_SUB_SORT_MAX 0x80
#define ERISA_PROB_SLOT_MAX 0x800  // max slots for probability model
#define ERISA_ESCAPE_CODE (-1)

typedef struct {
    WORD wOccured; // symbol occurence count
    SWORD wSymbol; // symbol (lower 8-bit only
} ERISA_CODE_SYMBOL;

typedef struct {
    UDWORD dwTotalCount;                                // param < 2000H
    UDWORD dwSymbolSorts;                               // number of symbol types
    ERISA_CODE_SYMBOL acsSymTable[ERISA_SYMBOL_SORTS];  // probability model
    ERISA_CODE_SYMBOL acsSubModel[ERISA_SUB_SORT_MAX];  // sub-probability model index
} ERISA_PROB_MODEL;

typedef enum { 
    ERINAEncodingFlag_efERINAOrder0 = 0x0000, 
    ERINAEncodingFlag_efERINAOrder1 = 0x0001
}  ERINAEncodingFlag;


/* coding context for the decoder (AKA ERISADecodeContext), used to extract codes from a bitstream buffer */

typedef struct MIOContext MIOContext;
struct MIOContext {
    // bitstream buffer
    int m_nIntBufCount;      // helper input buffer bitpos
    UDWORD m_dwIntBuffer;     // helper input buffer

    uint8_t* m_pFileBuf;
    int m_pFileLength;
    int m_pFilePos;

    // current symbol expansion
    ULONG (*m_pfnDecodeSymbolBytes)(MIOContext* context, SBYTE* ptrDst, ULONG nCount);

    // for run-length encoding
    ULONG m_nLength;

    // ERINA (huffman) code context
    UDWORD m_dwERINAFlags;
    ERINA_HUFFMAN_TREE* m_pLastHuffmanTree;
    ERINA_HUFFMAN_TREE** m_ppHuffmanTree;

    // ERISA (arithmetic) code context
    UDWORD m_dwCodeRegister;    // 16-bit
    UDWORD m_dwAugendRegister;  // 16-bit
    int m_nPostBitCount;        // end bit buffer counter

    // ERISA-N context
    ERISA_PROB_MODEL* m_pPhraseLenProb;
    ERISA_PROB_MODEL* m_pPhraseIndexProb;
    ERISA_PROB_MODEL* m_pRunLenProb;
    ERISA_PROB_MODEL* m_pLastERISAProb;
    ERISA_PROB_MODEL** m_ppTableERISA;
};

MIOContext* MIOContext_Open();
void MIOContext_Close(MIOContext* context);

void MIOContext_AttachInputFile(MIOContext* context, uint8_t* pFileBuf, int pFileLength);

int MIOContext_GetABit(MIOContext* context);

UINT MIOContext_GetNBits(MIOContext* context, int n);

void MIOContext_FlushBuffer(MIOContext* context);

ULONG MIOContext_DecodeSymbolBytes(MIOContext* context, SBYTE* ptrDst, ULONG nCount);

ESLError MIOContext_PrepareToDecodeERINACode(MIOContext* context, UDWORD dwFlags);

// (indirect call)
ULONG MIOContext_DecodeERINACodeBytes(MIOContext* context, SBYTE* ptrDst, ULONG nCount);

ESLError MIOContext_PrepareToDecodeERISACode(MIOContext* context);

void MIOContext_InitializeERISACode(MIOContext* context);

int MIOContext_DecodeERISACode(MIOContext* context, ERISA_PROB_MODEL* pModel);

//int MIOContext_DecodeERISACodeIndex(MIOContext* context, ERISA_PROB_MODEL* pModel);

// (indirect call)
ULONG MIOContext_DecodeERISACodeBytes(MIOContext* context, SBYTE* ptrDst, ULONG nCount);

ULONG MIOContext_DecodeERISACodeWords(MIOContext* context, SWORD* ptrDst, ULONG nCount);

#endif
