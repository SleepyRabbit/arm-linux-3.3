#include <linux/module.h>
#include <linux/kernel.h>
#include <adda308_api.h>
#include "gmaac_dec_api.h"
#include "gmaac_enc_api.h"
#include "typedefs.h"
#include "audio_coprocessor.h"
#include "g711.h"
#include "g711s.h"
#include "adpcm.h"
#include "mem_manag.h"

/* Replace adda308.ko */
int adda308_get_ssp1_rec_src(void)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(adda308_get_ssp1_rec_src);

void adda308_enable_spkout(void)
{
    printk("This is pseudo %s\n", __FUNCTION__);
}
EXPORT_SYMBOL(adda308_enable_spkout);

void adda308_disable_spkout(void)
{
    printk("This is pseudo %s\n", __FUNCTION__);
}
EXPORT_SYMBOL(adda308_disable_spkout);

void adda308_i2s_apply(void)
{
    printk("This is pseudo %s\n", __FUNCTION__);
}
EXPORT_SYMBOL(adda308_i2s_apply);

void adda302_set_loopback(int on)
{
    printk("This is pseudo %s\n", __FUNCTION__);
}
EXPORT_SYMBOL(adda302_set_loopback);

ADDA308_SSP_USAGE ADDA308_Check_SSP_Current_Usage(void)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(ADDA308_Check_SSP_Current_Usage);

void adda302_set_ad_fs(const enum adda308_fs_frequency fs)
{
    printk("This is pseudo %s\n", __FUNCTION__);
}
EXPORT_SYMBOL(adda302_set_ad_fs);

void adda302_set_da_fs(const enum adda308_fs_frequency fs)
{
    printk("This is pseudo %s\n", __FUNCTION__);
}
EXPORT_SYMBOL(adda302_set_da_fs);

void adda302_set_fs(const enum adda308_fs_frequency fs)
{
    printk("This is pseudo %s\n", __FUNCTION__);
}
EXPORT_SYMBOL(adda302_set_fs);

void adda302_set_ad_powerdown(int off)
{
    printk("This is pseudo %s\n", __FUNCTION__);
}
EXPORT_SYMBOL(adda302_set_ad_powerdown);

void adda302_set_da_powerdown(int off)
{
    printk("This is pseudo %s\n", __FUNCTION__);
}
EXPORT_SYMBOL(adda302_set_da_powerdown);

int adda308_get_output_mode(void)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(adda308_get_output_mode);

void ADDA308_Set_DA_Source(int da_src)
{
    printk("This is pseudo %s\n", __FUNCTION__);
}
EXPORT_SYMBOL(ADDA308_Set_DA_Source);


/* Replace codec.ko */
int GM_AAC_Dec_Init(GMAACDecHandle *hDecoder, aac_buffer *b, GMAACDecInfo **frameInfo)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(GM_AAC_Dec_Init);

void GM_AAC_Dec_Destory(GMAACDecHandle hDecoder, aac_buffer *b, GMAACDecInfo **frameInfo)
{
    printk("This is pseudo %s\n", __FUNCTION__);
}
EXPORT_SYMBOL(GM_AAC_Dec_Destory);

int AAC_Create(int id, GMAAC_INFORMATION *gm_aac, AUDIO_CODEC_SETTING *codec_setting, int MultiChannel)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(AAC_Create);

int AAC_Enc(int16_t convertBuf[][2048], uint8_t outbuf[][2048], int len[],  GMAAC_INFORMATION *gm_aac)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(AAC_Enc);

int AAC_Destory(GMAAC_INFORMATION *gm_aac)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(AAC_Destory);

void* GMAACDecode(GMAACDecHandle hDecoder, GMAACDecInfo *hInfo, unsigned char *buffer, unsigned long buffer_size)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return NULL;
}
EXPORT_SYMBOL(GMAACDecode);

int g711_init_enc(AUDIO_CODEC_SETTING *codec_setting, INPARAM *inparam[])
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(g711_init_enc);

int g711_init_dec(AUDIO_CODEC_SETTING *codec_setting, INPARAM **inparam1)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(g711_init_dec);

void g711enc_destory(INPARAM *inparam[], short MultiChannel)
{
    printk("This is pseudo %s\n", __FUNCTION__);
}
EXPORT_SYMBOL(g711enc_destory);

void g711dec_destory(INPARAM *inparam)
{
    printk("This is pseudo %s\n", __FUNCTION__);
}
EXPORT_SYMBOL(g711dec_destory);

int Init_IMA_ADPCM_Dec(ADPCM_CODEC_PARAMETERS *ima_paras)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(Init_IMA_ADPCM_Dec);

void IMA_ADPCM_dec(ADPCM_CODEC_PARAMETERS *ima_paras, unsigned char decInputBuf[], short decOutputBuf[])
{
    printk("This is pseudo %s\n", __FUNCTION__);
}
EXPORT_SYMBOL(IMA_ADPCM_dec);

int Init_IMA_ADPCM_Enc(ADPCM_CODEC_PARAMETERS *ima_paras, IMA_state_t *state[][2], int MultiChannel)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(Init_IMA_ADPCM_Enc);

int IMA_ADPCM_Enc(IMA_state_t *state[][2], short encInputBuf[][2048], unsigned char encOutputBuf[][2048], int encLen[],  AUDIO_CODEC_SETTING *codec_setting)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(IMA_ADPCM_Enc);

int IMA_ADPCM_Enc_Destory(IMA_state_t *state[][2], short multiChannel)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(IMA_ADPCM_Enc_Destory);

int G711_Enc(INPARAM *inparam[], short encInputBuf[][2048], unsigned char encOutputBuf[][2048], int encLen[], short Mulichannel_no )
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(G711_Enc);

int G711_Dec(INPARAM *inparam, unsigned char *InputBuf, short *OutputBuf )
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(G711_Dec);


/* Replace audio_drv.ko */
void set_live_sound(int ssp_idx, int chan_num, bool is_on)
{
    printk("This is pseudo %s\n", __FUNCTION__);
}
EXPORT_SYMBOL(set_live_sound);


int init_module(void)
{
    return 0;
}

void cleanup_module(void)
{
}
