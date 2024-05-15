#include "stdafx.h"
#include "H265Decoder.h"


H265Decoder::H265Decoder()
{
}


H265Decoder::~H265Decoder()
{
}
bool H265Decoder::Init()
{
	m_YuvBuf = new char[2560 * 1440 * 1.5];
	codec = avcodec_find_decoder(AV_CODEC_ID_H265);
	if (!codec) {
		fprintf(stderr, "Codec not found AV_CODEC_ID_H265\n");
		return false;
	}
	c = avcodec_alloc_context3(codec);
	if (!c) {
		fprintf(stderr, "Could not allocate audio codec context\n");
		return false;
	}
	/* open it */
	if (avcodec_open2(c, codec, NULL) < 0) {
		fprintf(stderr, "Could not open codec\n");
		return false;
	}
	packet = av_packet_alloc();
	if (NULL == packet)
	{
		return false;
	}
	frame = av_frame_alloc();
	if (NULL == frame)
	{
		return false;
	}
	return true;
}
void H265Decoder::AddData(char *pData, int iSize)
{
	int ret = 0;
	packet->size = iSize;
	packet->data = (uint8_t *)av_malloc(packet->size);
	memcpy(packet->data, pData, iSize);
	ret = av_packet_from_data(packet, packet->data, packet->size);
	if (ret <0)
	{
		fprintf(stderr, "av_packet_from_data error \n");
		av_free(packet->data);
		return;
	}
	ret = avcodec_send_packet(c, packet);
	av_packet_unref(packet);
	if (ret <0)
	{
		fprintf(stderr, "avcodec_send_packet error \n");
	}
}
bool H265Decoder::GetData(char *&pOutData, int &iSize)
{
	int ret = avcodec_receive_frame(c, frame);
	if (ret <0)
	{
		return false;
	}
	//Ð´ÈëÊý¾Ý  
	int a = 0, i;
	for (i = 0; i<frame->height; i++)
	{
		memcpy(m_YuvBuf + a, frame->data[0] + i * frame->linesize[0], frame->width);
		a += frame->width;
	}
	for (i = 0; i<frame->height / 2; i++)
	{
		memcpy(m_YuvBuf + a, frame->data[1] + i * frame->linesize[1], frame->width / 2);
		a += frame->width / 2;
	}
	for (i = 0; i<frame->height / 2; i++)
	{
		memcpy(m_YuvBuf + a, frame->data[2] + i * frame->linesize[2], frame->width / 2);
		a += frame->width / 2;
	}
	pOutData = m_YuvBuf;
	iSize = frame->height*frame->width*1.5;
	return true;
}