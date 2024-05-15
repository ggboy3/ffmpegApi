//============================================================================
// Name        : record_parse.cpp
// Author      : 
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================


#include "time.h"
#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "record_parse.h"
#define ZE_DVR_CHN_MAX		32 //最多32个通道
#define MDV_VERSION_03		"MDV-15.03.26\0\0\0\0"	// 版本
#define MDV_VERSION_LEN		16


#define TAG_FST			"_fst"
#define TAG_PRV			"_prv"
#define TAG_MED			"_med"
#define TAG_LEN			4

// 最大的帧长度
#define ZE_MAX_FRAME_SIZE		(512*1024)

#ifndef min
#define min(a,b) ((a) > (b) ? (b) : (a))
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif


#pragma pack(4)
typedef struct
{
	char		frm_type[4];
	int		chn;
	uint64_t	pts;
	int		prev_off;	// 上一个i帧的位置
	uint32_t	len;
}frm_header_s;

typedef struct
{
	char		frm_type[4];
	int		chn;
	uint64_t	pts;
	int		off;		// 该i帧的偏移
	int		len;
	int		idx_id;		// 和创建文件的时间相同, 表明是本文件的索引
}frm_idx_s;


// 文件基本信息
typedef struct
{
	int		reserved0[2];
	int		reserved1[11];
	int		st_size;		// 文件有效尺寸(文件正确结束标记)
	int		reserved2[2];
	int		st_atime;		// 文件开始时间
	int		reserved3;
	int		st_mtime;	// 文件结束时间
	int		reserved4;
	int		st_ctime;		// 文件结束时间2(文件正确结束标记)
	int		reserved5[3];
	int		statfsbuf[16];
} ze_dvr_fst_s;

// dvr私有信息
typedef struct
{
	int		reserved;
	int		stream_type;	// 码流类型, 0=主,1=次,2=镜像
	int		record_type;	// 录像类型, 0=普通, 非0=报警
	uint32_t	vchn_mask;	// 视频掩码
	uint32_t	achn_mask;	// 音频掩码
	char		device_num[16];	// 设备序列号, 000000~999999
	char		phone_num[16];	// 手机号
	char		license_num[16];	// 车牌号
	char		pathname[120];	// dvr录像时的全路径
	int		alarm_flag_bb;	// 部标普通报警
	int		alarm_flag_1076;	// 部标1076报警
} ze_dvr_prv_s;

// 通道信息(64bytes)
typedef struct
{
	int	vcodec;		// 0=h264
	int	vbit_rate;	// 视频码率
	int	width;
	int	height;
	int	frame_rate;	// 帧率
	int	pic_size;	// 0=D1, 1=HD1, 2=2CIF, 3=CIF, 4=QCIF

	int	acodec;		// 0=g726_40k
	int	abit_rate;	// 音频码率
	int	channels;	// 通道数
	int	sampling_rate;	// 采样率
	int	bit_depth;	// 采样位数

// 	uint32_t	stat_vframes;	// 单个通道的音视频数量及总尺寸
// 	uint32_t	stat_aframes;
// 	uint32_t	stat_bframes;
// 	uint32_t	stat_size;
// 	int	reserved[2];
} ze_dvr_med_s;
// 文件信息
typedef struct
{
	ze_dvr_fst_s		fst;		// 时间信息
	ze_dvr_prv_s		prv;		// dvr私有信息
	ze_dvr_med_s		medArr[ZE_DVR_CHN_MAX];	// 通道信息
}ze_dvr_s;
struct __ze_dvr_write_st
{
	char			fullpath[512];
	ze_dvr_s		file_info;

	int			idx_count;
	frm_idx_s		idx_arr[INDEX_MAXNUM];

	FILE			*fd;
	int			total_size;
	int			prev_off;
	int			video_keyframe[32];	// 视频的第一帧必须是关键帧, 如果不是则丢弃
	int			video_keyframe_ok;	// 在视频写入关键帧后, 才能写音频数据和二进制数据
	int			last_print_time;	// 上次打印时间
	int			stat_pkt_num;		// 数据包个数
	int			stat_pkt_len;		// 数据包长度;
	int64_t			first_pts;
	int64_t			last_pts;
};

#pragma pack()

struct __frame_type_tag_st
{
	int		frameType;
	const char	*tag;
};
// 帧类型枚举,0~99标准类型, 100~200,录像二进制格式, 200+,控制帧
typedef enum
{
	DVR_FRAME_TYPE_H264I = 0,		// (v)h264 iframe
	DVR_FRAME_TYPE_H264P = 1,		// (v)h264 pframe
	DVR_FRAME_TYPE_G726_40K = 2,		// (a)g726_40k_media
	DVR_FRAME_TYPE_BIN = 3,			// (b)bin data
	DVR_FRAME_TYPE_PCM = 4,			// (a)s16le
	DVR_FRAME_TYPE_AAC = 5,			// (a)aac
	DVR_FRAME_TYPE_G726_16K = 10,		// (a)g726_16k
	DVR_FRAME_TYPE_G711A = 21,		// (a)g711a
	DVR_FRAME_TYPE_G711U = 22,		// (a)g711u
	DVR_FRAME_TYPE_YV12 = 40,		// (v)yuv数据(yv12/yuv420p)
	DVR_FRAME_TYPE_YUV420SP = 41,		// (v)yuv数据(yuv420sp)
	DVR_FRAME_TYPE_RGB555 = 50,		// (v)rgb数据(rgba1555)
	DVR_FRAME_TYPE_RGB24 = 51,		// (v)rgb数据(rgb24/rgb888)
	DVR_FRAME_TYPE_TEXT = 72,		// (b)text
	DVR_FRAME_TYPE_H265I = 80,		// (v)h265 iframe
	DVR_FRAME_TYPE_H265P = 81,		// (v)h265 pframe
	DVR_FRAME_TYPE_ADPCM = 100,		// (a)adpcm
	DVR_FRAME_TYPE_NONE = 127,		// (a/v)none
}DVR_FRAME_TYPE_E;
// 二进制扩展帧类型
typedef enum
{
	DVR_FRAME_BINTYPE_HEARTBEAT = 0x71,	// [扩展]心跳帧
	DVR_FRAME_BINTYPE_REQ_IFRAME = 0x72,	// [扩展]请求关键帧
	DVR_FRAME_BINTYPE_FILE_END = 0x73,	// [扩展]文件结束帧
	// DVR_FRAME_BINTYPE_BIND = 0x74,	// 绑定帧(用于首帧绑定, 帧类型=音频帧, 帧数据=空)
	DVR_FRAME_BINTYPE_AV_THREAD_EXIT = 0x75,	// [扩展]本地线程退出
	DVR_FRAME_BINTYPE_CONTROL_RECORD = 0x76,	// [扩展]录像控制, 长度为BIN_RECORD_CONTROL_S
	DVR_FRAME_BINTYPE_REQ_AUDIO = 0x81,		// [扩展]请求同步音频

	DVR_FRAME_BINTYPE_GPS = 0x100,		// gps数据, 长度为GPS_INFO_S
	DVR_FRAME_BINTYPE_GSENSOR,		// gsensor数据, 长度为GSENSOR_INFO_S
	DVR_FRAME_BINTYPE_STATUS,		// 设备状态数据, 长度为STATUS_INFO_S
	DVR_FRAME_BINTYPE_ALARM,		// 报警数据, 长度为ALARM_INFO_S
	DVR_FRAME_BINTYPE_DEV,			// 设备数据, 长度为DEV_INFO_S
}DVR_FRAME_BINTYPE_E;
static struct __frame_type_tag_st g_frame_type_tag_array[] =
{
	{DVR_FRAME_TYPE_H264I,		"ifrm"},
	{DVR_FRAME_TYPE_H264P,		"pfrm"},
	{DVR_FRAME_TYPE_G726_40K,	"afrm"},
	{DVR_FRAME_TYPE_BIN,		"bfrm"},
	{DVR_FRAME_TYPE_G711A,		"711a"},
	{DVR_FRAME_TYPE_PCM,		"le16"},
	{DVR_FRAME_TYPE_AAC,		"faac"},
	{DVR_FRAME_TYPE_H265I,		"265i"},
	{DVR_FRAME_TYPE_H265P,		"265p"},
};
static const char *frame_type_to_tag(int frame_type)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(g_frame_type_tag_array); i++)
	{
		if (g_frame_type_tag_array[i].frameType == frame_type)
			return g_frame_type_tag_array[i].tag;
	}
	return "undf"; // 未定义
}
static int frame_type_from_tag(const char *tag)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(g_frame_type_tag_array); i++)
	{
		if (memcmp(tag, g_frame_type_tag_array[i].tag, TAG_LEN) == 0)
			return g_frame_type_tag_array[i].frameType;
	}
	return DVR_FRAME_TYPE_NONE;
}
/************************************************************************
读出
************************************************************************/
struct __ze_dvr_read_st
{
	char			fullpath[512];

	char			version[MDV_VERSION_LEN];
	ze_dvr_s		file_info;
	int			total_size;

	int			idx_count;
	frm_idx_s		idx_arr[INDEX_MAXNUM];

	FILE		*fd;
	int		next_idx_offset;	// 下一个索引的偏移
	int		next_read_offset;	// 下一个读取位置

	uint8_t		iframe_exist[32];	// 如果为0, 表示该通道需要i帧
};

typedef struct __ze_dvr_read_st		ze_dvr_read_t;

static int __dvr_check_packet_header(ze_dvr_read_t *hand, frm_header_s *header)
{
	int frame_type = frame_type_from_tag(header->frm_type);
	if (frame_type < 0)
		return -1;

	//if ( header->fix_file_flag != hand->file_info.fix_file_flag ) // 固定文件信息要匹配
	//	return -1;

	if (header->len <= 0 || header->len > ZE_MAX_FRAME_SIZE) // 帧长度不能超范围
		return -1;

	//if ( header->prev_off < HEAD_BYTES+INDEX_BYTES || header->prev_off >= hand->total_size ) // 在文件中的偏移不能超范围
	//	return -1;

	return 0;
}

int read_content(FILE *fp, char *tag, void *bufptr, int buflen)
{
	int rc;
	int content_len;

	rc = fread(tag, 1, TAG_LEN, fp);
	if (rc <= 0)
	{
		perror("read error\n");
		return -1;
	}

	rc = fread(&content_len, 1, sizeof(int), fp);
	if (rc <= 0)
	{
		perror("read error\n");
		return -1;
	}
	if (content_len <= 0 && content_len > buflen)
	{
		perror("content len error\n");
		return -1;
	}
	rc = fread(bufptr, 1, buflen, fp);
	if (rc <= 0)
	{
		perror("read error\n");
		return -1;
	}

	return rc;
}

std::vector<frame_info> media_list;



int read_packet(void *opaque, uint8_t *buf, int buf_size)
 {	
	FILE * fp = (FILE *)opaque;
	  int size;
	  bool a=false;
       bool b=false;
       bool c=false;
	int rc;
	if(fp == NULL)
	{
		perror("fopen");
		return -1;
	}
	//尝试读取数据
	//printf("\n\n读取数据\n\n");
	frm_header_s header;
  
		do
		{
		rc = fread(&header, 1, sizeof(header), fp);
		frame_info mi;                    
		if(rc <= 0)
		{
			perror("read error\n");
			return -1;
		}
			if (rc != sizeof(header) || __dvr_check_packet_header(NULL, &header) < 0)
				return -1; //break;
			/*printf("i=%d,frm_type=%c%c%c%c,chn=%d, pts=%lld, prev_off=%d, len=%d\n", \
				i, header.frm_type[0], header.frm_type[1], header.frm_type[2], header.frm_type[3], \
				header.chn, header.pts, header.prev_off, header.len);*/
								 	
			if(header.chn == 0)
			{
				a=true;
			}
		    else
			{
				a=false;
			}
			
			if(frame_type_from_tag((char*)header.frm_type) == 0)
			{
				b=true;
			}
		    else
			{
			  b=false;
			}
			
			if(frame_type_from_tag((char*)header.frm_type) == 1)
			{
				c=true;
			}
		    else
			{
				c=false;
			}
			
			if(a&&(b||c))
			{
				  memset(&mi, 0, sizeof(mi)); 
				if (media_list.empty())
				{
					mi.FrameNum = 1;

				}
				else
				{

					mi.FrameNum = media_list.back().FrameNum + 1;
				}
	                    mi.PTS=header.pts;
						if(frame_type_from_tag((char*)header.frm_type) == 0)
						{
							 mi.i_frame=1;							
						}
						else
						{							
							 mi.i_frame=0;
						}
						media_list.push_back(mi); 
						size = fread(buf, 1,header.len, fp);
						
				break;
			}
			
			    rc = fseek(fp, header.len, SEEK_CUR);
		}while(true);

	return size;
}



//int main()
//{
	//FILE *fp1 = fopen("/mnt/hgfs/share/recordex-20230101-020017-0001.mdvx", "rb+");
	//fseek(fp1, HEAD_BYTES + INDEX_BYTES, SEEK_SET);
	/*uint8_t buf[60240];
	int ret=read_packet(fp1,buf);
	printf("=====%d===========",ret);
	 memset(&buf, 0, sizeof(buf)); 
	 ret=read_packet(fp1,buf);
	 printf("=====%d===========",ret);
	  memset(&buf, 0, sizeof(buf)); 
	 ret=read_packet(fp1,buf);
	 printf("=====%d===========",ret);
	//read_packet(fp1);
   
	//read_packet(fp1);
	///read_packet(fp1);
	///read_packet(fp1);
	//read_packet(fp1);*/
	
	//for (const auto& frame : media_list)
	//{
    // 访问frame_info结构体的成员变量
    //std::cout << "PTS: " << frame.PTS << ", FrameNum: " << frame.FrameNum << ", i_frame: " << frame.i_frame << std::endl;
	//printf("mi.FrameNum:%d  mi.PTS:%lld  mi.i_frame:%d\n ",frame.FrameNum,frame.PTS,frame.i_frame);
   //}
   ///fclose(fp1);	
	
//}



