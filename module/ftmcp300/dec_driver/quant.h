#ifndef _QUANT_H_
#define _QUANT_H_

#include "portab.h"
#include "define.h"
#include "global.h"

void set_qp(VideoParams *p_Vid);
void assign_quant_params(DecoderParams *p_Dec, VideoParams *p_Vid);

#endif /* _QUANT_H_ */

