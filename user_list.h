#ifndef __DIPLOMA_USER_LIST_H
#define __DIPLOMA_USER_LIST_H

#include <stdint.h>

typedef struct UserList {
    int size;
    char** username;
} UserList;

UserList* user_list_init(int size);
void user_list_free(UserList* list);

#endif
