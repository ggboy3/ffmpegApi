#pragma once
#include <thread>
#include <mutex>
#include "FFmpegServer.h"
extern "C"
{
#include <libavutil/avassert.h>
#include <libavutil/opt.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/imgutils.h>
#include <libavfilter/buffersink.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}
using namespace std;
typedef struct Stream
{
	int pos_x;
	int pos_y;
	int width;
	int height;
	int format;//²Î¿¼AVPixelFormat
	AVFilterContext* buffersrc_ctx;
	const AVFilter* buffersrc;
	AVFilterInOut* output;
	AVFrame* inputFrame;
} Stream;
typedef struct Merge
{
	AVFilterGraph* filter_graph;
	AVFilterContext* buffersink_ctx;
	const AVFilter* buffersink;
	AVFilterInOut* input;
	const char* filters_descr;
	AVFrame* outputFrame;
	unsigned char* outputBuffer;
	int outputWidth;
	int outputHeight;
	int outputFormat;//²Î¿¼AVPixelFormat
	Stream* streams[128];
	int streamCount;
} Merge;
typedef struct OutputStream {
	AVStream* stream;
	AVCodecContext* codec_ctx;
	AVFrame* frame;
	AVPacket* packet;
	struct SwrContext* swr_ctx;
} OutputStream;
class MergerServer
{
public:
	MergerServer();
	~MergerServer();
private:
	bool isrun_merge = false;
	Merge* mergeVideo;
	Stream* inputStream[5];
	FFmpegServer* decode_video[5];
	FFmpegServer* decode_audio;
public:
	int MergeStartup(const char* url, const char* audio_url, VideoInfo* video);
	int ChangeMergeVideo(VideoInfo video1);
private:
	Merge* MergeCreate(int outputWidth, int outputHeight, int outputFormat);
	Stream* MergeAddStream(Merge* merge, int x, int y, int width, int height, int format);
	int MergeInit(Merge* merge);
	static void MergeDeinit(Merge* merge);
	void MergeWriteStream(Stream* stream, const unsigned char* buffer, int timestamp);
	const unsigned char* MergeMerge(Merge* merge);
	int ReloadVideo(const char* audio_url);
	int EncodecInit(const char* out_url);
	int AddStream(OutputStream* ost, AVFormatContext* oc, const AVCodec** codec, const char* encodec_name);
private:
	AVFormatContext * oformat_ctx;
	OutputStream video_st = { 0 };
	OutputStream audio_st = { 0 };

};
