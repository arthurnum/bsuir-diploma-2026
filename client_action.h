#ifndef __DIPLOMA_CLIENT_ACTION_H
#define __DIPLOMA_CLIENT_ACTION_H

typedef enum {
    Action_Idle,
    Action_CallRequest,
    Action_CallCancel,
    Action_IncomingCallAccept,
    Action_IncomingCallReject,
    Action_MicrophoneToggle
} ClientAction;

#endif
