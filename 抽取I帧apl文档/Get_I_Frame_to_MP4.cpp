
#include "Get_I_Frame_to_MP4.h"

int  videoindex  = 0;
int initAVFContext(AVFormatContext	**pFormatCtx,char *fileName)
{
	int i;
	
	*pFormatCtx = avformat_alloc_context();  //初始化视频数据上下文
	int ret;

	if ((ret = avformat_open_input(pFormatCtx, fileName, NULL, NULL)) != 0) {
		av_log(NULL, AV_LOG_WARNING, "Couldn't open input stream:  %d\n", ret);
		return -1;
	}
	if ((ret = avformat_find_stream_info(*pFormatCtx, NULL)) < 0) {
		av_log(NULL, AV_LOG_WARNING, "Couldn't find stream information. %d\n", ret);
		return -1;
	}

	videoindex = -1;
	for (i = 0; i < (*pFormatCtx)->nb_streams; i++)
		if ((*pFormatCtx)->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoindex = i;
			break;
		}
	if (videoindex == -1) {
		av_log(NULL, AV_LOG_WARNING, "videoindex");
		return -1;
	}

	return 0;
}


int Outfile_header(AVFormatContext **outFormatCtx,AVFormatContext **pFormatCtx,char *outfile)
{
	int ret;
	AVStream *video_st;
	//获取输出视频的格式
	if ((ret = avformat_alloc_output_context2(outFormatCtx, NULL, NULL, outfile)) < 0)
	{
		av_log(NULL, AV_LOG_WARNING, "Didn't alloc_output_context2: %d\n", ret);
		return -1;
	}
	video_st = avformat_new_stream(*outFormatCtx, 0);
	if (video_st == NULL)
	{
		av_log(NULL, AV_LOG_WARNING, "failed allocating output stram: %d\n", ret);
		return -1;
	}
	//Copy the settings of AVCodecContext 
	if ((ret = avcodec_copy_context(video_st->codec, (*pFormatCtx)->streams[videoindex]->codec)) < 0)
	{
		av_log(NULL, AV_LOG_WARNING, "failed avcode_copy_context: %d\n", ret);
		return -1;
	}
	video_st->codec->codec_tag = 0;

	//打开输出文件
	if ((ret = avio_open(&((*outFormatCtx)->pb), outfile, AVIO_FLAG_READ_WRITE)) < 0)
	{
		av_log(NULL, AV_LOG_WARNING, "failed avio_open: %d\n", ret);
		return -1;
	}

	//写入MP4文件头
	if ((ret = avformat_write_header(*outFormatCtx, NULL)) != 0)
	{
		av_log(NULL, AV_LOG_WARNING, "write header: %d\n", ret);
		return -1;
	}

	return 0;
}



int extrat_I_s_frameToMP4(const char **in_files, int infile_size,const char* out_MP4file,int fps)
{
	AVFormatContext	*pFormatCtx;            //视频数据上下文
	AVFormatContext *outFormatCtx;
	AVStream *in_stream,*out_stream;
	AVPacket *packet,*packetbuff;
		int cun = 0;
	int NFptsdts_flag = 1;
	int writeOutHead_flag = 1;
	int pts1 = 0;
	int ret = 0;
	int err_read =0;
	int  n = 1;
	int i = 0;
	int64_t pts = 0, dts = 0;
	char *in_file;

	double pts_time;
	double dts_time;
	double duration_time;
	double gophead;

	printf("%d\n", infile_size);
	for (int i = 0;i<infile_size;i++)
	{
		printf("%s\n", in_files[i]);
	}


	packet = (AVPacket *)av_malloc(sizeof(AVPacket));
	if (packet == NULL)
	{
		printf("malloc packet fail\n");
		goto fail;
	}

	packetbuff = (AVPacket *)av_malloc(sizeof(AVPacket));
	if (packetbuff == NULL)
	{
		printf("malloc packet fail\n");
		goto fail;
	}

	av_init_packet(packet);
	av_init_packet(packetbuff);

	av_register_all();                  //注册所有功能函数
	avformat_network_init();            //网络功能
	av_log_set_level(AV_LOG_INFO);

	for (int i = 0; i< infile_size; i++)
	{
		//char * in_file = in_files[i];
		//1.打开in_file文件
		//	2.读取in_file 
		//	3.写out_file
		//	4.关闭in
		in_file = (char *)in_files[i];
		
		if ((ret = initAVFContext(&pFormatCtx, in_file)) < 0)
		{
			av_log(NULL, AV_LOG_WARNING, "initAVFContext: %d\n", ret);
			goto fail;
		}
		printf("==================input  Information  ====================\n");
		av_dump_format(pFormatCtx, 0, in_file, 0);
		printf("videoindex = %d\n", videoindex);
		printf("==================input  Information  ====================\n");


		while (1)
		{
			err_read = av_read_frame(pFormatCtx, packet);
			if (err_read < 0)
			{
				av_log(NULL, AV_LOG_WARNING, "while av_read_frame return: %d\n", err_read);
				NFptsdts_flag = 0;

				//到达文件最后一帧的时候保存时间戳
				in_stream = pFormatCtx->streams[videoindex];
				if (videoindex == 1)
				{
					out_stream = outFormatCtx->streams[!videoindex];
				}
				else
				{
					out_stream = outFormatCtx->streams[videoindex];
				}
				packetbuff->pts = av_rescale_q_rnd(packetbuff->pts, in_stream->time_base, out_stream->time_base, (enum AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
				packetbuff->dts = av_rescale_q_rnd(packetbuff->dts, in_stream->time_base, out_stream->time_base, (enum AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
				pts = packetbuff->pts + pts + 1;
				dts = packetbuff->dts + dts + 1;
				avformat_close_input(&pFormatCtx);
				break;     //判断此文件读取完毕
			}

			//写入文件头
			if (writeOutHead_flag)
				if ((ret = Outfile_header(&outFormatCtx, &pFormatCtx, (char *)out_MP4file)) < 0)
				{
					av_log(NULL, AV_LOG_WARNING, "getOutfile_header: %d\n", ret);
					goto fail;
				}
			writeOutHead_flag = 0;

			// && packet->flags == AV_PKT_FLAG_KEY
			if (packet->stream_index == videoindex && packet->flags == AV_PKT_FLAG_KEY)
			{
				av_packet_unref(packetbuff);
				if ((ret = av_copy_packet(packetbuff, packet)) != 0)
				{
					av_log(NULL, AV_LOG_WARNING, "av_copy_packet: %d\n", ret);
				}		
				//转封装过程的时间基转换
				static int time = 1;
				if (time==1)
				{
					packet->pts = 0;
					time = 0;
				}
				in_stream = pFormatCtx->streams[videoindex];
				
				if (videoindex == 1)
				{
					out_stream = outFormatCtx->streams[!videoindex];
				}
				else
				{
					out_stream = outFormatCtx->streams[videoindex];
				}
				

				double calcDuration = 1.0/ fps;
				packet->duration = ((double)calcDuration / (double)(av_q2d(in_stream->time_base)));//根据需求重新打时间戳
				packet->duration = av_rescale_q(packet->duration, in_stream->time_base, out_stream->time_base);
				packet->pts = (packet->duration)*pts1;//加速播放
				packet->dts = packet->pts;
				pts1++;


				pts_time = packet->pts * av_q2d(out_stream->time_base);
				dts_time = packet->dts * av_q2d(out_stream->time_base);
				duration_time = packet->duration * av_q2d(out_stream->time_base);
				printf("===================== %d==========================\n", n++);
				printf("dts_time = %f\n", dts_time);
				printf("pts_time = %f\n", pts_time);
				printf("duration = %f\n", duration_time);
				gophead = pts_time / duration_time;
				printf("video1 gophead = %f\n", gophead);
				packet->pos = -1;
				if (videoindex == 1)
				{
					packet->stream_index = !videoindex;
				}
				else
				{
					packet->stream_index = videoindex;
				}
				ret =av_interleaved_write_frame(outFormatCtx, packet); 	//写入文件数据
				if(ret==0)cun++;
			}

			av_packet_unref(packet);

		}
	}

	av_write_trailer(outFormatCtx);		//写入文件尾

	printf("%d\n",cun);
fail:
	av_packet_unref(packet);
	av_packet_unref(packetbuff);
	avformat_close_input(&pFormatCtx);  //释放视频流上下文
	avformat_close_input(&outFormatCtx);

	return ret;
}


