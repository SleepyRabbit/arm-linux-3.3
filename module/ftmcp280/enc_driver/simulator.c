#define SIMULATOR_GOLBAL
#include <stdarg.h>
//#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "define.h"
#include "portab.h"
#include "simulator.h"

typedef struct {
	uint32_t orginal;		// 0
	uint32_t p0;			// 4
	uint32_t p1;			// 8
	uint32_t p2;			// c
	uint32_t cmd_status;	// 10
	uint32_t p3;				// only for rtl
} vpe_s;

#define vpes ((volatile vpe_s *)0x90180000)

uint32_t change_base (uint32_t address)
{
//	#ifdef HOST
		return address;
//	#else
//		#ifdef RISC0
//			return address+0x90700000L;
//		#else
//			return address+0x90710000L;
//		#endif
//	#endif
}

#if 0
void my_fprints(int handler, const char *fmt, ...);
#endif
int vpeOpen(char *filename, char * mode)
{
	int handler;
	vpes->p0 = change_base((uint32_t)filename);
	vpes->p1 = change_base((uint32_t)mode);
	vpes->cmd_status = 0;
	handler = vpes->cmd_status;
#if 0
	if (handler == 0)
		my_fprint("open filename: %s, mode: %s  .....fail\n.", filename, mode);
	else
		my_fprint("open filename: %s, mode: %s  ......handle = 0x%x)\n", filename, mode, handler);
#endif
	return handler;
}


int vpeRead(int handler, void * buf, uint32_t size, int swap_endian)
{
	vpes->p0 = (uint32_t)handler;
	vpes->p1 = change_base((uint32_t)buf);
	vpes->p2 = (uint32_t)((swap_endian << 31)|size);
	vpes->cmd_status = 1;
	return vpes->cmd_status;
}
int vpeWrite(int handler, void * buf, uint32_t size)
{
	vpes->p0 = (uint32_t)handler;
	vpes->p1 = change_base((uint32_t)buf);
	vpes->p2 = size;
	vpes->cmd_status = 2;
	return vpes->cmd_status;
}
void vpeClose(int handler)
{
	vpes->p0 = (uint32_t)handler;
	vpes->cmd_status = 5;
}

//===============================================
//===============================================
//#ifdef HOST
void vpePass(void)
{
	vpes->orginal =  0x01234568;
}
void vpeFail(void)
{
	vpes->orginal = 0x01234569;
}
void vpeFinish(void)
{
	vpes->orginal = 0x01234567;
}
void vpeSet(uint32_t val)
{
	vpes->orginal = val;
}

int vpeComp(uint8_t * buffer, uint8_t * bufferg, int size)
{
	vpes->p0 = change_base((uint32_t)buffer);
	vpes->p1 = change_base((uint32_t)bufferg);
	vpes->p2 = size;
	vpes->cmd_status = 3;
	return vpes->cmd_status;
}


int LoadFrame(void *buf, int size)
{
	vpes->p1 = (uint32_t)buf;
	vpes->p2 = (uint32_t)size;
	vpes->orginal = 0x70000000;
	return 0;
}

int LoadConfigure(void *buf, int word_num)
{
	vpes->p1 = (uint32_t)buf;
	vpes->p2 = (uint32_t)word_num*4;
	vpes->orginal = 0x70000004;
	return 0;
}

int OpenSrcGld(void)
{
	vpes->orginal = 0x70000001;
	return 0;
}

//int gld_read_byte = 0;
int CompareGolden(void * buf, int byte_size)
{
	vpes->cmd_status = (uint32_t)buf;
	vpes->p3 = byte_size;
	vpes->orginal = 0x70000002;
	return 0;
}

int SaveBuffer(char *filename, void *buf, int byte_size)
{
	int i, pos = 24, len = strlen(filename);
	uint32_t tmp = 0;

	vpes->orginal = 0xDDDD1111;	// start fill filename
	for (i = 0; i<len; i++) {
		tmp |= filename[i]<<pos;
		pos -= 8;
		if (pos < 0) {
			vpes->p0 = tmp;
			pos = 24;
			tmp = 0;
		}
	}
	vpes->orginal = 0xDDDDEEEE;	// end of fill filename

	vpes->cmd_status = (uint32_t)buf;
	vpes->p3 = byte_size;
	vpes->orginal = 0x70000003;
	return 0;
}

//#ifndef RTL
//uint8_t xlate_big[] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
uint8_t xlate_big[] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
/*
0 = standard input
1 = standard output
2 = standard error
*/
/*
int curHandler = 0;
static void out_str(char *buf, int size)
{
	if (curHandler == 1) {	// standard out
		#ifdef HOST
			*((volatile char **)0x9073a000) = buf;
		#else
			#ifdef RISC0
				*((volatile char **)0x3a000) = (char *)((int)buf+0x90700000L);
			#else
				*((volatile char **)0x3a000) = (char *)((int)buf+0x90710000L);
			#endif
		#endif
	}
	else
		vpeWrite(curHandler, buf, size);
}
static void _putch(char c)
{
	static int stn = 0;	// character stored
	static char buffer[0x40];
	buffer[stn++] = c;
	if (c == '\0') {	// flush	
		if (stn != 1)
			out_str(buffer, stn-1);
		stn = 0;
	}
	else if (stn == sizeof (buffer)) {	// buffer almost full
		out_str(buffer, stn);
		stn = 0;
	}	
}
static void phex(int bitcnt, uint32_t val)
{
	int i;
	int pr = 0;
//	int pr = 1;
	char c;

	if (bitcnt & BIT31) {
		bitcnt &= ~BIT31;
		pr = 1;
	}
	for(i = bitcnt - 4; i >=0; i -= 4) {
		c = (val >> i) & 0xF;
		if(c || pr) {
			pr = 1;
			_putch(xlate_big[c]);
		}
	}
	if (pr ==0)
		_putch('0');
}
void my_fprints(int handler, const char *fmt, ...)
{
//	const char *s;
	va_list ap;

	curHandler = handler;
	va_start(ap, fmt);
	while (*fmt) {
		if (*fmt != '%') {
			_putch(*fmt++);
			continue;
        	}
		switch (*++fmt) {
#if 0
			case 'c':
				_putch(va_arg(ap, int));
				break;
			case 's':
				s = va_arg(ap, const char *);
				for ( ; *s; s++)
					_putch(*s);
				_putch ('\0');
				break;
#endif
			case '0':
				if ((*(fmt + 1) == 'x') || (*(fmt + 1) == 'X')) {
					fmt++;
					phex(BIT31 | 32, va_arg(ap, int));
				}
				break;
			case 'x':
			case 'X':
				phex(32, va_arg(ap, int));
				break;
			case 'b':
				#if 1
				phex(8, va_arg(ap, char));
				#else
				if ((*(fmt + 1) == 'x') || (*(fmt + 1) == 'X')) {
					fmt++;
					phex(8, va_arg(ap, char));
				}
				else
					_putch('b');
				#endif
				break;
#if 0
			case 'l':
				if ((*(fmt + 1) == 'x') || (*(fmt + 1) == 'X')) {
					fmt++;
					phex(64, va_arg(ap, long));
				}
				else
					_putch('l');
				break;
#endif
			// Add other specifiers here... 
			default:
				_putch(*fmt);
				break;
        	}
        	fmt++;
	}
	_putch ('\0');
	va_end(ap);
}
#endif
*/
