#ifndef _QUANT_H_
#define _QUANT_H_

typedef enum
{
	Intra_4x4_Y = 0,
	Intra_4x4_Cb = 1,
	Intra_4x4_Cr = 2,
	Inter_4x4_Y = 3,
	Inter_4x4_Cb = 4,
	Inter_4x4_Cr = 5,
	Intra_8x8_Y = 0,
	Inter_8x8_Y = 1
} ScaleMatrixIndex;

typedef struct
{
	int qmatrix_4x4[6][16];
	int qmatrix_8x8[2][64];
	int iqmatrix_4x4[6][16];
	int iqmatrix_8x8[2][64];
} QuantParam;

static __inline int rshift_rnd_sf(int x, int a)
{
  return ((x + (1 << (a-1) )) >> a);
}

void set_quant_matrix(void *ptEnc, void *ptSPS, QuantParam *pQuant);
int calculate_quant_param(SeqParameterSet *sps, QuantParam *pQuant);
void dump_quant_matrix(void *ptEnc);


#endif
