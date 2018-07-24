#ifndef _DH9910_LIB_H_ 
#define _DH9910_LIB_H_

#define DHC_DEF_API

/*************************************************************************************
 *  DH9910 Type Definition
 *************************************************************************************/
typedef unsigned char           DHC_U8;
typedef unsigned short          DHC_U16;
typedef unsigned int            DHC_U32;

typedef signed char             DHC_S8;
typedef short                   DHC_S16;
typedef int                     DHC_S32;

typedef char                    DHC_CHAR;
#define DHC_VOID                void

/*************************************************************************************
 * DH9910 Defination
 *************************************************************************************/
typedef enum dhc_bool {
    DHC_FALSE = 0,
    DHC_TRUE  = 1,
} DHC_BOOL;

#ifndef NULL
    #define NULL            ( 0L )
#endif

#define DHC_NULL            ( 0L )
#define DHC_SUCCESS         ( 0 )
#define DHC_FAILURE         ( -1 )

#define DHC_DH9910_MAX_CHNS     ( 4 )
#define DHC_DH9910_MAX_CHIPS    ( 4 )
#define DHC_DH9910_INV_CHN      ( 0xFF )
#define DHC_DH9910_TDM_UNUSED   ( 0x0F )
#define DHC_DH9910_TDM_4        ( 0xEE )

typedef enum {
    DHC_DH9910_MSG_SILENT = 0,
    DHC_DH9910_MSG_ERR,
    DHC_DH9910_MSG_WARN,
    DHC_DH9910_MSG_INFO,
    DHC_DH9910_MSG_DEBUG,
    DHC_DH9910_MSG_BOTT
}DHC_Dh9910MsgType_E;

typedef enum {
    DHC_ERRID_SUCCESS = 0,
    DHC_ERRIF_FAILED,
    DHC_ERRID_TIMEOUT,
    DHC_ERRID_INVPARAM,
    DHC_ERRID_NOINIT,
    DHC_ERRID_INVPOINTER,
    DHC_ERRID_UNSUPPORT,
    DHC_ERRID_UNKNOW
}DHC_9910ErrID_E;

typedef struct {
    DHC_U8 u8HOffset;
    DHC_U8 u8VOffset;
}DHC_VideoOffset_S;

typedef enum {
    DHC_SET_MODE_DEFAULT = 0,
    DHC_SET_MODE_USER    = 1,
    DHC_SET_MODE_NONE
}DHC_SetMode_E;

typedef struct {
    DHC_U8 u8Brightness;
    DHC_U8 u8Contrast;
    DHC_U8 u8Saturation;
    DHC_U8 u8Hue;
    DHC_U8 u8Gain;
    DHC_U8 u8WhiteBalance;
    DHC_U8 u8Sharpness;
    DHC_U8 u8Reserved[1];
}DHC_VideoColor_S;

typedef enum {
    DHC_VOSIZE_HD = 0,
    DHC_VOSIZE_SD,
    DHC_VOSIZE_BUT
}DHC_VoSize_E;

typedef enum {
    DHC_SDFMT_PAL = 0,
    DHC_SDFMT_NTSC,
    DHC_SDFMT_BUT
}DHC_SDFmt_E;

typedef enum {
    DHC_HD_720P_25HZ = 0, /* 720P 25 */
    DHC_HD_720P_30HZ,
    DHC_HD_720P_50HZ,
    DHC_HD_720P_60HZ,
    DHC_HD_1080P_25HZ, 
    DHC_HD_1080P_30HZ,
    
    DHC_SD_NTSC_JM,
	DHC_SD_NTSC_443,
	DHC_SD_PAL_M,
	DHC_SD_PAL_60,
	DHC_SD_PAL_CN,
	DHC_SD_PAL_BGHID,
	
	DHC_INVALID_FMT = 15,
}DHC_VideoFmt_E;

typedef enum {
    DHC_COLORBAR_WHITE = 0,
    DHC_COLORBAR_YELLOW,
    DHC_COLORBAR_CYAN,
    DHC_COLORBAR_GREEN,
    DHC_COLORBAR_MAGENTA,
    DHC_COLORBAR_RED,
    DHC_COLORBAR_BLUE,
    DHC_COLORBAR_BLACK,
    DHC_COLORBAR_BUT
}DHC_ColorBar_E;

typedef enum {
    DHC_CABLE_TYPE_COAXIAL = 0,
    DHC_CABLE_TYPE_UTP_10OHM,
    DHC_CABLE_TYPE_UTP_17OHM,
    DHC_CABLE_TYPE_UTP_25OHM,
    DHC_CABLE_TYPE_UTP_35OHM,
    DHC_CABLE_TYPE_BUT
} DHC_CableType_E;

typedef enum {
    DHC_VIDEO_CONNECT = 0,
    DHC_VIDEO_LOST = 1,
    DHC_VIDEO_BUT
}DHC_VideoStatus_E;

typedef enum {
    DHC_AUDIO_LINE_IN = 0,
    DHC_AUDIO_COAXIAL = 1,
    DHC_AUDIO_BUT
}DHC_AudioDataSel_E;

typedef enum {
    DHC_VOCLK_EDGE_RISING = 0, ///< 2-CH byte interleave
    DHC_VOCLK_EDGE_DUAL = 1,   ///< 2-CH dual edge
    DHC_VOCLK_EDGE_BUT
}DHC_VoClkEdge_E;

typedef enum {
    DHC_VIDEO_OUTMODE_NO = 0,
    DHC_VIDEO_OUTMODE_HBLANK,
    DHC_VIDEO_OUTMODE_HEADINFO,
    DHC_VIDEO_OUTMODE_BOTH
}DHC_VideoOutMode_E;

typedef struct {
    DHC_U8 u8VideoLost;
    DHC_U8 u8VideoFormat;
    DHC_U8 u8VideoReportFormat;
    DHC_U8 u8Reserved[1];
}DHC_VideoStatus_S;

typedef enum {
    DHC_AUDIO_SAMPLERATE_8K  = 0,
    DHC_AUDIO_SAMPLERATE_16K = 1,
    DHC_AUDIO_SAMPLERATE_32K = 2,
    DHC_AUDIO_SAMPLERATE_48K = 3
}DHC_AudioSampleRate_E;

typedef enum {
    DHC_AUDIO_ENCCLK_MODE_SLAVE  = 0, 
    DHC_AUDIO_ENCCLK_MODE_MASTER = 1
}DHC_AudioEncClkMode_E;

typedef enum {
    DHC_AUDIO_SYNC_MODE_I2S = 0, 
    DHC_AUDIO_SYNC_MODE_DSP = 1
}DHC_AudioSyncMode_E;

typedef enum {
    DHC_AUDIO_ENCCLK_MODE_108M = 0,
    DHC_AUDIO_ENCCLK_MODE_54M  = 1,
    DHC_AUDIO_ENCCLK_MODE_27M  = 2
}DHC_AudioEncClkSel_E;

typedef enum {
    DHC_AUDIO_DEC_FREQ_32   = 0,
    DHC_AUDIO_DEC_FREQ_64   = 1,
    DHC_AUDIO_DEC_FREQ_128  = 2,
    DHC_AUDIO_DEC_FREQ_256  = 3
}DHC_AudioDecFreq_E;

typedef enum {
    DHC_AUDIO_DEC_I2S_CHN_RIGHT = 0,
    DHC_AUDIO_DEC_I2S_CHN_LEFT = 1
}DHC_AudioDecI2sChn_E;

typedef struct {
    DHC_U8 u8CascadeNum;   /*1 - 4*/
    DHC_U8 u8CascadeStage; /*0 - 3*/
}DHC_AudioConnectMode_S;

typedef struct {
    DHC_U8 u8CascadeNum;   /*1 - 4*/
    DHC_U8 u8CascadeStage; /*0 - 3*/
}DHC_AudioDecMode_S;

typedef struct {
    DHC_U8 u8CascadeNum;   /*1 - 4*/
    DHC_U8 u8CascadeStage; /*0 - 3*/
}DHC_AudioDecChn_S;

typedef union {
    struct {
        DHC_U8 EncDataSel3     : 1;    /* [0] */
        DHC_U8 EncDataSel2     : 1;    /* [1] */
        DHC_U8 EncDataSel1     : 1;    /* [2] */
        DHC_U8 EncDataSel0     : 1;    /* [3] */
        DHC_U8 Reserved        : 4;    /* [4..7] */
    };
    DHC_U8 u8Cfg;
}DHC_AudEncDataSelCfg_U;

typedef union {
    struct {
        DHC_U8 DecFreq         : 2;    /* [0..1] */
        DHC_U8 DecMode         : 1;    /* [2..2] */
        DHC_U8 DecSync         : 1;    /* [3..3] */
        DHC_U8 Reserved        : 2;    /* [4..5] */
        DHC_U8 DecChnSel       : 1;    /* [6..6] */
        DHC_U8 DecCs           : 1;    /* [7..7] */ 
    };
    DHC_U8 u8Cfg;
}DHC_AudDecCfg_U;

typedef union {
    struct {
        DHC_U8 CascadeStage    : 2;    /* [0..1] */
        DHC_U8 Reserved0       : 2;    /* [2..3] */
        DHC_U8 CascadeNum      : 2;    /* [4..5] */
        DHC_U8 Reserved1       : 2;    /* [6..7] */
    };
    DHC_U8 u8Mode;
}DHC_AudConnectMode_U;

typedef union {
    struct {
        DHC_U8 SyncMode         : 1;    /* [0..0] */
        DHC_U8 EncDiv           : 1;    /* [1..1] */
        DHC_U8 SampleRate       : 2;    /* [2..3] */
        DHC_U8 EncMclkEn        : 1;    /* [4..4] */
        DHC_U8 EncMode          : 1;    /* [5..5] */
        DHC_U8 EncTest          : 1;    /* [6..6] */
        DHC_U8 EncCs            : 1;    /* [7..7] */
    };
    DHC_U8 u8Cfg;
}DHC_AudModeCfg_U;

typedef struct {
    DHC_AudConnectMode_U uAudConnectMode;
    DHC_AudModeCfg_U uAudModeCfg;
    DHC_AudDecCfg_U uAudDecCfg;
    DHC_AudEncDataSelCfg_U uAuEncDataSelCfg;
}DHC_Dh9910AudioAttr_S;

typedef union {
    struct {
        DHC_U8 VoTmdMode        : 2;    /* [0..1] */
        DHC_U8 HdBitMode        : 1;    /* [2..2] */
        DHC_U8 HdGmMode         : 1;    /* [3..3] */
        DHC_U8 Sd960HEn         : 1;    /* [4..4] */
        DHC_U8 VoClkEdge        : 1;    /* [5..5]*/
        DHC_U8 ForceHdClk       : 1;    /* [6..6] */
        DHC_U8 Reserved1        : 1;    /* [7..7]*/
    };
    DHC_U8 u8VoCfg;
}DHC_VoCfg_U;

typedef union {
    struct {
        /* FreeRun Format -> 0 : HD, 1 : SD */
        DHC_U8 HDorSD           : 1;    /* [0] */
        /* SD mode P or N -> 0 : PAL, 1 : NTSC */
        DHC_U8 PalOrNtsc        : 1;    /* [1] */
        /* HD format -> 0 : 720p25,  1 : 720p30, 
                        2 : 720p50,  3 : 720p60,
                        4 : 1080p25, 5 : 1080p30 */
        DHC_U8 Format           : 3;    /* [2..4] */     
        /* If 960H -> 1 : 960H, 0 : 720H */        
        DHC_U8 Color            : 3;    /* [5..7] */
    };
    DHC_U8 u8VoFreeRunCfg;
}DHC_VoFreeRun_U;

typedef union {
    struct {
        DHC_U8 ChIdVal          : 2;    /* [0..1] */
        DHC_U8 ChIdPos          : 2;    /* [2..3] */
        DHC_U8 Reserved1        : 4;    /* [4..7] */
    };
    DHC_U8 u8VideoOutModeCfg;
}DHC_VideoOutMode_U;

typedef struct {
    DHC_U8 u8ChnSequence[ DHC_DH9910_MAX_CHNS ];
    DHC_U8 u8NetraModeCh[ DHC_DH9910_MAX_CHNS ];
    DHC_VoFreeRun_U uVoFreeRun; 
    DHC_VoCfg_U uVoCfg;
    DHC_VideoOutMode_U uVideoOutMode[ DHC_DH9910_MAX_CHNS ];
}DHC_Dh9910VideoAttr_S;

typedef struct {
    DHC_U8 u8ChipAddr;
    DHC_U8 u8ProtocolLen;
    DHC_U8 u8ProtocolType;
    DHC_U8 u8CheckChipId;
    DHC_Dh9910VideoAttr_S   stVideoAttr;
    DHC_Dh9910AudioAttr_S   stAudioAttr;
    //DHC_U32 u32Reserved[1];
}DHC_Dh9910Attr_S;

typedef struct {
    DHC_U8 u8AdCount;
    DHC_U8 u8DetectPeriod;
    DHC_Dh9910Attr_S stDh9910Attr[DHC_DH9910_MAX_CHIPS];
    DHC_9910ErrID_E (* Dh9910_WriteByte)(DHC_U8 u8ChipAddr, DHC_U8 u8RegAddr, DHC_U8 u8RegValue);
    DHC_U8 (* Dh9910_ReadByte)(DHC_U8 u8ChipAddr, DHC_U8 u8RegAddr);
    DHC_VOID (* Dh9910_MSleep)(DHC_U8 u8MsDly);
    DHC_VOID (* Dh9910_Printf)(DHC_CHAR *pszStr);
    DHC_VOID (* Dh9910_GetLock)(void);
    DHC_VOID (* Dh9910_ReleaseLock)(void);
    DHC_U32 u32Reserved[2];
}DHC_Dh9910UsrCfg_S;

/*************************************************************************************
 *  DH9910 API Export Function Prototype
 *************************************************************************************/
DHC_9910ErrID_E DHC_DEF_API DHC_DH9910_SDK_Init(DHC_Dh9910UsrCfg_S *pstDh9910UsrCfg);
DHC_9910ErrID_E DHC_DEF_API DHC_DH9910_SDK_DeInit(DHC_VOID);
DHC_VOID DHC_DEF_API DHC_DH9910_SDK_DetectThr(DHC_VOID);
DHC_9910ErrID_E DHC_DEF_API DHC_DH9910_SDK_SendCo485Enable(DHC_U8 u8ChipIndex, DHC_U8 u8Chn, DHC_BOOL bEnable);
DHC_9910ErrID_E DHC_DEF_API DHC_DH9910_SDK_SendCo485Buf(DHC_U8 u8ChipIndex, DHC_U8 u8Chn, DHC_U8 *pu8Buf, DHC_U8 u8Lenth);
DHC_9910ErrID_E DHC_DEF_API DHC_DH9910_SDK_RecvCo485Enable(DHC_U8 u8ChipIndex, DHC_U8 u8Chn, DHC_BOOL bEnable);
DHC_9910ErrID_E DHC_DEF_API DHC_DH9910_SDK_RecvCo485Buf(DHC_U8 u8ChipIndex, DHC_U8 u8Chn, DHC_U8 *pu8Buf, DHC_U8 *pu8Lenth);
DHC_9910ErrID_E DHC_DEF_API DHC_DH9910_SDK_ClearEq(DHC_U8 u8ChipIndex, DHC_U8 u8Chn);
DHC_9910ErrID_E DHC_DEF_API DHC_DH9910_SDK_SetVideoPos(DHC_U8 u8ChipIndex, DHC_U8 u8Chn, DHC_VideoOffset_S *pstVideoPos);
DHC_9910ErrID_E DHC_DEF_API DHC_DH9910_SDK_GetVideoPos(DHC_U8 u8ChipIndex, DHC_U8 u8Chn, DHC_VideoOffset_S *pstVideoPos);
DHC_9910ErrID_E DHC_DEF_API DHC_DH9910_SDK_SetClkPhase(DHC_U8 u8ChipIndex, DHC_U8 u8Chn, DHC_U8 u8Invert, DHC_U8 u8Delay);
DHC_9910ErrID_E DHC_DEF_API DHC_DH9910_SDK_GetVideoStatus(DHC_U8 u8ChipIndex, DHC_U8 u8Chn, DHC_VideoStatus_S *pstVideoStatus);
DHC_9910ErrID_E DHC_DEF_API DHC_DH9910_SDK_SetColor(DHC_U8 u8ChipIndex, DHC_U8 u8Chn, DHC_VideoColor_S *pstVideoColor, DHC_SetMode_E eColorMode);
DHC_9910ErrID_E DHC_DEF_API DHC_DH9910_SDK_GetColor(DHC_U8 u8ChipIndex, DHC_U8 u8Chn, DHC_VideoColor_S *pstVideoColor, DHC_SetMode_E eColorMode);
DHC_9910ErrID_E DHC_DEF_API DHC_DH9910_SDK_ReadReg(DHC_U8 u8ChipIndex, DHC_U8 u8Page, DHC_U8 u8RegAddr, DHC_U8 *pu8Value);
DHC_9910ErrID_E DHC_DEF_API DHC_DH9910_SDK_WriteReg(DHC_U8 u8ChipIndex, DHC_U8 u8Page, DHC_U8 u8RegAddr, DHC_U8 u8Value);
DHC_9910ErrID_E DHC_DEF_API DHC_DH9910_SDK_SetCableType(DHC_U8 u8ChipIndex, DHC_U8 u8Chn, DHC_CableType_E eCableType);
DHC_9910ErrID_E DHC_DEF_API DHC_DH9910_SDK_SetAudioInVolume(DHC_U8 u8ChipIndex, DHC_U8 u8Chn, DHC_U8 u8Volume);
DHC_9910ErrID_E DHC_DEF_API DHC_DH9910_SDK_SetLineInVolume(DHC_U8 u8ChipIndex, DHC_U8 u8Chn, DHC_U8 u8Volume);
DHC_9910ErrID_E DHC_DEF_API DHC_DH9910_SDK_SetLineOutVolume(DHC_U8 u8ChipIndex, DHC_U8 u8Volume);
DHC_9910ErrID_E DHC_DEF_API DHC_DH9910_SDK_SelectOutputChn(DHC_U8 u8ChipIndex, DHC_U8 u8OutputChn);
DHC_9910ErrID_E DHC_DEF_API DHC_DH9910_SDK_SetMsgLevel(DHC_Dh9910MsgType_E eMsgLevel);
DHC_9910ErrID_E DHC_DEF_API DHC_DH9910_SDK_SetEqLevel(DHC_U8 u8ChipIndex, DHC_U8 u8Chn, DHC_U8 u8Level);
DHC_9910ErrID_E DHC_DEF_API DHC_DH9910_SDK_GetEqLevel(DHC_U8 u8ChipIndex, DHC_U8 u8Chn, DHC_U8 *u8Level);

#endif /* _DH9910_LIB_H_ */
