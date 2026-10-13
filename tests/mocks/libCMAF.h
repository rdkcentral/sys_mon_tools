#ifndef TEST_MOCK_LIB_CMAF_H
#define TEST_MOCK_LIB_CMAF_H

#include <stddef.h>

typedef enum {
    CMAF_RESULT_SUCCESS = 0,
    CMAF_RESULT_INVALID_PARAM = 1
} CMAF_Result_t;

typedef enum {
    CMAF_MEMTYPE_PROCESSSHARE = 0
} CMAF_MemType_t;

typedef int CMAF_EventId_t;

CMAF_Result_t CMAF_Init(const char *group_name, const char *program_name, void **context);
CMAF_Result_t CMAF_Malloc(void *context, CMAF_MemType_t memory_type, size_t size, void **buffer);
CMAF_Result_t CMAF_NotifyEvent(void *context, CMAF_EventId_t event_id, void *event_data);

#endif