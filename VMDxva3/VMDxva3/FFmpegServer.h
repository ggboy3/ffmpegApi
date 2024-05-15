#pragma once
#include <map>
#include <mutex>
#include <thread>
//#include "MergerServer.h"
using namespace std;
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libavutil/imgutils.h"
#include "libavutil/time.h"
#include "libavutil/log.h"
#include "libavutil/avstring.h"
}
typedef struct VideoInfo
{
	const char* url;
	int index;
}VideoInfo;

typedef struct VideoContext
{
	AVFormatContext* iformat_ctx = nullptr;
	AVCodecContext* vdecodec_ctx = nullptr;
	const AVCodec* vdecoder = nullptr;
	AVPacket* vpacket = nullptr;
	AVFrame* vframe = nullptr;
	AVStream* vstream = nullptr;
	VideoInfo video_info;
	uint8_t* vdata = nullptr;
	int vindex = 0;
	bool isrun = false;
	bool isdispose = false;
	bool video_open_error = true;
}VideoContext; 
typedef struct AudioContext
{
	AVFormatContext* iformat_ctx = nullptr;
	AVCodecContext* adecodec_ctx = nullptr;
	const AVCodec* adecoder = nullptr;
	AVPacket* apacket = nullptr;
	AVFrame* aframe = nullptr;
	AVStream* astream = nullptr;
	const char* url = nullptr;
	int aindex = 0;
	bool isrun = false;
	bool isdispose = false;
	bool audio_open_error = true;
}AudioContext;
class FFmpegServer
{
public:
	FFmpegServer();
	~FFmpegServer();
public:
	map<int, VideoContext> video_map;
	AudioContext audio;
private:
	VideoContext video;
	void StartDecode(VideoContext* v_ctx);
public:
	int AudioDecoder(const char* url);
	int VideoDecoder(const char* url, int index);

private:
	int hw_decoder_init(AVCodecContext* ctx, const enum AVHWDeviceType type);
	int video_decode_read(VideoContext* v_ctx);
	int audio_decode_read(AudioContext* v_ctx);
private:
	AVBufferRef* hw_device_ctx;
	enum AVHWDeviceType decodecType;
};
