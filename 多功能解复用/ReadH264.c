#include "ReadH264.h"
#include "MediaDataList.h"
#include <libavutil/time.h>

static int(*read)(void *opaque, uint8_t *buf, int buf_size) = NULL;
static void* arg_read = NULL;
static int(*read2)(void *opaque, uint8_t *buf, int buf_size) = NULL;
static void* arg_read2 = NULL;
static int64_t(*seek)(void *opaque, int64_t offset, int whence) = NULL;
static int(*write)(void *opaque, uint8_t *buf, int buf_size) = NULL;
static void* arg_write = NULL;

void SetReadAudio_Callback(int(*read_Audio)(void *opaque, uint8_t *buf, int buf_size), void* args)
{
	if (read_Audio == NULL)
		return;
	read = read_Audio;
	arg_read = args;
	return;
}
void SetReadVideo_Callback(int(*read_Video)(void *opaque, uint8_t *buf, int buf_size), void* args)
{
	if (read_Video == NULL)
		return;
	read2 = read_Video;
	arg_read2 = args;
	return;
}
void SetSeekCallback(int64_t(*seek_packet)(void *opaque, int64_t offset, int whence))
{
	if (seek_packet == NULL)
		return;
	seek = seek_packet;
	return;
}
void SetWriteCallback(int(*write_packet)(void *opaque, uint8_t *buf, int buf_size), void* args)
{
	if (write_packet == NULL)
		return;
	write = write_packet;
	arg_write = args;
}
static int read_packet(void* opaque, uint8_t* buf, int buf_size)
{
	FILE* fb = (FILE*)opaque;
	int size;
	if (buf_size < 0)return -1;
	size = fread(buf, 1, buf_size, fb);
	if (size < 0)return -1;

	return size;
} 
static int write_packet(void *opaque, uint8_t *buf, int buf_size) {  
	FILE* fb = (FILE*)opaque;
	if (!feof(fb)) {  
		int true_size = fwrite(buf, 1, buf_size, fb);  
		return true_size;  
	}
	else {  
		return -1;  
	}  
}  
static int64_t seek_packet(void *opaque, int64_t offset, int whence)
{
	FILE * fb = (FILE *)opaque;


	return -1;
}
static void audio_muxer(AVFormatContext** input_format_context, AVFormatContext** output_format_context, AVPacket* input_pkt)
{
	//FILE *fb = fopen("666.h264", "wb+");
	AVStream* in_stream = (*input_format_context)->streams[0];
	AVStream* out_stream = (*output_format_context)->streams[(input_pkt)->stream_index];

		////write PTS
		//AVRational timeBase1 = in_stream->time_base;
		//AVRational time_base1;
		////Duration between 2 frames
		//double calcDuration = 1.0 / av_q2d(in_stream->r_frame_rate);

		//(input_pkt)->pts = (double)(frameIndex * calcDuration) / (double)(av_q2d(timeBase1));
		//(input_pkt)->dts = (input_pkt)->pts;
		//(input_pkt)->duration = (double)calcDuration / (double)(av_q2d(timeBase1));
		//frameIndex++;
	
	(input_pkt)->pts = av_rescale_q_rnd((input_pkt)->pts, in_stream->time_base, out_stream->time_base, (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
	(input_pkt)->dts = av_rescale_q_rnd((input_pkt)->dts, in_stream->time_base, out_stream->time_base, (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
	(input_pkt)->duration = av_rescale_q((input_pkt)->duration, in_stream->time_base, out_stream->time_base);
	(input_pkt)->pos = -1;

	printf("============================================\n");
	printf("Audio pts = %ld\n", input_pkt->pts);
    
//	if (av_write_frame(*output_format_context, input_pkt) < 0)
//	{
//		printf("Error muxing packet\n");
//	}
	if (av_interleaved_write_frame(*output_format_context, (input_pkt)) < 0) {
		printf("Error muxing packet\n");

	}

}
static void video_muxer(AVFormatContext** input_format_context, AVFormatContext** output_format_context, AVPacket* input_pkt)
{

	AVStream* in_stream = (*input_format_context)->streams[0];
	AVStream* out_stream = (*output_format_context)->streams[(input_pkt)->stream_index];


	
	(input_pkt)->pts = av_rescale_q_rnd((input_pkt)->pts, in_stream->time_base, out_stream->time_base, (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
	(input_pkt)->dts = av_rescale_q_rnd((input_pkt)->dts, in_stream->time_base, out_stream->time_base, (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
	(input_pkt)->duration = av_rescale_q((input_pkt)->duration, in_stream->time_base, out_stream->time_base);
	(input_pkt)->pos = -1;

	printf("============================================\n");
	printf("Video pts = %ld\n", input_pkt->pts);
    

	if (av_interleaved_write_frame(*output_format_context, (input_pkt)) < 0) {
		printf("Error muxing packet\n");

	}

}
static int open_input_file2(const char *filename,
	AVFormatContext **input_format_context,
	AVCodecContext **input_codec_context,
	AVCodecContext **video_codec_context)
{
	AVCodec *audio_codec = NULL;
	AVCodec *video_codec = NULL;
	int ret;
	int flag = 0;
	av_register_all();//初始化所有编解码器

	if ((ret = avformat_open_input(input_format_context, NULL, 0, 0)) < 0) {
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
static int open_input_file(const char* filename,
	AVFormatContext** input_format_context,
	AVCodecContext** input_codec_context,
	AVCodecContext** video_codec_context,
	AVIOContext** avio_ctx,
	void *opaque,
	int(*read_packet)(void *opaque, uint8_t *buf, int buf_size),
	int(*write_packet)(void *opaque, uint8_t *buf, int buf_size),
	int64_t(*seek)(void *opaque, int64_t offset, int whence),
	uint8_t* avio_ctx_buffer,
	int buffer_size,
	char *input_format)
{
	AVCodec* audio_codec = NULL;
	AVCodec* video_codec = NULL;
	int ret;
	int flag = 0;
	av_register_all();//初始化所有编解码器
	if (read != NULL || filename == NULL && *avio_ctx == NULL)
	{
		*avio_ctx = avio_alloc_context(avio_ctx_buffer, buffer_size, 0, opaque, read_packet, write_packet, seek);
		(*input_format_context) = avformat_alloc_context();  //初始化视频数据上下文
		(*input_format_context)->pb = *avio_ctx;
		(*input_format_context)->flags |= AVFMT_FLAG_CUSTOM_IO;
		(*input_format_context)->iformat = av_find_input_format(input_format);
	}
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
		if ((*input_format_context)->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && flag == 0)//在这里可以打开解码器或者其他操作
		{
			flag = 1;
//			video_codec = avcodec_find_decoder((*input_format_context)->streams[i]->codecpar->codec_id);
//			*video_codec_context = avcodec_alloc_context3(video_codec);
//
//			(*video_codec_context)->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
//			avcodec_parameters_to_context(*video_codec_context, (*input_format_context)->streams[i]->codecpar);
//
//			if (avcodec_open2(*video_codec_context, video_codec, 0) < 0)
//			{
//				printf("avcodec_open2 is error");
//				return -1;
//			}


		}
		if ((*input_format_context)->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)//在这里可以打开解码器或者其他操作
		{
			*input_codec_context = (*input_format_context)->streams[i]->codec;
			(*input_codec_context)->sample_rate = 8000;
			(*input_codec_context)->channels = 1;
			(*input_codec_context)->channel_layout = av_get_default_channel_layout((*input_codec_context)->channels);

			(*input_format_context)->streams[i]->codecpar->channels = 1;
			(*input_format_context)->streams[i]->codecpar->channel_layout = av_get_default_channel_layout((*input_format_context)->streams[i]->codecpar->channels);
			(*input_format_context)->streams[i]->codecpar->sample_rate = 8000;
		}
//		*input_codec_context = (*input_format_context)->streams[i]->codec;
//		audio_codec = avcodec_find_decoder((*input_codec_context)->codec_id);
//
//		if (avcodec_open2(*input_codec_context, audio_codec, 0) < 0)
//		{
//			printf("avcodec_open2 is error");
//			return -1;
//		}

	}
	return 0;
}

int init_output_file(AVFormatContext** ifmtCtxAudio,
	AVFormatContext** ifmtCtxVideo,
	AVFormatContext** ofmt_ctx, 
	AVIOContext** avio_ctx,
	char* outFilename,
	void *opaque,
	int(*read_packet)(void *opaque, uint8_t *buf, int buf_size),
	int(*write_packet)(void *opaque, uint8_t *buf, int buf_size),
	int64_t(*seek)(void *opaque, int64_t offset, int whence),
	uint8_t* avio_ctx_buffer,
	int buffer_size,
	char *input_format)
{
	
	avformat_alloc_output_context2(ofmt_ctx, NULL, input_format, outFilename);
 	if (!ofmt_ctx)
	{
		printf("can't create output context\n");
		return -1;
	}
	if (outFilename == NULL)
	{
		*avio_ctx = avio_alloc_context(avio_ctx_buffer, buffer_size, 1, opaque, NULL, write_packet, NULL);
		(*ofmt_ctx)->pb = *avio_ctx;
		(*ofmt_ctx)->flags |= AVFMT_FLAG_CUSTOM_IO;
	}
	
	
	
	for (int i = 0; i < (*ifmtCtxVideo) != NULL && (*ifmtCtxVideo)->nb_streams; ++i)
	{
		if ((*ifmtCtxVideo)->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			AVStream* inStream = (*ifmtCtxVideo)->streams[i];
			AVStream* outStream = avformat_new_stream(*ofmt_ctx, NULL);


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
	
	for (int i = 0; i < (*ifmtCtxAudio) != NULL && (*ifmtCtxAudio)->nb_streams; ++i)
	{
		if ((*ifmtCtxAudio)->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			AVStream* inStream = (*ifmtCtxAudio)->streams[i];
			AVStream* outStream = avformat_new_stream(*ofmt_ctx, NULL);

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
	
	if (*avio_ctx == NULL)
	{
		if (avio_open((&(*ofmt_ctx)->pb), outFilename, AVIO_FLAG_WRITE) < 0)
		{
			printf("can't open out file\n");
			return -1;
		}
	}
	
	//写文件头
	if (avformat_write_header(*ofmt_ctx, NULL) < 0)
	{
		printf("Error occurred when opening output file\n");
		return -1;
	}
	return 0;
}

int H264_G711u(const char* inputAudio, const char* inputH264, const char* outFilename)
{
	AVCodecContext* video_input_codectx = NULL;
	AVCodecContext* video_output_codectx = NULL;
	AVCodecContext* audio_out_codecctx = NULL;
	AVCodecContext* audio_input_codecctx = NULL;
	AVIOContext* avio_ctx = NULL;
	AVIOContext* avio_ctx2 = NULL;
	AVIOContext* avio_ctx3 = NULL;
	AVFormatContext* ifmt_ctx = NULL;
	AVFormatContext* ifmt_ctx2 = NULL;
	AVFormatContext* ofmt_ctx = NULL;
//	FILE* fb = NULL;
//	FILE* fb2 = NULL;
//	FILE* fb3 = NULL;

	AVPacket* pkt = av_packet_alloc();
	static int video_index, audio_index, push_flag = 0;
	int ret, cu_num = 0, frame_num = 0;
	long long AudioTimes = 0;
	long long VideoTimes = 0;
//	int(*read)(void *opaque, uint8_t *buf, int buf_size) = read_packet;
//	int64_t(*seek)(void *opaque, int64_t offset, int whence) = seek_packet;
//	int(*write)(void *opaque, uint8_t *buf, int buf_size) = write_packet;
//	if (inputAudio == NULL)
//	{
//		fb = fopen(inputAudio, "rb+");
//		if (!fb)
//		{
//			goto end;
//		}
//	}
//	if (outFilename == NULL)
//	{
//		fb2 = fopen(outFilename, "wb+");
//		if (!fb2)
//		{
//			goto end;
//		}
//	}
//	if (inputH264 == NULL)
//	{
//		fb3 = fopen(inputH264, "rb+");
//		if (!fb3)
//		{
//			goto end;
//		}
//	}

	
	int avio_ctx_buffer_size = 10240 * 25;
	
	uint8_t* avio_ctx_buffer = NULL;
	avio_ctx_buffer = (uint8_t*)av_malloc(avio_ctx_buffer_size);
	if (!avio_ctx_buffer) {
		av_log(NULL, AV_LOG_INFO, "avio_ctx_buffer is error");
		goto end;
	}
	
	uint8_t* avio_ctx_buffer2 = NULL;
	avio_ctx_buffer2 = (uint8_t*)av_malloc(avio_ctx_buffer_size);
	if (!avio_ctx_buffer2) {
		av_log(NULL, AV_LOG_INFO, "avio_ctx_buffer is error");
	}
	
	uint8_t* avio_ctx_buffer3 = NULL;
	avio_ctx_buffer3 = (uint8_t*)av_malloc(avio_ctx_buffer_size);
	if (!avio_ctx_buffer3) {
		av_log(NULL, AV_LOG_INFO, "avio_ctx_buffer is error");
	}
	

	ret = open_input_file(inputAudio,
		&ifmt_ctx,
		&audio_input_codecctx,
		&audio_out_codecctx, 
		&avio_ctx,
		arg_read,
		read,
		write,
		seek,
		avio_ctx_buffer,
		avio_ctx_buffer_size,
		"MULAW");
	if (ret < 0)
	{
		printf("open_input_file is error");
		goto end;
	}


	ret = open_input_file(inputH264,
		&ifmt_ctx2,
		&video_input_codectx,
		&video_output_codectx, 
		&avio_ctx3,
		arg_read2,
		read2,
		write,
		seek,
		avio_ctx_buffer3,
		avio_ctx_buffer_size,
		"h264");
	if (ret < 0)
	{
		printf("open_input_file is error");
		goto end;
	}
	

	ret = init_output_file(&ifmt_ctx, 
		&ifmt_ctx2,
		&ofmt_ctx, 
		&avio_ctx2, 
		outFilename, 
		arg_write, 
		read2, 
		write, 
		seek, 
		avio_ctx_buffer2, 
		avio_ctx_buffer_size,
		"flv");
	if (ret < 0)
	{
		printf("init_output_file is error");
		goto end;
	}
	while (1) {
		if (av_compare_ts(AudioTimes, ifmt_ctx->streams[0]->time_base, VideoTimes, ifmt_ctx2->streams[0]->time_base) < 0)
		//if (0)
		{
			ret = av_read_frame(ifmt_ctx, pkt);
//			if (ret < 0)
//			{
//				break;
//			}
			AVStream* in_stream = ifmt_ctx->streams[0];
			if (pkt->size != 0)
			{
				frame_num++;
				if (in_stream->codecpar->codec_id == AV_CODEC_ID_PCM_MULAW || in_stream->codecpar->codec_id == AV_CODEC_ID_PCM_ALAW)
				{
					if (pkt->pts != 0)
					{
						AudioTimes += (float)(pkt->size / 8000.0) / av_q2d(in_stream->time_base);
					}
					
					pkt->pts = AudioTimes;
					pkt->dts = AudioTimes;
					pkt->stream_index = 1;
				}
			
				audio_muxer(&ifmt_ctx, &ofmt_ctx, pkt);

				av_packet_unref(pkt);

			}
			

				
		}
		else
		{
			ret = av_read_frame(ifmt_ctx2, pkt);
//			if (ret < 0)
//			{
//				break;
//			}
			AVStream* in_stream = ifmt_ctx2->streams[0];
			if (pkt->size != 0)
			{


					AVRational timeBase1 = in_stream->time_base;
					AVRational time_base1;
					//Duration between 2 frames
					double calcDuration = 1.0 / av_q2d(in_stream->r_frame_rate);

					VideoTimes += (double) (calcDuration) / (double)(av_q2d(timeBase1));

					pkt->duration = (double)calcDuration / (double)(av_q2d(timeBase1));
					pkt->pts = VideoTimes;
					pkt->dts = VideoTimes;
					pkt->stream_index = 0;
				
				
				video_muxer(&ifmt_ctx2, &ofmt_ctx, pkt);
				av_packet_unref(pkt);
			}
				
			
		}
		

		av_usleep(100);
	}
	
	av_write_trailer(ofmt_ctx);//写文件尾
end:
//	fclose(fb);
//	fclose(fb2);
//	fclose(fb3);
	
	avformat_close_input(&ifmt_ctx);
	avformat_close_input(&ifmt_ctx2);
	avformat_close_input(&ofmt_ctx);
	av_packet_free(&pkt);

	if (avio_ctx_buffer2)
		av_free(avio_ctx_buffer2);
	avio_context_free(&avio_ctx2);
	av_freep(&avio_ctx2);
	
	
	if (avio_ctx_buffer3)
	av_free(avio_ctx_buffer3);
	avio_context_free(&avio_ctx3);
	av_freep(&avio_ctx3);
	
	if (avio_ctx_buffer)
		av_free(avio_ctx_buffer);
	avio_context_free(&avio_ctx);
	av_freep(&avio_ctx);

	return 0;
}