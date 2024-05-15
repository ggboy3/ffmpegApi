#include "CutFrameSbh.h"
#include <stdio.h>


#define VIDEO 0
#define AUDIO 1

extern std::vector<frame_info> media_list;

static int  streams_index[2] = { -1, -1 };



static int streams_init(AVFormatContext *ifmt_ctx, AVFormatContext *ofmt_ctx, AVCodecContext **video2_codec_context, AVCodecContext **audio_codec_context)
{
	int ret = 0, streams_flag = 0, streams_audio = 0, streams_video = 0;

	
	for (int i = 0; i < ifmt_ctx->nb_streams; i++)
	{
		if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			streams_index[AUDIO] = i;
		}
		if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			streams_index[VIDEO] = i;
		}
	}

	if (ifmt_ctx->streams[streams_index[VIDEO]]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
	{
		AVStream *in_stream = ifmt_ctx->streams[streams_index[VIDEO]];
		in_stream->codec->codec = avcodec_find_decoder(AV_CODEC_ID_H264);
		if (avcodec_open2(in_stream->codec, in_stream->codec->codec, NULL) < 0) {
			printf("Could not open codec\n");
		}
	}
	return ret;
}

static int open_input_file(const char *filename,
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




int video_decodec(AVFormatContext * &ifmt_ctx, AVPacket * &pkt, AVFrame * &video_frame)
{
	AVStream* in_stream = ifmt_ctx->streams[streams_index[VIDEO]];
	int ret;
	ret = avcodec_send_packet(in_stream->codec, pkt);
	if (ret != 0)
	{
		printf("avcodec_send_packet is error to video");
		return -1;
	}
	while (1)
	{
		ret = avcodec_receive_frame(in_stream->codec, video_frame);
		if (ret != 0)
		{
			break;
		}
		else
		{
			return 1;
			
		}
	}
	return -1;
}


static int64_t seek_packet(void *opaque, int64_t offset, int whence)
{
	FILE * fb = (FILE *)opaque;


	return -1;
}

static int write_packet(void *opaque, uint8_t *buf, int buf_size)
{
	FILE * fb = (FILE *)opaque;


	return -1;
}
struct pkt_gop
{
	AVPacket *packet;
	int   decode = 0;//解码引用数
	std::vector<char *>  cut_file_name;
};
static void push_pkt(std::vector<pkt_gop *> &gop_pkt, AVPacket * &pkt)
{
	pkt_gop *PacketGop = new(pkt_gop);
	PacketGop->packet = av_packet_alloc();
	if (av_packet_ref(PacketGop->packet, pkt) < 0)
	{
		av_packet_unref(PacketGop->packet);
		av_packet_free(&PacketGop->packet);
	}
	gop_pkt.push_back(PacketGop);
}

static int cut_frametojpg(AVFormatContext * &ifmt_ctx,AVPacket * &pkt, AVFrame * &video_frame, int cut_array_size, std::vector<pkt_gop *> &gop_pkt, cut_info_ *cutinfo)
{
	int decode_flag = 0;
    int cut_frame_num = 0;
	if (pkt == NULL || pkt->flags == 1)
	{
		decode_flag = 0;
		for (int i = 0; i < cut_array_size; i++)
		{
			
			if (cutinfo[i].cut_result != 0)
				continue;
			for (int j = 0; j < gop_pkt.size(); j++)
			{
				if (gop_pkt[j]->packet->pts >= cutinfo[i].cut_time_point)
				{
					decode_flag = 1;
					gop_pkt[j]->decode++;
					cutinfo[i].cut_result = 1;
					char* buf = new char[250];
					strcpy(buf, cutinfo[i].cut_file_name);
					gop_pkt[j]->cut_file_name.push_back(buf);
					printf("%s\n", cutinfo[i].cut_file_name);
					break;
				}
			}
					
		}			   				
		 
		for (int i = 0; i < gop_pkt.size(); i++)//清除一个GOP的队列
		{
			if (decode_flag == 1)
			{												
				if (video_decodec(ifmt_ctx, gop_pkt[i]->packet, video_frame) < 0)
				{
					break;
				}
				for (int l = 0; l < gop_pkt[i]->decode; l++)
				{
					save_yuv_data_to_jpg_from_yuvframe(video_frame, video_frame->width, video_frame->height, gop_pkt[i]->cut_file_name[l], 1);
					cut_frame_num++;
					delete[] gop_pkt[i]->cut_file_name[l];
				}
				av_frame_unref(video_frame);												
			}

			av_packet_unref(gop_pkt[i]->packet);
			av_packet_free(&gop_pkt[i]->packet);
			delete gop_pkt[i];
		}
		gop_pkt.clear();
	}
	if(pkt != NULL)
	push_pkt(gop_pkt, pkt);
	return cut_frame_num;
}
int cut_frame_batch_sbh(const char *inputfile, cut_info_ *cutinfo, int cut_array_size)
{
	
	AVCodecContext *video_input_codectx = NULL;
	AVCodecContext *video_output_codectx = NULL;
	AVCodecContext *audio_out_codecctx = NULL;
	AVCodecContext *audio_input_codecctx = NULL;
	AVIOContext *avio_ctx = NULL;
	AVFormatContext *ifmt_ctx = NULL;
	AVFormatContext  *ofmt_ctx = NULL;
	FILE *fb = NULL;
	AVFrame *video_frame = av_frame_alloc();
	AVPacket *pkt = av_packet_alloc();
	std::vector<pkt_gop *> gop_pkt;
	static int video_index, audio_index, push_flag = 0; 
	int ret, cu_num = 0, frame_num = 0;
	int(*read)(void *opaque, uint8_t *buf, int buf_size) = read_packet;
	int64_t(*seek)(void *opaque, int64_t offset, int whence) = seek_packet;
	int(*write)(void *opaque, uint8_t *buf, int buf_size) = write_packet;
	
	fb = fopen(inputfile, "rb+");
	fseek(fb, HEAD_BYTES + INDEX_BYTES, SEEK_SET);
	int avio_ctx_buffer_size = 10240 * 25;
	uint8_t * avio_ctx_buffer = NULL;
	avio_ctx_buffer = (uint8_t *)av_malloc(avio_ctx_buffer_size);
	if (!avio_ctx_buffer) {
		av_log(NULL, AV_LOG_INFO, "avio_ctx_buffer is error");
		goto fail;
	}
	
	avio_ctx = avio_alloc_context(avio_ctx_buffer, avio_ctx_buffer_size, 0, fb, read, NULL, seek);
	ifmt_ctx = avformat_alloc_context();  //初始化视频数据上下文
	ifmt_ctx->pb = avio_ctx;
	ifmt_ctx->flags |= AVFMT_FLAG_CUSTOM_IO;
	ifmt_ctx->iformat = av_find_input_format("h264");
	
	ret = open_input_file(inputfile, &ifmt_ctx, &audio_input_codecctx, &video_input_codectx);
	if (ret < 0)
	{
		printf("open_input_file is error");
		goto fail;
	}

	streams_init(ifmt_ctx, ofmt_ctx, &video_output_codectx, &audio_out_codecctx);//返回视频的index
	if (video_index < 0)
	{
		printf("streams_init is error");
		goto fail;
	}


	while (0 <= av_read_frame(ifmt_ctx, pkt)) {
		/*音频*/
		if (pkt->stream_index != video_index)
		{
			av_packet_unref(pkt);//丢弃
		}
		/*视频*/
		if (pkt->stream_index == video_index && pkt->size != 0)
		{
			pkt->pts = media_list[frame_num].PTS;
			frame_num++;
			if (pkt->flags == 1 && push_flag == 0)
			{
				push_flag = 1;
			}
			else if(push_flag == 0)
			{
				av_packet_unref(pkt);
				continue;
			}
			
			cu_num += cut_frametojpg(ifmt_ctx,pkt,video_frame,cut_array_size,gop_pkt,cutinfo);
		
			printf("media_list = %d \n", media_list.size());
			
			av_packet_unref(pkt);

		}
		if (cu_num == cut_array_size)
			break;
		av_usleep(100);
	}
fail:
	
	fclose(fb);
	av_free(avio_ctx_buffer);
	avio_context_free(&avio_ctx);
	av_freep(&avio_ctx);
	av_packet_free(&pkt);
	cut_frametojpg(ifmt_ctx, pkt, video_frame, cut_array_size, gop_pkt, cutinfo);
	av_frame_free(&video_frame);
	avcodec_free_context(&video_input_codectx);
	avcodec_free_context(&video_output_codectx);
	avcodec_free_context(&audio_out_codecctx);
	avcodec_free_context(&audio_input_codecctx);
	avformat_close_input(&ifmt_ctx);
	avformat_close_input(&ofmt_ctx);
	media_list.clear();
	return 0;
}