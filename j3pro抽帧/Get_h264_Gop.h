#ifndef FF_GET_H264  
#define FF_GET_H264

#define ONE_SECOND 1/av_q2d(in_stream->time_base)
extern "C"
{
#include "libavformat/avformat.h"

}
typedef struct _cut_info_
{
	int64_t cut_time_point;
	int     cut_result;
	char    cut_file_name[256];
}cut_info;
enum seek_mode {
	PRECISE_SEEK_TIME = 1,
	ROUGH_SEEK_TIME = 0,
};
class init_file
{
public:
	int initAVFContext(AVFormatContext **pFormatCtx, const char *fileName);
};
int extrat_Gop_frame_To_h264(cut_info* cut_array, int pick_num, const char *in_files,int mode);
#endif 
