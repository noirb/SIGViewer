#ifndef SIGVIEWER_FFMPEG_ENCODER_H
#define SIGVIEWER_FFMPEG_ENCODER_H

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

// work around duplicate math constants from avutil
#undef M_E
#undef M_LN2
#undef M_SQRT1_2

/* Adapted from: https://github.com/cirosantilli/cpp-cheat/blob/19044698f91fefa9cb75328c44f7a487d336b541/ffmpeg/encode.c */

namespace
{
    enum Constants { SCREENSHOT_MAX_FILENAME = 256 };
    static GLubyte *pixels = NULL;
    static GLuint fbo;
    static GLuint rbo_color;
    static GLuint rbo_depth;
    static const unsigned int HEIGHT = 100;
    static const unsigned int WIDTH = 100;
    static int offscreen = 1;
    static unsigned int max_nframes = 100;
    static unsigned int nframes = 0;
    static unsigned int time0;

    static AVCodecContext *c = NULL;
    static AVFrame *frame;
    static AVPacket pkt;
    static FILE *file;
    static struct SwsContext *sws_context = NULL;
    static uint8_t *rgb = NULL;

    static void ffmpeg_encoder_set_frame_yuv_from_rgb(uint8_t *rgb) {
        const int in_linesize[1] = { 3 * c->width };
        sws_context = sws_getCachedContext(sws_context,
            c->width, c->height, AV_PIX_FMT_RGB24,
            c->width, c->height, AV_PIX_FMT_YUV420P,
            0, 0, 0, 0);
        sws_scale(sws_context, (const uint8_t * const *)&rgb, in_linesize, 0,
            c->height, frame->data, frame->linesize);
    }

    void ffmpeg_encoder_start(const char *filename, AVCodecID codec_id, int fps, int width, int height) {
        AVCodec *codec;
        int ret;
        avcodec_register_all();
        codec = avcodec_find_encoder(codec_id);
        if (!codec) {
            fprintf(stderr, "Codec not found\n");
            exit(1);
        }
        c = avcodec_alloc_context3(codec);
        if (!c) {
            fprintf(stderr, "Could not allocate video codec context\n");
            exit(1);
        }
        c->bit_rate = 4000000;
        c->width = width;
        c->height = height;
        c->time_base.num = 1;
        c->time_base.den = fps;
        c->gop_size = 10;
        c->max_b_frames = 1;
        c->pix_fmt = AV_PIX_FMT_YUV420P;
        if (codec_id == AV_CODEC_ID_H264)
            av_opt_set(c->priv_data, "preset", "slow", 0);
        if (avcodec_open2(c, codec, NULL) < 0) {
            fprintf(stderr, "Could not open codec\n");
            exit(1);
        }
        file = fopen(filename, "wb");
        if (!file) {
            fprintf(stderr, "Could not open %s\n", filename);
            exit(1);
        }
        frame = av_frame_alloc();
        if (!frame) {
            fprintf(stderr, "Could not allocate video frame\n");
            exit(1);
        }
        frame->format = c->pix_fmt;
        frame->width = c->width;
        frame->height = c->height;
        ret = av_image_alloc(frame->data, frame->linesize, c->width, c->height, c->pix_fmt, 32);
        if (ret < 0) {
            fprintf(stderr, "Could not allocate raw picture buffer\n");
            exit(1);
        }
    }

    void ffmpeg_encoder_finish(void) {
        uint8_t endcode[] = { 0, 0, 1, 0xb7 };
        int got_output, ret;
        do {
            fflush(stdout);
            ret = avcodec_encode_video2(c, &pkt, NULL, &got_output);
            if (ret < 0) {
                fprintf(stderr, "Error encoding frame\n");
                exit(1);
            }
            if (got_output) {
                fwrite(pkt.data, 1, pkt.size, file);
                av_packet_unref(&pkt);
            }
        } while (got_output);
        fwrite(endcode, 1, sizeof(endcode), file);
        fclose(file);
        avcodec_close(c);
        av_free(c);
        av_freep(&frame->data[0]);
        av_frame_free(&frame);
    }

    void ffmpeg_encoder_encode_frame(uint8_t *rgb) {
        frame->pts = nframes; nframes++;
        int ret, got_output;
        ffmpeg_encoder_set_frame_yuv_from_rgb(rgb);
        av_init_packet(&pkt);
        pkt.data = NULL;
        pkt.size = 0;
        ret = avcodec_encode_video2(c, &pkt, frame, &got_output);
        if (ret < 0) {
            fprintf(stderr, "Error encoding frame\n");
            exit(1);
        }
        if (got_output) {
            fwrite(pkt.data, 1, pkt.size, file);
            av_packet_unref(&pkt);
        }
    }

    void ffmpeg_encoder_glread_rgb(uint8_t **rgb, GLubyte **pixels, unsigned int width, unsigned int height) {
        size_t i, j, k, cur_gl, cur_rgb, nvals;
        const size_t format_nchannels = 3;
        nvals = format_nchannels * width * height;
        *pixels = (GLubyte*)realloc(*pixels, nvals * sizeof(GLubyte));
        *rgb    = (GLubyte*)realloc(*rgb, nvals * sizeof(uint8_t));
        glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, *pixels);
        for (i = 0; i < height; i++) {
            for (j = 0; j < width; j++) {
                cur_gl  = format_nchannels * (width * (height - i - 1) + j);
                cur_rgb = format_nchannels * (width * i + j);
                for (k = 0; k < format_nchannels; k++)
                    (*rgb)[cur_rgb + k] = (*pixels)[cur_gl + k];
            }
        }
    }

    void ffmpeg_encoder_glgettexture_rgb(uint8_t **rgb, GLubyte **pixels, GLuint texid, unsigned int width, unsigned int height) {
        size_t i, j, k, cur_gl, cur_rgb, nvals;
        const size_t format_nchannels = 3;
        nvals = format_nchannels * width * height;
        *pixels = (GLubyte*)realloc(*pixels, nvals * sizeof(GLubyte));
        *rgb    = (GLubyte*)realloc(*rgb, nvals * sizeof(uint8_t));
        glGetTextureImageEXT(texid, GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, *pixels);
        for (i = 0; i < height; i++) {
            for (j = 0; j < width; j++) {
                cur_gl = format_nchannels * (width * i + j); // (width * (height - i - 1) + j);
                cur_rgb = format_nchannels * (width * i + j);
                for (k = 0; k < format_nchannels; k++)
                    (*rgb)[cur_rgb + k] = (*pixels)[cur_gl + k];
            }
        }
    }

}

#endif // SIGVIEWER_FFMPEG_ENCODER_H