#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <stdio.h>

#include "device.h"
#include <SDL3/SDL_main.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include "shared/net.h"
#include "shared/protocol.h"
#include "client_state.h"
#include "codec.h"
#include "picture_widget.h"
#include "user_list.h"

// Nuklear
#define NK_INCLUDE_COMMAND_USERDATA
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_INCLUDE_STANDARD_IO
#define NK_IMPLEMENTATION
#include "nuklear.h"

#define NK_SDL3_RENDERER_IMPLEMENTATION
#include "nuklear_sdl3_renderer.h"

// Nuklear
static struct nk_context *nk_ctx = NULL;
static struct nk_font_atlas *nk_atlas = NULL;

// UI состояние
static char server_ip[64] = "127.0.0.1";
static char server_ip_buffer[64] = "127.0.0.1";
static int server_port = 44323;

// Список пользователей (для UI)
static struct nk_list_view user_list_view;

static char username[USERNAME_SIZE] = "";

/* We will use this renderer to draw into this window every frame. */
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Camera *camera = NULL;

static PictureWidget *localCameraWidget = NULL;
static PictureWidget *remoteCameraWidget = NULL;

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
    uint8_t* data = calloc(PROTOCOL_NEW_CONNECTION_SIZE, 1);
    data[0] = PROTOCOL_NEW_CONNECTION;
    memcpy(&data[1], username, USERNAME_SIZE);
    send_to_bin(udpClient, addr, data, PROTOCOL_NEW_CONNECTION_SIZE);
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
    put_uint16_i(data, 1, (uint16_t)connectionIdx);
    put_uint32_i(data, 3, (uint32_t)pkt->size);

    frameId++;
    put_uint32_i(data, 14, frameId);

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
        put_uint32_i(data, 7, dataSize);
        put_uint16_i(data, 12, chunkNumber);
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
    // [5] data [<512B]
    uint8_t* data = calloc(FRAME_AUDIO_PACKET_SIZE, 1);
    data[0] = PROTOCOL_FRAME_AUDIO;
    put_uint16_i(data, 1, (uint16_t)connectionIdx);
    put_uint16_i(data, 3, (uint16_t)pkt->size);
    memcpy(&data[5], pkt->data, pkt->size);
    send_to_bin(udpClient, addr, data, FRAME_AUDIO_PACKET_SIZE);
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
        memcpy(dataPtr, &resData[18], dataSize);
        packet = NULL;

        if (resData[11] == FRAME_FLAG_EOF) {
            DecodeVideo(codec);
            AVFrame* decodedCameraFrame = av_frame_clone(codec->VideoDecodeOutFrame);
            if (picture_widget_is_initialized(remoteCameraWidget)) {
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

    if (resData[0] == PROTOCOL_USER_LIST) {
        uint16_t size = get_uint16_i(resData, 1);
        uint16_t offset = get_uint16_i(resData, 3);
        uint16_t count = get_uint16_i(resData, 5);
        SDL_Log("User list received.");
        SDL_Log("List size: %d", size);

        if (!state->users) {
            state->users = user_list_init(size);
        } else if (state->users->size < size) {
            user_list_free(state->users);
            state->users = user_list_init(size);
        }

        uint8_t* listEntityPtr = &resData[7];
        uint16_t id = 0;
        for (int i = 0; i < count; i++) {
            id = get_uint16_i(listEntityPtr, i * USERNAME_ENTRY_SIZE);
            if (!state->users->username[id]) {
                state->users->username[id] = calloc(USERNAME_SIZE, 1);
            }
            memcpy(state->users->username[id], &listEntityPtr[i * USERNAME_ENTRY_SIZE + 2], USERNAME_SIZE);
        }

        for (int i = 0; i < state->users->size; i++) {
            if (state->users->username[i]) {
                SDL_Log("User [%d] : %s", i, state->users->username[i]);
            } else {
                SDL_Log("User [%d] : NULL", i);
            }
        }
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

    SDL_SetAppMetadata("BSUIR Еремеев диплом", "1.0", "com.example.bsuir-eremeev-diploma");

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

    localCameraWidget = picture_widget_create(370, 0, 360, 200, 640, 360, SDL_PIXELFORMAT_NV12);
    remoteCameraWidget = picture_widget_create(370, 320, 360, 200, 640, 360, SDL_PIXELFORMAT_NV12);

    if (!localCameraWidget || !remoteCameraWidget) {
        SDL_Log("Failed to create picture widgets");
        return SDL_APP_FAILURE;
    }

    codec = InitCodec();

    pxls = calloc(3110400, 1);

    udpClient = make_client();
    serverAddr = address_with_port("127.0.0.1", 44323);
    // serverAddr = address_with_port("46.243.183.18", 44323);

    // requestConnectionIdx(serverAddr);
    // uint8_t* readBuf = calloc(1, READ_BUFFER_SIZE);
    // recv(udpClient, readBuf, READ_BUFFER_SIZE, 0);
    // connectionIdx = get_uint16_i(readBuf, 1);
    // printf("OPT code: %d\n", readBuf[0]);
    // printf("Connection Idx: %d\n", *(uint16_t*)(&readBuf[1]));
    // printf("Connection Idx: %d\n", connectionIdx);
    // printf("Meta string: %s\n", &readBuf[3]);

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
    ((ClientState*)(*appstate))->mic_on = 0;
    ((ClientState*)(*appstate))->next_frame_ready = 0;
    ((ClientState*)(*appstate))->show_settings = 0;
    ((ClientState*)(*appstate))->show_users_list = 0;
    ((ClientState*)(*appstate))->username_invalid = 0;
    ((ClientState*)(*appstate))->on_call = 0;
    ((ClientState*)(*appstate))->users = NULL;

    netThread = SDL_CreateThread(serverListen, "ServerListen", *appstate);

    // Инициализация Nuklear
    struct nk_allocator allocator = nk_sdl_allocator();
    nk_ctx = nk_sdl_init(window, renderer, allocator);
    nk_ctx->style.text.color = nk_rgb(240, 240, 240);
    nk_ctx->style.edit.text_active = nk_rgb(240, 240, 240);
    nk_ctx->style.edit.text_normal = nk_rgb(240, 240, 240);
    nk_ctx->style.edit.text_hover = nk_rgb(240, 240, 240);
    nk_ctx->style.button.text_normal = nk_rgb(240, 240, 240);
    nk_ctx->style.button.text_hover = nk_rgb(250, 250, 250);

    // Настройка шрифта с кириллицей (Noto Sans)
    nk_atlas = nk_sdl_font_stash_begin(nk_ctx);

    // Загрузка шрифта Noto Sans с кириллицей
    struct nk_font_config config = nk_font_config(0);
    config.range = nk_font_cyrillic_glyph_ranges();
    struct nk_font *font = nk_font_atlas_add_from_file(
        nk_atlas,
        "NotoSans-Regular.ttf",  // Путь к файлу шрифта
        16,                       // Размер шрифта
        &config
    );
    nk_sdl_font_stash_end(nk_ctx);

    // Проверка успешности загрузки шрифта
    if (!font) {
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "Не удалось загрузить шрифт NotoSans-Regular.ttf");
        // Fallback на дефолтный шрифт
        nk_atlas = nk_sdl_font_stash_begin(nk_ctx);
        font = nk_font_atlas_add_default(nk_atlas, 16, &config);
        nk_sdl_font_stash_end(nk_ctx);
    }

    // Устанавливаем шрифт
    nk_style_set_font(nk_ctx, &font->handle);

    // Начать обработку ввода первого кадра
    nk_input_begin(nk_ctx);

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    ClientState* state = appstate;

    // Передать событие в Nuklear
    nk_sdl_handle_event(nk_ctx, event);

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
                    SDL_Log("Камера включена");
                } else {
                    SDL_CloseCamera(camera);
                    SDL_Log("Камера выключена");
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

    // Завершить обработку ввода предыдущего кадра
    nk_input_end(nk_ctx);

    if (isCameraReady && state->camera_on) {
        SDL_Surface *frameAc = SDL_AcquireCameraFrame(camera, &timestampNS);
        SDL_Surface *frame = SDL_ScaleSurface(frameAc, 640, 360, SDL_SCALEMODE_PIXELART);

        if (frame != NULL) {
            unsigned char* px = frame->pixels;

            // Обновление локального виджета камеры
            picture_widget_update(localCameraWidget, renderer, frame);

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
            if (state->on_call && avp && avp->pts >= 0) {
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
        if (state->on_call && audioPkt) {
            sendAudioFramePacket(serverAddr, audioPkt);
        }
        av_packet_free(&audioPkt);
    }

    if (state->next_frame_ready) {
        picture_widget_update_pixels(remoteCameraWidget, renderer, pxls, 640);
        state->next_frame_ready = 0;
    }

    // Рендеринг: очистка экрана
    SDL_SetRenderDrawColor(renderer, 0x99, 0x99, 0x99, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);

    // Отрисовка видео-виджетов (задний план)
    if (state->camera_on) {
        picture_widget_render(localCameraWidget, renderer);
        picture_widget_render(remoteCameraWidget, renderer);
    }

    // Nuklear UI (передний план)
    if (nk_begin(nk_ctx, "SettingsMenu", nk_rect(0, 0, 200, 35),
                 NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)) {
        nk_layout_row_dynamic(nk_ctx, 25, 2);
        if (nk_button_label(nk_ctx, "Настройки")) {
            state->show_settings = 1;
        }
        if (nk_button_label(nk_ctx, "Пользователи")) {
            state->show_users_list = 1;
        }
        nk_end(nk_ctx);
    }
    // Создать Setting UI окно
    if (state->show_settings && nk_begin(nk_ctx, "Настройки", nk_rect(0, 40, 200, 380),
                 NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR)) {

        // Текстовое поле для IP-адреса
        nk_layout_row_dynamic(nk_ctx, 30, 1);
        nk_label(nk_ctx, "IP сервера:", NK_TEXT_LEFT);

        nk_edit_string_zero_terminated(nk_ctx, NK_EDIT_FIELD,
                                       server_ip_buffer, sizeof(server_ip_buffer), NULL);

        // Обновить server_ip при изменении
        if (server_ip_buffer[0] != '\0') {
            strncpy(server_ip, server_ip_buffer, sizeof(server_ip));
        }

        // Поле для порта
        nk_layout_row_dynamic(nk_ctx, 30, 1);
        nk_label(nk_ctx, "Порт:", NK_TEXT_LEFT);
        nk_property_int(nk_ctx, "#", 1024, &server_port, 65535, 1, 1);

        nk_label(nk_ctx, "Имя пользователя:", NK_TEXT_LEFT);
        nk_edit_string_zero_terminated(nk_ctx, NK_EDIT_FIELD, username, sizeof(username), NULL);

        if (state->username_invalid) {
            nk_label(nk_ctx, "Введите имя пользователя.", NK_TEXT_LEFT);
        } else {
            // Разделитель
            nk_layout_row_dynamic(nk_ctx, 20, 1);
            nk_spacer(nk_ctx);
        }

        // Кнопка подключения
        nk_layout_row_dynamic(nk_ctx, 30, 1);
        if (nk_button_label(nk_ctx, "Подключиться")) {
            if (strlen(username) < 1) {
                state->username_invalid = 1;
            } else {
                state->username_invalid = 0;
                // state->show_settings = 0;
                // Логика подключения
                requestConnectionIdx(serverAddr);
            }

        }

        nk_layout_row_dynamic(nk_ctx, 10, 1);
        nk_spacer(nk_ctx);

        nk_layout_row_dynamic(nk_ctx, 30, 1);
        if (nk_button_label(nk_ctx, "Закрыть")) {
            state->show_settings = 0;
        }

        // Чекбокс для камеры
        // nk_layout_row_dynamic(nk_ctx, 30, 1);
        // nk_checkbox_label(nk_ctx, "Камера", &state->camera_on);

        // Чекбокс для микрофона
        // nk_layout_row_dynamic(nk_ctx, 30, 1);
        // nk_checkbox_label(nk_ctx, "Микрофон", &state->mic_on);

        nk_end(nk_ctx);
    }

    // Отдельное окно списка пользователей
    if (state->show_users_list && nk_begin(nk_ctx, "Пользователи в сети", nk_rect(100, 40, 250, 370),
                 NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR)) {

        nk_layout_row_dynamic(nk_ctx, 230, 1);

        if (state->users && state->users->size > 0) {
            // Список с прокруткой через nk_list_view
            int list_begin = nk_list_view_begin(nk_ctx, &user_list_view, "##users", NK_WINDOW_BORDER, 30, state->users->size);

            nk_layout_row_dynamic(nk_ctx, 25, 1);
            for (int i = user_list_view.begin; i < user_list_view.end; i++) {
                if (state->users->username[i]) {
                    if (nk_button_label(nk_ctx, state->users->username[i])) {
                        SDL_Log("Выбран пользователь: %s", state->users->username[i]);
                    }
                }
            }
            nk_list_view_end(&user_list_view);
        } else {
            nk_layout_row_dynamic(nk_ctx, 30, 1);
            nk_label(nk_ctx, "Нет активных пользователей", NK_TEXT_CENTERED);
        }

        // Кнопка закрытия окна
        nk_layout_row_dynamic(nk_ctx, 10, 1);
        nk_spacer(nk_ctx);
        nk_layout_row_dynamic(nk_ctx, 30, 1);
        if (nk_button_label(nk_ctx, "Закрыть")) {
            state->show_users_list = 0;
        }

        nk_end(nk_ctx);
    }

    // Обновить состояние текстового ввода
    nk_sdl_update_TextInput(nk_ctx);

    nk_sdl_render(nk_ctx, NK_ANTI_ALIASING_ON);

    SDL_RenderPresent(renderer);

    // Начать обработку ввода следующего кадра
    nk_input_begin(nk_ctx);

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    // Завершить обработку ввода последнего кадра
    if (nk_ctx) {
        nk_input_end(nk_ctx);
    }

    // Очистка Nuklear
    if (nk_ctx) {
        nk_sdl_shutdown(nk_ctx);
        nk_ctx = NULL;
    }

    // Очистка списка пользователей
    if (appstate) {
        ClientState* state = (ClientState*)appstate;
        if (state->users) {
            user_list_free(state->users);
        }
    }

    // Уничтожение виджетов
    picture_widget_destroy(localCameraWidget);
    picture_widget_destroy(remoteCameraWidget);

    SDL_CloseCamera(camera);
    SDL_CloseAudioDevice(recDeviceID);
    free(pxls);
    SDL_Quit();
    FreeCodec(codec);
    /* SDL will clean up the window/renderer for us. */
}
