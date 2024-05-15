
#ifndef Ts_CONVERT_MP4 
#define Ts_CONVERT_MP4

extern "C"
{
#include <libavformat/avformat.h>   
#include <libswscale/swscale.h>   
#include "libswresample/swresample.h"
#include <libavutil/opt.h>        
#include <libavutil/audio_fifo.h>
#include "libavutil/imgutils.h"
};

int TS_Format_conversion_MP4(const char* inputfile, const char* outputfile);
#endif

