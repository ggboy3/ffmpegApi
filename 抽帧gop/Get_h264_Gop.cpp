#include "Get_h264_Gop.h"
#include "mg_log.h"
static int  videoindex;

static int initAVFContext(AVFormatContext **pFormatCtx,const char *fileName)
{

	int i, ret;


	*pFormatCtx = avformat_alloc_context();  //初始化视频数据上下文

	
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


static int Outfile_header(AVFormatContext **outFormatCtx, AVFormatContext **pFormatCtx, char *outfile)
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
static int write_data_precise(AVFormatContext *outFormatCtx, AVFormatContext *pFormatCtx, int64_t seek_times)
{

	float pts_time;
	float dts_time;
	float duration_time;
	int pts_num = 0;
	int ret = 0;
	int i = 0;
	int n = 0;
	int j = 0;
	AVStream *in_stream = pFormatCtx->streams[videoindex];
	AVStream *out_stream = NULL;
	AVPacket pkt;
	int64_t i_frame[10];
	av_init_packet(&pkt);
	AVRational av_time{1,AV_TIME_BASE };// 设备系统时间戳
	
	int64_t seek_time = av_rescale_q(seek_times, av_time, in_stream->time_base);
	LOG_INFO("seek_time =%ld", seek_time);
	LOG_INFO("in_stream->duration =%ld", in_stream->duration);
	if (seek_time<0 || seek_time > in_stream->duration)
	{
		LOG_ERROR("seek_time is error or too long");
		return -1;
	}
	
//seek到时间点的前一秒开始找i帧,不要问为什么不直接seek，问就是ts流不准确
	int64_t before_seek_time = seek_time - ONE_SECOND;
	if (before_seek_time < 0)
		before_seek_time = 0;
	
seek:

	if (av_seek_frame(pFormatCtx, videoindex, before_seek_time, AVSEEK_FLAG_ANY) < 0)
	{
		LOG_ERROR("av_seek_frame error");
		return -1;
	}
	
	while (1)
	{		
		ret = av_read_frame(pFormatCtx, &pkt);
		if (ret < 0)
		{
			LOG_INFO(NULL, AV_LOG_WARNING, "while av_read_frame return: %d", ret);
			av_packet_unref(&pkt);
			break;     //判断此文件读取完毕
		}
		if (pkt.pts > seek_time && pkt.stream_index == videoindex)
		{
			av_packet_unref(&pkt);
			break;
		}
		if (pkt.stream_index == videoindex)
		{			

			if (pkt.stream_index == videoindex && pkt.flags == AV_PKT_FLAG_KEY)
			{
				i_frame[i++] = pkt.pts;
			}

		}
		
		av_packet_unref(&pkt);

		
	}
	if (i==0)//如果没有找到i帧就继续往前加5秒找
	{
		n++;
		if (n == 5)
		{		
			LOG_INFO("%fs Can't find the I-frame", seek_time);
		    return -1;
		}
		before_seek_time = seek_time - (n * ONE_SECOND);
		if (before_seek_time < 0)before_seek_time = 0;
		goto seek;
	}

	//开始读取Gop序列
	if (av_seek_frame(pFormatCtx, videoindex, before_seek_time, AVSEEK_FLAG_ANY) < 0)
	{
		printf("av_seek_frame error\n");
		return -1;
	}

	while (1)
	{
		ret = av_read_frame(pFormatCtx, &pkt);
		if (ret < 0)
		{
			av_packet_unref(&pkt);
			LOG_INFO(NULL, AV_LOG_WARNING, "while av_read_frame return: %d", ret);
			break;     //判断此文件读取完毕
		}

		if (pkt.stream_index == videoindex && pkt.pts>= i_frame[i-1])
		{
			if (pkt.pts > seek_time)//结束条件，当pkt.pts大于要抽的时间点
			{
				av_packet_unref(&pkt);
				break;
			}
			in_stream = pFormatCtx->streams[videoindex];

			if (videoindex == 1)//此处的作用是保证输出的视频流id为0
				out_stream = outFormatCtx->streams[!videoindex];
			else
				out_stream = outFormatCtx->streams[videoindex];

                 //就默认输出24帧吧，有些流读取不到正确的帧率
				AVRational Standard_Frame_Rate{ 24,1 };
				double calcDuration = 1.0 / av_q2d(Standard_Frame_Rate);
				pkt.duration = (double)calcDuration / (double)(av_q2d(in_stream->time_base));
			


			//给视频流重新计算duration	
			pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
			pkt.pts = (pkt.duration)*pts_num;//加速播放
			pkt.dts = pkt.pts;
			pts_num++;


			pts_time = pkt.pts * av_q2d(out_stream->time_base);
			dts_time = pkt.dts * av_q2d(out_stream->time_base);
			duration_time = pkt.duration * av_q2d(out_stream->time_base);
		
			LOG_INFO("===================== %d frame==========================", ++j);
			LOG_INFO("video_time = %0.3fs", dts_time);
				
			LOG_INFO("time_duration = %0.3fs", duration_time);
				

			pkt.pos = -1;
			if (videoindex == 1)
				pkt.stream_index = !videoindex;
			else
				pkt.stream_index = videoindex;

			ret = av_interleaved_write_frame(outFormatCtx, &pkt); 	//写入文件数据
			if (ret < 0)
			{
				LOG_INFO("av_interleaved_write_frame return %d", ret);
				return -1;
			}


		}
			
		av_packet_unref(&pkt);

	}
	float pick_time = seek_time*av_q2d(in_stream->time_base);
	LOG_INFO("=================================================");
	LOG_INFO("pick_time for video= %0.3fs", pick_time);
	LOG_INFO("video file_name = %s", outFormatCtx->filename);
	LOG_INFO(">>>>>>>>>>>>>>>>>>>>>=<<<<<<<<<<<<<<<<<<<<<<end");
	return 0;
}
static int write_data_rough(AVFormatContext *outFormatCtx, AVFormatContext *pFormatCtx, int64_t seek_times)
{
	float pts_time;
	float dts_time;
	float duration_time;	
	int pts1 = 0;
	int ret = 0;
	int i = 0;
	int j = 0;
	int n = 0;
	AVStream *in_stream = pFormatCtx->streams[videoindex];
	AVStream *out_stream = NULL;
	AVPacket pkt;
	av_init_packet(&pkt);
	AVRational av_time{ 1,AV_TIME_BASE };// 设备系统时间戳


	int64_t seek_time = av_rescale_q(seek_times, av_time, in_stream->time_base);
		if (seek_time<0 || seek_time > pFormatCtx->duration)
		{
			LOG_ERROR("seek_time is error or too long");
			return -1;
		}
		seek:
		if (av_seek_frame(pFormatCtx, videoindex, seek_time + ONE_SECOND, AVSEEK_FLAG_ANY)<0)
		{
			LOG_ERROR("av_seek_frame error");
			return -1;
		}
			
		while (1)
		{
			ret = av_read_frame(pFormatCtx, &pkt);
			if (ret < 0)
			{
				if (i == 0)//如果没有找到i帧就继续往前加5秒找
				{
					n++;
					if (n == 5)
					{
						printf("%fs Can't find the I-frame\n", seek_time);
						return -1;
					}
					seek_time = n * ONE_SECOND  seek_time;
					goto seek;
				}
				av_log(NULL, AV_LOG_WARNING, "while av_read_frame return: %d\n", ret);
				break;     //判断此文件读取完毕
			}

			// && packet->flags == AV_PKT_FLAG_KEY
			if (pkt.stream_index == videoindex && pkt.flags == AV_PKT_FLAG_KEY)
			{

				in_stream = pFormatCtx->streams[videoindex];

				if (videoindex == 1)//判断输入视频流是哪个
					out_stream = outFormatCtx->streams[!videoindex];
				else
					out_stream = outFormatCtx->streams[videoindex];


					AVRational Standard_Frame_Rate{ 24,1 };
					double calcDuration = 1.0 / av_q2d(Standard_Frame_Rate);
					pkt.duration = (double)calcDuration / (double)(av_q2d(in_stream->time_base));
				


				//给视频流重新计算duration	
				pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
				pkt.pts = (pkt.duration)*pts1;//加速播放
				pkt.dts = pkt.pts;
				pts1++;


				pts_time = pkt.pts * av_q2d(out_stream->time_base);
				dts_time = pkt.dts * av_q2d(out_stream->time_base);
				duration_time = pkt.duration * av_q2d(out_stream->time_base);

				LOG_INFO("===================== %d frame==========================", ++j);
				LOG_INFO("video_time = %0.3fs", dts_time);

				LOG_INFO("time_duration = %0.3fs", duration_time);
				pkt.pos = -1;
				if (videoindex == 1)
				{
					pkt.stream_index = !videoindex;
				}
				else
				{
					pkt.stream_index = videoindex;
				}

				ret = av_interleaved_write_frame(outFormatCtx, &pkt); 	//写入文件数据
				if (ret < 0)
				{
					LOG_ERROR("av_interleaved_write_frame return %d\n", ret);
					return -1;
				}

				break;


			}

			av_packet_unref(&pkt);

		}
		float pick_time = seek_time * av_q2d(in_stream->time_base);
		LOG_INFO("=================================================\n");
		LOG_INFO("pick_time for video= %0.3fs", pick_time);
		LOG_INFO("video file_name = %s", outFormatCtx->filename);
		LOG_INFO(">>>>>>>>>>>>>>>>>>>>>=<<<<<<<<<<<<<<<<<<<<<<end");
	return 0;
}

int extrat_Gop_frame_To_h264(cut_frame_info* cut_array, int pick_num, const char *in_files, int mode)
{

	 //参数检测
	if (in_files==NULL)
	{
		printf("in_files is NULL\n");
		return -1;
	}
	if (cut_array == NULL)
	{
		printf("cut_array is NULL");
		return -1;
	}
	if (mode <0 || mode >1)
	{
		mode = ROUGH_SEEK_TIME;		
		return -1;
	}
	AVFormatContext	*pFormatCtx=NULL;            //视频数据上下文
	AVFormatContext *outFormatCtx=NULL;

	av_register_all();                  //注册所有功能函数


		if(initAVFContext(&pFormatCtx, in_files) < 0)
		{
			LOG_INFO("initAVFContext is error");
			goto fail;
		}
		printf("==================input  Information  ====================\n");
		av_dump_format(pFormatCtx, 0, in_files, 0);
		LOG_INFO("videoindex = %d\n", videoindex);
		printf("==================input  Information  ====================\n");


		
		
		for (int i = 0; i < pick_num; i++)
		{
			//写入文件头

			if (Outfile_header(&outFormatCtx, &pFormatCtx, cut_array[i].cut_file_name) < 0)
			{
				LOG_INFO("cut_file_name is error : cut_array[%d].cut_file_name",i);
				cut_array[i].cut_result = -1;
				continue;
			}
			//写入数据 
		
			if (mode == PRECISE_SEEK_TIME)
			{
				if (write_data_precise(outFormatCtx, pFormatCtx,cut_array[i].cut_time_point) < 0)
				{
					LOG_INFO("write_data_precise is error");
					cut_array[i].cut_result = -1;
					continue;
				}
			}
			else
			{
				if (write_data_rough(outFormatCtx, pFormatCtx, cut_array[i].cut_time_point) < 0)
				{
					LOG_INFO("write_data_rough is error");
					cut_array[i].cut_result = -1;
					continue;
				}
			}
					//写入文件尾
			if (av_write_trailer(outFormatCtx) < 0)
			{
				LOG_INFO("av_write_trailer is error");
				cut_array[i].cut_result = -1;
				continue;
			}
			cut_array[i].cut_result = 0;
		}
  
fail:
	if (pFormatCtx)
	avformat_close_input(&pFormatCtx);  //释放视频流上下文
	if (outFormatCtx)
	avformat_close_input(&outFormatCtx);

	return 0;
}