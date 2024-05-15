#include "pcm_alaw_to_aac.h"
#include "mg_log.h"

int init_decoder(audio_ctx_s* ctx);
int init_encoder(audio_ctx_s* ctx);
void add_adts_head(audio_ctx_s* ctx, uint8_t* pData, int packLen);

audio_ctx_s* ijk_create_audio_ctx(int sample_rate, int channels,enum AVCodecID codec_id)
{
	int i = 0;
    audio_ctx_s* ctx = (audio_ctx_s *)malloc(sizeof(audio_ctx_s));
    memset(ctx, 0, sizeof(audio_ctx_s));

    ctx->m_sampleRate = sample_rate;
    ctx->m_channels = channels;
    ctx->m_codecId = codec_id;

    ctx->m_pcmBuffer = (uint8_t *)malloc(PCM_BUFFER_SIZE * sizeof(uint8_t));
	memset(ctx->m_pcmBuffer, 0, PCM_BUFFER_SIZE);

	for (i = 0; i < 8; i++)
	{
        ctx->m_pcmPointer[i] = (uint8_t *)malloc(PCM_BUFFER_SIZE * sizeof(uint8_t));
	}
    return ctx;
}

void ijk_destroy_audio_ctx(audio_ctx_s* ctx)
{
	int i = 0;
	if (NULL != ctx->m_dePkt)
	{
		av_packet_free(&ctx->m_dePkt);
        ctx->m_dePkt = NULL;
	}
	if (NULL != ctx->m_deFrame)
	{
		av_frame_free(&ctx->m_deFrame);
        ctx->m_deFrame = NULL;
	}
	if (NULL != ctx->m_deCtx)
	{
		avcodec_close(ctx->m_deCtx);
		avcodec_free_context(&ctx->m_deCtx);
        ctx->m_deCtx = NULL;
	}
	if (NULL != ctx->m_decoder)
        ctx->m_decoder = NULL;

	if (NULL != ctx->m_enPkt)
	{
		av_packet_free(&ctx->m_enPkt);
        ctx->m_enPkt = NULL;
	}
	if (NULL != ctx->m_enFrame)
	{
		av_frame_free(&ctx->m_enFrame);
        ctx->m_enFrame = NULL;
	}
	if (NULL != ctx->m_enCtx)
	{
		avcodec_close(ctx->m_enCtx);
		avcodec_free_context(&ctx->m_enCtx);
        ctx->m_enCtx = NULL;
	}
	if (NULL != ctx->m_encoder)
        ctx->m_encoder = NULL;

	if (NULL != ctx->m_resampleCtx)
	{
		swr_free(&ctx->m_resampleCtx);
        ctx->m_resampleCtx = NULL;
	}

	if (NULL != ctx->m_pcmBuffer)
	{
		free(ctx->m_pcmBuffer);
        ctx->m_pcmBuffer = NULL;
        ctx->m_pcmSize = 0;
	}

	for (i = 0; i < 8; i++)
	{
		free(ctx->m_pcmPointer[i]);
        ctx->m_pcmPointer[i] = NULL;
	}
}

int init_decoder(audio_ctx_s* ctx)
{
    ctx->m_decoder = avcodec_find_decoder(ctx->m_codecId);
	if (!ctx->m_decoder)
	{
		LOG_ERROR("decoder:%d not found. ",ctx->m_codecId);
		return -1;
	}

    ctx->m_deCtx = avcodec_alloc_context3(ctx->m_decoder);
	if (!ctx->m_deCtx)
	{
		LOG_ERROR("could not allocate audio decoder context. ");
		return -1;
	}
    ctx->m_deCtx->sample_rate = ctx->m_sampleRate;
    ctx->m_deCtx->channels = ctx->m_channels;

	if (avcodec_open2(ctx->m_deCtx, ctx->m_decoder, NULL) < 0)
	{
		LOG_ERROR("could not open audio decoer. ");
		return -1;
	}

    ctx->m_dePkt = av_packet_alloc();
	if (NULL == ctx->m_dePkt)
	{
        LOG_ERROR("av_packet_alloc error ");
		return -1;
	}
    ctx->m_deFrame = av_frame_alloc();
	if (NULL == ctx->m_deFrame)
	{
        LOG_ERROR("av_frame_alloc error ");
		return -1;
	}
	printf("init_decoder ok \n ");
	return 0;
}


int ijk_add_audio_data(audio_ctx_s* ctx, uint8_t * pData, int iSize)
{
	if (!ctx->m_initDecoder)
	{
        ctx->m_initDecoder = 1;
        ctx->m_initDecoderOK = (init_decoder(ctx) == 0 ? 1 : 0);
	}
	if (!ctx->m_initDecoderOK)
    {
        LOG_ERROR("ctx->m_initDecoder = %d ctx->m_initDecoderOK = %d. ", ctx->m_initDecoder, ctx->m_initDecoderOK);
		return -1;
    }

    ctx->m_dePkt->size = iSize;
    ctx->m_dePkt->data = (uint8_t *)av_malloc(ctx->m_dePkt->size);
	memcpy(ctx->m_dePkt->data, pData, ctx->m_dePkt->size);
	int ret = av_packet_from_data(ctx->m_dePkt, ctx->m_dePkt->data, ctx->m_dePkt->size);
	if (ret < 0)
	{
		LOG_ERROR("av_packet_from_data error. ");
		av_free(ctx->m_dePkt->data);
        ctx->m_dePkt->data = NULL;
        ctx->m_dePkt->size = 0;
		return -1;
	}

	ret = avcodec_send_packet(ctx->m_deCtx, ctx->m_dePkt);
	av_packet_unref(ctx->m_dePkt);
	if (ret < 0)
	{
		LOG_ERROR("av_codec_send_packet error. ");
		return -1;
	}
	ret = avcodec_receive_frame(ctx->m_deCtx, ctx->m_deFrame);
	if (ret < 0)
	{
		LOG_ERROR("avcodec_receive_frame error. ");
		return -1;
	}

    ctx->m_sampleFmt = ctx->m_deCtx->sample_fmt = ctx->m_deFrame->format;
	int data_size = av_get_bytes_per_sample(ctx->m_deCtx->sample_fmt);
	if (data_size < 0)
	{
		LOG_ERROR("failed to calculate audio data size. ");
		return -1;
	}

   /* LOG_ERROR("format : %d decode data size : %d. ", ctx->m_deFrame->format, data_size);*/
	memcpy(ctx->m_pcmBuffer + ctx->m_pcmSize, ctx->m_deFrame->data[0], 
        ctx->m_deFrame->nb_samples * ctx->m_deCtx->channels * data_size);
    ctx->m_pcmSize += ctx->m_deFrame->nb_samples * ctx->m_deCtx->channels * data_size;

    return 0;
}


int init_encoder(audio_ctx_s* ctx)
{
    ctx->m_encoder = avcodec_find_encoder(AV_CODEC_ID_AAC);
	if (!ctx->m_encoder)
	{
		LOG_ERROR("encoder not found. ");
		return -1;
	}
    ctx->m_enCtx = avcodec_alloc_context3(ctx->m_encoder);
	if (!ctx->m_enCtx)
	{
		LOG_ERROR("could not allocate audio encoder context. ");
		return -1;
	}
    ctx->m_enCtx->channels = ctx->m_channels;
    ctx->m_enCtx->channel_layout = av_get_default_channel_layout(ctx->m_channels);
    ctx->m_enCtx->sample_rate = ctx->m_sampleRate;
    ctx->m_enCtx->sample_fmt = AV_SAMPLE_FMT_FLTP;
    ctx->m_enCtx->bit_rate = 8000; //2?¨º¦Ì?¨¹g711a¨°??¦Ì2¨¦?¨´?¨º?a8000¡ê?????¦Ì?DT??¡Á¡é¨°aDT??
    ctx->m_enCtx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

    LOG_ERROR("channels:%d,channel_layout:%" PRIi64 ",sample_rate:%d ",ctx->m_enCtx->channels,
    ctx->m_enCtx->channel_layout, ctx->m_enCtx->sample_rate);

	if (avcodec_open2(ctx->m_enCtx, ctx->m_encoder, NULL) < 0)
	{
		LOG_ERROR("could not open encoder. ");
		return -1;
	}
    ctx->m_enPkt = av_packet_alloc();
	if (NULL == ctx->m_enPkt)
	{
		printf("av_packet_alloc for  m_enPkt fail \n");
		return -1;
	}
    ctx->m_enFrame = av_frame_alloc();
	if (NULL == ctx->m_enFrame)
	{
		printf("av_frame_alloc for  m_enFrame fail \n");
		return -1;
	}
    ctx->m_resampleCtx = swr_alloc_set_opts(NULL, ctx->m_enCtx->channel_layout, ctx->m_enCtx->sample_fmt,
		ctx->m_enCtx->sample_rate, ctx->m_enCtx->channel_layout, ctx->m_sampleFmt, ctx->m_enCtx->sample_rate, 0, NULL);
	if (NULL == ctx->m_resampleCtx)
	{
		LOG_ERROR("could not allocate audio sample context. ");
		return -1;
	}
	if (swr_init(ctx->m_resampleCtx) < 0)
	{
		LOG_ERROR("could not open resample context. ");
		return -1;
	}
	return 0;
}


int ijk_get_audio_data(audio_ctx_s* ctx, uint8_t * pData, int * iSize)
{
	if (0 == ctx->m_initEncoder)
	{
        ctx->m_initEncoder = 1;
        ctx->m_initEncoderOK = (init_encoder(ctx) == 0 ? 1 : 0);
	}
	if (0 == ctx->m_initEncoderOK)
    {
        printf("ctx->m_initEncoderOK = %d. \n", ctx->m_initEncoderOK);
		return -1;
    }

	int data_size = av_get_bytes_per_sample(ctx->m_sampleFmt);
	if (ctx->m_pcmSize < data_size * 1024 * ctx->m_channels)
	{
		// LOG_ERROR("ctx->m_pcmSize:%d data_size:%d", ctx->m_pcmSize,data_size);
		return -1;
	}

	memcpy(ctx->m_pcmPointer[0], ctx->m_pcmBuffer, data_size * 1024 * ctx->m_channels);
    ctx->m_pcmSize = ctx->m_pcmSize - data_size * 1024 * ctx->m_channels;
	memcpy(ctx->m_pcmBuffer, ctx->m_pcmBuffer + data_size * 1024 * ctx->m_channels, ctx->m_pcmSize);

    ctx->m_enFrame->pts = ctx->m_pts;
    ctx->m_pts += 1024;
    ctx->m_enFrame->nb_samples = 1024;
    ctx->m_enFrame->format = ctx->m_enCtx->sample_fmt;
    ctx->m_enFrame->channel_layout = ctx->m_enCtx->channel_layout;
    ctx->m_enFrame->sample_rate = ctx->m_enCtx->sample_rate;
	if (av_frame_get_buffer(ctx->m_enFrame, 0) < 0)
	{
		LOG_ERROR("could not allocate audio data buffer. ");
		return -1;
	}
	if (swr_convert(ctx->m_resampleCtx, ctx->m_enFrame->extended_data, ctx->m_enFrame->nb_samples,
		(const uint8_t **)ctx->m_pcmPointer, 1024) < 0)
	{
		LOG_ERROR("could not convert input sample. ");
		if (NULL != ctx->m_enFrame)
			av_frame_unref(ctx->m_enFrame);
		return -1;
	}
	int ret = avcodec_send_frame(ctx->m_enCtx, ctx->m_enFrame);
	if (ret < 0)
	{
		LOG_ERROR("avcodec_send_frame error.");
		if (NULL != ctx->m_enFrame)
			av_frame_unref(ctx->m_enFrame);
		return -1;
	}
	if (NULL != ctx->m_enFrame)
		av_frame_unref(ctx->m_enFrame);
	
	ret = avcodec_receive_packet(ctx->m_enCtx, ctx->m_enPkt);
	if (ret < 0)
	{
		LOG_ERROR("avcodec_receive_packet error. ret = %d", ret);
		return -1;
	}

	*iSize = ctx->m_enPkt->size + 7;
	add_adts_head(ctx, pData, ctx->m_enPkt->size + 7);
	memcpy(pData + 7, ctx->m_enPkt->data, ctx->m_enPkt->size);
	av_packet_unref(ctx->m_enPkt);
	return 0;
}

void add_adts_head(audio_ctx_s* ctx, uint8_t* pData, int packLen)
{
	int profile = 1; // AAC LC  
	int freqIdx = 0xb; // 44.1KHz  
	int chanCfg = ctx->m_channels; // CPE  

	if (96000 == ctx->m_sampleRate)
	{
		freqIdx = 0x00;
	}
	else if (88200 == ctx->m_sampleRate)
	{
		freqIdx = 0x01;
	}
	else if (64000 == ctx->m_sampleRate)
	{
		freqIdx = 0x02;
	}
	else if (48000 == ctx->m_sampleRate)
	{
		freqIdx = 0x03;
	}
	else if (44100 == ctx->m_sampleRate)
	{
		freqIdx = 0x04;
	}
	else if (32000 == ctx->m_sampleRate)
	{
		freqIdx = 0x05;
	}
	else if (24000 == ctx->m_sampleRate)
	{
		freqIdx = 0x06;
	}
	else if (22050 == ctx->m_sampleRate)
	{
		freqIdx = 0x07;
	}
	else if (16000 == ctx->m_sampleRate)
	{
		freqIdx = 0x08;
	}
	else if (12000 == ctx->m_sampleRate)
	{
		freqIdx = 0x09;
	}
	else if (11025 == ctx->m_sampleRate)
	{
		freqIdx = 0x0a;
	}
	else if (8000 == ctx->m_sampleRate)
	{
		freqIdx = 0x0b;
	}
	else if (7350 == ctx->m_sampleRate)
	{
		freqIdx = 0xc;
	}
	// fill in ADTS data  
	pData[0] = 0xFF;
	pData[1] = 0xF1;
	pData[2] = ((profile) << 6) + (freqIdx << 2) + (chanCfg >> 2);
	pData[3] = (((chanCfg & 3) << 6) + (packLen >> 11));
	pData[4] = ((packLen & 0x7FF) >> 3);
	pData[5] = (((packLen & 7) << 5) + 0x1F);
	pData[6] = 0xFC;
}