#ifndef RECORD_PARSE_H
#define RECORD_PARSE_H

#define HEAD_BYTES		(4 * 1024)		// 文件头
#define INDEX_BYTES		(4 * 1024 * 1024)	// 索引
#define INDEX_MAXNUM		(3600 * 4)
#include <iostream>
#include <vector>
using namespace std;

typedef struct _frame_info_
{
        int64_t PTS;
        int64_t FrameNum;
        int     i_frame;
	    int    decode = 0;//解码引用数
}frame_info;

 
int read_packet(void *opaque, uint8_t *buf, int buf_size);

#endif // RECORD_PARSE_H
