#ifdef LINUX
	#include <linux/kernel.h>
#endif
#include "global.h"
#include "quant.h"
#include "params.h"
#include "mcp280_reg.h"

static const short Quant_intra_default[16] =
{
	 6,13,20,28,
	13,20,28,32,
	20,28,32,37,
	28,32,37,42
};

static const short Quant_inter_default[16] =
{
	10,14,20,24,
	14,20,24,27,
	20,24,27,30,
	24,27,30,34
};

static const short Quant8_intra_default[64] =
{
	 6,10,13,16,18,23,25,27,
	10,11,16,18,23,25,27,29,
	13,16,18,23,25,27,29,31,
	16,18,23,25,27,29,31,33,
	18,23,25,27,29,31,33,36,
	23,25,27,29,31,33,36,38,
	25,27,29,31,33,36,38,40,
	27,29,31,33,36,38,40,42
};

static const short Quant8_inter_default[64] =
{
	 9,13,15,17,19,21,22,24,
	13,13,17,19,21,22,24,25,
	15,17,19,21,22,24,25,27,
	17,19,21,22,24,25,27,28,
	19,21,22,24,25,27,28,30,
	21,22,24,25,27,28,30,32,
	22,24,25,27,28,30,32,33,
	24,25,27,28,30,32,33,35
};

const int quant_coef[6][4][4] = {
	{{13107, 8066,13107, 8066},{ 8066, 5243, 8066, 5243},{13107, 8066,13107, 8066},{ 8066, 5243, 8066, 5243}},
	{{11916, 7490,11916, 7490},{ 7490, 4660, 7490, 4660},{11916, 7490,11916, 7490},{ 7490, 4660, 7490, 4660}},
	{{10082, 6554,10082, 6554},{ 6554, 4194, 6554, 4194},{10082, 6554,10082, 6554},{ 6554, 4194, 6554, 4194}},
	{{ 9362, 5825, 9362, 5825},{ 5825, 3647, 5825, 3647},{ 9362, 5825, 9362, 5825},{ 5825, 3647, 5825, 3647}},
	{{ 8192, 5243, 8192, 5243},{ 5243, 3355, 5243, 3355},{ 8192, 5243, 8192, 5243},{ 5243, 3355, 5243, 3355}},
	{{ 7282, 4559, 7282, 4559},{ 4559, 2893, 4559, 2893},{ 7282, 4559, 7282, 4559},{ 4559, 2893, 4559, 2893}}
};

const int dequant_coef[6][4][4] = {
	{{10, 13, 10, 13},{ 13, 16, 13, 16},{10, 13, 10, 13},{ 13, 16, 13, 16}},
	{{11, 14, 11, 14},{ 14, 18, 14, 18},{11, 14, 11, 14},{ 14, 18, 14, 18}},
	{{13, 16, 13, 16},{ 16, 20, 16, 20},{13, 16, 13, 16},{ 16, 20, 16, 20}},
	{{14, 18, 14, 18},{ 18, 23, 18, 23},{14, 18, 14, 18},{ 18, 23, 18, 23}},
	{{16, 20, 16, 20},{ 20, 25, 20, 25},{16, 20, 16, 20},{ 20, 25, 20, 25}},
	{{18, 23, 18, 23},{ 23, 29, 23, 29},{18, 23, 18, 23},{ 23, 29, 23, 29}}
};

const int quant_coef8[6][8][8] =
{
	{
		{13107,  12222,  16777,  12222,  13107,  12222,  16777,  12222},
		{12222,  11428,  15481,  11428,  12222,  11428,  15481,  11428},
		{16777,  15481,  20972,  15481,  16777,  15481,  20972,  15481},
		{12222,  11428,  15481,  11428,  12222,  11428,  15481,  11428},
		{13107,  12222,  16777,  12222,  13107,  12222,  16777,  12222},
		{12222,  11428,  15481,  11428,  12222,  11428,  15481,  11428},
		{16777,  15481,  20972,  15481,  16777,  15481,  20972,  15481},
		{12222,  11428,  15481,  11428,  12222,  11428,  15481,  11428}
	},
	{
		{11916,  11058,  14980,  11058,  11916,  11058,  14980,  11058},
		{11058,  10826,  14290,  10826,  11058,  10826,  14290,  10826},
		{14980,  14290,  19174,  14290,  14980,  14290,  19174,  14290},
		{11058,  10826,  14290,  10826,  11058,  10826,  14290,  10826},
		{11916,  11058,  14980,  11058,  11916,  11058,  14980,  11058},
		{11058,  10826,  14290,  10826,  11058,  10826,  14290,  10826},
		{14980,  14290,  19174,  14290,  14980,  14290,  19174,  14290},
		{11058,  10826,  14290,  10826,  11058,  10826,  14290,  10826}
	},
	{
		{10082,   9675,  12710,   9675,  10082,   9675,  12710,   9675},
		{ 9675,   8943,  11985,   8943,   9675,   8943,  11985,   8943},
		{12710,  11985,  15978,  11985,  12710,  11985,  15978,  11985},
		{ 9675,   8943,  11985,   8943,   9675,   8943,  11985,   8943},
		{10082,   9675,  12710,   9675,  10082,   9675,  12710,   9675},
		{ 9675,   8943,  11985,   8943,   9675,   8943,  11985,   8943},
		{12710,  11985,  15978,  11985,  12710,  11985,  15978,  11985},
		{ 9675,   8943,  11985,   8943,   9675,   8943,  11985,   8943}
	},
	{
		{ 9362,   8931,  11984,   8931,   9362,   8931,  11984,   8931},
		{ 8931,   8228,  11259,   8228,   8931,   8228,  11259,   8228},
		{11984,  11259,  14913,  11259,  11984,  11259,  14913,  11259},
		{ 8931,   8228,  11259,   8228,   8931,   8228,  11259,   8228},
		{ 9362,   8931,  11984,   8931,   9362,   8931,  11984,   8931},
		{ 8931,   8228,  11259,   8228,   8931,   8228,  11259,   8228},
		{11984,  11259,  14913,  11259,  11984,  11259,  14913,  11259},
		{ 8931,   8228,  11259,   8228,   8931,   8228,  11259,   8228}
	},
	{
		{ 8192,   7740,  10486,   7740,   8192,   7740,  10486,   7740},
		{ 7740,   7346,   9777,   7346,   7740,   7346,   9777,   7346},
		{10486,   9777,  13159,   9777,  10486,   9777,  13159,   9777},
		{ 7740,   7346,   9777,   7346,   7740,   7346,   9777,   7346},
		{ 8192,   7740,  10486,   7740,   8192,   7740,  10486,   7740},
		{ 7740,   7346,   9777,   7346,   7740,   7346,   9777,   7346},
		{10486,   9777,  13159,   9777,  10486,   9777,  13159,   9777},
		{ 7740,   7346,   9777,   7346,   7740,   7346,   9777,   7346}
	},
	{
		{ 7282,   6830,   9118,   6830,   7282,   6830,   9118,   6830},
		{ 6830,   6428,   8640,   6428,   6830,   6428,   8640,   6428},
		{ 9118,   8640,  11570,   8640,   9118,   8640,  11570,   8640},
		{ 6830,   6428,   8640,   6428,   6830,   6428,   8640,   6428},
		{ 7282,   6830,   9118,   6830,   7282,   6830,   9118,   6830},
		{ 6830,   6428,   8640,   6428,   6830,   6428,   8640,   6428},
		{ 9118,   8640,  11570,   8640,   9118,   8640,  11570,   8640},
		{ 6830,   6428,   8640,   6428,   6830,   6428,   8640,   6428}
	}
};



const int dequant_coef8[6][8][8] =
{
	{
		{20,  19, 25, 19, 20, 19, 25, 19},
		{19,  18, 24, 18, 19, 18, 24, 18},
		{25,  24, 32, 24, 25, 24, 32, 24},
		{19,  18, 24, 18, 19, 18, 24, 18},
		{20,  19, 25, 19, 20, 19, 25, 19},
		{19,  18, 24, 18, 19, 18, 24, 18},
		{25,  24, 32, 24, 25, 24, 32, 24},
		{19,  18, 24, 18, 19, 18, 24, 18}
	},
	{
		{22,  21, 28, 21, 22, 21, 28, 21},
		{21,  19, 26, 19, 21, 19, 26, 19},
		{28,  26, 35, 26, 28, 26, 35, 26},
		{21,  19, 26, 19, 21, 19, 26, 19},
		{22,  21, 28, 21, 22, 21, 28, 21},
		{21,  19, 26, 19, 21, 19, 26, 19},
		{28,  26, 35, 26, 28, 26, 35, 26},
		{21,  19, 26, 19, 21, 19, 26, 19}
	},
	{
		{26,  24, 33, 24, 26, 24, 33, 24},
		{24,  23, 31, 23, 24, 23, 31, 23},
		{33,  31, 42, 31, 33, 31, 42, 31},
		{24,  23, 31, 23, 24, 23, 31, 23},
		{26,  24, 33, 24, 26, 24, 33, 24},
		{24,  23, 31, 23, 24, 23, 31, 23},
		{33,  31, 42, 31, 33, 31, 42, 31},
		{24,  23, 31, 23, 24, 23, 31, 23}
	},
	{
		{28,  26, 35, 26, 28, 26, 35, 26},
		{26,  25, 33, 25, 26, 25, 33, 25},
		{35,  33, 45, 33, 35, 33, 45, 33},
		{26,  25, 33, 25, 26, 25, 33, 25},
		{28,  26, 35, 26, 28, 26, 35, 26},
		{26,  25, 33, 25, 26, 25, 33, 25},
		{35,  33, 45, 33, 35, 33, 45, 33},
		{26,  25, 33, 25, 26, 25, 33, 25}
	},
	{
		{32,  30, 40, 30, 32, 30, 40, 30},
		{30,  28, 38, 28, 30, 28, 38, 28},
		{40,  38, 51, 38, 40, 38, 51, 38},
		{30,  28, 38, 28, 30, 28, 38, 28},
		{32,  30, 40, 30, 32, 30, 40, 30},
		{30,  28, 38, 28, 30, 28, 38, 28},
		{40,  38, 51, 38, 40, 38, 51, 38},
		{30,  28, 38, 28, 30, 28, 38, 28}
	},
	{
		{36,  34, 46, 34, 36, 34, 46, 34},
		{34,  32, 43, 32, 34, 32, 43, 32},
		{46,  43, 58, 43, 46, 43, 58, 43},
		{34,  32, 43, 32, 34, 32, 43, 32},
		{36,  34, 46, 34, 36, 34, 46, 34},
		{34,  32, 43, 32, 34, 32, 43, 32},
		{46,  43, 58, 43, 46, 43, 58, 43},
		{34,  32, 43, 32, 34, 32, 43, 32}
	}
};

//#ifdef DUMP_MESSAGE
void dump_quant_matrix(void *ptEnc)
{
#ifdef LINUX
    EncoderParams *p_Enc = (EncoderParams *)ptEnc;
    volatile H264Reg_Quant *ptReg_quant = (H264Reg_Quant *)(p_Enc->u32BaseAddr + MCP280_OFF_QUANT);
	int i;

	for (i = 0; i<32; i++) {
	    printk("intra 8x8 Y[%02d]: %08x (%08x)\n", i, ptReg_quant->INTRA_8x8_Y[i], 0x90400500+i*4);
        printk("inter 8x8 Y[%02d]: %08x (%08x)\n", i, ptReg_quant->INTER_8x8_Y[i], 0x90400580+i*4);
    }
    for (i = 0; i<8; i++) {
        printk("intra 4x4 Y[%02x]: %08x (%08x)\n", i, ptReg_quant->INTRA_4x4_Y[i], 0x90400600+i*4);
        printk("intra 4x4 Cb[%02x]: %08x (%08x)\n", i, ptReg_quant->INTRA_4x4_Cb[i], 0x90400620+i*4);
        printk("intra 4x4 Cr[%02x]: %08x (%08x)\n", i, ptReg_quant->INTRA_4x4_Cr[i], 0x90400640+i*4);
        printk("inter 4x4 Y[%02x]: %08x (%08x)\n", i, ptReg_quant->INTER_4x4_Y[i], 0x90400680+i*4);
        printk("inter 4x4 Cb[%02x]: %08x (%08x)\n", i, ptReg_quant->INTER_4x4_Cb[i], 0x904006A0+i*4);
        printk("inter 4x4 Cr[%02x]: %08x (%08x)\n", i, ptReg_quant->INTER_4x4_Cr[i], 0x904006C0+i*4);
    }

	for (i = 0; i<16; i++) {
	    printk("inverse intra 8x8 Y[%02x]: %08x (%08x)\n", i, ptReg_quant->INV_INTRA_8x8_Y[i], 0x90400700+i*4);
		printk("inverse inter 8x8 Y[%02x]: %08x (%08x)\n", i, ptReg_quant->INV_INTER_8x8_Y[i], 0x90400740+i*4);
    }
	for (i = 0; i<4; i++) {
	    printk("inverse intra 4x4 Y[%02x]: %08x (%08x)\n", i, ptReg_quant->INV_INTRA_4x4_Y[i], 0x90400780+i*4);
		printk("inverse intra 4x4 Cb[%02x]: %08x (%08x)\n", i, ptReg_quant->INV_INTRA_4x4_Cb[i], 0x90400790+i*4);
		printk("inverse intra 4x4 Cr[%02x]: %08x (%08x)\n", i, ptReg_quant->INV_INTRA_4x4_Cr[i], 0x904007A0+i*4);
		printk("inverse inter 4x4 Y[%02x]: %08x (%08x)\n", i, ptReg_quant->INV_INTER_4x4_Y[i], 0x904007C0+i*4);
		printk("inverse inter 4x4 Cb[%02x]: %08x (%08x)\n", i, ptReg_quant->INV_INTER_4x4_Cb[i], 0x904007D0+i*4);
		printk("inverse inter 4x4 Cr[%02x]: %08x (%08x)\n", i, ptReg_quant->INV_INTER_4x4_Cr[i], 0x904007E0+i*4);
	}
#endif
}
//#endif

int calculate_quant_param(SeqParameterSet *sps, QuantParam *pQuant)
{
	int k, i, j, idx;
	if (!sps->seq_scaling_matrix_present_flag) {
		// assign 4x4 qmatrix
		for (k = 0; k<6; k++) {
			for (i = 0; i<16; i++) {
				pQuant->qmatrix_4x4[k][i] = 4096;
				pQuant->iqmatrix_4x4[k][i] = 16;
			}
		}
		//assign 8x8 qmatrix
		for (i = 0; i<64; i++) {
			pQuant->qmatrix_8x8[0][i] = 4096;
			pQuant->qmatrix_8x8[1][i] = 4096;
			pQuant->iqmatrix_8x8[0][i] = 16;
			pQuant->iqmatrix_8x8[1][i] = 16;
		}
	}
	else {
		// assign 4x4 qmatrix
		for (j = 0; j<4; j++) {
			for (i = 0; i<4; i++) {
				idx = (j<<2) + i;
				// intra
				if ((!sps->seq_scaling_list_present_flag[0]) || sps->UseDefaultScalingMatrix4x4Flag[0]) {
               pQuant->qmatrix_4x4[0][idx] = ((1<<17)/Quant_intra_default[idx] + 1) >> 1;
					pQuant->iqmatrix_4x4[0][idx] = Quant_intra_default[idx];
				}
				else {
					pQuant->qmatrix_4x4[0][idx] = ((1<<17)/sps->ScalingList4x4[0][idx] + 1) >> 1;
					pQuant->iqmatrix_4x4[0][idx] = sps->ScalingList4x4[0][idx];
				}

				if (!sps->seq_scaling_list_present_flag[1]) {
					pQuant->qmatrix_4x4[1][idx] = pQuant->qmatrix_4x4[0][idx];
					pQuant->iqmatrix_4x4[1][idx] = pQuant->qmatrix_4x4[0][idx];
				}
				else {
					pQuant->qmatrix_4x4[1][idx] = ((1<<17)/(sps->UseDefaultScalingMatrix4x4Flag[1] ? Quant_intra_default[idx] : sps->ScalingList4x4[1][idx]) + 1 ) >> 1;
					pQuant->iqmatrix_4x4[1][idx] = sps->UseDefaultScalingMatrix4x4Flag[1] ? Quant_intra_default[idx] : sps->ScalingList4x4[1][idx];
				}

				if (!sps->seq_scaling_list_present_flag[2]) {
					pQuant->qmatrix_4x4[2][idx] = pQuant->qmatrix_4x4[1][idx];
					pQuant->iqmatrix_4x4[2][idx] = pQuant->qmatrix_4x4[1][idx];
				}
				else {
					pQuant->qmatrix_4x4[2][idx] = ((1<<17)/(sps->UseDefaultScalingMatrix4x4Flag[2] ? Quant_intra_default[idx] : sps->ScalingList4x4[2][idx]) + 1 ) >> 1;
					pQuant->iqmatrix_4x4[2][idx] = sps->UseDefaultScalingMatrix4x4Flag[2] ? Quant_intra_default[idx] : sps->ScalingList4x4[2][idx];
				}

				// inter
				if ((!sps->seq_scaling_list_present_flag[3]) || sps->UseDefaultScalingMatrix4x4Flag[3]) {
					pQuant->qmatrix_4x4[3][idx] = ((1<<17)/Quant_inter_default[idx] + 1) >> 1;
					pQuant->iqmatrix_4x4[3][idx] = Quant_inter_default[idx];
				}
				else {
					pQuant->qmatrix_4x4[3][idx] = ((1<<17)/sps->ScalingList4x4[3][idx] + 1) >> 1;
					pQuant->iqmatrix_4x4[3][idx] = sps->ScalingList4x4[3][idx];
				}

				if (!sps->seq_scaling_list_present_flag[4]) {
					pQuant->qmatrix_4x4[4][idx] = pQuant->qmatrix_4x4[3][idx];
					pQuant->iqmatrix_4x4[4][idx] = pQuant->iqmatrix_4x4[3][idx];
				}
				else {
					pQuant->qmatrix_4x4[4][idx] = ((1<<17)/(sps->UseDefaultScalingMatrix4x4Flag[4] ? Quant_inter_default[idx] : sps->ScalingList4x4[4][idx]) + 1)>>1;
					pQuant->iqmatrix_4x4[4][idx] = sps->UseDefaultScalingMatrix4x4Flag[4] ? Quant_inter_default[idx] : sps->ScalingList4x4[4][idx];
				}

				if (!sps->seq_scaling_list_present_flag[5]) {
					pQuant->qmatrix_4x4[5][idx] = pQuant->qmatrix_4x4[4][idx];
					pQuant->iqmatrix_4x4[5][idx] = pQuant->iqmatrix_4x4[4][idx];
				}
				else {
					pQuant->qmatrix_4x4[5][idx] = ((1<<17)/(sps->UseDefaultScalingMatrix4x4Flag[5] ? Quant_inter_default[idx] : sps->ScalingList4x4[5][idx]) + 1)>>1;
					pQuant->iqmatrix_4x4[5][idx] = sps->UseDefaultScalingMatrix4x4Flag[5] ? Quant_inter_default[idx] : sps->ScalingList4x4[5][idx];
				}
			}
		}

      //assign 8x8 qmatrix
		for (j=0; j<8; j++) {
			for (i=0; i<8; i++) {
				idx = (j<<3) + i;
				// intra
				if ((!sps->seq_scaling_list_present_flag[6]) || sps->UseDefaultScalingMatrix8x8Flag[0]) {
					pQuant->qmatrix_8x8[0][idx] = (((1<<17)/Quant8_intra_default[idx]) + 1) >> 1;
					pQuant->iqmatrix_8x8[0][idx] = Quant8_intra_default[idx];
				}
				else {
					pQuant->qmatrix_8x8[0][idx] = (((1<<17)/sps->ScalingList8x8[0][idx]) + 1) >> 1;
					pQuant->iqmatrix_8x8[0][idx] = sps->ScalingList8x8[0][idx];
				}

				// inter
				if ((!sps->seq_scaling_list_present_flag[7]) || sps->UseDefaultScalingMatrix8x8Flag[1]) {
					pQuant->qmatrix_8x8[1][idx] = (((1<<17)/Quant8_inter_default[idx]) + 1)>>1;
					pQuant->iqmatrix_8x8[1][idx] = Quant8_inter_default[idx];
				}
				else {
					pQuant->qmatrix_8x8[1][idx] = (((1<<17)/sps->ScalingList8x8[1][idx]) + 1) >> 1;
					pQuant->iqmatrix_8x8[1][idx] = sps->ScalingList8x8[1][idx];
				}
			}
		}
	}

	return H264E_OK;
}

void set_quant_matrix(void *ptEnc, void *ptSPS, QuantParam *pQuant)
{
	EncoderParams *p_Enc = (EncoderParams *)ptEnc;
	volatile H264Reg_Quant *ptReg_quant = (H264Reg_Quant *)(p_Enc->u32BaseAddr + MCP280_OFF_QUANT);
	int i;
	//calculate_quant_param(ptSPS, pQuant);
	for (i = 0; i<64; i+=2) {
		ptReg_quant->INTRA_8x8_Y[i/2] = CONCATE_2_16BITS_VALUES(pQuant->qmatrix_8x8[Intra_8x8_Y][i+1], pQuant->qmatrix_8x8[Intra_8x8_Y][i]);
		ptReg_quant->INTER_8x8_Y[i/2] = CONCATE_2_16BITS_VALUES(pQuant->qmatrix_8x8[Inter_8x8_Y][i+1], pQuant->qmatrix_8x8[Inter_8x8_Y][i]);
	}
	for (i = 0; i<16; i+=2) {
		ptReg_quant->INTRA_4x4_Y[i/2] = CONCATE_2_16BITS_VALUES(pQuant->qmatrix_4x4[Intra_4x4_Y][i+1], pQuant->qmatrix_4x4[Intra_4x4_Y][i]);
		ptReg_quant->INTRA_4x4_Cb[i/2] = CONCATE_2_16BITS_VALUES(pQuant->qmatrix_4x4[Intra_4x4_Cb][i+1], pQuant->qmatrix_4x4[Intra_4x4_Cb][i]);
		ptReg_quant->INTRA_4x4_Cr[i/2] = CONCATE_2_16BITS_VALUES(pQuant->qmatrix_4x4[Intra_4x4_Cr][i+1], pQuant->qmatrix_4x4[Intra_4x4_Cr][i]);
		ptReg_quant->INTER_4x4_Y[i/2] = CONCATE_2_16BITS_VALUES(pQuant->qmatrix_4x4[Inter_4x4_Y][i+1], pQuant->qmatrix_4x4[Inter_4x4_Y][i]);
		ptReg_quant->INTER_4x4_Cb[i/2] = CONCATE_2_16BITS_VALUES(pQuant->qmatrix_4x4[Inter_4x4_Cb][i+1], pQuant->qmatrix_4x4[Inter_4x4_Cb][i]);
		ptReg_quant->INTER_4x4_Cr[i/2] = CONCATE_2_16BITS_VALUES(pQuant->qmatrix_4x4[Inter_4x4_Cr][i+1], pQuant->qmatrix_4x4[Inter_4x4_Cr][i]);
	}

	for (i = 0; i<64; i+=4) {
		ptReg_quant->INV_INTRA_8x8_Y[i/4] = CONCATE_4_8BITS_VALUES(pQuant->iqmatrix_8x8[Intra_8x8_Y][i+3], \
			pQuant->iqmatrix_8x8[Intra_8x8_Y][i+2], pQuant->iqmatrix_8x8[Intra_8x8_Y][i+1], pQuant->iqmatrix_8x8[Intra_8x8_Y][i]);
		ptReg_quant->INV_INTER_8x8_Y[i/4] = CONCATE_4_8BITS_VALUES(pQuant->iqmatrix_8x8[Inter_8x8_Y][i+3], \
			pQuant->iqmatrix_8x8[Inter_8x8_Y][i+2], pQuant->iqmatrix_8x8[Inter_8x8_Y][i+1], pQuant->iqmatrix_8x8[Inter_8x8_Y][i]);
	}
	for (i = 0; i<16; i+=4) {
		ptReg_quant->INV_INTRA_4x4_Y[i/4] = CONCATE_4_8BITS_VALUES(pQuant->iqmatrix_4x4[Intra_4x4_Y][i+3], \
			pQuant->iqmatrix_4x4[Intra_4x4_Y][i+2], pQuant->iqmatrix_4x4[Intra_4x4_Y][i+1], pQuant->iqmatrix_4x4[Intra_4x4_Y][i]);
		ptReg_quant->INV_INTRA_4x4_Cb[i/4] = CONCATE_4_8BITS_VALUES(pQuant->iqmatrix_4x4[Intra_4x4_Cb][i+3], \
			pQuant->iqmatrix_4x4[Intra_4x4_Cb][i+2], pQuant->iqmatrix_4x4[Intra_4x4_Cb][i+1], pQuant->iqmatrix_4x4[Intra_4x4_Cb][i]);
		ptReg_quant->INV_INTRA_4x4_Cr[i/4] = CONCATE_4_8BITS_VALUES(pQuant->iqmatrix_4x4[Intra_4x4_Cr][i+3], \
			pQuant->iqmatrix_4x4[Intra_4x4_Cr][i+2], pQuant->iqmatrix_4x4[Intra_4x4_Cr][i+1], pQuant->iqmatrix_4x4[Intra_4x4_Cr][i]);
		ptReg_quant->INV_INTER_4x4_Y[i/4] = CONCATE_4_8BITS_VALUES(pQuant->iqmatrix_4x4[Inter_4x4_Y][i+3], \
			pQuant->iqmatrix_4x4[Inter_4x4_Y][i+2], pQuant->iqmatrix_4x4[Inter_4x4_Y][i+1], pQuant->iqmatrix_4x4[Inter_4x4_Y][i]);
		ptReg_quant->INV_INTER_4x4_Cb[i/4] = CONCATE_4_8BITS_VALUES(pQuant->iqmatrix_4x4[Inter_4x4_Cb][i+3], \
			pQuant->iqmatrix_4x4[Inter_4x4_Cb][i+2], pQuant->iqmatrix_4x4[Inter_4x4_Cb][i+1], pQuant->iqmatrix_4x4[Inter_4x4_Cb][i]);
		ptReg_quant->INV_INTER_4x4_Cr[i/4] = CONCATE_4_8BITS_VALUES(pQuant->iqmatrix_4x4[Inter_4x4_Cr][i+3], \
			pQuant->iqmatrix_4x4[Inter_4x4_Cr][i+2], pQuant->iqmatrix_4x4[Inter_4x4_Cr][i+1], pQuant->iqmatrix_4x4[Inter_4x4_Cr][i]);
	}
#ifdef DUMP_MESSAGE
	dump_quant_matrix(p_Enc);
#endif
}
