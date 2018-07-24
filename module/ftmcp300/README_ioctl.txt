README for ftmcp300 ioctl driver
================================

Build ftmcp300 driver module
----------------------------
  type the following commands to build the driver module: favc_dec_ioctl_drv.ko
  
    $ make GMGraphInterface=No

    

Build Application
-----------------
  type the following commands to build the applicaion: dec_main
  
    $ cd AP_ioctl
    $ make
    
    
Load Module
-----------
  under Linux shell, skip running vg_boot.sh and type the following commands to load driver module, 
  and create device nodes: /dev/favcdec0, /dev/favcdec1
  
    $ insmod frammap.ko  # skip this if it is already loaded in boot.sh
    $ insmod favc_dec_ioctl_drv.ko
    $ mdev -s
    
    
Run dec_main
------------
   Here is an example of using dec_main to decode an bitstream (AVCNL-1.264) and 
   compare the output yuv with golden (golden_AVCNL-1.dat)
   
    $ ./dec_main -d /dev/favcdec0 -S 0 -i pattern/jvt_bs/AVCNL-1.264 \
      -g golden/jvt_golden/golden_AVCNL-1.dat -c golden/jvt_golden/conf_AVCNL-1 \
      -r final_result -n AVCNL-1 -p 0 -f 17 -w 176 -h 144 -W 720 -H 576

      
Print Message in Driver
-----------------------
    The message print out to console are controled by the module parameter: log_level
    The larger log_level value is set, the more message are printed.
    
    To enable all messages in driver, type the following command:
    $ echo 4 > /sys/module/favc_dec_ioctl_drv/parameters/log_level

    To disable all messages in driver, type the following command:
    $ echo -1 > /sys/module/favc_dec_ioctl_drv/parameters/log_level

    
    The meaning of each value are listed as follow:
        LOG_ERROR     0
        LOG_WARNING   1
        LOG_INFO      2
        LOG_TRACE     3
        LOG_DEBUG     4
    
    The default value of log_level is 0, which means only error messages are shown.

