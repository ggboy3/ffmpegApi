
#ifndef CUT_FRAME_SBH
#define CUT_FRAME_SBH
#include <iostream>
#include <vector>
#include "record_parse.h"
extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/time.h>
#include "yuv_convert_to_jpeg.h"
}
;
typedef struct _cut_info_sbh
{
	int64_t cut_time_point;
	int     cut_result = 0;
	char    cut_file_name[256];
}cut_info_;

int cut_frame_batch_sbh(const char* inputfile,cut_info_ *cutinfo , int cut_array_size);
#endif

//int main(int argc, char** argv) {
//	cut_info_ CutBatch[10];
//	for (size_t i = 0; i < 3; i++)
//	{
//		sprintf(CutBatch[i].cut_file_name, "/home/ggboy/Desktop/device/kernel/source/test/linux/test%d.jpg", i);
//		if (i == 0)CutBatch[i].cut_time_point = 1696792422002847;
//		if (i == 1)CutBatch[i].cut_time_point = 1696792517659525;
//		if (i == 2)CutBatch[i].cut_time_point = 1696792720649307;
//	}
//	cut_frame_batch_sbh("/home/ggboy/Desktop/device/kernel/source/test/linux/recordex-20230101-020016-0012.mdvx", CutBatch, 3);
//}