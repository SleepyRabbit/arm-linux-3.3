#ifndef _BITSTREAM_H_
#define _BITSTREAM_H_

typedef struct bitstream_t
{
    unsigned char *streamBuffer;      /* pointer to bitstream buffer */
    int            totalLength;       /* length of bitstream buffer in bytes */
    int            bitOffset;         /* current bit offset within a byte of read pointer. 0~7 NOTE: not reliable at when sw_bs_emtpy() is true */
    unsigned char *cur_byte;          /* current byte offset of read pointer */
    unsigned char *streamBuffer_end;  /* point to streamBuffer + totalLength */
    unsigned char  empty_byte;        /* a zero-value byte for cur_byte to point to */
    unsigned char  escape_byte_cnt;   /* counter of escape byte */
    unsigned char  err_flg;           /* error flag */
} Bitstream;

int init_bitstream(Bitstream *stream, unsigned char *buffer, int length);
int count_emulation_prevent_byte(unsigned char *buf, int offset_start, int offset_end);
int sw_byte_align(Bitstream *stream);
int sw_skip_bits(Bitstream *stream, int skipBitNum);
int sw_skip_byte(Bitstream *stream, int skipByteNum);
unsigned int sw_read_bits(Bitstream *stream, int nbits);
unsigned int sw_show_bits(Bitstream *stream, int nbits);
//unsigned int sw_show_3byte(Bitstream *stream); /* for matching start code */
unsigned int sw_read_ue(Bitstream *stream);
int sw_read_se(Bitstream *stream);
int sw_bs_emtpy(Bitstream *stream);
int sw_bit_offset(Bitstream *stream);
int sw_byte_offset(Bitstream *stream);
void sw_parser_dump(Bitstream *stream);
int sw_seek_to_next_start_code(Bitstream *stream);


#endif /* _BITSTREAM_H_ */

