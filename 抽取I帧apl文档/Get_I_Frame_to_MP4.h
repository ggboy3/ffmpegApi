
#ifndef FF_MYDIR  
#define FF_MYDIR
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavformat/avio.h"
#include "libswscale/swscale.h"
#include "libavutil/log.h"
}
int extrat_I_s_frameToMP4(const char **in_files, int infile_size,const char* out_MP4file,int fps);

#endif // !EXTRAT_FRAMETOMP4_H

