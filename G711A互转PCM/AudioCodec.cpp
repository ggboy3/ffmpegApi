#include "AudioCodec.h"

#define AUDIO_INBUF_SIZE 20480
#define AUDIO_REFILL_THRESH 4096

static char err_buf[128] = { 0 };
static char* av_get_err(int errnum)
{
	av_strerror(errnum, err_buf, 128);
	return err_buf;
}
int CPcmAlawCodec::init_resampler(FrameData * &inData, FrameData * &outData, SwrContext * &swr_context)
{
    int ret;
	if (swr_context == NULL)
	{
		swr_context = swr_alloc();
		av_opt_set_int(swr_context, "in_channel_layout", av_get_default_channel_layout(inData->Channel), 0);//AV_CH_LAYOUT_STEREO
		av_opt_set_int(swr_context, "out_channel_layout", av_get_default_channel_layout(outData->Channel), 0);//AV_CH_LAYOUT_STEREO
		av_opt_set_int(swr_context, "in_sample_rate", inData->SampleRate, 0);//采样率
		av_opt_set_int(swr_context, "out_sample_rate", outData->SampleRate, 0);
		av_opt_set_sample_fmt(swr_context, "in_sample_fmt", AV_SAMPLE_FMT_S16, 0);//这个接口默认是AV_SAMPLE_FMT_S16
		av_opt_set_sample_fmt(swr_context, "out_sample_fmt", outData->SampleBit, 0);//AV_SAMPLE_FMT_FLTP
		swr_init(swr_context);
		if (!swr_context)
		{
			return -1;
		}
	}
   
    return 0;

}

int CPcmAlawCodec::init_swr_frame(AVFrame *decoded_frame, AVFrame * &frame,FrameData *outData)
{
	if (frame != NULL)
		return 0;
    int ret;
    if (!(frame = av_frame_alloc()))
    {
        printf("init_output_frame is error");
        return -1;
    }

	frame->format = outData->SampleBit;
    // 通道数
	frame->channels = outData->Channel;

    // 通道布局
	frame->channel_layout = av_get_channel_layout_nb_channels(outData->Channel);
	
	frame->sample_rate = outData->SampleRate;

    // 每一帧音频的样本数量
	frame->nb_samples = decoded_frame->nb_samples * outData->Channel * outData->SampleRate / decoded_frame->sample_rate;

    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0) {
        //cout << "av_frame_get_buffer failed" << endl;
        return -1;
    }
    return 0;

}


void CPcmAlawCodec::decode(AVCodecContext *dec_ctx,
	AVPacket *pkt,
	AVFrame *frame,
	FrameData * &out_data)
{
	//int i, ch;
	int ret, data_size;
	/* send the packet with the compressed data to the decoder */
	ret = avcodec_send_packet(dec_ctx, pkt);
	if (ret == AVERROR(EAGAIN))
	{
		fprintf(stderr, "Receive_frame and send_packet both returned EAGAIN, which is an API violation.\n");
	}
	else if (ret < 0)
	{
		fprintf(stderr, "Error submitting the packet to the decoder, err:%s, pkt_size:%d\n",
			av_get_err(ret), pkt->size);
		//        exit(1);
		return;
	}

	/* read all the output frames (infile general there may be any number of them */
	while (ret >= 0)
	{
		// 对于frame, avcodec_receive_frame内部每次都先调用
		ret = avcodec_receive_frame(dec_ctx, frame);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			return;
		else if (ret < 0)
		{
			fprintf(stderr, "Error during decoding\n");
			exit(1);
		}
		data_size = av_get_bytes_per_sample(dec_ctx->sample_fmt);
		if (data_size < 0)
		{
			/* This should not occur, checking just for paranoia */
			fprintf(stderr, "Failed to calculate data size\n");
			exit(1);
		}

		init_swr_frame(frame, swr_frame,out_data);
		int len = swr_convert(resample_context, swr_frame->data, frame->nb_samples * swr_frame->sample_rate / frame->sample_rate, (const uint8_t **)frame->data, frame->nb_samples);
            if (len <= 0) {
                //cout << "重采样输出异常" << len << endl;
                break;
            }
				
		/**
			P表示Planar（平面），其数据格式排列方式为 :
			LLLLLLRRRRRRLLLLLLRRRRRRLLLLLLRRRRRRL...（每个LLLLLLRRRRRR为一个音频帧）
			而不带P的数据格式（即交错排列）排列方式为：
			LRLRLRLRLRLRLRLRLRLRLRLRLRLRLRLRLRLRL...（每个LR为一个音频样本）
		 播放范例：   ffplay -ar 48000 -ac 2 -f f32le believe.pcm
		  */
		out_data->frameBufferSize = 0;

		if (swr_frame->format == AV_SAMPLE_FMT_S16P)
		{
			for (int i = 0; i < len; i++)
			{
				for (int ch = 0; ch < swr_frame->channels; ch++)  // 交错的方式写入, 大部分float的格式输出
				{		
					memcpy(out_data->frameBuffer + out_data->frameBufferSize, swr_frame->data[ch] + data_size * i, data_size);
					out_data->frameBufferSize += data_size;
				}
				
			}
			printf("Decode a frame\n");
		}
		else if(swr_frame->format == AV_SAMPLE_FMT_S16)
		{		
			memcpy(out_data->frameBuffer, swr_frame->data[0], len * swr_frame->channels * data_size);
			out_data->frameBufferSize += len * swr_frame->channels * data_size;
			printf("Decode a frame\n");
		}
			
	}
}
int CPcmAlawCodec::encode(AVCodecContext *ctx, AVFrame *frame, AVPacket *pkt, FrameData * &out_data)
{
	int ret;

	    /* send the frame for encoding */
	ret = avcodec_send_frame(ctx, frame);
	if (ret < 0) {
		fprintf(stderr, "Error sending the frame to the encoder\n");
		return -1;
	}

	    /* read all the available output packets (in general there may be any number of them */
	    // 编码和解码都是一样的，都是send 1次，然后receive多次, 直到AVERROR(EAGAIN)或者AVERROR_EOF
	while (ret >= 0) {
		ret = avcodec_receive_packet(ctx, pkt);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
			return 0;
		}
		else if (ret < 0) {
			fprintf(stderr, "Error encoding audio frame\n");
			return -1;
		}

		size_t len = 0;
		
//		len = fwrite(pkt->data, 1, pkt->size, output);
//		if (len != pkt->size) {
//			fprintf(stderr, "fwrite pcm data failed\n");
//			return -1;
//		}
		memcpy(out_data->frameBuffer, pkt->data, pkt->size);
		out_data->frameBufferSize = pkt->size;
		printf("Code a frame\n");
		/* 是否需要释放数据? avcodec_receive_packet第一个调用的就是 av_packet_unref
		* 所以我们不用手动去释放，这里有个问题，不能将pkt直接插入到队列，因为编码器会释放数据
		* 可以新分配一个pkt, 然后使用av_packet_move_ref转移pkt对应的buffer
		*/
		// av_packet_unref(pkt);
	}
	return -1;
}
int CPcmAlawCodec::PcmAlawDecode(FrameData * in_data, FrameData * out_data)
{
	const AVCodec *codec;
	AVCodecContext *codec_ctx = NULL;
	int len = 0;
	int ret = 0;
	FILE *outfile = NULL;
	uint8_t *data = in_data->frameBuffer;
	size_t   data_size = in_data->frameBufferSize;
	enum AVCodecID audio_codec_id ;
	audio_codec_id = in_data->CodecId;

	init_resampler(in_data, out_data,resample_context);

	// 查找解码器
	codec = avcodec_find_decoder(audio_codec_id);  
	if (!codec) {
		printf("Codec not found\n");
		
	}

	// 分配codec上下文
	codec_ctx = avcodec_alloc_context3(codec);
	if (!codec_ctx) {
		printf( "Could not allocate audio codec context\n");
		
	}
	codec_ctx->sample_fmt = in_data->SampleBit;
	codec_ctx->channels = in_data->Channel;
	codec_ctx->sample_rate = in_data->SampleRate;
	// 将解码器和解码器上下文进行关联
	if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
		printf( "Could not open codec\n");
		goto end;
	}


	av_init_packet(pkt);
	pkt->data = data;
	pkt->size = data_size;
	if (pkt->size)
		decode(codec_ctx, pkt, decoded_frame,out_data);
	av_frame_unref(decoded_frame);
	/* 冲刷解码器 */
	pkt->data = NULL;   // 让其进入drain mode
	pkt->size = 0;
	decode(codec_ctx, pkt, decoded_frame, out_data);
end:
	avcodec_free_context(&codec_ctx);
	return 0;
}


int CPcmAlawCodec::PcmEncode(FrameData * in_data, 
	FrameData * out_data)
{
	
	const AVCodec *codec;
	AVCodecContext *codec_ctx = NULL;
	int len = 0;
	int ret = 0;
	int size;
	uint8_t *data = in_data->frameBuffer;
	size_t   data_size = in_data->frameBufferSize;	
	enum AVCodecID audio_codec_id ;
	audio_codec_id = out_data->CodecId;


	if (encoded_frame->channels == 0)//这里是保证只分配一次内存，避免重复申请
	{
		encoded_frame->format = out_data->SampleBit;
		encoded_frame->channel_layout = av_get_default_channel_layout(in_data->Channel);
		encoded_frame->sample_rate = in_data->SampleRate;
		encoded_frame->channels = in_data->Channel;
		encoded_frame->nb_samples = in_data->frameBufferSize / in_data->Channel / 2;
		ret = av_frame_get_buffer(encoded_frame, 0);
		if (ret < 0) {
		    //cout << "av_frame_get_buffer failed" << endl;
			return -1;
		}
	}
	
	
	ret = av_samples_fill_arrays(encoded_frame->data,
		encoded_frame->linesize,
		data,
		encoded_frame->channels,
		encoded_frame->nb_samples,
		(AVSampleFormat)encoded_frame->format,
		0);

	init_resampler(in_data, out_data, resample_context2);
	init_swr_frame(encoded_frame, swr_frame2,out_data);
	len = swr_convert(resample_context2, swr_frame2->data,
		encoded_frame->nb_samples *  swr_frame2->sample_rate / in_data->SampleRate ,//计算重采样输出样本数
		(const uint8_t **)encoded_frame->data, encoded_frame->nb_samples);
	if (len <= 0) {
		printf("swr_convert is error\n");	
	}
	
	size = av_get_bytes_per_sample((AVSampleFormat)swr_frame2->format);
	if (size < 0)
	{
		/* This should not occur, checking just for paranoia */
		printf( "Failed to calculate data size\n");
		goto end;
	}
	
	// 查找解码器
	codec = avcodec_find_encoder(audio_codec_id);  
	if (!codec) {
		printf( "Codec not found\n");
		goto end;
	}
	
	// 分配codec上下文
	codec_ctx = avcodec_alloc_context3(codec);
	if (!codec_ctx) {
		printf("Could not allocate audio codec context\n");
		goto end;
	}
	codec_ctx->sample_fmt = out_data->SampleBit;
	codec_ctx->channels = out_data->Channel;
	codec_ctx->sample_rate = out_data->SampleRate;
	codec_ctx->bit_rate = 64000;
	codec_ctx->channel_layout = av_get_default_channel_layout(out_data->Channel);
	
	// 将解码器和解码器上下文进行关联
	if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
		printf("Could not open codec\n");
		goto end;
	}

	av_init_packet(pkt2);
	
	encode(codec_ctx, swr_frame2, pkt2, out_data);	
	
end:
	    /* 冲刷编码器 */
	encode(codec_ctx, NULL, pkt2, out_data);
	avcodec_free_context(&codec_ctx);
	return 0;
}
void CPcmAlawCodec::Init()
{
	av_register_all();//初始化所有编解码器
	if (!(pkt = av_packet_alloc()))
	{
		printf("Could not allocate audio frame\n");
	}
	
	if (!(pkt2 = av_packet_alloc()))
	{
		printf("Could not allocate audio frame\n");
	}
	if (!(decoded_frame = av_frame_alloc()))
	{
		printf("Could not allocate audio frame\n");
	}
	if (!(encoded_frame = av_frame_alloc()))
	{
		printf("Could not allocate audio frame\n");
	}
	resample_context = NULL;
	resample_context2 = NULL;
	swr_frame = NULL;
	swr_frame2 = NULL;

}
void CPcmAlawCodec::Uinit()
{
	if (decoded_frame)
	{
		av_frame_free(&decoded_frame);
	}
	if (encoded_frame)
	{
		av_frame_free(&encoded_frame);
	}
	if (swr_frame)
	{
		av_frame_free(&swr_frame);
	}
	if (swr_frame2)
	{
		av_frame_free(&swr_frame2);
	}
	if (pkt)
	{
		av_packet_free(&pkt); 
	}
	if (pkt2)
	{
		av_packet_free(&pkt2); 
	}
	if (resample_context)
	{
		swr_free(&resample_context);
	}
	if (resample_context2)
	{
		swr_free(&resample_context2);
	}

	
}

