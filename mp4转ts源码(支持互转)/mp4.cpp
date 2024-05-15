#include "mp4.h"
#include <stdio.h>
#include <iostream>
using namespace std;
extern "C"
{
#include "libavcodec/codec.h"
#include <libavformat/avformat.h>   
#include <libswscale/swscale.h>   
#include "libswresample/swresample.h"
#include <libavutil/opt.h>        
#include <libavutil/audio_fifo.h>
};

const int sampling_frequencies[] = {
	96000,  // 0x0
	88200,  // 0x1
	64000,  // 0x2
	48000,  // 0x3
	44100,  // 0x4
	32000,  // 0x5
	24000,  // 0x6
	22050,  // 0x7
	16000,  // 0x8
	12000,  // 0x9
	11025,  // 0xa
	8000   // 0xb
	// 0xc d e f是保留的
};

int adts_header(uint8_t * const p_adts_header, const int data_length,
	const int profile, const int samplerate,
	const int channels)
{

	int sampling_frequency_index = 3; // 默认使用48000hz
	int adtsLen = data_length + 7;

	int frequencies_size = sizeof(sampling_frequencies) / sizeof(sampling_frequencies[0]);
	int i = 0;
	for (i = 0; i < frequencies_size; i++)
	{
		if (sampling_frequencies[i] == samplerate)
		{
			sampling_frequency_index = i;
			break;
		}
	}
	if (i >= frequencies_size)
	{
		printf("unsupport samplerate:%d\n", samplerate);
		return -1;
	}

	p_adts_header[0] = 0xff;         //syncword:0xfff                          高8bits
	p_adts_header[1] = 0xf0;         //syncword:0xfff                          低4bits
	p_adts_header[1] |= (0 << 3);    //MPEG Version:0 for MPEG-4,1 for MPEG-2  1bit
	p_adts_header[1] |= (0 << 1);    //Layer:0                                 2bits
	p_adts_header[1] |= 1;           //protection absent:1                     1bit

	p_adts_header[2] = (profile) << 6;            //profile:profile               2bits
	p_adts_header[2] |= (sampling_frequency_index & 0x0f) << 2; //sampling frequency index:sampling_frequency_index  4bits
	p_adts_header[2] |= (0 << 1);             //private bit:0                   1bit
	p_adts_header[2] |= (channels & 0x04) >> 2; //channel configuration:channels  高1bit

	p_adts_header[3] = (channels & 0x03) << 6; //channel configuration:channels 低2bits
	p_adts_header[3] |= (0 << 5);               //original：0                1bit
	p_adts_header[3] |= (0 << 4);               //home：0                    1bit
	p_adts_header[3] |= (0 << 3);               //copyright id bit：0        1bit
	p_adts_header[3] |= (0 << 2);               //copyright id start：0      1bit
	p_adts_header[3] |= ((adtsLen & 0x1800) >> 11);           //frame length：value   高2bits

	p_adts_header[4] = (uint8_t)((adtsLen & 0x7f8) >> 3);     //frame length:value    中间8bits
	p_adts_header[5] = (uint8_t)((adtsLen & 0x7) << 5);       //frame length:value    低3bits
	p_adts_header[5] |= 0x1f;                                 //buffer fullness:0x7ff 高5bits
	p_adts_header[6] = 0xfc;      //11111100      //buffer fullness:0x7ff 低6bits
	// number_of_raw_data_blocks_in_frame：
	//    表示ADTS帧中有number_of_raw_data_blocks_in_frame + 1个AAC原始帧。

	return 0;
}


int streams_init(AVFormatContext *ifmt_ctx, AVFormatContext *ofmt_ctx)
{
	for (int i = 0; i < ifmt_ctx->nb_streams; ++i)
	{
		if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			AVStream *inStream = ifmt_ctx->streams[i];
			AVStream *outStream = avformat_new_stream(ofmt_ctx, NULL);


			if (!outStream)
			{
				printf("failed to allocate output stream\n");
				return -1;
			}



			if (avcodec_parameters_copy(outStream->codecpar, inStream->codecpar) < 0)
			{
				printf("faild to copy context from input to output stream");
				return -1;
			}

			outStream->codecpar->codec_tag = 0;

			break;
		}
	}
	for (int i = 0; i < ifmt_ctx->nb_streams; ++i)
	{
		if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			AVStream *inStream = ifmt_ctx->streams[i];
			AVStream *outStream = avformat_new_stream(ofmt_ctx, NULL);


			if (!outStream)
			{
				printf("failed to allocate output stream\n");
				return -1;
			}
			outStream->codecpar->codec_id = AV_CODEC_ID_AAC;
			outStream->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
			outStream->codecpar->format = 8;
			outStream->codecpar->bit_rate = 66316;
			outStream->codecpar->codec_tag = 0;
			outStream->codecpar->profile = 1;
			outStream->codecpar->frame_size = 1024;
			outStream->codecpar->sample_rate = 44100;
			outStream->codecpar->channels = 2;
			outStream->codecpar->channel_layout = 3;
			break;
		}
	}
	return 0;
}

static int open_input_file(const char *filename,
	AVFormatContext **input_format_context,
	AVCodecContext **input_codec_context)
{
	AVCodec *input_codec;
	int ret;
	if ((ret = avformat_open_input(input_format_context, filename, 0, 0)) < 0) {
		printf("Could not open input file.");
		return -1;
	}
	if ((ret = avformat_find_stream_info(*input_format_context, 0)) < 0) {
		printf("Failed to retrieve input stream information");
		return -1;
	}


	*input_codec_context = (*input_format_context)->streams[1]->codec;
	input_codec = avcodec_find_decoder((*input_codec_context)->codec_id);

	if (avcodec_open2(*input_codec_context, input_codec, 0) < 0)
	{
		printf("avcodec_open2 is error");
		return -1;
	}
	return 0;
}



static int open_output_file(const char *filename, AVFormatContext **output_format_context, AVCodecContext **output_codec_context)
{
	AVCodec *audio_codec = NULL;
	int ret;
	avformat_alloc_output_context2(output_format_context, NULL, NULL, filename);
	if (!output_format_context) {
		printf("Could not create output context\n");
		ret = AVERROR_UNKNOWN;
		return -1;
	}
	audio_codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
	if (audio_codec == NULL)
	{
		fprintf(stderr, "Codec not found!\n");
		return -1;
	}

	*output_codec_context = avcodec_alloc_context3(audio_codec);
	if (output_codec_context == NULL)
	{
		fprintf(stderr, "Could not allocate audio codec context!\n");
		return -1;
	}

	//put sample param
	(*output_codec_context)->bit_rate = 64000;
	(*output_codec_context)->sample_fmt = AV_SAMPLE_FMT_FLTP;
	(*output_codec_context)->sample_rate = 44100;
	(*output_codec_context)->channel_layout = 3;
	(*output_codec_context)->channels = 2;
	(*output_codec_context)->codec_type = AVMEDIA_TYPE_AUDIO;
	(*output_codec_context)->profile = FF_PROFILE_AAC_LOW;

	if (avio_open(&((*output_format_context)->pb), filename, AVIO_FLAG_WRITE) < 0)
	{
		return -1;
	}
	if (avcodec_open2(*output_codec_context, audio_codec, NULL) < 0)
	{
		return -1;
	}
	return 0;
}



static int init_output_frame(AVFrame **frame)
{
	int ret;
	if (!(*frame = av_frame_alloc()))
	{
		printf("init_output_frame is error");
		return -1;
	}

	(*frame)->format = AV_SAMPLE_FMT_FLTP;
	// 通道数
	(*frame)->channels = 2;

	// 通道布局
	(*frame)->channel_layout = AV_CH_LAYOUT_STEREO;

	// 每一帧音频的样本数量
	(*frame)->nb_samples = 1024;

	ret = av_frame_get_buffer(*frame, 0);
	if (ret < 0) {
		cout << "av_frame_get_buffer failed" << endl;
		return -1;
	}
	return 0;

}


static int init_swr_frame(AVFrame **swr_frame)
{
	int ret;
	if (!(*swr_frame = av_frame_alloc()))
	{
		printf("init_output_frame is error");
		return -1;
	}

	(*swr_frame)->format = AV_SAMPLE_FMT_FLTP;
	// 通道数
	(*swr_frame)->channels = 2;

	// 通道布局
	(*swr_frame)->channel_layout = AV_CH_LAYOUT_STEREO;

	// 每一帧音频的样本数量
	(*swr_frame)->nb_samples = 1024;

	ret = av_frame_get_buffer(*swr_frame, 0);
	if (ret < 0) {
		cout << "av_frame_get_buffer failed" << endl;
		return -1;
	}
	return 0;

}



static int init_resampler(AVCodecContext *input_codec_context,
	AVCodecContext *output_codec_context,
	SwrContext **resample_context)
{
	int ret;
	*resample_context = swr_alloc();
	av_opt_set_int(*resample_context, "in_channel_layout", AV_CH_LAYOUT_STEREO, 0);//AV_CH_LAYOUT_STEREO
	av_opt_set_int(*resample_context, "out_channel_layout", AV_CH_LAYOUT_STEREO, 0);//AV_CH_LAYOUT_STEREO
	av_opt_set_int(*resample_context, "in_sample_rate", input_codec_context->sample_rate, 0);//采样率
	av_opt_set_int(*resample_context, "out_sample_rate", output_codec_context->sample_rate, 0);
	av_opt_set_sample_fmt(*resample_context, "in_sample_fmt", input_codec_context->sample_fmt, 0);//AV_SAMPLE_FMT_FLTP
	av_opt_set_sample_fmt(*resample_context, "out_sample_fmt", output_codec_context->sample_fmt, 0);//AV_SAMPLE_FMT_FLTP
	swr_init(*resample_context);
	if (!*resample_context)
	{
		return -1;
	}
	return 0;

}



static int init_fifo(AVAudioFifo **fifo, AVCodecContext *output_codec_context)
{
	if (!(*fifo = av_audio_fifo_alloc(output_codec_context->sample_fmt,
		output_codec_context->channels, 1))) {

		return -1;
	}
	return 0;
}


static void video_muxer(AVFormatContext **input_format_context, AVFormatContext **output_format_context, AVPacket **input_pkt)
{
	AVStream *in_stream = (*input_format_context)->streams[(*input_pkt)->stream_index];
	AVStream *out_stream = (*output_format_context)->streams[(*input_pkt)->stream_index];

	(*input_pkt)->pts = av_rescale_q_rnd((*input_pkt)->pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
	(*input_pkt)->dts = av_rescale_q_rnd((*input_pkt)->dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
	(*input_pkt)->duration = av_rescale_q((*input_pkt)->duration, in_stream->time_base, out_stream->time_base);
	(*input_pkt)->pos = -1;

	if (av_interleaved_write_frame(*output_format_context, (*input_pkt)) < 0) {
		printf("Error muxing packet\n");

	}
	av_free_packet(*input_pkt);
}



static void write_audio_pts_dts(AVPacket **out_pkt, AVCodecContext *output_codec_context, AVStream *out_stream)
{
	if ((*out_pkt)->pts == AV_NOPTS_VALUE)
	{
		static int frameIndex = 0;
		//write PTS																									
		double calcDuration = 1024.0 / 44100.0;//aac一帧的时间									
		(*out_pkt)->pts = frameIndex * (double)calcDuration / (double)(av_q2d(output_codec_context->time_base)); //pts=duration x frameIndex
		(*out_pkt)->dts = (*out_pkt)->pts;
		(*out_pkt)->duration = (double)calcDuration / (double)(av_q2d(output_codec_context->time_base));//时间戳间隔=一帧的时间X当前时间基							
		frameIndex++;
	}

	(*out_pkt)->pts = av_rescale_q_rnd((*out_pkt)->pts, output_codec_context->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
	(*out_pkt)->dts = av_rescale_q_rnd((*out_pkt)->dts, output_codec_context->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
	(*out_pkt)->duration = av_rescale_q((*out_pkt)->duration, output_codec_context->time_base, out_stream->time_base);
	(*out_pkt)->pos = -1;
	(*out_pkt)->stream_index = 1;
}



static void merging_audio_packet(AVCodecContext *output_codec_context, AVPacket *pkt, AVPacket *pkt2)
{
	uint8_t adts_header_buf[7] = { 0 };

	adts_header(adts_header_buf, pkt->size,
		output_codec_context->profile,
		output_codec_context->sample_rate,
		output_codec_context->channels);

	av_new_packet(pkt2, pkt->size + 7);//动态分配ptk2的内存
	memset(pkt2->data, 0, pkt->size + 7);
	memcpy(pkt2->data, adts_header_buf, (sizeof(uint8_t) * 7));
	memcpy(pkt2->data + 7, pkt->data, pkt->size);
}



static void audio_decodec_encodec_write(AVFormatContext **output_format_context, AVCodecContext **output_codec_context, AVCodecContext **input_codec_context, SwrContext **swrContext, AVAudioFifo **audiofifo, AVPacket **pkt)
{
	AVFrame *swr_frame, *frame, *out_frame;
	AVPacket *pkt2;
	AVStream * out_stream = (*output_format_context)->streams[(*pkt)->stream_index];
	pkt2 = av_packet_alloc();
	init_swr_frame(&swr_frame);
	init_output_frame(&out_frame);
	frame = av_frame_alloc();


	int ret = avcodec_send_packet(*input_codec_context, *pkt);

	while (!ret)
	{
		ret = avcodec_receive_frame((*input_codec_context), frame);
		if (ret != 0) {
			break;
		}
		else {

			int len = swr_convert(*swrContext, swr_frame->data, 1024, (const uint8_t **)frame->data, frame->nb_samples);
			if (len <= 0) {
				cout << "重采样输出异常" << len << endl;
				break;
			}

			int cache_size = av_audio_fifo_size(*audiofifo);
			ret = av_audio_fifo_realloc(*audiofifo, swr_frame->nb_samples + cache_size);//分配缓冲区大小
			ret = av_audio_fifo_write(*audiofifo, reinterpret_cast<void **>(swr_frame->data), len);//写入len个采样点到缓冲区
			av_frame_make_writable(out_frame);

			int cur_pts = 0;
			while (av_audio_fifo_size(*audiofifo) >= (*output_codec_context)->frame_size)//当缓冲区慢1024个采样点时进行编码
			{

				ret = av_audio_fifo_read(*audiofifo, reinterpret_cast<void **>(out_frame->data),
					(*output_codec_context)->frame_size);//从缓冲区读取1024个采样点到out_frame

				ret = avcodec_send_frame(*output_codec_context, out_frame);
				if (ret != 0)continue;
				while (true)
				{

					ret = avcodec_receive_packet(*output_codec_context, *pkt);
					if (ret != 0)break;
					merging_audio_packet(*output_codec_context, *pkt, pkt2);
					write_audio_pts_dts(&pkt2, *output_codec_context, out_stream);
					if (av_interleaved_write_frame(*output_format_context, pkt2) < 0)
					{
						printf("Error muxing packet\n");
						break;
					}

				}
			}
		}
	}
}

int Mp4_transcodes_to_TS(const char *inputfile, const char *outputfile)
{
	AVCodecContext *video_codectx = NULL, *audio_codecctx = NULL;
	AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx;
	AVPacket *pkt = av_packet_alloc();
	AVCodec *audio_codec = NULL, *pcodec = NULL;
	SwrContext *swrContext = NULL;
	AVAudioFifo *audiofifo = NULL;


	if (open_input_file(inputfile, &ifmt_ctx, &video_codectx) < 0)
	{
		printf("open_input_file is error");
		return -1;
	}

	if (open_output_file(outputfile, &ofmt_ctx, &audio_codecctx) < 0)
	{
		printf("open_input_file is error");
		return -1;
	}
	if (streams_init(ifmt_ctx, ofmt_ctx) < 0)
	{
		printf("streams_init is error");
		return -1;
	}

	if (init_resampler(video_codectx, audio_codecctx, &swrContext) < 0)
	{
		printf("init_resampler is error");
		return -1;
	}

	if (init_fifo(&audiofifo, audio_codecctx) < 0)
	{
		printf("init_fifo is error");
		return -1;
	}

	if (avformat_write_header(ofmt_ctx, NULL) < 0)
	{
		printf("avformat_write_header is error");
		return -1;
	}

	while (0 <= av_read_frame(ifmt_ctx, pkt)) {
		/*如果是视频就复用*/
		if (pkt->stream_index == 0)
		{
			video_muxer(&ifmt_ctx, &ofmt_ctx, &pkt);
		}
		/*如果是音频就转码*/
		if (pkt->stream_index == 1)
		{
			audio_decodec_encodec_write(&ofmt_ctx, &audio_codecctx, &video_codectx, &swrContext, &audiofifo, &pkt);
		}
	}
	av_write_trailer(ofmt_ctx);//写文件尾

	avformat_close_input(&ifmt_ctx);
	avformat_free_context(ofmt_ctx);
	av_free_packet(pkt);
	swr_free(&swrContext);
	return 0;
}