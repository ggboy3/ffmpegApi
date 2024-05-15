#include "VideoReview.h"


int VideoReview::Open_Input_File()
{
	int i;
	int ret;
	if (pFormatCtx != NULL)
	{
		Close();
	}
	if ((ret = avformat_open_input(&pFormatCtx, in_files[The_num_video].file_path, NULL, NULL)) != 0) {
		av_log(NULL, AV_LOG_WARNING, "Couldn't open input stream:  %d\n", ret);
		return -1;
	}
	
	
	if ((ret = avformat_find_stream_info(pFormatCtx, NULL)) < 0) {
		av_log(NULL, AV_LOG_WARNING, "Couldn't find stream information. %d\n", ret);
		return -1;
	}

	video_index = -1;
	for (i = 0; i < pFormatCtx->nb_streams; i++) {
		if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) 
			video_index = i;
		if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
			audio_index = i;
	}
	

	if (video_index == -1) {
		av_log(NULL, AV_LOG_WARNING, "videoindex");
		return -1;
	}

	return 0;
}


int VideoReview::adts_header(uint8_t * const p_adts_header,
	const int data_length,
	const int profile,
	const int samplerate,
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
void VideoReview::merging_audio_packet(AVCodecContext *output_codec_context, AVPacket *pkt, AVPacket *pkt2)
{
	uint8_t adts_header_buf[7] = { 0 };

	adts_header(adts_header_buf,
		pkt->size,
		output_codec_context->profile,
		output_codec_context->sample_rate,
		output_codec_context->channels);

	av_new_packet(pkt2, pkt->size + 7);//动态分配ptk2的内存
	memset(pkt2->data, 0, pkt->size + 7);
	memcpy(pkt2->data, adts_header_buf, (sizeof(uint8_t) * 7));
	memcpy(pkt2->data + 7, pkt->data, pkt->size);
}
int  VideoReview::Stream_Filter(AVStream * &in_stream, AVPacket * &packet)
{
	int ret = 0;
	if (in_stream->codec->codec_id == AV_CODEC_ID_HEVC)
	{
		if (filter2 == NULL || bsf_ctx2 == NULL)
		{
			filter2 = const_cast<AVBitStreamFilter *>(av_bsf_get_by_name("hevc_mp4toannexb"));//aac_adtstoasc h264_mp4toannexb
			if (!filter2) {
				printf("bs error 2");
				Close();
				return -1;
			}

			ret = av_bsf_alloc(filter2, &bsf_ctx2);
			if (ret < 0) {
				printf("bs error");
				Close();
				return -1;
			}
			avcodec_parameters_copy(bsf_ctx2->par_in, in_stream->codecpar);
			av_bsf_init(bsf_ctx2);
		}
		ret = av_bsf_send_packet(bsf_ctx2, packet);
		if (ret < 0) {
			printf("bsf send error");
			Close();
			return -1;
		}
		av_packet_unref(packet);
		ret = av_bsf_receive_packet(bsf_ctx2, packet);
		if (ret < 0) {
			printf("bsf send error 2");
			Close();
			return -1;
		}
					

	}
	else if (in_stream->codec->codec_id == AV_CODEC_ID_H264)
	{
			
		if (filter == NULL || bsf_ctx == NULL)
		{
			filter = const_cast<AVBitStreamFilter *>(av_bsf_get_by_name("h264_mp4toannexb"));//aac_adtstoasc h264_mp4toannexb
			if (!filter) {
				printf("bs error 2");
				Close();
				return -1;
			}

			int ret = av_bsf_alloc(filter, &bsf_ctx);
			if (ret < 0) {
				printf("bs error");
				Close();
				return -1;
			}
			avcodec_parameters_copy(bsf_ctx->par_in, in_stream->codecpar);
			av_bsf_init(bsf_ctx);
		}
		ret = av_bsf_send_packet(bsf_ctx, packet);
		if (ret < 0) {
			printf("bsf send error");
			Close();
			return -1;
		}
		av_packet_unref(packet);
		ret = av_bsf_receive_packet(bsf_ctx, packet);
		if (ret < 0) {
			printf("bsf send error 2");
			Close();
			return -1;
		}

	}
	
	return 0;
}
int VideoReview::Init_Video_Review(VIDEO_INFO * in_files, int num, int(*callback)(int data_type, uint8_t *data, int data_size, int i_key, unsigned long long Times, void *args), void *args_)
{

	if (in_files == NULL || num <= 0 || (*callback) == NULL)
	{
		av_log(NULL, AV_LOG_ERROR, "Init_Video_Review error\n");
		return -1;
	}
	this->in_files = in_files;
	this->num = num;
	this->callback = callback;
	this->args = args_;

	
	return 0;
}
int VideoReview::Seek_Video(int64_t next_seek_times)
{
	av_usleep(10 * 1000);
	int flag = 0;
	for (int i = 0; i < num; i++)
	{
		if (in_files[i].file_start_time <= next_seek_times && in_files[i].file_end_time > next_seek_times)
		{
			seek_times = (next_seek_times - in_files[i].file_start_time);
			The_num_video = i;
			flag = 1;
			break;
		}
	}
	if (flag == 0)
	{
		av_log(NULL, AV_LOG_ERROR, "Video not found\n");
		return -1;
	}
		
	g_cv.notify_one();

	if (stop_thread == 1)
	{
		//std::thread t(std::bind(&VideoReview::Video_Push, shared_from_this()));
		read_thread = std::make_shared<std::thread>(std::bind(&VideoReview::Video_Push, shared_from_this()));
		//t.detach();
	}
	else 
	{
		stop_video = 1;
	}
 
	return 0;
}
void VideoReview::Close()
{

	if (pFormatCtx)
	{
		avformat_close_input(&pFormatCtx);  //释放视频流上下文
		pFormatCtx = NULL;
	}

	if (hevcbsfc)
	{
		av_bitstream_filter_close(hevcbsfc);
		hevcbsfc = NULL;
	}

	if (h264bsfc)
	{
		av_bitstream_filter_close(h264bsfc);
		h264bsfc = NULL;
	}
	if (bsf_ctx2)
	{
		av_bsf_free(&bsf_ctx2);
		bsf_ctx2 = NULL;
		filter2 = NULL;
	}
	if (bsf_ctx)
	{
		av_bsf_free(&bsf_ctx);
		bsf_ctx = NULL;
		filter = NULL;
	}
}

void VideoReview::Stop()
{
	av_usleep(10 * 1000);
	stop_thread = 1;
	cv.notify_one();
	if (read_thread && read_thread.get()->joinable())
		read_thread.get()->join();
}


unsigned long long VideoReview::getElapsedTime() {
	static std::chrono::high_resolution_clock::time_point lastTime; 
	std::chrono::high_resolution_clock::time_point currentTime = std::chrono::high_resolution_clock::now();

	if (lastTime.time_since_epoch().count() == 0) {
		lastTime = currentTime;
	}

	unsigned long long elapsedTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastTime).count();
	lastTime = currentTime;

	return elapsedTimeMs;
}



int VideoReview::Video_Push()
{
	av_register_all();                  //注册所有功能函数
	avformat_network_init();            //网络功能
	av_log_set_level(AV_LOG_INFO);
	double global_pts_time = 0;
	double Audio_global_pts_time = 0;
	unsigned long long Last_sleep = 0;
	int  first_flag = 0;
	int  audio_first_flag = 0;
	timer.start();
start:

	AVStream *in_stream = NULL, *out_stream = NULL;
	int pts1 = 0;
	int ret = 0;
	int n = 0;
	int err_read = 0;
	uint64_t video_frame_index = 0;
	uint64_t cumulative_frame = 0;
	double first_pts = 0;
	double first_dts = 0;
	double pts_time = 0;
	double dts_time = 0;
	double audio_first_pts = 0;
	double audio_pts_time = 0;
	double audio_dts_time = 0;
	double duration_time = 0;
	double gophead = 0;
	int wait_key_frame = 1;
	stop_video = 0;
	stop_thread = 0;
	printf("start \n");
	
	AVPacket *packet = av_packet_alloc();
	AVPacket *pkt2 = av_packet_alloc();
	

	for (; The_num_video < num && stop_video != 1; The_num_video++)  
	{

		
		if (Open_Input_File() < 0)
		{
			av_log(NULL, AV_LOG_ERROR, "Open_Input_File is error\n");
			Close();
			return -1;
		}
		printf("==================input  Information  ====================\n");
		av_dump_format(pFormatCtx, 0, in_files[The_num_video].file_path, 0);
		printf("videoindex = %d\n", video_index);
		printf("==================input  Information  ====================\n");
		
		AVRational av_time{ 1, 1000 }
		;// 毫秒时间戳
		in_stream = pFormatCtx->streams[video_index];
		double seek_time = av_rescale_q(seek_times, av_time, in_stream->time_base);
//		if (seek_time<0 || seek_time > in_stream->duration)
//		{
//			av_log(NULL, AV_LOG_ERROR, "seek_time is error or too long");
//			Close();
//			av_packet_free(&packet);
//			av_packet_free(&pkt2);
//			return -1;
//		}
		if (av_seek_frame(pFormatCtx, video_index, seek_time, AVSEEK_FLAG_BACKWARD) < 0)
		{
			av_log(NULL, AV_LOG_ERROR, "av_seek_frame error");
			Close();
			av_packet_free(&packet);
			av_packet_free(&pkt2);
			return -1;
		}

		while (1)
		{
			if (stop_thread == 1) 
			{			
				printf("该文件播放时间：%03f 秒\n", timer.getTime());
				av_log(NULL, AV_LOG_WARNING, "while av_read_frame return: %d\n", err_read);			
				goto end;
			}
			err_read = av_read_frame(pFormatCtx, packet);

			if (err_read < 0)
			{
				timer.stop();
				break;
			}
			else if (packet->flags == AV_PKT_FLAG_KEY && wait_key_frame == 1 && packet->stream_index == video_index)
			{
				printf("I frame hava comme\n");
				wait_key_frame = 0;
			}
			else if (wait_key_frame == 1)
			{
				printf("wait for i frame come\n");
				av_packet_unref(packet);
				continue;
			}
			// && packet->flags == AV_PKT_FLAG_KEY

			if (packet->stream_index == video_index && packet->size != 0)
			{

				in_stream = pFormatCtx->streams[video_index];
				if (stop_video == 1) 
				{
					pts_time = packet->pts * av_q2d(in_stream->time_base) - first_pts + global_pts_time;
					printf("该文件播放时间：%03f 秒\n", timer.getTime());
					av_log(NULL, AV_LOG_WARNING, "while av_read_frame return: %d\n", err_read);			
					goto end;
				}
			//	double calcDuration = 1.0 / (av_q2d(in_stream->r_frame_rate));
                
				while (timer.getTime() < pts_time)
				{

					av_usleep(100);
				}
				//if (strcmp(pFormatCtx->oformat->name, "mpegts") != 0)//如果不是ts封装格式需要过滤器
				{
					if (Stream_Filter(in_stream, packet) < 0)
					{
						printf("Stream_Filter is error\n");
						goto end;
					}
				}
				
				
				if (first_flag == 0)
				{
					first_pts = packet->pts * av_q2d(in_stream->time_base);
					first_dts = packet->dts * av_q2d(in_stream->time_base);
					first_flag = 1;
				}
				pts_time = packet->pts * av_q2d(in_stream->time_base) - first_pts + global_pts_time;
				dts_time = packet->dts * av_q2d(in_stream->time_base) - first_dts + global_pts_time;
				duration_time = packet->duration * av_q2d(in_stream->time_base);
//				printf("=====================Video data %d==========================\n", n++);
//				printf("dts_time = %f\n", dts_time);
//				printf("pts_time = %f\n", pts_time);
//				printf("duration = %f\n", duration_time);
//				gophead = pts_time / duration_time;
//				printf("video gophead = %f\n", gophead);
				packet->pos = -1;
				packet->stream_index = packet->stream_index;

				callback(in_stream->codec->codec_id, packet->data, packet->size, packet->flags, (unsigned long long)(dts_time * 1000), args);
				if (ret < 0)
				{
					av_log(NULL, AV_LOG_ERROR, "callback return: %d\n", ret);
					av_packet_unref(packet);
					continue;
				}
				video_frame_index++;


					
				av_packet_unref(packet);
			}
			else if (packet->stream_index == audio_index &&  packet->size != 0)
			{
				
				in_stream = pFormatCtx->streams[audio_index];

				if (in_stream->codec->codec_id == AV_CODEC_ID_AAC && strcmp(pFormatCtx->iformat->name, "mpegts"))//音频是AAC而且不是ts格式
				{              
					merging_audio_packet(in_stream->codec, packet, pkt2);//加上ADTS头
					pkt2->pts = packet->pts;
					pkt2->dts = packet->dts;
					pkt2->duration = packet->duration;
				}
				else if (in_stream->codec->codec_id == AV_CODEC_ID_PCM_S16LE)
				{
					pkt2->dts += 1000.0 * (double)packet->size / in_stream->codec->channels / in_stream->codec->sample_rate;
					pkt2->pts = pkt2->dts;
					packet->duration = packet->duration;
					pkt2 = packet;
				}
				else
				{
					av_packet_ref(pkt2, packet);
				}
				
				
                 
				if (audio_first_flag == 0)
				{
					audio_first_pts = packet->pts * av_q2d(in_stream->time_base);
					audio_first_flag = 1;
				}
				
				audio_pts_time = pkt2->pts * av_q2d(in_stream->time_base) - audio_first_pts + Audio_global_pts_time;//计算时间戳，这里确保一直单调递增
				duration_time = pkt2->duration * av_q2d(in_stream->time_base);
				printf("=====================Audio data %d==========================\n", n++);
				printf("dts_time = %f\n", Audio_pts_time);
				printf("duration = %f\n", duration_time);
				gophead = Audio_pts_time / duration_time;
				printf("Audio gophead = %f\n", gophead);
				printf("Audio Size = %d\n", pkt2->size);
				pkt2->pos = -1;
				pkt2->stream_index = packet->stream_index;

				ret = callback(in_stream->codec->codec_id, pkt2->data, pkt2->size, (unsigned long long)((Audio_pts_time + Audio_global_pts_time) * 1000));
				if (ret < 0)
				{
					av_log(NULL, AV_LOG_ERROR, "callback return: %d\n", ret);
					continue;
				}
				av_packet_unref(packet);
				av_packet_unref(pkt2);
			}			

		}
		seek_times = 0;		
		global_pts_time = pts_time;		
		Audio_global_pts_time = audio_pts_time;		
		first_flag = 0;
		audio_first_flag = 0;
		stop_video = 0;
		pts_time = 0;

	}

end:
	
	av_packet_unref(packet);
	av_packet_unref(pkt2);
	av_packet_free(&packet);
	av_packet_free(&pkt2);
	global_pts_time = pts_time;		
	Audio_global_pts_time = audio_pts_time;	
	first_flag = 0;
	audio_first_flag = 0;
	pts_time = 0;

	/*当文件读取完成时，或者调用Next_Video接口，调用Stop接口都进入这里进行状态判断*/
	if (The_num_video == num)//The_num_video是读取当前文件列表中的第几个视频，等于num说明文件列表读取完成
	{

		std::unique_lock <std::mutex> lk(cvMutex);
		cv.wait(lk);//休眠等待执行信号
		if (stop_thread == 1)
		{
			Close();
			return 0;
		}
		else 
		{
			goto start;
		}
	}
	else if (stop_thread == 1 && The_num_video != num)//当停止线程
	{
		return 0;
	}
	else if (stop_video == 1 && The_num_video != num)//当seekto文件
	{
		goto start;
	}
	return 0;
}


