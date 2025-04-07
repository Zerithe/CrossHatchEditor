#pragma once
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}
#include <bgfx/bgfx.h>

class VideoPlayer
{
public:
    AVFormatContext* fmtCtx = nullptr;
    AVCodecContext* codecCtx = nullptr;
    AVFrame* frame = nullptr;
    AVFrame* rgbFrame = nullptr;
    AVPacket* packet = nullptr;
    SwsContext* swsCtx = nullptr;
    int videoStreamIndex = -1;
    bgfx::TextureHandle texture = BGFX_INVALID_HANDLE;
    int width = 0, height = 0;
    uint8_t* buffer = nullptr;

    bool load(const char* filename)
    {
        avformat_open_input(&fmtCtx, filename, nullptr, nullptr);
        avformat_find_stream_info(fmtCtx, nullptr);

        videoStreamIndex = -1;
        for (unsigned i = 0; i < fmtCtx->nb_streams; i++)
        {
            if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            {
                videoStreamIndex = i;
                break;
            }
        }
        if (videoStreamIndex == -1) return false;

        AVCodec* codec = const_cast<AVCodec*>(avcodec_find_decoder(fmtCtx->streams[videoStreamIndex]->codecpar->codec_id));
        codecCtx = avcodec_alloc_context3(codec);
        avcodec_parameters_to_context(codecCtx, fmtCtx->streams[videoStreamIndex]->codecpar);
        avcodec_open2(codecCtx, codec, nullptr);

        frame = av_frame_alloc();
        rgbFrame = av_frame_alloc();
        packet = av_packet_alloc();

        width = codecCtx->width;
        height = codecCtx->height;

        swsCtx = sws_getContext(width, height, codecCtx->pix_fmt, width, height, AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr);

        int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGBA, width, height, 1);
        buffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));
        av_image_fill_arrays(rgbFrame->data, rgbFrame->linesize, buffer, AV_PIX_FMT_RGBA, width, height, 1);

        // Create bgfx texture
        texture = bgfx::createTexture2D((uint16_t)width, (uint16_t)height, false, 1, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_NONE, nullptr);

        return true;
    }

    void update()
    {
        if (av_read_frame(fmtCtx, packet) >= 0)
        {
            if (packet->stream_index == videoStreamIndex)
            {
                int response = avcodec_send_packet(codecCtx, packet);
                if (response >= 0)
                {
                    response = avcodec_receive_frame(codecCtx, frame);
                    if (response >= 0)
                    {
                        sws_scale(swsCtx, frame->data, frame->linesize, 0, codecCtx->height, rgbFrame->data, rgbFrame->linesize);

                        const bgfx::Memory* mem = bgfx::makeRef(buffer, width * height * 4);
                        bgfx::updateTexture2D(texture, 0, 0, 0, 0, (uint16_t)width, (uint16_t)height, mem);
                    }
                }
            }
            av_packet_unref(packet);
        }
        else
        {
            // Loop video: seek back to start
            av_seek_frame(fmtCtx, videoStreamIndex, 0, AVSEEK_FLAG_BACKWARD);
        }
    }

    void shutdown()
    {
        if (texture.idx != bgfx::kInvalidHandle) bgfx::destroy(texture);
        if (packet) av_packet_free(&packet);
        if (frame) av_frame_free(&frame);
        if (rgbFrame) av_frame_free(&rgbFrame);
        if (codecCtx) avcodec_free_context(&codecCtx);
        if (fmtCtx) avformat_close_input(&fmtCtx);
        if (buffer) av_free(buffer);
        if (swsCtx) sws_freeContext(swsCtx);
    }
};