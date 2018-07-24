ifeq ($(CONFIG_PLATFORM_GM8180),y)
CONFIG_ENCODER_2DMA=no
CONFIG_ALLOCMEM_FROM_RAMMAP=no
VideoGraph_Interface=no
LCD_PARAM_Y_Extend=no
FIQ_Function = no
REF_RECON_Circle = no
endif


ifeq ($(CONFIG_PLATFORM_GM8185),y)
CONFIG_ENCODER_2DMA=yes
CONFIG_ALLOCMEM_FROM_RAMMAP=yes
VideoGraph_Interface=no
LCD_PARAM_Y_Extend=no
FIQ_Function = no
REF_RECON_Circle = no
endif

ifeq ($(CONFIG_PLATFORM_GM8185_v2),y)
CONFIG_ENCODER_2DMA=yes
CONFIG_ALLOCMEM_FROM_RAMMAP=yes
VideoGraph_Interface=no
LCD_PARAM_Y_Extend=yes
FIQ_Function = yes
REF_RECON_Circle = no
endif

ifeq ($(CONFIG_PLATFORM_GM8181),y)
CONFIG_ENCODER_2DMA=yes
CONFIG_ALLOCMEM_FROM_RAMMAP=yes
VideoGraph_Interface=yes
LCD_PARAM_Y_Extend=yes
FIQ_Function = yes
REF_RECON_Circle = yes
endif

ifeq ($(CONFIG_PLATFORM_FIE8150),y)
CONFIG_ALLOCMEM_FROM_RAMMAP=yes
VideoGraph_Interface=no
endif


#-------------------------------
# Select Large or normal resolution
#-------------------------------
CONFIG_2592x1944_RESOLUTION = yes
CONFIG_1600x1200_RESOLUTION = no
CONFIG_720x576_RESOLUTION = no

#-------------------------------
# HW Busy Performance Evaluation
#-------------------------------
CONFIG_HW_BUSY_PERFORMANCE = no

#--------------------------------------
# Embedded debug message in driver.
#--------------------------------------
DEBUG_MESSAGE = yes

CONFIG_EVALUATION_PERFORMANCE = no
CONFIG_STREAMING_INPUT = no
