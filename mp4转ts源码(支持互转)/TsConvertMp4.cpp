#include "TsConvertMp4.h"
#include <stdio.h>




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

int streams_init(AVFormatContext *ifmt_ctx, AVFormatContext *ofmt_ctx, AVCodecContext **video2_codec_context, AVCodecContext **audio_codec_context)
{
	int ret = 0, streams_flag = 0, streams_audio = 0, streams_video = 0;
	if (ifmt_ctx->streams[0]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
	{
		streams_flag = 1;
	}
	for (int i = 0; i <= ifmt_ctx->nb_streams; ++i)
	{

		if (streams_flag == 1 && streams_audio == 0)
		{
			for (int i = 0; i < ifmt_ctx->nb_streams; ++i)
			{
				if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && ifmt_ctx->streams[i]->codecpar->sample_rate != 0)
				{
					AVStream *inStream = ifmt_ctx->streams[i];
					AVStream *outStream = avformat_new_stream(ofmt_ctx, NULL);

					if (!outStream)
					{
						printf("failed to allocate output stream\n");
						return -1;
					}

					if (inStream->codec->codec_id == AV_CODEC_ID_AAC)//判断输入音频是否需要编码，不需要则复制>codecpar
					{
						if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
							(*audio_codec_context)->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
						outStream->codecpar->codec_id = inStream->codecpar->codec_id;
						outStream->codecpar->codec_type = inStream->codecpar->codec_type;
						outStream->codecpar->format = inStream->codecpar->format;
						outStream->codecpar->bit_rate = inStream->codecpar->bit_rate;
						outStream->codecpar->codec_tag = 0;
						outStream->codecpar->profile = inStream->codecpar->profile;
						outStream->codecpar->frame_size = inStream->codecpar->frame_size;
						outStream->codecpar->sample_rate = inStream->codecpar->sample_rate;
						outStream->codecpar->channels = inStream->codecpar->channels;
						outStream->codecpar->channel_layout = inStream->codecpar->channel_layout;
						streams_flag = 0;
						streams_audio = 1;
					}
					else
					{
						(*audio_codec_context)->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
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
						streams_flag = 0;
						streams_audio = 1;
					}
					break;

				}
			}
		}
		if (streams_flag == 0 && streams_video == 0)
		{
			for (int i = 0; i < ifmt_ctx->nb_streams; ++i)
			{
				if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
				{
					AVStream *inStream = ifmt_ctx->streams[i];
					AVStream *outStream = avformat_new_stream(ofmt_ctx, 0);
					
					if (!outStream)
					{
						printf("failed to allocate output stream\n");
						return -1;
					}
					if (inStream->codec->codec_id == AV_CODEC_ID_H264)//如果输入码流不是h264，需要转码，不能把inStream->codecpar的参数给video2_codec_context
					{
						ret = avcodec_parameters_to_context(*video2_codec_context, inStream->codecpar);
						if (ret < 0) {
							printf("Failed to copy in_stream codecpar to codec context\n");
							return -1;
						}
					}
					(*video2_codec_context)->codec_tag = 0;
					if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
						(*video2_codec_context)->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

					ret = avcodec_parameters_from_context(outStream->codecpar, (*video2_codec_context));
					if (ret < 0) {
						printf("Failed to copy codec context to out_stream codecpar context\n");
						return -1;
					}

					ret = i;//返回视频流的id
					streams_flag = 1;
					streams_video = 1;
					break;

				}
			}
		}
	}
	return ret;
}

static int open_input_file(const char *filename,
	AVFormatContext **input_format_context,
	AVCodecContext **input_codec_context, AVCodecContext **video_codec_context)
{
	AVCodec *audio_codec = NULL;
	AVCodec *video_codec = NULL;
	int ret;
	int flag = 0;
	av_register_all();//初始化所有编解码器

	if ((ret = avformat_open_input(input_format_context, filename, 0, 0)) < 0) {
		printf("Could not open input file.");
		return -1;
	}
	if ((ret = avformat_find_stream_info(*input_format_context, 0)) < 0) {
		printf("Failed to retrieve input stream information");
		return -1;
	}
	for (int i = 0; i < (*input_format_context)->nb_streams; ++i)
	{
		if ((*input_format_context)->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && flag == 0)//打开视频解码器
		{
			flag = 1;
			video_codec = avcodec_find_decoder((*input_format_context)->streams[i]->codecpar->codec_id);
			*video_codec_context = avcodec_alloc_context3(video_codec);

			(*video_codec_context)->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
			avcodec_parameters_to_context(*video_codec_context, (*input_format_context)->streams[i]->codecpar);

			if (avcodec_open2(*video_codec_context, video_codec, 0) < 0)
			{
				printf("avcodec_open2 is error");
				return -1;
			}

		}
		if ((*input_format_context)->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)//打开音频解码器
		{
			*input_codec_context = (*input_format_context)->streams[i]->codec;
			audio_codec = avcodec_find_decoder((*input_codec_context)->codec_id);

			if (avcodec_open2(*input_codec_context, audio_codec, 0) < 0)
			{
				printf("avcodec_open2 is error");
				return -1;
			}

		}

	}
	return 0;
}



static int open_output_file(const char *filename,
	AVFormatContext **output_format_context,
	AVFormatContext **intput_format_context,
	AVCodecContext **audio_codec_context,
	AVCodecContext **video_codec_context)
{
	AVCodec *audio_codec = NULL;
	AVCodec *video_codec = NULL;
	int ret;
	int flag = 0;
	avformat_alloc_output_context2(output_format_context, NULL, NULL, filename);
	if (!output_format_context) {
		printf("Could not create output context\n");
		ret = AVERROR_UNKNOWN;
		return -1;
	}


	audio_codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
	if (audio_codec == NULL)
	{
		printf("Codec not found!\n");
		return -1;
	}

	*audio_codec_context = avcodec_alloc_context3(audio_codec);
	if (audio_codec_context == NULL)
	{
		printf("Could not allocate audio codec context!\n");
		return -1;
	}
	video_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
	if (video_codec == NULL)
	{
		printf("Codec not found!\n");
		return -1;
	}

	*video_codec_context = avcodec_alloc_context3(video_codec);
	if (video_codec_context == NULL)
	{
		printf("Could not allocate audio codec context!\n");
		return -1;
	}

	//设置音频编码参数
	(*audio_codec_context)->bit_rate = 64000;
	(*audio_codec_context)->sample_fmt = AV_SAMPLE_FMT_FLTP;
	(*audio_codec_context)->sample_rate = 44100;
	(*audio_codec_context)->channel_layout = AV_CH_LAYOUT_STEREO;
	(*audio_codec_context)->channels = 2;
	(*audio_codec_context)->codec_type = AVMEDIA_TYPE_AUDIO;
	(*audio_codec_context)->profile = FF_PROFILE_AAC_LOW;

	//设置视频编码器参数
	(*video_codec_context)->pix_fmt = AV_PIX_FMT_YUV420P;
	for (int i = 0; i < (*intput_format_context)->nb_streams; ++i)
	{
		if ((*intput_format_context)->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && flag == 0)
		{
			flag = 1;
			(*video_codec_context)->width = (*intput_format_context)->streams[i]->codecpar->width;
			(*video_codec_context)->height = (*intput_format_context)->streams[i]->codecpar->height;
		}
	}
	AVRational time_base1={ 1,24 }, framerate={ 24,1 };
	(*video_codec_context)->time_base = time_base1;
	(*video_codec_context)->framerate = framerate;
	(*video_codec_context)->bit_rate = 400000;
	(*video_codec_context)->gop_size = 13;//两个I帧之间的帧数目,一组GOP
	(*video_codec_context)->slices = 2;
	(*video_codec_context)->max_b_frames = 0;

	(*video_codec_context)->flags = 0;
	if ((*output_format_context)->oformat->flags &AVFMT_GLOBALHEADER)//确保extradata被写入
		(*video_codec_context)->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	if (avio_open(&((*output_format_context)->pb), filename, AVIO_FLAG_READ_WRITE) < 0)
	{
		return -1;
	}
	if (avcodec_open2(*audio_codec_context, audio_codec, NULL) < 0)
	{
		return -1;
	}
	if (avcodec_open2(*video_codec_context, video_codec, NULL) < 0)
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
		//cout << "av_frame_get_buffer failed" << endl;
		return -1;
	}
	return 0;

}


static int init_swr_frame(AVFrame **swr_frame, AVCodecContext *input_codec_context)
{
	int ret;
	if (!(*swr_frame = av_frame_alloc()))
	{
		printf("init_output_frame is error");
		return -1;
	}

	(*swr_frame)->format = input_codec_context->sample_fmt;
	// 通道数
	(*swr_frame)->channels = input_codec_context->channels;

	// 通道布局
	(*swr_frame)->channel_layout = input_codec_context->channel_layout;

	// 每一帧音频的样本数量
	(*swr_frame)->nb_samples = 1024;

	ret = av_frame_get_buffer(*swr_frame, 0);
	if (ret < 0) {
		//cout << "av_frame_get_buffer failed" << endl;
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
	av_opt_set_int(*resample_context, "in_channel_layout", input_codec_context->channel_layout, 0);//AV_CH_LAYOUT_STEREO
	av_opt_set_int(*resample_context, "out_channel_layout", input_codec_context->channel_layout, 0);//AV_CH_LAYOUT_STEREO
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


static int init_fifo(AVAudioFifo **fifo, AVCodecContext *input_codec_context)
{
	if (!(*fifo = av_audio_fifo_alloc(input_codec_context->sample_fmt,
		input_codec_context->channels, 1))) {

		return -1;
	}
	return 0;
}


static void video_muxer(AVFormatContext **input_format_context, AVFormatContext **output_format_context, AVPacket **input_pkt)
{
	static int frameIndex = 0;
	AVStream *in_stream = (*input_format_context)->streams[(*input_pkt)->stream_index];
	AVStream *out_stream = (*output_format_context)->streams[(*input_pkt)->stream_index];
	//if ((*input_pkt)->pts == AV_NOPTS_VALUE)
	{
		//write PTS
		AVRational timeBase1 = in_stream->time_base;
		AVRational time_base1 = { 23,1 };
		//Duration between 2 frames
		double calcDuration = 1.0 / av_q2d(time_base1);

		(*input_pkt)->pts = (double)(frameIndex*calcDuration) / (double)(av_q2d(timeBase1));
		(*input_pkt)->dts = (*input_pkt)->pts;
		(*input_pkt)->duration = (double)calcDuration / (double)(av_q2d(timeBase1));
		frameIndex++;
	}
	(*input_pkt)->pts = av_rescale_q_rnd((*input_pkt)->pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
	(*input_pkt)->dts = av_rescale_q_rnd((*input_pkt)->dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
	(*input_pkt)->duration = av_rescale_q((*input_pkt)->duration, in_stream->time_base, out_stream->time_base);
	(*input_pkt)->pos = -1;

	printf("============================================\n");
	printf("video %ld\n", (*input_pkt)->pts);
	printf("video %ld\n", (*input_pkt)->dts);
	printf("============================================\n");
	if (av_interleaved_write_frame(*output_format_context, (*input_pkt)) < 0) {
		printf("Error muxing packet\n");


	}
	av_free_packet(*input_pkt);
}

static void video_encodec_muxer(AVFormatContext **input_format_context, AVFormatContext **output_format_context, AVPacket **input_pkt)
{
	static int frameIndex = 0;
	AVStream *in_stream = (*input_format_context)->streams[(*input_pkt)->stream_index];
	AVStream *out_stream = (*output_format_context)->streams[(*input_pkt)->stream_index];

	//write PTS
	AVRational timeBase1 = in_stream->time_base;
	//Duration between 2 frames

	double calcDuration = 1.0 / av_q2d(in_stream->r_frame_rate);
	(*input_pkt)->pts = (double)(frameIndex*calcDuration) / (double)(av_q2d(timeBase1));
	(*input_pkt)->dts = (*input_pkt)->pts;
	(*input_pkt)->duration = (double)calcDuration / (double)(av_q2d(timeBase1));
	frameIndex++;

	(*input_pkt)->pts = av_rescale_q_rnd((*input_pkt)->pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
	(*input_pkt)->dts = av_rescale_q_rnd((*input_pkt)->dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
	(*input_pkt)->duration = av_rescale_q((*input_pkt)->duration, in_stream->time_base, out_stream->time_base);
	(*input_pkt)->pos = -1;
	printf("============================================");
	printf("video %ld\n", (*input_pkt)->pts);
	printf("video %ld\n", (*input_pkt)->dts);
	printf("============================================");
	if (av_interleaved_write_frame(*output_format_context, (*input_pkt)) < 0) {
		printf("Error muxing packet\n");


	}

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
	(*out_pkt)->stream_index = (out_stream->index);
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
	init_swr_frame(&swr_frame, *input_codec_context);
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
				//cout << "重采样输出异常" << len << endl;
				break;
			}

			int cache_size = av_audio_fifo_size(*audiofifo);
			ret = av_audio_fifo_realloc(*audiofifo, swr_frame->nb_samples + cache_size);//分配缓冲区大小
			ret = av_audio_fifo_write(*audiofifo, (void **)(swr_frame->data), len);//写入len个采样点到缓冲区
			av_frame_make_writable(out_frame);

			while (av_audio_fifo_size(*audiofifo) >= (*output_codec_context)->frame_size)//当缓冲区慢1024个采样点时进行编码
			{

				ret = av_audio_fifo_read(*audiofifo, (void **)(out_frame->data),
					(*output_codec_context)->frame_size);//从缓冲区读取1024个采样点到out_frame
				ret = avcodec_send_frame(*output_codec_context, out_frame);
				if (ret != 0)continue;
				while (1)
				{
					ret = avcodec_receive_packet(*output_codec_context, *pkt);
					if (ret != 0)break;
					if (strcmp((*output_format_context)->oformat->name, "mpegts") != 0)
					{
						write_audio_pts_dts(pkt, *output_codec_context, out_stream);
						if (av_interleaved_write_frame(*output_format_context, *pkt) < 0)
						{
							printf("Error muxing packet\n");
							break;
						}
					}
					else
					{
						merging_audio_packet(*output_codec_context, *pkt, pkt2);
						write_audio_pts_dts(&pkt2, *output_codec_context, out_stream);
						if (av_interleaved_write_frame(*output_format_context, pkt2) < 0)
						{
							printf("Error muxing packet\n");
							break;
						}
					}
					av_frame_free(&swr_frame);
					av_frame_free(&out_frame);
					av_frame_free(&frame);
				}
			}
		}
	}
}
static int video_decodec_encodec_write(AVFormatContext **output_format_context, AVFormatContext **input_format_context, AVCodecContext **output_codec_context, AVCodecContext **input_codec_context, AVPacket **pkt)
{
	int ret;

	AVBSFContext *bsf_ctx, *bsf_ctx2;
	AVFrame *video_frame = av_frame_alloc();
	AVPacket  *video_pkt = av_packet_alloc();

	const AVBitStreamFilter *filter2 = av_bsf_get_by_name("h264_mp4toannexb");
	if (!filter2)
	{
		av_log(NULL, AV_LOG_ERROR, "Unkonw bitstream filter");
	}

	av_bsf_alloc(filter2, &bsf_ctx2);


	ret = avcodec_send_packet(*input_codec_context, *pkt);
	if (ret != 0)
	{
		printf("avcodec_send_packet is error to video");
		return -1;
	}
	while (1)
	{
		ret = avcodec_receive_frame(*input_codec_context, video_frame);
		if (ret != 0)
		{
			break;
		}
		else
		{
			ret = avcodec_send_frame(*output_codec_context, video_frame);
			av_frame_unref(video_frame);
			if (ret != 0)
			{
				printf("avcodec_send_frame is error to video");
				return -1;
			}
			while (1)
			{
				ret = avcodec_receive_packet(*output_codec_context, video_pkt);
				if (ret != 0)
				{
					break;
				}
				else
				{
					ret = av_bsf_send_packet(bsf_ctx2, video_pkt);
					if (ret < 0)
					{
						printf("av_bsf_send_packet is error\n");
						av_packet_unref(video_pkt);

					}
					ret = av_bsf_receive_packet(bsf_ctx2, video_pkt);
					if (ret < 0)
					{
						printf("av_bsf_receive_packet is error\n");
					}
					video_encodec_muxer(input_format_context, output_format_context, &video_pkt);
					av_free_packet(*pkt);
				}
				return 1;
			}
		}
	}
}

static int audio_muxer(AVFormatContext **output_format_context, AVFormatContext **input_format_context, AVCodecContext **input_codec_context, AVPacket **pkt)
{
	AVPacket *pkt2 = av_packet_alloc();
	AVStream *in_stream = (*input_format_context)->streams[(*pkt)->stream_index];
	AVStream *out_stream = (*output_format_context)->streams[((*pkt)->stream_index)];

	if (strcmp((*output_format_context)->oformat->name, "mpegts") != 0)
	{

		(*pkt)->pts = av_rescale_q_rnd((*pkt)->pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		(*pkt)->dts = av_rescale_q_rnd((*pkt)->dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		(*pkt)->duration = av_rescale_q((*pkt)->duration, in_stream->time_base, out_stream->time_base) / 2;
		(*pkt)->pos = -1;

		printf("============================================\n");
		printf("audio %ld\n", (*pkt)->pts);
		printf("audio %ld\n", (*pkt)->dts);
		printf("============================================\n");

		if (av_interleaved_write_frame((*output_format_context), (*pkt)) < 0)
		{
			printf("Error muxing packet\n");
			return -1;
		}
		av_free_packet(*pkt);
	}
	else
	{
		merging_audio_packet(*input_codec_context, (*pkt), pkt2);
		pkt2->pts = (*pkt)->pts;
		pkt2->dts = (*pkt)->dts;
		pkt2->pts = av_rescale_q_rnd(pkt2->pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt2->dts = av_rescale_q_rnd(pkt2->dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt2->duration = av_rescale_q(pkt2->duration, in_stream->time_base, out_stream->time_base);
		pkt2->pos = -1;
		pkt2->stream_index = (*pkt)->stream_index;

		printf("============================================\n");
		printf("aac pts is %ld\n", pkt2->pts);
		printf("aac dts is %ld\n", pkt2->dts);
		printf("============================================\n");

		if (av_interleaved_write_frame((*output_format_context), pkt2) < 0) {
			printf("Error muxing packet\n");
			return -1;
		}
		av_free_packet(*pkt);
		av_free_packet(pkt2);
	}
	return 0;

}

int TS_Format_conversion_MP4(const char *inputfile, const char *outputfile)
{
	AVCodecContext *video_input_codectx = NULL;
	AVCodecContext *video_output_codectx = NULL;
	AVCodecContext *audio_out_codecctx = NULL;
	AVCodecContext *audio_input_codecctx = NULL;

	AVFormatContext *ifmt_ctx = NULL;
	AVFormatContext  *ofmt_ctx = NULL;

	AVPacket *pkt = av_packet_alloc();

	SwrContext *swrContext = NULL;

	AVAudioFifo *audiofifo = NULL;
	static int video_index,audio_index;
	int ret;
	avformat_network_init();
	ret = open_input_file(inputfile, &ifmt_ctx, &audio_input_codecctx, &video_input_codectx);
	if (ret < 0)
	{
		printf("open_input_file is error");
		return -1;
	}
	if (open_output_file(outputfile, &ofmt_ctx, &ifmt_ctx, &audio_out_codecctx, &video_output_codectx) < 0)
	{
		printf("open_input_file is error");
		return -1;
	}
	streams_init(ifmt_ctx, ofmt_ctx, &video_output_codectx, &audio_out_codecctx);//返回视频的index
	if (video_index < 0)
	{
		printf("streams_init is error");
		return -1;
	}

	if (ifmt_ctx->nb_streams > 1)//判断是否需要初始化重采样
	{
		if (init_resampler(audio_input_codecctx, audio_out_codecctx, &swrContext) < 0)
		{
			printf("init_resampler is error");
			return -1;
		}
	

		if (init_fifo(&audiofifo, audio_input_codecctx) < 0)
		{
			printf("init_fifo is error");
			return -1;
		}
	}
	ret = avformat_write_header(ofmt_ctx, NULL);
	if (ret < 0)
	{
		printf("avformat_write_header is error");
		return -1;
	}
	//video_input_codectx->gop_size;
	while (0 <= av_read_frame(ifmt_ctx, pkt)) {
		/*音频*/
		if (pkt->stream_index != video_index)
		{
			if (ifmt_ctx->streams[pkt->stream_index]->codecpar->codec_id == AV_CODEC_ID_AAC)//判断是否为aac，决定是否复用和转码
			{
				audio_muxer(&ofmt_ctx, &ifmt_ctx, &audio_input_codecctx, &pkt);//复用
				av_free_packet(pkt);
				continue;
			}
			else//转码
			{
				audio_decodec_encodec_write(&ofmt_ctx, &audio_out_codecctx, &audio_input_codecctx, &swrContext, &audiofifo, &pkt);
				av_free_packet(pkt);
				continue;
			}
		}
		/*视频*/
		if (pkt->stream_index == video_index)
		{
			if (ifmt_ctx->streams[pkt->stream_index]->codecpar->codec_id == AV_CODEC_ID_H264)//判断是否为H264，决定是否复用和转码
			{
				video_muxer(&ifmt_ctx, &ofmt_ctx, &pkt);//复用
				av_free_packet(pkt);
				continue;
			}
			else //转码
			{
				video_decodec_encodec_write(&ofmt_ctx, &ifmt_ctx, &video_output_codectx, &video_input_codectx, &pkt);
				av_free_packet(pkt);
				continue;
			}
		}
		av_free_packet(pkt);
	}
	av_write_trailer(ofmt_ctx);//写文件尾
	avformat_close_input(&ifmt_ctx);
	avformat_free_context(ofmt_ctx);
	av_free_packet(pkt);
	swr_free(&swrContext);
	return 0;
}