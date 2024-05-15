#ifndef READ_H264_H
#define READ_H264_H
#include <libavformat/avformat.h>
//将视频流和G711a/u合成flv,支持自定义io
int H264_G711u(const char* filename, const char* inputH264,const char* outFilename);

void SetReadAudio_Callback(int(*read_Audio)(void *opaque, uint8_t *buf, int buf_size), void* args);

void SetReadVideo_Callback(int(*read_Video)(void *opaque, uint8_t *buf, int buf_size), void* args);

void SetSeekCallback(int64_t(*seek_packet)(void *opaque, int64_t offset, int whence));

void SetWriteCallback(int(*write_packet)(void *opaque, uint8_t *buf, int buf_size), void* args);

#endif
//H264_G711u("/home/ggboy/Desktop/LinuxGDB_RTMP/test.G711u", "/home/ggboy/Desktop/LinuxGDB_RTMP/test.h264", "test_G711u.flv");