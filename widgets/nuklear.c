#include "nuklear.h"

ClientAction user_call_widget(struct nk_context *nk_ctx, ClientState *state) {
    ClientAction result = Action_Idle;
    const int w = 200;
    const int h = 130;
    float x = (state->window_width - w) / 2.0;
    float y = (state->window_height - h) / 2.0;
    if (nk_begin(nk_ctx, "Вызов", nk_rect(x, y, w, h),
                 NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR)) {

        nk_layout_row_dynamic(nk_ctx, 50, 1);
        nk_label(nk_ctx, state->users->username[state->callee_idx], NK_TEXT_CENTERED);

        nk_layout_row_dynamic(nk_ctx, 30, 1);
        if (nk_button_label(nk_ctx, "Отмена")) {
            result = Action_CallCancel;
        }
        nk_end(nk_ctx);
    }

    return result;
}

ClientAction incoming_call_widget(struct nk_context *nk_ctx, ClientState *state) {
    ClientAction result = Action_Idle;
    const int w = 200;
    const int h = 130;
    float x = (state->window_width - w) / 2.0;
    float y = (state->window_height - h) / 2.0;
    if (nk_begin(nk_ctx, "Входящий вызов", nk_rect(x, y, w, h),
                 NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR)) {

        nk_layout_row_dynamic(nk_ctx, 50, 1);
        nk_label(nk_ctx, state->users->username[state->caller_idx], NK_TEXT_CENTERED);

        nk_layout_row_dynamic(nk_ctx, 30, 2);
        if (nk_button_label(nk_ctx, "Отклонить")) {
            result = Action_IncomingCallReject;
        }
        if (nk_button_label(nk_ctx, "Принять")) {
            result = Action_IncomingCallAccept;
        }
        nk_end(nk_ctx);
    }

    return result;
}

ClientAction user_busy_widget(struct nk_context *nk_ctx, ClientState *state) {
    ClientAction result = Action_Idle;
    const int w = 200;
    const int h = 130;
    float x = (state->window_width - w) / 2.0;
    float y = (state->window_height - h) / 2.0;
    if (nk_begin(nk_ctx, "Пользователь занят", nk_rect(x, y, w, h),
                 NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR)) {

        nk_layout_row_dynamic(nk_ctx, 50, 1);
        nk_label(nk_ctx, state->users->username[state->callee_idx], NK_TEXT_CENTERED);

        nk_layout_row_dynamic(nk_ctx, 30, 1);
        if (nk_button_label(nk_ctx, "Закрыть")) {
            state->show_user_busy = 0;
        }
        nk_end(nk_ctx);
    }

    return result;
}

ClientAction user_list_widget(struct nk_context *nk_ctx, ClientState *state, struct nk_list_view *user_list_view) {
    ClientAction result = Action_Idle;

    if (nk_begin(nk_ctx, "Пользователи в сети", nk_rect(100, 40, 250, 370),
                 NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR)) {

        nk_layout_row_dynamic(nk_ctx, 230, 1);

        if (state->users && state->users->size > 0) {
            // Список с прокруткой через nk_list_view
            int list_begin = nk_list_view_begin(nk_ctx, user_list_view, "##users", NK_WINDOW_BORDER, 30, state->users->size);

            nk_layout_row_dynamic(nk_ctx, 25, 1);
            for (int i = user_list_view->begin; i < user_list_view->end; i++) {
                if (state->users->username[i]) {

                    if (i == state->connection_idx) {
                        nk_label(nk_ctx, state->users->username[i], NK_TEXT_CENTERED);
                    } else {
                        if (nk_button_label(nk_ctx, state->users->username[i])) {
                            state->waiting_call_response = 1;
                            state->callee_idx = i;
                            result = Action_CallRequest;
                        }
                    }

                }
            }
            nk_list_view_end(user_list_view);
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

    return result;
}

ClientAction media_control_widget(struct nk_context *nk_ctx, ClientState *state) {
    ClientAction result = Action_Idle;
    if (nk_begin(nk_ctx, "MediaControlWidget", nk_rect(370, 0, 300, 35),
                 NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)) {

        nk_layout_row_dynamic(nk_ctx, 25, 3);
        // Чекбокс для камеры
        nk_checkbox_label(nk_ctx, "Камера", &state->camera_on);

        // Чекбокс для микрофона
        nk_checkbox_label(nk_ctx, "Микрофон", &state->mic_on);
        nk_end(nk_ctx);
    }

    return result;
}
