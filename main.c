#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <stdio.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include "net.h"
#include "protocol.h"
#include "client_state.h"

/* We will use this renderer to draw into this window every frame. */
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Camera *camera = NULL;
static SDL_Texture *texture = NULL;
static SDL_FRect *targetRect = NULL;

static SDL_Texture *texture2 = NULL;
static SDL_FRect *targetRect2 = NULL;

static SDL_AudioDeviceID recDeviceID, playDeviceID;
static SDL_AudioStream* recStream;
static SDL_AudioStream* playStream;

// Coder
static const AVCodec *codec = NULL;
static const AVCodec *codecDecoder = NULL;
static AVCodecContext *codecContext = NULL;
static AVCodecContext *decoderContext = NULL;
static AVFrame *rawCameraFrame = NULL;
static AVPacket *cameraPacket = NULL;
static int currentFramePts = 0;
static char isCameraReady = 0;
static uint8_t *pxls = NULL;

static int udpClient;
static net_sock_addr* serverAddr;
static int connectionIdx = 0;

static AVFrame *bufFrame = NULL;
AVFrame* decode(AVCodecContext *enc_ctx, AVPacket *pkt) {
    int err;
    if (!bufFrame) {
        bufFrame = av_frame_alloc();
    }
    AVFrame* resultFrame = NULL;

    err = avcodec_send_packet(enc_ctx, pkt);
    if (err < 0) {
        SDL_Log("Error sending a packet for decoding: %s", av_err2str(err));
        return NULL;
    }

    while (err >= 0) {
        err = avcodec_receive_frame(enc_ctx, bufFrame);
        if (err == AVERROR(EAGAIN) || err == AVERROR_EOF)
            return NULL;
        else if (err < 0) {
            SDL_Log("Error during decoding");
            return NULL;
        }

        AVFrame* resultFrame = av_frame_clone(bufFrame);
        av_frame_unref(bufFrame);
        return resultFrame;
    }

    return NULL;
}

AVPacket* encode(AVCodecContext *enc_ctx, AVFrame *frame, AVPacket *pkt) {
    int err;

    err = avcodec_send_frame(enc_ctx, frame);
    if (err < 0) {
        SDL_Log("Error sending a frame for encoding");
        return NULL;
    }

    while (err >= 0) {
        err = avcodec_receive_packet(enc_ctx, pkt);
        if (err == AVERROR(EAGAIN) || err == AVERROR_EOF)
            return NULL;
        else if (err < 0) {
            SDL_Log("Error during encoding");
            return NULL;
        }

        // SDL_Log("Write packet (size=%5d)", pkt->size);
        return av_packet_clone(pkt);
    }

    return NULL;
}

void requestConnectionIdx(net_sock_addr* addr) {
    uint8_t data[1] = {PROTOCOL_NEW_CONNECTION};
    send_to_bin(udpClient, addr, data, 1);
}

void sendFramePacket(net_sock_addr* addr, AVPacket* pkt) {
    // [0] OPT code [8bit]
    // [1] connection idx [16bit]
    // [3] frame size [32bit]
    // [7] data size [32bit]
    // [11] frame EOF flag [8bit]
    // [12] data [1024B]
    uint8_t* data = malloc(READ_BUFFER_SIZE);
    data[0] = PROTOCOL_FRAME;
    *(uint16_t*)(&data[1]) = connectionIdx;
    *(uint32_t*)(&data[3]) = pkt->size;

    int i = 0;
    while (i < pkt->size) {
        uint32_t dataSize = FRAME_CHUNK;
        uint8_t oef_flag = 0;
        if (i + FRAME_CHUNK > pkt->size) {
            dataSize = (uint32_t)(pkt->size - i);
            oef_flag = FRAME_FLAG_EOF;
        }
        *(uint32_t*)(&data[7]) = dataSize;
        data[11] = oef_flag;
        memcpy(&data[12], &pkt->data[i], dataSize);
        i += FRAME_CHUNK;
        send_to_bin(udpClient, addr, data, READ_BUFFER_SIZE);
    }
    free(data);
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    SDL_CameraID *devices = NULL;
    int devcount = 0;
    int err;

    SDL_SetAppMetadata("Example Camera Read and Draw", "1.0", "com.example.camera-read-and-draw");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_CAMERA | SDL_INIT_AUDIO)) {
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

    AVDictionary* codecOpts = NULL;
    av_dict_set(&codecOpts, "threads", "1", 0);
    av_dict_set(&codecOpts, "preset", "ultrafast", 0);
    av_dict_set(&codecOpts, "tune", "zerolatency", 0);
    err = avcodec_open2(codecContext, codec, &codecOpts);
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

    cameraPacket = av_packet_alloc();
    if (!cameraPacket) {
        SDL_Log("Could not allocate camera packet");
        return SDL_APP_FAILURE;
    }

    pxls = calloc(3110400, 1);

    udpClient = make_client();
    serverAddr = address_with_port("127.0.0.1", 44323);
    requestConnectionIdx(serverAddr);
    uint8_t* readBuf = calloc(1, READ_BUFFER_SIZE);
    recv(udpClient, readBuf, READ_BUFFER_SIZE, 0);
    printf("OPT code: %d\n", readBuf[0]);
    printf("Connection Idx: %d\n", *(uint16_t*)(&readBuf[1]));
    printf("Meta string: %s\n", &readBuf[3]);

    SDL_AudioDeviceID* recAudioDevices = SDL_GetAudioRecordingDevices(NULL);
    if (!recAudioDevices) {
        SDL_Log("SDL_GetAudioRecordingDevices error: %s", SDL_GetError());
    }
    recDeviceID = recAudioDevices[0];
    SDL_free(recAudioDevices);
    SDL_AudioSpec recSpec;
    if (!SDL_GetAudioDeviceFormat(recDeviceID, &recSpec, NULL)) {
        SDL_Log("SDL_GetAudioDeviceFormat error: %s", SDL_GetError());
    }
    recSpec.channels = 2;
    recStream = SDL_OpenAudioDeviceStream(recDeviceID, &recSpec, NULL, NULL);
    if (!recStream) {
        SDL_Log("SDL_OpenAudioDeviceStream error: %s", SDL_GetError());
    }

    SDL_AudioDeviceID* playAudioDevices = SDL_GetAudioPlaybackDevices(NULL);
    if (!playAudioDevices) {
        SDL_Log("SDL_GetAudioPlaybackDevices error: %s", SDL_GetError());
    }
    playDeviceID = playAudioDevices[0];
    SDL_free(playAudioDevices);
    SDL_AudioSpec playSpec;
    if (!SDL_GetAudioDeviceFormat(playDeviceID, &playSpec, NULL)) {
        SDL_Log("SDL_GetAudioDeviceFormat error: %s", SDL_GetError());
    }
    playStream = SDL_OpenAudioDeviceStream(playDeviceID, &recSpec, NULL, NULL);
    if (!playStream) {
        SDL_Log("SDL_OpenAudioDeviceStream error: %s", SDL_GetError());
    }
    SDL_ResumeAudioStreamDevice(playStream);

    *appstate = (ClientState*)malloc(sizeof(ClientState));

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    ClientState* state = appstate;

    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;  /* end the program, reporting success to the OS. */
    } else if (event->type == SDL_EVENT_CAMERA_DEVICE_APPROVED) {
        SDL_Log("Camera use approved by user!");
        isCameraReady = 1;
    } else if (event->type == SDL_EVENT_CAMERA_DEVICE_DENIED) {
        SDL_Log("Camera use denied by user!");
        return SDL_APP_FAILURE;
    } else if (event->type == SDL_EVENT_KEY_DOWN) {
        switch (event->key.scancode)
        {
            case SDL_SCANCODE_ESCAPE:
                return SDL_APP_SUCCESS;
            case SDL_SCANCODE_SPACE:
                state->mic_on = !state->mic_on;
                state->mic_on ? SDL_ResumeAudioStreamDevice(recStream) : SDL_PauseAudioStreamDevice(recStream) ;
                break;
            default:
                break;
        }
        return SDL_APP_CONTINUE;
    }

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    ClientState* state = appstate;
    Uint64 timestampNS = 0;
    SDL_Surface *frame = SDL_AcquireCameraFrame(camera, &timestampNS);

    if (frame != NULL) {
        unsigned char* px = frame->pixels;
        // SDL_Log("%u\t%u\t%u", px[2073599], px[2073600], px[3110400]);
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

            AVPacket* avp = encode(codecContext, rawCameraFrame, cameraPacket);
            if (avp) {
                sendFramePacket(serverAddr, avp);
                av_packet_free(&avp);
            }

            if (state->mic_on) {
                void* recordingBuffer = calloc(8192, 1);
                int sizetest = SDL_GetAudioStreamData(recStream, recordingBuffer, 8192);
                if (sizetest < 0) {
                    SDL_Log("SDL_GetAudioStreamData error: %s", SDL_GetError());
                }
                SDL_PutAudioStreamData(playStream, recordingBuffer, sizetest);
                free(recordingBuffer);
            }
        }

        // Do not call SDL_DestroySurface() on the returned surface!
        // It must be given back to the camera subsystem with SDL_ReleaseCameraFrame!
        SDL_ReleaseCameraFrame(camera, frame);
    }

    if (recv_packet_dontwait(udpClient) > 0) {
        uint8_t* resData = get_read_buffer();
        if (resData[0] == PROTOCOL_FRAME) {
            AVPacket* packet = av_packet_alloc();
            av_packet_make_writable(packet);
            uint32_t frameSize = get_uint32_i(resData, 3);
            uint32_t dataSize = get_uint32_i(resData, 7);
            packet->data = malloc(frameSize);
            packet->size = frameSize;
            uint8_t* dataPtr = packet->data;

            memcpy(dataPtr, &resData[12], dataSize);
            dataPtr += dataSize;
            while (resData[11] != FRAME_FLAG_EOF) {
                recv_packet(udpClient, NULL);
                resData = get_read_buffer();
                dataSize = get_uint32_i(resData, 7);
                memcpy(dataPtr, &resData[12], dataSize);
                dataPtr += dataSize;
            }

            AVFrame* decodedCameraFrame = decode(decoderContext, packet);

            if (texture2) {
                if (decodedCameraFrame && decodedCameraFrame->buf[0]) {
                    memcpy(pxls, decodedCameraFrame->buf[0]->data, 2073600);
                    memcpy(&pxls[2073600], decodedCameraFrame->buf[1]->data, 518400);
                    memcpy(&pxls[2592000], decodedCameraFrame->buf[2]->data, 518400);
                    SDL_UpdateTexture(texture2, NULL, pxls, decodedCameraFrame->linesize[0]);
                    av_frame_free(&decodedCameraFrame);
                }
            }

            free(packet->data);
            av_packet_free(&packet);
        }
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
    SDL_CloseAudioDevice(recDeviceID);
    SDL_Quit();

    avcodec_free_context(&codecContext);
    av_frame_free(&rawCameraFrame);
    av_packet_free(&cameraPacket);
    /* SDL will clean up the window/renderer for us. */
}
