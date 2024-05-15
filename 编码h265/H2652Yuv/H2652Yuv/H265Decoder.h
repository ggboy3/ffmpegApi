#pragma once
class H265Decoder
{
public:
	H265Decoder();
	virtual ~H265Decoder();


	bool Init();
	void AddData(char *pData, int iSize);
	bool GetData(char *&pOutData, int &iSize);

	char * m_YuvBuf;
	AVPacket *packet;
	AVFrame *frame;
	const AVCodec *codec;
	AVCodecContext *c = NULL;

};

