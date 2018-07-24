#ifdef LINUX
#include <linux/kernel.h>
#include <linux/string.h>
#endif
#include "bitstream.h"
#include "define.h"

/*
 * Advance cur_byte n bytes and return
 * NOTE: 
 *   'emulation prevention byte' is not handled. This function should be used only at finding start code
 */
static __inline unsigned char *adv_cur_byte(Bitstream *stream, unsigned char *cur_byte, int n)
{
    if(cur_byte == &(stream->empty_byte)){
        return cur_byte;
    }
    cur_byte += n;
    if(cur_byte >= stream->streamBuffer_end){
        return &(stream->empty_byte);
    }    
    return cur_byte;
}


/*
 * Advance cur_byte one byte and return
 */
static __inline unsigned char *adv_cur_byte_one(Bitstream *stream, unsigned char *cur_byte)
{
    if(cur_byte == &(stream->empty_byte)){
        return cur_byte;
    }

next_byte:    
    cur_byte += 1;
    if(cur_byte >= stream->streamBuffer_end){
        return &(stream->empty_byte);
    }
    
    /* handle emulation prevention byte */
    if(*cur_byte == 3){
        if((cur_byte - stream->streamBuffer) >= 3 && *(cur_byte-1) == 0 && *(cur_byte-2) == 0){
            stream->escape_byte_cnt++;
#if DBG_SW_PARSER
            printk("escape_byte_cnt:%d\n", stream->escape_byte_cnt);
#endif
            goto next_byte;
        }
    }

    return cur_byte;
}


/*
 * Get the next one bit (emulation preventation byte is skipped)
 */
static __inline int sw_next_bit(Bitstream *stream)
{
    int value = ((*stream->cur_byte) >> (stream->bitOffset--)) & 0x01;
    if (stream->bitOffset < 0) {
        stream->bitOffset = 7;
        stream->cur_byte = adv_cur_byte_one(stream, stream->cur_byte);
    }
    return value;
}


/*
 * Init the sw parser (Bitstream) with a buffer
 */
int init_bitstream(Bitstream *stream, unsigned char *buffer, int length)
{
    stream->streamBuffer = buffer;
    stream->totalLength = length;
    stream->bitOffset = 7;
    stream->cur_byte = stream->streamBuffer;

    stream->streamBuffer_end = stream->streamBuffer + length;
    stream->empty_byte = 0;
    stream->escape_byte_cnt = 0;
    stream->err_flg = 0;

#if DBG_SW_PARSER
    printk("init_bitstream: %08X len:%d\n", (unsigned int)stream->streamBuffer, p_Dec->stream.totalLength);
#endif /* DBG_SW_PARSER */

    return 0;
}


/*
 * count the number of 'emulation prevention byte' within the range 
 * from offset_start to offset_end
 */
int count_emulation_prevent_byte(unsigned char *buf, int offset_start, int offset_end)
{
    int emulation_bytes = 0;
    int zero_cnt = 0;
    int i;
    
    for(i = offset_start; i < offset_end; i++){
        if(buf[i] == 0){
            zero_cnt++;
            continue;
        }
        if(buf[i] == 3 && zero_cnt >= 2){
            emulation_bytes++;
            zero_cnt = 0;
            //i++;
#if DBG_SW_PARSER
            printk("0x000003 found in remain bytes: %d\n", i);
#endif
            continue;
        }
        zero_cnt = 0; /* count for consecutive bytes only */
    }
#if DBG_SW_PARSER
    if(emulation_bytes){
        printk("found %d emulation prevention byte in the range: %d~%d\n", emulation_bytes, offset_start, offset_end);
        for(i = offset_start; i < offset_end; i++){
            printk("byte[%d]:%02X\n", i, buf[i]);
        }
    }
#endif
    
    return emulation_bytes;
}


/*
 * Dump parser data/status
 */
void sw_parser_dump(Bitstream *stream)
{
    int i;
    unsigned char *cur_byte = stream->cur_byte;
    int end_pos, start_pos;
    int n;
    printk("    streamBuffer:%08X\n", (unsigned int)stream->streamBuffer);
    printk("streamBuffer_end:%08X\n", (unsigned int)stream->streamBuffer_end);
    printk("        cur_byte:%08X\n", (unsigned int)stream->cur_byte);
    printk("  cur_byte value:%02X\n", *stream->cur_byte);
    printk("sw_show_bits(32):%08X\n", sw_show_bits(stream, 32));
    printk(" sw_bit_offset():%d\n", sw_bit_offset(stream));
    printk("       bitOffset:%d\n", stream->bitOffset);
    printk("     totalLength:%d\n", stream->totalLength);
    printk(" escape_byte_cnt:%d\n", stream->escape_byte_cnt);
    printk("         err_flg:%d\n", stream->err_flg);

    if(sw_bs_emtpy(stream)){
        cur_byte = stream->streamBuffer_end - 1;
        printk("cur_byte reaches end\n");
    }

    n = 4;
    /* dump the first n bytes */
    end_pos = stream->totalLength;
    if(end_pos > n){
        end_pos = n;
    }
    for(i = 0; i < end_pos; i++){
        printk("byte[%d]:%02X\n", i, stream->streamBuffer[i]);
    }

    /* dump n bytes before cur_byte */
    end_pos = cur_byte - stream->streamBuffer;
    printk("cur_byte pos:%d\n", end_pos);

    start_pos = end_pos - n;
    if(start_pos < 0){
        start_pos = 0;
    }
    
    for(i = start_pos; i <= end_pos; i++){
        printk("byte[%d]:%02X\n", i, stream->streamBuffer[i]);
    }
}


/*
 * Advance read pointer to byte aligned address
 */
int sw_byte_align(Bitstream *stream)
{
    if (stream->bitOffset != 7) {
        stream->bitOffset = 7;
        stream->cur_byte = adv_cur_byte_one(stream, stream->cur_byte);
    }
    return 0;
}


/*
 * Advance read pointer n bytes
 * NOTE: 
 *   'emulation prevention byte' is not handled. This function should be used only at finding start code
 */
int sw_skip_byte(Bitstream *stream, int skipByteNum)
{
    stream->cur_byte = adv_cur_byte(stream, stream->cur_byte, skipByteNum);
    return 0;
}


/*
 * Skip n bits of read pointer
 */
int sw_skip_bits(Bitstream *stream, int skipBitNum)
{
    while (skipBitNum--) {
        stream->bitOffset--;
        if (stream->bitOffset < 0) {
            stream->bitOffset = 7;
            stream->cur_byte = adv_cur_byte_one(stream, stream->cur_byte);
        }
    }
    return 0;
}


/*
 * Get n bits
 */
unsigned int sw_read_bits(Bitstream *stream, int nbits)
{
    int bitcounter = nbits;
    int inf = 0;

    while (bitcounter--) {
        inf = (inf << 1) | sw_next_bit(stream);
    }
#if DBG_SW_PARSER
    printk("read_u%d:%d\n", nbits, inf);
#endif
    return inf;
}


/*
 * seek read pointer to the next start code
 * (seek to end of stream if not found)
 */
int sw_seek_to_next_start_code(Bitstream *stream)
{
    unsigned char *cur_byte;
    unsigned char *end_byte;
	sw_byte_align(stream);

    if(sw_bs_emtpy(stream)){
        return -1;
    }

    cur_byte = stream->cur_byte;
    end_byte = stream->streamBuffer_end - 3;

    while(cur_byte <= end_byte){
        if(cur_byte[0] == 0 && 
           cur_byte[1] == 0 &&
           cur_byte[2] == 1)
        {
            /* found */
            stream->cur_byte = cur_byte;
            return 0;
        }
        if(cur_byte[2] == 0){
            cur_byte += 1;
        }else{
            cur_byte += 3;
        }
    }

    
    /* not found, seek to end of stream */
    stream->cur_byte = &(stream->empty_byte);
    return -1;
}


/*
 * Peek the value of the next 3 bytes
 * NOTE:
 *   'emulation prevention byte' is not handled. This function should be used only at finding start code
 */
#if 0
unsigned int sw_show_3byte(Bitstream *stream)
{
    unsigned char  *curbyte  = stream->cur_byte;
    int inf        = 0;
#if 0
	/* this may failed in the bistream empty case */
    if(stream->streamBuffer_end - curbyte >= 3){
        inf = (curbyte[0] << 16) |
              (curbyte[1] << 8)  |
               curbyte[2];
    }else
#endif
    {
        inf = *curbyte << 16;
        curbyte = adv_cur_byte(stream, curbyte, 1);
        inf |= *curbyte << 8;
        curbyte = adv_cur_byte(stream, curbyte, 1);
        inf |= *curbyte;
    }
#if DBG_SW_PARSER
    //printk("sw_show_3byte:%06X\n", inf);
#endif
    return inf;
}
#endif

/*
 * Peek the next n bits
 */
unsigned int sw_show_bits(Bitstream *stream, int nbits)
{
    int bitoffset  = stream->bitOffset;
    unsigned char  *curbyte  = stream->cur_byte;
    int inf        = 0;

    while (nbits--) {
        inf <<=1;    
        inf |= ((*curbyte) >> (bitoffset--)) & 0x01;

        if (bitoffset == -1 ) { //Move onto next byte to get all of numbits
            curbyte = adv_cur_byte_one(stream, curbyte);
            bitoffset = 7;
        }
    }
#if DBG_SW_PARSER
    printk("show_bits:%d\n", inf);
#endif
    return inf;
}


/*
 * Read ue
 */
unsigned int sw_read_ue(Bitstream *stream)
{
    int bitcounter = 1;
    int len = 0;
    int ctr_bit;
    int inf = 0;
    int value;

    ctr_bit = sw_next_bit(stream);

    while (ctr_bit == 0) {  // find leading 1 bit
        if(sw_bs_emtpy(stream)){
            stream->err_flg++;
            break;
        }
        
        len++;
        bitcounter++;
        ctr_bit = sw_next_bit(stream);
    }

    while (len--) {
        inf = (inf << 1) | sw_next_bit(stream);
        bitcounter++;
    }

    value = (int) (((unsigned int) 1 << (bitcounter >> 1)) + (unsigned int) (inf) - 1);

#if DBG_SW_PARSER
    printk("read_ue:%d\n", value);
#endif

    return value; 
}


/*
 * Read se
 */
int sw_read_se(Bitstream *stream)
{
    int bitcounter = 1;
    int len = 0;
    int ctr_bit;// = (stream->curByte >> stream->bitOffset) & 0x01;  // control bit for current bit posision
    int inf = 0;
    int value;
    unsigned int n;

    ctr_bit = sw_next_bit(stream);

    while (ctr_bit == 0) {  // find leading 1 bit
        if(sw_bs_emtpy(stream)){
            stream->err_flg++;
            break;
        }
        
        len++;
        bitcounter++;
        ctr_bit = sw_next_bit(stream);
    }

    while (len--) {
        inf = (inf << 1) | sw_next_bit(stream);
        bitcounter++;
    }

    n = ((unsigned int) 1 << (bitcounter >> 1)) + (unsigned int) inf - 1;
    value = (n + 1) >> 1;
    if ((n & 0x01) == 0)
        value = -value;
#if DBG_SW_PARSER
    printk("read_se:%d\n", value);
#endif
    return value;
}


/*
 * Return the current bit offset (from 0 to 7. the first bit offset of a byte is 0)
 */
int sw_bit_offset(Bitstream *stream)
{
    return (7 - stream->bitOffset);
}


/*
 * Return the current byte offset
 */
int sw_byte_offset(Bitstream *stream)
{
    if(sw_bs_emtpy(stream)){
        return stream->totalLength;
    }
    return (stream->cur_byte - stream->streamBuffer);
}


/*
 * Return whether bitstream is out (read pointer reaches the end of bitstream)
 */
int sw_bs_emtpy(Bitstream *stream)
{
    if(stream->cur_byte == &(stream->empty_byte)){
        return 1;
    }
    if(stream->cur_byte >= stream->streamBuffer_end){
        return 1;
    }

    /* reach the last bit of last byte */
    if(stream->cur_byte == (stream->streamBuffer_end - 1) && stream->bitOffset == 0){
        return 1;
    }
    return 0;
}



