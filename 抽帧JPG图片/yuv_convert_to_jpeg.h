#ifndef _YUV_CONVERT_TO_JPEG_H
#define _YUV_CONVERT_TO_JPEG_H

#include <libavutil/frame.h>

#ifdef __cplusplus
extern "C" {
#endif

int ffmpeg_convert_yuv_to_jpeg(unsigned char *yuv,int srcwidth,int srcheight,int realwidth,int realheight, const char *filename,int quality);

int save_yuv_data_to_jpg_from_yuvframe(AVFrame *yuvframe, const int width, const int height, const char *filename,int quality);

#ifdef __cplusplus
}
#endif

#endif //_YUV_CONVERT_TO_JPEG_H