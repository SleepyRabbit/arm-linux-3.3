#ifndef AEC_API
#define AEC_API

//#include "common.h"

#include "TypeDef.h"
#include "ng.h"
#include "speex_echo.h"
#include "speex_preprocess.h"
#include "lpf.h"

#define R_Shift1
#define ENABLE_NLP
#define WQ20
#define INT_MODE
#define V5TE_ASM_FFT
//#define DEBUG_MODE
//#define COMPARE
//#define TEST_NR

#define ABITSTREAM_LEN 			2048
#define REMOTE_BUF_LENGTH 	64*ABITSTREAM_LEN
#define AEC_FAR_BUF_LEN     64*1024//32768//16384	//samples
#define AEC_FRAME_SIZE	    1024	//samples
//#define Q24_format 				16777216 //(int)pow(2, 24)



//--- come from aec_r.h ---//
#define PI 						3.14159265359
#define M							8
#define MP1						9
#define	N							256 //512
#define N2						512 //1024
#define N3            192
#define N3P1          193
#define N3P64         320
#define NM1						N-1
#define FSZ						N/2
#define NC						N/2	
#define FSZP1					FSZ+1
#define Win_Size			2*FSZ
#define	CPXBYTES			N*4
#define N10000				(int)(N*10000.0/32768.0 + 0.5)
#define N1000					(int)(N*1000.0/32768.0 + 0.5)
#define N100					(int)(N*100.0/32768.0 + 0.5)
#define	NEXP					(int)(log(N)/log(2))
#define log2N					(NEXP + 1)
#define RNDCNT				(int)(pow(2,NEXP-1))
#define ROUNDFFT(x)		((x)>0? (((x)+RNDCNT)>>NEXP):(-((-(x)+RNDCNT)>>NEXP)))
#define	MAIN_LOOP_NUM 1024/FSZ

#define 	Q0_format	(1<<0)
#define 	Q1_format	(1<<1)
#define 	Q2_format	(1<<2)
#define 	Q3_format	(1<<3)
#define 	Q4_format	(1<<4)
#define 	Q5_format	(1<<5)
#define 	Q6_format	(1<<6)
#define 	Q7_format	(1<<7)
#define 	Q8_format	(1<<8)
#define 	Q9_format	(1<<9)
#define 	Q10_format	(1<<10)
#define 	Q11_format 	(1<<11)
#define 	Q12_format 	(1<<12)
#define 	Q13_format 	(1<<13)
#define 	Q14_format 	(1<<14)
#define 	Q15_format 	(1<<15)
#define 	Q_Format	(1<<15)
#define 	Q16_format 	(1<<16)
#define 	Q17_format 	(1<<17)
#define 	Q18_format 	(1<<18)
#define 	Q19_format 	(1<<19)
#define 	Q20_format 	(1<<20)
#define 	Q21_format 	(1<<21)
#define 	Q22_format 	(1<<22)
#define 	Q23_format 	(1<<23)
#define 	Q24_format 	(1<<24)
#define 	Q25_format 	(1<<25)
#define 	Q26_format 	(1<<26)
#define 	Q27_format 	(1<<27)
#define 	Q28_format 	(1<<28)
#define 	Q29_format	(1<<29)
#define 	Q30_format	(1<<30)
#define 	Q31_format	(1<<31)
#define 	Q32_format	(1<<32)

#if 1
#define		round32(x)		((x)>0? (((x)+Q31_format)>>32):(-((-(x)+Q31_format)>>32)))
#define		round31(x)		((x)>0? (((x)+Q30_format)>>31):(-((-(x)+Q30_format)>>31)))
#define		round30(x)		((x)>0? (((x)+Q29_format)>>30):(-((-(x)+Q29_format)>>30)))
#define		round29(x)		((x)>0? (((x)+Q28_format)>>29):(-((-(x)+Q28_format)>>29)))
#define		round28(x)		((x)>0? (((x)+Q27_format)>>28):(-((-(x)+Q27_format)>>28)))
#define		round27(x)		((x)>0? (((x)+Q26_format)>>27):(-((-(x)+Q26_format)>>27)))
#define		round26(x)		((x)>0? (((x)+Q25_format)>>26):(-((-(x)+Q25_format)>>26)))
#define		round25(x)		((x)>0? (((x)+Q24_format)>>25):(-((-(x)+Q24_format)>>25)))
#define		round24(x)		((x)>0? (((x)+Q23_format)>>24):(-((-(x)+Q23_format)>>24)))
#define		round23(x)		((x)>0? (((x)+Q22_format)>>23):(-((-(x)+Q22_format)>>23)))
#define		round22(x)		((x)>0? (((x)+Q21_format)>>22):(-((-(x)+Q21_format)>>22)))
#define		round21(x)		((x)>0? (((x)+Q20_format)>>21):(-((-(x)+Q20_format)>>21)))
#define		round20(x)		((x)>0? (((x)+Q19_format)>>20):(-((-(x)+Q19_format)>>20)))
#define		round19(x)		((x)>0? (((x)+Q18_format)>>19):(-((-(x)+Q18_format)>>19)))
#define		round18(x)		((x)>0? (((x)+Q17_format)>>18):(-((-(x)+Q17_format)>>18)))
#define		round17(x)		((x)>0? (((x)+Q16_format)>>17):(-((-(x)+Q16_format)>>17)))
#define		round16(x)		((x)>0? (((x)+Q15_format)>>16):(-((-(x)+Q15_format)>>16)))
#define		round15(x)		((x)>0? (((x)+Q14_format)>>15):(-((-(x)+Q14_format)>>15)))
#define		round14(x)		((x)>0? (((x)+Q13_format)>>14):(-((-(x)+Q13_format)>>14)))
#define		round13(x)		((x)>0? (((x)+Q12_format)>>13):(-((-(x)+Q12_format)>>13)))
#define		round12(x)		((x)>0? (((x)+Q11_format)>>12):(-((-(x)+Q11_format)>>12)))
#define		round11(x)		((x)>0? (((x)+Q10_format)>>11):(-((-(x)+Q10_format)>>11)))
#define		round10(x)		((x)>0? (((x)+Q9_format)>>10):(-((-(x)+Q9_format)>>10)))
#define		round9(x)		((x)>0? (((x)+Q8_format)>>9):(-((-(x)+Q8_format)>>9)))
#define		round8(x)		((x)>0? (((x)+Q7_format)>>8):(-((-(x)+Q7_format)>>8)))
#define		round7(x)		((x)>0? (((x)+Q6_format)>>7):(-((-(x)+Q6_format)>>7)))
#define		round6(x)		((x)>0? (((x)+Q5_format)>>6):(-((-(x)+Q5_format)>>6)))
#define		round5(x)		((x)>0? (((x)+Q4_format)>>5):(-((-(x)+Q4_format)>>5)))
#define		round4(x)		((x)>0? (((x)+Q3_format)>>4):(-((-(x)+Q3_format)>>4)))
#define		round3(x)		((x)>0? (((x)+Q2_format)>>3):(-((-(x)+Q2_format)>>3)))
#define		round2(x)		((x)>0? (((x)+Q1_format)>>2):(-((-(x)+Q1_format)>>2)))
#define		round1(x)		((x)>0? (((x)+Q0_format)>>1):(-((-(x)+Q0_format)>>1)))
#endif

//#define		round(x)		(x)>0? ((x)+0.5):((x)-0.5)
#define		round(x)		( (x) > 0 ) ? ( ((x<<1) + 1)>>1 ):( ((x<<1) - 1)>>1 )	
#define 	CEILING(x) 	(int)(x) + (1 - (int)((int)((x) + 1) - (x)))	

#define 	coef0001 		1678 			//(int)(0.0001*Q24_format + 0.5)			//0.0001*Q24_format; %Q1.24, 1677.7
#define 	coef03 	 		503316		//(int)(0.03*Q24_format + 0.5)				//0.03*Q24_format;           503316.48
#define 	coefP7 	 		11744051 	//(int)(Q24_format*0.7 + 0.5)					//Q1.15
#define 	coefP3 	 		5033165		//(int)(Q24_format*0.3 + 0.5)					//Q1.15

#define		coefP1_Q20	104858		//(int)(0.1*Q20_format + 0.5)		
#define		coefP99_Q20	1038090 	//(int)(0.99*Q20_format + 0.5)

#define		coefP1_Q15	3277  		//(int)(0.1*Q15_format + 0.5)
#define		coefP9_Q15	29491 		//(int)(0.9*Q15_format + 0.5)	

#define 	ss					1434  		//(int)((0.35/8.0)*Q15_format + 0.5) //((0.35/M)*Q_Format)			//Q1.15
#define 	ss_1 	 			31334 		//(int)(Q15_format - ss)

#ifdef INT_MODE	
	#define   MACI(x,y)		((__int64)(x)*(y))
	#define		SHIFT(x,n)		(x)>0? ((x)>>(n)):(-((-(x))>>(n)))	
#endif


typedef struct 
{
	short 				*c;
	unsigned int 	MM;
} fir_information;

typedef struct
{
	short 					u_delay_line[256+150+2];
	short						d_delay_line[256+150+2];
	fir_information	*fir_info;
} fir_filter;

typedef struct
{
		long long 	trigger_AEC_power_thd;
	
		short nlp_min_gain;
		short	nlp_max_gain;
		short	nlp_step_gain;
		short	nlp_thd;
		unsigned short sample_rate;
	
		//int	nr_alpha;
		//int	nr_beta;
		//int	nr_UThr;
		//int	nr_DThr;
	
		//float 	ng_a;
		int 	ng_enable;
		int 	ng_ltrhold;
		int 	ng_utrhold;
		int 	ng_rel;
		int 	ng_att;
		//float 	ng1_a;
		int 	ng_inv_rel;
		int 	ng_inv_att;
		int 	ng_ht;

    //int		aec_v_bt;
    //int 	aec_v1u;     	//0.5*32768
    //int		aec_v2u;     	//0.25*32768
	 	//int 	aec_m_lk;
	 	//int 	aec_c_av10;
	 	//int 	aec_c_av11;
	 	//int 	aec_c_av20;
	 	//int 	aec_c_av21;
	 	//int 	aec_c_Dv10;
	 	//int 	aec_c_Dv11;
	 	//int 	aec_c_Dv20;
	 	//int 	aec_c_Dv21;
	 	int 	aec_RER_Thr;
	 	//float	aec_prop;
	 	short	log;
	 	short farend_RShift;
	 	short aec_preprocess_enable;
 	  short aec_switch;
}aec_parameter;

typedef struct
{
		//pthread_mutex_t buf_mutex;
		//short 	far_delay_line[AEC_FAR_BUF_LEN];
		short 			sync_pattern[8];
  	short 			get_delay;
  	short 			insert_sync_pattern;
  	int					far_delay_buf_length;
		int 				far_delay_buf_read_out_len;
		int 				farend_delay_line_sample_thd;
		int 				farend_delay_line_sample_num;
		short 			blk_size_byte;
		short 			blk_size_word;
		int 				power_on_AEC;
		long long 	power_accu;
   	long long   trigger_AEC_power_thd;// = 	200*200*256;
   	//short	reset_aec;
   	short				log;
		short				underrun;
		short				overrun;
    short				align_word_dummy_data;
    aec_parameter *aec_p;
} aec_func_control;

typedef struct
{		
	
    // pthread_mutex_t buf_mutex;
    int 	remote_audio_data_length;
    int 	DAC_remote_samples_cnt;
    int 	AEC_remote_samples_cnt;
    char	*DAC_read_ptr;
    char	*AEC_read_ptr;
    char    *write_ptr;
    //char    remote_audio_data[REMOTE_BUF_LENGTH];	//10K samples
}  remote_buffer_control;

#ifdef FLOATING_MODE 
typedef struct 
{
		int	circ_buf_cnt;
		short 	frame_size ;
		short 	window_size;
		short 	cancel_count;
		short	saturated;
		short	screwed_up;
		unsigned short sampling_rate;	
		int	sum_adapt;
		int	spec_average;
		int	beta0;
		int	beta_max;
		int	leak_estimate;
		int		e[N];
		int		x[N];
		int		input[FSZ];
		int		y[N];
		//double	last_y[512];
		int		Yf[N3P1];
		int		Rf[N3P1];
		int		Xf[N3P1];		//Q40
		int		Yh[N3P1];
		int		Eh[N3P1];
#if 1
		complex_int	X[M+1][N3P1];
		complex_int	Y[N3P1];
		complex_int	E[N3P1];
		complex_int	W[M][N3P1];
		complex_int	foreground[M][N3P1];
#else
		complex_int	X[M+1][N];
		complex_int	Y[FSZP1];
		complex_int	E[FSZP1];
		complex_int	W[M][N];
		complex_int	foreground[M][N];
#endif
		//complex_int	PHI[N];					
		int	power[N3P1];
		int	power_1[N3P1];
		int	window[N];
		int	prop[M];
    int	wtmp[N];
    int	memX;
    int	memD;
    int	memE;
    int	preemph;
    int	notch_radius;
    int den2;
    int	notch_mem[2];
    int	notch_memf[2];
    int	adapted;
    int	Pey;
    int	Pyy;
    __int64	Davg1;
    __int64	Davg2;
    __int64	Dvar1;
    __int64	Dvar2;
		int AvgRER[2];
    
    //--------- convert constant to variable ----------
    int		VAR_BACKTRACK;
    int		VAR1_UPDATE;
    int		VAR2_UPDATE;
    int 	MIN_LEAK;
    int 	c_avg1_0;
    int 	c_avg1_1; 
    int 	c_avg2_0;
    int 	c_avg2_1;   
    int 	c_Dvar1_0;
    int 	c_Dvar1_1;
    int 	c_Dvar2_0;
    int 	c_Dvar2_1;
    int 	RER_Thr;
    //----------- end -------------------------------------
	
    //cfft_info *cfft_bw, *cfft_fw;
		Nlp_int *nlp;
		//nr_s	*nr;	
		noise_gating *ng;
		//noise_gating *ng1;
		fir_filter *fir_bp;
		SpeexEchoState *st;
   	SpeexPreprocessState *den;
}  aec_st_int;
#endif

typedef struct 
{
		Nlp_int 							*nlp;
		noise_gating 					*ng;
		//noise_gating 					*ng1;
   	SpeexEchoState 				*st;
   	SpeexPreprocessState 	*den;	
}  aec_st_int;


/*
int init_aec(aec_st_int **st_int_, aec_func_control **aec_fc_, unsigned short sample_rate);
void destory_aec(aec_st_int *st_int, aec_func_control *aec_fc);
void put_remote_samples_into_buf( aec_func_control *aec_fc, remote_buffer_control *remote_buf_ctl, char *client_message, int read_size);
void main_aec_process(aec_st_int *st_int, aec_func_control *aec_fc, short *bs_buf) ;
*/

//unsigned long long udivdi3(unsigned long long u, unsigned long long v);
//long long __divdi3(long long num, long long den);
//unsigned long gm_fxsqrt(unsigned long long x);

void init_aec_parameters(aec_parameter *aec_p);
int init_aec(aec_st_int **st_int_, aec_func_control *aec_fc_, aec_parameter *aec_p);
void destory_aec(aec_st_int *st_int);

aec_st_int* init_st_int(aec_parameter *aec_p);

//--- come from aec_r.h ---//
//void mdf_adjust_prop_int(complex_int W[M][N3P1], int prop[M]);
//__int64 nor(__int64 x, int nor_bit);
//int speex_echo_cancellation_mdf(aec_st_int *st_int, aec_func_control *aec_fc, short in16[256], short far_end16[256], short out_int[256]); 
//aec_st_int* speex_echo_state_init_mc_mdf(aec_parameter *aec_p);
void ms_aec_process(aec_st_int *st_int, short *tRxIn, short *tTxIn, short *tRxOut, int RER_Thr, int ng_enable, int farend_RShift);

void re_init_mc_mdf(aec_st_int *st_int, aec_parameter *aec_p);
//void filter_dc_notch16_int(short *in,  int radius, int den2, int mem[2], short *out);
//void fir_bandpass(short *input, short *delay_line, fir_information *fir_info);


//void put_remote_samples_into_buf( aec_func_control *aec_fc, remote_buffer_control *remote_buf_ctl, char *client_message, int read_size);
//int main_aec_process(void *st_int, aec_func_control *aec_fc, short *remote_sample_ptr,int aec_remote_samples_cnt, short *bs_buf);

void re_init_aec(aec_st_int *st_int, aec_func_control *aec_fc_, aec_parameter *aec_p);

void ng_init_update_parameters(noise_gating	*ng, aec_parameter *aec_p);
void nlp_init_update_parameters(Nlp_int	*nlp, aec_parameter *aec_p);
void display_update_parameters(aec_st_int *st_int);
#endif
