// H2652Yuv.cpp: 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "H265Decoder.h"

#pragma error(disable:4996) //全部禁用
#pragma warning(disable:4996)

uint32_t cached = 0;
inline uint32_t Put(unsigned char n, uint32_t v)
{

	uint32_t cache = (cache << n) | (v & (0xFFFFFFFF >> (32 - n)));

	cached += n;
	return v;
}
int main()
{
	Put(5,2);

	//char H265Buf[1024 * 512];
	//int H265Lenth = 0;

	//H265Decoder h265Decoder;
	//h265Decoder.Init();
	//char frame_buf[1024] = { 0 };
	////输出文件
	//FILE * OutFile = fopen("test.YUV", "wb");
	////读取文件
	//FILE * InFile = fopen("2.h265", "rb");
	//while (true)
	//{
	//	int iReadSize = fread(frame_buf, 1, 28, InFile);
	//	if (iReadSize <= 0)
	//	{
	//		break;
	//	}
	//	memcpy(H265Buf+ H265Lenth, frame_buf, iReadSize);
	//	H265Lenth = H265Lenth + iReadSize;
	//	//获取一帧数据
	//	while (true)
	//	{
	//		bool OneFrame = false;
	//		if (H265Lenth<=8)
	//		{
	//			break;
	//		}
	//		for (int i = 4; i < H265Lenth-4; i++)
	//		{
	//			if (H265Buf[i] == 0x00 && H265Buf[i+1] == 0x00&& H265Buf[i+2] == 0x00&& H265Buf[i+3] == 0x01)
	//			{
	//				h265Decoder.AddData(H265Buf, i);
	//				while (true)
	//				{
	//					char * pOutData = NULL;
	//					int OutSize = 0;
	//					if (!h265Decoder.GetData(pOutData, OutSize))
	//					{
	//						break;
	//					}
	//					fwrite(pOutData, 1, OutSize, OutFile);
	//				}
	//				H265Lenth = H265Lenth - i;
	//				memcpy(H265Buf, H265Buf+i, H265Lenth);
	//				OneFrame = true;
	//				break;
	//			}
	//		}
	//		if (OneFrame)
	//		{
	//			continue;
	//		}
	//		else
	//		{
	//			break;
	//		}
	//	}
	//	
	//	


	//}
	//printf("pasing end\r\n");
	//getchar();
    return 0;
}

