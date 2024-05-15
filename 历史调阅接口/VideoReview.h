


#ifndef FF_VIDEOPUSH
#define FF_VIDEOPUSH
extern "C"
{
#include "libavformat/avformat.h"
#include "libavutil/imgutils.h"
#include "libavutil/time.h" 
#include "libavutil/dict.h"
}
#include <sys/time.h>
#include <stdint.h>
#include <string>
#include <atomic>
#include<thread>
#include <mutex>
#include <chrono>
#include <condition_variable>
#include <functional>

//该接口的功能是将mp4，ts，flv等标准媒体文件里面的音视频数据解封装并回调给调用者，并且支持多文件列表，和任意视频时间点跳转的数据回调
typedef struct video_info
{
	int64_t                        file_start_time = { 0 };//文件开始时间
	int64_t                        file_end_time = { 0 };//文件结束时间
	char                           file_path[265] = { 0 };//文件路径
} VIDEO_INFO;

class Timer {
private:
	std::chrono::high_resolution_clock::time_point startTime; // 
	std::chrono::duration<double, std::milli> elapsedTime; //

public:

	void start() {
		startTime = std::chrono::high_resolution_clock::now();
		elapsedTime = std::chrono::duration<double, std::milli>::zero();
	}

	
	double stop() {
		std::chrono::high_resolution_clock::time_point endTime = std::chrono::high_resolution_clock::now();
		return (elapsedTime + (endTime - startTime)).count() / 1000000000.0;
	}

	    // 
	double getTime() {
		std::chrono::high_resolution_clock::time_point currentTime = std::chrono::high_resolution_clock::now();
		return (elapsedTime + (currentTime - startTime)).count() / 1000000000.0;
	}
};

class VideoReview : public std::enable_shared_from_this<VideoReview>
{
public:
	~VideoReview()
	{
		Close();
	}
	VideoReview()
		: stop_thread(1)
		, pause_video(0)
		, stop_video(-1)
	{}
	

	int Init_Video_Review(VIDEO_INFO * in_files, int num, int(*callback)(int data_type, uint8_t *data, int data_size, int i_key, unsigned long long Times, void *args), void *args_);//????h?????
	
	int Seek_Video(int64_t next_seek_times);//跳转视频时间

	void Stop();//ֹͣ停止数据回答停止线程

private:
	int Video_Push();//回调任务线程
	void Close();
	int Open_Input_File();//打开视频文件初始化
	int Stream_Filter(AVStream * &in_stream, AVPacket * &packet);
	
	const int sampling_frequencies[12] = {
		96000,  // 0x0
		88200,  // 0x1
		64000,  // 0x2
		48000,  // 0x3
		44100,  // 0x4
		32000,  // 0x5
		24000,  // 0x6
		22050,  // 0x7
		16000,  // 0x8
		12000,  // 0x9
		11025,  // 0xa
		8000   // 0xb
		// 0xc d e f�Ǳ�����
	};

	int adts_header(uint8_t * const p_adts_header,
		const int data_length,
		const int profile,
		const int samplerate,
		const int channels);

	void merging_audio_packet(AVCodecContext *output_codec_context, AVPacket *pkt, AVPacket *pkt2);
	
	unsigned long long getElapsedTime();
		
	AVStream *in_stream = NULL;
	AVFormatContext	*pFormatCtx = NULL;           
	AVBitStreamFilterContext* hevcbsfc = NULL;
	AVBitStreamFilterContext* h264bsfc = NULL;
	AVBSFContext *bsf_ctx = NULL;
	AVBSFContext *bsf_ctx2 = NULL;
	AVBitStreamFilter *filter = NULL;
	AVBitStreamFilter *filter2 = NULL;
	AVIOContext *avio_ctx = NULL;

	
	int video_index = -1;
	int audio_index = -1;
	void *args = NULL;
	
	int64_t seek_times = 0;
	int num = 0;
	int The_num_video = 0;
	VIDEO_INFO * in_files;
	const char * out_file = NULL;
	std::atomic<int> stop_thread;
	std::atomic<int> pause_video;
	std::atomic<int> stop_video;
	std::mutex cvMutex;/
	std::shared_ptr<std::thread> read_thread;
	Timer timer;//定时器
	std::condition_variable cv;
	int(*callback)(int data_type, uint8_t *data, int data_size, int i_key, unsigned long long Times, void *args);
};



#endif // !EXTRAT_FRAMETOMP4_H

// int read(int data_type, uint8_t *data, int data_size, unsigned long long Times)//数据回调函数，将音视频数据回调到这里
// {
// 	if (data_type == AV_CODEC_ID_H264)
// 	{
// 	printf("data_type = %d data = %02x %02x %02x %02x %02x %02x data_size = %d pts = %lld\n", data_type, 
// 	data[0], data[1], data[2], data[3], data[4], data[5], data_size, Times);
// 	}
// 	else if(data_type == AV_CODEC_ID_AAC)
// 	{
// 		printf("data_type = %d data = %02x %02x %02x %02x %02x %02x data_size = %d pts = %lld\n", data_type, 
// 	data[0], data[1], data[2], data[3], data[4], data[5], data_size, Times);
// 	}
// }

// int main()
// {
// VIDEO_INFO  in_files[2];//视频文件列表
// 	in_files[0].file_start_time = 1695250800000;//UTC时间
// 	in_files[0].file_end_time = 1695251045000;
// 	strcpy(in_files[0].file_path, "/home/ggboy/Desktop/device/kernel/source/test/linux/wdsj.mp4");
// 	in_files[1].file_start_time = 1695251046000;
// 	in_files[1].file_end_time = 1695251253000;
// 	strcpy(in_files[1].file_path, "/home/ggboy/Desktop/device/kernel/source/test/linux/Vitas.mp4");

// 	std::shared_ptr<VideoReview> Pull;//创建对象
// 	Pull = std::make_shared<VideoReview>();
//    Pull->Init_Video_Review(in_files, 2, &read);//初始化接口
// 		while (true) {
// 		std::string userInput;
	
// 		std::cout << "请输入功能选项: ";
// 		std::getline(std::cin, userInput);

// 		        //视频时间跳转
// 		if (userInput == "1") {

// 			Pull->Seek_Video(1695250800000);
          	
// 		}
// 		else if (userInput == "1s")//停止数据回调介绍线程
// 		{
// 			Pull->Stop();			
// 		}

// 		}

// }