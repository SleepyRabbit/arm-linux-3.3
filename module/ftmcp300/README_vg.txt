README for ftmcp300 vg driver
=============================

Patch and Build em.ko/log.ko used in test environment
-----------------------------------------------------
  1. check out gm_graph
  
    $ cd /usr/src/
    $ cvs checkout gm_graph
    
  2. set up configuration
  
    $ cd /usr/src/gm_graph
    $ ./setup.sh

  3. patch em
    $ cp /usr/src/arm-linux-3.3/module/ftmcp300/vg_test_entity/em_test.c \
         /usr/src/arm-linux-3.3/module/videograph.src/em/
         
    $ cd /usr/src/arm-linux-3.3/module/include/videograph.include/em
    $ patch -p0 < /usr/src/arm-linux-3.3/module/ftmcp300/vg_test_entity/video_entity.h.patch
    
    # NOTE: the following step can be skipped since it has been merged into VG at 2014/05/12
    $ cd /usr/src/arm-linux-3.3/module/ftmcp300/vg_test_entity/
    $ patch -p0 < /usr/src/arm-linux-3.3/module/ftmcp300/vg_test_entity/em_utility.c.patch
  
  4. build videograph
  
    $ cd /usr/src/arm-linux-3.3/module/videograph.src/
    $ mkdir bin
    $ ./build.sh
    
    em.ko, log.ko are generated under /usr/src/arm-linux-3.3/module/videograph.src/bin/
    


Build decoder/ftmcp300 driver module
------------------------------------
  type the following commands to build the driver module: decoder.ko
  
    $ cd /usr/src/arm-linux-3.3/module/decoder/
    $ make

  type the following commands to build the driver module: favc_dec.ko
  
    $ cd /usr/src/arm-linux-3.3/module/ftmcp300/
    $ make

    

Build Application
-----------------
  type the following commands to build the applicaion: test
  
    $ cd AP_vg
    $ make
    

Load Module for test environment
--------------------------------
  under Linux shell, skip running vg_boot.sh and type the following commands to load driver module, 
  
    $ insmod frammap.ko  # skip this if it is already loaded in boot.sh
    $ insmod log.ko
    $ insmod em.ko
    $ insmod decoder.ko
    $ insmod favc_dec.ko
    
    
Run test    
--------
   Here is an example of using test to decode an bitstream (news_cif.264) and 
   write output yuv to output_422_00.yuv
   
    $ ./test 1 -rec -p news_cif -f 10
    
   To compare result with golden:
   
    $ cmp output_422_00.yuv news_cif.yuv
   

   
Recording Bitstream to file
---------------------------

    After VG driver is loaded, the bitstream recording of each channel can be 
    enabled/disabled by typing the following commands:

    $ echo rec 1 0 3 > /proc/videograph/h264d/dbg_mode # to enable the recording for chn 0~3
    $ echo rec 0 0 3 > /proc/videograph/h264d/dbg_mode # to disable the recording for chn 0~3
    
    If the recording is enabled, bitstream of jobs will be worte to these files:
    /mnt/nfs/rec_bs_[chn].264 (bitstream)
    /mnt/nfs/rec_bs_[chn].txt (length of bitstream of each frame, job id, error number)
    
    The defualt dump path is: /mnt/nfs
    To change the path, type the following command:
    
    $ echo path /mnt/mtd/ > /proc/videograph/h264d/dbg_mode
    
    NOTE:
    - only the last GOP are recorded
    - enabling recording has negative impact on performace
    

Print Message in Driver
-----------------------

  - log_level
  
    The message print out to log buffer or console are controled by the module parameter: log_level
    The larger log_level value is set, the more messages are printed.
    If the log_level > 0x10, the message will be outputed to console.

    To enable all messages in driver, type the following command:
    $ echo 4 > /proc/videograph/h264d/log_level

    To enable all messages and output to console in driver, type the following command:
    $ echo 0x14 > /proc/videograph/h264d/log_level

    The meaning of each value are listed as follow:
        LOG_ERROR     0
        LOG_WARNING   1
        LOG_INFO      2
        LOG_TRACE     3
        LOG_DEBUG     4
    
    The default value of log_level is 0, which means only the error messages are shown.

  - dump log to file
  
    To dump the log buffer to file, type the following command:
    $ cat /proc/videograph/dumplog
    
    
  - log_level for profiling/tracing **NOTE A
  
    The driver also make use of log_level < 0 for profiling or trace register programing (REG_DUMP)
    The meaning of valid value are listed as follow:
    #define PROF      -1 **NOTE B
    #define PROF_PP   -2
    #define PROF_WSCH -3
    #define PROF_ISR  -4
    #define PROF_CB   -5
    #define PROF_DEC  -9
    #define REG_DUMP  -10
    
    NOTE:
    A. To enable this feature, driver must be compiled with the following define (in define.h):
    #define USE_PROFILING 1 
    B. A perl script(parse_log.pl) are used to parse and display the info in the messages with PROF level.
    
    
   
Meaning of error messages
-------------------------

    Here is an example of error message printed by MCP300 VG drvier.

    [FD](0,2) job 2 seq: 240 size: 16919 ISR error: -6(decode vld error) buf:1 stored:1 read ptr:9
         ^^^      ^      ^^^       ^^^^^ ^^^^       ^^ ^^^^^^^^^^^^^^^^      ^        ^          ^
    (engine,    job   put job  bitstream error     error    error           buf    buf     bitstream parser
    channel)    id    seq num  size      position  code    message          idx   stored   read ptr offset
                                       (TRIG/ISR/
                                        TIMEOUT)

    'put job seq number' is incremented as the job is put into the driver.
   

    
Frequently reported error number/message of MCP300 driver
---------------------------------------------------------

-29(loss pic error)
    
    It means the MCP300 driver detect the incoming bitsteam has incorrect frame_num. (!= previous frame_num + 1)
    This is typically caused by frame dropping/skipping.
    
    There are two ways to identify if this error is detected:
    1. When it occured, the driver shell print the error message:
    [FD](0,4) job 680558 seq: 8900 size: 4836 trigger failed error: -29(loss pic error) buf:1 stored:0
    
    To disable this error message, you can type the following command
    $ echo 2 > /sys/module/favc_dec/parameters/lose_pic_handle_flags
    
    2. Driver maintains a counter for this error. Type the following command and check the counter named "fs" 
       of the channel:
    
    # cat /proc/videograph/h264d/info 
    chn:  0 dec_data: 0x83F00000 eng: -1 mir:  0 dec: 0xAF779780 used_buf_num: 2 ready_job: 1 
    vc: 7/7 intr cnt: bl:0 be:0 to:0 dr:0 dw:0 ll:0 fd:61808 sd:61808 vl:0 cnt: df:61808 dt:61808 err cnt: fs:0 wi:0 si:0 buf used:1 unoutput:0 max:0 num_ref_frames:1
                                                                                                           ^^^^
                                                                                                           
    "chn: 0" indicates the channel number, which is channel 0 in this case.
    "fs:0" indicates the number of frame skipped occured in this channel.
    
    
    If user do not want to treat "frame skipping" as an error(such as implementing the fast forward), 
    then you can type the following command:
    $ echo 0 > /sys/module/favc_dec/parameters/lose_pic_handle_flags
    
    It means "do not treat loss picture as an error(bit 1 == 0)" and "do not print error message (bit 0 == 0)".


-6(decode vld error)
    
    It means the HW detected error.
    This is typically caused by error bitstream.
    
    The process of decoding bitstream consists of "parsing header" and "decoding slice data".
    This VLD error are detected by HW only at "decode slice data", and it is usually caused by incorrect slice data.
    
    To confirm if the bitstream is incorrect, you can record the error bitsteam and decode it with JM or analysis it by other tools.
    If the decoded bitstream comes from a file, you can check if data and size of recorded bitstreams matches the orininal file.
    
    Please refer to "Recording Bitstream to file" section to see how to record bitstream via MCP300 driver


-7(no valid bitstream in input buffer (bit stream empty))

    It means parser read out all bitstream data at header parsing. 
    It is usually caused by bitsteam size/data error.

    To confirm if the bitstream is incorrect, you can record the error bitsteam and decode it with JM or analysis it by other tools.
    If the decoded bitstream comes from a file, you can check if data and size of recorded bitstreams matches the orininal file.
    
    Please refer to "Recording Bitstream to file" section to see how to record bitstream via MCP300 driver

    
    If HW parser is used, this may occurr during burn-in (>= 8hrs). 
    Please switch to SW parser to avoid this error.
    
    Type the following command to check if the driver is using HW parser:
    $ echo 1 > /proc/videograph/h264d/config # enable detail config
    $ cat /proc/videograph/h264d/config # get detail config
    ...
    USE_SW_PARSER:1 # 1: SW parser, 0: HW parser
    ...

    
-5(frame size is larger than max frame size from input property dst_bg_dim)

    It means the width/height specified in the header of btistream are larger than the 
    buffer size specified in the input property DST_BG_DIM.
                
    The driver will skip the decoding of this frame, so that the buffer overflow will not happen.


unknown message (from JPEG/MPEG4 driver)

    There are some cases that the user got error message that comes from other driver. Here is a scenario of the unknown error message.
    While decoding error bitstreams, the bitstream may be miss-identified as JPEG or MPEG4 bitstream by the multi-format decoder(decoder.ko).
    The miss-identified bitstream may then be dispatched to JPEG/MPEG4 driver and then the error messages of JPEG/MPEG4 driver are shown.
    
    If it is guaranteed that only the H264 bitstream are passed into driver, user may diable the JPEG/MPEG4 job dispatching at decoder.ko
    to avoid this kind of errors by setting module paramters:
    
    $ insmod decoder.ko en_jpeg=0 en_mpeg4=0


B-frame decoding
----------------
    
    By default, the B-frame decoding is DISABLED. 
    It will consume less buffer, since the buffer for MBINFO is not required.
    If driver encounters a b-frame when B-frame decoding is disabled, driver will show the following error message: -15(driver unsupport(b-frame))
    
    To enable B-frame decoding, the folling module parameter must be set:
    mcp300_b_frame=1 # enable B-frame decoding
    output_buf_num=M # The proper value of M depend on bitstream. M=1 should work for the typical bitstream containing b-frame. User may try larger value if the output order is incorrect
    mcp300_max_buf_num=N # where N >= SPS.num_ref_frames. otherwise the error message may occurred: -25(buffer number not enough)
    
    
    NOTE: The driver provide a simple way to query SPS.num_ref_frames via proc. Here is an example:   
    
    $ cat /proc/videograph/h264d/info 
    chn:  0 dec_data: 0x83D80000 eng: -1 mir:  0 dec: 0x838D74A0 used_buf_num: 1 ready_job: 0 rec_en: 0 
    vc: 7/7 intr cnt: ..... num_ref_frames:1
                            ^^^^^^^^^^^^^^^^

    
Chip version checking at module loading
---------------------------------------

    Dirver will check chip version ID to see if the built driver module is loaded on incorrect chip.
    For example, if the driver build for GM828X is loading on chip GM8210, the module loading will fail.
    
    To skip this checking, you may set the module parameter chip_ver_limit=0.
    $ insmod favc_dec.ko chip_ver_limit=0
    
    NOTE: the description here should be reviewed as a new platform is addded.
    
    Actually, the driver module built for GM8210 is ok to loaded on GM8287. 
    (But the driver built for GM8287 shell not be loaded on GM8210, since only one engine structure is allocated at compile time)
    
    To do so, you may set the module parameter at module loading
    $ insmod favc_dec.ko chip_ver_limit=0x8280 chip_ver_mask=0x0

    
Profiling
---------

    Driver provides several ways to get time profile.
    
    - proc nodes
    
    $ cat /proc/videograph/h264d/job_prof # get profiling information of last N jobs (N=64)
    $ cat /proc/videograph/h264d/profiling # get profiling of ISR, prepare job wq, callback wq of the last M exectuion (M=64)
    
    
    - log file

    Please refer to the "log_level for profiling/tracing" paragraph of the "Print Message in Driver" section.
    
    
Proc Nodes
----------

    Driver implements several proc nodes to allow user to query info or to change
    driver parameters. After VG driver is loaded, these nodes are created under 
    "/proc/videograph/h264d/"

    Here is the list and description of each proc nodes.

    Proc node name   Description
    ---------------  -------------------------------------------
    buf_num          Set/get the value of the module parameter: mcp300_max_buf_num
    callback_period  Set/get the delay (in msec) from triggering callback workqueue to execution
    prepare_period   Set/get the delay (in msec) from triggering prepare_job workqueue to execution 
    config           Show the message (driver version, built platform) at module loading.
                     Write non-zero to it to enable 'detail config', which means showing 
                     detail defines and internal counters, and show the 'dbg_mode controlled behavior' 
                     at reading dbg_mode.
    dbg_mode         Set debug mode level, set dump file path, enable/disable bitstream recording, 
                     backdoors for calling driver's functions.
                     The operations controlled by dbg_mode will also show if 'detail config' is enabled
    dump             Set and get the dump conditions of each channel
    eng_reg          Show register value of the specified engine
    info             Show counters, parameters, jobs, buffers of each channel
    job              Show status and time of jobs that is queued in driver
    job_prof         Show the time profiling and output property of the last M jobs (M=64)
    log_level        Set/get the log level.
    param            Set/show the parameters
    profiling        Show the time profiling of the last N execution of callback_scheduler(), 
                     prepare_job() and ISR (N=64)
    property         Show the last job's input/output property and error number
    utilization      Show the HW utilization during the last X seconds (X is 5 by default)
                     X Can be change by write values to this node. 
                     If X=0, profiling is disabled
    vg_info          Show the engine number, and whether B-frame decoding is enabled
    
    fs/fs            Clear/Get the lose picutre counter of a specified channel
                     (echo 0 to this node to clear counter)
    fs/ch            Set/get the specified channel in the fs/fs node

    
Module Parameters
-----------------

    - latency (from setting to taking effect)
    
        The module parameters shell be set at module loading time.
            $ insmod favc_dec.ko xxx=N ...
         
        If you set them at run-time :
            $ echo N > /sys/module/favc_dec/parameters/xxx
        there might be a period of latecy according to the usage of the module parameters.
        
        If the module parameter is use directly in driver, there will be no latency.
        If the values of module parameter are passing to low level driver via H264Dec_Init(),
        they will take effect when:
        - decoding first job after decoding error
        - decoding first job after channel stop
        - resolution change
    
    
    - important module parameters
    
        log_level: log message level (can be accessed via /proc/videograph/log_level)
                   The larger log_level is, the driver prints more messages.
        dbg_mode: debug mode level (can be accessed via /proc/videograph/dbg_mode)
                  The larger dbg_mode is, the driver enables more debug tools.
        mcp300_max_buf_num: Max Buffer Number per channel. It affects the output latency of DPB. 
                            If it is 0, the picture output order will be the same as decoding order.
                            It >0, this value must be larger then SPS.num_ref_frame, otherwise driver will show error.
        output_buf_num: It affects the output latency of DPB. 
                        If it is 0, the picture output order will be the same as decoding order.
                        If the picture output is not the same as POC order, you may try to enlarge this value.
                        In most cases, output_buf_num=0 or 1 shell works fine.
        mcp300_b_frame: Whether to allocate memory required by B-frame. 
                        If it is 0, driver will show error message when B-frame is encountered.
                        It must be 1 when :
                            a.  width >= VCACHE_MAX_WIDTH(1920), or 
                            b.  VCACHE SYSINFO is disabled.
        mcp300_codec_padding_size: padded size at the end of each bitstream buffer
        mcp300_max_width: The max width allowed. It determines the intra buffer size allocated 
                          at module loading.
        mcp300_max_height: The max height allowed. It determines the intra buffer size allocated 
                           at module loading.
        reserve_buf: Whether to call video_free_buffer() and video_reserve_buffer()
        chn_num: Number of channel (It determines the array size allocated at module loading)
        buf_ddr_id: The ddr ID used to allocate intra buffer at module loading
        mcp300_sw_reset: Whether to enable software reset (reset HW engine) at start of decoding each frame
        output_mbinfo: Whether to allow output MV (and whether to allocate MB info buffers)
        vc_cfg: vcache enable bits(bit0: ILF, bit1: SYSINFO, bit2: REF)
        chip_ver_limit: the value used to compare with the chip version read from register
        chip_ver_mask: the mask used at comparing chip version
        lose_pic_handle_flags: lose picture handling flags.
                               (bit 0: whether to print error message. bit 1: whether to return error)
        max_level_idc: driver will show error when the level idc in bitstream > max_level_idc
        err_handle: error handling method.
                    1-reinit for all errors
                    others-reinit for some errors only
        use_ioremap_wc: use ioremap_wc (to use bufferable virtual address at accessing HW registers)
                        NOTE:Do NOT enable it, since the buffer flushing is not handled completly
        mcp300_alloc_mem_from_vg: allocate mbinfo buffers from VG or frammap                
        clean_en: enable clean D-cache for input bitstream buffer
        callback_period: the delay at triggering callback workq (in msecs)
        prepare_period: the delay at triggering prepare_job workq (in msecs)
        en_src_fr_prop: enable the src_frame_rate output property
        engine_num: index of the enabled engine (0: enable engine 0 only, 1: enable engine 1 only, -1: enable all engines)
        minor_prof: enable the minor profiling (disable to reduce CPU loading)
        pad_en: enable padding 4 byte start code at the end of each bitstream buffer
        chroma interlance: How the chroma componment is placed (bit5,6 of offset 0x44)
                0-set it to 0(progressive), 
                1-set it to 1(interlace)
                3-set HW chroma interlance to 1(interlace) at field coding, set it to 0(progressive) at frame coding
        sw_reset_timeout_ret: whether to return and skip the rest of the reset process if timeout occurred at sw reset
        clear_sram: where to clear SRAM at inital mcp300 at module loading 
                   (It is required to avoid DMA error at decoding error bitstream)
        sw_timeout_delay: the software timeout period (in jiffies)
        max_pp_buf_num: number of buffer allowed per channel when b-frame is disabled and MV is not required (MBINFO is not required)
                        If low hw utilizatoin is caused prepare job through put, you may try to enlarge this value
        extra_buf_num: number of extra buffer allowd per channed when b-frame is enabled or MV is required (MBINFO is required)
                       number of buffer allowed per channel = (mcp300_max_buf_num + extra_buf_num) if MBINFO is required
                       
                       
    - module parameters for debugging
    
        trace_job: Whether to enable two char debug message at put job/callback/ISR
        chk_buf_num: Whether to check mcp300_max_buf_num >= SPS.num_ref_frame in driver
        print_prop: Whether to print property to console
        rec_prop: Whether to record job's property
                  0-record input property only
                  1-record input and output property
                  others-do not record property
        file_path: the path where the dump file saved
        save_err_bs_flg: save input bitstream for all jobs in driver
        save_bs_cnt: save input bitstream for the next incomming N jobs
        save_mbinfo_cnt: save output MBINFO for the next incomming N jobs
        save_output_cnt: save output YUV for the next incomming N jobs
        print_out_verbose: where to dump job's info at dumpping log
        
        err_bs: generate error in input bitstream for the next incomming N jobs
        err_bs_type: type of error in input bitstream.
                  0-copy the last half of the bitstream to the first half
                  1-copy the first half of the bitstream to the last half
                  2-clear the input bitstream to zero
                  3-fill the input bitstream buffer with the 4-byte size value
                  
        dump_cond: set condition for DUMP_MSG_G()
        
        
    - module parameters for HW verification
    
        bs_padding_size: number of bytes added for each input buffer size (64 is required)
        delay_en: add delay before triggering decoder
        blk_read_dis: value for 0x24 bit 21:block read disable (change this value when DMA access unstable)
        clear_en: whether to clear top 8 lines of the output buffer to zero before decoding each frame
                  (to see if output buffer is wrote by HW DMA)
        clear_mbinfo: whether to clear mbinfo memory after memory allocation
        clk_sel: 0-use ACLK(396MHz) 1-use pll4out3 clk(403MHz)
        hw_timeout_delay: HW timeout value. 0-use dafault. other: values to overwrite HW timeout value
        vcache_one_ref: whether the HW has only one SRAM for VCACHE ref
        max_ref1_width: the max allowed width that HW can enable VCACHE REF 1

        
    - module parameters for reverse playback (REV_PLAY=1)
    
        gop_output_group_size: output group size for gop_job_item (0: use gop_size/2)
        max_gop_size: max allowed GOP size
        
        

    - module parameters for updated em
    
        The put_job/stop behaviors are different in VG test environment and the real VG application.
        
        The job number is always sufficient in VG test environment, while in the real VG application 
        the job number is limited and job throughput may be limited by other video entity
        (such as scaler, LCD controller driver).
        
        The timing of calling stop is fixed in VG test environmet, while it should be random in the 
        real VG application.
        
        To simulate these behavior, here are the extra module parameters to change the behavior of 
        put_job/stop of em in VG test environment.
        
        pause_msec: pause duration in msec in test case 1
        pause_period: the period (in number of jobs) of pausing put_job in test case 1
        test_stop:  whether to call stop in test case 1
        stop_job_num:  call stop after putting N job in test case 1
        
        em_dbg: whether to print message in em_test
