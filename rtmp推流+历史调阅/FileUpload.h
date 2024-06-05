#ifndef FILE_UPLOAD_H
#define FILE_UPLOAD_H
extern "C"
{
#include <libavformat/avformat.h>
#include "libavutil/imgutils.h"
#include "libavutil/time.h" 
}
#include <sys/time.h>
#include <stdint.h>
#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <chrono>
#include <condition_variable>
#include <functional>
typedef struct video_info_
{
	int64_t                        file_start_time = { 0 };
	int64_t                        file_end_time = { 0 };
	char                           file_path[265] = { 0 };
	char                           rtmp_url[265] = { 0 };
} VIDEO_FILE_INFO;

class FileTimer {
private:
	std::chrono::high_resolution_clock::time_point startTime; 
	std::chrono::duration<double, std::milli> elapsedTime; 

public:
    
	void start() {
		startTime = std::chrono::high_resolution_clock::now();
		elapsedTime = std::chrono::duration<double, std::milli>::zero();
	}

	    
	double stop() {
		std::chrono::high_resolution_clock::time_point endTime = std::chrono::high_resolution_clock::now();
		return (elapsedTime + (endTime - startTime)).count() / 1000000000.0;
	}

	   
	double getTime() {
		std::chrono::high_resolution_clock::time_point currentTime = std::chrono::high_resolution_clock::now();
		return (elapsedTime + (currentTime - startTime)).count() / 1000000000.0;
	}
};

class VideoUpload : public std::enable_shared_from_this<VideoUpload>
{
public:
	~VideoUpload()
	{
		Close();
	}
	VideoUpload()
		: stop_thread(1)
		, pause_video(0)
		, stop_video(-1)
	{}
	

	int Init_Video_Upload(VIDEO_FILE_INFO * in_files, int num);
	
	int Seek_Video(int64_t next_seek_times);
	void Stop();

private:
	int Video_Push();
	void Close();
	int Open_Input_File();
	int Init_Output_File(const char* outFilename, const char *out_format);
	int Stream_Filter(AVStream * &in_stream, AVPacket * &packet);
	void video_muxer(AVFormatContext** input_format_context, AVFormatContext** output_format_context, AVPacket* input_pkt);
	void audio_muxer(AVFormatContext** input_format_context, AVFormatContext** output_format_context, AVPacket* input_pkt);
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
		// 0xc d e f???????
	};

	int adts_header(uint8_t * const p_adts_header,
		const int data_length,
		const int profile,
		const int samplerate,
		const int channels);

	void merging_audio_packet(AVCodecContext *output_codec_context, AVPacket *pkt, AVPacket *pkt2);
	
	unsigned long long getElapsedTime();
		
	AVStream *in_stream = NULL;
	AVFormatContext* ofmt_ctx = NULL;
	AVFormatContext	*pFormatCtx = NULL;            //?????????????
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
	VIDEO_FILE_INFO * in_files;
	const char * out_file = NULL;
	std::atomic<int> stop_thread;
	std::atomic<int> pause_video;
	std::atomic<int> stop_video;
	std::mutex cvMutex;//??
	std::shared_ptr<std::thread> read_thread;
	FileTimer timer;//?????
	std::condition_variable cv;//????????
	int(*callback)(int data_type, uint8_t *data, int data_size, int i_key, unsigned long long Times, void *args);
};
#endif
