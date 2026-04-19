#include "picture_widget.h"
#include <stdlib.h>

PictureWidget* picture_widget_create(float x, float y,
                                      float screen_w, float screen_h,
                                      int texture_w, int texture_h,
                                      SDL_PixelFormat format) {
    PictureWidget* widget = (PictureWidget*)calloc(1, sizeof(PictureWidget));
    if (!widget) {
        return NULL;
    }

    // Инициализация полей структуры
    widget->format = format;
    widget->texture_width = texture_w;
    widget->texture_height = texture_h;
    widget->flip_mode = SDL_FLIP_HORIZONTAL;  // По умолчанию отражаем по горизонтали
    widget->is_initialized = 0;

    // Создание прямоугольника для позиции и размера на экране
    widget->rect = (SDL_FRect*)malloc(sizeof(SDL_FRect));
    if (!widget->rect) {
        free(widget);
        return NULL;
    }
    widget->rect->x = x;
    widget->rect->y = y;
    widget->rect->w = screen_w;
    widget->rect->h = screen_h;

    // Текстура будет создана при первом обновлении с известными размерами
    widget->texture = NULL;

    widget->is_initialized = 1;
    return widget;
}

void picture_widget_destroy(PictureWidget* widget) {
    if (!widget) {
        return;
    }

    if (widget->texture) {
        SDL_DestroyTexture(widget->texture);
        widget->texture = NULL;
    }

    if (widget->rect) {
        free(widget->rect);
        widget->rect = NULL;
    }

    widget->is_initialized = 0;
    free(widget);
}

int picture_widget_update(PictureWidget* widget, SDL_Renderer* renderer,
                          SDL_Surface* surface) {
    if (!widget || !widget->is_initialized || !surface || !renderer) {
        return -1;
    }

    // Создание или пересоздание текстуры, если размеры изменились
    if (!widget->texture ||
        widget->texture_width != surface->w ||
        widget->texture_height != surface->h) {

        // Удаляем старую текстуру, если она существует
        if (widget->texture) {
            SDL_DestroyTexture(widget->texture);
            widget->texture = NULL;
        }

        // Обновляем размеры текстуры в структуре
        widget->texture_width = surface->w;
        widget->texture_height = surface->h;
        widget->format = surface->format;

        // Создаём новую текстуру с размерами из SDL_Surface
        widget->texture = SDL_CreateTexture(
            renderer,
            widget->format,
            SDL_TEXTUREACCESS_STREAMING,
            widget->texture_width,
            widget->texture_height
        );
        if (!widget->texture) {
            return -1;
        }
    }

    // Обновление текстуры данными из SDL_Surface
    if (!SDL_UpdateTexture(widget->texture, NULL, surface->pixels, surface->pitch)) {
        return -1;
    }

    return 0;
}

int picture_widget_update_pixels(PictureWidget* widget, SDL_Renderer* renderer,
                                  const void* pixels, int pitch) {
    if (!widget || !widget->is_initialized || !pixels || !renderer) {
        return -1;
    }

    // Создание текстуры при первом обновлении, если она ещё не создана
    if (!widget->texture) {
        widget->texture = SDL_CreateTexture(
            renderer,
            widget->format,
            SDL_TEXTUREACCESS_STREAMING,
            widget->texture_width,
            widget->texture_height
        );
        if (!widget->texture) {
            return -1;
        }
    }

    // Обновление текстуры сырыми данными пикселей
    if (!SDL_UpdateTexture(widget->texture, NULL, pixels, pitch)) {
        return -1;
    }

    return 0;
}

void picture_widget_render(PictureWidget* widget, SDL_Renderer* renderer) {
    if (!widget || !widget->is_initialized || !widget->texture || !renderer) {
        return;
    }

    // Отрисовка текстуры с учётом отражения
    SDL_RenderTextureRotated(
        renderer,
        widget->texture,
        NULL,                           // Весь исходный прямоугольник текстуры
        widget->rect,                   // Позиция и размер на экране
        0.0f,                           // Угол поворота (всегда 0)
        NULL,                           // Центр поворота (NULL - центр текстуры)
        widget->flip_mode               // Режим отражения
    );
}

void picture_widget_set_position(PictureWidget* widget,
                                  float x, float y,
                                  float w, float h) {
    if (!widget || !widget->rect) {
        return;
    }

    widget->rect->x = x;
    widget->rect->y = y;
    widget->rect->w = w;
    widget->rect->h = h;
}

void picture_widget_set_flip(PictureWidget* widget, SDL_FlipMode flip_mode) {
    if (!widget) {
        return;
    }
    widget->flip_mode = flip_mode;
}

int picture_widget_is_initialized(PictureWidget* widget) {
    if (!widget) {
        return 0;
    }
    return widget->is_initialized;
}
