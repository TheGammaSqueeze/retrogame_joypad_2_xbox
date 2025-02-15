#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/uinput.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/file.h>
#include <signal.h>
#include <math.h> // Include for ceil function

#define BUFFER_SIZE sizeof(int)
#define DIRECTORY_PATH "/data/rgp2xbox/"
#define msleep(ms) usleep((ms) * 1000)
#define RETRY_DELAY 5000 // Retry delay in microseconds (5 milliseconds)

#define MOUSE_ANALOG_THRESHOLD 80
#define MOUSE_ANALOG_SPEED 1 // Default speed of mouse movement

// Global variable declarations
const int debug_messages_enabled = 0;
int * abxy_layout, * abxy_layout_isupdated, abxy_layout_isupdated_local;
int * performance_mode, * performance_mode_isupdated, performance_mode_isupdated_local;
int * analog_sensitivity, * analog_sensitivity_isupdated, analog_sensitivity_isupdated_local;
int * analog_axis, * analog_axis_isupdated, analog_axis_isupdated_local;
int * rightanalog_axis, * rightanalog_axis_isupdated, rightanalog_axis_isupdated_local;
int * dpad_analog_swap, * dpad_analog_swap_isupdated, dpad_analog_swap_isupdated_local;
int * fan_control, * fan_control_isupdated, fan_control_isupdated_local, * fan_control_isenabled, fan_control_isenabled_local;

// File descriptors for locking memory maps
int fd_abxy_layout, fd_abxy_layout_isupdated;
int fd_performance_mode, fd_performance_mode_isupdated;
int fd_analog_sensitivity, fd_analog_sensitivity_isupdated;
int fd_analog_axis, fd_analog_axis_isupdated;
int fd_rightanalog_axis, fd_rightanalog_axis_isupdated;
int fd_dpad_analog_swap, fd_dpad_analog_swap_isupdated;
int fd_fan_control, fd_fan_control_isupdated, fd_fan_control_isenabled;

// Method declarations
static void bus_error_handler(int sig);
static void setup_abs(int fd, unsigned chan, int min, int max);
static char * send_shell_command(char * shellcmd);
static int lcd_brightness(int value);
static int get_retroarch_status();
static void set_performance_mode();
static int get_screen_status();
static int get_fan_status();
static int openAndMap(const char * filePath, int ** shared_data, int * fd);
static void setupMaps();
static void updateMapVars();
static void fanControl();
static int get_cpu_temp();
static void clearLookupTable();
static void createAnalogSensitvityCSV();
static void setAnalogSensitvityTable(int mode);

///////////////////

typedef struct {
    int firstColumn;
    int secondColumn;
} LookupEntry;

// Global variable for the lookup table
LookupEntry *lookupTable = NULL;
int lookupTableSize = 0;

static void readCSVAndBuildLookup(const char *filename);
static int lookupValue(int value);

// New global variables for mouse mode
int mouse_mode = 0;
int select_pressed_time = 0;
int r1_pressed_time = 0;
int select_and_r1_timer_started = 0;
int mouse_speed = MOUSE_ANALOG_THRESHOLD;

// New method to enable mouse mode
void enable_mouse_mode(int fd) {
    mouse_mode = 1;
    fprintf(stderr, "Mouse mode enabled\n");
    send_shell_command("su -lp 2000 -c \"am start -a android.intent.action.MAIN -e toasttext 'Mouse mode enabled. Hold down Select and R1 to disable.' -n bellavita.toast/.MainActivity\"");

    // Clear all button states
    struct input_event ev[33];
    memset(&ev, 0, sizeof ev);
    
    ev[0].type = EV_KEY;
    ev[0].code = BTN_A;
    ev[0].value = 0;
    
    ev[1].type = EV_KEY;
    ev[1].code = BTN_B;
    ev[1].value = 0;
    
    ev[2].type = EV_KEY;
    ev[2].code = BTN_X;
    ev[2].value = 0;
    
    ev[3].type = EV_KEY;
    ev[3].code = BTN_Y;
    ev[3].value = 0;
    
    ev[4].type = EV_KEY;
    ev[4].code = BTN_TL;
    ev[4].value = 0;
    
    ev[5].type = EV_KEY;
    ev[5].code = BTN_TR;
    ev[5].value = 0;
    
    ev[6].type = EV_KEY;
    ev[6].code = BTN_TL2;
    ev[6].value = 0;
    
    ev[7].type = EV_KEY;
    ev[7].code = BTN_TR2;
    ev[7].value = 0;
    
    ev[8].type = EV_KEY;
    ev[8].code = BTN_SELECT;
    ev[8].value = 0;
    
    ev[9].type = EV_KEY;
    ev[9].code = BTN_START;
    ev[9].value = 0;
    
    ev[10].type = EV_KEY;
    ev[10].code = BTN_THUMBL;
    ev[10].value = 0;
    
    ev[11].type = EV_KEY;
    ev[11].code = BTN_THUMBR;
    ev[11].value = 0;
    
    ev[12].type = EV_KEY;
    ev[12].code = BTN_DPAD_UP;
    ev[12].value = 0;
    
    ev[13].type = EV_KEY;
    ev[13].code = BTN_DPAD_DOWN;
    ev[13].value = 0;
    
    ev[14].type = EV_KEY;
    ev[14].code = BTN_DPAD_LEFT;
    ev[14].value = 0;
    
    ev[15].type = EV_KEY;
    ev[15].code = BTN_DPAD_RIGHT;
    ev[15].value = 0;
    
    ev[16].type = EV_KEY;
    ev[16].code = BTN_BACK;
    ev[16].value = 0;
    
    ev[17].type = EV_KEY;
    ev[17].code = BTN_MODE;
    ev[17].value = 0;
    
    ev[18].type = EV_KEY;
    ev[18].code = BTN_GAMEPAD;
    ev[18].value = 0;
    
    ev[19].type = EV_KEY;
    ev[19].code = KEY_VOLUMEDOWN;
    ev[19].value = 0;
    
    ev[20].type = EV_KEY;
    ev[20].code = KEY_VOLUMEUP;
    ev[20].value = 0;
    
    ev[21].type = EV_KEY;
    ev[21].code = KEY_POWER;
    ev[21].value = 0;
    
    ev[22].type = EV_ABS;
    ev[22].code = ABS_X;
    ev[22].value = 0;
    
    ev[23].type = EV_ABS;
    ev[23].code = ABS_Y;
    ev[23].value = 0;
    
    ev[24].type = EV_ABS;
    ev[24].code = ABS_Z;
    ev[24].value = 0;
    
    ev[25].type = EV_ABS;
    ev[25].code = ABS_RZ;
    ev[25].value = 0;
    
    ev[26].type = EV_ABS;
    ev[26].code = ABS_GAS;
    ev[26].value = 0;
    
    ev[27].type = EV_ABS;
    ev[27].code = ABS_BRAKE;
    ev[27].value = 0;
    
    ev[28].type = EV_ABS;
    ev[28].code = ABS_HAT0X;
    ev[28].value = 0;
    
    ev[29].type = EV_ABS;
    ev[29].code = ABS_HAT0Y;
    ev[29].value = 0;
    
    ev[30].type = EV_KEY;
    ev[30].code = BTN_1;
    ev[30].value = 0;
    
    ev[31].type = EV_KEY;
    ev[31].code = BTN_2;
    ev[31].value = 0;

    // sync event tells input layer we're done with a "batch" of updates
    ev[32].type = EV_SYN;
    ev[32].code = SYN_REPORT;
    ev[32].value = 0;

    if (write(fd, &ev, sizeof ev) < 0) {
        perror("write");
    }
}

// New method to disable mouse mode
void disable_mouse_mode(int fd) {
    mouse_mode = 0;
    fprintf(stderr, "Mouse mode disabled\n");
    // Clear all button states
    struct input_event ev[33];
    memset(&ev, 0, sizeof ev);
    
    ev[0].type = EV_KEY;
    ev[0].code = BTN_A;
    ev[0].value = 0;
    
    ev[1].type = EV_KEY;
    ev[1].code = BTN_B;
    ev[1].value = 0;
    
    ev[2].type = EV_KEY;
    ev[2].code = BTN_X;
    ev[2].value = 0;
    
    ev[3].type = EV_KEY;
    ev[3].code = BTN_Y;
    ev[3].value = 0;
    
    ev[4].type = EV_KEY;
    ev[4].code = BTN_TL;
    ev[4].value = 0;
    
    ev[5].type = EV_KEY;
    ev[5].code = BTN_TR;
    ev[5].value = 0;
    
    ev[6].type = EV_KEY;
    ev[6].code = BTN_TL2;
    ev[6].value = 0;
    
    ev[7].type = EV_KEY;
    ev[7].code = BTN_TR2;
    ev[7].value = 0;
    
    ev[8].type = EV_KEY;
    ev[8].code = BTN_SELECT;
    ev[8].value = 0;
    
    ev[9].type = EV_KEY;
    ev[9].code = BTN_START;
    ev[9].value = 0;
    
    ev[10].type = EV_KEY;
    ev[10].code = BTN_THUMBL;
    ev[10].value = 0;
    
    ev[11].type = EV_KEY;
    ev[11].code = BTN_THUMBR;
    ev[11].value = 0;
    
    ev[12].type = EV_KEY;
    ev[12].code = BTN_DPAD_UP;
    ev[12].value = 0;
    
    ev[13].type = EV_KEY;
    ev[13].code = BTN_DPAD_DOWN;
    ev[13].value = 0;
    
    ev[14].type = EV_KEY;
    ev[14].code = BTN_DPAD_LEFT;
    ev[14].value = 0;
    
    ev[15].type = EV_KEY;
    ev[15].code = BTN_DPAD_RIGHT;
    ev[15].value = 0;
    
    ev[16].type = EV_KEY;
    ev[16].code = BTN_BACK;
    ev[16].value = 0;
    
    ev[17].type = EV_KEY;
    ev[17].code = BTN_MODE;
    ev[17].value = 0;
    
    ev[18].type = EV_KEY;
    ev[18].code = BTN_GAMEPAD;
    ev[18].value = 0;
    
    ev[19].type = EV_KEY;
    ev[19].code = KEY_VOLUMEDOWN;
    ev[19].value = 0;
    
    ev[20].type = EV_KEY;
    ev[20].code = KEY_VOLUMEUP;
    ev[20].value = 0;
    
    ev[21].type = EV_KEY;
    ev[21].code = KEY_POWER;
    ev[21].value = 0;
    
    ev[22].type = EV_ABS;
    ev[22].code = ABS_X;
    ev[22].value = 0;
    
    ev[23].type = EV_ABS;
    ev[23].code = ABS_Y;
    ev[23].value = 0;
    
    ev[24].type = EV_ABS;
    ev[24].code = ABS_Z;
    ev[24].value = 0;
    
    ev[25].type = EV_ABS;
    ev[25].code = ABS_RZ;
    ev[25].value = 0;
    
    ev[26].type = EV_ABS;
    ev[26].code = ABS_GAS;
    ev[26].value = 0;
    
    ev[27].type = EV_ABS;
    ev[27].code = ABS_BRAKE;
    ev[27].value = 0;
    
    ev[28].type = EV_ABS;
    ev[28].code = ABS_HAT0X;
    ev[28].value = 0;
    
    ev[29].type = EV_ABS;
    ev[29].code = ABS_HAT0Y;
    ev[29].value = 0;
    
    ev[30].type = EV_KEY;
    ev[30].code = BTN_1;
    ev[30].value = 0;
    
    ev[31].type = EV_KEY;
    ev[31].code = BTN_2;
    ev[31].value = 0;

    // sync event tells input layer we're done with a "batch" of updates
    ev[32].type = EV_SYN;
    ev[32].code = SYN_REPORT;
    ev[32].value = 0;

    if (write(fd, &ev, sizeof ev) < 0) {
        perror("write");
    }	
}

static void bus_error_handler(int sig) {
    // Log the bus error
    fprintf(stderr, "Caught bus error (signal %d). Ignoring it.\n", sig);
}

static void setup_abs(int fd, unsigned chan, int min, int max) {
    if (ioctl(fd, UI_SET_ABSBIT, chan))
        perror("UI_SET_ABSBIT");

    struct uinput_abs_setup s = {
        .code = chan,
        .absinfo = {
            .minimum = min,
            .maximum = max
        },
    };

    if (ioctl(fd, UI_ABS_SETUP, &s))
        perror("UI_ABS_SETUP");
}

static char *send_shell_command(char *shellcmd) {
    FILE *shell_cmd_pipe;
    static char cmd_output[5000];
    shell_cmd_pipe = popen(shellcmd, "r");
    if (NULL == shell_cmd_pipe) {
        perror("pipe");
        exit(1);
    }
    fgets(cmd_output, sizeof(cmd_output), shell_cmd_pipe);
    cmd_output[strlen(cmd_output) - 1] = '\0';
    pclose(shell_cmd_pipe);

    return cmd_output;
}

static int lcd_brightness(int value) {
    int current_brightness = atoi(send_shell_command("settings get system screen_brightness"));
    if (debug_messages_enabled == 1) {
        fprintf(stderr, "Current brightness: %i\n", current_brightness);
    }

    if (value == 1 && current_brightness < 255) {
        current_brightness = current_brightness + 10;
        if (current_brightness > 255) {
            current_brightness = 255;
        }
    }
    if (value == 0 && current_brightness > 1) {
        current_brightness = current_brightness - 10;
        if (current_brightness < 1) {
            current_brightness = 1;
        }
    }

    char new_brightness[3];
    sprintf(new_brightness, "%d", current_brightness);

    char set_brightness_cmd[100] = "settings put system screen_brightness ";
    strcat(set_brightness_cmd, new_brightness);

    send_shell_command(set_brightness_cmd);

    if (debug_messages_enabled == 1) {
        fprintf(stderr, "New brightness: %i\n", current_brightness);
    }
    return current_brightness;
}

static int get_retroarch_status() {
    // Check the current value via the shell command, return 0 if not running
    int current_retroarch_status = atoi(send_shell_command("dumpsys activity activities | grep VisibleActivityProcess | grep retroarch &> /dev/null; if [ $? -eq 0 ]; then echo 1; else echo 0; fi"));
    if (debug_messages_enabled == 1) {
        fprintf(stderr, "Retroarch running status: %i\n", current_retroarch_status);
    }

    return current_retroarch_status;
}

static void set_performance_mode() {
    switch (*performance_mode) {
    case 0:
        send_shell_command("/system/bin/setclock_max.sh");
        break;
    case 1:
        send_shell_command("/system/bin/setclock_stock.sh");
        break;
    case 2:
        send_shell_command("/system/bin/setclock_powersave.sh");
        break;
    default:
        send_shell_command("/system/bin/setclock_max.sh");
    }
}

static int get_screen_status() {
    int screenstatus = atoi(send_shell_command("cat /sys/devices/platform/backlight/backlight/backlight/brightness"));

    if (screenstatus != 0) {
        screenstatus = 1;
    } else {
        screenstatus = 0;
    }

    return screenstatus;
}

static int get_fan_status() {
    int fanstatus = atoi(send_shell_command("cat /sys/devices/platform/singleadc-joypad/fan_power"));

    if (fanstatus != 0) {
        fanstatus = 1;
    } else {
        fanstatus = 0;
    }

    return fanstatus;
}

static int openAndMap(const char *filePath, int **shared_data, int *fd) {
    *fd = open(filePath, O_RDWR | O_CREAT, 0666);
    if (*fd == -1) {
        fprintf(stderr, "Error opening file: %s\n", filePath);
        return -1;
    }

    if (chmod(filePath, 0666) == -1) {
        fprintf(stderr, "Error setting file permissions: %s\n", filePath);
        close(*fd);
        return -1;
    }

    ftruncate(*fd, BUFFER_SIZE);

    *shared_data = mmap(NULL, BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, *fd, 0);
    if (*shared_data == MAP_FAILED) {
        fprintf(stderr, "Error mapping file: %s\n", filePath);
        close(*fd);
        return -1;
    }

    return 0;
}

static void setupMaps() {
    struct stat st = {0};
    if (stat(DIRECTORY_PATH, &st) == -1) {
        if (mkdir(DIRECTORY_PATH, 0777) == -1) {
            fprintf(stderr, "Error creating directory: %s\n", DIRECTORY_PATH);
            return;
        }
        chmod(DIRECTORY_PATH, 0777);
    }

    char filePath[255];

    openAndMap(strcat(strcpy(filePath, DIRECTORY_PATH), "ABXY_LAYOUT"), &abxy_layout, &fd_abxy_layout);
    openAndMap(strcat(strcpy(filePath, DIRECTORY_PATH), "ABXY_LAYOUT_ISUPDATED"), &abxy_layout_isupdated, &fd_abxy_layout_isupdated);

    openAndMap(strcat(strcpy(filePath, DIRECTORY_PATH), "PERFORMANCE_MODE"), &performance_mode, &fd_performance_mode);
    openAndMap(strcat(strcpy(filePath, DIRECTORY_PATH), "PERFORMANCE_MODE_ISUPDATED"), &performance_mode_isupdated, &fd_performance_mode_isupdated);

    openAndMap(strcat(strcpy(filePath, DIRECTORY_PATH), "ANALOG_SENSITIVITY"), &analog_sensitivity, &fd_analog_sensitivity);
    openAndMap(strcat(strcpy(filePath, DIRECTORY_PATH), "ANALOG_SENSITIVITY_ISUPDATED"), &analog_sensitivity_isupdated, &fd_analog_sensitivity_isupdated);

    openAndMap(strcat(strcpy(filePath, DIRECTORY_PATH), "ANALOG_AXIS"), &analog_axis, &fd_analog_axis);
    openAndMap(strcat(strcpy(filePath, DIRECTORY_PATH), "ANALOG_AXIS_ISUPDATED"), &analog_axis_isupdated, &fd_analog_axis_isupdated);

    openAndMap(strcat(strcpy(filePath, DIRECTORY_PATH), "RIGHTANALOG_AXIS"), &rightanalog_axis, &fd_rightanalog_axis);
    openAndMap(strcat(strcpy(filePath, DIRECTORY_PATH), "RIGHTANALOG_AXIS_ISUPDATED"), &rightanalog_axis_isupdated, &fd_rightanalog_axis_isupdated);

    openAndMap(strcat(strcpy(filePath, DIRECTORY_PATH), "DPAD_ANALOG_SWAP"), &dpad_analog_swap, &fd_dpad_analog_swap);
    openAndMap(strcat(strcpy(filePath, DIRECTORY_PATH), "DPAD_ANALOG_SWAP_ISUPDATED"), &dpad_analog_swap_isupdated, &fd_dpad_analog_swap_isupdated);

    openAndMap(strcat(strcpy(filePath, DIRECTORY_PATH), "FAN_CONTROL"), &fan_control, &fd_fan_control);
    openAndMap(strcat(strcpy(filePath, DIRECTORY_PATH), "FAN_CONTROL_ISUPDATED"), &fan_control_isupdated, &fd_fan_control_isupdated);
    openAndMap(strcat(strcpy(filePath, DIRECTORY_PATH), "FAN_CONTROL_ISENABLED"), &fan_control_isenabled, &fd_fan_control_isenabled);
}

static void updateMapVars() {
    abxy_layout_isupdated_local = *abxy_layout_isupdated;
    performance_mode_isupdated_local = *performance_mode_isupdated;
    analog_sensitivity_isupdated_local = *analog_sensitivity_isupdated;
    analog_axis_isupdated_local = *analog_axis_isupdated;
    rightanalog_axis_isupdated_local = *rightanalog_axis_isupdated;
    dpad_analog_swap_isupdated_local = *dpad_analog_swap_isupdated;
    fan_control_isupdated_local = *fan_control_isupdated;
    fan_control_isenabled_local = *fan_control_isenabled;
}

static void fanControl() {
    switch (*fan_control) {
    case 0:
        send_shell_command("/system/bin/setfan_off.sh");
        break;
    case 1:
        send_shell_command("/system/bin/setfan_auto.sh");
        break;
    case 2:
        send_shell_command("/system/bin/setfan_cool.sh");
        break;
    case 3:
        send_shell_command("/system/bin/setfan_max.sh");
        break;
    default:
        send_shell_command("/system/bin/setfan_off.sh");
    }
}

static int get_cpu_temp() {
    // Check the current value
    int current_cpu_temp = atoi(send_shell_command("cat /sys/class/thermal/thermal_zone*/temp 2>/dev/null | awk '{sum += $1; n++} END {if (n > 0) print int((sum / n + 99) / 1000)}'"));

    return current_cpu_temp;
}

static void createAnalogSensitvityCSV() {
    //------------ -5% sensitivity
    FILE *file;
    int exists = 0;
    char *filepath = "/data/rgp2xbox/DecreaseAnalogSensitivityBy15Percent.csv";

    // Check if file exists
    file = fopen(filepath, "r");
    if (file) {
        exists = 1;
        fclose(file);
    }

    // Create file if it does not exist
    if (!exists) {
        file = fopen(filepath, "w");
        if (file == NULL) {
            perror("Error opening file");
            return;
        }

        // Set file permissions to 0666
        chmod(filepath, 0666);

        // Write data to CSV
        for (int i = -32768; i <= 32768; i++) {
            double secondValue;
            if (i == 0) {
                // Keep 0 as is
                secondValue = 0;
            } else if (i > 0) {
                // For positive numbers
                secondValue = ceil(i * 0.85);
            } else {
                // For negative numbers
                secondValue = floor(i * 0.85);
            }
            fprintf(file, "%d,%.0f\n", i, secondValue);
        }

        fclose(file);
    }

    //------------ -10% sensitivity    
    exists = 0;
    filepath = "/data/rgp2xbox/DecreaseAnalogSensitivityBy25Percent.csv";

    // Check if file exists
    file = fopen(filepath, "r");
    if (file) {
        exists = 1;
        fclose(file);
    }

    // Create file if it does not exist
    if (!exists) {
        file = fopen(filepath, "w");
        if (file == NULL) {
            perror("Error opening file");
            return;
        }

        // Set file permissions to 0666
        chmod(filepath, 0666);

        // Write data to CSV
        for (int i = -32768; i <= 32768; i++) {
            double secondValue;
            if (i == 0) {
                // Keep 0 as is
                secondValue = 0;
            } else if (i > 0) {
                // For positive numbers
                secondValue = ceil(i * 0.75);
            } else {
                // For negative numbers
                secondValue = floor(i * 0.75);
            }
            fprintf(file, "%d,%.0f\n", i, secondValue);
        }

        fclose(file);
    }

    //------------ -25% sensitivity

    exists = 0;
    filepath = "/data/rgp2xbox/DecreaseAnalogSensitivityBy50Percent.csv";

    // Check if file exists
    file = fopen(filepath, "r");
    if (file) {
        exists = 1;
        fclose(file);
    }

    // Create file if it does not exist
    if (!exists) {
        file = fopen(filepath, "w");
        if (file == NULL) {
            perror("Error opening file");
            return;
        }

        // Set file permissions to 0666
        chmod(filepath, 0666);

        // Write data to CSV
        for (int i = -32768; i <= 32768; i++) {
            double secondValue;
            if (i == 0) {
                // Keep 0 as is
                secondValue = 0;
            } else if (i > 0) {
                // For positive numbers
                secondValue = ceil(i * 0.50);
            } else {
                // For negative numbers
                secondValue = floor(i * 0.50);
            }
            fprintf(file, "%d,%.0f\n", i, secondValue);
        }

        fclose(file);
    }


    //------------ custom% sensitivity

    exists = 0;
    filepath = "/data/rgp2xbox/DecreaseAnalogSensitivityCustom.csv";

    // Check if file exists
    file = fopen(filepath, "r");
    if (file) {
        exists = 1;
        fclose(file);
    }

    // Create file if it does not exist
    if (!exists) {
        file = fopen(filepath, "w");
        if (file == NULL) {
            perror("Error opening file");
            return;
        }

        // Set file permissions to 0666
        chmod(filepath, 0666);

        // Write data to CSV
        for (int i = -32768; i <= 32768; i++) {
            fprintf(file, "%d,%d\n", i, i);
        }

        fclose(file);
    }
}

static void clearLookupTable() {
    if (lookupTable != NULL) {
        free(lookupTable);
        lookupTable = NULL;
        lookupTableSize = 0;
    }
}

static void readCSVAndBuildLookup(const char *filename) {
    clearLookupTable(); // Clear any existing data

    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        return;
    }

    lookupTable = (LookupEntry *)malloc(3600 * sizeof(LookupEntry));
    if (!lookupTable) {
        perror("Memory allocation failed");
        fclose(file);
        return;
    }

    char line[100];
    int i = 0;
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = 0;

        if (sscanf(line, "%d,%d", &lookupTable[i].firstColumn, &lookupTable[i].secondColumn) == 2) {
            if (lookupTable[i].firstColumn != 0) {
                i++;
            }
        } else {
            clearLookupTable(); // Clear and free memory on error
            fclose(file);
            return;
        }
    }
    lookupTableSize = i;
    fclose(file);
}

static int lookupValue(int value) {
    if (lookupTable == NULL || lookupTableSize == 0) {
        return 0;
    }

    for (int i = 0; i < lookupTableSize; i++) {
        if (lookupTable[i].firstColumn == value) {
            return lookupTable[i].secondColumn;
        }
    }
    return 0;
}

static void setAnalogSensitvityTable(int mode) {
    int size;

    switch (mode) {
    case 1:
        readCSVAndBuildLookup("/data/rgp2xbox/DecreaseAnalogSensitivityBy15Percent.csv");
        break;
    case 2:
        readCSVAndBuildLookup("/data/rgp2xbox/DecreaseAnalogSensitivityBy25Percent.csv");
        break;
    case 3:
        readCSVAndBuildLookup("/data/rgp2xbox/DecreaseAnalogSensitivityBy50Percent.csv");
        break;
    case 4:
        readCSVAndBuildLookup("/data/rgp2xbox/DecreaseAnalogSensitivityCustom.csv");
        break;
    default:
        *analog_sensitivity = 0;
        *analog_axis_isupdated = 1;
        break;
    }
}

int main(void) {
    // Set up the signal handler for SIGBUS
    struct sigaction sa;
    sa.sa_handler = bus_error_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGBUS, &sa, NULL) == -1) {
        perror("Error setting signal handler for SIGBUS");
        exit(EXIT_FAILURE);
    }

    setupMaps();
    createAnalogSensitvityCSV();
    fprintf(stderr, "Open /dev/uinput...\n");
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);

    if (fd < 0) {
        perror("open /dev/uinput");
        return 1;
    }

    fprintf(stderr, "Open /dev/uinput for mouse...\n");
    int mouse_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);

    if (mouse_fd < 0) {
        perror("open /dev/uinput for mouse");
        return 1;
    }

    fprintf(stderr, "Set up virtual controller keys and analog options...\n");
    ioctl(fd, UI_SET_EVBIT, EV_KEY); // enable button/key handling

    ioctl(fd, UI_SET_KEYBIT, BTN_A);
    ioctl(fd, UI_SET_KEYBIT, BTN_B);
    ioctl(fd, UI_SET_KEYBIT, BTN_C);
    ioctl(fd, UI_SET_KEYBIT, BTN_X);
    ioctl(fd, UI_SET_KEYBIT, BTN_Y);
    ioctl(fd, UI_SET_KEYBIT, BTN_Z);
    ioctl(fd, UI_SET_KEYBIT, BTN_1);
    ioctl(fd, UI_SET_KEYBIT, BTN_2);
    ioctl(fd, UI_SET_KEYBIT, BTN_TL);
    ioctl(fd, UI_SET_KEYBIT, BTN_TR);
    ioctl(fd, UI_SET_KEYBIT, BTN_TL2);
    ioctl(fd, UI_SET_KEYBIT, BTN_TR2);
    ioctl(fd, UI_SET_KEYBIT, BTN_START);
    ioctl(fd, UI_SET_KEYBIT, BTN_SELECT);
    ioctl(fd, UI_SET_KEYBIT, BTN_THUMBL);
    ioctl(fd, UI_SET_KEYBIT, BTN_THUMBR);
    ioctl(fd, UI_SET_KEYBIT, BTN_DPAD_UP);
    ioctl(fd, UI_SET_KEYBIT, BTN_DPAD_DOWN);
    ioctl(fd, UI_SET_KEYBIT, BTN_DPAD_LEFT);
    ioctl(fd, UI_SET_KEYBIT, BTN_DPAD_RIGHT);
    ioctl(fd, UI_SET_KEYBIT, BTN_BACK);
    ioctl(fd, UI_SET_KEYBIT, BTN_MODE);
    ioctl(fd, UI_SET_KEYBIT, BTN_GAMEPAD);
    ioctl(fd, UI_SET_KEYBIT, KEY_VOLUMEDOWN);
    ioctl(fd, UI_SET_KEYBIT, KEY_VOLUMEUP);
    ioctl(fd, UI_SET_KEYBIT, KEY_POWER);

    ioctl(fd, UI_SET_EVBIT, EV_ABS); // enable analog absolute position handling

    setup_abs(fd, ABS_X, -32768, 32768);
    setup_abs(fd, ABS_Y, -32768, 32768);

    setup_abs(fd, ABS_Z, -32768, 32768);
    setup_abs(fd, ABS_RZ, -32768, 32768);

    setup_abs(fd, ABS_GAS, 0, 1);
    setup_abs(fd, ABS_BRAKE, 0, 1);

    setup_abs(fd, ABS_HAT0X, -1, 1);
    setup_abs(fd, ABS_HAT0Y, -1, 1);

    fprintf(stderr, "Set up virtual mouse keys and relative options...\n");
    ioctl(mouse_fd, UI_SET_EVBIT, EV_KEY); // enable button/key handling for mouse
    ioctl(mouse_fd, UI_SET_KEYBIT, BTN_LEFT);
    ioctl(mouse_fd, UI_SET_KEYBIT, BTN_RIGHT);
	ioctl(mouse_fd, UI_SET_RELBIT, REL_WHEEL);
	ioctl(mouse_fd, UI_SET_RELBIT, REL_HWHEEL);

    ioctl(mouse_fd, UI_SET_EVBIT, EV_REL); // enable relative position handling for mouse
    ioctl(mouse_fd, UI_SET_RELBIT, REL_X);
    ioctl(mouse_fd, UI_SET_RELBIT, REL_Y);

    fprintf(stderr, "Assign virtual controller as Microsoft X-Box One S Controller...\n");
    struct uinput_setup setup = {
        .name = "Xbox Wireless Controller",
        .id = {
            .bustype = BUS_USB,
            .vendor = 0x045e,
            .product = 0x02fd,
            .version = 0x0003,
        }
    };

    fprintf(stderr, "Finalize virtual controller configuration...\n");
    if (ioctl(fd, UI_DEV_SETUP, &setup)) {
        perror("UI_DEV_SETUP");
        return 1;
    }

    fprintf(stderr, "Assign virtual mouse as Generic Mouse...\n");
    struct uinput_setup mouse_setup = {
        .name = "Generic Mouse",
        .id = {
            .bustype = BUS_USB,
            .vendor = 0x045e,
            .product = 0x02ff,
            .version = 0x0003,
        }
    };

    fprintf(stderr, "Finalize virtual mouse configuration...\n");
    if (ioctl(mouse_fd, UI_DEV_SETUP, &mouse_setup)) {
        perror("UI_DEV_SETUP for mouse");
        return 1;
    }

    fprintf(stderr, "Create virtual controller uinput device...\n");
    if (ioctl(fd, UI_DEV_CREATE)) {
        perror("UI_DEV_CREATE");
        return 1;
    }

    fprintf(stderr, "Create virtual mouse uinput device...\n");
    if (ioctl(mouse_fd, UI_DEV_CREATE)) {
        perror("UI_DEV_CREATE for mouse");
        return 1;
    }

    // Create /dev/input/event# string by using grep to get physical retrogame_joypad event number
    char openrgp[1000] = "/dev/input/";
    strcat(openrgp, send_shell_command("grep -E 'Name|Handlers|Phys=' /proc/bus/input/devices | grep -A2 magicx-input | grep -Eo 'event[0-9]+'"));
    fprintf(stderr, "Physical magicx-input: %s\nReady.\n", openrgp);

    // Open physical_retrogame_joypad, with exclusive access to this application only
    int physical_retrogame_joypad = open(openrgp, O_RDWR | O_NONBLOCK, S_IRUSR | S_IWUSR);
    ioctl(physical_retrogame_joypad, EVIOCGRAB, 1);

    char rgpremove[1000] = "rm ";
    strcat(rgpremove, openrgp);
    send_shell_command(rgpremove);

    // Define data structure to capture physical inputs
    struct input_event ie;

    // Create /dev/input/event# string by using grep to get physical retrogame_joypad event number
    char opengpio[1000] = "/dev/input/";
    strcat(opengpio, send_shell_command("grep -E 'Name|Handlers|Phys=' /proc/bus/input/devices | grep -A1 gpio-keys/ | grep -Eo 'event[0-9]+'"));
    fprintf(stderr, "Physical gpio_keys: %s\nReady.\n", opengpio);

    // Open gpio-=keys, no exclusive access 
    int physical_gpio_keys = open(opengpio, O_RDWR | O_NONBLOCK, S_IRUSR | S_IWUSR);
    ioctl(physical_gpio_keys, EVIOCGRAB, 1);

    // Define data structure to capture physical inputs
    struct input_event gpioie;

    // Create /dev/input/event# string by using grep to get physical adc-keys event number
    char openadckeys[1000] = "/dev/input/";
    strcat(openadckeys, send_shell_command("grep -E 'Name|Handlers|Phys=' /proc/bus/input/devices | grep -A1 adc-keys/ | grep -Eo 'event[0-9]+'"));
    fprintf(stderr, "Physical gpio_keys: %s\nReady.\n", openadckeys);

    // Open adc-keys, exclusive access 
    int physical_adc_keys = open(openadckeys, O_RDWR | O_NONBLOCK, S_IRUSR | S_IWUSR);
    ioctl(physical_adc_keys, EVIOCGRAB, 1);

    // Define data structure to capture physical inputs
    struct input_event adckeysie;

    // you can write events one at a time, but to save overhead we'll
    // update all of them in a single write

    unsigned count = 0;

    int PHYSICAL_BTN_A = 0;
    int PHYSICAL_BTN_B = 0;
    int PHYSICAL_BTN_C = 0;
    int PHYSICAL_BTN_X = 0;
    int PHYSICAL_BTN_Y = 0;
    int PHYSICAL_BTN_Z = 0;
    int PHYSICAL_BTN_TL = 0;
    int PHYSICAL_BTN_TR = 0;
    int PHYSICAL_BTN_TL2 = 0;
    int PHYSICAL_BTN_TR2 = 0;
    int PHYSICAL_BTN_START = 0;
    int PHYSICAL_BTN_SELECT = 0;
    int PHYSICAL_BTN_THUMBL = 0;
    int PHYSICAL_BTN_THUMBR = 0;
    int PHYSICAL_BTN_DPAD_UP = 0;
    int PHYSICAL_BTN_DPAD_DOWN = 0;
    int PHYSICAL_BTN_DPAD_LEFT = 0;
    int PHYSICAL_BTN_DPAD_RIGHT = 0;
    int PHYSICAL_BTN_BACK = 0;
    int PHYSICAL_BTN_HOME = 0;
    int PHYSICAL_BTN_VOLUMEDOWN = 0;
    int PHYSICAL_BTN_VOLUMEUP = 0;
    int PHYSICAL_BTN_POWER = 0;
    int VIRTUAL_BTN_MODE = 0;
    int VIRTUAL_BTN_1 = 0;
    int VIRTUAL_BTN_2 = 0;

    int PHYSICAL_HAT_X = 0;
    int PHYSICAL_HAT_Y = 0;

    int PHYSICAL_ABS_Y = 0;
    int PHYSICAL_ABS_X = 0;

    int PHYSICAL_ABS_Z = 0;
    int PHYSICAL_ABS_RZ = 0;

    int homepressed = 0;

    int backpressed = 0;
    int backcount = 0;
    int backpresscomplete = 0;
    int homepresscomplete = 0;
    int bpresscomplete = 0;

    // Check for screen on
    int screenison = 1;
    int isadjustingbrightness = 0;

    int menutoggleactivated = 0;
    int menutogglecompleted = 0;

    int isfanturnedoffduringsleep = 0;
    int fanison = 0;
    set_performance_mode();

    if (*analog_sensitivity != 0) {
        setAnalogSensitvityTable(*analog_sensitivity);
    }

    while (1) {
        // Update screen status and MMAP variables
        if (count % 250 == 0) {
            updateMapVars();
        }

        if (analog_sensitivity_isupdated_local == 1) {
            *analog_sensitivity_isupdated = 0;
            analog_sensitivity_isupdated_local = 0;
            if (*analog_sensitivity != 0) {
                setAnalogSensitvityTable(*analog_sensitivity);
            }
        }

        read(physical_gpio_keys, &gpioie, sizeof(struct input_event));
        // Read physical gpio-keys inputs
        if (gpioie.type == 1) {
            screenison = 1;

            if (debug_messages_enabled == 1) {
                fprintf(stderr, "time:%ld.%06ld\ttype:%u\tcode:%u\tvalue:%d\n", gpioie.time.tv_sec, gpioie.time.tv_usec, gpioie.type, gpioie.code, gpioie.value);
            }
            if (gpioie.code == 114) {
                PHYSICAL_BTN_VOLUMEDOWN = gpioie.value;
            }
            if (gpioie.code == 115) {
                PHYSICAL_BTN_VOLUMEUP = gpioie.value;
            }
            if (gpioie.code == 116) {
                PHYSICAL_BTN_POWER = gpioie.value;
            }
        }

        //Read input on adc buttons            
        read(physical_adc_keys, &adckeysie, sizeof(struct input_event));

        // Read physical adc-keys inputs
        if (adckeysie.type == 1 && adckeysie.code == 158) {
            if (debug_messages_enabled == 1) {
                fprintf(stderr, "ADCKEYS time:%ld.%06ld\ttype:%u\tcode:%u\tvalue:%d\n", adckeysie.time.tv_sec, adckeysie.time.tv_usec, adckeysie.type, adckeysie.code, adckeysie.value);
            }
        }

        struct input_event ev[33];
        memset(&ev, 0, sizeof ev);
        read(physical_retrogame_joypad, &ie, sizeof(struct input_event));

        // Read physical retrogame_joypad inputs and update our virtual gamepad inputs accordingly
        if (ie.type != 0) {
            if (debug_messages_enabled == 1 && ie.value != 127) {
                fprintf(stderr, "time:%ld.%06ld\ttype:%u\tcode:%u\tvalue:%d\n", ie.time.tv_sec, ie.time.tv_usec, ie.type, ie.code, ie.value);
            }


            if (ie.code == 114) {
                PHYSICAL_BTN_VOLUMEDOWN = ie.value;
            }
            if (ie.code == 115) {
                PHYSICAL_BTN_VOLUMEUP = ie.value;
            }
			
            // L1
            if (ie.code == 310) {
                PHYSICAL_BTN_TL = ie.value;
            }

            // L2
            if (ie.code == 312) {
                PHYSICAL_BTN_TL2 = ie.value;
            }

            // L3
            if (ie.code == 317) {
                PHYSICAL_BTN_THUMBL = ie.value;
            }

            // R1
            if (ie.code == 311) {
                PHYSICAL_BTN_TR = ie.value;
            }

            // R2
            if (ie.code == 313) {
                PHYSICAL_BTN_TR2 = ie.value;
            }

            // R3
            if (ie.code == 318) {
                PHYSICAL_BTN_THUMBR = ie.value;
            }

            // A
            if (ie.code == 304) {
                PHYSICAL_BTN_A = ie.value;
            }

            // B
            if (ie.code == 305) {
                PHYSICAL_BTN_B = ie.value;
            }

            // C
            if (ie.code == 306) {
                PHYSICAL_BTN_C = ie.value;
            }

            // X
            if (ie.code == 307) {
                PHYSICAL_BTN_X = ie.value;
            }

            // Y
            if (ie.code == 308) {
                PHYSICAL_BTN_Y = ie.value;
            }

            // Z
            if (ie.code == 309) {
                PHYSICAL_BTN_Z = ie.value;
            }

            // SELECT
            if (ie.code == 314) {
                PHYSICAL_BTN_SELECT = ie.value;
            }

            // START
            if (ie.code == 315) {
                PHYSICAL_BTN_START = ie.value;
            }

            // BACK
            if (ie.code == 158) {
                PHYSICAL_BTN_BACK = ie.value;
            }

            // HOME
            if (ie.code == 68) {
                PHYSICAL_BTN_HOME = ie.value;
            }

            // MODE - RG503
            if (ie.code == 316) {
                PHYSICAL_BTN_BACK = ie.value;
            }

            // MODE - HYBRID USAGE WITH BACK BUTTON
            if (VIRTUAL_BTN_MODE == 1) {
                PHYSICAL_BTN_BACK = 0;
            }

			// DPAD UP/DOWN
			// Original codes: 17 (vertical axis), 544 (UP), 545 (DOWN)
			// New codes: 103 (UP), 108 (DOWN)
			if (ie.code == 17 || ie.code == 544 || ie.code == 545 || ie.code == 103 || ie.code == 108) {
				// Check if we’re swapping DPAD with left analog
				if (*dpad_analog_swap == 1 && (ie.code == 17 || ie.code == 544 || ie.code == 545 || ie.code == 103 || ie.code == 108)) {
					// Swap logic: controlling PHYSICAL_ABS_Y instead of PHYSICAL_HAT_Y
					if (ie.code == 17 && ie.value == 1) {
						PHYSICAL_ABS_Y = 32768;
					} else if (ie.code == 17 && ie.value == -1) {
						PHYSICAL_ABS_Y = -32768;
					} 
					else if ((ie.code == 544 || ie.code == 103) && ie.value == 1) {
						// Both 544 and 103 are "DPAD UP", which normally means negative Y
						PHYSICAL_ABS_Y = -32768;
					} 
					else if ((ie.code == 545 || ie.code == 108) && ie.value == 1) {
						// Both 545 and 108 are "DPAD DOWN", which normally means positive Y
						PHYSICAL_ABS_Y = 32768;
					} 
					else {
						PHYSICAL_ABS_Y = 0;
					}
				} else {
					// Normal DPAD logic (no swap)
					if (ie.code == 17) {
						// '17' often means up (value = -1) or down (value = 1)
						PHYSICAL_HAT_Y = ie.value; 
					} else {
						// 544, 103 = UP (so Y = -1 when pressed) 
						// 545, 108 = DOWN (so Y = 1 when pressed)
						if (ie.code == 544 || ie.code == 103) {
							PHYSICAL_HAT_Y = -ie.value;
						}
						if (ie.code == 545 || ie.code == 108) {
							PHYSICAL_HAT_Y = ie.value;
						}
					}
				}
			}

			// DPAD LEFT/RIGHT
			// Original codes: 16 (horizontal axis), 546 (LEFT), 547 (RIGHT)
			// New codes: 105 (LEFT), 106 (RIGHT)
			if (ie.code == 16 || ie.code == 546 || ie.code == 547 || ie.code == 105 || ie.code == 106) {
				// Check if we’re swapping DPAD with left analog
				if (*dpad_analog_swap == 1 && (ie.code == 16 || ie.code == 546 || ie.code == 547 || ie.code == 105 || ie.code == 106)) {
					// Swap logic: controlling PHYSICAL_ABS_X instead of PHYSICAL_HAT_X
					if (ie.code == 16 && ie.value == 1) {
						PHYSICAL_ABS_X = 32768;
					} else if (ie.code == 16 && ie.value == -1) {
						PHYSICAL_ABS_X = -32768;
					} 
					else if ((ie.code == 546 || ie.code == 105) && ie.value == 1) {
						// Both 546 and 105 are "DPAD LEFT", which normally means negative X
						PHYSICAL_ABS_X = -32768;
					} 
					else if ((ie.code == 547 || ie.code == 106) && ie.value == 1) {
						// Both 547 and 106 are "DPAD RIGHT", which normally means positive X
						PHYSICAL_ABS_X = 32768;
					} 
					else {
						PHYSICAL_ABS_X = 0;
					}
				} else {
					// Normal DPAD logic (no swap)
					if (ie.code == 16) {
						// '16' often means left (value = -1) or right (value = 1)
						PHYSICAL_HAT_X = ie.value;
					} else {
						// 546, 105 = LEFT (so X = -1 when pressed)
						// 547, 106 = RIGHT (so X = 1 when pressed)
						if (ie.code == 546 || ie.code == 105) {
							PHYSICAL_HAT_X = -ie.value;
						}
						if (ie.code == 547 || ie.code == 106) {
							PHYSICAL_HAT_X = ie.value;
						}
					}
				}
			}

            // LEFT ANALOG Y
            if (ie.code == 1) {
                if (*dpad_analog_swap == 1) {
                    if (ie.value < 10000 && ie.value > -10000) {
                        PHYSICAL_HAT_Y = 0;
                    }
                    if (ie.value >= 10000) {
                        PHYSICAL_HAT_Y = 1;
                    }
                    if (ie.value <= -10000) {
                        PHYSICAL_HAT_Y = -1;
                    }
                } else {
                    if (*analog_sensitivity != 0) {
                        PHYSICAL_ABS_Y = lookupValue(ie.value);
                    } else {
                        PHYSICAL_ABS_Y = ie.value;
                    }
                }
            }

            // LEFT ANALOG X
            if (ie.code == 0) {
                if (*dpad_analog_swap == 1) {
                    if (ie.value < 10000 && ie.value > -10000) {
                        PHYSICAL_HAT_X = 0;
                    }
                    if (ie.value >= 10000) {
                        PHYSICAL_HAT_X = 1;
                    }
                    if (ie.value <= -10000) {
                        PHYSICAL_HAT_X = -1;
                    }
                } else {
                    if (*analog_sensitivity != 0) {
                        PHYSICAL_ABS_X = lookupValue(ie.value);
                    } else {
                        PHYSICAL_ABS_X = ie.value;
                    }
                }
            }

            // RIGHT ANALOG Y
            if (ie.code == 2 || ie.code == 3) {
                if (*analog_sensitivity != 0) {
                    PHYSICAL_ABS_Z = lookupValue(ie.value);
                } else {
                    PHYSICAL_ABS_Z = ie.value;
                }
            }

            // RIGHT ANALOG X
            if (ie.code == 5 || ie.code == 4) {
                if (*analog_sensitivity != 0) {
                    PHYSICAL_ABS_RZ = lookupValue(ie.value);
                } else {
                    PHYSICAL_ABS_RZ = ie.value;
                }
            }
        }

        struct input_event mouse_ev[3];
        memset(&mouse_ev, 0, sizeof mouse_ev);

        if (mouse_mode) {
            // Adjust mouse speed based on X button press
            if (PHYSICAL_BTN_X == 1) {
                mouse_speed = MOUSE_ANALOG_SPEED * 2;
            } else {
                mouse_speed = MOUSE_ANALOG_SPEED;
            }

            // Mouse movement using DPAD
            if (PHYSICAL_HAT_Y == 1) {
                mouse_ev[0].type = EV_REL;
                mouse_ev[0].code = REL_Y;
                mouse_ev[0].value = mouse_speed;
            } else if (PHYSICAL_HAT_Y == -1) {
                mouse_ev[0].type = EV_REL;
                mouse_ev[0].code = REL_Y;
                mouse_ev[0].value = -mouse_speed;
            }

            if (PHYSICAL_HAT_X == 1) {
                mouse_ev[1].type = EV_REL;
                mouse_ev[1].code = REL_X;
                mouse_ev[1].value = mouse_speed;
            } else if (PHYSICAL_HAT_X == -1) {
                mouse_ev[1].type = EV_REL;
                mouse_ev[1].code = REL_X;
                mouse_ev[1].value = -mouse_speed;
            }

            // Mouse movement using LEFT ANALOG
            if (PHYSICAL_ABS_Y > MOUSE_ANALOG_THRESHOLD && (count % 2 == 0)) {
                mouse_ev[0].type = EV_REL;
                mouse_ev[0].code = REL_Y;
				mouse_ev[0].value = round(PHYSICAL_ABS_Y / 10000.00 * mouse_speed);
            } else if (PHYSICAL_ABS_Y < -MOUSE_ANALOG_THRESHOLD && (count % 3 == 0)) {
                mouse_ev[0].type = EV_REL;
                mouse_ev[0].code = REL_Y;
                mouse_ev[0].value = round(PHYSICAL_ABS_Y / 10000.00 * mouse_speed);
            }

            if (PHYSICAL_ABS_X > MOUSE_ANALOG_THRESHOLD && (count % 3 == 0)) {
                mouse_ev[1].type = EV_REL;
                mouse_ev[1].code = REL_X;
                mouse_ev[1].value = round(PHYSICAL_ABS_X / 10000.00 * mouse_speed);
            } else if (PHYSICAL_ABS_X < -MOUSE_ANALOG_THRESHOLD && (count % 3 == 0)) {
                mouse_ev[1].type = EV_REL;
                mouse_ev[1].code = REL_X;
                mouse_ev[1].value = round(PHYSICAL_ABS_X / 10000.00 * mouse_speed);
            }

            mouse_ev[2].type = EV_SYN;
            mouse_ev[2].code = SYN_REPORT;
            mouse_ev[2].value = 0;

            if (write(mouse_fd, &mouse_ev, sizeof(mouse_ev)) < 0) {
                perror("write mouse event");
                return 1;
            }

            // Mouse click emulation
            struct input_event mouse_click_ev[2];
            memset(&mouse_click_ev, 0, sizeof mouse_click_ev);

            // Left mouse click
            if (PHYSICAL_BTN_A == 1) {
                mouse_click_ev[0].type = EV_KEY;
                mouse_click_ev[0].code = BTN_LEFT;
                mouse_click_ev[0].value = 1;

                mouse_click_ev[1].type = EV_SYN;
                mouse_click_ev[1].code = SYN_REPORT;
                mouse_click_ev[1].value = 0;

                if (write(mouse_fd, &mouse_click_ev, sizeof(mouse_click_ev)) < 0) {
                    perror("write mouse click event");
                    return 1;
                }
            } else if (PHYSICAL_BTN_A == 0) {
                mouse_click_ev[0].type = EV_KEY;
                mouse_click_ev[0].code = BTN_LEFT;
                mouse_click_ev[0].value = 0;

                mouse_click_ev[1].type = EV_SYN;
                mouse_click_ev[1].code = SYN_REPORT;
                mouse_click_ev[1].value = 0;

                if (write(mouse_fd, &mouse_click_ev, sizeof(mouse_click_ev)) < 0) {
                    perror("write mouse click event");
                    return 1;
                }
            }

            // Right mouse click
            if (PHYSICAL_BTN_Y == 1) {
                mouse_click_ev[0].type = EV_KEY;
                mouse_click_ev[0].code = BTN_RIGHT;
                mouse_click_ev[0].value = 1;

                mouse_click_ev[1].type = EV_SYN;
                mouse_click_ev[1].code = SYN_REPORT;
                mouse_click_ev[1].value = 0;

                if (write(mouse_fd, &mouse_click_ev, sizeof(mouse_click_ev)) < 0) {
                    perror("write mouse click event");
                    return 1;
                }
            } else if (PHYSICAL_BTN_Y == 0) {
                mouse_click_ev[0].type = EV_KEY;
                mouse_click_ev[0].code = BTN_RIGHT;
                mouse_click_ev[0].value = 0;

                mouse_click_ev[1].type = EV_SYN;
                mouse_click_ev[1].code = SYN_REPORT;
                mouse_click_ev[1].value = 0;

                if (write(mouse_fd, &mouse_click_ev, sizeof(mouse_click_ev)) < 0) {
                    perror("write mouse click event");
                    return 1;
                }
            }

            // Send input keyevent 4 for B button
            if (PHYSICAL_BTN_B == 1 && bpresscomplete == 0) {
                send_shell_command("input keyevent 4");
                bpresscomplete = 1;
            }

            if (PHYSICAL_BTN_B == 0) {
                bpresscomplete = 0;
            }
			
			struct input_event scroll_ev[4];
			memset(&scroll_ev, 0, sizeof scroll_ev);

			// Scroll wheel emulation using RIGHT ANALOG Y (PHYSICAL_ABS_RZ)
			if ((PHYSICAL_ABS_RZ < -MOUSE_ANALOG_THRESHOLD) && PHYSICAL_ABS_RZ != 0 && count % 20 == 0) {
				scroll_ev[0].type = EV_REL;
				scroll_ev[0].code = REL_WHEEL;
				scroll_ev[0].value = -floor(PHYSICAL_ABS_RZ / 500.00 * mouse_speed) ; // Scale scrolling speed
			} else if ((PHYSICAL_ABS_RZ > MOUSE_ANALOG_THRESHOLD) && PHYSICAL_ABS_RZ != 0 && count % 20 == 0) {
				scroll_ev[0].type = EV_REL;
				scroll_ev[0].code = REL_WHEEL;
				scroll_ev[0].value = -ceil(PHYSICAL_ABS_RZ / 500.00 * mouse_speed); // Scale scrolling speed
			} else if (PHYSICAL_BTN_TL == 1 && count % 20 == 0) {
				scroll_ev[0].type = EV_REL;
				scroll_ev[0].code = REL_WHEEL;
				scroll_ev[0].value = 1 * mouse_speed; // Scale scrolling speed
			} else if (PHYSICAL_BTN_TR == 1 && PHYSICAL_BTN_SELECT == 0 && count % 20 == 0) {
				scroll_ev[0].type = EV_REL;
				scroll_ev[0].code = REL_WHEEL;
				scroll_ev[0].value = -1 * mouse_speed; // Scale scrolling speed
			} else {
				scroll_ev[0].type = EV_REL;
				scroll_ev[0].code = REL_WHEEL;
				scroll_ev[0].value = 0;
			}
			
			scroll_ev[1].type = EV_SYN;
			scroll_ev[1].code = SYN_REPORT;
			scroll_ev[1].value = 0;
			
			// Horizontal scroll wheel emulation using RIGHT ANALOG X (PHYSICAL_ABS_Z)
			if ((PHYSICAL_ABS_Z < -MOUSE_ANALOG_THRESHOLD) && PHYSICAL_ABS_Z != 0 && count % 20 == 0) {
				scroll_ev[1].type = EV_REL;
				scroll_ev[1].code = REL_HWHEEL;
				scroll_ev[1].value = ceil(PHYSICAL_ABS_Z / 500.00 * mouse_speed); // Scale scrolling speed
			} else if ((PHYSICAL_ABS_Z > MOUSE_ANALOG_THRESHOLD) && PHYSICAL_ABS_Z != 0 && count % 20 == 0) {
				scroll_ev[1].type = EV_REL;
				scroll_ev[1].code = REL_HWHEEL;
				scroll_ev[1].value = floor(PHYSICAL_ABS_Z / 500.00 * mouse_speed); // Scale scrolling speed
			} else if (PHYSICAL_BTN_TL2 == 1 && count % 20 == 0) {
				scroll_ev[1].type = EV_REL;
				scroll_ev[1].code = REL_HWHEEL;
				scroll_ev[1].value = -1 * mouse_speed;
			} else if (PHYSICAL_BTN_TR2 == 1 && count % 20 == 0) {
				scroll_ev[1].type = EV_REL;
				scroll_ev[1].code = REL_HWHEEL;
				scroll_ev[1].value = 1 * mouse_speed;
			}  else {
				scroll_ev[1].type = EV_REL;
				scroll_ev[1].code = REL_HWHEEL;
				scroll_ev[1].value = 0;
			}

			// Synchronization event
			scroll_ev[2].type = EV_SYN;
			scroll_ev[2].code = SYN_REPORT;
			scroll_ev[2].value = 0;
			
			if (write(mouse_fd, &scroll_ev, sizeof(scroll_ev)) < 0) {
				perror("write mouse scroll event");
				return 1;
			}

        } else {
            ev[0].type = EV_KEY;
            if (*abxy_layout == 0) {
                ev[0].code = BTN_A;
            } else {
                ev[0].code = BTN_B;
            }
            ev[0].value = PHYSICAL_BTN_A;

            ev[1].type = EV_KEY;
            if (*abxy_layout == 0) {
                ev[1].code = BTN_B;
            } else {
                ev[1].code = BTN_A;
            }
            ev[1].value = PHYSICAL_BTN_B;

            ev[2].type = EV_KEY;
            ev[2].code = BTN_TL2;
            ev[2].value = PHYSICAL_BTN_TL2;

            ev[3].type = EV_KEY;
            ev[3].code = BTN_TR2;
            ev[3].value = PHYSICAL_BTN_TR2;

            ev[4].type = EV_ABS;
            ev[4].code = ABS_Y;
            if (*analog_axis == 0) {
                ev[4].value = PHYSICAL_ABS_Y;
            } else {
                ev[4].value = -PHYSICAL_ABS_Y;
            }

            ev[5].type = EV_ABS;
            ev[5].code = ABS_X;
            if (*analog_axis == 0) {
                ev[5].value = PHYSICAL_ABS_X;
            } else {
                ev[5].value = -PHYSICAL_ABS_X;
            }

            ev[6].type = EV_ABS;
            ev[6].code = ABS_GAS;
            ev[6].value = PHYSICAL_BTN_TR2;

            ev[7].type = EV_ABS;
            ev[7].code = ABS_BRAKE;
            ev[7].value = PHYSICAL_BTN_TL2;

            ev[8].type = EV_KEY;
            ev[8].code = 66;
            ev[8].value = PHYSICAL_BTN_HOME;

            ev[9].type = EV_KEY;
            ev[9].code = BTN_TL;
            ev[9].value = PHYSICAL_BTN_TL;

            ev[10].type = EV_ABS;
            ev[10].code = ABS_Z;
            if (*rightanalog_axis == 0) {
                ev[10].value = PHYSICAL_ABS_Z;
            } else {
                ev[10].value = -PHYSICAL_ABS_Z;
            }

            ev[11].type = EV_KEY;
            ev[11].code = BTN_TR;
            ev[11].value = PHYSICAL_BTN_TR;

            ev[13].type = EV_KEY;
            if (*abxy_layout == 0) {
                ev[13].code = BTN_X;
            } else {
                ev[13].code = BTN_Y;
            }
            ev[13].value = PHYSICAL_BTN_X;

            ev[14].type = EV_KEY;
            if (*abxy_layout == 0) {
                ev[14].code = BTN_Y;
            } else {
                ev[14].code = BTN_X;
            }
            ev[14].value = PHYSICAL_BTN_Y;

			// Update BTN_THUMBL logic
			ev[15].type = EV_KEY;
			ev[15].code = BTN_THUMBL;
			if (PHYSICAL_BTN_C == 1 || PHYSICAL_BTN_THUMBL == 1) {
				ev[15].value = 1; // Register as pressed if either C or THUMBL is pressed
			} else {
				ev[15].value = 0; // Not pressed otherwise
			}

			// Update BTN_THUMBR logic
			ev[16].type = EV_KEY;
			ev[16].code = BTN_THUMBR;
			if (PHYSICAL_BTN_Z == 1 || PHYSICAL_BTN_THUMBR == 1) {
				ev[16].value = 1; // Register as pressed if either Z or THUMBR is pressed
			} else {
				ev[16].value = 0; // Not pressed otherwise
			}

            ev[17].type = EV_KEY;
            ev[17].code = BTN_SELECT;
            if (VIRTUAL_BTN_MODE == 1) {
                ev[17].value = VIRTUAL_BTN_MODE;
            } else {
                ev[17].value = PHYSICAL_BTN_SELECT;
            }

            ev[18].type = EV_KEY;
            ev[18].code = BTN_START;
            ev[18].value = PHYSICAL_BTN_START;

            ev[19].type = EV_KEY;
            ev[19].code = BTN_BACK;
            ev[19].value = PHYSICAL_BTN_BACK;

            ev[20].type = EV_KEY;
            ev[20].code = BTN_DPAD_UP;
            ev[20].value = PHYSICAL_BTN_DPAD_UP;

            ev[21].type = EV_ABS;
            ev[21].code = ABS_HAT0Y;
            ev[21].value = PHYSICAL_HAT_Y;

            ev[22].type = EV_ABS;
            ev[22].code = ABS_HAT0X;
            ev[22].value = PHYSICAL_HAT_X;

            ev[23].type = EV_ABS;
            ev[23].code = ABS_RZ;
            if (*rightanalog_axis == 0) {
                ev[23].value = PHYSICAL_ABS_RZ;
            } else {
                ev[23].value = -PHYSICAL_ABS_RZ;
            }

            ev[26].type = EV_KEY;
            ev[26].code = BTN_1;
            ev[26].value = VIRTUAL_BTN_1;

            ev[27].type = EV_KEY;
            ev[27].code = BTN_2;
            ev[27].value = VIRTUAL_BTN_2;

            ev[28].type = EV_KEY;
            ev[28].code = KEY_POWER;
            ev[28].value = PHYSICAL_BTN_POWER;

            if (PHYSICAL_BTN_BACK == 0 && isadjustingbrightness == 0) {
                ev[29].type = EV_KEY;
                ev[29].code = KEY_VOLUMEDOWN;
                ev[29].value = PHYSICAL_BTN_VOLUMEDOWN;

                ev[30].type = EV_KEY;
                ev[30].code = KEY_VOLUMEUP;
                ev[30].value = PHYSICAL_BTN_VOLUMEUP;
            }

// ...
// after we've filled ev[0..30], then ev[31] is the SYN_REPORT
ev[31].type  = EV_SYN;
ev[31].code  = SYN_REPORT;
ev[31].value = 0;

if (!mouse_mode)
{
    // We'll keep a static oldEv buffer to compare with
    static struct input_event oldEv[32];
    static int oldEvInitialized = 0;

    // Decide if there's any difference in the 31 input events
    // (index 31 is just SYN, we can skip or include it).
    int changed = 0;
    if (!oldEvInitialized)
    {
        // First time we run, we treat it as changed (so we send once).
        changed = 1;
        oldEvInitialized = 1;
    }
    else
    {
        // Compare each field (type, code, value) of the first 31 entries
        // (the 32nd is just the SYN, typically won't matter).
        for (int i = 0; i < 31; i++)
        {
            if (ev[i].type != oldEv[i].type ||
                ev[i].code != oldEv[i].code ||
                ev[i].value != oldEv[i].value)
            {
                changed = 1;
                break;
            }
        }
    }

    if (changed)
    {
        // Update oldEv array for next iteration
        memcpy(oldEv, ev, sizeof(oldEv));

        // Now actually write these events out
        if (write(fd, ev, sizeof(ev)) < 0)
        {
            perror("write");
            return 1;
        }
    }
}

            // Add logic for back/mode/home functionality
            if (PHYSICAL_BTN_BACK == 1) {
                ++backcount;
                backpressed = 1;
            }

            if (backpressed == 1 && backpresscomplete == 0 && homepresscomplete == 0) {
                // Check if back button and any other buttons are pressed, enable MODE key and stop back/home functionality until the back/home button is released. Allows for using back/home button as hotkey with other buttons.
                if (PHYSICAL_BTN_BACK == 1 && (PHYSICAL_BTN_VOLUMEUP == 1 || PHYSICAL_BTN_VOLUMEDOWN == 1 || PHYSICAL_BTN_A == 1 || PHYSICAL_BTN_B == 1 || PHYSICAL_BTN_C == 1 || PHYSICAL_BTN_X == 1 || PHYSICAL_BTN_Y == 1 || PHYSICAL_BTN_Z == 1 || PHYSICAL_BTN_SELECT == 1 || PHYSICAL_BTN_START == 1 || PHYSICAL_BTN_TL == 1 || PHYSICAL_BTN_TL2 == 1 || PHYSICAL_BTN_TR == 1 || PHYSICAL_BTN_TR2 || PHYSICAL_BTN_THUMBL == 1 || PHYSICAL_BTN_THUMBR == 1 || PHYSICAL_HAT_X != 0 || PHYSICAL_HAT_Y != 0 || PHYSICAL_ABS_RZ > 1500 || PHYSICAL_ABS_RZ < -1500 || PHYSICAL_ABS_X > 1500 || PHYSICAL_ABS_X < -1500 || PHYSICAL_ABS_Y > 1500 || PHYSICAL_ABS_Y < -1500 || PHYSICAL_ABS_Z > 1500 || PHYSICAL_ABS_Z < -1500)) {
                    VIRTUAL_BTN_MODE = 1;
                    backpresscomplete = 1;
                    homepresscomplete = 1;
                    if (PHYSICAL_BTN_VOLUMEUP == 1 || PHYSICAL_BTN_VOLUMEDOWN == 1 || PHYSICAL_ABS_RZ > 1500 || PHYSICAL_ABS_RZ < -1500) {
                        isadjustingbrightness = 1;
                        VIRTUAL_BTN_MODE = 0;
                    } else {
                        isadjustingbrightness = 0;
                    }
                }

                // Check if back button pressed and released quickly, send back keyevent
                if (PHYSICAL_BTN_BACK == 0 && backcount < 150 && backpresscomplete == 0) {
                    if (get_retroarch_status() == 0) {
                        send_shell_command("input keyevent 4");
                        fprintf(stderr, "RA Not Active\n");
                    } else {
                        menutoggleactivated = 1;
                        fprintf(stderr, "RA Active: Short Press\n");
                    }
                    backpresscomplete = 1;
                    homepresscomplete = 1;
                }

                // Check if back button held down with no other buttons pressed, send home keyevent
                if (PHYSICAL_BTN_BACK == 1 && backcount > 150 && homepresscomplete == 0) {
                    if (get_retroarch_status() == 0) {
                        send_shell_command("input keyevent 3");
                        fprintf(stderr, "RA Not Active\n");
                    } else {
                        VIRTUAL_BTN_MODE = 1;
                        VIRTUAL_BTN_2 = 1;
                        fprintf(stderr, "RA Active: Long Press\n");
                    }
                    backpresscomplete = 1;
                    homepresscomplete = 1;
                }
            }

            // Add brightness control - analog sticks
            if (VIRTUAL_BTN_MODE == 0 && isadjustingbrightness == 1 && PHYSICAL_ABS_RZ > 1500 && count % 10 == 0) {
                lcd_brightness(0);
            }
            if (VIRTUAL_BTN_MODE == 0 && isadjustingbrightness == 1 && PHYSICAL_ABS_RZ < -1500 && count % 10 == 0) {
                lcd_brightness(1);
            }

            // Add brightness control - volume rocker
            if (VIRTUAL_BTN_MODE == 0 && isadjustingbrightness == 1 && PHYSICAL_BTN_VOLUMEDOWN == 1 && count % 10 == 0) {
                lcd_brightness(0);
            }
            if (VIRTUAL_BTN_MODE == 0 && isadjustingbrightness == 1 && PHYSICAL_BTN_VOLUMEUP == 1 && count % 10 == 0) {
                lcd_brightness(1);
            }

            // Stop brightness control when buttons are released
            if ((ie.code == 158 || ie.code == 316) && ie.value == 0 && PHYSICAL_BTN_VOLUMEUP == 0 && PHYSICAL_BTN_VOLUMEDOWN == 0 && PHYSICAL_ABS_RZ < 1500 && PHYSICAL_ABS_RZ > -1500) {
                isadjustingbrightness = 0;
            }

            if ((adckeysie.code == 158 || ie.code == 316) && adckeysie.value == 0 && PHYSICAL_BTN_VOLUMEUP == 0 && PHYSICAL_BTN_VOLUMEDOWN == 0 && PHYSICAL_ABS_RZ < 1500 && PHYSICAL_ABS_RZ > -1500) {
                isadjustingbrightness = 0;
            }

            // Reset variables when back button no longer pressed
            if ((ie.code == 158 || ie.code == 316) && ie.value == 0) {
                PHYSICAL_BTN_BACK = 0;
                VIRTUAL_BTN_MODE = 0;
                VIRTUAL_BTN_1 = 0;
                VIRTUAL_BTN_2 = 0;
            }

            if ((adckeysie.code == 158 || adckeysie.code == 316) && adckeysie.value == 0) {
                PHYSICAL_BTN_BACK = 0;
                VIRTUAL_BTN_MODE = 0;
                VIRTUAL_BTN_1 = 0;
                VIRTUAL_BTN_2 = 0;
            }

            if (PHYSICAL_BTN_BACK == 0 && VIRTUAL_BTN_MODE == 0) {
                backpressed = 0;
                backcount = 0;
                backpresscomplete = 0;
                homepresscomplete = 0;
            }

            if (menutoggleactivated == 0 && menutogglecompleted == 1 && count % 10 == 0) {
                VIRTUAL_BTN_MODE = 0;
                VIRTUAL_BTN_1 = 0;
                menutogglecompleted = 0;
            }

            if (menutoggleactivated == 1 && count % 10 == 0) {
                VIRTUAL_BTN_MODE = 1;
                VIRTUAL_BTN_1 = 1;
                menutoggleactivated = 0;
                menutogglecompleted = 1;
            }

            // Add logic for switching between performance mode
            if (performance_mode_isupdated_local == 1) {
                set_performance_mode();
                performance_mode_isupdated_local = 0;
                *performance_mode_isupdated = 0;
            }

            if (PHYSICAL_BTN_HOME == 1 && homepressed == 0) {
                send_shell_command("input keyevent 3");
                homepressed = 1;
            }

            if (PHYSICAL_BTN_HOME == 0) {
                homepressed = 0;
            }
        }

        // Mouse mode toggle logic
        if (PHYSICAL_BTN_SELECT && PHYSICAL_BTN_TR) {
            if (!select_and_r1_timer_started) {
                select_and_r1_timer_started = 1;
                select_pressed_time = count;
                r1_pressed_time = count;
            } else if (count - select_pressed_time >= 250 && count - r1_pressed_time >= 250) {
                if (mouse_mode) {
                    disable_mouse_mode(fd);
                       send_shell_command("su -lp 2000 -c \"am start -a android.intent.action.MAIN -e toasttext 'Mouse mode disabled. Hold down Select and R1 to enable.' -n bellavita.toast/.MainActivity\"");
                } else {
                    enable_mouse_mode(fd);
                }
                send_shell_command("settings put secure accessibility_display_inversion_enabled 1 && sleep 0.5 && settings put secure accessibility_display_inversion_enabled 0");
                select_and_r1_timer_started = 0;
            }
        } else {
            select_and_r1_timer_started = 0;
        }

        msleep(3);
        ++count;
    }

    if (ioctl(fd, UI_DEV_DESTROY)) {
        printf("UI_DEV_DESTROY");
        return 1;
    }

    if (ioctl(mouse_fd, UI_DEV_DESTROY)) {
        printf("UI_DEV_DESTROY for mouse");
        return 1;
    }

    close(fd);
    close(mouse_fd);
    return 0;
}
