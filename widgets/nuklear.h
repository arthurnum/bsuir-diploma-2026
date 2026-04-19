#ifndef __DIPLOMA_NUKLEAR_H
#define __DIPLOMA_NUKLEAR_H

#include "../nuklear.h"
#include "../client_state.h"
#include "../client_action.h"

ClientAction incoming_call_widget(struct nk_context *nk_ctx, ClientState *state);
ClientAction user_list_widget(struct nk_context *nk_ctx, ClientState *state, struct nk_list_view *user_list_view);

#endif

