#ifndef _CUT_FRAME_H_
#define _CUT_FRAME_H_

typedef struct _cut_info_
{
    int64_t cut_time_point;
    int     cut_result;
    char    cut_file_name[256];
}cut_info;

typedef struct _cut_ctx_s_ {
    AVFormatContext *fmt_ctx;
    AVCodecContext *video_dec_ctx;
    int width;
    int height;
    enum AVPixelFormat pix_fmt;
    AVStream *video_stream;
    int dest_xpos;
    int dest_ypos;
    int dest_width;
    int dest_height;

    uint8_t *video_dst_data[4];
    int video_dst_linesize[4];
    int video_dst_bufsize;

    int video_stream_idx;
    AVFrame *frame;
    AVPacket pkt;
    int video_frame_count;

    int refcount;
} cut_ctx_s;

/*
 *  抽帧
 *  x 左上角x原点坐标
 *  y 左上角y原点坐标
 *  w 图片宽
 *  h 图片高
 *  cut_time_us 抽帧时间
 *  src_file 抽帧的视频文件
 *  dest_file 抽帧的jpeg文件
 */

int convert_percent_to_ffmepg(int value);

int64_t get_first_video_packet_us(cut_ctx_s *cut_ctx);

int ffmpeg_cut_frame( int x,int y,int w,int h,int64_t cut_time_us,const char *src_file,const char *dest_file, int compressibility);

/*
 *  批量抽帧
 *  x 左上角x原点坐标
 *  y 左上角y原点坐标
 *  w 图片宽
 *  h 图片高
 *  cut_array 抽帧上下文
 *  src_file 抽帧的视频文件
 */
int ffmpeg_cut_frame_batch(int x, int y, int w, int h, cut_info* cut_array, int cut_array_size, const char *src_media_file, int compressibility);


#endif //_CUT_FRAME_H_

//测试程序1
//cut_info inof;
//strcpy(inof.cut_file_name, "666.jpg");
//inof.cut_time_point = 5000;
//
//ffmpeg_cut_frame_batch(1920, 1080, 1920, 1080, &inof, 1, "0_20230809121935-272.ts", "666.jpg", 0);

//测试程序2
//cut_info *cutInfo_ = new cut_info[10];
//
//for (size_t i = 0; i < 3; i++)
//{
//	sprintf(cutInfo_[i].cut_file_name, "/home/ggboy/Desktop/LinuxGDB_RTMP/test%d.jpg", i);
//	cutInfo_[i].cut_result = 0;
//	if (i == 0)cutInfo_[i].cut_time_point = 30 * 1000;
//	if (i == 1)cutInfo_[i].cut_time_point = 60 * 1000;
//	if (i == 2)cutInfo_[i].cut_time_point = 120 * 1000;
//}
//
//ffmpeg_cut_frame_batch(0,
//	0,
//	0,
//	0,
//	cutInfo_,
//	3,
//	"/home/ggboy/Desktop/LinuxGDB_RTMP/666.mp4",
//	2);
//delete[] cutInfo_;