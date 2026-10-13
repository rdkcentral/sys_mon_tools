#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define main legacy_key_simulator_main
#include "../key_simulator/IARM_UIEventSimulator.c"
#undef main

static int assertion_failures;
static int cmaf_init_calls;
static int cmaf_malloc_calls;
static int cmaf_notify_calls;
static int sleep_calls;
static int usleep_calls;
static CMAF_Result_t cmaf_init_result = CMAF_RESULT_SUCCESS;
static UIDev_EventData_t sent_events[32];
static size_t sent_event_count;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
        assertion_failures++; \
    } \
} while (0)

CMAF_Result_t CMAF_Init(const char *group_name, const char *program_name, void **context)
{
    (void)group_name;
    (void)program_name;
    cmaf_init_calls++;
    *context = (void *)1;
    return cmaf_init_result;
}

CMAF_Result_t CMAF_Malloc(void *context, CMAF_MemType_t memory_type, size_t size, void **buffer)
{
    (void)context;
    (void)memory_type;
    cmaf_malloc_calls++;
    *buffer = calloc(1, size);
    return (*buffer == NULL) ? CMAF_RESULT_INVALID_PARAM : CMAF_RESULT_SUCCESS;
}

CMAF_Result_t CMAF_NotifyEvent(void *context, CMAF_EventId_t event_id, void *event_data)
{
    (void)context;
    CHECK(event_id == UIDEV_EVENT_IRKEY);
    CHECK(event_data != NULL);
    if (event_data != NULL && sent_event_count < sizeof(sent_events) / sizeof(sent_events[0])) {
        sent_events[sent_event_count++] = *(UIDev_EventData_t *)event_data;
    }
    cmaf_notify_calls++;
    free(event_data);
    return CMAF_RESULT_SUCCESS;
}

int usleep(useconds_t usec)
{
    (void)usec;
    usleep_calls++;
    return 0;
}

unsigned int sleep(unsigned int seconds)
{
    (void)seconds;
    sleep_calls++;
    return 0;
}

static void reset_mocks(void)
{
    cmaf_init_calls = 0;
    cmaf_malloc_calls = 0;
    cmaf_notify_calls = 0;
    sleep_calls = 0;
    usleep_calls = 0;
    cmaf_init_result = CMAF_RESULT_SUCCESS;
    sent_event_count = 0;
    memset(sent_events, 0, sizeof(sent_events));
    keyCode = -1;
    interval = 1;
    repeat = 1;
    executableName = "legacy-simulator-test";
    m_grpCtx = NULL;
}

static void test_key_code_lookup(void)
{
    char menu_command[] = "menu";
    char digit_command[] = "7";
    char unknown_command[] = "not-a-key";

    reset_mocks();
    CHECK(findKeyCode(menu_command) == CMAF_RESULT_SUCCESS);
    CHECK(keyCode == KED_MENU);

    CHECK(findKeyCode(digit_command) == CMAF_RESULT_SUCCESS);
    CHECK(keyCode == KED_DIGIT7);

    CHECK(findKeyCode(unknown_command) == CMAF_RESULT_INVALID_PARAM);
    CHECK(keyCode == KED_DIGIT7);
}

static void test_cli_help_and_no_command(void)
{
    char *help_argv[] = { "legacy-simulator", "-h", NULL };
    char *no_command_argv[] = { "legacy-simulator", NULL };

    reset_mocks();
    CHECK(legacy_key_simulator_main(2, help_argv) == 0);
    CHECK(cmaf_init_calls == 1);
    CHECK(cmaf_malloc_calls == 0);

    reset_mocks();
    CHECK(legacy_key_simulator_main(1, no_command_argv) == 0);
    CHECK(cmaf_init_calls == 1);
    CHECK(cmaf_notify_calls == 0);
}

static void test_cli_key_repeat_and_option_handling(void)
{
    char *repeat_argv[] = { "legacy-simulator", "-kmenu", "-r2", "-i0", NULL };
    char *unknown_option_argv[] = { "legacy-simulator", "-zvalue", NULL };

    reset_mocks();
    CHECK(legacy_key_simulator_main(4, repeat_argv) == 0);
    CHECK(cmaf_init_calls == 1);
    CHECK(cmaf_malloc_calls == 4);
    CHECK(cmaf_notify_calls == 4);
    CHECK(sent_event_count == 4);
    CHECK(sent_events[0].id == UIDEV_EVENT_IRKEY);
    CHECK(sent_events[0].data.irkey.keyType == KET_KEYDOWN);
    CHECK(sent_events[0].data.irkey.keyCode == KED_MENU);
    CHECK(sent_events[1].data.irkey.keyType == KET_KEYUP);
    CHECK(sent_events[2].data.irkey.keyType == KET_KEYDOWN);
    CHECK(sent_events[3].data.irkey.keyType == KET_KEYUP);
    CHECK(sleep_calls == 2);

    reset_mocks();
    CHECK(legacy_key_simulator_main(2, unknown_option_argv) == 0);
    CHECK(cmaf_init_calls == 1);
    CHECK(cmaf_notify_calls == 0);
}

static void test_cli_invalid_key_and_cmaf_init_failure(void)
{
    char *invalid_key_argv[] = { "legacy-simulator", "-knot-a-key", NULL };
    char *key_argv[] = { "legacy-simulator", "-kmenu", NULL };

    reset_mocks();
    CHECK(legacy_key_simulator_main(2, invalid_key_argv) == 0);
    CHECK(cmaf_init_calls == 1);
    CHECK(cmaf_notify_calls == 0);

    reset_mocks();
    cmaf_init_result = CMAF_RESULT_INVALID_PARAM;
    CHECK(legacy_key_simulator_main(2, key_argv) == 0);
    CHECK(cmaf_init_calls == 1);
    CHECK(cmaf_notify_calls == 0);
}

int main(void)
{
    test_key_code_lookup();
    test_cli_help_and_no_command();
    test_cli_key_repeat_and_option_handling();
    test_cli_invalid_key_and_cmaf_init_failure();
    if (assertion_failures != 0) {
        fprintf(stderr, "%d assertion(s) failed\n", assertion_failures);
        return 1;
    }
    puts("legacy key simulator L1 tests passed");
    return 0;
}