#include "yuv_convert_to_jpeg.h"


#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libswresample/swresample.h>

int save_yuv_data_to_jpg_from_yuvframe(AVFrame *yuvframe, const int width, const int height, const char *filename,int quality) {
    const char*              fileext = NULL;
    enum AVCodecID     codecid = AV_CODEC_ID_NONE;
    struct SwsContext* sws_ctx = NULL;
    enum AVPixelFormat      swsofmt = AV_PIX_FMT_NONE;
    AVFrame            *picture = av_frame_alloc();
    int                ret     = -1;
    AVFormatContext*   fmt_ctxt   = NULL;
    AVOutputFormat*    out_fmt    = NULL;
    AVStream*          stream     = NULL;
    AVCodecContext*    codec_ctxt = NULL;
    AVCodec*           codec      = NULL;
    AVPacket           *packet  = av_packet_alloc() ;
    int                retry      = 8;
    int                got        = 0;
    // init ffmpeg
    av_register_all();
    fileext = filename + strlen(filename) - 3;
    if (strcmp(fileext, "png") == 0)
    {
        codecid = AV_CODEC_ID_APNG;
        swsofmt = AV_PIX_FMT_RGB24;
    }
    else
    {
        codecid = AV_CODEC_ID_MJPEG;
        swsofmt = AV_PIX_FMT_YUVJ420P;
    }
    // alloc picture
    picture->format = swsofmt;
    picture->width  = width > 0 ? width : yuvframe->width;
    picture->height = height > 0 ? height : yuvframe->height;
    if (av_frame_get_buffer(picture, 32) < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "failed to allocate picture !\n", filename);
        goto done;
    }
    // scale picture
    sws_ctx = sws_getContext(yuvframe->width, yuvframe->height, (enum AVPixelFormat)yuvframe->format,
                             picture->width, picture->height, swsofmt, SWS_FAST_BILINEAR, NULL, NULL, NULL);
    if (!sws_ctx)
    {
        av_log(NULL, AV_LOG_ERROR, "could not initialize the conversion context jpg\n");
        goto done;
    }
    sws_scale(sws_ctx, yuvframe->data, yuvframe->linesize, 0, yuvframe->height, picture->data, picture->linesize);
    // do encoding
    fmt_ctxt = avformat_alloc_context();
    out_fmt  = av_guess_format(codecid == AV_CODEC_ID_APNG ? "apng" : "mjpeg", NULL, NULL);
    fmt_ctxt->oformat = out_fmt;
    if (!out_fmt)
    {
        av_log(NULL, AV_LOG_ERROR, "failed to guess format !\n");
        goto done;
    }
    if (avio_open(&fmt_ctxt->pb, filename, AVIO_FLAG_READ_WRITE) < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "failed to open output file: %s !\n", filename);
        goto done;
    }
    stream = avformat_new_stream(fmt_ctxt, 0);
    if (!stream)
    {
        av_log(NULL, AV_LOG_ERROR, "failed to create a new stream !\n");
        goto done;
    }
    codec_ctxt                = stream->codec;
    codec_ctxt->codec_id      = out_fmt->video_codec;
    codec_ctxt->codec_type    = AVMEDIA_TYPE_VIDEO;
    codec_ctxt->pix_fmt       = swsofmt;
    codec_ctxt->width         = picture->width;
    codec_ctxt->height        = picture->height;
    codec_ctxt->time_base.num = 1;
    codec_ctxt->time_base.den = 25;
    codec_ctxt->qcompress = (float)quality/100.f;
    codec_ctxt->qmin = 2;
    codec_ctxt->qmax = 31;

    codec = avcodec_find_encoder(codec_ctxt->codec_id);
    if (!codec)
    {
        av_log(NULL, AV_LOG_ERROR, "failed to find encoder !\n");
        goto done;
    }
    if (avcodec_open2(codec_ctxt, codec, NULL) < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "failed to open encoder !\n");
        goto done;
    }

    while (retry-- && !got)
    {
        if (avcodec_encode_video2(codec_ctxt, packet, picture, &got) < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "failed to do picture encoding !\n");
            goto done;
        }
        if (got)
        {
            ret = avformat_write_header(fmt_ctxt, NULL);
            if (ret < 0)
            {
                av_log(NULL, AV_LOG_ERROR, "error occurred when opening output file !\n");
                goto done;
            }
          //  av_write_frame(fmt_ctxt, packet);
			ret = av_interleaved_write_frame(fmt_ctxt, packet);
            av_write_trailer(fmt_ctxt);
        }
    }
    // ok
    ret = 0;
    done:
	avcodec_close(codec_ctxt);
	avformat_close_input(&fmt_ctxt);
	if (fmt_ctxt != NULL)
		avio_close(fmt_ctxt->pb);
	avformat_free_context(fmt_ctxt);
	av_packet_unref(packet);
	av_frame_unref(picture);
	av_frame_free(&picture);
	av_packet_free(&packet);
	sws_freeContext(sws_ctx);
    return ret;
}


int ffmpeg_convert_yuv_to_jpeg(unsigned char *yuv,int srcwidth,int srcheight,int realwidth,int realheight, const char *filename,int quality){
    if(!yuv)
        return -1;

    if(!filename)
        return -1;

    AVFrame *enc_frame = NULL;
    unsigned char *enc_buf = NULL;
    int flag = 0;

    do
    {
        int  enc_width = srcwidth;
        int  enc_height = srcheight;
        enc_frame = av_frame_alloc();
        if (NULL == enc_frame){
            break;
        }
        unsigned int enc_buf_size = avpicture_get_size(AV_PIX_FMT_YUV420P, enc_width, enc_height);
        enc_buf = (unsigned char *)av_malloc(enc_buf_size);
        if (NULL == enc_buf){
            break;
        }
        avpicture_fill(
                (AVPicture *)enc_frame,
                enc_buf,
                AV_PIX_FMT_YUV420P,
                enc_width,
                enc_height
        );
        memset(enc_buf, 0, sizeof(char)* enc_buf_size);
        unsigned int src_y_size = enc_width * enc_height;
        enc_frame->data[0] = enc_buf;
        enc_frame->data[1] = enc_buf + src_y_size;  // U
        enc_frame->data[2] = enc_buf + src_y_size * 5 / 4; // V
        enc_frame->width = enc_width;
        enc_frame->height = enc_height;
        enc_frame->format = AV_PIX_FMT_YUV420P;
        memcpy(enc_buf, yuv, enc_buf_size);

        realwidth = srcwidth;
        realheight = srcheight;
        flag = save_yuv_data_to_jpg_from_yuvframe(enc_frame, realwidth, realheight, filename,quality);

    } while (0);

    if (NULL != enc_frame){
        av_frame_free(&enc_frame);
    }
    if (NULL != enc_buf){
        av_free(enc_buf);
    }

    return flag;
}