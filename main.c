#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <stdio.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

/* We will use this renderer to draw into this window every frame. */
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Camera *camera = NULL;
static SDL_Texture *texture = NULL;
static SDL_FRect *targetRect = NULL;

static SDL_Texture *texture2 = NULL;
static SDL_FRect *targetRect2 = NULL;

// Coder
static const AVCodec *codec = NULL;
static const AVCodec *codecDecoder = NULL;
static AVCodecContext *codecContext = NULL;
static AVCodecContext *decoderContext = NULL;
static AVFrame *rawCameraFrame = NULL;
static AVFrame *decodedCameraFrame = NULL;
static AVPacket *cameraPacket = NULL;
static int currentFramePts = 0;
static char isCameraReady = 0;
static u_int8_t *pxls = NULL;

static AVFrame *bufFrame = NULL;
static void decode(AVCodecContext *enc_ctx, AVFrame *frame, AVPacket *pkt) {
    int err;
    if (!bufFrame) {
        bufFrame = av_frame_alloc();
    }

    err = avcodec_send_packet(enc_ctx, pkt);
    if (err < 0) {
        SDL_Log("Error sending a packet for decoding: %s", av_err2str(err));
        return;
    }

    while (err >= 0) {
        err = avcodec_receive_frame(enc_ctx, bufFrame);
        if (err == AVERROR(EAGAIN) || err == AVERROR_EOF)
            return;
        else if (err < 0) {
            SDL_Log("Error during decoding");
            return;
        }

        decodedCameraFrame = av_frame_clone(bufFrame);
        av_frame_unref(bufFrame);
        av_packet_free(&pkt);
    }
}

static void encode(AVCodecContext *enc_ctx, AVFrame *frame, AVPacket *pkt) {
    int err;

    err = avcodec_send_frame(enc_ctx, frame);
    if (err < 0) {
        SDL_Log("Error sending a frame for encoding");
        return;
    }

    while (err >= 0) {
        err = avcodec_receive_packet(enc_ctx, pkt);
        if (err == AVERROR(EAGAIN) || err == AVERROR_EOF)
            return;
        else if (err < 0) {
            SDL_Log("Error during encoding");
            return;
        }

        SDL_Log("Write packet (size=%5d)", pkt->size);
        decode(decoderContext, decodedCameraFrame, av_packet_clone(pkt));
        av_packet_unref(pkt);
    }
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    SDL_CameraID *devices = NULL;
    int devcount = 0;
    int err;

    SDL_SetAppMetadata("Example Camera Read and Draw", "1.0", "com.example.camera-read-and-draw");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_CAMERA)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("examples/camera/read-and-draw", 800, 480, 0, &window, &renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    devices = SDL_GetCameras(&devcount);
    if (devices == NULL) {
        SDL_Log("Couldn't enumerate camera devices: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    } else if (devcount == 0) {
        SDL_Log("Couldn't find any camera devices! Please connect a camera and try again.");
        return SDL_APP_FAILURE;
    }

    camera = SDL_OpenCamera(devices[0], NULL);  // just take the first thing we see in any format it wants.
    SDL_free(devices);
    if (camera == NULL) {
        SDL_Log("Couldn't open camera: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    targetRect = SDL_malloc(sizeof(SDL_FRect));
    targetRect->x = 0;
    targetRect->y = 0;
    targetRect->w = 360;
    targetRect->h = 200;
    targetRect2 = SDL_malloc(sizeof(SDL_FRect));
    targetRect2->x = 370;
    targetRect2->y = 0;
    targetRect2->w = 360;
    targetRect2->h = 200;

    codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    codecContext = avcodec_alloc_context3(codec);
    if (!codecContext) {
        SDL_Log("Could not allocate video codec context");
        return SDL_APP_FAILURE;
    }
    codecContext->width = 1920;
    codecContext->height = 1080;
    codecContext->gop_size = 10;
    codecContext->max_b_frames = 1;
    codecContext->pix_fmt = AV_PIX_FMT_YUV420P;
    codecContext->time_base = (AVRational){1, 25};
    codecContext->framerate = (AVRational){25, 1};

    codecDecoder = avcodec_find_decoder(AV_CODEC_ID_H264);
    decoderContext = avcodec_alloc_context3(codecDecoder);
    decoderContext->width = 1920;
    decoderContext->height = 1080;
    decoderContext->pix_fmt = AV_PIX_FMT_YUV420P;

    err = avcodec_open2(codecContext, codec, NULL);
    if (err < 0) {
        SDL_Log("Could not open codec: %s", av_err2str(err));
        return SDL_APP_FAILURE;
    }

    err = avcodec_open2(decoderContext, codecDecoder, NULL);
    if (err < 0) {
        SDL_Log("Could not open decoder: %s", av_err2str(err));
        return SDL_APP_FAILURE;
    }

    rawCameraFrame = av_frame_alloc();
    rawCameraFrame->format = AV_PIX_FMT_YUV420P;
    decodedCameraFrame = av_frame_alloc();

    cameraPacket = av_packet_alloc();
    if (!cameraPacket) {
        SDL_Log("Could not allocate camera packet");
        return SDL_APP_FAILURE;
    }

    pxls = calloc(3110400, 1);

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;  /* end the program, reporting success to the OS. */
    } else if (event->type == SDL_EVENT_CAMERA_DEVICE_APPROVED) {
        SDL_Log("Camera use approved by user!");
        isCameraReady = 1;
    } else if (event->type == SDL_EVENT_CAMERA_DEVICE_DENIED) {
        SDL_Log("Camera use denied by user!");
        return SDL_APP_FAILURE;
    }
    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    Uint64 timestampNS = 0;
    SDL_Surface *frame = SDL_AcquireCameraFrame(camera, &timestampNS);

    if (frame != NULL) {
        unsigned char* px = frame->pixels;
        // SDL_Log("Frame width %u", frame->w);
        // SDL_Log("Frame height %u", frame->h);
        SDL_Log("%u\t%u\t%u", px[2073599], px[2073600], px[3110400]);
        /* Some platforms (like Emscripten) don't know _what_ the camera offers
           until the user gives permission, so we build the texture and resize
           the window when we get a first frame from the camera. */
        if (!texture) {
            // SDL_SetWindowSize(window, frame->w, frame->h);  /* Resize the window to match */
            texture = SDL_CreateTexture(renderer, frame->format, SDL_TEXTUREACCESS_STREAMING, frame->w, frame->h);
        }
        if (!texture2) {
            texture2 = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_NV12, SDL_TEXTUREACCESS_STREAMING, frame->w, frame->h);
        }

        if (texture) {
            SDL_UpdateTexture(texture, NULL, frame->pixels, frame->pitch);
        }
        if (texture2) {
            if (decodedCameraFrame->buf[0]) {
                memcpy(pxls, decodedCameraFrame->buf[0]->data, 2073600);
                memcpy(&pxls[2073600], decodedCameraFrame->buf[1]->data, 518400);
                memcpy(&pxls[2592000], decodedCameraFrame->buf[2]->data, 518400);
                SDL_UpdateTexture(texture2, NULL, pxls, decodedCameraFrame->linesize[0]);
                av_frame_unref(decodedCameraFrame);
            }
        }

        rawCameraFrame->width = frame->w;
        rawCameraFrame->height = frame->h;

        if (isCameraReady) {
            int err;
            if (!rawCameraFrame->buf[0]) {
                err = av_frame_get_buffer(rawCameraFrame, 0);
                if (err < 0) {
                    SDL_Log("Could not allocate the video frame data");
                    return SDL_APP_FAILURE;
                }
            }
            err = av_frame_make_writable(rawCameraFrame);
            if (err < 0) {
                SDL_Log("Could not make writable frame");
                return SDL_APP_FAILURE;
            }

            rawCameraFrame->pts = currentFramePts;
            currentFramePts++;

            memcpy(rawCameraFrame->data[0], px, 2073600);
            memcpy(rawCameraFrame->data[1], &px[2073600], 518400);
            memcpy(rawCameraFrame->data[2], &px[2592000], 518400);

            encode(codecContext, rawCameraFrame, cameraPacket);
        }


        // Do not call SDL_DestroySurface() on the returned surface!
        // It must be given back to the camera subsystem with SDL_ReleaseCameraFrame!
        SDL_ReleaseCameraFrame(camera, frame);
    }

    SDL_SetRenderDrawColor(renderer, 0x99, 0x99, 0x99, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);
    if (texture) {  /* draw the latest camera frame, if available. */
        SDL_RenderTextureRotated(renderer, texture, NULL, targetRect, 0, NULL, SDL_FLIP_HORIZONTAL);
        // SDL_RenderTexture(renderer, texture, NULL, targetRect);
    }
    if (texture2) {  /* draw the latest camera frame, if available. */
        SDL_RenderTextureRotated(renderer, texture2, NULL, targetRect2, 0, NULL, SDL_FLIP_HORIZONTAL);
        // SDL_RenderTexture(renderer, texture2, NULL, targetRect2);
    }
    SDL_RenderPresent(renderer);

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    SDL_CloseCamera(camera);
    SDL_DestroyTexture(texture);

    avcodec_free_context(&codecContext);
    av_frame_free(&rawCameraFrame);
    av_packet_free(&cameraPacket);
    /* SDL will clean up the window/renderer for us. */
}
