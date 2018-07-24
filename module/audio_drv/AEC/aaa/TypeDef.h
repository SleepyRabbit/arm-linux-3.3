#ifndef NLP_TYPE
#define NLP_TYPE

typedef short int  Word16 ;
typedef long int   Word32 ;

//typedef long long __int64;

#define _round(x)   (Word32) (x < 0 ? (x - 0.5) : (x + 0.5));
#define _round15(x)  (Word32) (x < 0 ? (x - 16384) : (x + 16384));
#define abs_p(x)   (Word32) (x < 0 ? (-x) : (x));


typedef struct NLP_structure{
	Word16   u_frame_Q0[128];
	//Word16   d_frame_Q0[128]; 
	Word16   out_frame_Q0[128];
	Word16   mola_frame_Q0[128] ;
	Word16   Gkm;
	Word16   dummy_nlp;
	// NLP parameters
	Word16   step_gain, min_gain, max_gain, thd ;
}Nlp_int;

void NLP_power(Nlp_int *nlp);
void ashiftR_32(Word32 *rst, Word16 num);
#endif
