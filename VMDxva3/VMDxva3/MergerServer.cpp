#include "MergerServer.h"

static FILE* output_file = NULL;
MergerServer::MergerServer()
{
}

int MergerServer::MergeStartup(const char* url, const char* audio_url, VideoInfo* video)
{
	int ret, pic_stream_index;
	const AVCodec* pic_decoder = nullptr;
	AVCodecContext* pic_decodec_ctx = nullptr;
	AVFormatContext* iformat_ctx = nullptr;
	AVPacket* pic_packet = nullptr;
	AVFrame* pic_frame = nullptr;
	uint8_t* buffer = nullptr;

	ret = avformat_open_input(&iformat_ctx, url, nullptr, nullptr);
	if (ret < 0)
	{
		avformat_close_input(&iformat_ctx);
		av_log(nullptr, AV_LOG_ERROR, "open picture faile\n");
		return ret;
	}
	ret = avformat_find_stream_info(iformat_ctx, nullptr);
	if (ret < 0)
	{
		av_log(nullptr, AV_LOG_ERROR, "Cannot find input stream information\n");
		return ret;
	}
	ret = av_find_best_stream(iformat_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &pic_decoder, 0);
	if (ret < 0)
	{
		av_log(nullptr, AV_LOG_ERROR, "Cannot find a video stream in the input file\n");
		return ret;
	}
	pic_stream_index = ret;
	av_dump_format(iformat_ctx, 0, url, 0);
	if (!(pic_decodec_ctx = avcodec_alloc_context3(pic_decoder)))
	{
		avcodec_free_context(&pic_decodec_ctx);
		av_log(nullptr, AV_LOG_ERROR, "Cannot alloc context3 to pic_decoder\n");
		return ret;
	}
	if ((ret = avcodec_open2(pic_decodec_ctx, pic_decoder, nullptr)) < 0)
	{
		av_log(nullptr, AV_LOG_ERROR, "Failed to open codec for stream #%u\n", pic_stream_index);
		return ret;
	}
	pic_packet = av_packet_alloc();
	if (pic_frame == nullptr)
	{
		pic_frame = av_frame_alloc();
	}
	av_new_packet(pic_packet, pic_decodec_ctx->width * pic_decodec_ctx->height);
	while ((av_read_frame(iformat_ctx, pic_packet)) >= 0)
	{
		if (pic_stream_index == pic_packet->stream_index)
		{
			ret = avcodec_send_packet(pic_decodec_ctx, pic_packet);
			if (ret < 0)
			{
				av_log(nullptr, AV_LOG_ERROR, "Error sending a packet for decoding\n");
				return ret;
			}
			ret = avcodec_receive_frame(pic_decodec_ctx, pic_frame);
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			{
				avcodec_flush_buffers(pic_decodec_ctx);
				return ret;
			}
			else if (ret < 0)
			{
				av_log(nullptr, AV_LOG_ERROR, "Error during decoding\n");
				return ret;
			}
			av_log(nullptr, AV_LOG_INFO, "saving frame %d\n", pic_decodec_ctx->frame_num);
		}
		av_packet_unref(pic_packet);
	}
	av_packet_free(&pic_packet);

	ret = EncodecInit("rtp://192.168.1.11/live");
	if (ret < 0)
	{
		av_log(iformat_ctx, AV_LOG_ERROR, "Encode init fail\n");
		return ret;
	}
	AVDictionary* opts = nullptr;
	av_dict_set_int(&opts, "video_track_timescale", 25, 0);  //MP4 格式需要
	avformat_write_header(oformat_ctx, &opts);
	av_dict_free(&opts);

	decode_audio = new FFmpegServer();
	decode_video[video[1].index] = new FFmpegServer();
reload:
	thread thread_audio(&FFmpegServer::AudioDecoder, decode_audio, audio_url);
	thread_audio.detach();
	thread thread_video1(&FFmpegServer::VideoDecoder, decode_video[video[1].index], video[1].url, video[1].index);
	thread_video1.join();

	//output_file = fopen("out2.mp3", "w+b");
	int video_frame_index = 1, audio_frame_index = 0;
	isrun_merge = true;
	AVRational time_base = { 1,120000 };
	while (true)
	{

		static int flag = 0;
		if (flag)
		{
			flag = 0;
			av_image_fill_arrays(video_st.frame->data, video_st.frame->linesize, decode_video[video[1].index]->video_map.at(1).vdata, AV_PIX_FMT_YUV420P, 1920, 1080, 1);
			ret = avcodec_send_frame(video_st.codec_ctx, video_st.frame);
			if (ret < 0)
			{
				av_log(oformat_ctx, AV_LOG_ERROR, "Error sending a frame for encoding\n");
				return ret;
			}
			while (ret >= 0)
			{
				ret = avcodec_receive_packet(video_st.codec_ctx, video_st.packet);
				if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
				{
					break;
				}
				else if (ret < 0)
				{
					av_log(oformat_ctx, AV_LOG_ERROR, "Error during encoding\n");
					return ret;
				}
				AVRational fps{ 25,1 };
				video_st.stream->time_base = { 1,10000 };
				double calc_duration = 1.0 / av_q2d(fps);//
				video_st.packet->pts = (double)(video_frame_index * calc_duration) / (double)(av_q2d(video_st.stream->time_base));
				video_st.packet->dts = video_st.packet->pts;
				video_st.packet->duration = (double)calc_duration / (double)(av_q2d(video_st.stream->time_base));
				video_frame_index++;
				/*video_st.packet->pts = av_rescale_q_rnd(video_st.packet->pts, time_base, video_st.stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
				video_st.packet->dts = av_rescale_q_rnd(video_st.packet->dts, time_base, video_st.stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
				video_st.packet->duration = av_rescale_q(video_st.packet->duration, time_base, video_st.stream->time_base);*/
				video_st.packet->pos = -1;
				video_st.packet->stream_index = 0;
				av_log(oformat_ctx, AV_LOG_INFO, "Video write packet PTS=%d DTS=%d "" (size=%5d)\n", video_st.packet->pts, video_st.packet->dts, video_st.packet->size);
				av_interleaved_write_frame(oformat_ctx, video_st.packet);
				av_packet_unref(video_st.packet);
			}
		}
		else
		{
			flag = 1;
			audio_st.swr_ctx = swr_alloc();
			if (!audio_st.swr_ctx)
			{
				av_log(oformat_ctx, AV_LOG_ERROR, "%s", "swr_alloc_set_opts failed!");
				return ret;
			}
			/* set options */
			av_opt_set_chlayout(audio_st.swr_ctx, "in_chlayout", &decode_audio->audio.adecodec_ctx->ch_layout, 0);
			av_opt_set_int(audio_st.swr_ctx, "in_sample_rate", decode_audio->audio.adecodec_ctx->sample_rate, 0);
			av_opt_set_sample_fmt(audio_st.swr_ctx, "in_sample_fmt", decode_audio->audio.adecodec_ctx->sample_fmt, 0);

			av_opt_set_chlayout(audio_st.swr_ctx, "out_chlayout", &audio_st.codec_ctx->ch_layout, 0);
			av_opt_set_int(audio_st.swr_ctx, "out_sample_rate", audio_st.codec_ctx->sample_rate, 0);
			av_opt_set_sample_fmt(audio_st.swr_ctx, "out_sample_fmt", audio_st.codec_ctx->sample_fmt, 0);

			ret = swr_init(audio_st.swr_ctx);
			if (ret < 0)
			{
				av_log(oformat_ctx, AV_LOG_ERROR, "%s", "Failed to initialize the resampling context");
				swr_free(&audio_st.swr_ctx);
				return ret;
			}

			swr_convert(audio_st.swr_ctx, audio_st.frame->data, audio_st.frame->nb_samples, (const uint8_t**)decode_audio->audio.aframe->data, decode_audio->audio.aframe->nb_samples);
			swr_free(&audio_st.swr_ctx);

			ret = avcodec_send_frame(audio_st.codec_ctx, audio_st.frame);
			if (ret < 0)
			{
				av_log(oformat_ctx, AV_LOG_ERROR, "Error sending the frame to the encoder\n");
				return ret;
			}
			while (ret >= 0)
			{
				ret = avcodec_receive_packet(audio_st.codec_ctx, audio_st.packet);
				if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
				{
					break;
				}
				else if (ret < 0)
				{
					av_log(oformat_ctx, AV_LOG_ERROR, "Error encoding audio frame\n");
					return ret;
				}
				double calc_duration = (double)audio_st.codec_ctx->sample_rate / (double)audio_st.codec_ctx->bit_rate;
				audio_st.stream->time_base = { 1,10000 };
				audio_st.packet->pts = (double)(audio_frame_index * calc_duration) / (double)(av_q2d(audio_st.stream->time_base));
				audio_st.packet->dts = audio_st.packet->pts;
				audio_st.packet->duration = (double)calc_duration / (double)(av_q2d(time_base));
				audio_frame_index++;
				/*audio_st.packet->pts = av_rescale_q_rnd(audio_st.packet->pts, time_base, audio_st.stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
				audio_st.packet->dts = av_rescale_q_rnd(audio_st.packet->dts, time_base, audio_st.stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
				audio_st.packet->duration = av_rescale_q(audio_st.packet->duration, time_base, audio_st.stream->time_base);*/
				audio_st.packet->pos = -1;
				audio_st.packet->stream_index = 1;
				av_log(oformat_ctx, AV_LOG_INFO, "Audio write packet PTS=%d DTS=%d "" (size=%5d)\n", audio_st.packet->pts, audio_st.packet->dts, audio_st.packet->size);
				av_interleaved_write_frame(oformat_ctx, audio_st.packet);
				av_packet_unref(audio_st.packet);
			}
		}
		//if (i == 200)
		//{
		//	VideoInfo video;
		//	video.url = "rtsp://admin:jkkj12345@192.168.1.66/h264/ch1/sub/av_stream";
		//	video.index = 3;
		//	ChangeMergeVideo(video);
		//}
	}

	av_write_trailer(oformat_ctx);
	isrun_merge = false;
	avcodec_free_context(&pic_decodec_ctx);
	avformat_close_input(&iformat_ctx);
	av_freep(buffer);
	return 0;
}

int MergerServer::ChangeMergeVideo(VideoInfo video)
{
	if (isrun_merge)
	{
		decode_video[video.index]->video_map.at(video.index).isrun = false;
		while (!decode_video[video.index]->video_map.at(video.index).isdispose)
		{
			fprintf(stderr, "Waiting for resource release...\n");
			this_thread::sleep_for(chrono::milliseconds(20));
			if (decode_video[video.index]->video_map.at(video.index).video_open_error) break;
		}
		thread thread_video(&FFmpegServer::VideoDecoder, decode_video[video.index], video.url, video.index);
		thread_video.join();
	}
	else
	{
		fprintf(stderr, "Merge adapter is not started, please start and try again\n");
	}
	return 0;
}

int MergerServer::ReloadVideo(const char* audio_url)
{
	while (true)
	{
		for (int i = 1; i < 5; i++)
		{
			if (decode_video[i]->video_map.at(i).isdispose || decode_video[i]->video_map.at(i).video_open_error)
			{
				ChangeMergeVideo(decode_video[i]->video_map.at(i).video_info);
			}
		}
		if (decode_video[1]->audio.isdispose || decode_video[1]->audio.audio_open_error)
		{
			decode_video[1]->audio.isrun = false;
			thread thread_audio(&FFmpegServer::AudioDecoder, decode_video[1], audio_url);
			thread_audio.detach();
		}
		this_thread::sleep_for(chrono::milliseconds(3000));
	}
	return 0;
}

int MergerServer::EncodecInit(const char* out_url)
{
	int ret = 0;
	const AVOutputFormat* fmt = nullptr;
	const AVCodec* audio_codec = nullptr, * video_codec = nullptr;
	ret = avformat_alloc_output_context2(&oformat_ctx, nullptr, "rtsp", out_url);
	if (ret < 0)
	{
		av_log(oformat_ctx, AV_LOG_ERROR, "Could not allocate oformat_ctx\n");
		goto fail;
	}
	fmt = oformat_ctx->oformat;
	if (fmt->video_codec != AV_CODEC_ID_NONE)
	{
		AddStream(&video_st, oformat_ctx, &video_codec, "h264_mf");
	}
	if (fmt->audio_codec != AV_CODEC_ID_NONE)
	{
		AddStream(&audio_st, oformat_ctx, &audio_codec, "pcm_alaw");
	}
	if (!(oformat_ctx->flags & AVFMT_NOFILE))
	{
		ret = avio_open(&oformat_ctx->pb, out_url, AVIO_FLAG_WRITE);
		if (ret < 0)
		{
			av_log(oformat_ctx, AV_LOG_ERROR, "Error occurred when opening output URL\n");
			goto fail;
		}
	}

	av_dump_format(oformat_ctx, 0, out_url, 1);

	return 0;
fail:
	avio_close(oformat_ctx->pb);
	avformat_close_input(&oformat_ctx);
	return -100;
}

int MergerServer::AddStream(OutputStream* ost, AVFormatContext* oc, const AVCodec** codec, const char* encodec_name)
{
	int ret = -1;
	*codec = avcodec_find_encoder_by_name(encodec_name);
	if (!(*codec))
	{
		av_log(ost, AV_LOG_ERROR, "Could not find encoder for '%s'\n", encodec_name);
		goto fail;
	}
	ost->packet = av_packet_alloc();
	if (!ost->packet)
	{
		av_log(ost, AV_LOG_ERROR, "Could not allocate AVPacket\n");
		goto fail;
	}
	ost->stream = avformat_new_stream(oc, NULL);
	if (!ost->stream)
	{
		av_log(ost, AV_LOG_ERROR, "Could not allocate stream\n");
		goto fail;
	}
	ost->stream->id = oc->nb_streams - 1;
	ost->codec_ctx = avcodec_alloc_context3(*codec);
	if (!ost->codec_ctx)
	{
		av_log(ost, AV_LOG_ERROR, "Could not alloc an encoding context\n");
		goto fail;
	}
	switch ((*codec)->type)
	{
	case AVMEDIA_TYPE_AUDIO:
		ost->codec_ctx->bit_rate = 64000;
		ost->codec_ctx->sample_fmt = AV_SAMPLE_FMT_S16;
		ost->codec_ctx->codec_type = AVMEDIA_TYPE_AUDIO;
		ost->codec_ctx->sample_rate = 8000;
		ost->codec_ctx->channel_layout = AV_CH_LAYOUT_MONO;
		ost->codec_ctx->channels = av_get_channel_layout_nb_channels(ost->codec_ctx->channel_layout);
		ost->codec_ctx->time_base = { 1, 10000 };
		ost->codec_ctx->frame_size = 320;
		av_channel_layout_default(&ost->codec_ctx->ch_layout, ost->codec_ctx->channels);
		ost->frame = av_frame_alloc();
		if (!ost->frame)
		{
			av_log(ost, AV_LOG_ERROR, "Could not allocate video frame\n");
			goto fail;
		}
		ost->frame->sample_rate = ost->codec_ctx->sample_rate;
		ost->frame->format = ost->codec_ctx->sample_fmt;
		ost->frame->nb_samples = ost->codec_ctx->frame_size;
		ost->frame->channel_layout = ost->codec_ctx->channel_layout;
		av_channel_layout_copy(&ost->frame->ch_layout, &ost->codec_ctx->ch_layout);
		ost->stream->time_base = { 1, 10000 };
		ost->frame->pts = 0;
		ret = av_frame_get_buffer(ost->frame, 0);
		if (ret < 0)
		{
			av_log(ost, AV_LOG_ERROR, "Could not allocate the video frame data\n");
			goto fail;
		}
		ost->packet = av_packet_alloc();
		if (!ost->packet)
		{
			av_log(ost, AV_LOG_ERROR, "alloc en_packet fail\n");
			goto fail;
		}
		ret = avcodec_parameters_from_context(ost->stream->codecpar, ost->codec_ctx);
		if (ret < 0)
		{
			av_log(ost, AV_LOG_ERROR, "copy Codec_ctx to audio stream codecpar failed\n");
			goto fail;
		}
		//打开编码器
		ret = avcodec_open2(ost->codec_ctx, *codec, nullptr);
		if (ret < 0)
		{
			av_log(ost, AV_LOG_ERROR, "Could not open codec\n");
			goto fail;
		}
		break;

	case AVMEDIA_TYPE_VIDEO:
		ost->codec_ctx->bit_rate = 90000;
		ost->codec_ctx->width = 1920;
		ost->codec_ctx->height = 1080;
		ost->codec_ctx->framerate = { 25,1 };//fps
		ost->stream->time_base = { 1, 10000 };
		ost->codec_ctx->time_base = { 1, 10000 };
		ost->codec_ctx->gop_size = 50; /* emit one intra frame every twelve frames at most */
		ost->codec_ctx->pix_fmt = AV_PIX_FMT_NV12;
		ost->codec_ctx->max_b_frames = 0;
		if ((*codec)->id == AV_CODEC_ID_H264)
		{
			av_opt_set(ost->codec_ctx->priv_data, "preset", "medium", 0);
			av_opt_set(ost->codec_ctx->priv_data, "tune", "zerolatency", 0);
			av_opt_set(ost->codec_ctx->priv_data, "profile", "main", 0);
			av_opt_set(ost->codec_ctx->priv_data, "x264opts", "crf=20", 0);
		}
		//分配数据帧参数
		ost->frame = av_frame_alloc();
		if (!ost->frame)
		{

			av_log(ost, AV_LOG_ERROR, "Could not allocate video frame\n");
			goto fail;
		}
		ost->frame->format = ost->codec_ctx->pix_fmt;
		ost->frame->width = ost->codec_ctx->width;
		ost->frame->height = ost->codec_ctx->height;
		ost->frame->pts = 0;
		ret = av_frame_get_buffer(ost->frame, 0);
		if (ret < 0)
		{
			av_log(ost, AV_LOG_ERROR, "Could not allocate the video frame data\n");
			goto fail;
		}
		//分配数据包内存
		ost->packet = av_packet_alloc();
		if (!ost->packet)
		{
			av_log(ost, AV_LOG_ERROR, "alloc en_packet fail\n");
			goto fail;
		}
		ret = avcodec_parameters_from_context(ost->stream->codecpar, ost->codec_ctx);
		if (ret < 0)
		{
			av_log(ost, AV_LOG_ERROR, "copy Codec_ctx to video stream codecpar failed\n");
			goto fail;
		}
		//打开编码器
		ret = avcodec_open2(ost->codec_ctx, *codec, nullptr);
		if (ret < 0)
		{
			av_log(ost, AV_LOG_ERROR, "Could not open codec\n");
			goto fail;
		}
		break;

	default:
		break;
	}
	/* Some formats want stream headers to be separate. */
	if (oc->oformat->flags & AVFMT_GLOBALHEADER)
	{
		ost->codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}
	return 0;
fail:
	av_packet_free(&ost->packet);
	av_frame_free(&ost->frame);
	avcodec_close(ost->codec_ctx);
	return ret;
}

MergerServer::~MergerServer()
{
	delete[] & decode_video;
}