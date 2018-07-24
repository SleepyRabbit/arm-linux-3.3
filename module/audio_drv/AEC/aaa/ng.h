#ifndef NOISE_GATING
#define NOISE_GATING

#define NG_MIN_GAIN 8192
typedef struct 
{
	int b;
	int a[2];	
	int y_dly[2];
} filter;

typedef struct 
{
	int Fs;
	unsigned short g;
	unsigned short g_prev;
	int	first_time_ng;
	//unsigned short g1;
	//int holdtime;
	//int release;
	//int attack;
	float a;
	int ltrhold;
	int utrhold;
	int rel;
	int inv_rel;
	int att;
	int inv_att;
	int lthcnt;
	int uthcnt;
	int ht;
	unsigned int sample_cnt;
	filter *epd;
} noise_gating;

void noise_gate(short  *samples, noise_gating *ng);
#endif