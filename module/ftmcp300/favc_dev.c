#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/mm.h> // for PAGE_ALIGN
#include <mach/ftpmu010.h> /* for PMU */

#include "frammap_if.h"

#include "dec_driver/define.h"
#include "favc_dev.h"
#include "h264d_ioctl.h"

#define RSVD_SZ     0x8 // reserved bytes per-allocationg (for recording allocated size, and maintain N-byte alignment)

/*
 * start of frammap wrapper macros
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0))
#define INIT_FRAMMAP_INFO(buf_info, _size, _type, _name) \
    do {                                            \
        memset(&buf_info, 0, sizeof(buf_info));     \
        buf_info.size = _size;                      \
        buf_info.alloc_type = _type;                \
        buf_info.name = _name;                      \
    } while(0)
#else
#define INIT_FRAMMAP_INFO(buf_info, _size, _type, _name) \
    do {                                            \
        memset(&buf_info, 0, sizeof(buf_info));     \
        buf_info.size = _size;                      \
        buf_info.alloc_type = _type;                \
    } while(0)
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0))
#define __frm_free_buf_ddr(va, pa)  frm_free_buf_ddr((void *)va)
#else
__inline static int __frm_free_buf_ddr(unsigned int va, unsigned int pa) {
    struct frammap_buf_info buf_info;
    buf_info.va_addr = va;
    buf_info.phy_addr = pa;
    return frm_free_buf_ddr(&buf_info);
}
#endif
#define FREE_FRAMMAP_INFO(va, pa) __frm_free_buf_ddr(va, pa)

/*
 * end of frammap wrapper macros
 */


/*
 * module parameters
 */
unsigned int mcp300_max_width = MAX_DEFAULT_WIDTH;
module_param(mcp300_max_width,  uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(mcp300_max_width,  "Max Width");

unsigned int mcp300_max_height = MAX_DEFAULT_HEIGHT;
module_param(mcp300_max_height, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(mcp300_max_height, "Max Height");

unsigned int mcp300_sw_reset = 0;
module_param(mcp300_sw_reset,   uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(mcp300_sw_reset,   "whether to enable sw reset");

unsigned int mcp300_dev_reg = 0;
module_param(mcp300_dev_reg,  uint, S_IRUGO);
MODULE_PARM_DESC(mcp300_dev_reg,  "register driver bitmask. (bit N: engine N device registered)");

unsigned int mcp300_engine_en = (1 << FTMCP300_NUM) - 1;
module_param(mcp300_engine_en,  uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(mcp300_engine_en,  "enabled engine bitmask. (bit N: engine N enable) NOTE:can be set at module loading only");

#if USE_SW_PARSER == 0
unsigned int pad_bytes = 5;
#else
unsigned int pad_bytes = 0;
#endif
module_param(pad_bytes,  uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(pad_bytes,  "padded bytes after each input bitstream");

unsigned int bs_padding_size = PADDING_SIZE;
module_param(bs_padding_size, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(bs_padding_size, "bitstream buffer padding size");

unsigned int memtest = 0; /* for testing memory on FPGA board of A369 */
module_param(memtest,  uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(memtest,  "whether to run memory test");

int log_level = 0; /* larger level, more message */
module_param(log_level, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(log_level,  "debug message level");

int dbg_mode = 0; /* to control whether to call damnit at error */
module_param(dbg_mode, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(dbg_mode,  "debug mode");

unsigned int hw_timeout_delay = 0;
module_param(hw_timeout_delay, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(hw_timeout_delay,  "hardware timeout delay");

int blk_read_dis = BLOCK_READ_DISABLE;
module_param(blk_read_dis, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(blk_read_dis, "value for 0x24 bit 21:block read disable");

int sw_reset_timeout_ret = SW_RESET_TIMEOUT_RET;
module_param(sw_reset_timeout_ret, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(sw_reset_timeout_ret, "1: return at sw reset timeout. 0: not return");

int clear_sram = CLEAR_SRAM_AT_INIT;
module_param(clear_sram, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(clear_sram, "whether to fill ref list 0/1 up");

int ddr_id_fr = DDR_ID_SYSTEM;
module_param(ddr_id_fr, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(ddr_id_fr, "ddr id for recon and scale output buffer");

int ddr_id_mbinfo = DDR_ID_SYSTEM;
module_param(ddr_id_mbinfo, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(ddr_id_mbinfo, "ddr id for mbinfo");

int ddr_id_bs = DDR_ID_SYSTEM; /* ddr ID for bs, intra, linked list buffer */
module_param(ddr_id_bs, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(ddr_id_bs, "ddr id for bitstream and other buffers");

unsigned int vc_cfg = (ENABLE_VCACHE_REF << 2)|(ENABLE_VCACHE_SYSINFO << 1)|ENABLE_VCACHE_ILF; /* vcache config */
module_param(vc_cfg, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(vc_cfg, "vcache enable config: bit0: ILF, bit1: SYSINFO, bit2: REF");

int chk_buf_num = 0;
module_param(chk_buf_num, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(chk_buf_num, "whether to compare max buf num and ref num");

int output_buf_num = 16;
module_param(output_buf_num, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(output_buf_num, "number of output buffer");

int dump_start = 0;
module_param(dump_start, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(dump_start, "frame number of dump start");

int dump_end = 0;
module_param(dump_end, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(dump_end, "frame number of dump end");

unsigned int dump_cond = 0;
module_param(dump_cond, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(dump_cond, "dump enable bit mask");

unsigned int sw_timeout = 0;
module_param(sw_timeout, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(sw_timeout, "testing sw timeout enable");

unsigned int ilf_en = 3;
module_param(ilf_en, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(ilf_en, "for testing ilf error");

unsigned int delay_en = 0;
module_param(delay_en, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(delay_en, "for testing ilf error");

unsigned int clear_en = 3;
module_param(clear_en, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(clear_en, "for testing ilf error");

unsigned int sw_timeout_delay = HZ * 3;
module_param(sw_timeout_delay, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(sw_timeout_delay, "software timeout period in msecs");

unsigned int clk_sel = 0;
module_param(clk_sel, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(clk_sel, "select clock source");

unsigned int count_kmalloc_size = 0;
module_param(count_kmalloc_size, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(count_kmalloc_size, "kmalloc size");

unsigned int chip_ver_limit = FTMCP300_CHIP_VER_VAL; /* 0: skip chip version check. != 0: compare PMU CHIP version with this version */
module_param(chip_ver_limit, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(chip_ver_limit, "chip version check value");

unsigned int chip_ver_mask = FTMCP300_CHIP_VER_MASK; /* mask used at comparing chip version */
module_param(chip_ver_mask, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(chip_ver_mask, "chip version mask value");


/* 1 if vcache has only one reference memory, may set ref0 local base according to current frame width 
 * 0 define 0 to avoid changing VCACHE 0x30 (LOCAL_BASE) register on 8210
 */
unsigned int vcache_one_ref = VCACHE_ONE_REFERENCE; 
module_param(vcache_one_ref, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(vcache_one_ref, "vcache has one reference memory");

unsigned int max_ref1_width = MAX_REF1_WIDTH; /* max width to enable vcache ref 1 */
module_param(max_ref1_width, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(max_ref1_width, "max width to enable vcache ref 1");

unsigned int max_level_idc = MAX_LEVEL_IDC; /* MAX supported level. Set this to 51 when testing with 4K2K patterns */
module_param(max_level_idc, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(max_level_idc, "max supported level");

char *plt_str = FTMCP300_PLT_STR;


/*
 * allocate memory for decoder data structure (accessed by CPU only)
 */
void *fkmalloc(size_t size)
{
	unsigned int sz = size + RSVD_SZ;
	unsigned char *ptr;
	
	ptr = kmalloc(sz, GFP_KERNEL);
	
	C_DEBUG("fkmalloc for addr 0x%x size 0x%x count=0x%x\n", (unsigned int)ptr, (unsigned int)size, count_kmalloc_size + sz);
	
	if (ptr==0)
	{
		F_DEBUG("Fail to fkmalloc for addr	0x%x size	0x%x count=0x%x\n",(unsigned int)ptr, (unsigned int)size, count_kmalloc_size);
		return 0;
	}

#if RSVD_SZ >= 4
	*(unsigned int *)ptr = sz;
#endif
	count_kmalloc_size += sz;

	return (ptr + RSVD_SZ);
}


/*
 * free memory for decoder data structure (accessed by CPU only)
 */
void fkfree(void *mem_ptr)
{
	unsigned char *ptr;

	ptr = mem_ptr;
    ptr -= RSVD_SZ;

	if (ptr){
#if RSVD_SZ >= 4
		unsigned int sz = *(unsigned int *)ptr;
		count_kmalloc_size -= sz;
#endif
		C_DEBUG("fkfree	for	addr 0x%x	count=0x%x\n", (unsigned int)ptr, count_kmalloc_size);
		kfree(ptr);
	}
}


/*
 * allocate DMA-able memroy via frammap
 */
int allocate_dec_buffer(unsigned int *addr_virt, unsigned int *addr_phy, unsigned int size)
{
	struct frammap_buf_info buf_info;
	int size_aligned = 0;
    int ret;

	if (size == 0) {
		printk("allocate buffer size = 0\n");
		return -1;
    }
    
    size_aligned = PAGE_ALIGN(size);
    INIT_FRAMMAP_INFO(buf_info, size_aligned, ALLOC_NONCACHABLE, "aldecb");
    ret = frm_get_buf_ddr(ddr_id_bs, &buf_info);
	if (ret < 0 || buf_info.va_addr == 0) { 
		printk("allocate buffer error\n");
		return -1;
	}

    *addr_virt = (unsigned int)buf_info.va_addr;
    *addr_phy = (unsigned int)buf_info.phy_addr;
    
	return 0;
}


/*
 * free DMA-able memroy via frammap
 */
void release_dec_buffer(unsigned int addr_virt, unsigned int addr_phy)
{
    if(FREE_FRAMMAP_INFO(addr_virt, addr_phy) < 0){
        printk("free buffer error\n");
    }
}


/*
 * free buffers associated to a frame
 */
int release_frame_buffer(MCP300_Buffer *pBuf)
{
    if(pBuf->ref_va){
        if(FREE_FRAMMAP_INFO(pBuf->ref_va, pBuf->ref_pa)){
            printk("free ref buffer error\n");
        }
        pBuf->ref_va = 0;
    }
    if(pBuf->mbinfo_va){
        if(FREE_FRAMMAP_INFO(pBuf->mbinfo_va, pBuf->mbinfo_pa)){
            printk("free mbinfo buffer error\n");
        }
        pBuf->mbinfo_va = 0;
    }
    return 0;
}

/*
 * allocate buffers associated to a frame
 */
int allocate_frame_buffer(MCP300_Buffer *pBuf, unsigned int Y_sz, enum buf_type_flags buffer_flag)
{
    struct frammap_buf_info buf_info;
    int ref_size_aligned;
    int mbinfo_size_aligned;
    int scale_size_aligned;
    int alloc_scale_flg = 0;

    if (Y_sz == 0) {
        printk("allocate memory buffer size = 0\n");
        return -1;
    }

    memset(pBuf, 0, sizeof(*pBuf));

    /* allocate memory for reconstructed frame (and scale buffer if necessary) */
    ref_size_aligned = PAGE_ALIGN(Y_sz * 2);

    if (buffer_flag & FR_BUF_SCALE){
        alloc_scale_flg = 1;
        scale_size_aligned = PAGE_ALIGN(Y_sz / 2);
    } else {
        alloc_scale_flg = 0;
        scale_size_aligned = 0;
    }

    INIT_FRAMMAP_INFO(buf_info, ref_size_aligned + scale_size_aligned, ALLOC_NONCACHABLE, "aldecb");
    if (frm_get_buf_ddr(ddr_id_fr, &buf_info) >= 0) {
        pBuf->ref_va = (unsigned int)buf_info.va_addr;
        pBuf->ref_pa = (unsigned int)buf_info.phy_addr;
    }
    else {
        printk("allocate recon. buffer error\n");
        goto err_ret;
    }

    if(alloc_scale_flg) { // allocate scale buffer right after reference frame
        pBuf->scale_va = pBuf->ref_va + ref_size_aligned;
        pBuf->scale_pa = pBuf->ref_pa + ref_size_aligned;
    }else{
        pBuf->scale_va = 0;
        pBuf->scale_pa = 0;
    }

    /* allocate memory for MB info */
    if(buffer_flag & FR_BUF_MBINFO){
        mbinfo_size_aligned = PAGE_ALIGN(Y_sz);
        INIT_FRAMMAP_INFO(buf_info, mbinfo_size_aligned, ALLOC_NONCACHABLE, "mbinfo");
        if (frm_get_buf_ddr(ddr_id_mbinfo, &buf_info) >= 0) {
            pBuf->mbinfo_va = (unsigned int)buf_info.va_addr;
            pBuf->mbinfo_pa = (unsigned int)buf_info.phy_addr;
        }
        else {
            printk("allocate mbinfo buffer error\n");
            goto err_ret;
        }
    }else{
        pBuf->mbinfo_va = 0;
        pBuf->mbinfo_pa = 0;
    }

    return 0;

err_ret:

    release_frame_buffer(pBuf);
    memset(pBuf, 0, sizeof(*pBuf));
        
    return -1;
}


extern int __init h264d_init_ioctl(void);
extern int h264d_cleanup_ioctl(void);


int __init h264d_chk_chip_ver(void)
{
    unsigned int ver;
    unsigned int chip_ver;

    ver = ftpmu010_get_attr(ATTR_TYPE_CHIPVER);
    
    chip_ver = (ver >> 16) & chip_ver_mask;

    if(chip_ver_limit){
        /* check chip version */
        if(chip_ver != (chip_ver_limit & chip_ver_mask)){
            printk("chip version incorrect(driver build for %04X, chip version %04X mask %04X)\n", chip_ver_limit, chip_ver, chip_ver_mask);
            return -EINVAL;
        }

        /* set parameters for specific platform */
        switch(chip_ver_limit){
            case 0x8210: /* insmod with chip_ver_limit=0x8210 chip_ver_mask=0x0 to use parameters for 8210 */
                vcache_one_ref = VCACHE_ONE_REFERENCE_8210;
                max_ref1_width = MAX_REF1_WIDTH_8210;
                mcp300_engine_en = 3; /* enable both engines */
                plt_str = FTMCP300_PLT_STR_8210;
                break;
            case 0x8280: /* insmod with chip_ver_limit=0x8280 chip_ver_mask=0x0 to use parameters for 8287 */
                vcache_one_ref = VCACHE_ONE_REFERENCE_8287;
                max_ref1_width = MAX_REF1_WIDTH_8287;
                mcp300_engine_en = 1; /* enable engine 0 only */
                plt_str = FTMCP300_PLT_STR_8287;
                break;
            case 0xA369: /* insmod with chip_ver_limit=0x8280 chip_ver_mask=0x0 to use parameters for A369 */
                vcache_one_ref = VCACHE_ONE_REFERENCE_A369;
                max_ref1_width = MAX_REF1_WIDTH_A369;
                mcp300_engine_en = 1; /* enable engine 0 only */
                plt_str = FTMCP300_PLT_STR_A369;
                break;
            default:
                break;
        }
    }

    if(ver == 0x82100000){ /* enable workaround for 8210 version A */
        mcp300_sw_reset = 1;
    }


    return 0;
}


static int __init dec_init(void)
{
    char __plt_str[16] = FTMCP300_PLT_STR;
    if(strcmp(plt_str, FTMCP300_PLT_STR) != 0){
        snprintf(__plt_str, sizeof(__plt_str), "%s/" FTMCP300_PLT_STR, plt_str);
    }

    if (h264d_chk_chip_ver()){
		return -1;
    }

	if (h264d_init_ioctl() < 0)
		return -1;
    if (H264D_VER_BRANCH) {
    	printk("FAVC Decoder IRQ mode v%d.%d.%d.%d PLT %s, built @ %s %s\n", H264D_VER_MAJOR, 
            H264D_VER_MINOR, H264D_VER_MINOR2, H264D_VER_BRANCH, plt_str, __DATE__, __TIME__);
    }
    else {
        printk("FAVC Decoder IRQ mode %d.%d.%d PLT %s, built @ %s %s\n", H264D_VER_MAJOR, 
            H264D_VER_MINOR, H264D_VER_MINOR2, plt_str, __DATE__, __TIME__);
    }


    printk("mcp300 engines: %08X\n", mcp300_engine_en);


	return 0;
}

static void __exit dec_clean(void)
{
	h264d_cleanup_ioctl();
#if RSVD_SZ >= 4
    if(count_kmalloc_size){
        printk("memory leak detected: count_kmalloc_size:%d\n", count_kmalloc_size);
    }
#endif
}

module_init(dec_init);
module_exit(dec_clean);
MODULE_AUTHOR("Grain Media Corp.");
MODULE_LICENSE("Grain Media License");
MODULE_DESCRIPTION("FTMCP300 driver");
MODULE_VERSION(H264D_VER_STR);

