#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "IrInputRemoteKeyCodes.h"
#include "uInputInternal.h"

#define FAKE_DEVICE_FD 42
#define MAX_IOCTL_CALLS 512
#define MAX_INPUT_EVENTS 32

typedef struct {
    unsigned long request;
    int value;
} ioctl_call_t;

static int assertion_failures;
static int open_result = FAKE_DEVICE_FD;
static int fail_ioctl_request;
static int fail_input_write;
static int open_calls;
static int close_calls;
static int destroy_calls;
static int setup_ioctl_calls;
static int input_write_calls;
static char opened_path[64];
static int opened_flags;
static ioctl_call_t ioctl_calls[MAX_IOCTL_CALLS];
static size_t ioctl_call_count;
static struct input_event input_events[MAX_INPUT_EVENTS];
static size_t input_event_count;
static struct uinput_setup configured_setup;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
        assertion_failures++; \
    } \
} while (0)

int __wrap_open(const char *path, int flags, ...)
{
    open_calls++;
    snprintf(opened_path, sizeof(opened_path), "%s", path);
    opened_flags = flags;
    return open_result;
}

int __wrap_ioctl(int fd, unsigned long request, ...)
{
    va_list arguments;
    int value = 0;

    (void)fd;
    if (request != UI_DEV_DESTROY) {
        va_start(arguments, request);
        if (request == UI_DEV_SETUP) {
            struct uinput_setup *setup = va_arg(arguments, struct uinput_setup *);
            configured_setup = *setup;
            setup_ioctl_calls++;
        } else {
            value = va_arg(arguments, int);
        }
        va_end(arguments);
    } else {
        destroy_calls++;
    }

    if (ioctl_call_count < MAX_IOCTL_CALLS) {
        ioctl_calls[ioctl_call_count].request = request;
        ioctl_calls[ioctl_call_count].value = value;
        ioctl_call_count++;
    }

    if (request == (unsigned long)fail_ioctl_request) {
        errno = EIO;
        return -1;
    }
    return 0;
}

ssize_t __wrap_write(int fd, const void *buffer, size_t size)
{
    (void)fd;
    if (size == sizeof(struct input_event)) {
        input_write_calls++;
        if (fail_input_write) {
            errno = EIO;
            return -1;
        }
        if (input_event_count < MAX_INPUT_EVENTS) {
            memcpy(&input_events[input_event_count], buffer, size);
            input_event_count++;
        }
    }
    return (ssize_t)size;
}

int __wrap_close(int fd)
{
    (void)fd;
    close_calls++;
    return 0;
}

#define open __wrap_open
#define ioctl __wrap_ioctl
#define write __wrap_write
#define close __wrap_close
#include "../key_simulator/uinput.c"
#undef close
#undef write
#undef ioctl
#undef open

static void reset_mocks(void)
{
    UINPUT_term();
    open_result = FAKE_DEVICE_FD;
    fail_ioctl_request = 0;
    fail_input_write = 0;
    open_calls = 0;
    close_calls = 0;
    destroy_calls = 0;
    setup_ioctl_calls = 0;
    input_write_calls = 0;
    opened_path[0] = '\0';
    opened_flags = 0;
    ioctl_call_count = 0;
    input_event_count = 0;
    memset(ioctl_calls, 0, sizeof(ioctl_calls));
    memset(input_events, 0, sizeof(input_events));
    memset(&configured_setup, 0, sizeof(configured_setup));
}

static int has_ioctl(unsigned long request, int value)
{
    size_t index;

    for (index = 0; index < ioctl_call_count; index++) {
        if (ioctl_calls[index].request == request && ioctl_calls[index].value == value) {
            return 1;
        }
    }
    return 0;
}

static void check_event(size_t index, unsigned short type, unsigned short code, int value)
{
    CHECK(index < input_event_count);
    if (index < input_event_count) {
        CHECK(input_events[index].type == type);
        CHECK(input_events[index].code == code);
        CHECK(input_events[index].value == value);
    }
}

static void test_open_failure_returns_no_dispatcher(void)
{
    reset_mocks();
    open_result = -1;

    CHECK(UINPUT_init() == 0);
    CHECK(open_calls == 1);
    CHECK(UINPUT_GetDispatcher() == NULL);
}

static void test_successful_setup_creates_device(void)
{
    reset_mocks();

    CHECK(UINPUT_init() == 0);
    CHECK(strcmp(opened_path, "/dev/uinput") == 0);
    CHECK(opened_flags == (O_WRONLY | O_SYNC));
    CHECK(has_ioctl(UI_SET_EVBIT, EV_KEY));
    CHECK(has_ioctl(UI_SET_EVBIT, EV_SYN));
    CHECK(has_ioctl(UI_DEV_SETUP, 0));
    CHECK(has_ioctl(UI_DEV_CREATE, 0));
    CHECK(UINPUT_GetDispatcher() != NULL);
    CHECK(setup_ioctl_calls == 1);
    CHECK(strcmp(configured_setup.name, "key-simulator") == 0);
    CHECK(configured_setup.id.bustype == BUS_USB);
    CHECK(configured_setup.id.vendor == 0xbeef);
    CHECK(configured_setup.id.product == 0xfedc);
    CHECK(configured_setup.id.version == 1);

    UINPUT_term();
    CHECK(destroy_calls == 1);
    CHECK(close_calls == 1);
    CHECK(UINPUT_GetDispatcher() == NULL);
}

static void test_modifier_key_down_up_and_repeat_events(void)
{
    uinput_dispatcher_t dispatcher;

    reset_mocks();
    UINPUT_init();
    dispatcher = UINPUT_GetDispatcher();
    CHECK(dispatcher != NULL);
    if (dispatcher == NULL) {
        return;
    }

    dispatcher(KED_CHANNELUP, KET_KEYDOWN, 0);
    CHECK(input_event_count == 4);
    check_event(0, EV_KEY, KEY_LEFTCTRL, 1);
    check_event(1, EV_SYN, SYN_REPORT, 0);
    check_event(2, EV_KEY, KEY_UP, 1);
    check_event(3, EV_SYN, SYN_REPORT, 0);

    input_event_count = 0;
    dispatcher(KED_CHANNELUP, KET_KEYUP, 0);
    CHECK(input_event_count == 4);
    check_event(0, EV_KEY, KEY_UP, 0);
    check_event(2, EV_KEY, KEY_LEFTCTRL, 0);

    input_event_count = 0;
    dispatcher(KED_CHANNELUP, KET_KEYREPEAT, 0);
    CHECK(input_event_count == 2);
    check_event(0, EV_KEY, KEY_UP, 2);
    check_event(1, EV_SYN, SYN_REPORT, 0);

    UINPUT_term();
}

static void test_regular_key_and_unknown_key_events(void)
{
    uinput_dispatcher_t dispatcher;

    reset_mocks();
    UINPUT_init();
    dispatcher = UINPUT_GetDispatcher();
    CHECK(dispatcher != NULL);
    if (dispatcher == NULL) {
        return;
    }

    dispatcher(KED_DIGIT1, KET_KEYDOWN, 0);
    CHECK(input_event_count == 2);
    check_event(0, EV_KEY, KEY_1, 1);
    check_event(1, EV_SYN, SYN_REPORT, 0);

    input_event_count = 0;
    dispatcher(UINT32_MAX, KET_KEYUP, 0);
    CHECK(input_event_count == 2);
    check_event(0, EV_KEY, KEY_RESERVED, 0);

    UINPUT_term();
}

static void test_setup_failure_destroys_and_closes_device(void)
{
    reset_mocks();
    fail_ioctl_request = UI_DEV_CREATE;

    CHECK(UINPUT_init() == 0);
    CHECK(UINPUT_GetDispatcher() == NULL);
    CHECK(destroy_calls == 1);
    CHECK(close_calls == 1);
}

static void test_event_write_failure_terminates_device(void)
{
    uinput_dispatcher_t dispatcher;

    reset_mocks();
    UINPUT_init();
    dispatcher = UINPUT_GetDispatcher();
    CHECK(dispatcher != NULL);
    if (dispatcher == NULL) {
        return;
    }

    fail_input_write = 1;
    dispatcher(KED_DIGIT1, KET_KEYDOWN, 0);
    CHECK(input_write_calls == 1);
    CHECK(destroy_calls == 1);
    CHECK(close_calls == 1);
    CHECK(UINPUT_GetDispatcher() == NULL);
}

int run_uinput_tests(void)
{
    test_open_failure_returns_no_dispatcher();
    test_successful_setup_creates_device();
    test_modifier_key_down_up_and_repeat_events();
    test_regular_key_and_unknown_key_events();
    test_setup_failure_destroys_and_closes_device();
    test_event_write_failure_terminates_device();

    if (assertion_failures != 0) {
        fprintf(stderr, "%d assertion(s) failed\n", assertion_failures);
        return 1;
    }
    puts("uinput L1 tests passed");
    return 0;
}