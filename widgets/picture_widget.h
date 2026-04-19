#ifndef __DIPLOMA_PICTURE_WIDGET_H
#define __DIPLOMA_PICTURE_WIDGET_H

#include <SDL3/SDL.h>

/**
 * @brief Структура виджета для отображения видеокадров
 */
typedef struct PictureWidget {
    SDL_Texture* texture;        /**< Текстура с видеокадром */
    SDL_FRect* rect;             /**< Позиция и размер виджета на экране */
    SDL_PixelFormat format;      /**< Формат пикселей текстуры */
    int texture_width;           /**< Ширина текстуры (разрешение изображения) */
    int texture_height;          /**< Высота текстуры (разрешение изображения) */
    SDL_FlipMode flip_mode;      /**< Режим отражения */
    int is_initialized;          /**< Флаг инициализации */
} PictureWidget;

/**
 * @brief Создаёт новый виджет для отображения видеокадров
 *
 * @param x Позиция X на экране
 * @param y Позиция Y на экране
 * @param screen_w Ширина виджета на экране
 * @param screen_h Высота виджета на экране
 * @param texture_w Ширина текстуры (разрешение изображения)
 * @param texture_h Высота текстуры (разрешение изображения)
 * @param format Формат пикселей для текстуры
 * @return PictureWidget* Указатель на созданный виджет или NULL при ошибке
 */
PictureWidget* picture_widget_create(float x, float y,
                                      float screen_w, float screen_h,
                                      int texture_w, int texture_h,
                                      SDL_PixelFormat format);

/**
 * @brief Уничтожает виджет и освобождает ресурсы
 *
 * @param widget Указатель на виджет
 */
void picture_widget_destroy(PictureWidget* widget);

/**
 * @brief Обновляет текстуру виджета данными из SDL_Surface
 *
 * @param widget Указатель на виджет
 * @param renderer Указатель на рендерер SDL
 * @param surface Указатель на SDL_Surface с данными кадра
 * @return int 0 при успехе, отрицательное значение при ошибке
 */
int picture_widget_update(PictureWidget* widget, SDL_Renderer* renderer,
                          SDL_Surface* surface);

/**
 * @brief Обновляет текстуру виджета из сырых данных пикселей
 *
 * @param widget Указатель на виджет
 * @param renderer Указатель на рендерер SDL
 * @param pixels Указатель на данные пикселей
 * @param pitch Шаг (pitch) данных пикселей в байтах
 * @return int 0 при успехе, отрицательное значение при ошибке
 */
int picture_widget_update_pixels(PictureWidget* widget, SDL_Renderer* renderer,
                                  const void* pixels, int pitch);

/**
 * @brief Отрисовывает виджет на экране
 *
 * @param widget Указатель на виджет
 * @param renderer Указатель на рендерер SDL
 */
void picture_widget_render(PictureWidget* widget, SDL_Renderer* renderer);

/**
 * @brief Устанавливает позицию и размер виджета
 *
 * @param widget Указатель на виджет
 * @param x Позиция X на экране
 * @param y Позиция Y на экране
 * @param w Ширина виджета
 * @param h Высота виджета
 */
void picture_widget_set_position(PictureWidget* widget,
                                  float x, float y,
                                  float w, float h);

/**
 * @brief Устанавливает режим отражения виджета
 *
 * @param widget Указатель на виджет
 * @param flip_mode Режим отражения (SDL_FLIP_NONE, SDL_FLIP_HORIZONTAL, SDL_FLIP_VERTICAL)
 */
void picture_widget_set_flip(PictureWidget* widget, SDL_FlipMode flip_mode);

/**
 * @brief Проверяет, инициализирован ли виджет
 *
 * @param widget Указатель на виджет
 * @return int 1 если инициализирован, 0 если нет
 */
int picture_widget_is_initialized(PictureWidget* widget);

#endif // PICTURE_WIDGET_H
