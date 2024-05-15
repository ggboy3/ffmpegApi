# CMakeList.txt: VMDxva2 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
cmake_minimum_required (VERSION 3.8)

project ("VMDxva2")

set(CMAKE_CXX_STANDARD 17)
set(INC_DIR "ffmpeg/include/")
set(LIB_DIR "ffmpeg/lib/")
include_directories(${INC_DIR} )
link_directories(${LIB_DIR} )
  
# 将源代码添加到此项目的可执行文件。
add_executable (VMDxva2 "VMDxva2.cpp" "VMDxva2.h" "MergerServer.cpp" "MergerServer.h" "FFmpegServer.cpp" "FFmpegServer.h")

# TODO: 如有需要，请添加测试并安装目标。

target_link_libraries(VMDxva2 avdevice.lib avformat.lib  avcodec.lib avutil.lib swscale.lib swresample.lib avfilter.lib postproc.lib)

if(CMAKE_BUILD_TYPE STREQUAL Debug)
    #target_link_libraries(VMDxva2 log4cplusD.lib)
else()
    #target_link_libraries(VMDxva2 log4cplus.lib)
endif()