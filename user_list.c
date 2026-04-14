#include "user_list.h"
#include <stdlib.h>

UserList* user_list_init(int size) {
    UserList* list = malloc(sizeof(UserList));
    list->size = size;
    list->username = malloc(sizeof(char*) * size);
    return list;
}

void user_list_free(UserList* list) {
    for (int i = 0; i < list->size; i++) {
        if (list->username[i]) free(list->username[i]);
    }
    free(list->username);
    free(list);
}
