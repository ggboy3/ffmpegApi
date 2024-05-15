#include "VMDxva2.h"
#include "MergerServer.h"
using namespace std;

MergerServer vmServer;
int main()
{
	cout << "hello world\n";
	int flag = 1;
	VideoInfo video[5];

	video[1].url = "rtsp://admin:jkkj12345@192.168.1.66/h264/ch1/sub/av_stream";
	video[1].index = 1;
	video[2].url = "rtsp://admin:jkkj12345@192.168.1.66/h264/ch1/sub/av_stream";
	video[2].index = 2;
	video[3].url = "rtsp://admin:jkkj12345@192.168.1.66/h264/ch1/sub/av_stream";
	video[3].index = 3;
	video[4].url = "rtsp://admin:jkkj12345@192.168.1.66/h264/ch1/main/av_stream";
	video[4].index = 4;
	while (flag)
	{
		clock_t time = clock();
		//获取每路流的数据
		vmServer.MergeStartup("C:/Users/Jack-PC/Desktop/logo.png", "rtsp://admin:jkkj12345@192.168.1.66/h264/ch1/sub/av_stream",video);
		//获取每路流的数据-end

		//显示或编码推流
		//...
		//显示或编码推流-end
	}
	//ffServer.MergeDeinit(merge);
}