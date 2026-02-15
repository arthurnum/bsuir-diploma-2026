#include "codec.h"

Codec* InitCodec() {
    int err;
    Codec* codec = malloc(sizeof(Codec));
    codec->error_str = calloc(256, 1);

    // Video
    codec->VideoEncoder = avcodec_find_encoder(AV_CODEC_ID_H264);
    codec->VideoEncoderCtx = avcodec_alloc_context3(codec->VideoEncoder);
    if (!codec->VideoEncoderCtx) {
        strcpy(codec->error_str, "Could not allocate video encoder context");
        return codec;
    }
    codec->VideoEncoderCtx->width = 1920;
    codec->VideoEncoderCtx->height = 1080;
    codec->VideoEncoderCtx->gop_size = 10;
    codec->VideoEncoderCtx->max_b_frames = 1;
    codec->VideoEncoderCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    codec->VideoEncoderCtx->time_base = (AVRational){1, 25};
    codec->VideoEncoderCtx->framerate = (AVRational){25, 1};
    AVDictionary* codecOpts = NULL;
    av_dict_set(&codecOpts, "threads", "1", 0);
    av_dict_set(&codecOpts, "preset", "ultrafast", 0);
    av_dict_set(&codecOpts, "tune", "zerolatency", 0);
    err = avcodec_open2(codec->VideoEncoderCtx, codec->VideoEncoder, &codecOpts);
    av_dict_free(&codecOpts);
    if (err < 0) {
        sprintf(codec->error_str, "Could not open codec: %s", av_err2str(err));
        return codec;
    }

    codec->VideoDecoder = avcodec_find_decoder(AV_CODEC_ID_H264);
    codec->VideoDecoderCtx = avcodec_alloc_context3(codec->VideoDecoder);
    if (!codec->VideoDecoderCtx) {
        strcpy(codec->error_str, "Could not allocate video decoder context");
        return codec;
    }
    codec->VideoDecoderCtx->width = 1920;
    codec->VideoDecoderCtx->height = 1080;
    codec->VideoDecoderCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    err = avcodec_open2(codec->VideoDecoderCtx, codec->VideoDecoder, NULL);
    if (err < 0) {
        sprintf(codec->error_str, "Could not open codec: %s", av_err2str(err));
        return codec;
    }

    codec->VideoEncodeInFrame = av_frame_alloc();
    codec->VideoEncodeInFrame->format = AV_PIX_FMT_YUV420P;
    codec->VideoDecodeOutFrame = av_frame_alloc();

    codec->VideoEncodeOutPacket = av_packet_alloc();
    codec->VideoDecodeInPacket = av_packet_alloc();
    av_packet_make_writable(codec->VideoDecodeInPacket);
    codec->VideoDecodeInPacket->data = malloc(1 << 19);

    // Audio
    codec->AudioEncoder = avcodec_find_encoder(AV_CODEC_ID_OPUS);
    codec->AudioEncoderCtx = avcodec_alloc_context3(codec->AudioEncoder);
    codec->AudioEncoderCtx->sample_fmt = AV_SAMPLE_FMT_FLT;
    codec->AudioEncoderCtx->sample_rate = 48000;
    codec->AudioEncoderCtx->bit_rate = 64000;
    av_channel_layout_copy(&codec->AudioEncoderCtx->ch_layout, &(AVChannelLayout)AV_CHANNEL_LAYOUT_MONO);
    err = avcodec_open2(codec->AudioEncoderCtx, codec->AudioEncoder, NULL);
    if (err < 0) {
        sprintf(codec->error_str, "Could not open audio codec: %s", av_err2str(err));
        return codec;
    }
    codec->AudioEncodeInFrame = av_frame_alloc();
    codec->AudioEncodeInFrame->format = AV_SAMPLE_FMT_FLT;
    codec->AudioEncodeInFrame->nb_samples = codec->AudioEncoderCtx->frame_size;
    av_channel_layout_copy(&codec->AudioEncodeInFrame->ch_layout, &codec->AudioEncoderCtx->ch_layout);
    codec->AudioEncodeOutPacket = av_packet_alloc();

    codec->AudioDecoder = avcodec_find_decoder(AV_CODEC_ID_OPUS);
    codec->AudioDecoderCtx = avcodec_alloc_context3(codec->AudioDecoder);
    codec->AudioDecoderCtx->request_sample_fmt = AV_SAMPLE_FMT_FLT;
    codec->AudioDecoderCtx->sample_rate = 48000;
    codec->AudioDecoderCtx->bit_rate = 64000;
    av_channel_layout_copy(&codec->AudioDecoderCtx->ch_layout, &(AVChannelLayout)AV_CHANNEL_LAYOUT_MONO);
    err = avcodec_open2(codec->AudioDecoderCtx, codec->AudioDecoder, NULL);
    if (err < 0) {
        sprintf(codec->error_str, "Could not open audio codec: %s", av_err2str(err));
        return codec;
    }
    codec->AudioDecodeOutFrame = av_frame_alloc();
    codec->AudioDecodeOutFrame->format = codec->AudioDecoderCtx->sample_fmt;
    codec->AudioDecodeInPacket = av_packet_alloc();
    av_packet_make_writable(codec->AudioDecodeInPacket);
    codec->AudioDecodeInPacket->data = malloc(1 << 10);

    return codec;
}

void FreeCodec(Codec* codec) {
    avcodec_free_context(&codec->AudioEncoderCtx);
    avcodec_free_context(&codec->AudioDecoderCtx);
    avcodec_free_context(&codec->VideoEncoderCtx);
    avcodec_free_context(&codec->VideoDecoderCtx);
    av_frame_free(&codec->AudioEncodeInFrame);
    av_frame_free(&codec->AudioDecodeOutFrame);
    av_frame_free(&codec->VideoEncodeInFrame);
    av_frame_free(&codec->VideoDecodeOutFrame);
    av_packet_free(&codec->AudioEncodeOutPacket);
    av_packet_free(&codec->AudioDecodeInPacket);
    av_packet_free(&codec->VideoEncodeOutPacket);
    av_packet_free(&codec->VideoDecodeInPacket);
    free(codec->error_str);
    free(codec);
}

int EncodeAudio(Codec* codec) {
    return _encode(codec->AudioEncoderCtx, &codec->AudioEncodeInFrame, &codec->AudioEncodeOutPacket);
}

int DecodeAudio(Codec* codec) {
    return _decode(codec->AudioDecoderCtx, &codec->AudioDecodeOutFrame, &codec->AudioDecodeInPacket);
}

int EncodeVideo(Codec* codec) {
    return _encode(codec->VideoEncoderCtx, &codec->VideoEncodeInFrame, &codec->VideoEncodeOutPacket);
}

int DecodeVideo(Codec* codec) {
    return _decode(codec->VideoDecoderCtx, &codec->VideoDecodeOutFrame, &codec->VideoDecodeInPacket);
}

int _encode(AVCodecContext *enc_ctx, AVFrame **frame, AVPacket **pkt) {
    int err = 0;

    err = avcodec_send_frame(enc_ctx, *frame);
    if (err < 0) {
        return ERR_FRAME_ENCODE;
    }

    while (err >= 0) {
        err = avcodec_receive_packet(enc_ctx, *pkt);
        if (err == AVERROR(EAGAIN) || err == AVERROR_EOF)
            return 0;
        else if (err < 0) {
            return ERR_ENCODING;
        }

        return 0;
    }

    return 0;
}

int _decode(AVCodecContext *enc_ctx, AVFrame **frame, AVPacket **pkt) {
    int err;

    err = avcodec_send_packet(enc_ctx, *pkt);
    if (err < 0) {
        return ERR_PACKET_DECODE;
    }

    while (err >= 0) {
        err = avcodec_receive_frame(enc_ctx, *frame);
        if (err == AVERROR(EAGAIN) || err == AVERROR_EOF)
            return 0;
        else if (err < 0) {
            return ERR_DECODING;
        }

        return 0;
    }

    return 0;
}
