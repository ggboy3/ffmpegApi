#Generated by VisualGDB project wizard.
#Note: VisualGDB will automatically update this file when you add new sources to the project.

cmake_minimum_required(VERSION 2.7)
project(Rtmp)
set(LIBRARIES_FROM_REFERENCES "")
add_executable(Rtmp Rtmp.cpp)
include_directories(/tmp/VisualGDB/c/Users/Administrator/Desktop/media/Rtmp/Rtmp/include)
#set(CMAKE_BUILD_RPATH "/tmp/VisualGDB/c/Users/Administrator/Desktop/media/Rtmp/Rtmp/lib")
target_link_libraries(Rtmp 
    /tmp/VisualGDB/c/Users/Administrator/Desktop/media/Rtmp/Rtmp/lib/librtmp.so.0 
    /tmp/VisualGDB/c/Users/Administrator/Desktop/media/Rtmp/Rtmp/lib/libssl.so 
    /tmp/VisualGDB/c/Users/Administrator/Desktop/media/Rtmp/Rtmp/lib/libcrypto.so 
    "${LIBRARIES_FROM_REFERENCES}"
)


