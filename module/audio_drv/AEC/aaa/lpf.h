#ifndef LOWPASS_FILTER
#define LOWPASS_FILTER

#define ENABLE_LOWPASS_FILTER 1

typedef struct {
	short b_dly_s0[2];
	short a_dly_s0[2];
	short b_dly_s1[2];
	short a_dly_s1[2];
	short b_dly_s2[2];
	short a_dly_s2[2];
	short b_dly_s3[2];
	short a_dly_s3[2];	
} lp_dely_line;

void lpf(short *lp_input, lp_dely_line *lp, int size);
#endif
