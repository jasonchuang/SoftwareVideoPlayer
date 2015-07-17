/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#include <string.h>
#include <time.h>
#include <jni.h>
#include <android/bitmap.h>
#include "libavutil/pixfmt.h"

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "debug.h"
#include "yuv2rgb.neon.h"


AVFormatContext *gAVFormatContext;
AVStream *gVideoAVStream;
int gVideoStreamIndex;
int gVideoFrameNumber;

struct SwsContext *gSwsContext;
void* buffer;
AVFrame *gRGBAFrame = NULL;
AVFrame *gYUVFrame = NULL;

#define ONE_MILLI_IN_MICROS 1000

jint
Java_com_jasonsoft_softwarevideoplayer_VideoSurfaceView_nativeMediaInit(JNIEnv* env, jobject thiz, jstring pFilename)
{
    char *videoFileName = (char *)(*env)->GetStringUTFChars(env, pFilename, NULL);
    LOGI("Video file name is %s", videoFileName);

    av_register_all();

    // open the video file
    if (avformat_open_input(&gAVFormatContext, videoFileName, NULL, NULL) != 0) {
        LOGE("could not open video file: %s", videoFileName);
        return -1;
    }

    LOGI("After avformat_open, video file name is %s", videoFileName);
    // retrieve stream info
    if (avformat_find_stream_info(gAVFormatContext, NULL) < 0) {
        LOGE("could not find stream info");
        return -1;
    }

    int index;
    gVideoStreamIndex = -1;
    LOGI("Stream no:%d", gAVFormatContext->nb_streams);
    for (index = 0; index < gAVFormatContext->nb_streams; index++) {
        if (AVMEDIA_TYPE_VIDEO == gAVFormatContext->streams[index]->codec->codec_type) {
            gVideoStreamIndex = index;
            gVideoAVStream = gAVFormatContext->streams[index];
        }
    }

    if (-1 == gVideoStreamIndex) {
        LOGI("could not find a video stream");
        return -1;
    }

    AVCodecContext *pCodecctx = gAVFormatContext->streams[gVideoStreamIndex]->codec;
    AVCodec *pCodec = avcodec_find_decoder(pCodecctx->codec_id);
    LOGI("AVCodec name: %s", pCodec->name);
    LOGI("AVCodec long name: %s", pCodec->long_name);
    if (pCodec == NULL) {
        LOGE("unsupported codec");
        return -1;
    }

    if (avcodec_open2(pCodecctx, pCodec, NULL) < 0) {
        LOGE("could not open codec");
        return -1;
    }

    gVideoFrameNumber = 0;
    LOGI("After avcodec_open2");

    return 0;
}

jintArray
Java_com_jasonsoft_softwarevideoplayer_VideoSurfaceView_nativeGetMediaResolution(JNIEnv* env, jobject thiz) {
    jintArray lRes;
    AVCodecContext* codecCtx;
    codecCtx = gVideoAVStream->codec;

    if (NULL == codecCtx) {
        return NULL;
    }

    lRes = (*env)->NewIntArray(env, 2);
    if (lRes == NULL) {
        LOGI("cannot allocate memory for video size");
        return NULL;
    }
    jint lVideoRes[2];
    lVideoRes[0] = codecCtx->width;
    lVideoRes[1] = codecCtx->height;
    (*env)->SetIntArrayRegion(env, lRes, 0, 2, lVideoRes);
    return lRes;
}

jint
Java_com_jasonsoft_softwarevideoplayer_VideoSurfaceView_nativeGetMediaDuration(JNIEnv* env, jobject thiz) {
    if (NULL != gAVFormatContext) {
        return (gAVFormatContext->duration / AV_TIME_BASE);
    } else {
        return -1;
    }
}

jint
Java_com_jasonsoft_softwarevideoplayer_VideoSurfaceView_nativeGetMediaFps(JNIEnv* env, jobject thiz) {
    int fps;
    if(gVideoAVStream->avg_frame_rate.den && gVideoAVStream->avg_frame_rate.num) {
        fps = av_q2d(gVideoAVStream->avg_frame_rate);
    } else if(gVideoAVStream->r_frame_rate.den && gVideoAVStream->r_frame_rate.num) {
        fps = av_q2d(gVideoAVStream->r_frame_rate);
    } else if(gVideoAVStream->time_base.den && gVideoAVStream->time_base.num) {
        fps = 1/av_q2d(gVideoAVStream->time_base);
    } else if(gVideoAVStream->codec->time_base.den && gVideoAVStream->codec->time_base.num) {
        fps = 1/av_q2d(gVideoAVStream->codec->time_base);
    }
    return fps;
}

jint
Java_com_jasonsoft_softwarevideoplayer_VideoSurfaceView_nativePrepareBitmap(JNIEnv* env, jobject thiz,
        jobject pBitmap, jint width, jint height) {
    LOGE("In nativePrepareBitmap");
    // Allocate video frame
    gYUVFrame = avcodec_alloc_frame();
    gRGBAFrame = avcodec_alloc_frame();

    if(gRGBAFrame == NULL || gYUVFrame == NULL) {
        return -1;
    }

    AndroidBitmapInfo linfo;
    int lret;
    // Retrieve information about the bitmap
    if ((lret = AndroidBitmap_getInfo(env, pBitmap, &linfo)) < 0) {
        LOGE("AndroidBitmap_getinfo failed! error = %d", lret);
        return -1;
    }

    if (linfo.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
        LOGE("bitmap format is not rgba_8888!");
        return- 1;
    }
    // Lock the pixel buffer and retrieve a pointer to it
    if ((lret = AndroidBitmap_lockPixels(env, pBitmap, &buffer)) < 0) {
        LOGE("AndroidBitmap_lockpixels() failed! error = %d", lret);
        return -1;
    }

    // for android, we use the bitmap buffer as the buffer for pFrameRGBA
    avpicture_fill((AVPicture*) gRGBAFrame, buffer, PIX_FMT_RGBA, width, height);

    AVCodecContext* codecCtx = gVideoAVStream->codec;

    gSwsContext = sws_getContext(codecCtx->height, codecCtx->width,
            codecCtx->pix_fmt, width, height, PIX_FMT_RGBA, SWS_BICUBIC, NULL, NULL, NULL);
    
    if (NULL == gSwsContext) {
        LOGE("error initialize the video frame conversion context");
        return -1;
    }

    LOGI("All bitmaps and frames are ready");
    return 0;
}


jint
Java_com_jasonsoft_softwarevideoplayer_VideoSurfaceView_nativeGetFrame(JNIEnv* env, jobject thiz)
{
    AVPacket packet;
    int framefinished;

    while (av_read_frame(gAVFormatContext, &packet) >= 0) {
        if (gVideoStreamIndex == packet.stream_index) {

            // Actually decode video API
            avcodec_decode_video2(gVideoAVStream->codec, gYUVFrame, &framefinished, &packet);

            if (framefinished) {
                int width, height;
                width = gVideoAVStream->codec->width;
                height = gVideoAVStream->codec->height;

                if (gYUVFrame->format == PIX_FMT_YUV420P) {
                    yuv420_2_rgb8888_neon(gRGBAFrame->data[0],
                            gYUVFrame->data[0], gYUVFrame->data[2], gYUVFrame->data[1],
                            width, height, gYUVFrame->linesize[0], gYUVFrame->linesize[1], width << 2);
                } else {
                    sws_scale(gSwsContext, gYUVFrame->data, gYUVFrame->linesize, 0, gVideoAVStream->codec->height,
                            gRGBAFrame->data, gRGBAFrame->linesize);
                }

                gVideoFrameNumber++;
                return gVideoFrameNumber;
            }
        }

        av_free_packet(&packet);
    }

    return -1;
}

    jint
Java_com_jasonsoft_softwarevideoplayer_VideoSurfaceView_nativeMediaFinish(JNIEnv* env, jobject thiz, jobject pBitmap)
{
    AndroidBitmap_unlockPixels(env, pBitmap);
    // Free the RGB image
    av_free(gRGBAFrame);
    // Free the YUV frame
    av_free(gYUVFrame);

    avcodec_close(gVideoAVStream->codec);
    avformat_close_input(&gAVFormatContext);
    return 0;
}

void
Java_com_jasonsoft_softwarevideoplayer_VideoSurfaceView_nativeSleep(JNIEnv* env, jobject thiz, jint ms) {
    LOGE("nativeSleep %d", ms);
    usleep(ms);
//    sleep_in_millis(ms);
    return 0;
}

void sleep_in_millis(int ms) {
    LOGE("sleep_in_millis");
   struct timeval tv = {.tv_sec = 0, .tv_usec = ms * ONE_MILLI_IN_MICROS};
   select(0, NULL, NULL, NULL, &tv);
}
