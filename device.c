#include "device.h"

SDL_Camera* device_open_camera() {
    int devcount = 0;
    SDL_CameraID* devices = SDL_GetCameras(&devcount);
    if (devices == NULL) {
        SDL_Log("Couldn't enumerate camera devices: %s", SDL_GetError());
        return NULL;
    } else if (devcount == 0) {
        SDL_Log("Couldn't find any camera devices! Please connect a camera and try again.");
        return NULL;
    }

    SDL_Camera* camera = SDL_OpenCamera(devices[0], NULL);  // just take the first thing we see in any format it wants.
    SDL_free(devices);
    if (camera == NULL) {
        SDL_Log("Couldn't open camera: %s", SDL_GetError());
        return NULL;
    }

    return camera;
}
