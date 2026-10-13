#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "power_controller.h"

int __wrap_open(const char *path, int flags, ...);
off_t __wrap_lseek(int fd, off_t offset, int whence);
ssize_t __wrap_read(int fd, void *buffer, size_t count);
int __wrap_close(int fd);

#define open __wrap_open
#define lseek __wrap_lseek
#define read __wrap_read
#define close __wrap_close
#define main query_power_state_main
#include "../iarm_query_powerstate/IARM_Bus_CheckPowerStatus.c"
#undef main
#undef close
#undef read
#undef lseek
#undef open

#define FAKE_SETTINGS_FD 55

static int assertion_failures;
static int power_init_calls;
static int power_term_calls;
static int get_power_state_calls;
static int settings_open_calls;
static int settings_close_calls;
static int settings_read_calls;
static int settings_seek_calls;
static int settings_open_result = FAKE_SETTINGS_FD;
static int settings_read_result = sizeof(PWRMgr_Settings_t) - PADDING_SIZE;
static off_t settings_seek_result;
static uint32_t get_power_state_result;
static PowerController_PowerState_t current_power_state = POWER_STATE_ON;
static PowerController_PowerState_t previous_power_state = POWER_STATE_UNKNOWN;
static PWRMgr_PowerState_t settings_power_state = PWRMGR_POWERSTATE_ON;

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

void PowerController_Term(void)
{
    power_term_calls++;
}

uint32_t PowerController_GetPowerState(PowerController_PowerState_t *currentState,
                                       PowerController_PowerState_t *previousState)
{
    get_power_state_calls++;
    *currentState = current_power_state;
    *previousState = previous_power_state;
    return get_power_state_result;
}

int __wrap_open(const char *path, int flags, ...)
{
    (void)flags;
    CHECK(strcmp(path, "/opt/uimgr_settings.bin") == 0);
    settings_open_calls++;
    return settings_open_result;
}

off_t __wrap_lseek(int fd, off_t offset, int whence)
{
    (void)fd;
    CHECK(offset == 0);
    CHECK(whence == SEEK_SET);
    settings_seek_calls++;
    return settings_seek_result;
}

ssize_t __wrap_read(int fd, void *buffer, size_t count)
{
    PWRMgr_Settings_t *settings = (PWRMgr_Settings_t *)buffer;

    (void)fd;
    CHECK(count == sizeof(PWRMgr_Settings_t) - PADDING_SIZE);
    settings_read_calls++;
    settings->powerState = settings_power_state;
    return settings_read_result;
}

int __wrap_close(int fd)
{
    if (fd == FAKE_SETTINGS_FD) {
        settings_close_calls++;
        return 0;
    }
    return close(fd);
}

static void reset_mocks(void)
{
    power_init_calls = 0;
    power_term_calls = 0;
    get_power_state_calls = 0;
    settings_open_calls = 0;
    settings_close_calls = 0;
    settings_read_calls = 0;
    settings_seek_calls = 0;
    settings_open_result = FAKE_SETTINGS_FD;
    settings_read_result = sizeof(PWRMgr_Settings_t) - PADDING_SIZE;
    settings_seek_result = 0;
    get_power_state_result = POWER_CONTROLLER_ERROR_NONE;
    current_power_state = POWER_STATE_ON;
    previous_power_state = POWER_STATE_UNKNOWN;
    settings_power_state = PWRMGR_POWERSTATE_ON;
}

static int run_with_output(int argc, char **argv, char *output, size_t output_size)
{
    FILE *capture;
    int saved_stdout;
    int result;
    size_t output_length;

    fflush(stdout);
    saved_stdout = dup(STDOUT_FILENO);
    capture = tmpfile();
    if (saved_stdout < 0 || capture == NULL) {
        if (saved_stdout >= 0) {
            close(saved_stdout);
        }
        if (capture != NULL) {
            fclose(capture);
        }
        return -1;
    }

    if (dup2(fileno(capture), STDOUT_FILENO) < 0) {
        close(saved_stdout);
        fclose(capture);
        return -1;
    }

    result = query_power_state_main(argc, argv);
    fflush(stdout);
    fseek(capture, 0, SEEK_SET);
    output_length = fread(output, 1, output_size - 1, capture);
    output[output_length] = '\0';
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);
    fclose(capture);
    return result;
}

static void test_power_controller_states(void)
{
    struct {
        PowerController_PowerState_t state;
        const char *expected;
    } cases[] = {
        { POWER_STATE_OFF, "OFF\n" },
        { POWER_STATE_STANDBY, "STANDBY\n" },
        { POWER_STATE_ON, "ON\n" },
        { POWER_STATE_STANDBY_LIGHT_SLEEP, "LIGHTSLEEP\n" },
        { POWER_STATE_STANDBY_DEEP_SLEEP, "DEEPSLEEP\n" },
        { POWER_STATE_UNKNOWN, "Unknown\n" }
    };
    size_t index;

    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index++) {
        char *argv[] = { "QueryPowerState", "-c", NULL };
        char output[512];

        reset_mocks();
        current_power_state = cases[index].state;
        CHECK(run_with_output(2, argv, output, sizeof(output)) == 0);
        CHECK(strcmp(output, cases[index].expected) == 0);
        CHECK(power_init_calls == 1);
        CHECK(get_power_state_calls == 1);
        CHECK(power_term_calls == 1);
    }
}

static void test_power_controller_errors(void)
{
    char *argv[] = { "QueryPowerState", "-c", NULL };
    char output[512];

    reset_mocks();
    get_power_state_result = POWER_CONTROLLER_ERROR_UNAVAILABLE;
    CHECK(run_with_output(2, argv, output, sizeof(output)) == 0);
    CHECK(strstr(output, "PowerManager plugin unavailable") != NULL);

    reset_mocks();
    get_power_state_result = POWER_CONTROLLER_ERROR_GENERAL;
    CHECK(run_with_output(2, argv, output, sizeof(output)) == 0);
    CHECK(strstr(output, "Error :: Unknown") != NULL);
}

static void test_help_does_not_connect_to_power_controller(void)
{
    char *argv[] = { "QueryPowerState", "-h", NULL };
    char output[512];

    reset_mocks();
    CHECK(run_with_output(2, argv, output, sizeof(output)) == 0);
    CHECK(strstr(output, "Usage:") != NULL);
    CHECK(power_init_calls == 0);
    CHECK(get_power_state_calls == 0);
}

static void test_settings_file_state_and_failures(void)
{
    struct {
        PWRMgr_PowerState_t state;
        const char *expected;
    } cases[] = {
        { PWRMGR_POWERSTATE_OFF, "OFF\n" },
        { PWRMGR_POWERSTATE_STANDBY, "STANDBY\n" },
        { PWRMGR_POWERSTATE_ON, "ON\n" },
        { PWRMGR_POWERSTATE_STANDBY_LIGHT_SLEEP, "LIGHTSLEEP\n" },
        { PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP, "DEEPSLEEP\n" },
        { PWRMGR_POWERSTATE_MAX, "Unknown Power state\n" }
    };
    size_t index;

    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index++) {
        char *argv[] = { "QueryPowerState", NULL };
        char output[512];

        reset_mocks();
        settings_power_state = cases[index].state;
        CHECK(run_with_output(1, argv, output, sizeof(output)) == 0);
        CHECK(strcmp(output, cases[index].expected) == 0);
        CHECK(settings_open_calls == 1);
        CHECK(settings_read_calls == 1);
        CHECK(settings_close_calls == 1);
    }

    {
        char *argv[] = { "QueryPowerState", NULL };
        char output[512];

        reset_mocks();
        settings_open_result = -1;
        CHECK(run_with_output(1, argv, output, sizeof(output)) == 0);
        CHECK(strstr(output, "Error in reading PWRMgr settings File") != NULL);
        CHECK(settings_read_calls == 0);

        reset_mocks();
        settings_seek_result = -1;
        CHECK(run_with_output(1, argv, output, sizeof(output)) == 0);
        CHECK(strstr(output, "Error in seeking PWRMgr settings File") != NULL);
        CHECK(settings_read_calls == 0);
        CHECK(settings_close_calls == 1);
    }
}

int main(void)
{
    test_power_controller_states();
    test_power_controller_errors();
    test_help_does_not_connect_to_power_controller();
    test_settings_file_state_and_failures();

    if (assertion_failures != 0) {
        fprintf(stderr, "%d assertion(s) failed\n", assertion_failures);
        return 1;
    }
    puts("QueryPowerState L1 tests passed");
    return 0;
}