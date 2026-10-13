#include <errno.h>
#include <glib.h>
#include <pthread.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "power_controller.h"

FILE *__wrap_fopen(const char *path, const char *mode);
int __wrap_fclose(FILE *file);
int __wrap_fprintf(FILE *file, const char *format, ...);
int __wrap_remove(const char *path);
int __wrap_stat(const char *path, struct stat *buffer);
int __wrap_pthread_create(pthread_t *thread, const pthread_attr_t *attributes,
                          void *(*start_routine)(void *), void *argument);
int __wrap_pthread_detach(pthread_t thread);
GAsyncQueue *__wrap_g_async_queue_new(void);
gpointer __wrap_g_async_queue_pop(GAsyncQueue *queue);

#define fopen __wrap_fopen
#define fclose __wrap_fclose
#define fprintf __wrap_fprintf
#define remove __wrap_remove
#define stat(path, buffer) __wrap_stat((path), (buffer))
#define pthread_create __wrap_pthread_create
#define pthread_detach __wrap_pthread_detach
#define g_async_queue_new __wrap_g_async_queue_new
#define g_async_queue_pop __wrap_g_async_queue_pop
#define main power_state_monitor_main
#include "../power-state-monitor/powerStateMonitorMain.c"
#undef main
#undef g_async_queue_pop
#undef g_async_queue_new
#undef pthread_detach
#undef pthread_create
#undef stat
#undef remove
#undef fprintf
#undef fclose
#undef fopen

static int assertion_failures;
static int power_init_calls;
static int is_operational = 1;
static uint32_t connect_results[8];
static size_t connect_result_count;
static size_t connect_result_index;
static int connect_calls;
static int get_state_calls;
static int register_callback_calls;
static PowerController_PowerModeChangedCb registered_callback;
static void *registered_callback_userdata;
static int create_thread_calls;
static int detach_thread_calls;
static int run_thread_in_mock;
static int thread_create_result;
static int remove_calls;
static int close_file_calls;
static int fprintf_calls;
static int stat_calls;
static int sleep_calls;
static int async_queue_new_calls;
static int async_queue_pop_calls;
static int async_queue_available = 1;
static int escape_on_queue_pop;
static jmp_buf queue_pop_escape;
static int open_file_success = 1;
static int mock_lightsleep_exists;
static int mock_power_on_exists;
static int power_on_after_sleep;
static int remove_result;
static uint32_t get_state_result = POWER_CONTROLLER_ERROR_NONE;
static PowerController_PowerState_t current_state = POWER_STATE_ON;
static char opened_path[512];
static char opened_mode[16];
static char removed_path[512];

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
        assertion_failures++; \
    } \
} while (0)

void PowerController_Init(void)
{
    power_init_calls++;
}

bool PowerController_IsOperational(void)
{
    return is_operational != 0;
}

uint32_t PowerController_Connect(void)
{
    uint32_t result = POWER_CONTROLLER_ERROR_NONE;

    connect_calls++;
    if (connect_result_index < connect_result_count) {
        result = connect_results[connect_result_index++];
    }
    if (result == POWER_CONTROLLER_ERROR_NONE) {
        is_operational = 1;
    }
    return result;
}

uint32_t PowerController_GetPowerState(PowerController_PowerState_t *currentState,
                                       PowerController_PowerState_t *previousState)
{
    get_state_calls++;
    *currentState = current_state;
    *previousState = POWER_STATE_UNKNOWN;
    return get_state_result;
}

uint32_t PowerController_RegisterPowerModeChangedCallback(
    PowerController_PowerModeChangedCb callback, void *userdata)
{
    register_callback_calls++;
    registered_callback = callback;
    registered_callback_userdata = userdata;
    return POWER_CONTROLLER_ERROR_NONE;
}

FILE *__wrap_fopen(const char *path, const char *mode)
{
    snprintf(opened_path, sizeof(opened_path), "%s", path);
    snprintf(opened_mode, sizeof(opened_mode), "%s", mode);
    return open_file_success ? (FILE *)(uintptr_t)1 : NULL;
}

int __wrap_fclose(FILE *file)
{
    (void)file;
    close_file_calls++;
    return 0;
}

int __wrap_fprintf(FILE *file, const char *format, ...)
{
    (void)file;
    (void)format;
    fprintf_calls++;
    return 0;
}

int __wrap_remove(const char *path)
{
    remove_calls++;
    snprintf(removed_path, sizeof(removed_path), "%s", path);
    return remove_result;
}

int __wrap_stat(const char *path, struct stat *buffer)
{
    int exists = 0;

    (void)buffer;
    stat_calls++;
    if (strcmp(path, TMP_LIGHTSLEEP_ON) == 0) {
        exists = mock_lightsleep_exists;
    } else if (strcmp(path, TMP_POWER_ON) == 0) {
        exists = mock_power_on_exists;
    }
    if (exists) {
        return 0;
    }
    errno = ENOENT;
    return -1;
}

int __wrap_pthread_create(pthread_t *thread, const pthread_attr_t *attributes,
                          void *(*start_routine)(void *), void *argument)
{
    (void)attributes;
    create_thread_calls++;
    if (thread_create_result != 0) {
        return thread_create_result;
    }
    *thread = (pthread_t)1;
    if (run_thread_in_mock) {
        start_routine(argument);
    }
    return thread_create_result;
}

int __wrap_pthread_detach(pthread_t thread)
{
    (void)thread;
    detach_thread_calls++;
    return 0;
}

GAsyncQueue *__wrap_g_async_queue_new(void)
{
    async_queue_new_calls++;
    return async_queue_available ? (GAsyncQueue *)(uintptr_t)1 : NULL;
}

gpointer __wrap_g_async_queue_pop(GAsyncQueue *queue)
{
    (void)queue;
    async_queue_pop_calls++;
    if (escape_on_queue_pop) {
        longjmp(queue_pop_escape, 1);
    }
    return NULL;
}

int usleep(useconds_t usec)
{
    (void)usec;
    return 0;
}

unsigned int sleep(unsigned int seconds)
{
    (void)seconds;
    sleep_calls++;
    if (power_on_after_sleep) {
        mock_power_on_exists = 1;
        power_on_after_sleep = 0;
    }
    return 0;
}

static void reset_mocks(void)
{
    power_init_calls = 0;
    is_operational = 1;
    memset(connect_results, 0, sizeof(connect_results));
    connect_result_count = 0;
    connect_result_index = 0;
    connect_calls = 0;
    get_state_calls = 0;
    register_callback_calls = 0;
    registered_callback = NULL;
    registered_callback_userdata = NULL;
    create_thread_calls = 0;
    detach_thread_calls = 0;
    run_thread_in_mock = 0;
    thread_create_result = 0;
    remove_calls = 0;
    close_file_calls = 0;
    fprintf_calls = 0;
    stat_calls = 0;
    sleep_calls = 0;
    async_queue_new_calls = 0;
    async_queue_pop_calls = 0;
    async_queue_available = 1;
    escape_on_queue_pop = 0;
    open_file_success = 1;
    mock_lightsleep_exists = 0;
    mock_power_on_exists = 0;
    power_on_after_sleep = 0;
    remove_result = 0;
    get_state_result = POWER_CONTROLLER_ERROR_NONE;
    current_state = POWER_STATE_ON;
    opened_path[0] = '\0';
    opened_mode[0] = '\0';
    removed_path[0] = '\0';
    gpowerState = POWER_STATE_ON;
}

static void test_on_and_standby_flag_updates(void)
{
    reset_mocks();
    setModeSettings(1);
    CHECK(strcmp(opened_path, TMP_POWER_ON) == 0);
    CHECK(strcmp(opened_mode, "w+") == 0);
    CHECK(close_file_calls == 1);
    CHECK(strcmp(removed_path, "/tmp/.standby") == 0);
    CHECK(create_thread_calls == 0);

    reset_mocks();
    setModeSettings(0);
    CHECK(strcmp(opened_path, "/tmp/.standby") == 0);
    CHECK(strcmp(removed_path, TMP_POWER_ON) == 0);
    CHECK(create_thread_calls == 1);
    CHECK(detach_thread_calls == 1);

    reset_mocks();
    setModeSettings(7);
    CHECK(opened_path[0] == '\0');
    CHECK(remove_calls == 0);
    CHECK(create_thread_calls == 0);
}

static void test_power_mode_callback_updates_state_flags(void)
{
    reset_mocks();
    _lightsleepEventHandler(POWER_STATE_STANDBY, POWER_STATE_ON, NULL);
    CHECK(gpowerState == POWER_STATE_ON);
    CHECK(strcmp(opened_path, TMP_POWER_ON) == 0);

    reset_mocks();
    _lightsleepEventHandler(POWER_STATE_ON, POWER_STATE_STANDBY_LIGHT_SLEEP, NULL);
    CHECK(gpowerState == POWER_STATE_STANDBY_LIGHT_SLEEP);
    CHECK(strcmp(opened_path, "/tmp/.standby") == 0);
    CHECK(create_thread_calls == 1);
}

static void test_initial_state_sets_power_flags(void)
{
    reset_mocks();
    current_state = POWER_STATE_ON;
    CHECK(power_monitor_init() == 0);
    CHECK(power_init_calls == 1);
    CHECK(get_state_calls == 1);
    CHECK(strcmp(opened_path, TMP_POWER_ON) == 0);

    reset_mocks();
    current_state = POWER_STATE_STANDBY_DEEP_SLEEP;
    CHECK(power_monitor_init() == 0);
    CHECK(strcmp(opened_path, "/tmp/.standby") == 0);
    CHECK(create_thread_calls == 1);

    reset_mocks();
    get_state_result = POWER_CONTROLLER_ERROR_UNAVAILABLE;
    CHECK(power_monitor_init() == 0);
    CHECK(get_state_calls == 1);
    CHECK(opened_path[0] == '\0');

    reset_mocks();
    current_state = POWER_STATE_UNKNOWN;
    CHECK(power_monitor_init() == 0);
    CHECK(get_state_calls == 1);
    CHECK(opened_path[0] == '\0');

    reset_mocks();
    get_state_result = POWER_CONTROLLER_ERROR_GENERAL;
    CHECK(power_monitor_init() == 0);
    CHECK(get_state_calls == 1);
    CHECK(opened_path[0] == '\0');
}

static void test_power_controller_connection_retries(void)
{
    reset_mocks();
    is_operational = 0;
    connect_results[0] = POWER_CONTROLLER_ERROR_UNAVAILABLE;
    connect_results[1] = POWER_CONTROLLER_ERROR_NOT_EXIST;
    connect_results[2] = POWER_CONTROLLER_ERROR_GENERAL;
    connect_results[3] = POWER_CONTROLLER_ERROR_NONE;
    connect_result_count = 4;

    CHECK(power_monitor_init() == 0);
    CHECK(connect_calls == 4);
    CHECK(connect_result_index == 4);
    CHECK(get_state_calls == 1);
}

static void test_monitor_thread_file_and_exit_paths(void)
{
    reset_mocks();
    mock_lightsleep_exists = 1;
    CHECK(lightsleep_monitor(NULL) == NULL);
    CHECK(stat_calls == 1);
    CHECK(opened_path[0] == '\0');

    reset_mocks();
    run_thread_in_mock = 1;
    power_on_after_sleep = 1;
    setModeSettings(0);
    CHECK(create_thread_calls == 1);
    CHECK(detach_thread_calls == 1);
    CHECK(stat_calls == 3);
    CHECK(sleep_calls == 1);
    CHECK(fprintf_calls == 2);
    CHECK(remove_calls == 2);
    CHECK(close_file_calls == 4);

    reset_mocks();
    run_thread_in_mock = 1;
    mock_power_on_exists = 1;
    remove_result = -1;
    setModeSettings(0);
    CHECK(fprintf_calls == 1);
    CHECK(remove_calls == 2);
    CHECK(sleep_calls == 0);

    reset_mocks();
    open_file_success = 0;
    mock_power_on_exists = 1;
    CHECK(lightsleep_monitor(NULL) == NULL);
    CHECK(close_file_calls == 0);
    CHECK(fprintf_calls == 0);
    CHECK(remove_calls == 1);
}

static void test_flag_file_and_thread_failures(void)
{
    reset_mocks();
    open_file_success = 0;
    remove_result = -1;
    setModeSettings(1);
    CHECK(close_file_calls == 0);
    CHECK(remove_calls == 1);

    reset_mocks();
    thread_create_result = 1;
    setModeSettings(0);
    CHECK(create_thread_calls == 1);
    CHECK(detach_thread_calls == 0);
}

static void test_main_registers_callback_and_handles_queue(void)
{
    char *argv[] = { "power-state-monitor", NULL };
    int jump_result;

    reset_mocks();
    async_queue_available = 0;
    CHECK(power_state_monitor_main(1, argv) == 1);
    CHECK(register_callback_calls == 1);
    CHECK(registered_callback == _lightsleepEventHandler);
    CHECK(registered_callback_userdata == NULL);
    CHECK(async_queue_new_calls == 1);
    CHECK(async_queue_pop_calls == 0);

    reset_mocks();
    escape_on_queue_pop = 1;
    jump_result = setjmp(queue_pop_escape);
    if (jump_result == 0) {
        power_state_monitor_main(1, argv);
        CHECK(0);
    }
    CHECK(register_callback_calls == 1);
    CHECK(registered_callback == _lightsleepEventHandler);
    CHECK(async_queue_new_calls == 1);
    CHECK(async_queue_pop_calls == 1);
}

int main(void)
{
    test_on_and_standby_flag_updates();
    test_power_mode_callback_updates_state_flags();
    test_initial_state_sets_power_flags();
    test_power_controller_connection_retries();
    test_monitor_thread_file_and_exit_paths();
    test_flag_file_and_thread_failures();
    test_main_registers_callback_and_handles_queue();

    if (assertion_failures != 0) {
        fprintf(stderr, "%d assertion(s) failed\n", assertion_failures);
        return 1;
    }
    puts("power-state-monitor L1 tests passed");
    return 0;
}