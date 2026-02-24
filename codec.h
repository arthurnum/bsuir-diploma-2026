#ifndef __DIPLOMA_CODEC_H
#define __DIPLOMA_CODEC_H

#include <libavcodec/avcodec.h>

// Error code
#define ERR_FRAME_ENCODE -10
#define ERR_ENCODING -11
#define ERR_PACKET_DECODE -20
#define ERR_DECODING -21

typedef struct Codec {
    const AVCodec* AudioEncoder;
    const AVCodec* AudioDecoder;
    const AVCodec* VideoEncoder;
    const AVCodec* VideoDecoder;

    AVCodecContext* AudioEncoderCtx;
    AVCodecContext* AudioDecoderCtx;
    AVCodecContext* VideoEncoderCtx;
    AVCodecContext* VideoDecoderCtx;

    AVFrame* AudioEncodeInFrame;
    AVFrame* AudioDecodeOutFrame;
    AVFrame* VideoEncodeInFrame;
    AVFrame* VideoDecodeOutFrame;

    AVPacket* AudioEncodeOutPacket;
    AVPacket* AudioDecodeInPacket;
    AVPacket* VideoEncodeOutPacket;
    AVPacket* VideoDecodeInPacket;

    char* error_str;
} Codec;

Codec* InitCodec();
void FreeCodec(Codec* codec);

int EncodeAudio(Codec* codec);
int DecodeAudio(Codec* codec);
int EncodeVideo(Codec* codec);
int DecodeVideo(Codec* codec);
int _encode(AVCodecContext *enc_ctx, AVFrame **frame, AVPacket **pkt);
int _decode(AVCodecContext *enc_ctx, AVFrame **frame, AVPacket **pkt);

#endif
