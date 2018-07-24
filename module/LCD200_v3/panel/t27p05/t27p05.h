/*******************************************************************************
* Copyright  Grain-Media, Inc. 2011-2012.  All rights reserved.                *
*------------------------------------------------------------------------------*
* Name: t27p05.h                                                           *
* Description: header file for t27p05 control                              *
*                                                                              *
* Author: giggs_hu                                                             *
*******************************************************************************/

#ifndef _t27p05_H_
#define _t27p05_H_

//==============================================================================
//
//                              DEFINES
//
//==============================================================================

#define PANEL_RATIO_16_9            0
#define PANEL_FUNC_INIT             panel_t27p05_init
#define PANEL_FUNC_SET_MIRROR       panel_t27p05_set_mirror
#define PANEL_FUNC_GET_MIRROR       panel_t27p05_get_mirror
#define PANEL_FUNC_SET_FLIP         panel_t27p05_set_flip
#define PANEL_FUNC_GET_FLIP         panel_t27p05_get_flip
#define PANEL_FUNC_SET_NARROW       panel_t27p05_set_narrow
#define PANEL_FUNC_GET_NARROW       panel_t27p05_get_narrow

//==============================================================================
//
//                              STRUCTURES
//
//==============================================================================



//==============================================================================
//
//                              EXTERNAL VARIABLES REFERENCE
//
//==============================================================================

//==============================================================================
//
//                              EXTERNAL FUNCTION PROTOTYPES
//
//==============================================================================

extern int panel_t27p05_set_mirror(u32 enable);
extern int panel_t27p05_get_mirror(void);
extern int panel_t27p05_set_flip(u32 enable);
extern int panel_t27p05_get_flip(void);
extern int panel_t27p05_set_narrow(u32 enable);
extern int panel_t27p05_get_narrow(void);

#endif /* _t27p05_H_ */
