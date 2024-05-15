
#ifdef __cplusplus
extern "C" {
#endif

#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/timestamp.h>
#include <libavcodec/avcodec.h>

#ifdef __cplusplus
}
#endif

#include "CutFrame.h"
#include "yuv_convert_to_jpeg.h"

static cut_ctx_s  g_cut_ctx;

static void ffmpeg_cut_frame_reset(cut_ctx_s* cut_ctx)
{
    cut_ctx->fmt_ctx = NULL;
    cut_ctx->video_dec_ctx = NULL;

    cut_ctx->width = 0;
    cut_ctx->height = 0;
    cut_ctx->dest_xpos = 0;
    cut_ctx->dest_ypos = 0;
    cut_ctx->dest_width = 0;
    cut_ctx->dest_height = 0;

    cut_ctx->video_stream = NULL;

    memset(cut_ctx->video_dst_data,0,sizeof(uint8_t*)*4);
    memset(cut_ctx->video_dst_linesize,0,sizeof(uint8_t*)*4);
    cut_ctx->video_dst_bufsize = 0;

    cut_ctx->video_stream_idx = -1;

    cut_ctx->frame = NULL;

    cut_ctx->video_frame_count = 0;

    cut_ctx->refcount = 0;
}

static int decode_packet(cut_ctx_s *cut_ctx,int *got_frame, int cached)
{
    int ret = 0;
    int decoded = cut_ctx->pkt.size;

    *got_frame = 0;

    if (cut_ctx->pkt.stream_index == cut_ctx->video_stream_idx) {
        /* decode video frame */
        ret = avcodec_decode_video2(cut_ctx->video_dec_ctx, cut_ctx->frame, got_frame, &cut_ctx->pkt);
        if (ret < 0) {
#ifndef SECRET
            printf("decode_packet Error decoding video frame (%s)", av_err2str(ret));
#endif
            return ret;
        }

        if (*got_frame) {

            if (cut_ctx->frame->width != cut_ctx->width || cut_ctx->frame->height != cut_ctx->height || cut_ctx->frame->format != cut_ctx->pix_fmt) {

#ifndef SECRET
				printf("width:%d height:%d,pix_fmt:%d",cut_ctx->frame->width,cut_ctx->frame->height,cut_ctx->frame->format);
#endif
				av_freep(cut_ctx->video_dst_data);

                cut_ctx->width = cut_ctx->frame->width;
                cut_ctx->height = cut_ctx->frame->height;
                cut_ctx->pix_fmt = cut_ctx->frame->format;
				if (cut_ctx->dest_width > cut_ctx->width || cut_ctx->dest_width <= 0)
                    cut_ctx->dest_width = cut_ctx->width;
				if (cut_ctx->dest_height > cut_ctx->height || cut_ctx->dest_height <= 0)
                    cut_ctx->dest_height = cut_ctx->height;
				ret = av_image_alloc(cut_ctx->video_dst_data, cut_ctx->video_dst_linesize,
                                     cut_ctx->width, cut_ctx->height, cut_ctx->pix_fmt, 1);
				if (ret < 0) {
#ifndef SECRET
					printf("ffmpeg_cut_frame Could not allocate raw video buffer,resolution:[%d*%d]", cut_ctx->width, cut_ctx->height);
#endif
					return ret;
				}
                cut_ctx->video_dst_bufsize = ret;
            }

#ifndef SECRET
            printf("video_frame%s n:%d coded_n:%d, pkt->pts:%"PRId64", pkt->flags:%d",
                   cached ? "(cached)" : "",
                   cut_ctx->video_frame_count++, cut_ctx->frame->coded_picture_number, cut_ctx->pkt.pts, cut_ctx->pkt.flags);
#endif

            /* copy decoded frame to destination buffer:
             * this is required since rawvideo expects non aligned data */
            av_image_copy(cut_ctx->video_dst_data, cut_ctx->video_dst_linesize,
                          (const uint8_t **)(cut_ctx->frame->data), cut_ctx->frame->linesize,
                          cut_ctx->pix_fmt, cut_ctx->width, cut_ctx->height);
        }
    }

    /* If we use frame reference counting, we own the data and need
     * to de-reference it when we don't use it anymore */
    if (*got_frame && cut_ctx->refcount)
        av_frame_unref(cut_ctx->frame);

    return decoded;
}

static int open_codec_context(cut_ctx_s *cut_ctx, enum AVMediaType type)
{
    int ret;
    AVStream *st;
    AVCodec *dec = NULL;
    AVDictionary *opts = NULL;

    st = cut_ctx->fmt_ctx->streams[cut_ctx->video_stream_idx];

	/* find decoder for the stream */
	dec = avcodec_find_decoder(st->codecpar->codec_id);
	if (!dec) {
#ifndef SECRET
		printf("open_codec_context Failed to find %s codec,codec_id:%d",
			av_get_media_type_string(type), st->codecpar->codec_id);
#endif
		return AVERROR(EINVAL);
	}

	//为解码器分配编码上下文
    cut_ctx->video_dec_ctx = avcodec_alloc_context3(dec);
    if (!cut_ctx->video_dec_ctx) {
#ifndef SECRET
		printf("open_codec_context Failed to allocate the %s codec context",
			av_get_media_type_string(type));
#endif
		return AVERROR(ENOMEM);
	}

	/* Copy codec parameters from input stream to output codec context */
    if ((ret = avcodec_parameters_to_context(cut_ctx->video_dec_ctx, st->codecpar)) < 0) {
#ifndef SECRET
		printf("open_codec_context Failed to copy %s codec parameters to decoder context",
			av_get_media_type_string(type));
#endif
		return ret;
	}

	/* Init the decoders, with or without reference counting */
  //  av_dict_set(&opts, "refcounted_frames", cut_ctx->refcount ? "1" : "0", 0);
    if ((ret = avcodec_open2(cut_ctx->video_dec_ctx, dec, &opts)) < 0) {
#ifndef SECRET
		printf("open_codec_context Failed to open %s codec",
			av_get_media_type_string(type));
#endif
		return ret;
	}

    return 0;
}

int ffmpeg_seek_to_time(cut_ctx_s *cut_ctx, int timeus){
    int64_t timestamp;

    int video_time_base = cut_ctx->fmt_ctx->streams[cut_ctx->video_stream_idx]->time_base.den/cut_ctx->fmt_ctx->streams[cut_ctx->video_stream_idx]->time_base.num;
    timestamp = (timeus * video_time_base)/1000000;
    if(avformat_seek_file(cut_ctx->fmt_ctx, cut_ctx->video_stream_idx, timestamp, timestamp, INT64_MAX, 0) < 0){
#ifndef SECRET
		printf("ffmpeg_seek_to_time seek video failed");
#endif
        return -1;
    }

    return 0;
}

static int64_t pts_to_us(cut_ctx_s *cut_ctx,int64_t pts){
    if(!cut_ctx->fmt_ctx)
        return -1;
    if(cut_ctx->video_stream_idx < 0)
        return -1;

    return av_rescale_q(pts,cut_ctx->fmt_ctx->streams[cut_ctx->video_stream_idx]->time_base, AV_TIME_BASE_Q);
}

static int ffmpeg_goto_cut_keyframe(cut_ctx_s *cut_ctx,int64_t first_packet_time_us, int64_t cut_time_us) {
    int ret = 0;
    int count = 1;
    int64_t seek_time_us = cut_time_us;

#ifndef SECRET
	if(first_packet_time_us > 0)
		printf("first_packet_time_us:%lld, cut_time_us:%lld", first_packet_time_us, cut_time_us);
#endif

seek:
    if (seek_time_us < 0)
        seek_time_us = 0;

    int64_t target = av_rescale_q(seek_time_us + first_packet_time_us, AV_TIME_BASE_Q,
                                  cut_ctx->fmt_ctx->streams[cut_ctx->video_stream_idx]->time_base);
    if (av_seek_frame(cut_ctx->fmt_ctx, cut_ctx->video_stream_idx, target, AVSEEK_FLAG_BACKWARD) < 0) {
		printf("seek frame fail,let try");
#ifndef SECRET
		printf("call av_seek_frame(fmt_ctx, video_stream_idx, target, AVSEEK_FLAG_BACKWARD) Failed");
#endif
        if (av_seek_frame(cut_ctx->fmt_ctx, cut_ctx->video_stream_idx, target, AVSEEK_FLAG_ANY) < 0) {
#ifndef SECRET
			printf("ffmpeg_goto_cut_keyframe seek video failed");
#endif
            return -1;
        }
    }

    //如果从文件头开始，可以直接返回
    if(0 == seek_time_us) {
#ifndef SECRET
		printf("first start");
#endif
        return 1;
    }

    int64_t frame_time_us = 0;
    //尝试向前搜索
    do {
        if ((ret = av_read_frame(cut_ctx->fmt_ctx, &cut_ctx->pkt)) >= 0) {
            if (cut_ctx->pkt.stream_index != cut_ctx->video_stream_idx) {
                av_packet_unref(&cut_ctx->pkt);
                continue;
            }

            frame_time_us = pts_to_us(cut_ctx,cut_ctx->pkt.pts);
            if (cut_ctx->pkt.flags & AV_PKT_FLAG_KEY){
#ifndef SECRET
                printf("find key frame,pts = %" PRId64", frame_time_us = %" PRId64", cut_time_us = %" PRId64", first_packet_time_us = %" PRId64"",
                     cut_ctx->pkt.pts, frame_time_us, cut_time_us, first_packet_time_us);
#endif
                goto end;
            }

            if(frame_time_us > (cut_time_us + first_packet_time_us + 500000)){
#ifndef SECRET
                printf("Current_frame_time_us = %" PRId64", target_seek_time_us:%" PRId64", current_frame_pts = %" PRId64", target_seek_pts:%" PRId64", Not find, seek again!!!",
                        frame_time_us, cut_time_us + first_packet_time_us, cut_ctx->pkt.pts, target);
#endif
                //搜索时间提前800毫秒
                seek_time_us -= 800000;
                count++;
                av_packet_unref(&cut_ctx->pkt);
                goto seek;
            }

            av_packet_unref(&cut_ctx->pkt);
            if(count > 10){
                //向前10秒依然找不到关键字，则马上返回
                seek_time_us = 0;
                goto seek;
            }
        }  else if (ret == AVERROR_EOF) { //读到文件尾了， 就给最后一帧
#ifndef SECRET
            printf("Read EOF, the end frame_time_us = %" PRId64", cut_time_us(%" PRId64") + first_packet_time_us(%" PRId64") = target_cut_time_us:%" PRId64,
                 frame_time_us, cut_time_us, first_packet_time_us, cut_time_us + first_packet_time_us);
#endif
            //搜索时间提前800毫秒
            seek_time_us -= 800000;
            count++;
            goto seek;
        } else {
            return -1;
        }
    } while (1);

end:
    return ret;
}

static void yuv_buf_to_frame(AVFrame *frmae, int w, int h, const unsigned char *ptr)
{
    int ysize, usize;
    ysize = w * h;
    usize = ( (w + 1) >> 1 ) * ( (h + 1) >> 1 );
    frmae->width = w;
    frmae->height = h;
    frmae->data[0] = (unsigned char*)ptr;
    frmae->data[1] = frmae->data[0] + ysize;
    frmae->data[2] =frmae->data[1] + usize;
    frmae->data[3] = 0;
    frmae->linesize[0] = w;
    frmae->linesize[1] = w / 2;
    frmae->linesize[2] = frmae->linesize[1];
    frmae->linesize[3] = 0;
}

//align 字节对其 宽与高必须为偶数 除32=0
static void yuv420p_crop(AVFrame *srcFrame, int pointX, int pointY,
                   AVFrame *dscFrame, int align) {
    //ffmpeg -s 440x280 -i aaa-crop.yuv aa.jpg
    //YUV 4:2:0采样，每四个Y共用一组UV分量。
    //Y       width x heigth
    //U分量   (w/2)x(h/2)
    //V分量   (w/2)x(h/2)
    int w = dscFrame->width;
    int h = dscFrame->height;
	uint8_t *ptr = dscFrame->data[0];
	int y = 0;
    for (y = pointY; y < h + pointY; y++) {
        memcpy(ptr, srcFrame->data[0] + y * srcFrame->linesize[0] + pointX, w);
        ptr += w;
    }

    //u
    void *ptrStart = NULL;
    int linesize = w / 2;
    int uy = pointY / 2;
    int u=0;
    int v=0;
    //u 必须为align的倍数
    ptr = dscFrame->data[1];
    for (u = 0; u < h / 2; u++) {
        ptrStart = srcFrame->data[1] + (u + uy) * srcFrame->linesize[1]
                   + pointX / 2;
        memcpy(ptr, ptrStart, linesize);

        ptr += dscFrame->linesize[1];
    }
    //v 必须为align的倍数
    ptr = dscFrame->data[2];
    for (v = 0; v < h / 2; v++) {
        ptrStart = srcFrame->data[2] + (v + uy) * srcFrame->linesize[2]
                   + pointX / 2;
        memcpy(ptr, ptrStart, linesize);

        ptr += dscFrame->linesize[2];
    }
}

static int get_next_frame(cut_ctx_s *cut_ctx){
    int ret = 0;
    do{
        if ((ret = av_read_frame(cut_ctx->fmt_ctx, &cut_ctx->pkt)) < 0)
            break;

        if (cut_ctx->pkt.stream_index != cut_ctx->video_stream_idx) {
//            LOGW("非视频帧，丢弃数据");
            av_packet_unref(&cut_ctx->pkt);
            continue;
        }

        return 0;
    }while(1);

    return ret;
}

int save_as_jpg(cut_ctx_s *cut_ctx,char* video_dst_filename, int quality){
    if(cut_ctx->frame && cut_ctx->frame->width > 0 && cut_ctx->frame->height > 0) {
        AVFrame *cropped_frame = 0;
        if(  cut_ctx->dest_width != cut_ctx->frame->width || cut_ctx->dest_height != cut_ctx->frame->height ){
            //需要裁剪
            cropped_frame = av_frame_alloc();
            cropped_frame->format = cut_ctx->frame->format;
            cropped_frame->width = cut_ctx->dest_width;
            cropped_frame->height = cut_ctx->dest_height;

            // Determine required buffer size and allocate buffer
            int numBytes = av_image_get_buffer_size((enum AVPixelFormat)cropped_frame->format,
                                                    cropped_frame->width,
                                                    cropped_frame->height,
                                                    1
            );

            uint8_t* buffer = (uint8_t *)av_malloc(numBytes);
            av_image_fill_arrays(cropped_frame->data,
                                 cropped_frame->linesize,
                                 buffer,
                                 (enum AVPixelFormat)cropped_frame->format,
                                 cropped_frame->width,
                                 cropped_frame->height,
                                 1
            );

            yuv420p_crop(cut_ctx->frame,cut_ctx->dest_xpos,cut_ctx->dest_ypos,cropped_frame,0);
            return save_yuv_data_to_jpg_from_yuvframe(cropped_frame, cropped_frame->width, cropped_frame->height,
                                               video_dst_filename, quality); //

            av_free(cropped_frame->data[0]);
            av_frame_free(&cropped_frame);
        }else
            return save_yuv_data_to_jpg_from_yuvframe(cut_ctx->frame, cut_ctx->frame->width, cut_ctx->frame->height,
                                               video_dst_filename, quality);
    }
	return 0;
}

// 调整抽帧时间， 抽帧时间需要从文件的开始时间计算
int64_t get_first_packet_time_us(cut_ctx_s *cut_ctx,AVPacket* first_packet){
    AVRational dst_tb = {1, 1000000}; //转换成微秒时间单位
    av_packet_rescale_ts(first_packet, cut_ctx->fmt_ctx->streams[first_packet->stream_index]->time_base, dst_tb);
    return first_packet->pts;
}

//获得的帧的pts >= cut_time_us
static int ffmpeg_cut_one_frame(cut_ctx_s *cut_ctx, int64_t first_video_packet_us, int64_t cut_time_us, int quality, char* video_dst_filename) {

    int ret = 0, got_frame = 0;

    //定位到抽帧位置
    ret = ffmpeg_goto_cut_keyframe(cut_ctx, first_video_packet_us, cut_time_us);
    if (ret < 0) {
        printf("ffmpeg_cut_one_frame seek key frame failed");
		av_packet_unref(&cut_ctx->pkt); //释放暂存的第一帧
        return -1;
    } else if (ret == 1) { // 从第一帧开始
//        memcpy(&cut_ctx->pkt, first_video_packet, sizeof(AVPacket));
    } else {
//        av_packet_unref(&cut_ctx->pkt); //释放暂存的第一帧
    }

    do{
        int64_t frame_time_us = 0;
        AVRational dst_tb = {1, 1000000}; //转换成微秒时间单位
        av_packet_rescale_ts(&cut_ctx->pkt, cut_ctx->fmt_ctx->streams[cut_ctx->video_stream_idx]->time_base, dst_tb);
        frame_time_us = cut_ctx->pkt.pts;
        AVPacket orig_pkt = cut_ctx->pkt;
        do {
            ret = decode_packet(cut_ctx,&got_frame, 0);
            if (ret < 0)
                break;
            cut_ctx->pkt.data += ret;
            cut_ctx->pkt.size -= ret;
        } while (cut_ctx->pkt.size > 0);
        av_packet_unref(&orig_pkt);

        cut_ctx->pkt.data = NULL;
        cut_ctx->pkt.size = 0;
        if(0 == got_frame) {
            /* flush cached frames */
            decode_packet(cut_ctx,&got_frame, 1);
        }

        int64_t scope_time = cut_time_us + first_video_packet_us - 100000;
        if ( scope_time < 0 )
            scope_time = cut_time_us + first_video_packet_us;

        if (got_frame && frame_time_us >= scope_time)
        {
		SAVA_AS_JPG:
            ret = save_as_jpg(cut_ctx,video_dst_filename,quality);
#ifndef SECRET
            printf("ffmpeg_cut_one_frame frame_time_us:%" PRId64"(ms)  cut_time_us:%" PRId64"(ms)  first_packet_time_us:%" PRId64"(ms), save as jpg return %d",
                 frame_time_us / 1000, cut_time_us / 1000, first_video_packet_us / 1000, ret);
#endif
            return ret;
        }

        if ((ret = get_next_frame(cut_ctx)) < 0) {
            if (ret == AVERROR_EOF) {
                goto SAVA_AS_JPG;
            }
            break;
        }
    }while(1);

    return -1;
}

int convert_percent_to_ffmepg(int value)
{
    if ( value < 0 || value > 100 )
    {
        return 2;
    }
    //value是oms配置的百分比
    return 2 + (29 * value / 100.0);
}

int64_t get_first_video_packet_us(cut_ctx_s* cut_ctx)
{
    uint8_t is_adjust = 0;// 是否调整了抽帧时间
    int64_t first_packet_time_us = 0;
    do{
        if (av_read_frame(cut_ctx->fmt_ctx, &cut_ctx->pkt) < 0)
            break;

        if (cut_ctx->pkt.stream_index != cut_ctx->video_stream_idx) {
            if (is_adjust == 0) {
                first_packet_time_us = get_first_packet_time_us(cut_ctx,&cut_ctx->pkt);
                is_adjust = 1;
            }

            av_packet_unref(&cut_ctx->pkt);
            continue;
        }else{
            if (is_adjust == 0) {
                first_packet_time_us = get_first_packet_time_us(cut_ctx,&cut_ctx->pkt);
                is_adjust = 1;
            }
            break;
        }
    }while(1);
    av_packet_unref(&cut_ctx->pkt);
    return first_packet_time_us;
}

int ffmpeg_cut_frame( int x,int y,int w,int h,int64_t cut_time_us,const char *src_file,
			const char *dest_file, int quality){
    //检查参数
    if(w < 0) {
        printf("ffmpeg_cut_frame w:%d",w);
        return -1;
    }
    if(h < 0) {
#ifndef SECRET
        printf("ffmpeg_cut_frame h:%d",h);
#endif
        return -1;
    }

    if(x < 0 || x > w) {
#ifndef SECRET
        printf("ffmpeg_cut_frame x:%d,w:%d",x,w);
#endif
        x = 0;
    }
    if(y < 0 || y > h) {
#ifndef SECRET
        printf("ffmpeg_cut_frame y:%d,h:%d",y,h);
#endif
        y = 0;
    }
    if( !src_file || strlen(src_file) == 0){
#ifndef SECRET
        printf("ffmpeg_cut_frame media_file:%p,%s",src_file,src_file);
#endif
        return -1;
    }
    if( !dest_file || strlen(dest_file) == 0){
#ifndef SECRET
        printf("ffmpeg_cut_frame dest_file:%p,%s",dest_file,dest_file);
#endif
        return -1;
    }

    if( cut_time_us < 0){
	#ifndef SECRET
        printf("ffmpeg_cut_frame cut_time:%" PRId64,cut_time_us);
#endif
        return -1;
    }

    cut_ctx_s *cut_ctx = &g_cut_ctx;
    //初始化packet,设置data为NULL
    av_init_packet(&cut_ctx->pkt);
    cut_ctx->pkt.data = NULL;
    cut_ctx->pkt.size = 0;

    //参数赋值
    ffmpeg_cut_frame_reset(cut_ctx);
    cut_ctx->dest_xpos = x;
    cut_ctx->dest_ypos = y;
    cut_ctx->dest_width = w;
    cut_ctx->dest_height = h;

    //解复用和解码
    int ret = 0;

	int count = 0;
	int is_open_file = 0;
	while (count < 4)
	{
		int base_multi = pow(2, count++);

		cut_ctx->fmt_ctx = avformat_alloc_context();
		cut_ctx->fmt_ctx->flags = (cut_ctx->fmt_ctx->flags | AVFMT_FLAG_NOBUFFER);

		AVDictionary * avdic = NULL;
		cut_ctx->fmt_ctx->probesize = 128 * 1024 * base_multi;

		//打开输入文件并且分配格式上下文
		if (avformat_open_input(&cut_ctx->fmt_ctx, src_file, NULL, &avdic) < 0) {
#ifndef SECRET
			printf("ffmpeg_cut_frame Could not open source file %s", src_file);
#endif
			avformat_close_input(&cut_ctx->fmt_ctx);
			return -1;
		}

		//获得流信息
		AVDictionary * avdic1 = NULL;
		cut_ctx->fmt_ctx->max_analyze_duration = 0.1 * AV_TIME_BASE * base_multi;

		if (avformat_find_stream_info(cut_ctx->fmt_ctx, &avdic1) < 0) {
#ifndef SECRET
			printf("ffmpeg_cut_frame Could not find stream information");
#endif
			avformat_close_input(&cut_ctx->fmt_ctx);
			continue;
		}
		avformat_flush(cut_ctx->fmt_ctx);

		//根据结构化数据来找视频流，需要确认avformat_find_stream_info的结果，但是avformat_find_stream_info
		//比较耗时和耗内存，决定增量处理即可
		ret = av_find_best_stream(cut_ctx->fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
		if (ret < 0) {
#ifndef SECRET
			printf("open_codec_context Could not find %s stream in input file '%s'",
				av_get_media_type_string(AVMEDIA_TYPE_VIDEO), src_file);
#endif
			avformat_close_input(&cut_ctx->fmt_ctx);
			continue;
		}
		cut_ctx->video_stream_idx = ret;
		is_open_file = 1;
		break;
	}

	if ( !is_open_file )
	{
		ret = -1;
        goto end;
	}

    //打开解码上下文
    if (open_codec_context(cut_ctx, AVMEDIA_TYPE_VIDEO) < 0) {
      
		printf("open_codec_context failed");
		ret = -1;
		goto end;
    }

    cut_ctx->video_stream = cut_ctx->fmt_ctx->streams[cut_ctx->video_stream_idx];
    if (!cut_ctx->video_stream) {
#ifndef SECRET
        printf("ffmpeg_cut_frame Could not find audio or video stream in the input, aborting");
#endif
        ret = -1;
        goto end;
    }

    cut_ctx->frame = av_frame_alloc();
    if (!cut_ctx->frame) {
#ifndef SECRET
        printf("ffmpeg_cut_frame Could not allocate frame");
#endif
        ret = -1;
        goto end;
    }

    int64_t video_start_time = get_first_video_packet_us(cut_ctx);

    //开始抽帧
    if( (ret = ffmpeg_cut_one_frame(cut_ctx,video_start_time,cut_time_us, quality,dest_file)) < 0){
#ifndef SECRET
		printf("ffmpeg_cut_frame cut frame failed!");
#endif
    }

end:
    avcodec_free_context(&cut_ctx->video_dec_ctx);
    avformat_close_input(&cut_ctx->fmt_ctx);
    av_frame_free(&cut_ctx->frame);
    av_freep(cut_ctx->video_dst_data);

    return ret;
}

int ffmpeg_cut_frame_batch(int x, int y, int w, int h,
						   cut_info* cut_array,
						   int cut_array_size,
						   const char *src_media_file,
						   int compressibility)
{
	//检查参数
	if (w < 0 || h < 0) {
#ifndef SECRET
		printf("ffmpeg_cut_frame w:%d,h:%d", w, h);
#endif
		return -1;
	}

	if (x < 0 || x > w) {
#ifndef SECRET
		printf("cut x:%d w:%d", x,w);
#endif
		x = 0;
	}
	if (y < 0 || y > h) {
#ifndef SECRET
		printf("cut y:%d h:%d", y,h);
#endif
		y = 0;
	}
	if (!src_media_file || strlen(src_media_file) == 0) {
#ifndef SECRET
		printf("cut media_file:%s", src_media_file);
#endif
		return -1;
	}

	if (cut_array == NULL || cut_array_size <= 0) {
#ifndef SECRET
		printf("cut_time_array:%p,array size:%d", cut_array, cut_array_size);
#endif
		return -1;
	}

	cut_ctx_s cut_ctx;
	ffmpeg_cut_frame_reset(&cut_ctx);

	//参数赋值
    cut_ctx.dest_xpos = x;
    cut_ctx.dest_ypos = y;
    cut_ctx.dest_width = w;
    cut_ctx.dest_height = h;

	//解复用和解码
	int ret = 0;

	int count = 0;
	int is_open_file = 0;

	while (count < 4)
	{
		int base_multi = pow(2, count++);

		cut_ctx.fmt_ctx = avformat_alloc_context();
		cut_ctx.fmt_ctx->flags = (cut_ctx.fmt_ctx->flags | AVFMT_FLAG_NOBUFFER);

		AVDictionary * avdic = NULL;
		cut_ctx.fmt_ctx->probesize = 128 * 1024 * base_multi;

		//打开输入文件并且分配格式上下文
		if (avformat_open_input(&cut_ctx.fmt_ctx, src_media_file, NULL, &avdic) < 0) {
#ifndef SECRET
			printf("ffmpeg_cut_frame Could not open source file %s", src_media_file);
#endif
			avformat_close_input(&cut_ctx.fmt_ctx);
			return -1;
		}

		//获得流信息
		AVDictionary * avdic1 = NULL;
		cut_ctx.fmt_ctx->max_analyze_duration = 0.1 * AV_TIME_BASE * base_multi;

		if (avformat_find_stream_info(cut_ctx.fmt_ctx, NULL) < 0) {
#ifndef SECRET
			printf("ffmpeg_cut_frame Could not find stream information");
#endif
			avformat_close_input(&cut_ctx.fmt_ctx);
			continue;
		}
		avformat_flush(cut_ctx.fmt_ctx);

		//根据结构化数据来找视频流，需要确认avformat_find_stream_info的结果，但是avformat_find_stream_info
		//比较耗时和耗内存，决定增量处理即可
		ret = av_find_best_stream(cut_ctx.fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
		if (ret < 0) {
#ifndef SECRET
			printf("open_codec_context Could not find %s stream in input file '%s'",
				av_get_media_type_string(AVMEDIA_TYPE_VIDEO), src_media_file);
#endif
			avformat_close_input(&cut_ctx.fmt_ctx);
			continue;
		}
		cut_ctx.video_stream_idx = ret;
		is_open_file = 1;
		break;
	}

	if (!is_open_file)
	{
		ret = -1;
		goto end;
	}

	//打开解码上下文
	if (open_codec_context(&cut_ctx, AVMEDIA_TYPE_VIDEO) < 0) {

		printf("open_codec_context failed");
		ret = -1;
		goto end;
	}

    cut_ctx.video_stream = cut_ctx.fmt_ctx->streams[cut_ctx.video_stream_idx];
	if (!cut_ctx.video_stream) {
#ifndef SECRET
		printf("ffmpeg_cut_frame Could not find audio or video stream in the input, aborting");
#endif
		ret = -1;
		goto end;
	}

    cut_ctx.frame = av_frame_alloc();
	if (!cut_ctx.frame) {
#ifndef SECRET
		printf("ffmpeg_cut_frame Could not allocate frame");
#endif
		ret = -1;
		goto end;
	}
	int got_picture = 0;
//	AVPacket *packet = av_packet_alloc();
//	while (1)
//	{
//		av_seek_frame(cut_ctx.fmt_ctx, 0, 360, AVSEEK_FLAG_BACKWARD);
//		av_read_frame(cut_ctx.fmt_ctx, packet);
//		if (packet->stream_index == 0 && packet->flags == 1)
//		ret = avcodec_decode_video2(cut_ctx.video_dec_ctx, cut_ctx.frame, &got_picture, packet);
//	}
	
	int64_t first_video_packet_us = get_first_video_packet_us(&cut_ctx);

	char out_file_path[256] = {0};
	int i = 0;
	for ( ; i < cut_array_size; i++ )
	{
		if (cut_array[i].cut_time_point >= 0)
		{
			strcpy(out_file_path, cut_array[i].cut_file_name);
			//snprintf(out_file_path,sizeof(out_file_path), "%s",dest_path, cut_array[i].cut_file_name);

			//初始化packet,设置data为NULL
			av_init_packet(&cut_ctx.pkt);

			//开始抽帧
			if ((ret = ffmpeg_cut_one_frame(&cut_ctx, first_video_packet_us, cut_array[i].cut_time_point *1000, compressibility, out_file_path)) < 0)
			{
#ifndef SECRET
				printf("cut frame time:%" PRId64" failed,path:%s", cut_array[i].cut_time_point,out_file_path);
#endif
				cut_array[i].cut_result = 0;
			}
			else
			{
#ifndef SECRET
				printf("cut frame time:%" PRId64" ok,path:%s", cut_array[i].cut_time_point, out_file_path);
#endif
				cut_array[i].cut_result = ret;
			}
		}
	}

end:
	avcodec_free_context(&cut_ctx.video_dec_ctx);
	avformat_close_input(&cut_ctx.fmt_ctx);
	av_frame_free(&cut_ctx.frame);
	av_freep(cut_ctx.video_dst_data);

	return 0;
}
