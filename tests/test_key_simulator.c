#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "UIEventSimulator.h"
#include "uInputInternal.h"

#define main key_simulator_main
#include "../key_simulator/IARM_BUS_UIEventSimulator.c"
#undef main

#define MAX_SENT_EVENTS 128

typedef struct {
    int key_type;
    int key_code;
} sent_event_t;

static int assertion_failures;
static int bus_init_calls;
static int bus_connect_calls;
static int bus_disconnect_calls;
static int bus_term_calls;
static int uinput_init_calls;
static int uinput_term_calls;
static int sleep_calls;
static int usleep_calls;
static int dispatcher_available = 1;
static sent_event_t sent_events[MAX_SENT_EVENTS];
static size_t sent_event_count;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
        assertion_failures++; \
    } \
} while (0)

static void mock_dispatcher(int event_key_code, int event_key_type, int source)
{
    (void)source;
    if (sent_event_count < MAX_SENT_EVENTS) {
        sent_events[sent_event_count].key_type = event_key_type;
        sent_events[sent_event_count].key_code = event_key_code;
        sent_event_count++;
    }
}

IARM_Result_t IARM_Bus_Init(const char *name)
{
    (void)name;
    bus_init_calls++;
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t IARM_Bus_Connect(void)
{
    bus_connect_calls++;
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t IARM_Bus_Disconnect(void)
{
    bus_disconnect_calls++;
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t IARM_Bus_Term(void)
{
    bus_term_calls++;
    return IARM_RESULT_SUCCESS;
}

int UINPUT_init(void)
{
    uinput_init_calls++;
    return 0;
}

int UINPUT_term(void)
{
    uinput_term_calls++;
    return 0;
}

uinput_dispatcher_t UINPUT_GetDispatcher(void)
{
    return dispatcher_available ? mock_dispatcher : NULL;
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
    bus_init_calls = 0;
    bus_connect_calls = 0;
    bus_disconnect_calls = 0;
    bus_term_calls = 0;
    uinput_init_calls = 0;
    uinput_term_calls = 0;
    sleep_calls = 0;
    usleep_calls = 0;
    dispatcher_available = 1;
    sent_event_count = 0;
    memset(sent_events, 0, sizeof(sent_events));
    executableName = "key-simulator-test";
    keyCode = -1;
    interval = 1;
    repeat = 1;
    pressAndHold = 0;
    duration = 5;
}

static void test_key_code_lookup(void)
{
    char known_command[] = "menu";
    char unknown_command[] = "not-a-key";

    reset_mocks();
    CHECK(findKeyCode(known_command) == IARM_RESULT_SUCCESS);
    CHECK(keyCode == KED_MENU);
    CHECK(findKeyCode(unknown_command) == IARM_RESULT_INVALID_PARAM);
}

static void test_repeated_key_down_up_sequence(void)
{
    reset_mocks();
    keyCode = KED_DIGIT4;
    repeat = 3;
    interval = 2;

    CHECK(sendCommand() == IARM_RESULT_SUCCESS);
    CHECK(sent_event_count == 6);
    CHECK(sent_events[0].key_type == KET_KEYDOWN);
    CHECK(sent_events[0].key_code == KED_DIGIT4);
    CHECK(sent_events[1].key_type == KET_KEYUP);
    CHECK(sent_events[2].key_type == KET_KEYDOWN);
    CHECK(sent_events[5].key_type == KET_KEYUP);
    CHECK(sleep_calls == 3);
    CHECK(usleep_calls == 4);
}

static void test_press_and_hold_repeats_then_releases(void)
{
    reset_mocks();
    keyCode = KED_ARROWUP;
    repeat = 1;
    duration = 2;
    pressAndHold = 1;

    CHECK(sendCommand() == IARM_RESULT_SUCCESS);
    CHECK(sent_event_count == 42);
    CHECK(sent_events[0].key_type == KET_KEYDOWN);
    CHECK(sent_events[1].key_type == KET_KEYREPEAT);
    CHECK(sent_events[40].key_type == KET_KEYREPEAT);
    CHECK(sent_events[41].key_type == KET_KEYUP);
}

static void test_dispatcher_absence_is_handled(void)
{
    reset_mocks();
    dispatcher_available = 0;
    sendKeyEventToIARM(KET_KEYDOWN, KED_MENU);
    CHECK(sent_event_count == 0);
}

static void test_start_and_stop_lifecycle(void)
{
    reset_mocks();

    CHECK(UIEventSimulator_Start() == IARM_RESULT_SUCCESS);
    CHECK(bus_init_calls == 1);
    CHECK(bus_connect_calls == 1);
    CHECK(uinput_init_calls == 1);

    CHECK(UIEventSimulator_Stop() == IARM_RESULT_SUCCESS);
    CHECK(bus_disconnect_calls == 1);
    CHECK(bus_term_calls == 1);
    CHECK(uinput_term_calls == 1);
}

static void test_cli_lifecycle_and_options(void)
{
    char *no_command_argv[] = { "key-simulator", NULL };
    char *help_argv[] = { "key-simulator", "-h", NULL };
    char *unknown_option_argv[] = { "key-simulator", "-zvalue", NULL };
    char *invalid_key_argv[] = { "key-simulator", "-knot-a-key", NULL };
    char *repeat_argv[] = { "key-simulator", "-kmenu", "-r2", "-i0", NULL };
    char *hold_argv[] = { "key-simulator", "-p1", "-kmenu", "-r1", "-i0", NULL };

    reset_mocks();
    CHECK(key_simulator_main(1, no_command_argv) == 0);
    CHECK(bus_init_calls == 1);
    CHECK(bus_disconnect_calls == 1);
    CHECK(uinput_term_calls == 1);
    CHECK(sent_event_count == 0);

    reset_mocks();
    CHECK(key_simulator_main(2, help_argv) == 0);
    CHECK(bus_init_calls == 1);
    CHECK(bus_disconnect_calls == 1);
    CHECK(sent_event_count == 0);

    reset_mocks();
    CHECK(key_simulator_main(2, unknown_option_argv) == 0);
    CHECK(bus_disconnect_calls == 1);
    CHECK(sent_event_count == 0);

    reset_mocks();
    CHECK(key_simulator_main(2, invalid_key_argv) == 0);
    CHECK(bus_init_calls == 1);
    CHECK(bus_disconnect_calls == 0);
    CHECK(sent_event_count == 0);

    reset_mocks();
    CHECK(key_simulator_main(4, repeat_argv) == 0);
    CHECK(sent_event_count == 4);
    CHECK(sent_events[0].key_type == KET_KEYDOWN);
    CHECK(sent_events[1].key_type == KET_KEYUP);
    CHECK(sent_events[2].key_type == KET_KEYDOWN);
    CHECK(sent_events[3].key_type == KET_KEYUP);
    CHECK(bus_disconnect_calls == 1);

    reset_mocks();
    CHECK(key_simulator_main(5, hold_argv) == 0);
    CHECK(sent_event_count == 22);
    CHECK(sent_events[0].key_type == KET_KEYDOWN);
    CHECK(sent_events[1].key_type == KET_KEYREPEAT);
    CHECK(sent_events[20].key_type == KET_KEYREPEAT);
    CHECK(sent_events[21].key_type == KET_KEYUP);
    CHECK(bus_disconnect_calls == 1);
}

int main(void)
{
    test_key_code_lookup();
    test_repeated_key_down_up_sequence();
    test_press_and_hold_repeats_then_releases();
    test_dispatcher_absence_is_handled();
    test_start_and_stop_lifecycle();
    test_cli_lifecycle_and_options();

    if (assertion_failures != 0) {
        fprintf(stderr, "%d assertion(s) failed\n", assertion_failures);
        return 1;
    }
    puts("key simulator L1 tests passed");
    return 0;
}