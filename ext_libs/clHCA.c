
//--------------------------------------------------
// インクルード
//--------------------------------------------------
#include "clHCA.h"
//#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#ifdef _MSC_VER
#define inline __inline
#endif

//--------------------------------------------------
// インライン関数
//--------------------------------------------------
#if 0
static inline unsigned short get_le16(unsigned short v_){const unsigned char *v=(const unsigned char *)&v_;unsigned short r=v[1];r<<=8;r|=v[0];return r;}
static inline unsigned short get_be16(unsigned short v_){const unsigned char *v=(const unsigned char *)&v_;unsigned short r=v[0];r<<=8;r|=v[1];return r;}
static inline unsigned int get_be24(unsigned int v_){const unsigned char *v=(const unsigned char *)&v_;unsigned int r=v[0];r<<=8;r|=v[1];r<<=8;r|=v[2];return r;};
static inline unsigned int get_le32(unsigned int v_){const unsigned char *v=(const unsigned char *)&v_;unsigned int r=v[3];r<<=8;r|=v[2];r<<=8;r|=v[1];r<<=8;r|=v[0];return r;}
static inline unsigned int get_be32(unsigned int v_){const unsigned char *v=(const unsigned char *)&v_;unsigned int r=v[0];r<<=8;r|=v[1];r<<=8;r|=v[2];r<<=8;r|=v[3];return r;}
static inline float get_bef32(float v_){union{float f;unsigned int i;}v;v.f=v_;v.i=get_be32(v.i);return v.f;}

static union { unsigned int i; unsigned char c[4]; } g_is_le = {1};
static inline unsigned short swap_u16(unsigned short v){unsigned short r=v&0xFF;r<<=8;v>>=8;r|=v&0xFF;return r;}
static inline unsigned short swap_u32(unsigned int v){unsigned int r=v&0xFF;r<<=8;v>>=8;r|=v&0xFF;r<<=8;v>>=8;r|=v&0xFF;r<<=8;v>>=8;r|=v&0xFF;return r;}
static inline unsigned short ret_le16(unsigned short v){if (g_is_le.c[0]) return v; else return swap_u16(v);}
static inline unsigned short ret_le32(unsigned int v){if (g_is_le.c[0]) return v; else return swap_u32(v);}
#endif

static inline unsigned int ceil2(unsigned int a,unsigned int b){return (b>0)?(a/b+((a%b)?1:0)):0;}

//--------------------------------------------------
// structure definitions
//--------------------------------------------------
typedef struct stHeader{//ファイル情報 (必須)
	unsigned int hca;              // 'HCA'
	unsigned short version;        // バージョン。v1.3とv2.0の存在を確認
	unsigned short dataOffset;     // データオフセット
} stHeader;
typedef struct stFormat{//フォーマット情報 (必須)
	unsigned int fmt;              // 'fmt'
	unsigned int channelCount:8;   // チャンネル数 1〜16
	unsigned int samplingRate:24;  // サンプリングレート 1〜0x7FFFFF
	unsigned int blockCount;       // ブロック数 0以上
	unsigned short r01;            // 先頭の無音部分(ブロック数*0x400+0x80)
	unsigned short r02;            // 末尾の無音部分？計算方法不明(0x226)
} stFormat;
typedef struct stCompress{//圧縮情報 (圧縮情報かデコード情報のどちらか一つが必須)
	unsigned int comp;             // 'comp'
	unsigned short blockSize;      // ブロックサイズ(CBRのときに有効？) 8〜0xFFFF、0のときはVBR
	unsigned char r01;             // 不明(1) 0〜r02      v2.0現在1のみ対応
	unsigned char r02;             // 不明(15) r01〜0x1F  v2.0現在15のみ対応
	unsigned char r03;             // 不明(1)(1)
	unsigned char r04;             // 不明(1)(0)
	unsigned char r05;             // 不明(0x80)(0x80)
	unsigned char r06;             // 不明(0x80)(0x20)
	unsigned char r07;             // 不明(0)(0x20)
	unsigned char r08;             // 不明(0)(8)
	unsigned char reserve1;        // 予約
	unsigned char reserve2;        // 予約
} stCompress;
typedef struct stDecode{//デコード情報 (圧縮情報かデコード情報のどちらか一つが必須)
	unsigned int dec;              // 'dec'
	unsigned short blockSize;      // ブロックサイズ(CBRのときに有効？) 8〜0xFFFF、0のときはVBR
	unsigned char r01;             // 不明(1) 0〜r02      v2.0現在1のみ対応
	unsigned char r02;             // 不明(15) r01〜0x1F  v2.0現在15のみ対応
	unsigned char count1;          // type0とtype1の数-1
	unsigned char count2;          // type2の数-1
	unsigned char r03:4;           // 不明(0)
	unsigned char r04:4;           // 不明(0) 0は1に修正される
	unsigned char enableCount2;    // count2を使うフラグ
} stDecode;
typedef struct stVBR{//可変ビットレート情報 (廃止？)
	unsigned int vbr;              // 'vbr'
	unsigned short r01;            // 不明 0〜0x1FF
	unsigned short r02;            // 不明
} stVBR;
typedef struct stATH{//ATHテーブル情報 (v2.0から廃止？)
	unsigned int ath;              // 'ath'
	unsigned short type;           // テーブルの種類(0:全て0 1:テーブル1)
} stATH;
typedef struct stLoop{//ループ情報
	unsigned int loop;             // 'loop'
	unsigned int loopStart;        // ループ開始ブロックインデックス 0〜loopEnd
	unsigned int loopEnd;          // ループ終了ブロックインデックス loopStart〜(stFormat::blockCount-1)
	unsigned short r01;            // 不明(0x80)ループフラグ？ループ回数？
	unsigned short r02;            // 不明(0x226)
} stLoop;
typedef struct stCipher{//暗号テーブル情報
	unsigned int ciph;             // 'ciph'
	unsigned short type;           // 暗号化の種類(0:暗号化なし 1:鍵なし暗号化 0x38:鍵あり暗号化)
} stCipher;
typedef struct stRVA{//相対ボリューム調節情報
	unsigned int rva;              // 'rva'
	float volume;                  // ボリューム
} stRVA;
typedef struct stComment{//コメント情報
	unsigned int comm;             // 'comm'
	unsigned char len;             // コメントの長さ？
	//char comment[];
} stComment;
typedef struct stPadding{//パディング
	unsigned int pad;              // 'pad'
} stPadding;
typedef struct clATH{
	unsigned char _table[0x80];
} clATH;
typedef struct clCipher{
	unsigned char _table[0x100];
} clCipher;
typedef struct stChannel{
	float block[0x80];
	float base[0x80];
	char value[0x80];
	char scale[0x80];
	char value2[8];
	int type;
	char *value3;
	unsigned int count;
	float wav1[0x80];
	float wav2[0x80];
	float wav3[0x80];
	float wave[8][0x80];
} stChannel;
typedef struct clHCA{
	unsigned int _validFile;
	unsigned int _version;
	unsigned int _dataOffset;
	unsigned int _channelCount;
	unsigned int _samplingRate;
	unsigned int _blockCount;
	unsigned int _fmt_r01;
	unsigned int _fmt_r02;
	unsigned int _blockSize;
	unsigned int _comp_r01;
	unsigned int _comp_r02;
	unsigned int _comp_r03;
	unsigned int _comp_r04;
	unsigned int _comp_r05;
	unsigned int _comp_r06;
	unsigned int _comp_r07;
	unsigned int _comp_r08;
	unsigned int _comp_r09;
	unsigned int _vbr_r01;
	unsigned int _vbr_r02;
	unsigned int _ath_type;
	unsigned int _loopStart;
	unsigned int _loopEnd;
	unsigned int _loop_r01;
	unsigned int _loop_r02;
	unsigned int _loopFlg;
	unsigned int _ciph_type;
	unsigned int _ciph_key1;
	unsigned int _ciph_key2;
	float _rva_volume;
	unsigned int _comm_len;
	char *_comm_comment;
	clATH _ath;
	clCipher _cipher;
	stChannel _channel[0x10];
} clHCA;
typedef struct clData{
	const unsigned char *_data;
	int _size;
	int _bit;
} clData; 

//--------------------------------------------------
// コンストラクタ
//--------------------------------------------------
static void clATH_constructor(clATH *ath);
static void clCipher_constructor(clCipher *cipher);

static void clHCA_constructor(clHCA *hca,unsigned int ciphKey1,unsigned int ciphKey2){
	memset(hca,0,sizeof(*hca));
	hca->_ciph_key1 = ciphKey1;
	hca->_ciph_key2 = ciphKey2;
	clATH_constructor(&hca->_ath);
	clCipher_constructor(&hca->_cipher);
	hca->_validFile = 0;
	hca->_comm_comment = 0;
}

static void clHCA_destructor(clHCA *hca)
{
	if(hca->_comm_comment){free(hca->_comm_comment);hca->_comm_comment=0;}
}

//--------------------------------------------------
// HCAチェック
//--------------------------------------------------
#if 0
static int clHCA_CheckFile(void *data,unsigned int size){
	return (data&&size>=4&&(get_be32(*(unsigned int *)data)&0x7F7F7F7F)==0x48434100);/*'HCA\0'*/
}
#endif

//--------------------------------------------------
// チェックサム
//--------------------------------------------------
static const unsigned short clHCA_CheckSum_v[]={
	0x0000,0x8005,0x800F,0x000A,0x801B,0x001E,0x0014,0x8011,0x8033,0x0036,0x003C,0x8039,0x0028,0x802D,0x8027,0x0022,
	0x8063,0x0066,0x006C,0x8069,0x0078,0x807D,0x8077,0x0072,0x0050,0x8055,0x805F,0x005A,0x804B,0x004E,0x0044,0x8041,
	0x80C3,0x00C6,0x00CC,0x80C9,0x00D8,0x80DD,0x80D7,0x00D2,0x00F0,0x80F5,0x80FF,0x00FA,0x80EB,0x00EE,0x00E4,0x80E1,
	0x00A0,0x80A5,0x80AF,0x00AA,0x80BB,0x00BE,0x00B4,0x80B1,0x8093,0x0096,0x009C,0x8099,0x0088,0x808D,0x8087,0x0082,
	0x8183,0x0186,0x018C,0x8189,0x0198,0x819D,0x8197,0x0192,0x01B0,0x81B5,0x81BF,0x01BA,0x81AB,0x01AE,0x01A4,0x81A1,
	0x01E0,0x81E5,0x81EF,0x01EA,0x81FB,0x01FE,0x01F4,0x81F1,0x81D3,0x01D6,0x01DC,0x81D9,0x01C8,0x81CD,0x81C7,0x01C2,
	0x0140,0x8145,0x814F,0x014A,0x815B,0x015E,0x0154,0x8151,0x8173,0x0176,0x017C,0x8179,0x0168,0x816D,0x8167,0x0162,
	0x8123,0x0126,0x012C,0x8129,0x0138,0x813D,0x8137,0x0132,0x0110,0x8115,0x811F,0x011A,0x810B,0x010E,0x0104,0x8101,
	0x8303,0x0306,0x030C,0x8309,0x0318,0x831D,0x8317,0x0312,0x0330,0x8335,0x833F,0x033A,0x832B,0x032E,0x0324,0x8321,
	0x0360,0x8365,0x836F,0x036A,0x837B,0x037E,0x0374,0x8371,0x8353,0x0356,0x035C,0x8359,0x0348,0x834D,0x8347,0x0342,
	0x03C0,0x83C5,0x83CF,0x03CA,0x83DB,0x03DE,0x03D4,0x83D1,0x83F3,0x03F6,0x03FC,0x83F9,0x03E8,0x83ED,0x83E7,0x03E2,
	0x83A3,0x03A6,0x03AC,0x83A9,0x03B8,0x83BD,0x83B7,0x03B2,0x0390,0x8395,0x839F,0x039A,0x838B,0x038E,0x0384,0x8381,
	0x0280,0x8285,0x828F,0x028A,0x829B,0x029E,0x0294,0x8291,0x82B3,0x02B6,0x02BC,0x82B9,0x02A8,0x82AD,0x82A7,0x02A2,
	0x82E3,0x02E6,0x02EC,0x82E9,0x02F8,0x82FD,0x82F7,0x02F2,0x02D0,0x82D5,0x82DF,0x02DA,0x82CB,0x02CE,0x02C4,0x82C1,
	0x8243,0x0246,0x024C,0x8249,0x0258,0x825D,0x8257,0x0252,0x0270,0x8275,0x827F,0x027A,0x826B,0x026E,0x0264,0x8261,
	0x0220,0x8225,0x822F,0x022A,0x823B,0x023E,0x0234,0x8231,0x8213,0x0216,0x021C,0x8219,0x0208,0x820D,0x8207,0x0202,
};

static unsigned short clHCA_CheckSum(const void *data,int size,unsigned short sum){
	const unsigned char *s, *e;
	for(s=(const unsigned char *)data,e=s+size;s<e;s++)sum=(sum<<8)^clHCA_CheckSum_v[(sum>>8)^*s];
	return sum;
}

//--------------------------------------------------
// データ
//--------------------------------------------------
void clData_constructor(clData *ds,const void *data,int size){
	ds->_data=(const unsigned char *)data;
	ds->_size=size*8;
	ds->_bit=0;
}

static unsigned int clData_CheckBit(clData *ds,int bitSize){
	unsigned int v=0;
	if(ds->_bit+bitSize<=ds->_size){
		unsigned int bitOffset=bitSize+(ds->_bit&7);
		if((ds->_size-ds->_bit)>=32&&bitOffset>=25){
			static const int mask[]={0xFFFFFFFF,0x7FFFFFFF,0x3FFFFFFF,0x1FFFFFFF,0x0FFFFFFF,0x07FFFFFF,0x03FFFFFF,0x01FFFFFF};
			const unsigned int _bit=ds->_bit;
			const unsigned char *data=&ds->_data[_bit>>3];
			v=data[0];v=(v<<8)|data[1];v=(v<<8)|data[2];v=(v<<8)|data[3];
			v&=mask[_bit&7];
			v>>=32-(_bit&7)-bitSize;
		}
		else if((ds->_size-ds->_bit)>=24&&bitOffset>=17){
			static const int mask[]={0xFFFFFF,0x7FFFFF,0x3FFFFF,0x1FFFFF,0x0FFFFF,0x07FFFF,0x03FFFF,0x01FFFF};
			const unsigned int _bit=ds->_bit;
			const unsigned char *data=&ds->_data[_bit>>3];
			v=data[0];v=(v<<8)|data[1];v=(v<<8)|data[2];
			v&=mask[_bit&7];
			v>>=24-(_bit&7)-bitSize;
		}
		else if((ds->_size-ds->_bit)>=16&&bitOffset>=9){
			static const int mask[]={0xFFFF,0x7FFF,0x3FFF,0x1FFF,0x0FFF,0x07FF,0x03FF,0x01FF};
			const unsigned int _bit=ds->_bit;
			const unsigned char *data=&ds->_data[_bit>>3];
			v=data[0];v=(v<<8)|data[1];
			v&=mask[_bit&7];
			v>>=16-(_bit&7)-bitSize;
		}
		else{
		  static const int mask[]={0xFF,0x7F,0x3F,0x1F,0x0F,0x07,0x03,0x01};
			const unsigned int _bit=ds->_bit;
			const unsigned char *data=&ds->_data[_bit>>3];
			v=data[0];
			v&=mask[_bit&7];
			v>>=8-(_bit&7)-bitSize;
		}
	}
	return v;
}

static unsigned int clData_GetBit(clData *ds,int bitSize){
	unsigned int v=clData_CheckBit(ds,bitSize);
	ds->_bit+=bitSize;
	return v;
}

static void clData_AddBit(clData *ds,int bitSize){
	ds->_bit+=bitSize;
}

//--------------------------------------------------
// ヘッダ情報をコンソール出力
//--------------------------------------------------
#if 0
static int clHCA_PrintInfo(const char *filenameHCA){
	FILE *fp;
	stHeader header;
	unsigned char *data;
	unsigned int size;
	clData d;

	// temporaries, so we don't need a state structure
	unsigned int _version;
	unsigned int _dataOffset;
	unsigned int _channelCount;
	unsigned int _samplingRate;
	unsigned int _blockCount;
	unsigned int _fmt_r01;
	unsigned int _fmt_r02;
	unsigned int _blockSize;
	unsigned int _comp_r01;
	unsigned int _comp_r02;
	unsigned int _comp_r03;
	unsigned int _comp_r04;
	unsigned int _comp_r05;
	unsigned int _comp_r06;
	unsigned int _comp_r07;
	unsigned int _comp_r08;
	unsigned int _comp_r09;
	unsigned int _vbr_r01;
	unsigned int _vbr_r02;
	unsigned int _ath_type;
	unsigned int _loopStart;
	unsigned int _loopEnd;
	unsigned int _loop_r01;
	unsigned int _loop_r02;
	unsigned int _loopFlg;
	unsigned int _ciph_type;
	unsigned int _ciph_key1;
	unsigned int _ciph_key2;
	float _rva_volume;
	unsigned int _comm_len;
	char *_comm_comment;

	// チェック
	if(!(filenameHCA))return -1;

	// HCAファイルを開く
	if((fp = fopen(filenameHCA,"rb")) == NULL){
		printf("Error: ファイルが開けませんでした。\n");
		return -1;
	}

	// ヘッダチェック
	memset(&header,0,sizeof(header));
	fread(&header,sizeof(header),1,fp);
	if(!clHCA_CheckFile(&header,sizeof(header))){
		printf("Error: HCAファイルではありません。\n");
		fclose(fp);return -1;
	}

	// ヘッダ解析
	clData_constructor(&d, &header, sizeof(header));
	clData_AddBit(&d, 32);
	header.dataOffset=clData_CheckBit(&d, 16);
	data=(unsigned char*) malloc(header.dataOffset);
	if(!data){
		printf("Error: メモリ不足です。\n");
		fclose(fp);return -1;
	}
	fseek(fp,0,SEEK_SET);
	fread(data,header.dataOffset,1,fp);

	size=header.dataOffset;
	
	clData_constructor(&d, data, size);

	// サイズチェック
	if(size<sizeof(stHeader)){
		printf("Error: ヘッダのサイズが小さすぎます。\n");
		free(data);fclose(fp);return -1;
	}

	// HCA
	if(size>=sizeof(stHeader) && (clData_CheckBit(&d,32)&0x7F7F7F7F)==0x48434100){/*'HCA\0'*/
		clData_AddBit(&d,32);
		_version=clData_GetBit(&d,16);
		_dataOffset=clData_GetBit(&d,16);
		printf("コーデック: HCA\n");
		printf("バージョン: %d.%d\n",_version>>8,_version&0xFF);
		//if(size<_dataOffset)return -1;
		if(clHCA_CheckSum(data,_dataOffset,0))printf("※ ヘッダが破損しています。改変してる場合もこの警告が出ます。\n");
		size-=(16+16+32)/8;
	}else{
		printf("※ HCAチャンクがありません。再生に必要な情報です。\n");
	}

	// fmt
	if(size>=sizeof(stFormat) && (clData_CheckBit(&d, 32)&0x7F7F7F7F)==0x666D7400){/*'fmt\0'*/
		clData_AddBit(&d,32);
		_channelCount=clData_GetBit(&d,8);
		_samplingRate=clData_GetBit(&d,24);
		_blockCount=clData_GetBit(&d,32);
		_fmt_r01=clData_GetBit(&d,16);
		_fmt_r02=clData_GetBit(&d,16);
		switch(_channelCount){
		case 1:printf("チャンネル数: モノラル (1チャンネル)\n");break;
		case 2:printf("チャンネル数: ステレオ (2チャンネル)\n");break;
		default:printf("チャンネル数: %dチャンネル\n",_channelCount);break;
		}
		if(!(_channelCount>=1&&_channelCount<=16)){
			printf("※ チャンネル数の範囲は1〜16です。\n");
		}
		printf("サンプリングレート: %dHz\n",_samplingRate);
		if(!(_samplingRate>=1&&_samplingRate<=0x7FFFFF)){
			printf("※ サンプリングレートの範囲は1〜8388607(0x7FFFFF)です。\n");
		}
		printf("ブロック数: %d\n",_blockCount);
		printf("先頭無音ブロック数: %d\n",(_fmt_r01-0x80)/0x400);
		printf("末尾無音サンプル数？: %d\n",_fmt_r02);
		size-=(16+16+32+24+8+32)/8;
	}else{
		printf("※ fmtチャンクがありません。再生に必要な情報です。\n");
	}

	// comp
	if(size>=sizeof(stCompress) && (clData_CheckBit(&d, 32)&0x7F7F7F7F)==0x636F6D70){/*'comp'*/
		clData_AddBit(&d,32);
		_blockSize=clData_GetBit(&d,16);
		_comp_r01=clData_GetBit(&d,8);
		_comp_r02=clData_GetBit(&d,8);
		_comp_r03=clData_GetBit(&d,8);
		_comp_r04=clData_GetBit(&d,8);
		_comp_r05=clData_GetBit(&d,8);
		_comp_r06=clData_GetBit(&d,8);
		_comp_r07=clData_GetBit(&d,8);
		_comp_r08=clData_GetBit(&d,8);
		clData_AddBit(&d,8+8);
		printf("ビットレート: CBR (固定ビットレート)\n");
		printf("ブロックサイズ: 0x%04X\n",_blockSize);
		if(!(_blockSize>=8&&_blockSize<=0xFFFF)){
			printf("※ ブロックサイズの範囲は8〜65535(0xFFFF)です。v1.3では0でVBRになるようになってましたが、v2.0から廃止されたようです。\n");
		}
		printf("comp1: %d\n",_comp_r01);
		printf("comp2: %d\n",_comp_r02);
		if(!(_comp_r01>=0&&_comp_r01<=_comp_r02&&_comp_r02<=0x1F)){
			printf("※ comp1とcomp2の範囲は0<=comp1<=comp2<=31です。v2.0現在、comp1は1、comp2は15で固定されています。\n");
		}
		printf("comp3: %d\n",_comp_r03);
		if(!_comp_r03){
			printf("※ comp3は1以上の値です。\n");
		}
		printf("comp4: %d\n",_comp_r04);
		printf("comp5: %d\n",_comp_r05);
		printf("comp6: %d\n",_comp_r06);
		printf("comp7: %d\n",_comp_r07);
		printf("comp8: %d\n",_comp_r08);
		size-=(32+16+8*8)/8;
	}

	// dec
	else if(size>=sizeof(stDecode) && (clData_CheckBit(&d, 32)&0x7F7F7F7F)==0x64656300){/*'dec\0'*/
		unsigned char count1,count2,enableCount2;
		clData_AddBit(&d, 32);
		_blockSize=clData_GetBit(&d,16);
		_comp_r01=clData_GetBit(&d,8);
		_comp_r02=clData_GetBit(&d,8);
		count1=clData_GetBit(&d,8);
		count2=clData_GetBit(&d,8);
		_comp_r03=clData_GetBit(&d,4);
		_comp_r04=clData_GetBit(&d,4);
		enableCount2=clData_GetBit(&d,8);
		_comp_r05=count1+1;
		_comp_r06=((enableCount2)?count2:count1)+1;
		_comp_r07=_comp_r05-_comp_r06;
		_comp_r08=0;
		printf("ビットレート: CBR (固定ビットレート)\n");
		printf("ブロックサイズ: 0x%04X\n",_blockSize);
		if(!(_blockSize>=8&&_blockSize<=0xFFFF)){
			printf("※ ブロックサイズの範囲は8〜65535(0xFFFF)です。v1.3では0でVBRになるようになってましたが、v2.0から廃止されたようです。\n");
		}
		printf("dec1: %d\n",_comp_r01);
		printf("dec2: %d\n",_comp_r02);
		if(!(_comp_r01>=0&&_comp_r01<=_comp_r02&&_comp_r02<=0x1F)){
			printf("※ dec1とdec2の範囲は0<=dec1<=dec2<=31です。v2.0現在、dec1は1、dec2は15で固定されています。\n");
		}
		printf("dec3: %d\n",_comp_r03);
		if(!_comp_r03){
			printf("※ dec3は再生時に1以上の値に修正されます。\n");
		}
		printf("dec4: %d\n",_comp_r04);
		printf("dec5: %d\n",_comp_r05);
		printf("dec6: %d\n",_comp_r06);
		printf("dec7: %d\n",_comp_r07);
		size-=(8+4+4+8+8+8+8+16+32)/8;
	}else{
		printf("※ compチャンクまたはdecチャンクがありません。再生に必要な情報です。\n");
	}

	// vbr
	if(size>=sizeof(stVBR) && (clData_CheckBit(&d,32)&0x7F7F7F7F)==0x76627200){/*'vbr\0'*/
		clData_AddBit(&d, 32);
		_vbr_r01=clData_GetBit(&d,16);
		_vbr_r02=clData_GetBit(&d,16);
		printf("ビットレート: VBR (可変ビットレート) ※v2.0で廃止されています。\n");
		if(!(_blockSize==0)){
			printf("※ compまたはdecチャンクですでにCBRが指定されています。\n");
		}
		printf("vbr1: %d\n",_vbr_r01);
		if(!(_vbr_r01>=0&&_vbr_r01<=0x1FF)){
			printf("※ vbr1の範囲は0〜511(0x1FF)です。\n");
		}
		printf("vbr2: %d\n",_vbr_r02);
		size-=(16+16+32)/8;
	}else{
		_vbr_r01=0;
		_vbr_r02=0;
	}

	// ath
	if(size>=6 && (clData_CheckBit(&d,32)&0x7F7F7F7F)==0x61746800){/*'ath\0'*/
		clData_AddBit(&d,32);
		_ath_type=clData_GetBit(&d,16);
		printf("ATHタイプ:%d ※v2.0から廃止されています。\n",_ath_type);
		size-=(16+32)/8;
	}else{
		if(_version<0x200){
			printf("ATHタイプ:1 ※v2.0から廃止されています。\n");
		}
	}

	// loop
	if(size>=sizeof(stLoop) && (clData_CheckBit(&d,32)&0x7F7F7F7F)==0x6C6F6F70){/*'loop'*/
		clData_AddBit(&d, 32);
		_loopStart=clData_GetBit(&d,32);
		_loopEnd=clData_GetBit(&d,32);
		_loop_r01=clData_GetBit(&d,16);
		_loop_r02=clData_GetBit(&d,16);
		printf("ループ開始ブロック: %d\n",_loopStart);
		printf("ループ終了ブロック: %d\n",_loopEnd);
		if(!(_loopStart>=0&&_loopStart<=_loopEnd&&_loopEnd<_blockCount)){
			printf("※ ループ開始ブロックとループ終了ブロックの範囲は、0<=ループ開始ブロック<=ループ終了ブロック<ブロック数 です。\n");
		}
		printf("ループ情報1: %d\n",_loop_r01);
		printf("ループ情報2: %d\n",_loop_r02);
		size-=(16+16+32+32+32)/8;
	}

	// ciph
	if(size>=6 && (clData_CheckBit(&d, 32)&0x7F7F7F7F)==0x63697068){/*'ciph'*/
		clData_AddBit(&d,32);
		_ciph_type=clData_GetBit(&d,16);
		switch(_ciph_type){
		case 0:printf("暗号化タイプ: なし\n");break;
		case 1:printf("暗号化タイプ: 鍵無し暗号化\n");break;
		case 0x38:printf("暗号化タイプ: 鍵有り暗号化 ※正しい鍵を使わないと出力波形がおかしくなります。\n");break;
		default:printf("暗号化タイプ: %d\n",_ciph_type);break;
		}
		if(!(_ciph_type==0||_ciph_type==1||_ciph_type==0x38)){
			printf("※ この暗号化タイプは、v2.0現在再生できません。\n");
		}
		size-=6;
	}

	// rva
	if(size>=sizeof(stRVA) && (clData_CheckBit(&d,32)&0x7F7F7F7F)==0x72766100){/*'rva\0'*/
		union { unsigned int i; float f; } v;
		clData_AddBit(&d,32);
		v.i=clData_GetBit(&d,32);
		_rva_volume=v.f;
		printf("相対ボリューム調節: %g倍\n",_rva_volume);
		size-=(32+32)/8;
	}

	// comm
	if(size>=5 && (clData_CheckBit(&d,32)&0x7F7F7F7F)==0x636F6D6D){/*'comm'*/
		int i;
		clData_AddBit(&d,32);
		_comm_len=clData_GetBit(&d,8);
		_comm_comment=malloc(_comm_len+1);
		for(i=0;i<_comm_len;++i)_comm_comment[i]=clData_GetBit(&d,8);
		_comm_comment[i]='\0';
		printf("コメント: %s\n",_comm_comment);
		free(_comm_comment);
	}

	free(data);

	// 閉じる
	fclose(fp);

	return 0;
}
#endif

//--------------------------------------------------
// デコードしてWAVEファイルに保存
//--------------------------------------------------
#if 0
static int clHCA_DecodeToWavefile_Decode(void *fp1,void *fp2,unsigned int address,unsigned int count,void *data,void *modeFunction);

static void clHCA_DecodeToWavefile_DecodeModeFloat(float f,void *fp){fwrite(&f,sizeof(f),1,(FILE *)fp);}
static void clHCA_DecodeToWavefile_DecodeMode8bit(float f,void *fp){int v=(int)(f*0x7F)+0x80;fwrite(&v,1,1,(FILE *)fp);}
static void clHCA_DecodeToWavefile_DecodeMode16bit(float f,void *fp){int v=(int)(f*0x7FFF);fwrite(&v,2,1,(FILE *)fp);}
static void clHCA_DecodeToWavefile_DecodeMode24bit(float f,void *fp){int v=(int)(f*0x7FFFFF);fwrite(&v,3,1,(FILE *)fp);}
static void clHCA_DecodeToWavefile_DecodeMode32bit(float f,void *fp){int v=(int)(f*0x7FFFFFFF);fwrite(&v,4,1,(FILE *)fp);}

static int clHCA_DecodeToWavefile(clHCA *hca,const char *filenameHCA,const char *filenameWAV,float volume,int mode,int loop){
	FILE *fp1;
	FILE *fp2;
	stHeader header;
	unsigned char *data1;
	unsigned char *data2;
	struct stWAVEHeader{
		char riff[4];
		unsigned int riffSize;
		char wave[4];
		char fmt[4];
		unsigned int fmtSize;
		unsigned short fmtType;
		unsigned short fmtChannelCount;
		unsigned int fmtSamplingRate;
		unsigned int fmtSamplesPerSec;
		unsigned short fmtSamplingSize;
		unsigned short fmtBitCount;
	}wavRiff={'R','I','F','F',0,'W','A','V','E','f','m','t',' ',ret_le32(0x10),0,0,0,0,0,0};
	struct stWAVEsmpl{
		char smpl[4];
		unsigned int smplSize;
		unsigned int manufacturer;
		unsigned int product;
		unsigned int samplePeriod;
		unsigned int MIDIUnityNote;
		unsigned int MIDIPitchFraction;
		unsigned int SMPTEFormat;
		unsigned int SMPTEOffset;
		unsigned int sampleLoops;
		unsigned int samplerData;
		unsigned int loop_Identifier;
		unsigned int loop_Type;
		unsigned int loop_Start;
		unsigned int loop_End;
		unsigned int loop_Fraction;
		unsigned int loop_PlayCount;
	}wavSmpl={'s','m','p','l',ret_le32(0x3C),0,0,0,ret_le32(0x3C),0,0,0,ret_le32(1),ret_le32(0x18),0,0,0,0,0,0};
	struct stWAVEnote{
		char note[4];
		unsigned int noteSize;
		unsigned int dwName;
	}wavNote={'n','o','t','e',0,0};
	struct stWAVEdata{
		char data[4];
		unsigned int dataSize;
	}wavData={'d','a','t','a',0};

	// チェック
	if(!(filenameHCA&&filenameWAV&&(mode==0||mode==8||mode==16||mode==24||mode==32)&&loop>=0))return -1;

	// HCAファイルを開く
	if((fp1 = fopen(filenameHCA,"rb")) == NULL)return -1;

	// ヘッダチェック
	memset(&header,0,sizeof(header));
	fread(&header,sizeof(header),1,fp1);
	if(!CheckFile(&header,sizeof(header))){fclose(fp1);return -1;}

	// ヘッダ解析
	header.dataOffset=get_be16(header.dataOffset);
	data1=(unsigned char*) malloc(header.dataOffset);
	if(!data1){fclose(fp1);return -1;}
	fseek(fp1,0,SEEK_SET);
	fread(data1,header.dataOffset,1,fp1);
	if(clHCA_Decode(cl,data1,header.dataOffset,0) < 0){free(data1);fclose(fp1);return -1;}

	// WAVEファイルを開く
	if((fp2 = fopen(filenameWAV,"wb")) == NULL){free(data1);fclose(fp1);return -1;}

	// WAVEヘッダを書き込み
	wavRiff.fmtType=ret_le16((mode>0)?1:3);
	wavRiff.fmtChannelCount=ret_le16(hca->_channelCount);
	wavRiff.fmtBitCount=ret_le16((mode>0)?mode:32);
	wavRiff.fmtSamplingRate=ret_le32(hca->_samplingRate);
	wavRiff.fmtSamplingSize=ret_le16(wavRiff.fmtBitCount/8*wavRiff.fmtChannelCount);
	wavRiff.fmtSamplesPerSec=ret_le32(wavRiff.fmtSamplingRate*wavRiff.fmtSamplingSize);
	if(hca->_loopFlg){
		wavSmpl.samplePeriod=ret_le32((unsigned int)(1/(double)wavRiff.fmtSamplingRate*1000000000));
		wavSmpl.loop_Start=ret_le32(hca->_loopStart*0x80*8*wavRiff.fmtSamplingSize);
		wavSmpl.loop_End=ret_le32(hca->_loopEnd*0x80*8*wavRiff.fmtSamplingSize);
		wavSmpl.loop_PlayCount=ret_le32((hca->_loop_r01==0x80)?0:hca->_loop_r01);
	}else if(loop){
		wavSmpl.loop_Start=0;
		wavSmpl.loop_End=ret_le32(hca->_blockCount*0x80*8*wavRiff.fmtSamplingSize);
		hca->_loopStart=0;
		hca->_loopEnd=hca->_blockCount;
	}
	if(_comm_comment){
		unsigned int noteSize=4+hca->_comm_len+1;
		if(noteSize&3)noteSize+=4-(noteSize&3);
		waveNote.noteSize=ret_le32(noteSize);
	}
	wavData.dataSize=ret_le32(hca->_blockCount*0x80*8*wavRiff.fmtSamplingSize+(wavSmpl.loop_End-wavSmpl.loop_Start)*loop);
	wavRiff.riffSize=ret_le32(0x1C+((hca->_loopFlg&&!loop)?sizeof(wavSmpl):0)+(hca->_comm_comment?8+wavNote.noteSize:0)+sizeof(wavData)+wavData.dataSize);
	fwrite(&wavRiff,sizeof(wavRiff),1,fp2);
	if(hca->_loopFlg&&!loop)fwrite(&wavSmpl,sizeof(wavSmpl),1,fp2);
	if(hca->_comm_comment){
		int address=ftell(fp2);
		fwrite(&wavNote,sizeof(wavNote),1,fp2);
		fputs(hca->_comm_comment,fp2);
		fseek(fp2,address+8+wavNote.noteSize,SEEK_SET);
	}
	fwrite(&wavData,sizeof(wavData),1,fp2);

	// 相対ボリュームを調節
	hca->_rva_volume*=volume;

	// デコード
	void *modeFunction;
	switch(mode){
	case 0:modeFunction=(void *)clHCA_DecodeToWavefile_DecodeModeFloat;break;
	case 8:modeFunction=(void *)clHCA_DecodeToWavefile_DecodeMode8bit;break;
	case 16:modeFunction=(void *)clHCA_DecodeToWavefile_DecodeMode16bit;break;
	case 24:modeFunction=(void *)clHCA_DecodeToWavefile_DecodeMode24bit;break;
	case 32:modeFunction=(void *)clHCA_DecodeToWavefile_DecodeMode32bit;break;
	}
	data2=(unsigned char*) malloc(_blockSize);
	if(!data2){free(data1);fclose(fp2);fclose(fp1);return -1;}
	if(!loop){
		if(clHCA_DecodeToWavefile_Decode(cl,fp1,fp2,_dataOffset,_blockCount,data2,modeFunction) < 0){free(data2);free(data1);fclose(fp2);fclose(fp1);return -1;}
	}else{
		unsigned int loopBlockOffset=hca->_dataOffset+hca->_loopStart*hca->_blockSize;
		unsigned int loopBlockCount=hca->_loopEnd-hca->_loopStart;
		int i;
		if(clHCA_DecodeToWavefile_Decode(cl,fp1,fp2,_dataOffset,_loopEnd,data2,modeFunction) < 0){free(data2);free(data1);fclose(fp2);fclose(fp1);return -1;}
		for(i=1;i<loop;i++){
			if(clHCA_DecodeToWavefile_Decode(cl,fp1,fp2,loopBlockOffset,loopBlockCount,data2,modeFunction) < 0){free(data2);free(data1);fclose(fp2);fclose(fp1);return -1;}
		}
		if(clHCA_DecodeToWavefile_Decode(cl,fp1,fp2,loopBlockOffset,_blockCount-_loopStart,data2,modeFunction) < 0){free(data2);free(data1);fclose(fp2);fclose(fp1);return -1;}
	}
	free(data2);
	free(data1);

	// 閉じる
	fclose(fp2);
	fclose(fp1);

	return 0;
}
int clHCA_DecodeToWavefile_Decode(clHCA *hca,void *fp1,void *fp2,unsigned int address,unsigned int count,void *data,void *modeFunction){
	float f;
	unsigned int l, k, m;
	int i, j;
	fseek((FILE *)fp1,address,SEEK_SET);
	for(l=0;l<count;l++,address+=_blockSize){
		fread(data,hca->_blockSize,1,(FILE *)fp1);
		if(clHCA_Decode(cl,data,hca->_blockSize,address) < 0)return -1;
		for(i=0;i<8;i++){
			for(j=0;j<0x80;j++){
				for(k=0,m=hca->_channelCount;k<m;k++){
					f=_channel[k].wave[i][j]*hca->_rva_volume;
					if(f>1){f=1;}else if(f<-1){f=-1;}
					((void (*)(float,void *))modeFunction)(f,fp2);
				}
			}
		}
	}
	return 0;
}
#endif

//--------------------------------------------------
// Decoder utilities
//--------------------------------------------------

int clHCA_isOurFile0(const void *data){
	clData d;
	clData_constructor(&d, data, 8);
	if((clData_CheckBit(&d,32)&0x7F7F7F7F)==0x48434100){/*'HCA\0'*/
		clData_AddBit(&d,32+16);
		return clData_CheckBit(&d,16);
	}
	return -1;
}

int clHCA_isOurFile1(const void *data, unsigned int size){
	int minsize;
	if (size<8)return -1;
	minsize = clHCA_isOurFile0(data);
	if (minsize < 0 || (unsigned int)minsize > size)return -1;
	if (clHCA_CheckSum(data, minsize, 0))return -1;
	return 0;
}

int clHCA_getInfo(clHCA *hca, clHCA_stInfo *info){
	if (!hca->_validFile)return -1;
	info->version=hca->_version;
	info->dataOffset=hca->_dataOffset;
	info->samplingRate=hca->_samplingRate;
	info->channelCount=hca->_channelCount;
	info->blockSize=hca->_blockSize;
	info->blockCount=hca->_blockCount;
	info->loopEnabled=hca->_loopFlg;
	info->loopStart=hca->_loopStart;
	info->loopEnd=hca->_loopEnd;
	info->comment=hca->_comm_comment;
	return 0;
}

void clHCA_DecodeSamples16(clHCA *hca,signed short *samples){
	const float scale = 32768.0f;
	float f;
	signed int s;
	int i, j;
	unsigned int k, l;
	//const float _rva_volume=hca->_rva_volume;
	for(i=0;i<8;i++){
		for(j=0;j<0x80;j++){
			for(k=0,l=hca->_channelCount;k<l;k++){
				f=hca->_channel[k].wave[i][j]/**_rva_volume*/;
				if(f>1){f=1;}else if(f<-1){f=-1;}
				s=(signed int)(f*scale);
				if ((unsigned)(s+0x8000)&0xFFFF0000)s=(s>>31)^0x7FFF;
				*samples++=(signed short)s;
			}
		}
	}
}

//--------------------------------------------------
// Allocation and creation
//--------------------------------------------------

int clHCA_sizeof(){
	return sizeof(clHCA);
}

void clHCA_clear(clHCA *hca,unsigned int ciphKey1,unsigned int ciphKey2){
	clHCA_constructor(hca,ciphKey1,ciphKey2);
}

void clHCA_done(clHCA *hca)
{
	clHCA_destructor(hca);
}

clHCA * clHCA_new(unsigned int ciphKey1, unsigned int ciphKey2){
	clHCA *hca = (clHCA *) malloc(clHCA_sizeof());
	if (hca){
		clHCA_constructor(hca,ciphKey1,ciphKey2);
	}
	return hca;
}

void clHCA_delete(clHCA *hca){
	if (hca){
		clHCA_destructor(hca);
		free(hca);
	}
}

//--------------------------------------------------
// ATH
//--------------------------------------------------
static void clATH_Init0(clATH *ath);
static void clATH_Init1(clATH *ath,unsigned int key);

void clATH_constructor(clATH *ath){
	memset(ath,0,sizeof(*ath));
	clATH_Init0(ath);
}

static int clATH_Init(clATH *ath,int type,unsigned int key){
	switch(type){
	case 0:clATH_Init0(ath);break;
	case 1:clATH_Init1(ath,key);break;
	default:return -1;
	}
	return 0;
}

static const unsigned char * clATH_GetTable(clATH *ath){
	return ath->_table;
}

void clATH_Init0(clATH *ath){
	memset(ath->_table,0,sizeof(ath->_table));
}

static const unsigned char clATH_list[]={
	0x78,0x5F,0x56,0x51,0x4E,0x4C,0x4B,0x49,0x48,0x48,0x47,0x46,0x46,0x45,0x45,0x45,
	0x44,0x44,0x44,0x44,0x43,0x43,0x43,0x43,0x43,0x43,0x42,0x42,0x42,0x42,0x42,0x42,
	0x42,0x42,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x40,0x40,0x40,0x40,
	0x40,0x40,0x40,0x40,0x40,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,
	0x3F,0x3F,0x3F,0x3E,0x3E,0x3E,0x3E,0x3E,0x3E,0x3D,0x3D,0x3D,0x3D,0x3D,0x3D,0x3D,
	0x3C,0x3C,0x3C,0x3C,0x3C,0x3C,0x3C,0x3C,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,
	0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,
	0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3C,0x3C,0x3C,0x3C,0x3C,0x3C,0x3C,0x3C,
	0x3D,0x3D,0x3D,0x3D,0x3D,0x3D,0x3D,0x3D,0x3E,0x3E,0x3E,0x3E,0x3E,0x3E,0x3E,0x3F,
	0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,
	0x3F,0x3F,0x3F,0x3F,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,
	0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x41,0x41,0x41,0x41,0x41,0x41,0x41,
	0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,
	0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x42,
	0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x43,0x43,0x43,
	0x43,0x43,0x43,0x43,0x43,0x43,0x43,0x43,0x43,0x43,0x43,0x43,0x43,0x43,0x44,0x44,
	0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x45,0x45,0x45,0x45,
	0x45,0x45,0x45,0x45,0x45,0x45,0x45,0x45,0x46,0x46,0x46,0x46,0x46,0x46,0x46,0x46,
	0x46,0x46,0x47,0x47,0x47,0x47,0x47,0x47,0x47,0x47,0x47,0x47,0x48,0x48,0x48,0x48,
	0x48,0x48,0x48,0x48,0x49,0x49,0x49,0x49,0x49,0x49,0x49,0x49,0x4A,0x4A,0x4A,0x4A,
	0x4A,0x4A,0x4A,0x4A,0x4B,0x4B,0x4B,0x4B,0x4B,0x4B,0x4B,0x4C,0x4C,0x4C,0x4C,0x4C,
	0x4C,0x4D,0x4D,0x4D,0x4D,0x4D,0x4D,0x4E,0x4E,0x4E,0x4E,0x4E,0x4E,0x4F,0x4F,0x4F,
	0x4F,0x4F,0x4F,0x50,0x50,0x50,0x50,0x50,0x51,0x51,0x51,0x51,0x51,0x52,0x52,0x52,
	0x52,0x52,0x53,0x53,0x53,0x53,0x54,0x54,0x54,0x54,0x54,0x55,0x55,0x55,0x55,0x56,
	0x56,0x56,0x56,0x57,0x57,0x57,0x57,0x57,0x58,0x58,0x58,0x59,0x59,0x59,0x59,0x5A,
	0x5A,0x5A,0x5A,0x5B,0x5B,0x5B,0x5B,0x5C,0x5C,0x5C,0x5D,0x5D,0x5D,0x5D,0x5E,0x5E,
	0x5E,0x5F,0x5F,0x5F,0x60,0x60,0x60,0x61,0x61,0x61,0x61,0x62,0x62,0x62,0x63,0x63,
	0x63,0x64,0x64,0x64,0x65,0x65,0x66,0x66,0x66,0x67,0x67,0x67,0x68,0x68,0x68,0x69,
	0x69,0x6A,0x6A,0x6A,0x6B,0x6B,0x6B,0x6C,0x6C,0x6D,0x6D,0x6D,0x6E,0x6E,0x6F,0x6F,
	0x70,0x70,0x70,0x71,0x71,0x72,0x72,0x73,0x73,0x73,0x74,0x74,0x75,0x75,0x76,0x76,
	0x77,0x77,0x78,0x78,0x78,0x79,0x79,0x7A,0x7A,0x7B,0x7B,0x7C,0x7C,0x7D,0x7D,0x7E,
	0x7E,0x7F,0x7F,0x80,0x80,0x81,0x81,0x82,0x83,0x83,0x84,0x84,0x85,0x85,0x86,0x86,
	0x87,0x88,0x88,0x89,0x89,0x8A,0x8A,0x8B,0x8C,0x8C,0x8D,0x8D,0x8E,0x8F,0x8F,0x90,
	0x90,0x91,0x92,0x92,0x93,0x94,0x94,0x95,0x95,0x96,0x97,0x97,0x98,0x99,0x99,0x9A,
	0x9B,0x9B,0x9C,0x9D,0x9D,0x9E,0x9F,0xA0,0xA0,0xA1,0xA2,0xA2,0xA3,0xA4,0xA5,0xA5,
	0xA6,0xA7,0xA7,0xA8,0xA9,0xAA,0xAA,0xAB,0xAC,0xAD,0xAE,0xAE,0xAF,0xB0,0xB1,0xB1,
	0xB2,0xB3,0xB4,0xB5,0xB6,0xB6,0xB7,0xB8,0xB9,0xBA,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF,
	0xC0,0xC1,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xC9,0xCA,0xCB,0xCC,0xCD,
	0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,
	0xDE,0xDF,0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xED,0xEE,
	0xEF,0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFF,0xFF,
};

void clATH_Init1(clATH *ath, unsigned int key){
	unsigned int i, v;
	unsigned char *_table=ath->_table;
	for(i=0,v=0;i<0x80;i++,v+=key){
		unsigned int index=v>>13;
		if(index>=0x28E){
			memset(&_table[i],0xFF,0x80-i);
			break;
		}
		_table[i]=clATH_list[index];
	}
}

//--------------------------------------------------
// 暗号化テーブル
//--------------------------------------------------
static void clCipher_Init0(clCipher *cipher);
static void clCipher_Init1(clCipher *cipher);
static void clCipher_Init56(clCipher *cipher,unsigned int key1,unsigned int key2);

void clCipher_constructor(clCipher *cipher){
	memset(cipher,0,sizeof(*cipher));
	clCipher_Init0(cipher);
}

static int clCipher_Init(clCipher *cipher,int type,unsigned int key1,unsigned int key2){
	if(!(key1|key2))type=0;
	switch(type){
	case 0:clCipher_Init0(cipher);break;
	case 1:clCipher_Init1(cipher);break;
	case 56:clCipher_Init56(cipher,key1,key2);break;
	default:return -1;
	}
	return 0;
}

static void clCipher_Mask(clCipher *cipher,void *data,int size){
	unsigned char *_table=cipher->_table;
	unsigned char *d;
	for(d=(unsigned char *)data;size>0;d++,size--)*d=_table[*d];
}

void clCipher_Init0(clCipher *cipher){
	unsigned char *_table=cipher->_table;
	int i;
	for(i=0;i<0x100;i++)_table[i]=i;
}

void clCipher_Init1(clCipher *cipher){
	unsigned char *_table=cipher->_table;
	int i, v;
	for(i=1,v=0;i<0xFF;i++){
		v=(v*13+11)&0xFF;
		if(v==0||v==0xFF)v=(v*13+11)&0xFF;
		_table[i]=v;
	}
	_table[0]=0;
	_table[0xFF]=0xFF;
}

static void clCipher_Init56_CreateTable(unsigned char *r,unsigned char key);

void clCipher_Init56(clCipher *cipher,unsigned int key1,unsigned int key2){
	unsigned char *_table=cipher->_table;

	// テーブル1を生成
	unsigned char t1[8];
	unsigned char t2[0x10];
	unsigned char t3[0x100],t31[0x10],t32[0x10],*t;

	int i, j, v;

	if(!key1)key2--;
	key1--;
	for(i=0;i<7;i++){
		t1[i]=key1;
		key1=(key1>>8)|(key2<<24);
		key2>>=8;
	}

	// テーブル2
	t2[0x00]=t1[1];       t2[0x01]=t1[1]^t1[6];
	t2[0x02]=t1[2]^t1[3]; t2[0x03]=t1[2];
	t2[0x04]=t1[2]^t1[1]; t2[0x05]=t1[3]^t1[4];
	t2[0x06]=t1[3];       t2[0x07]=t1[3]^t1[2];
	t2[0x08]=t1[4]^t1[5]; t2[0x09]=t1[4];
	t2[0x0A]=t1[4]^t1[3]; t2[0x0B]=t1[5]^t1[6];
	t2[0x0C]=t1[5];       t2[0x0D]=t1[5]^t1[4];
	t2[0x0E]=t1[6]^t1[1]; t2[0x0F]=t1[6];

	// テーブル3
	t=t3;
	clCipher_Init56_CreateTable(t31,t1[0]);
	for(i=0;i<0x10;i++){
		unsigned char v;
		clCipher_Init56_CreateTable(t32,t2[i]);
		v=t31[i]<<4;
		for(j=0;j<0x10;j++){
			*(t++)=v|t32[j];
		}
	}

	// CIPHテーブル
	t=&_table[1];
	for(i=0,v=0;i<0x100;i++){
		unsigned char a;
		v=(v+0x11)&0xFF;
		a=t3[v];
		if(a!=0&&a!=0xFF)*(t++)=a;
	}
	_table[0]=0;
	_table[0xFF]=0xFF;

}

void clCipher_Init56_CreateTable(unsigned char *r,unsigned char key){
	int mul=((key&1)<<3)|5;
	int add=(key&0xE)|1;
	int i;
	key>>=4;
	for(i=0;i<0x10;i++){
		key=(key*mul+add)&0xF;
		*(r++)=key;
	}
}

//--------------------------------------------------
// デコード
//--------------------------------------------------
static void stChannel_Decode1(stChannel *ch,clData *data,unsigned int a,int b,const unsigned char *ath);
static void stChannel_Decode2(stChannel *ch,clData *data);
static void stChannel_Decode3(stChannel *ch,unsigned int a,unsigned int b,unsigned int c,unsigned int d);
static void stChannel_Decode4(stChannel *ch,int index,unsigned int a,unsigned int b,unsigned int c);
static void stChannel_Decode5(stChannel *ch,int index);

int clHCA_Decode(clHCA *hca,void *data,unsigned int size,unsigned int address){

	// チェック
	if(!(data))return -1;

	// ヘッダ
	if(address==0){
		char r[0x10];
		unsigned int i, b;
		clData d;

		hca->_validFile = 0;

		clData_constructor(&d, data, size);

		// サイズチェック
		if(size<sizeof(stHeader))return -1;

		// HCA
		if((clData_CheckBit(&d,32)&0x7F7F7F7F)==0x48434100){/*'HCA\0'*/
			clData_AddBit(&d,32);
			hca->_version=clData_GetBit(&d,16);
			hca->_dataOffset=clData_GetBit(&d,16);
			//if(!(_version<=0x200&&_version>0x101))return -1; // バージョンチェック(無効)
			if(size<hca->_dataOffset)return -1;
			//if(clHCA_CheckSum(hca,_dataOffset,0))return -1; // ヘッダの破損チェック(ヘッダ改変を有効にするため破損チェック無効)
			size-=sizeof(stHeader);
		}else{
			return -1;
		}

		// fmt
		if(size>=sizeof(stFormat) && (clData_CheckBit(&d,32)&0x7F7F7F7F)==0x666D7400){/*'fmt\0'*/
			clData_AddBit(&d,32);
			hca->_channelCount=clData_GetBit(&d,8);
			hca->_samplingRate=clData_GetBit(&d,24);
			hca->_blockCount=clData_GetBit(&d,32);
			hca->_fmt_r01=clData_GetBit(&d,16);
			hca->_fmt_r02=clData_GetBit(&d,16);
			if(!(hca->_channelCount>=1&&hca->_channelCount<=16))return -1;
			if(!(hca->_samplingRate>=1&&hca->_samplingRate<=0x7FFFFF))return -1;
			size-=sizeof(stFormat);
		}else{
			return -1;
		}

		// comp
		if(size>=sizeof(stCompress) && (clData_CheckBit(&d,32)&0x7F7F7F7F)==0x636F6D70){/*'comp'*/
			clData_AddBit(&d,32);
			hca->_blockSize=clData_GetBit(&d,16);
			hca->_comp_r01=clData_GetBit(&d,8);
			hca->_comp_r02=clData_GetBit(&d,8);
			hca->_comp_r03=clData_GetBit(&d,8);
			hca->_comp_r04=clData_GetBit(&d,8);
			hca->_comp_r05=clData_GetBit(&d,8);
			hca->_comp_r06=clData_GetBit(&d,8);
			hca->_comp_r07=clData_GetBit(&d,8);
			hca->_comp_r08=clData_GetBit(&d,8);
			clData_AddBit(&d,8+8);
			if(!((hca->_blockSize>=8&&hca->_blockSize<=0xFFFF)||(hca->_blockSize==0)))return -1;
			if(!(/*hca->_comp_r01>=0&&*/hca->_comp_r01<=hca->_comp_r02&&hca->_comp_r02<=0x1F))return -1;
			size-=sizeof(stCompress);
		}

		// dec
		else if(size>=sizeof(stDecode) && (clData_CheckBit(&d,32)&0x7F7F7F7F)==0x64656300){/*'dec\0'*/
			unsigned char count1,count2,enableCount2;
			clData_AddBit(&d,32);
			hca->_blockSize=clData_GetBit(&d,16);
			hca->_comp_r01=clData_GetBit(&d,8);
			hca->_comp_r02=clData_GetBit(&d,8);
			count1=clData_GetBit(&d,8);
			count2=clData_GetBit(&d,8);
			hca->_comp_r03=clData_GetBit(&d,4);
			hca->_comp_r04=clData_GetBit(&d,4);
			enableCount2=clData_GetBit(&d,8);
			hca->_comp_r05=count1+1;
			hca->_comp_r06=((enableCount2)?count2:count1)+1;
			hca->_comp_r07=hca->_comp_r05-hca->_comp_r06;
			hca->_comp_r08=0;
			if(!((hca->_blockSize>=8&&hca->_blockSize<=0xFFFF)||(hca->_blockSize==0)))return -1;
			if(!(/*hca->_comp_r01>=0&&*/hca->_comp_r01<=hca->_comp_r02&&hca->_comp_r02<=0x1F))return -1;
			if(!hca->_comp_r03)hca->_comp_r03=1;
			size-=sizeof(stDecode);
		}else{
			return -1;
		}

		// vbr
		if(size>=sizeof(stVBR) && (clData_CheckBit(&d,32)&0x7F7F7F7F)==0x76627200){/*'vbr\0'*/
			clData_AddBit(&d,32);
			hca->_vbr_r01=clData_GetBit(&d,16);
			hca->_vbr_r02=clData_GetBit(&d,16);
			if(!(hca->_blockSize==0&&/*hca->_vbr_r01>=0&&*/hca->_vbr_r01<=0x1FF))return -1;
		}else{
			hca->_vbr_r01=0;
			hca->_vbr_r02=0;
		}

		// ath
		if(size>=6 && (clData_CheckBit(&d,32)&0x7F7F7F7F)==0x61746800){/*'ath\0'*/
			clData_AddBit(&d,32);
			hca->_ath_type=clData_GetBit(&d,16);
		}else{
			hca->_ath_type=(hca->_version<0x200)?1:0;//v1.3ではデフォルト値が1になってたが、v2.0からATHテーブルが廃止されてるみたいなので0に
		}

		// loop
		if(size>=sizeof(stLoop) && (clData_CheckBit(&d,32)&0x7F7F7F7F)==0x6C6F6F70){/*'loop'*/
			clData_AddBit(&d,32);
			hca->_loopStart=clData_GetBit(&d,32);
			hca->_loopEnd=clData_GetBit(&d,32);
			hca->_loop_r01=clData_GetBit(&d,16);
			hca->_loop_r02=clData_GetBit(&d,16);
			hca->_loopFlg=1;
			if(!(/*hca->_loopStart>=0&&*/hca->_loopStart<=hca->_loopEnd&&hca->_loopEnd<hca->_blockCount))return -1;
			size-=sizeof(stLoop);
		}else{
			hca->_loopStart=0;
			hca->_loopEnd=0;
			hca->_loop_r01=0;
			hca->_loop_r02=0x400;
			hca->_loopFlg=0;
		}

		// ciph
		if(size>=6 && (clData_CheckBit(&d,32)&0x7F7F7F7F)==0x63697068){/*'ciph'*/
			clData_AddBit(&d,32);
			hca->_ciph_type=clData_GetBit(&d,16);
			if(!(hca->_ciph_type==0||hca->_ciph_type==1||hca->_ciph_type==0x38))return -1;
			size-=6;
		}else{
			hca->_ciph_type=0;
		}

		// rva
		if(size>=sizeof(stRVA) && (clData_CheckBit(&d,32)&0x7F7F7F7F)==0x72766100){/*'rva\0'*/
			union { unsigned int i; float f; } v;
			clData_AddBit(&d,32);
			v.i=clData_GetBit(&d,32);
			hca->_rva_volume=v.f;
			size-=sizeof(stRVA);
		}else{
			hca->_rva_volume=1;
		}

		// comm
		if(size>=5 && (clData_CheckBit(&d,32)&0x7F7F7F7F)==0x636F6D6D){/*'comm'*/
			void * newmem;
			unsigned int i;
			clData_AddBit(&d,32);
			hca->_comm_len=clData_GetBit(&d,8);
			if(hca->_comm_len>size)return -1;
			newmem=realloc(hca->_comm_comment,hca->_comm_len+1);
			if(!newmem)return -1;
			hca->_comm_comment=(char *)newmem;
			for(i=0;i<hca->_comm_len;++i)
				((char *)newmem)[i]=clData_GetBit(&d,8);
			((char *)newmem)[i]='\0';
			size-=5+hca->_comm_len;
		}else{
			hca->_comm_len=0;
			hca->_comm_comment=NULL;
		}

		// 初期化
		if(clATH_Init(&hca->_ath,hca->_ath_type,hca->_samplingRate) < 0)return -1;
		if(clCipher_Init(&hca->_cipher,hca->_ciph_type,hca->_ciph_key1,hca->_ciph_key2) < 0)return -1;

		// 値チェック(ヘッダの改変ミスによるエラーを回避するため)
		if(!hca->_comp_r03)hca->_comp_r03=1;//0での除算を防ぐため

		// デコード準備
		memset(hca->_channel,0,sizeof(hca->_channel));

		if(!(hca->_comp_r01==1&&hca->_comp_r02==15))return -1;

		hca->_comp_r09=ceil2(hca->_comp_r05-(hca->_comp_r06+hca->_comp_r07),hca->_comp_r08);

		memset(r,0,sizeof(r));
		b=hca->_channelCount/hca->_comp_r03;
		if(hca->_comp_r07&&b>1){
			char *c=r;
			for(i=0;i<hca->_comp_r03;i++,c+=b){
				switch(b){
				case 2:c[0]=1;c[1]=2;break;
				case 3:c[0]=1;c[1]=2;break;
				case 4:c[0]=1;c[1]=2;if(hca->_comp_r04==0){c[2]=1;c[3]=2;}break;
				case 5:c[0]=1;c[1]=2;if(hca->_comp_r04<=2){c[3]=1;c[4]=2;}break;
				case 6:c[0]=1;c[1]=2;c[4]=1;c[5]=2;break;
				case 7:c[0]=1;c[1]=2;c[4]=1;c[5]=2;break;
				case 8:c[0]=1;c[1]=2;c[4]=1;c[5]=2;c[6]=1;c[7]=2;break;
				}
			}
		}
		for(i=0;i<hca->_channelCount;i++){
			hca->_channel[i].type=r[i];
			hca->_channel[i].value3=&hca->_channel[i].value[hca->_comp_r06+hca->_comp_r07];
			hca->_channel[i].count=hca->_comp_r06+((r[i]!=2)?hca->_comp_r07:0);
		}

		hca->_validFile = 1;
	}

	// ブロックデータ
	else if(address>=hca->_dataOffset){
		clData d;
		int magic;
		unsigned int i, j;
		if(!hca->_validFile)return -1;
		if(size<hca->_blockSize)return -1;
		if(clHCA_CheckSum(data,hca->_blockSize,0))return -1;
		clCipher_Mask(&hca->_cipher,data,hca->_blockSize);
		clData_constructor(&d,data,hca->_blockSize);
		magic=clData_GetBit(&d,16);//0xFFFF固定
		if(magic==0xFFFF){
			const unsigned int _channelCount=hca->_channelCount;
			int a=(clData_GetBit(&d,9)<<8)-clData_GetBit(&d,7);
			for(i=0;i<_channelCount;i++)stChannel_Decode1(&hca->_channel[i],&d,hca->_comp_r09,a,clATH_GetTable(&hca->_ath));
			for(i=0;i<8;i++){
				for(j=0;j<_channelCount;j++)stChannel_Decode2(&hca->_channel[j],&d);
				for(j=0;j<_channelCount;j++)stChannel_Decode3(&hca->_channel[j],hca->_comp_r09,hca->_comp_r08,hca->_comp_r07+hca->_comp_r06,hca->_comp_r05);
				for(j=0;j<_channelCount-1;j++)stChannel_Decode4(&hca->_channel[j],i,hca->_comp_r05-hca->_comp_r06,hca->_comp_r06,hca->_comp_r07);
				for(j=0;j<_channelCount;j++)stChannel_Decode5(&hca->_channel[j],i);
			}
		}
	}

	return 0;
}

//--------------------------------------------------
// デコード第一段階
//   ベースデータの読み込み
//--------------------------------------------------
static const unsigned char stChannel_Decode1_scalelist[]={
	// v2.0
	0x0E,0x0E,0x0E,0x0E,0x0E,0x0E,0x0D,0x0D,
	0x0D,0x0D,0x0D,0x0D,0x0C,0x0C,0x0C,0x0C,
	0x0C,0x0C,0x0B,0x0B,0x0B,0x0B,0x0B,0x0B,
	0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x09,
	0x09,0x09,0x09,0x09,0x09,0x08,0x08,0x08,
	0x08,0x08,0x08,0x07,0x06,0x06,0x05,0x04,
	0x04,0x04,0x03,0x03,0x03,0x02,0x02,0x02,
	0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	// v1.3
	//0x0E,0x0E,0x0E,0x0E,0x0E,0x0E,0x0D,0x0D,
	//0x0D,0x0D,0x0D,0x0D,0x0C,0x0C,0x0C,0x0C,
	//0x0C,0x0C,0x0B,0x0B,0x0B,0x0B,0x0B,0x0B,
	//0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x09,
	//0x09,0x09,0x09,0x09,0x09,0x08,0x08,0x08,
	//0x08,0x08,0x08,0x07,0x06,0x06,0x05,0x04,
	//0x04,0x04,0x03,0x03,0x03,0x02,0x02,0x02,
	//0x02,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
};
static const unsigned int stChannel_Decode1_valueInt[]={
	0x342A8D26,0x34633F89,0x3497657D,0x34C9B9BE,0x35066491,0x353311C4,0x356E9910,0x359EF532,
	0x35D3CCF1,0x360D1ADF,0x363C034A,0x367A83B3,0x36A6E595,0x36DE60F5,0x371426FF,0x3745672A,
	0x37838359,0x37AF3B79,0x37E97C38,0x381B8D3A,0x384F4319,0x388A14D5,0x38B7FBF0,0x38F5257D,
	0x3923520F,0x39599D16,0x3990FA4D,0x39C12C4D,0x3A00B1ED,0x3A2B7A3A,0x3A647B6D,0x3A9837F0,
	0x3ACAD226,0x3B071F62,0x3B340AAF,0x3B6FE4BA,0x3B9FD228,0x3BD4F35B,0x3C0DDF04,0x3C3D08A4,
	0x3C7BDFED,0x3CA7CD94,0x3CDF9613,0x3D14F4F0,0x3D467991,0x3D843A29,0x3DB02F0E,0x3DEAC0C7,
	0x3E1C6573,0x3E506334,0x3E8AD4C6,0x3EB8FBAF,0x3EF67A41,0x3F243516,0x3F5ACB94,0x3F91C3D3,
	0x3FC238D2,0x400164D2,0x402C6897,0x4065B907,0x40990B88,0x40CBEC15,0x4107DB35,0x413504F3,
};
static const unsigned int stChannel_Decode1_scaleInt[]={
	0x00000000,0x3F2AAAAB,0x3ECCCCCD,0x3E924925,0x3E638E39,0x3E3A2E8C,0x3E1D89D9,0x3E088889,
	0x3D842108,0x3D020821,0x3C810204,0x3C008081,0x3B804020,0x3B002008,0x3A801002,0x3A000801,
};
static const float *stChannel_Decode1_valueFloat=(const float *)stChannel_Decode1_valueInt;
static const float *stChannel_Decode1_scaleFloat=(const float *)stChannel_Decode1_scaleInt;

void stChannel_Decode1(stChannel *ch,clData *data,unsigned int a,int b,const unsigned char *ath){
	unsigned int i;
	const unsigned int count=ch->count;
	char *value=ch->value;
	char *value2=ch->value2;
	char *value3=ch->value3;
	char *scale=ch->scale;
	float *base=ch->base;
	int v=clData_GetBit(data,3);
	if(v>=6){
		for(i=0;i<count;i++)value[i]=clData_GetBit(data,6);
	}else if(v){
		int v1=clData_GetBit(data,6),v2=(1<<v)-1,v3=v2>>1,v4;
		value[0]=v1;
		for(i=1;i<count;i++){
			v4=clData_GetBit(data,v);
			if(v4!=v2){v1+=v4-v3;}else{v1=clData_GetBit(data,6);}
			value[i]=v1;
		}
	}else{
		memset(value,0,0x80);
	}
	if(ch->type==2){
		v=clData_CheckBit(data,4);value2[0]=v;
		if(v<15)for(i=0;i<8;i++)value2[i]=clData_GetBit(data,4);
	}else{
		for(i=0;i<a;i++)value3[i]=clData_GetBit(data,6);
	}
	for(i=0;i<count;i++){
		v=value[i];
		if(v){
			v=ath[i]+((b+i)>>8)-((v*5)>>1)+1;
			if(v<0)v=15;
			else if(v>=0x39)v=1;
			else v=stChannel_Decode1_scalelist[v];
		}
		scale[i]=v;
	}
	memset(&scale[count],0,0x80-count);
	for(i=0;i<count;i++)base[i]=stChannel_Decode1_valueFloat[value[i]]*stChannel_Decode1_scaleFloat[scale[i]];
}

//--------------------------------------------------
// デコード第二段階
//   ブロックデータの読み込み
//--------------------------------------------------
static const char stChannel_Decode2_list1[]={
	0,2,3,3,4,4,4,4,5,6,7,8,9,10,11,12,
};
static const char stChannel_Decode2_list2[]={
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,2,2,0,0,0,0,0,0,0,0,0,0,0,0,
	2,2,2,2,2,2,3,3,0,0,0,0,0,0,0,0,
	2,2,3,3,3,3,3,3,0,0,0,0,0,0,0,0,
	3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,4,
	3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,
	3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,
	3,3,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
};
static const float stChannel_Decode2_list3[]={
	+0,+0,+0,+0,+0,+0,+0,+0,+0,+0,+0,+0,+0,+0,+0,+0,
	+0,+0,+1,-1,+0,+0,+0,+0,+0,+0,+0,+0,+0,+0,+0,+0,
	+0,+0,+1,+1,-1,-1,+2,-2,+0,+0,+0,+0,+0,+0,+0,+0,
	+0,+0,+1,-1,+2,-2,+3,-3,+0,+0,+0,+0,+0,+0,+0,+0,
	+0,+0,+1,+1,-1,-1,+2,+2,-2,-2,+3,+3,-3,-3,+4,-4,
	+0,+0,+1,+1,-1,-1,+2,+2,-2,-2,+3,-3,+4,-4,+5,-5,
	+0,+0,+1,+1,-1,-1,+2,-2,+3,-3,+4,-4,+5,-5,+6,-6,
	+0,+0,+1,-1,+2,-2,+3,-3,+4,-4,+5,-5,+6,-6,+7,-7,
};

void stChannel_Decode2(stChannel *ch,clData *data){
	unsigned int i;
	const unsigned int count=ch->count;
	const char *scale=ch->scale;
	const float *base=ch->base;
	float *block=ch->block;
	for(i=0;i<count;i++){
		float f;
		int s=scale[i];
		int bitSize=stChannel_Decode2_list1[s];
		int v=clData_GetBit(data,bitSize);
		if(s<8){
			v+=s<<4;
			clData_AddBit(data,stChannel_Decode2_list2[v]-bitSize);
			f=stChannel_Decode2_list3[v];
		}else{
			v=(1-((v&1)<<1))*(v>>1);
			if(!v)clData_AddBit(data,-1);
			f=(float)v;
		}
		block[i]=base[i]*f;
	}
	memset(&block[count],0,sizeof(float)*(0x80-count));
}

//--------------------------------------------------
// デコード第三段階
//   ブロックデータ修正その１ ※v2.0から追加
//--------------------------------------------------
static const unsigned int stChannel_Decode3_listInt[2][0x40]={
	{
		0x00000000,0x00000000,0x32A0B051,0x32D61B5E,0x330EA43A,0x333E0F68,0x337D3E0C,0x33A8B6D5,
		0x33E0CCDF,0x3415C3FF,0x34478D75,0x3484F1F6,0x34B123F6,0x34EC0719,0x351D3EDA,0x355184DF,
		0x358B95C2,0x35B9FCD2,0x35F7D0DF,0x36251958,0x365BFBB8,0x36928E72,0x36C346CD,0x370218AF,
		0x372D583F,0x3766F85B,0x3799E046,0x37CD078C,0x3808980F,0x38360094,0x38728177,0x38A18FAF,
		0x38D744FD,0x390F6A81,0x393F179A,0x397E9E11,0x39A9A15B,0x39E2055B,0x3A16942D,0x3A48A2D8,
		0x3A85AAC3,0x3AB21A32,0x3AED4F30,0x3B1E196E,0x3B52A81E,0x3B8C57CA,0x3BBAFF5B,0x3BF9295A,
		0x3C25FED7,0x3C5D2D82,0x3C935A2B,0x3CC4563F,0x3D02CD87,0x3D2E4934,0x3D68396A,0x3D9AB62B,
		0x3DCE248C,0x3E0955EE,0x3E36FD92,0x3E73D290,0x3EA27043,0x3ED87039,0x3F1031DC,0x3F40213B,
	},{
		0x3F800000,0x3FAA8D26,0x3FE33F89,0x4017657D,0x4049B9BE,0x40866491,0x40B311C4,0x40EE9910,
		0x411EF532,0x4153CCF1,0x418D1ADF,0x41BC034A,0x41FA83B3,0x4226E595,0x425E60F5,0x429426FF,
		0x42C5672A,0x43038359,0x432F3B79,0x43697C38,0x439B8D3A,0x43CF4319,0x440A14D5,0x4437FBF0,
		0x4475257D,0x44A3520F,0x44D99D16,0x4510FA4D,0x45412C4D,0x4580B1ED,0x45AB7A3A,0x45E47B6D,
		0x461837F0,0x464AD226,0x46871F62,0x46B40AAF,0x46EFE4BA,0x471FD228,0x4754F35B,0x478DDF04,
		0x47BD08A4,0x47FBDFED,0x4827CD94,0x485F9613,0x4894F4F0,0x48C67991,0x49043A29,0x49302F0E,
		0x496AC0C7,0x499C6573,0x49D06334,0x4A0AD4C6,0x4A38FBAF,0x4A767A41,0x4AA43516,0x4ADACB94,
		0x4B11C3D3,0x4B4238D2,0x4B8164D2,0x4BAC6897,0x4BE5B907,0x4C190B88,0x4C4BEC15,0x00000000,
	}
};
static const float *stChannel_Decode3_listFloat=(float *)stChannel_Decode3_listInt[1];

void stChannel_Decode3(stChannel *ch,unsigned int a,unsigned int b,unsigned int c,unsigned int d){
	if(ch->type!=2&&b){
		unsigned int i, j, k, l;
		const char *value=ch->value;
		const char *value3=ch->value3;
		float *block=ch->block;
		for(i=0,k=c,l=c-1;i<a;i++){
			for(j=0;j<b&&k<d;j++,l--){
				block[k++]=stChannel_Decode3_listFloat[value3[i]-value[l]]*block[l];
			}
		}
		block[0x80-1]=0;
	}
}

//--------------------------------------------------
// デコード第四段階
//   ブロックデータ修正その２
//--------------------------------------------------
static const unsigned int stChannel_Decode4_listInt[]={
	// v2.0
	0x40000000,0x3FEDB6DB,0x3FDB6DB7,0x3FC92492,0x3FB6DB6E,0x3FA49249,0x3F924925,0x3F800000,
	0x3F5B6DB7,0x3F36DB6E,0x3F124925,0x3EDB6DB7,0x3E924925,0x3E124925,0x00000000,0x00000000,
	0x00000000,0x32A0B051,0x32D61B5E,0x330EA43A,0x333E0F68,0x337D3E0C,0x33A8B6D5,0x33E0CCDF,
	0x3415C3FF,0x34478D75,0x3484F1F6,0x34B123F6,0x34EC0719,0x351D3EDA,0x355184DF,0x358B95C2,
	0x35B9FCD2,0x35F7D0DF,0x36251958,0x365BFBB8,0x36928E72,0x36C346CD,0x370218AF,0x372D583F,
	0x3766F85B,0x3799E046,0x37CD078C,0x3808980F,0x38360094,0x38728177,0x38A18FAF,0x38D744FD,
	0x390F6A81,0x393F179A,0x397E9E11,0x39A9A15B,0x39E2055B,0x3A16942D,0x3A48A2D8,0x3A85AAC3,
	0x3AB21A32,0x3AED4F30,0x3B1E196E,0x3B52A81E,0x3B8C57CA,0x3BBAFF5B,0x3BF9295A,0x3C25FED7,
	//↓この2行要らない？
	0x3C5D2D82,0x3C935A2B,0x3CC4563F,0x3D02CD87,0x3D2E4934,0x3D68396A,0x3D9AB62B,0x3DCE248C,
	0x3E0955EE,0x3E36FD92,0x3E73D290,0x3EA27043,0x3ED87039,0x3F1031DC,0x3F40213B,0x00000000,
	// v1.3
	//0x40000000,0x3FEDB6DB,0x3FDB6DB7,0x3FC92492,0x3FB6DB6E,0x3FA49249,0x3F924925,0x3F800000,
	//0x3F5B6DB7,0x3F36DB6E,0x3F124925,0x3EDB6DB7,0x3E924925,0x3E124925,0x00000000,0x00000000,
};

void stChannel_Decode4(stChannel *ch,int index,unsigned int a,unsigned int b,unsigned int c){
	if(ch->type==1&&c){
		float f1=((float *)stChannel_Decode4_listInt)[ch[1].value2[index]];
		float f2=f1-2.0f;
		float *s=&ch->block[b];
		float *d=&ch[1].block[b];
		unsigned int i;
		for(i=0;i<a;i++){
			*d=*s*f2;
			*s=*s*f1;
			++d;++s;
		}
	}
}

//--------------------------------------------------
// デコード第五段階
//   波形データを生成
//--------------------------------------------------
static const unsigned int stChannel_Decode5_list1Int[7][0x40]={
	{
		0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,
		0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,
		0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,
		0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,
		0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,
		0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,
		0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,
		0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,
	},{
		0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,
		0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,
		0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,
		0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,
		0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,
		0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,
		0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,
		0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,
	},{
		0x3F7EC46D,0x3F74FA0B,0x3F61C598,0x3F45E403,0x3F7EC46D,0x3F74FA0B,0x3F61C598,0x3F45E403,
		0x3F7EC46D,0x3F74FA0B,0x3F61C598,0x3F45E403,0x3F7EC46D,0x3F74FA0B,0x3F61C598,0x3F45E403,
		0x3F7EC46D,0x3F74FA0B,0x3F61C598,0x3F45E403,0x3F7EC46D,0x3F74FA0B,0x3F61C598,0x3F45E403,
		0x3F7EC46D,0x3F74FA0B,0x3F61C598,0x3F45E403,0x3F7EC46D,0x3F74FA0B,0x3F61C598,0x3F45E403,
		0x3F7EC46D,0x3F74FA0B,0x3F61C598,0x3F45E403,0x3F7EC46D,0x3F74FA0B,0x3F61C598,0x3F45E403,
		0x3F7EC46D,0x3F74FA0B,0x3F61C598,0x3F45E403,0x3F7EC46D,0x3F74FA0B,0x3F61C598,0x3F45E403,
		0x3F7EC46D,0x3F74FA0B,0x3F61C598,0x3F45E403,0x3F7EC46D,0x3F74FA0B,0x3F61C598,0x3F45E403,
		0x3F7EC46D,0x3F74FA0B,0x3F61C598,0x3F45E403,0x3F7EC46D,0x3F74FA0B,0x3F61C598,0x3F45E403,
	},{
		0x3F7FB10F,0x3F7D3AAC,0x3F7853F8,0x3F710908,0x3F676BD8,0x3F5B941A,0x3F4D9F02,0x3F3DAEF9,
		0x3F7FB10F,0x3F7D3AAC,0x3F7853F8,0x3F710908,0x3F676BD8,0x3F5B941A,0x3F4D9F02,0x3F3DAEF9,
		0x3F7FB10F,0x3F7D3AAC,0x3F7853F8,0x3F710908,0x3F676BD8,0x3F5B941A,0x3F4D9F02,0x3F3DAEF9,
		0x3F7FB10F,0x3F7D3AAC,0x3F7853F8,0x3F710908,0x3F676BD8,0x3F5B941A,0x3F4D9F02,0x3F3DAEF9,
		0x3F7FB10F,0x3F7D3AAC,0x3F7853F8,0x3F710908,0x3F676BD8,0x3F5B941A,0x3F4D9F02,0x3F3DAEF9,
		0x3F7FB10F,0x3F7D3AAC,0x3F7853F8,0x3F710908,0x3F676BD8,0x3F5B941A,0x3F4D9F02,0x3F3DAEF9,
		0x3F7FB10F,0x3F7D3AAC,0x3F7853F8,0x3F710908,0x3F676BD8,0x3F5B941A,0x3F4D9F02,0x3F3DAEF9,
		0x3F7FB10F,0x3F7D3AAC,0x3F7853F8,0x3F710908,0x3F676BD8,0x3F5B941A,0x3F4D9F02,0x3F3DAEF9,
	},{
		0x3F7FEC43,0x3F7F4E6D,0x3F7E1324,0x3F7C3B28,0x3F79C79D,0x3F76BA07,0x3F731447,0x3F6ED89E,
		0x3F6A09A7,0x3F64AA59,0x3F5EBE05,0x3F584853,0x3F514D3D,0x3F49D112,0x3F41D870,0x3F396842,
		0x3F7FEC43,0x3F7F4E6D,0x3F7E1324,0x3F7C3B28,0x3F79C79D,0x3F76BA07,0x3F731447,0x3F6ED89E,
		0x3F6A09A7,0x3F64AA59,0x3F5EBE05,0x3F584853,0x3F514D3D,0x3F49D112,0x3F41D870,0x3F396842,
		0x3F7FEC43,0x3F7F4E6D,0x3F7E1324,0x3F7C3B28,0x3F79C79D,0x3F76BA07,0x3F731447,0x3F6ED89E,
		0x3F6A09A7,0x3F64AA59,0x3F5EBE05,0x3F584853,0x3F514D3D,0x3F49D112,0x3F41D870,0x3F396842,
		0x3F7FEC43,0x3F7F4E6D,0x3F7E1324,0x3F7C3B28,0x3F79C79D,0x3F76BA07,0x3F731447,0x3F6ED89E,
		0x3F6A09A7,0x3F64AA59,0x3F5EBE05,0x3F584853,0x3F514D3D,0x3F49D112,0x3F41D870,0x3F396842,
	},{
		0x3F7FFB11,0x3F7FD397,0x3F7F84AB,0x3F7F0E58,0x3F7E70B0,0x3F7DABCC,0x3F7CBFC9,0x3F7BACCD,
		0x3F7A7302,0x3F791298,0x3F778BC5,0x3F75DEC6,0x3F740BDD,0x3F721352,0x3F6FF573,0x3F6DB293,
		0x3F6B4B0C,0x3F68BF3C,0x3F660F88,0x3F633C5A,0x3F604621,0x3F5D2D53,0x3F59F26A,0x3F5695E5,
		0x3F531849,0x3F4F7A1F,0x3F4BBBF8,0x3F47DE65,0x3F43E200,0x3F3FC767,0x3F3B8F3B,0x3F373A23,
		0x3F7FFB11,0x3F7FD397,0x3F7F84AB,0x3F7F0E58,0x3F7E70B0,0x3F7DABCC,0x3F7CBFC9,0x3F7BACCD,
		0x3F7A7302,0x3F791298,0x3F778BC5,0x3F75DEC6,0x3F740BDD,0x3F721352,0x3F6FF573,0x3F6DB293,
		0x3F6B4B0C,0x3F68BF3C,0x3F660F88,0x3F633C5A,0x3F604621,0x3F5D2D53,0x3F59F26A,0x3F5695E5,
		0x3F531849,0x3F4F7A1F,0x3F4BBBF8,0x3F47DE65,0x3F43E200,0x3F3FC767,0x3F3B8F3B,0x3F373A23,
	},{
		0x3F7FFEC4,0x3F7FF4E6,0x3F7FE129,0x3F7FC38F,0x3F7F9C18,0x3F7F6AC7,0x3F7F2F9D,0x3F7EEA9D,
		0x3F7E9BC9,0x3F7E4323,0x3F7DE0B1,0x3F7D7474,0x3F7CFE73,0x3F7C7EB0,0x3F7BF531,0x3F7B61FC,
		0x3F7AC516,0x3F7A1E84,0x3F796E4E,0x3F78B47B,0x3F77F110,0x3F772417,0x3F764D97,0x3F756D97,
		0x3F748422,0x3F73913F,0x3F7294F8,0x3F718F57,0x3F708066,0x3F6F6830,0x3F6E46BE,0x3F6D1C1D,
		0x3F6BE858,0x3F6AAB7B,0x3F696591,0x3F6816A8,0x3F66BECC,0x3F655E0B,0x3F63F473,0x3F628210,
		0x3F6106F2,0x3F5F8327,0x3F5DF6BE,0x3F5C61C7,0x3F5AC450,0x3F591E6A,0x3F577026,0x3F55B993,
		0x3F53FAC3,0x3F5233C6,0x3F5064AF,0x3F4E8D90,0x3F4CAE79,0x3F4AC77F,0x3F48D8B3,0x3F46E22A,
		0x3F44E3F5,0x3F42DE29,0x3F40D0DA,0x3F3EBC1B,0x3F3CA003,0x3F3A7CA4,0x3F385216,0x3F36206C,
	}
};
static const unsigned int stChannel_Decode5_list2Int[7][0x40]={
	{
		0xBD0A8BD4,0x3D0A8BD4,0x3D0A8BD4,0xBD0A8BD4,0x3D0A8BD4,0xBD0A8BD4,0xBD0A8BD4,0x3D0A8BD4,
		0x3D0A8BD4,0xBD0A8BD4,0xBD0A8BD4,0x3D0A8BD4,0xBD0A8BD4,0x3D0A8BD4,0x3D0A8BD4,0xBD0A8BD4,
		0x3D0A8BD4,0xBD0A8BD4,0xBD0A8BD4,0x3D0A8BD4,0xBD0A8BD4,0x3D0A8BD4,0x3D0A8BD4,0xBD0A8BD4,
		0xBD0A8BD4,0x3D0A8BD4,0x3D0A8BD4,0xBD0A8BD4,0x3D0A8BD4,0xBD0A8BD4,0xBD0A8BD4,0x3D0A8BD4,
		0x3D0A8BD4,0xBD0A8BD4,0xBD0A8BD4,0x3D0A8BD4,0xBD0A8BD4,0x3D0A8BD4,0x3D0A8BD4,0xBD0A8BD4,
		0xBD0A8BD4,0x3D0A8BD4,0x3D0A8BD4,0xBD0A8BD4,0x3D0A8BD4,0xBD0A8BD4,0xBD0A8BD4,0x3D0A8BD4,
		0xBD0A8BD4,0x3D0A8BD4,0x3D0A8BD4,0xBD0A8BD4,0x3D0A8BD4,0xBD0A8BD4,0xBD0A8BD4,0x3D0A8BD4,
		0x3D0A8BD4,0xBD0A8BD4,0xBD0A8BD4,0x3D0A8BD4,0xBD0A8BD4,0x3D0A8BD4,0x3D0A8BD4,0xBD0A8BD4,
	},{
		0xBE47C5C2,0xBF0E39DA,0x3E47C5C2,0x3F0E39DA,0x3E47C5C2,0x3F0E39DA,0xBE47C5C2,0xBF0E39DA,
		0x3E47C5C2,0x3F0E39DA,0xBE47C5C2,0xBF0E39DA,0xBE47C5C2,0xBF0E39DA,0x3E47C5C2,0x3F0E39DA,
		0x3E47C5C2,0x3F0E39DA,0xBE47C5C2,0xBF0E39DA,0xBE47C5C2,0xBF0E39DA,0x3E47C5C2,0x3F0E39DA,
		0xBE47C5C2,0xBF0E39DA,0x3E47C5C2,0x3F0E39DA,0x3E47C5C2,0x3F0E39DA,0xBE47C5C2,0xBF0E39DA,
		0x3E47C5C2,0x3F0E39DA,0xBE47C5C2,0xBF0E39DA,0xBE47C5C2,0xBF0E39DA,0x3E47C5C2,0x3F0E39DA,
		0xBE47C5C2,0xBF0E39DA,0x3E47C5C2,0x3F0E39DA,0x3E47C5C2,0x3F0E39DA,0xBE47C5C2,0xBF0E39DA,
		0xBE47C5C2,0xBF0E39DA,0x3E47C5C2,0x3F0E39DA,0x3E47C5C2,0x3F0E39DA,0xBE47C5C2,0xBF0E39DA,
		0x3E47C5C2,0x3F0E39DA,0xBE47C5C2,0xBF0E39DA,0xBE47C5C2,0xBF0E39DA,0x3E47C5C2,0x3F0E39DA,
	},{
		0xBDC8BD36,0xBE94A031,0xBEF15AEA,0xBF226799,0x3DC8BD36,0x3E94A031,0x3EF15AEA,0x3F226799,
		0x3DC8BD36,0x3E94A031,0x3EF15AEA,0x3F226799,0xBDC8BD36,0xBE94A031,0xBEF15AEA,0xBF226799,
		0x3DC8BD36,0x3E94A031,0x3EF15AEA,0x3F226799,0xBDC8BD36,0xBE94A031,0xBEF15AEA,0xBF226799,
		0xBDC8BD36,0xBE94A031,0xBEF15AEA,0xBF226799,0x3DC8BD36,0x3E94A031,0x3EF15AEA,0x3F226799,
		0x3DC8BD36,0x3E94A031,0x3EF15AEA,0x3F226799,0xBDC8BD36,0xBE94A031,0xBEF15AEA,0xBF226799,
		0xBDC8BD36,0xBE94A031,0xBEF15AEA,0xBF226799,0x3DC8BD36,0x3E94A031,0x3EF15AEA,0x3F226799,
		0xBDC8BD36,0xBE94A031,0xBEF15AEA,0xBF226799,0x3DC8BD36,0x3E94A031,0x3EF15AEA,0x3F226799,
		0x3DC8BD36,0x3E94A031,0x3EF15AEA,0x3F226799,0xBDC8BD36,0xBE94A031,0xBEF15AEA,0xBF226799,
	},{
		0xBD48FB30,0xBE164083,0xBE78CFCC,0xBEAC7CD4,0xBEDAE880,0xBF039C3D,0xBF187FC0,0xBF2BEB4A,
		0x3D48FB30,0x3E164083,0x3E78CFCC,0x3EAC7CD4,0x3EDAE880,0x3F039C3D,0x3F187FC0,0x3F2BEB4A,
		0x3D48FB30,0x3E164083,0x3E78CFCC,0x3EAC7CD4,0x3EDAE880,0x3F039C3D,0x3F187FC0,0x3F2BEB4A,
		0xBD48FB30,0xBE164083,0xBE78CFCC,0xBEAC7CD4,0xBEDAE880,0xBF039C3D,0xBF187FC0,0xBF2BEB4A,
		0x3D48FB30,0x3E164083,0x3E78CFCC,0x3EAC7CD4,0x3EDAE880,0x3F039C3D,0x3F187FC0,0x3F2BEB4A,
		0xBD48FB30,0xBE164083,0xBE78CFCC,0xBEAC7CD4,0xBEDAE880,0xBF039C3D,0xBF187FC0,0xBF2BEB4A,
		0xBD48FB30,0xBE164083,0xBE78CFCC,0xBEAC7CD4,0xBEDAE880,0xBF039C3D,0xBF187FC0,0xBF2BEB4A,
		0x3D48FB30,0x3E164083,0x3E78CFCC,0x3EAC7CD4,0x3EDAE880,0x3F039C3D,0x3F187FC0,0x3F2BEB4A,
	},{
		0xBCC90AB0,0xBD96A905,0xBDFAB273,0xBE2F10A2,0xBE605C13,0xBE888E93,0xBEA09AE5,0xBEB8442A,
		0xBECF7BCA,0xBEE63375,0xBEFC5D27,0xBF08F59B,0xBF13682A,0xBF1D7FD1,0xBF273656,0xBF3085BB,
		0x3CC90AB0,0x3D96A905,0x3DFAB273,0x3E2F10A2,0x3E605C13,0x3E888E93,0x3EA09AE5,0x3EB8442A,
		0x3ECF7BCA,0x3EE63375,0x3EFC5D27,0x3F08F59B,0x3F13682A,0x3F1D7FD1,0x3F273656,0x3F3085BB,
		0x3CC90AB0,0x3D96A905,0x3DFAB273,0x3E2F10A2,0x3E605C13,0x3E888E93,0x3EA09AE5,0x3EB8442A,
		0x3ECF7BCA,0x3EE63375,0x3EFC5D27,0x3F08F59B,0x3F13682A,0x3F1D7FD1,0x3F273656,0x3F3085BB,
		0xBCC90AB0,0xBD96A905,0xBDFAB273,0xBE2F10A2,0xBE605C13,0xBE888E93,0xBEA09AE5,0xBEB8442A,
		0xBECF7BCA,0xBEE63375,0xBEFC5D27,0xBF08F59B,0xBF13682A,0xBF1D7FD1,0xBF273656,0xBF3085BB,
	},{
		0xBC490E90,0xBD16C32C,0xBD7B2B74,0xBDAFB680,0xBDE1BC2E,0xBE09CF86,0xBE22ABB6,0xBE3B6ECF,
		0xBE541501,0xBE6C9A7F,0xBE827DC0,0xBE8E9A22,0xBE9AA086,0xBEA68F12,0xBEB263EF,0xBEBE1D4A,
		0xBEC9B953,0xBED53641,0xBEE0924F,0xBEEBCBBB,0xBEF6E0CB,0xBF00E7E4,0xBF064B82,0xBF0B9A6B,
		0xBF10D3CD,0xBF15F6D9,0xBF1B02C6,0xBF1FF6CB,0xBF24D225,0xBF299415,0xBF2E3BDE,0xBF32C8C9,
		0x3C490E90,0x3D16C32C,0x3D7B2B74,0x3DAFB680,0x3DE1BC2E,0x3E09CF86,0x3E22ABB6,0x3E3B6ECF,
		0x3E541501,0x3E6C9A7F,0x3E827DC0,0x3E8E9A22,0x3E9AA086,0x3EA68F12,0x3EB263EF,0x3EBE1D4A,
		0x3EC9B953,0x3ED53641,0x3EE0924F,0x3EEBCBBB,0x3EF6E0CB,0x3F00E7E4,0x3F064B82,0x3F0B9A6B,
		0x3F10D3CD,0x3F15F6D9,0x3F1B02C6,0x3F1FF6CB,0x3F24D225,0x3F299415,0x3F2E3BDE,0x3F32C8C9,
	},{
		0xBBC90F88,0xBC96C9B6,0xBCFB49BA,0xBD2FE007,0xBD621469,0xBD8A200A,0xBDA3308C,0xBDBC3AC3,
		0xBDD53DB9,0xBDEE3876,0xBE039502,0xBE1008B7,0xBE1C76DE,0xBE28DEFC,0xBE354098,0xBE419B37,
		0xBE4DEE60,0xBE5A3997,0xBE667C66,0xBE72B651,0xBE7EE6E1,0xBE8586CE,0xBE8B9507,0xBE919DDD,
		0xBE97A117,0xBE9D9E78,0xBEA395C5,0xBEA986C4,0xBEAF713A,0xBEB554EC,0xBEBB31A0,0xBEC1071E,
		0xBEC6D529,0xBECC9B8B,0xBED25A09,0xBED8106B,0xBEDDBE79,0xBEE363FA,0xBEE900B7,0xBEEE9479,
		0xBEF41F07,0xBEF9A02D,0xBEFF17B2,0xBF0242B1,0xBF04F484,0xBF07A136,0xBF0A48AD,0xBF0CEAD0,
		0xBF0F8784,0xBF121EB0,0xBF14B039,0xBF173C07,0xBF19C200,0xBF1C420C,0xBF1EBC12,0xBF212FF9,
		0xBF239DA9,0xBF26050A,0xBF286605,0xBF2AC082,0xBF2D1469,0xBF2F61A5,0xBF31A81D,0xBF33E7BC,
	}
};
static const unsigned int stChannel_Decode5_list3Int[2][0x40]={
	{
		0x3A3504F0,0x3B0183B8,0x3B70C538,0x3BBB9268,0x3C04A809,0x3C308200,0x3C61284C,0x3C8B3F17,
		0x3CA83992,0x3CC77FBD,0x3CE91110,0x3D0677CD,0x3D198FC4,0x3D2DD35C,0x3D434643,0x3D59ECC1,
		0x3D71CBA8,0x3D85741E,0x3D92A413,0x3DA078B4,0x3DAEF522,0x3DBE1C9E,0x3DCDF27B,0x3DDE7A1D,
		0x3DEFB6ED,0x3E00D62B,0x3E0A2EDA,0x3E13E72A,0x3E1E00B1,0x3E287CF2,0x3E335D55,0x3E3EA321,
		0x3E4A4F75,0x3E56633F,0x3E62DF37,0x3E6FC3D1,0x3E7D1138,0x3E8563A2,0x3E8C72B7,0x3E93B561,
		0x3E9B2AEF,0x3EA2D26F,0x3EAAAAAB,0x3EB2B222,0x3EBAE706,0x3EC34737,0x3ECBD03D,0x3ED47F46,
		0x3EDD5128,0x3EE6425C,0x3EEF4EFF,0x3EF872D7,0x3F00D4A9,0x3F0576CA,0x3F0A1D3B,0x3F0EC548,
		0x3F136C25,0x3F180EF2,0x3F1CAAC2,0x3F213CA2,0x3F25C1A5,0x3F2A36E7,0x3F2E9998,0x3F32E705,
	},{
		0xBF371C9E,0xBF3B37FE,0xBF3F36F2,0xBF431780,0xBF46D7E6,0xBF4A76A4,0xBF4DF27C,0xBF514A6F,
		0xBF547DC5,0xBF578C03,0xBF5A74EE,0xBF5D3887,0xBF5FD707,0xBF6250DA,0xBF64A699,0xBF66D908,
		0xBF68E90E,0xBF6AD7B1,0xBF6CA611,0xBF6E5562,0xBF6FE6E7,0xBF715BEF,0xBF72B5D1,0xBF73F5E6,
		0xBF751D89,0xBF762E13,0xBF7728D7,0xBF780F20,0xBF78E234,0xBF79A34C,0xBF7A5397,0xBF7AF439,
		0xBF7B8648,0xBF7C0ACE,0xBF7C82C8,0xBF7CEF26,0xBF7D50CB,0xBF7DA88E,0xBF7DF737,0xBF7E3D86,
		0xBF7E7C2A,0xBF7EB3CC,0xBF7EE507,0xBF7F106C,0xBF7F3683,0xBF7F57CA,0xBF7F74B6,0xBF7F8DB6,
		0xBF7FA32E,0xBF7FB57B,0xBF7FC4F6,0xBF7FD1ED,0xBF7FDCAD,0xBF7FE579,0xBF7FEC90,0xBF7FF22E,
		0xBF7FF688,0xBF7FF9D0,0xBF7FFC32,0xBF7FFDDA,0xBF7FFEED,0xBF7FFF8F,0xBF7FFFDF,0xBF7FFFFC,
	}
};

void stChannel_Decode5(stChannel *ch,int index){
	const float *s, *s1, *s2;
	float *d;
	unsigned int i, count1, count2, j, k;
	s=ch->block;d=ch->wav1;
	for(i=0,count1=1,count2=0x40;i<7;i++,count1<<=1,count2>>=1){
		float *d1=d;
		float *d2=&d[count2];
		float *w;
		for(j=0;j<count1;j++){
			for(k=0;k<count2;k++){
				float a=*(s++);
				float b=*(s++);
				*(d1++)=b+a;
				*(d2++)=a-b;
			}
			d1+=count2;
			d2+=count2;
		}
		w=(float*)&s[-0x80];s=d;d=w;
	}
	s=ch->wav1;d=ch->block;
	for(i=0,count1=0x40,count2=1;i<7;i++,count1>>=1,count2<<=1){
		const float *list1Float=(const float *)stChannel_Decode5_list1Int[i];
		const float *list2Float=(const float *)stChannel_Decode5_list2Int[i];
		float *d1, *d2, *w;
		s1=s;
		s2=&s1[count2];
		d1=d;
		d2=&d1[count2*2-1];
		for(j=0;j<count1;j++){
			for(k=0;k<count2;k++){
				float a=*(s1++);
				float b=*(s2++);
				float c=*(list1Float++);
				float d=*(list2Float++);
				*(d1++)=a*c-b*d;
				*(d2--)=a*d+b*c;
			}
			s1+=count2;
			s2+=count2;
			d1+=count2;
			d2+=count2*3;
		}
		w=(float*)s;s=d;d=w;
	}
	d=ch->wav2;
	for(i=0;i<0x80;i++)*(d++)=*(s++);
	s=(const float *)stChannel_Decode5_list3Int;d=ch->wave[index];
	s1=&ch->wav2[0x40];s2=ch->wav3;
	for(i=0;i<0x40;i++)*(d++)=*(s1++)**(s++)+*(s2++);
	for(i=0;i<0x40;i++)*(d++)=*(s++)**(--s1)-*(s2++);
	s1=&ch->wav2[0x40-1];d=ch->wav3;
	for(i=0;i<0x40;i++)*(d++)=*(s1--)**(--s);
	for(i=0;i<0x40;i++)*(d++)=*(--s)**(++s1);
}
