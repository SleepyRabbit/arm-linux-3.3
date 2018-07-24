#ifdef LINUX
	#include <linux/kernel.h>
#else
	#include <stdio.h>
#endif
#include "quant.h"
#include "global.h"
#include "h264_reg.h"

static const int8_t default_scaling_list_intra_4x4[16] = {
	 6,13,20,28,
	13,20,28,32,
	20,28,32,37,
	28,32,37,42
};

static const int8_t default_scaling_list_inter_4x4[16] = {
	10,14,20,24,
	14,20,24,27,
	20,24,27,30,
	24,27,30,34
};

static const int8_t default_scaling_list_intra_8x8[64] = {
 6,10,13,16,18,23,25,27,
10,11,16,18,23,25,27,29,
13,16,18,23,25,27,29,31,
16,18,23,25,27,29,31,33,
18,23,25,27,29,31,33,36,
23,25,27,29,31,33,36,38,
25,27,29,31,33,36,38,40,
27,29,31,33,36,38,40,42
};

static const int8_t default_scaling_list_inter_8x8[64] = {
 9,13,15,17,19,21,22,24,
13,13,17,19,21,22,24,25,
15,17,19,21,22,24,25,27,
17,19,21,22,24,25,27,28,
19,21,22,24,25,27,28,30,
21,22,24,25,27,28,30,32,
22,24,25,27,28,30,32,33,
24,25,27,28,30,32,33,35
};

static const int8_t flat_scaling_list_4x4[16] = 
{	16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16
};

static const int8_t flat_scaling_list_8x8[64] = 
{
	16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16
};



static const int8_t QPC_TABLE[52] = 
{	 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 
	20,	21, 22, 23,	24, 25, 26, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 34, 35, 35, 
	36, 36,	37, 37, 37, 38, 38, 38, 39, 39, 39, 39
};

static void set_scaling_list(DecoderParams *p_Dec, VideoParams *p_Vid);

void set_qp(VideoParams *p_Vid)
{
	//p_Vid->QPy = p_Vid->slice_hdr->SliceQPy;

	p_Vid->QPu = p_Vid->active_pps->chroma_qp_index_offset + p_Vid->slice_hdr->SliceQPy;
	p_Vid->QPu = (p_Vid->QPu < 0 ? 0 : (p_Vid->QPu > 51 ? 51 : p_Vid->QPu));
	p_Vid->QPu = QPC_TABLE[p_Vid->QPu];

	p_Vid->QPv = p_Vid->active_pps->second_chroma_qp_index_offset + p_Vid->slice_hdr->SliceQPy;
	p_Vid->QPv = (p_Vid->QPv < 0 ? 0 : (p_Vid->QPv > 51 ? 51 : p_Vid->QPv));
	p_Vid->QPv = QPC_TABLE[p_Vid->QPv];
}


/*!
 ************************************************************************
 * \brief
 *    For mapping the q-matrix to the active id and calculate quantisation values
 *    and setting scaling list
 *
 ************************************************************************
 */
void assign_quant_params(DecoderParams *p_Dec, VideoParams *p_Vid)
{
	SeqParameterSet *sps = p_Vid->active_sps;
	PicParameterSet *pps = p_Vid->active_pps;

	uint32_t i;
	
	if ((!pps->pic_scaling_matrix_present_flag) && (!sps->seq_scaling_matrix_present_flag))
		for (i = 0; i<8; i++)
			p_Vid->ScalingList[i] = ((i < 6) ? (int8_t*)flat_scaling_list_4x4 : (int8_t*)flat_scaling_list_8x8);
	else {
		if (sps->seq_scaling_matrix_present_flag){ /* check sps first */
			for (i = 0; i<8; i++){
				if (i < 6){
					if (!sps->seq_scaling_list_present_flag[i]){ /* fall-back rule A */
						if(i==0)
							p_Vid->ScalingList[i] = (int8_t*)default_scaling_list_intra_4x4;
						else if(i==3)
							p_Vid->ScalingList[i] = (int8_t*)default_scaling_list_inter_4x4;
						else
							p_Vid->ScalingList[i] = p_Vid->ScalingList[i-1];
					}
					else {
						if (sps->UseDefaultScalingMatrix4x4Flag[i])
							p_Vid->ScalingList[i] = (i < 3 ? (int8_t*)default_scaling_list_intra_4x4 : (int8_t*)default_scaling_list_inter_4x4);
						else
							p_Vid->ScalingList[i] = sps->ScalingList4x4[i];
					}
				} // i >= 6
				else {
					if (!sps->seq_scaling_list_present_flag[i]){ /* fall-back rule A */
						if (i == 6)
							p_Vid->ScalingList[i] = (int8_t*)default_scaling_list_intra_8x8;
						else
							p_Vid->ScalingList[i] = (int8_t*)default_scaling_list_inter_8x8;
					}

					else {
						if (sps->UseDefaultScalingMatrix8x8Flag[i-6])
							p_Vid->ScalingList[i] = (i == 6 ? (int8_t*)default_scaling_list_intra_8x8 : (int8_t*)default_scaling_list_inter_8x8);
						else
							p_Vid->ScalingList[i] = sps->ScalingList8x8[i-6];
					}
				}
			}
		}

		if (pps->pic_scaling_matrix_present_flag) /* then check pps */
		{
			for (i = 0; i<8; i++){
				if (i < 6){
					if (!pps->pic_scaling_list_present_flag[i]){ /* fall-back rule B */
						if (i==0){
							if(!sps->seq_scaling_matrix_present_flag)
								p_Vid->ScalingList[i] = (int8_t*)default_scaling_list_intra_4x4;
						}
						else if (i==3){
						if(!sps->seq_scaling_matrix_present_flag)
							p_Vid->ScalingList[i] = (int8_t*)default_scaling_list_inter_4x4;
						}
						else
							p_Vid->ScalingList[i] = p_Vid->ScalingList[i-1];
					}
					else {
						if (pps->UseDefaultScalingMatrix4x4Flag[i])
							p_Vid->ScalingList[i] = (i < 3 ? (int8_t*)default_scaling_list_intra_4x4 : (int8_t*)default_scaling_list_inter_4x4);
						else
							p_Vid->ScalingList[i] = pps->ScalingList4x4[i];
					}
				}
				else {
					if (!pps->pic_scaling_list_present_flag[i]){ /* fall-back rule B */
						if (i == 6){
							if (!sps->seq_scaling_matrix_present_flag)
								p_Vid->ScalingList[i] = (int8_t*)default_scaling_list_intra_8x8;
						}
						else if (i == 7){
							if (!sps->seq_scaling_matrix_present_flag)
								p_Vid->ScalingList[i] = (int8_t*)default_scaling_list_inter_8x8;
						}
					}
					else {
						if (pps->UseDefaultScalingMatrix8x8Flag[i-6])
							p_Vid->ScalingList[i] = (i == 6 ? (int8_t*)default_scaling_list_intra_8x8 : (int8_t*)default_scaling_list_inter_8x8);
						else
							p_Vid->ScalingList[i] = pps->ScalingList8x8[i-6];
					}
				}
			}
		}
	}

	if (sps->seq_scaling_matrix_present_flag | pps->pic_scaling_matrix_present_flag){
		set_scaling_list(p_Dec, p_Vid);
    }
}

static void set_scaling_list(DecoderParams *p_Dec, VideoParams *p_Vid)
{
	volatile H264_reg_scale *ptReg_scale = (H264_reg_scale*)((uint32_t)p_Dec->pu32BaseAddr + H264_OFF_SFMM);
	uint32_t i;

	for (i = 0; i < 4; i++) {
		//ptReg_scale->INTRA4_Y[i] = CONCAT_4_8BITS_VALUES(p_Vid->ScalingList[0][i*4+3], p_Vid->ScalingList[0][i*4+2], p_Vid->ScalingList[0][i*4+1], p_Vid->ScalingList[0][i*4]);
		WRITE_REG(p_Dec, ((MCP300_SL0 << 7) + 0x00 + i * 4), ptReg_scale->INTRA4_Y[i], CONCAT_4_8BITS_VALUES(p_Vid->ScalingList[0][i*4+3], p_Vid->ScalingList[0][i*4+2], p_Vid->ScalingList[0][i*4+1], p_Vid->ScalingList[0][i*4]));
		//ptReg_scale->INTRA4_CB[i] = CONCAT_4_8BITS_VALUES(p_Vid->ScalingList[1][i*4+3], p_Vid->ScalingList[1][i*4+2], p_Vid->ScalingList[1][i*4+1], p_Vid->ScalingList[1][i*4]);
		WRITE_REG(p_Dec, ((MCP300_SL0 << 7) + 0x10 + i * 4), ptReg_scale->INTRA4_CB[i], CONCAT_4_8BITS_VALUES(p_Vid->ScalingList[1][i*4+3], p_Vid->ScalingList[1][i*4+2], p_Vid->ScalingList[1][i*4+1], p_Vid->ScalingList[1][i*4]));
		//ptReg_scale->INTRA4_CR[i] = CONCAT_4_8BITS_VALUES(p_Vid->ScalingList[2][i*4+3], p_Vid->ScalingList[2][i*4+2], p_Vid->ScalingList[2][i*4+1], p_Vid->ScalingList[2][i*4]);
		WRITE_REG(p_Dec, ((MCP300_SL0 << 7) + 0x20 + i * 4), ptReg_scale->INTRA4_CR[i], CONCAT_4_8BITS_VALUES(p_Vid->ScalingList[2][i*4+3], p_Vid->ScalingList[2][i*4+2], p_Vid->ScalingList[2][i*4+1], p_Vid->ScalingList[2][i*4]));

		//ptReg_scale->INTER4_Y[i] = CONCAT_4_8BITS_VALUES(p_Vid->ScalingList[3][i*4+3], p_Vid->ScalingList[3][i*4+2], p_Vid->ScalingList[3][i*4+1], p_Vid->ScalingList[3][i*4]);
		WRITE_REG(p_Dec, ((MCP300_SL0 << 7) + 0x30 + i * 4), ptReg_scale->INTER4_Y[i], CONCAT_4_8BITS_VALUES(p_Vid->ScalingList[3][i*4+3], p_Vid->ScalingList[3][i*4+2], p_Vid->ScalingList[3][i*4+1], p_Vid->ScalingList[3][i*4]));
		//ptReg_scale->INTER4_CB[i] = CONCAT_4_8BITS_VALUES(p_Vid->ScalingList[4][i*4+3], p_Vid->ScalingList[4][i*4+2], p_Vid->ScalingList[4][i*4+1], p_Vid->ScalingList[4][i*4]);
		WRITE_REG(p_Dec, ((MCP300_SL0 << 7) + 0x40 + i * 4), ptReg_scale->INTER4_CB[i], CONCAT_4_8BITS_VALUES(p_Vid->ScalingList[4][i*4+3], p_Vid->ScalingList[4][i*4+2], p_Vid->ScalingList[4][i*4+1], p_Vid->ScalingList[4][i*4]));
		//ptReg_scale->INTER4_CR[i] = CONCAT_4_8BITS_VALUES(p_Vid->ScalingList[5][i*4+3], p_Vid->ScalingList[5][i*4+2], p_Vid->ScalingList[5][i*4+1], p_Vid->ScalingList[5][i*4]);
		WRITE_REG(p_Dec, ((MCP300_SL0 << 7) + 0x50 + i * 4), ptReg_scale->INTER4_CR[i], CONCAT_4_8BITS_VALUES(p_Vid->ScalingList[5][i*4+3], p_Vid->ScalingList[5][i*4+2], p_Vid->ScalingList[5][i*4+1], p_Vid->ScalingList[5][i*4]));
	}

	for (i = 0; i < 16; i++) {
		//ptReg_scale->INTRA8[i] = CONCAT_4_8BITS_VALUES(p_Vid->ScalingList[6][i*4+3], p_Vid->ScalingList[6][i*4+2], p_Vid->ScalingList[6][i*4+1], p_Vid->ScalingList[6][i*4]);
		WRITE_REG(p_Dec, ((MCP300_SL0 << 7) + 0x60 + i * 4), ptReg_scale->INTRA8[i], CONCAT_4_8BITS_VALUES(p_Vid->ScalingList[6][i*4+3], p_Vid->ScalingList[6][i*4+2], p_Vid->ScalingList[6][i*4+1], p_Vid->ScalingList[6][i*4]));
		//ptReg_scale->INTER8[i] = CONCAT_4_8BITS_VALUES(p_Vid->ScalingList[7][i*4+3], p_Vid->ScalingList[7][i*4+2], p_Vid->ScalingList[7][i*4+1], p_Vid->ScalingList[7][i*4]);
		WRITE_REG(p_Dec, ((MCP300_SL0 << 7) + 0xA0 + i * 4), ptReg_scale->INTER8[i], CONCAT_4_8BITS_VALUES(p_Vid->ScalingList[7][i*4+3], p_Vid->ScalingList[7][i*4+2], p_Vid->ScalingList[7][i*4+1], p_Vid->ScalingList[7][i*4]));
	}
}

