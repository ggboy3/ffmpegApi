#include "FFmpegServer.h"

enum AVPixelFormat hw_pix_fmt;

#pragma region hw_decodec

FFmpegServer::FFmpegServer()
{
	av_log_set_level(AV_LOG_INFO);
	decodecType = av_hwdevice_find_type_by_name("dxva2");
	if (decodecType == AV_HWDEVICE_TYPE_NONE)
	{
		av_log(nullptr, AV_LOG_ERROR, "Not found DXVA2 decodec\n");
		while ((decodecType = av_hwdevice_iterate_types(decodecType)) != AV_HWDEVICE_TYPE_NONE)
			av_log(nullptr, AV_LOG_ERROR, "The device support decodec is : %s\n", av_hwdevice_get_type_name(decodecType));
	}
	video.vframe = av_frame_alloc();
	video.vpacket = av_packet_alloc();
	audio.aframe = av_frame_alloc();
	audio.apacket = av_packet_alloc();
	video.video_info.url = (const char*)av_mallocz(512);
	audio.url = (const char*)av_mallocz(512);
	video.vdata = (uint8_t*)malloc(size_t(1920 * 1080 * 1.5));
//	audio.adata = (uint8_t*)malloc(2048);
}
enum AVPixelFormat get_hw_format(AVCodecContext* ctx, const enum AVPixelFormat* pix_fmts)
{
	const enum AVPixelFormat* p;
	for (p = pix_fmts; *p != -1; p++)
	{
		if (*p == hw_pix_fmt)
			return *p;
	}
	av_log(nullptr, AV_LOG_ERROR, "Failed to get HW surface format\n");
	return AV_PIX_FMT_NONE;
}
int FFmpegServer::hw_decoder_init(AVCodecContext* ctx, const enum AVHWDeviceType type)
{
	int err = 0;
	if ((err = av_hwdevice_ctx_create(&hw_device_ctx, type, nullptr, nullptr, 0)) < 0)
	{
		av_log(nullptr, AV_LOG_ERROR, "Failed to create specified HW device\n");
		return err;
	}
	ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);

	return err;
}
#pragma endregion

#pragma region decode_fun
int FFmpegServer::audio_decode_read(AudioContext* a_ctx)
{
	int size, ret;
	AVFrame* aframe = nullptr;
//	uint8_t* buffer = nullptr;
	ret = avcodec_send_packet(a_ctx->adecodec_ctx, a_ctx->apacket);
	if (ret < 0) {
		av_log(nullptr, AV_LOG_ERROR, "Error during decoding\n");
		return ret;
	}
	while (true)
	{
		if (!(aframe = av_frame_alloc()))
		{
			av_log(nullptr, AV_LOG_ERROR, "Can not alloc frame\n");
			ret = AVERROR(ENOMEM);
			goto fail;
		}
		ret = avcodec_receive_frame(a_ctx->adecodec_ctx, aframe);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
			av_frame_free(&aframe);
			avcodec_flush_buffers(a_ctx->adecodec_ctx);
			break;
		}
		else if (ret < 0) {
			av_log(nullptr, AV_LOG_ERROR, "Error during decoding\n");
			break;
		}
		av_frame_unref(a_ctx->aframe);
		av_frame_move_ref(a_ctx->aframe, aframe);
		/*a_ctx->aframe->format = aframe->format;
		a_ctx->aframe->channels = aframe->channels;
		a_ctx->aframe->channel_layout = aframe->channel_layout;
		a_ctx->aframe->nb_samples = aframe->nb_samples;
		ret = av_frame_get_buffer(a_ctx->aframe, 32);
		ret = av_frame_copy(a_ctx->aframe,aframe);
		ret = av_frame_copy_props(a_ctx->aframe, aframe);*/
	fail:
		av_frame_free(&aframe);
	}
	return 0;
}
int FFmpegServer::video_decode_read(VideoContext* v_ctx)
{
	AVFrame* frame = nullptr, * sw_frame = nullptr;
	AVFrame* tmp_frame = nullptr;
	uint8_t* buffer = nullptr;
	int size = 0;
	int ret = 0;
	ret = avcodec_send_packet(v_ctx->vdecodec_ctx, v_ctx->vpacket);
	if (ret < 0)
	{
		av_log(nullptr, AV_LOG_ERROR, "Error during decoding\n");
		return ret;
	}
	while (true)
	{
		if (!(frame = av_frame_alloc()) || !(sw_frame = av_frame_alloc()))
		{
			av_log(nullptr, AV_LOG_ERROR, "Can not alloc frame\n");
			ret = AVERROR(ENOMEM);
			goto fail;
		}
		ret = avcodec_receive_frame(v_ctx->vdecodec_ctx, frame);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
		{
			av_frame_free(&frame);
			av_frame_free(&sw_frame);
			break;
		}
		else if (ret < 0)
		{
			av_log(nullptr, AV_LOG_ERROR, "Error while decoding\n");
			goto fail;
		}
		if (frame->format == hw_pix_fmt)
		{
			/* retrieve data from GPU to CPU */
			if ((ret = av_hwframe_transfer_data(sw_frame, frame, 0)) < 0)
			{
				av_log(nullptr, AV_LOG_ERROR, "Error transferring the data to system memory\n");
				goto fail;
			}
			tmp_frame = sw_frame;
		}
		else
		{
			tmp_frame = frame;
		}
		size = av_image_get_buffer_size((AVPixelFormat)tmp_frame->format, tmp_frame->width, tmp_frame->height, 1);
		buffer = (uint8_t*)av_mallocz(size);
		if (!buffer)
		{
			av_log(nullptr, AV_LOG_ERROR, "Can not alloc buffer\n");
			goto fail;
		}
		ret = av_image_copy_to_buffer(buffer, size, (const uint8_t* const*)tmp_frame->data, (const int*)tmp_frame->linesize, (AVPixelFormat)tmp_frame->format, tmp_frame->width, tmp_frame->height, 1);
		if (ret < 0)
		{
			av_log(nullptr, AV_LOG_ERROR, "Can not copy image to buffer\n");
			goto fail;
		}
		av_frame_unref(v_ctx->vframe);
		av_frame_move_ref(v_ctx->vframe, tmp_frame);
		memcpy_s(v_ctx->vdata, size, buffer, size);
	fail:
		av_frame_free(&frame);
		av_frame_free(&sw_frame);
		av_freep(&buffer);
	}
	return 0;
}
#pragma endregion
int FFmpegServer::AudioDecoder(const char* url)
{
	int ret = 0;
	const char  url_path[512] = { 0x00 };
	memset((void*)url_path, 0x00, 512);
	memcpy_s((void*)url_path, 512, url, strlen(url));
	memcpy_s((void*)audio.url, 512, url_path, 512);

	AVDictionary* opts = nullptr;
	av_dict_set(&opts, "buffer_size", "1024000", 0);
	av_dict_set(&opts, "max_delay", "300000", 0);
	av_dict_set(&opts, "stimeout", "2000000", 0);  //设置超时断开连接时间
	av_dict_set(&opts, "timeout", "5000000", 0);   //设置超时5秒
    av_dict_set(&opts, "rtsp_transport", "tcp", 0);  //以udp方式打开，如果以tcp方式打开将udp替换为tcp
	/* open the input file */
	ret = avformat_open_input(&audio.iformat_ctx, url, nullptr, &opts);
	if (ret < 0)
	{
		av_log(nullptr, AV_LOG_ERROR, "Cannot open input file %s\n", url);
		avformat_close_input(&audio.iformat_ctx);
		audio.audio_open_error = true;
		return ret;
	}
	audio.audio_open_error = false;
	ret = avformat_find_stream_info(audio.iformat_ctx, nullptr);
	if (ret < 0)
	{
		av_log(nullptr, AV_LOG_ERROR, "Cannot find input stream information\n");
		return ret;
	}
	ret = av_find_best_stream(audio.iformat_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &audio.adecoder, 0);
	if (ret < 0)
	{
		av_log(nullptr, AV_LOG_ERROR, "Cannot find a audio stream in the input file\n");
		return ret;
	}
	audio.aindex = ret;
	audio.astream = audio.iformat_ctx->streams[audio.aindex];
	audio.adecoder = avcodec_find_decoder(audio.adecoder->id);
	if (!audio.adecoder) {
		av_log(nullptr, AV_LOG_ERROR, "Cannot find a audio decodec \n");
		return ret;
	}
	if (!(audio.adecodec_ctx = avcodec_alloc_context3(audio.adecoder))) //根据编码器信息创建编码器上下文
	{
		av_log(nullptr, AV_LOG_ERROR, "Failed allocating audio output decodec_ctx\n");
		return ret;
	}
	ret = avcodec_parameters_to_context(audio.adecodec_ctx, audio.iformat_ctx->streams[audio.aindex]->codecpar);
	if (ret < 0)
	{
		av_log(nullptr, AV_LOG_ERROR, "Copy parameters to context fail\n");
		return ret;
	}
	if ((ret = avcodec_open2(audio.adecodec_ctx, audio.adecoder, nullptr)) < 0)
	{
		av_log(nullptr, AV_LOG_ERROR, "Failed to open codec for stream #%u\n", audio.aindex);
		return ret;
	}
	audio.isrun = true;
	audio.isdispose = false;
	/* actual decoding and dump the raw data */
	while (audio.isrun)
	{
		ret = av_read_frame(audio.iformat_ctx, audio.apacket);
		if (ret < 0)
		{
			av_log(nullptr, AV_LOG_ERROR, "Read audio frame fail\n");
			break;
		}
		if (audio.aindex == audio.apacket->stream_index)
		{
			ret = audio_decode_read(&audio);
		}
		av_packet_unref(audio.apacket);
	}
	/* flush the decoder */
	ret = audio_decode_read(&audio);
	avcodec_free_context(&audio.adecodec_ctx);
	avformat_close_input(&audio.iformat_ctx);
	audio.audio_open_error = true;
	audio.isrun = false;
	audio.isdispose = true;
	av_dict_free(&opts);
	return 0;
}
int FFmpegServer::VideoDecoder(const char* url, int index)
{
	int ret = 0;
	const char  url_path[512] = { 0x00 };
	memset((void*)url_path, 0x00, 512);
	memcpy_s((void*)url_path, 512, url, strlen(url));
	memcpy_s((void*)video.video_info.url, 512, url_path, 512);
	video.video_info.index = index;
	video_map.insert(make_pair(index, video));
	AVDictionary* opts = nullptr;
	av_dict_set(&opts, "buffer_size", "1024000", 0);
	av_dict_set(&opts, "max_delay", "300000", 0);
	av_dict_set(&opts, "stimeout", "2000000", 0);  //设置超时断开连接时间
	av_dict_set(&opts, "timeout", "5000000", 0);   //设置超时5秒
	av_dict_set(&opts, "rtsp_transport", "tcp", 0);  //以udp方式打开，如果以tcp方式打开将udp替换为tcp
	/* open the input file */
	ret = avformat_open_input(&video_map.at(index).iformat_ctx, url, nullptr, &opts);
	if (ret < 0)
	{
		av_log(nullptr, AV_LOG_ERROR, "Cannot open input file %s\n", url);
		avformat_close_input(&video_map.at(index).iformat_ctx);
		video_map.at(index).video_open_error = true;
		return ret;
	}
	video_map.at(index).video_open_error = false;
	ret = avformat_find_stream_info(video_map.at(index).iformat_ctx, nullptr);
	if (ret < 0)
	{
		av_log(nullptr, AV_LOG_ERROR, "Cannot find input stream information\n");
		return ret;
	}
	/* find the video stream information */
	ret = av_find_best_stream(video_map.at(index).iformat_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &video_map.at(index).vdecoder, 0);
	if (ret < 0)
	{
		av_log(nullptr, AV_LOG_ERROR, "Cannot find a video stream in the input file\n");
		return ret;
	}
	video_map.at(index).vindex = ret;
	for (int i = 0;; i++)
	{
		const AVCodecHWConfig* config = avcodec_get_hw_config(video_map.at(index).vdecoder, i);
		if (!config)
		{
			av_log(nullptr, AV_LOG_ERROR, "Decoder %s does not support device type %s\n", video_map.at(index).vdecoder->name, av_hwdevice_get_type_name(decodecType));
			break;
		}
		if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX && config->device_type == decodecType)
		{
			hw_pix_fmt = config->pix_fmt;
			break;
		}
	}
	av_dump_format(video_map.at(index).iformat_ctx, 0, url, 0);
	if (!(video_map.at(index).vdecodec_ctx = avcodec_alloc_context3(video_map.at(index).vdecoder))) //根据编码器信息创建编码器上下文
	{
		av_log(nullptr, AV_LOG_ERROR, "Failed allocating output decodec_ctx\n");
		return ret;
	}
	video_map.at(index).vstream = video_map.at(index).iformat_ctx->streams[video_map.at(index).vindex];
	if (avcodec_parameters_to_context(video_map.at(index).vdecodec_ctx, video_map.at(index).vstream->codecpar) < 0)
	{
		return ret;
	}
	video_map.at(index).vdecodec_ctx->get_format = get_hw_format;
	if (hw_decoder_init(video_map.at(index).vdecodec_ctx, decodecType) < 0)
	{
		return ret;
	}
	if ((ret = avcodec_open2(video_map.at(index).vdecodec_ctx, video_map.at(index).vdecoder, nullptr)) < 0)
	{
		av_log(nullptr, AV_LOG_ERROR, "Failed to open codec for stream #%u\n", video_map.at(index).vindex);
		return ret;
	}
	video_map.at(index).vpacket = av_packet_alloc();
	if (!video_map.at(index).vpacket)
	{
		av_log(nullptr, AV_LOG_ERROR, "Failed to allocate AVPacket\n");
		return ret;
	}
	av_dict_free(&opts);
	video_map.at(index).isrun = true;
	video_map.at(index).isdispose = false;
	thread thread_decode(&FFmpegServer::StartDecode, this, &video_map.at(index));
	thread_decode.detach();
	return 0;
}

void FFmpegServer::StartDecode(VideoContext* v_ctx)
{
	int ret = 0;
	/* actual decoding and dump the raw data */
	while (v_ctx->isrun)
	{
		ret = av_read_frame(v_ctx->iformat_ctx, v_ctx->vpacket);
		if (ret < 0)
		{
			av_log(nullptr, AV_LOG_ERROR, "Read frame fail\n");
			break;
		}
		if (v_ctx->vindex == v_ctx->vpacket->stream_index)
		{
			ret = video_decode_read(v_ctx);
		}
		av_packet_unref(v_ctx->vpacket);
	}
	/* flush the decoder */
	ret = video_decode_read(v_ctx);
	avcodec_free_context(&v_ctx->vdecodec_ctx);
	avformat_close_input(&v_ctx->iformat_ctx);
	av_buffer_unref(&hw_device_ctx);
	v_ctx->video_open_error = true;
	v_ctx->isrun = false;
	v_ctx->isdispose = true;
}

FFmpegServer::~FFmpegServer()
{
	av_frame_free(&video.vframe);
	av_packet_free(&video.vpacket);
	av_frame_free(&audio.aframe);
	av_packet_free(&audio.apacket);
	avformat_close_input(&video.iformat_ctx);
	avcodec_free_context(&video.vdecodec_ctx);
	avcodec_free_context(&audio.adecodec_ctx);
	av_buffer_unref(&hw_device_ctx);
	av_freep(&video.video_info.url);
	free(&video.vdata);
//	free(&audio.adata);
}