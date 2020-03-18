#include "jni.h"
#include "com_blues_ffmpegdemo_NativePlayer.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/log.h>

#define TAG    "blues-jni-test" // 这个是自定义的LOG的标识
#define LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG,TAG,__VA_ARGS__) // 定义LOGD类型
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,TAG,__VA_ARGS__) // 定义LOGE类型

JNIEXPORT void JNICALL Java_com_blues_ffmpegdemo_NativePlayer_playVideo
  (JNIEnv * env, jclass clazz, jstring url, jobject surface){

    LOGD("start playvideo... url");

    //视频url
    const char * file_name = (*env)->GetStringUTFChars(env,url,JNI_FALSE);
    LOGD("start playvideo... url,%s",file_name);

    av_register_all();

    AVFormatContext * pFormatCtx = avformat_alloc_context();

    //打开视频文件
    int result = avformat_open_input(&pFormatCtx,file_name,NULL,NULL);
    if( result!=0){
        LOGE("Couldn't open file:%s\n， error msg:%s",file_name,av_err2str(result));
        return;
    }

    //检索流信息
    if(avformat_find_stream_info(pFormatCtx,NULL)<0){
        LOGE("Couldn't find stream information");
        return;
    }

    //找到第一个视频流
    int videoStream =-1,i;
    for(int i=0;i<pFormatCtx->nb_streams;i++){
        if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO && videoStream <0 ){
            videoStream =i;
        }
    }

    if(videoStream == 1){
        LOGE("Didn't find a video stream");
        return;
    }

    //得到视频流的编码器上下文环境的指针
    AVCodecContext * pCodecCtx = pFormatCtx->streams[videoStream]->codec;

    //找到视频流对应的解码器
    AVCodec * pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    if(pCodec==NULL){
        LOGE("Codec not found");
        return;
    }

    if(avcodec_open2(pCodecCtx,pCodec,NULL)<0){
        LOGE("Could not open codec");
        return;
    }

    //获取NativeWindow 用于渲染
    ANativeWindow * nativeWindow = ANativeWindow_fromSurface(env,surface);
    //获取视频宽高
    int videoWidth = pCodecCtx->width;
    int videoHeight = pCodecCtx->height;

    //LOGD("video width:%s , video height:%s",videoWidth,videoHeight);

    //设置NativeWindow的Buffer大小，可自动拉伸
    ANativeWindow_setBuffersGeometry(nativeWindow,videoWidth,videoHeight,WINDOW_FORMAT_RGBA_8888);
    ANativeWindow_Buffer windowBuffer;
    if(avcodec_open2(pCodecCtx,pCodec,NULL)<0){
        LOGE("Could not open codec");
        return;
    }

    //分配视频帧空间内存
    AVFrame * pFrame = av_frame_alloc();
    //用于渲染
    AVFrame * pFrameRGBA = av_frame_alloc();
    if(pFrameRGBA == NULL || pFrame ==NULL){
        LOGE("Could not allocate video frame");
        return;
    }

    //确定所需缓冲区大小并分配缓冲区内存空间
    //Buffer中的数据就是用于渲染的，格式为RGBA
    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGBA,pCodecCtx->width,pCodecCtx->height,1);
    uint8_t * buffer = (uint8_t *)av_malloc(numBytes*sizeof(uint8_t));
    av_image_fill_arrays(pFrameRGBA->data,pFrameRGBA->linesize,buffer,AV_PIX_FMT_RGBA,pCodecCtx->width,pCodecCtx->height,1);

    //由于解码出来的帧格式不是RGBA的，所以在渲染之前需要进行格式转换
    struct SwsContext *sws_ctx = sws_getContext(pCodecCtx->width,pCodecCtx->height,pCodecCtx->pix_fmt,pCodecCtx->width,pCodecCtx->height,AV_PIX_FMT_RGBA,SWS_BILINEAR,NULL,NULL,NULL);
    int frameFinished;
    AVPacket packet;
    while(av_read_frame(pFormatCtx,&packet)>=0){
    //判断Packet是否是视频流
        if(packet.stream_index ==videoStream){
        //解码视频帧
            avcodec_decode_video2(pCodecCtx,pFrame,&frameFinished,&packet);

            if(frameFinished){
                //锁住NativeWindow缓冲区
                ANativeWindow_lock(nativeWindow,&windowBuffer,0);

                //格式转换
                sws_scale(sws_ctx,(uint8_t const * const *)pFrame->data,pFrame->linesize,0,pCodecCtx->height,pFrameRGBA->data,pFrameRGBA->linesize);

                //获取stride
                uint8_t * dst = windowBuffer.bits;
                int dstStride = windowBuffer.stride * 4;
                uint8_t * src = (uint8_t*)(pFrameRGBA->data[0]);
                int srcStride = pFrameRGBA->linesize[0];
                //由于窗口的stride和帧的stride不同，因此需要逐行复制
                int h;
                for(h=0;h<videoHeight;h++){
                    memcpy(dst + h * dstStride,src+h * srcStride,srcStride);
                }

                ANativeWindow_unlockAndPost(nativeWindow);
            }
        }
        av_packet_unref(&packet);
    }

    av_free(buffer);
    av_free(pFrameRGBA);
    //释放YUV图像帧
    av_free(pFrame);
    //关闭解码器
    avcodec_close(pCodecCtx);
    //关闭视频文件
    avformat_close_input(&pFormatCtx);
    return;
  };
