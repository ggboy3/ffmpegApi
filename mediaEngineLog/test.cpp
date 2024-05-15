
#include "MGVideoInterface.h"
#include <stdio.h>
#include <iostream>


int read(int data_type, uint8_t *data, int data_size, void *args)//提供数据的回调函数
{
	static int num = 0;
	
	if (data_type == AV_CODEC_ID_H264)
	{
	
		FeedH264Data(0, (const char *) data, data_size, 1920, 1080, -1);

	}
	else if(data_type == AV_CODEC_ID_H265)
	{

		FeedH265Data(0, (const char *) data, data_size, 1920, 1080, -1);

	}
	else if(data_type == AV_CODEC_ID_AAC)
	{
		FeedAACAudioData(0, (const char *)data, data_size);
	}

	return 0;
}

int main(int argc, char** argv) {

InitMGMediaEngine(0);

	while (true) {
		std::string userInput;
	
		std::cout << "请输入字符串: ";
		std::getline(std::cin, userInput);

		        // 根据用户输入的不同字符串执行相应的代码
		if (userInput == "1")
		{
			std::cout << "1路推流" << std::endl;
			SetVideoCodecType(0, VIDEO_CODEC_TYPE_H265);
			DoStartLive(0, "rtmp://192.168.223.132/live/stream", 0);
          	
		}
		else if (userInput == "1s")
		{
		std::cout << "1路停止" << std::endl;
			DoStopLive(0, "rtmp://192.168.223.132/live/stream");			
		}
		else if (userInput == "exit")
		{
			std::cout << "程序结束" << std::endl;
			break;  // 如果用户输入 exit 则跳出循环，结束程序
		}
		else {
		
			std::cout << "未知的输入" << std::endl;
			
			break;
		}
	}


}