#pragma once
/**
* @projectName   AudioCodec
* @brief         编解码音频
* @author        zhu yi jun
* @date          2023-11-4
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
#include <libavutil/mem.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>        
#include <libavutil/audio_fifo.h>
#include <libavcodec/avcodec.h>
	
}

class CPcmAlawCodec
{
public:
	CPcmAlawCodec()
	{
		Init();
	}
	~CPcmAlawCodec()
	{
		Uinit();
	}

	typedef struct {
		enum AVCodecID CodecId ;    // 帧数据类型 注意：目前只支持 AV_CODEC_ID_PCM_ALAW 和 AV_CODEC_ID_PCM
		uint8_t* frameBuffer;       // 数据指针，注意：谁申请的内存，谁释放。作为输出结构体时，最小分配内存：输入字节 * 输出声道 * 输出采样率 / 输入采样率 * 输出采样位数 / 输入采样位数
		uint32_t frameBufferSize;   // 数据大小 
		int Channel;				// 通道数
		enum AVSampleFormat SampleBit;				// 采样位
		int SampleRate;				// 采样率
	}FrameData;
	
	int PcmAlawDecode(FrameData * in_data, FrameData * out_data);
	
	int PcmEncode(FrameData * in_data, FrameData * out_data);
	
private:
	void Init();
	
	void Uinit();
	
	int init_resampler(FrameData *&inData,
		FrameData * &outData,
		SwrContext * &swr_context);
	
	void decode(AVCodecContext *dec_ctx,AVPacket *pkt,AVFrame *frame,FrameData * &out_data);
	
	int init_swr_frame(AVFrame *decoded_frame, AVFrame *&frame, FrameData *outData);
	
	int encode(AVCodecContext *ctx, AVFrame *frame, AVPacket *pkt, FrameData * &out_data);
	
	
	SwrContext * resample_context;
	SwrContext * resample_context2;
	AVFrame *decoded_frame;
	AVFrame *encoded_frame;
	AVFrame *swr_frame;
	AVFrame *swr_frame2;
	AVPacket *pkt;
	AVPacket *pkt2;

};

//G711a解码
//int main(int argc, char** argv) {
//	FILE *infile = NULL;
//	FILE *outfile = NULL;
//	CPcmAlawCodec decode;
//
//	int len = 0;
//	infile = fopen("/home/ggboy/Desktop/LinuxGDB_RTMP/Debug/666.G711a", "rb");
//	if (!infile) {
//		
//		exit(1);
//	}
//	
//	outfile = fopen("./666.pcm", "ab");
//	if (!outfile) {
//		exit(1);
//	}	
//	CPcmAlawCodec::FrameData OutData;//创建输出结构体，自己分配内存
//	OutData.frameBuffer = new uint8_t[800 * 2 * 32000 / 8000 * (16 / 8)];//输入字节 * 输出声道 * 输出采样率 / 输入采样率 * 输出采样位数 / 输入采样位数
//
//	CPcmAlawCodec::FrameData InData;
//	InData.frameBuffer = new uint8_t[800];
//	while (true)
//	{
//	
//		len = fread(InData.frameBuffer, 1, 800, infile);
//		if (len <= 0)
//		{
//			break;
//		}
//		//初始化输入参数
//		InData.frameBufferSize = len;//输入字节
//		InData.SampleBit = AV_SAMPLE_FMT_S16;//采样格式
//		InData.SampleRate = 8000;//采样率
//		InData.CodecId = AV_CODEC_ID_PCM_ALAW;//G711a音频解码器
//		InData.Channel = 1;//通道
//	
//		//初始化输出的pcm数据
//		OutData.Channel = 2;//需要的通道
//		OutData.SampleRate = 32000;//需要的采样率
//		OutData.SampleBit = AV_SAMPLE_FMT_S16P;//需要的采用格式
//	
//		decode.PcmAlawDecode(&InData, &OutData);//G711a数据解码PCM并重采样
//		fwrite(OutData.frameBuffer, 1, OutData.frameBufferSize, outfile);//把pcm写到本地文件
//	}
//	fclose(infile);
//	fclose(outfile);
//	delete[] OutData.frameBuffer;
//	delete[] InData.frameBuffer;
//}

//pcm编码G711a
//int main(int argc, char** argv) {
//	FILE *infile = NULL;
//	FILE *outfile = NULL;
//	CPcmAlawCodec decode;
//
//	int len = 0;
//	infile = fopen("/home/ggboy/Desktop/LinuxGDB_RTMP/Debug/audio_src.pcm", "rb");
//	if (!infile) {
//		
//		exit(1);
//	}
//	
//	outfile = fopen("./audio_src.G711a", "ab");
//	if (!outfile) {
//		exit(1);
//	}	
//	CPcmAlawCodec::FrameData OutData;//创建输出结构体，自己分配内存
//	OutData.frameBuffer = new uint8_t[2560];//和输入的内存空间一样就行了
//
//	CPcmAlawCodec::FrameData InData;
//	InData.frameBuffer = new uint8_t[2560];
//	while (true)
//	{
//	
//		len = fread(InData.frameBuffer, 1, 2560, infile);
//		if (len <= 0)
//		{
//			break;
//		}
//		//初始化输入参数
//		InData.frameBufferSize = len;//输入字节
//		InData.SampleBit = AV_SAMPLE_FMT_S16;//采样格式
//		InData.SampleRate = 32000;//采样率
//		InData.Channel = 1;//通道
//	
//		//初始化输出的pcm数据
//		OutData.Channel = 1;//需要的通道
//		OutData.SampleRate = 8000;//需要的采样率
//		OutData.SampleBit = AV_SAMPLE_FMT_S16P;//需要的采用格式
//		OutData.CodecId = AV_CODEC_ID_PCM_ALAW;//G711a编码器
//	
//		decode.PcmEncode(&InData, &OutData);
//		fwrite(OutData.frameBuffer, 1, OutData.frameBufferSize, outfile);//把pcm写到本地文件
//	}
//	fclose(infile);
//	fclose(outfile);
//	delete[] OutData.frameBuffer;
//	delete[] InData.frameBuffer;
//}