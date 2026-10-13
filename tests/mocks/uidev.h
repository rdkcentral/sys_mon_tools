#ifndef TEST_MOCK_UIDEV_H
#define TEST_MOCK_UIDEV_H

#define UIDEV_PROCESSGROUP_NAME "UIEventSimulator"
#define UIDEV_EVENT_IRKEY 1

typedef struct {
    int keyType;
    int keyCode;
} UIDev_IRKeyData_t;

typedef struct {
    int id;
    union {
        UIDev_IRKeyData_t irkey;
    } data;
} UIDev_EventData_t;

#endif