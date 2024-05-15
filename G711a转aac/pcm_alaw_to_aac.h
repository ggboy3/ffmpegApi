#pragma once

#include "libavutil/avstring.h"
#include "libavutil/eval.h"
#include "libavutil/mathematics.h"
#include "libavutil/pixdesc.h"
#include "libavutil/imgutils.h"
#include "libavutil/dict.h"
#include "libavutil/parseutils.h"
#include "libavutil/samplefmt.h"
#include "libavutil/avassert.h"
#include "libavutil/time.h"
#include "libavformat/avformat.h"
#if CONFIG_AVDEVICE
#include "libavdevice/avdevice.h"
#endif
#include "libswscale/swscale.h"
#include "libavutil/opt.h"
#include "libavcodec/avfft.h"
#include "libswresample/swresample.h"

#if CONFIG_AVFILTER
#include "libavcodec/avcodec.h"
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"

#endif

#define PCM_BUFFER_SIZE		20480

typedef struct
{
    //pcm para
	int					m_sampleRate;
	enum AVSampleFormat		m_sampleFmt;
	int					m_channels;

	//g711 decoder
	enum AVCodecID			m_codecId;
	const AVCodec		*m_decoder;
	AVCodecContext		*m_deCtx;
	AVPacket			*m_dePkt;
	AVFrame				*m_deFrame;
	int				    m_initDecoder;
	int				    m_initDecoderOK;

	//aac encoder
	AVCodec				*m_encoder;
	AVCodecContext		*m_enCtx;
	AVPacket			*m_enPkt;
	AVFrame				*m_enFrame;
	int				    m_initEncoder;
	int				    m_initEncoderOK;
	SwrContext			*m_resampleCtx;
	int64_t				m_pts;
	uint8_t				*m_pcmPointer[8];

	//buffer
	uint8_t				*m_pcmBuffer;
	int					m_pcmSize;
}audio_ctx_s;

audio_ctx_s* ijk_create_audio_ctx(int sample_rate, int channels, enum AVCodecID codec_id);

void ijk_destroy_audio_ctx(audio_ctx_s* ctx);

int ijk_add_audio_data(audio_ctx_s* ctx, uint8_t *pData, int iSize);
int ijk_get_audio_data(audio_ctx_s* ctx,  uint8_t* pData, int *iSize);
	