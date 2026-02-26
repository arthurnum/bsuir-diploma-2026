#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <stdio.h>

#include "device.h"
#include <SDL3/SDL_main.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include <poll.h>

#include "shared/net.h"
#include "shared/protocol.h"
#include "client_state.h"
#include "codec.h"

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

static const SDL_Thread* netThread = NULL;

// Coder
static Codec* codec = NULL;

static int currentFramePts = 0;
static char isCameraReady = 0;
static uint8_t *pxls = NULL;

static int udpClient;
static net_sock_addr* serverAddr;
static int connectionIdx = 0;

static uint32_t frameId = 0;

void requestConnectionIdx(net_sock_addr* addr) {
    uint8_t data[1] = {PROTOCOL_NEW_CONNECTION};
    send_to_bin(udpClient, addr, data, 1);
}

void sendFramePacket(net_sock_addr* addr, AVPacket* pkt) {
    // [0] OPT code [8bit]
    // [1] connection idx [16bit]
    // [3] frame size [32bit]
    // [7] data size [32bit]
    // [11] frame flag [8bit]
    // [12] chunk number [16bit]
    // [14] frame ID [32bit]
    // [18] data [512B]
    uint8_t isKeyFrame = pkt->flags & AV_PKT_FLAG_KEY;
    uint8_t* data = malloc(READ_BUFFER_SIZE);
    data[0] = PROTOCOL_FRAME;
    *(uint16_t*)(&data[1]) = connectionIdx;
    *(uint32_t*)(&data[3]) = pkt->size;

    frameId++;
    *(uint32_t*)(&data[14]) = frameId;

    uint16_t chunkNumber = 0;
    int i = 0;

    // packet sequence
    while (i < pkt->size) {
        data[11] = 0;
        uint32_t dataSize = FRAME_CHUNK;
        if (i + FRAME_CHUNK > pkt->size) {
            dataSize = (uint32_t)(pkt->size - i);
            data[11] = FRAME_FLAG_EOF;
        }
        *(uint32_t*)(&data[7]) = dataSize;
        *(uint16_t*)(&data[12]) = chunkNumber;
        chunkNumber++;
        memcpy(&data[18], &pkt->data[i], dataSize);
        i += FRAME_CHUNK;
        send_to_bin(udpClient, addr, data, FRAME_PACKET_SIZE);
    }

    free(data);
}

void sendAudioFramePacket(net_sock_addr* addr, AVPacket* pkt) {
    // [0] OPT code [8bit]
    // [1] connection idx [16bit]
    // [3] frame size [16bit]
    // [5] data [512B]
    uint8_t* data = malloc(1024);
    data[0] = PROTOCOL_FRAME_AUDIO;
    *(uint16_t*)(&data[1]) = connectionIdx;
    *(uint16_t*)(&data[3]) = pkt->size;
    memcpy(&data[5], pkt->data, pkt->size);
    send_to_bin(udpClient, addr, data, READ_BUFFER_SIZE);
    free(data);
}

void handleNetData(ClientState *state) {
    uint8_t* resData = get_read_buffer();

    if (resData[0] == PROTOCOL_FRAME) {
        AVPacket* packet = codec->VideoDecodeInPacket;
        av_packet_make_writable(packet);
        uint32_t frameSize = get_uint32_i(resData, 3);
        uint32_t dataSize = get_uint32_i(resData, 7);
        uint16_t chunkNumber = get_uint16_i(resData, 12);
        packet->size = frameSize;
        uint8_t* dataPtr = packet->data + FRAME_CHUNK * chunkNumber;
        memcpy(dataPtr, &resData[14], dataSize);
        packet = NULL;

        if (resData[11] == FRAME_FLAG_EOF) {
            DecodeVideo(codec);
            AVFrame* decodedCameraFrame = av_frame_clone(codec->VideoDecodeOutFrame);
            if (texture2) {
                if (decodedCameraFrame && decodedCameraFrame->buf[0]) {
                    // memcpy(pxls, decodedCameraFrame->buf[0]->data, 2073600);
                    // memcpy(&pxls[2073600], decodedCameraFrame->buf[1]->data, 518400);
                    // memcpy(&pxls[2592000], decodedCameraFrame->buf[2]->data, 518400);
                    memcpy(pxls, decodedCameraFrame->buf[0]->data, 230400);
                    memcpy(&pxls[230400], decodedCameraFrame->buf[1]->data, 57600);
                    memcpy(&pxls[288000], decodedCameraFrame->buf[2]->data, 57600);
                    state->next_frame_ready = 1;
                    // SDL_UpdateTexture(texture2, NULL, pxls, decodedCameraFrame->linesize[0]);
                }
            }
            av_frame_free(&decodedCameraFrame);
        }
    }

    if (resData[0] == PROTOCOL_FRAME_AUDIO) {
        AVPacket* packetAudio = codec->AudioDecodeInPacket;
        av_packet_make_writable(packetAudio);
        uint16_t frameSize = get_uint16_i(resData, 3);
        packetAudio->size = frameSize;
        memcpy(packetAudio->data, &resData[5], frameSize);
        packetAudio = NULL;

        DecodeAudio(codec);
        AVFrame* decodedAudioFrame = av_frame_clone(codec->AudioDecodeOutFrame);
        if (decodedAudioFrame && decodedAudioFrame->buf[0]) {
            SDL_PutAudioStreamData(playStream, decodedAudioFrame->buf[0]->data, 3840);
        }
        av_frame_free(&decodedAudioFrame);
    }
}

int SDLCALL serverListen(void* userData) {
    SDL_SetCurrentThreadPriority(SDL_THREAD_PRIORITY_HIGH);
    ClientState* state = (ClientState*)userData;
    while (1) {
        if (recv_packet_dontwait(udpClient) > 0) {
            handleNetData(state);
        }
    }
    return 0;
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    SDL_CameraID *devices = NULL;
    int devcount = 0;
    int err;

    SDL_SetAppMetadata("BSUIR Eremeev diploma", "1.0", "com.example.bsuir-eremeev-diploma");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_CAMERA | SDL_INIT_AUDIO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("BSUIR Eremeev diploma", 800, 480, 0, &window, &renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    camera = device_open_camera();
    if (camera == NULL) {
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

    codec = InitCodec();

    pxls = calloc(3110400, 1);

    udpClient = make_client();
    serverAddr = address_with_port("127.0.0.1", 44323);
    // serverAddr = address_with_port("46.243.183.18", 44323);
    requestConnectionIdx(serverAddr);
    uint8_t* readBuf = calloc(1, READ_BUFFER_SIZE);
    recv(udpClient, readBuf, READ_BUFFER_SIZE, 0);
    connectionIdx = get_uint16_i(readBuf, 1);
    printf("OPT code: %d\n", readBuf[0]);
    // printf("Connection Idx: %d\n", *(uint16_t*)(&readBuf[1]));
    printf("Connection Idx: %d\n", connectionIdx);
    printf("Meta string: %s\n", &readBuf[3]);

    SDL_AudioDeviceID* recAudioDevices = SDL_GetAudioRecordingDevices(NULL);
    if (!recAudioDevices) {
        SDL_Log("SDL_GetAudioRecordingDevices error: %s", SDL_GetError());
    }
    recDeviceID = recAudioDevices[0];
    SDL_free(recAudioDevices);
    SDL_AudioSpec audioSpec;
    audioSpec.channels = 1;
    audioSpec.format = SDL_AUDIO_F32;
    audioSpec.freq = 48000;
    recStream = SDL_OpenAudioDeviceStream(recDeviceID, &audioSpec, NULL, NULL);
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
    playSpec.channels = 1;
    playSpec.format = SDL_AUDIO_F32;
    playSpec.freq = 48000;
    playStream = SDL_OpenAudioDeviceStream(playDeviceID, &audioSpec, NULL, NULL);
    if (!playStream) {
        SDL_Log("SDL_OpenAudioDeviceStream error: %s", SDL_GetError());
    }
    SDL_ResumeAudioStreamDevice(playStream);

    *appstate = (ClientState*)malloc(sizeof(ClientState));
    ((ClientState*)(*appstate))->camera_on = 1;

    netThread = SDL_CreateThread(serverListen, "ServerListen", *appstate);

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
            case SDL_SCANCODE_C:
                state->camera_on = !state->camera_on;
                if (state->camera_on) {
                    camera = device_open_camera();
                } else {
                    SDL_CloseCamera(camera);
                }
            default:
                break;
        }
        return SDL_APP_CONTINUE;
    }

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    int err;
    ClientState* state = appstate;
    Uint64 timestampNS = 0;

    if (isCameraReady && state->camera_on) {
        SDL_Surface *frameAc = SDL_AcquireCameraFrame(camera, &timestampNS);
        SDL_Surface *frame = SDL_ScaleSurface(frameAc, 640, 360, SDL_SCALEMODE_PIXELART);

        if (frame != NULL) {
            unsigned char* px = frame->pixels;

            if (!texture) {
                texture = SDL_CreateTexture(renderer, frame->format, SDL_TEXTUREACCESS_STREAMING, frame->w, frame->h);
            }
            if (!texture2) {
                texture2 = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_NV12, SDL_TEXTUREACCESS_STREAMING, frame->w, frame->h);
            }

            if (texture) {
                SDL_UpdateTexture(texture, NULL, frame->pixels, frame->pitch);
            }

            AVFrame* rawCameraFrame = codec->VideoEncodeInFrame;
            rawCameraFrame->width = frame->w;
            rawCameraFrame->height = frame->h;

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

            // memcpy(rawCameraFrame->data[0], px, 2073600);
            // memcpy(rawCameraFrame->data[1], &px[2073600], 518400);
            // memcpy(rawCameraFrame->data[2], &px[2592000], 518400);
            memcpy(rawCameraFrame->data[0], px, 230400);
            memcpy(rawCameraFrame->data[1], &px[230400], 57600);
            memcpy(rawCameraFrame->data[2], &px[288000], 57600);
            rawCameraFrame = NULL;

            EncodeVideo(codec);
            AVPacket* avp = av_packet_clone(codec->VideoEncodeOutPacket);
            if (avp && avp->pts >= 0) {
                sendFramePacket(serverAddr, avp);
            }
            av_packet_free(&avp);

            // Do not call SDL_DestroySurface() on the returned surface!
            // It must be given back to the camera subsystem with SDL_ReleaseCameraFrame!
            SDL_ReleaseCameraFrame(camera, frameAc);
            SDL_DestroySurface(frame);
        }
    }

    int frameBufSize = codec->AudioEncoderCtx->frame_size * 4; // 4 bytes per sample
    if (SDL_GetAudioStreamAvailable(recStream) > frameBufSize) {
        void* recordingBuffer = calloc(4096, 1);

        int sizetest = SDL_GetAudioStreamData(recStream, recordingBuffer, frameBufSize);
        if (sizetest < 0) {
            SDL_Log("SDL_GetAudioStreamData error: %s", SDL_GetError());
        }

        AVFrame* audioFrameEncode = codec->AudioEncodeInFrame;
        if (!audioFrameEncode->buf[0]) {
            err = av_frame_get_buffer(audioFrameEncode, 0);
            if (err < 0) {
                SDL_Log("Could not allocate the audio frame data");
                return SDL_APP_FAILURE;
            }
        }
        err = av_frame_make_writable(audioFrameEncode);
        if (err < 0) {
            SDL_Log("Could not make writable audio frame");
            return SDL_APP_FAILURE;
        }

        memcpy(audioFrameEncode->data[0], recordingBuffer, sizetest);
        free(recordingBuffer);
        audioFrameEncode = NULL;

        EncodeAudio(codec);
        AVPacket* audioPkt = av_packet_clone(codec->AudioEncodeOutPacket);
        if (audioPkt) {
            sendAudioFramePacket(serverAddr, audioPkt);
        }
        av_packet_free(&audioPkt);
    }

    // struct pollfd fd;
    // int ret;
    // fd.fd = udpClient;
    // fd.events = POLLIN;
    // ret = poll(&fd, 1, 20); // timeout
    // SDL_Log("POLL socket: %d", ret);
    // switch (ret) {
    //     case -1:
    //         // Error
    //         break;
    //     case 0:
    //         // Timeout
    //         break;
    //     default:
    //         recv_packet(udpClient, NULL);
    //         break;
    // }

    if (state->next_frame_ready) {
        // SDL_UpdateTexture(texture2, NULL, pxls, decodedCameraFrame->linesize[0]);
        SDL_UpdateTexture(texture2, NULL, pxls, 640);
        state->next_frame_ready = 0;
    }

    SDL_SetRenderDrawColor(renderer, 0x99, 0x99, 0x99, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);
    if (state->camera_on) {
        if (texture) {  /* draw the latest camera frame, if available. */
            SDL_RenderTextureRotated(renderer, texture, NULL, targetRect, 0, NULL, SDL_FLIP_HORIZONTAL);
            // SDL_RenderTexture(renderer, texture, NULL, targetRect);
        }
        if (texture2) {  /* draw the latest camera frame, if available. */
            SDL_RenderTextureRotated(renderer, texture2, NULL, targetRect2, 0, NULL, SDL_FLIP_HORIZONTAL);
            // SDL_RenderTexture(renderer, texture2, NULL, targetRect2);
        }
    }
    SDL_RenderPresent(renderer);

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    SDL_CloseCamera(camera);
    SDL_DestroyTexture(texture);
    SDL_CloseAudioDevice(recDeviceID);
    SDL_Quit();
    FreeCodec(codec);
    /* SDL will clean up the window/renderer for us. */
}
