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

static void bus_error_handler(int sig) {
        // Log the bus error
        fprintf(stderr, "Caught bus error (signal %d). Ignoring it.\n", sig);
}

int main(void) {

        // Set up the signal handler for SIGBUS
        struct sigaction sa;
        sa.sa_handler = bus_error_handler;
        sigemptyset( & sa.sa_mask);
        sa.sa_flags = 0;

        if (sigaction(SIGBUS, & sa, NULL) == -1) {
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
        ioctl(fd, UI_SET_KEYBIT, KEY_HOMEPAGE);

        ioctl(fd, UI_SET_EVBIT, EV_ABS); // enable analog absolute position handling

        setup_abs(fd, ABS_X, 0, 256);
        setup_abs(fd, ABS_Y, 0, 256);

        setup_abs(fd, ABS_Z, 0, 256);
        setup_abs(fd, ABS_RZ, 0, 256);

        setup_abs(fd, ABS_GAS, 0, 1);
        setup_abs(fd, ABS_BRAKE, 0, 1);

        setup_abs(fd, ABS_HAT0X, -1, 1);
        setup_abs(fd, ABS_HAT0Y, -1, 1);

        fprintf(stderr, "Assign virtual controller as Microsoft X-Box One S Controller...\n");
        struct uinput_setup setup = {
                .name = "Xbox Wireless Controller",
                .id = {
                        .bustype = 0x0005,
                        .vendor = 0x045e,
                        .product = 0x02fd,
                        .version = 0x0003,
                }
        };

        fprintf(stderr, "Finalize virtual controller configuration...\n");
        if (ioctl(fd, UI_DEV_SETUP, & setup)) {
                perror("UI_DEV_SETUP");
                return 1;
        }

        // Unbind retrogame_joypad and rebind
        // fprintf(stderr, "Unbinding 'soc:odm:kte-joystick'...\n");
        // send_shell_command("echo 'soc:odm:kte-joystick' > /sys/bus/platform/drivers/kte-gpio-keys/unbind");
        // send_shell_command("echo 'soc:odm:kte-joystick' > /sys/bus/platform/drivers/kte-gpio-keys/unbind");
        // fprintf(stderr, "Unbinding 'mtk-pmic-keys'...\n");
        // send_shell_command("echo 'mtk-pmic-keys' > /sys/bus/platform/drivers/mtk-pmic-keys/unbind");
        // send_shell_command("echo 'mtk-pmic-keys' > /sys/bus/platform/drivers/mtk-pmic-keys/unbind");
        // sleep(3);

        fprintf(stderr, "Create virtual controller uinput device...\n");
        if (ioctl(fd, UI_DEV_CREATE)) {
                perror("UI_DEV_CREATE");
                return 1;
        }

        // fprintf(stderr, "Rebinding 'soc:odm:kte-joystick', force a failure\n");
        // send_shell_command("echo 'soc:odm:kte-joystick' > /sys/bus/platform/drivers/kte-gpio-keys/bind");
        // sleep(1);
        // fprintf(stderr, "Clean up any left over 'soc\\:odm\\:kte-joystick' files\n");
        // send_shell_command("rm -rf /sys/devices/platform/soc/soc:odm/soc:odm:kte-joystick");
        // send_shell_command("rm -rf /sys/devices/platform/soc/soc:odm/soc:odm:kte-joystick");
        // send_shell_command("rm -rf /sys/devices/platform/soc/10026000.pwrap/10026000.pwrap:mt6366/mtk-pmic-keys");
        // send_shell_command("rm -rf /sys/devices/platform/soc/10026000.pwrap/10026000.pwrap:mt6366/mtk-pmic-keys");
        // sleep(1);
        // fprintf(stderr, "Finally bind the physical 'soc:odm:kte-joystick' again\n");
        // send_shell_command("echo 'soc:odm:kte-joystick' > /sys/bus/platform/drivers/kte-gpio-keys/bind");
        // send_shell_command("echo 'mtk-pmic-keys' > /sys/bus/platform/drivers/mtk-pmic-keys/bind");
        // sleep(1);

        // Create /dev/input/event# string by using grep to get physical retrogame_joypad event number
        char openrgp[1000] = "/dev/input/";
        strcat(openrgp, send_shell_command("grep -E 'Name|Handlers|Phys=' /proc/bus/input/devices | grep -A2 'KT Controller' | grep -Eo 'event[0-9]+'"));
        fprintf(stderr, "Physical KT Controller: %s\nReady.\n", openrgp);

        // Open physical_retrogame_joypad, with exclusive access to this application only
        int physical_retrogame_joypad = open(openrgp, O_RDWR | O_NONBLOCK, S_IRUSR | S_IWUSR);
        ioctl(physical_retrogame_joypad, EVIOCGRAB, 1);

        char rgpremove[1000] = "rm ";
        strcat(rgpremove, openrgp);
        send_shell_command(rgpremove);

        //char tjpremove[1000] = "rm /dev/input/";
        //strcat(tjpremove, send_shell_command("grep -E 'Name|Handlers|Phys=' /proc/bus/input/devices | grep -A1 input/touch_joypad | grep -Eo 'event[0-9]+'"));
        //send_shell_command(tjpremove);

        // Define data structure to capture physical inputs
        struct input_event ie;

        // Create /dev/input/event# string by using grep to get physical retrogame_joypad event number
        char opengpio[1000] = "/dev/input/";
        strcat(opengpio, send_shell_command("grep -E 'Name|Handlers|Phys=' /proc/bus/input/devices | grep -A2 'mtk-kpd' | grep -Eo 'event[0-9]+'"));
        fprintf(stderr, "Physical gpio_keys: %s\nReady.\n", opengpio);

        // Open gpio-=keys, no exclusive access 
        int physical_gpio_keys = open(opengpio, O_RDWR | O_NONBLOCK, S_IRUSR | S_IWUSR);
        ioctl(physical_gpio_keys, EVIOCGRAB, 1);

        // Define data structure to capture physical inputs
        struct input_event gpioie;

        // Create /dev/input/event# string by using grep to get physical adc-keys event number
        //char openadckeys[1000] = "/dev/input/";
        //strcat(openadckeys, send_shell_command("grep -E 'Name|Handlers|Phys=' /proc/bus/input/devices | grep -A1 adc-keys/ | grep -Eo 'event[0-9]+'"));
        //fprintf(stderr, "Physical gpio_keys: %s\nReady.\n", openadckeys);

        // Open adc-keys, exclusive access 
        //int physical_adc_keys = open(openadckeys, O_RDWR | O_NONBLOCK, S_IRUSR | S_IWUSR);
        //ioctl(physical_adc_keys, EVIOCGRAB, 1);

        // Define data structure to capture physical inputs
        //struct input_event adckeysie;

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

        // Check for screen on
        int screenison = 1;
        int isadjustingbrightness = 0;

        int menutoggleactivated = 0;
        int menutogglecompleted = 0;

        int isfanturnedoffduringsleep = 0;
        int fanison = 0;
        set_performance_mode();
		
		if (* analog_sensitivity != 0 ) {setAnalogSensitvityTable(* analog_sensitivity);}

        while (1) {

                // Update screen status and MMAP variables
                if (count % 250 == 0) {
                        //screenison = get_screen_status();
                        //if (fan_control_isenabled_local == 1) {
                        //        fanison = get_fan_status();
                        //}
                        updateMapVars();
                }

                // Check if screen is off, and make sure to turn the fan off.
                //if (screenison == 0 && fan_control_isenabled_local == 1 && fanison == 1) {
                //        send_shell_command("/system/bin/setfan_off.sh");
                //}
				
				if (analog_sensitivity_isupdated_local == 1) {* analog_sensitivity_isupdated = 0; analog_sensitivity_isupdated_local = 0; if (* analog_sensitivity != 0) {setAnalogSensitvityTable(* analog_sensitivity);}}

                read(physical_gpio_keys, & gpioie, sizeof(struct input_event));
                // Read physical gpio-keys inputs
                if (gpioie.type == 1) {
                        screenison = 1;

                        if (debug_messages_enabled == 1) {
                                fprintf(stderr, "gpiotime:%ld.%06ld\ttype:%u\tcode:%u\tvalue:%d\n", gpioie.time.tv_sec, gpioie.time.tv_usec, gpioie.type, gpioie.code, gpioie.value);
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
                //read(physical_adc_keys, & adckeysie, sizeof(struct input_event));

                // Read physical adc-keys inputs
                // if (adckeysie.type == 1 && adckeysie.code == 139) {

                        // if (debug_messages_enabled == 1) {
                                // fprintf(stderr, "adctime:%ld.%06ld\ttype:%u\tcode:%u\tvalue:%d\n", adckeysie.time.tv_sec, adckeysie.time.tv_usec, adckeysie.type, adckeysie.code, adckeysie.value);
                        // }

                // }

                struct input_event ev[32];
                memset( & ev, 0, sizeof ev);
                read(physical_retrogame_joypad, & ie, sizeof(struct input_event));

                // Read physical retrogame_joypad inputs and update our virtual gamepad inputs accordingly
                if (ie.type != 0) {
                        if (debug_messages_enabled == 1 && ie.value != 127) {
                                fprintf(stderr, "ietime:%ld.%06ld\ttype:%u\tcode:%u\tvalue:%d\n", ie.time.tv_sec, ie.time.tv_usec, ie.type, ie.code, ie.value);
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
                        if (ie.code == 139) {
                                PHYSICAL_BTN_BACK = ie.value;
                        }

                        // HOME
                        if (ie.code == 172) {
                                PHYSICAL_BTN_HOME = ie.value;
                        }

                        // MODE - HYBRID USAGE WITH BACK BUTTON
                        if (VIRTUAL_BTN_MODE == 1) {
                                PHYSICAL_BTN_BACK = 0;
                        }

                        // DPAD UP/DOWN
                        if (ie.code == 17) {
                                if ( * dpad_analog_swap == 1) {
                                        //PHYSICAL_HAT_Y = 0; 
                                        if (ie.value == 0) {
                                                PHYSICAL_ABS_Y = 128;
                                        }
                                        if (ie.value == 1) {
                                                PHYSICAL_ABS_Y = 254;
                                        }
                                        if (ie.value == -1) {
                                                PHYSICAL_ABS_Y = 0;
                                        }
                                } else {
                                        PHYSICAL_HAT_Y = ie.value;
                                }
                        }

                        // DPAD LEFT/RIGHT
                        if (ie.code == 16) {
                                if ( * dpad_analog_swap == 1) {
                                        //PHYSICAL_HAT_X = 0; 
                                        if (ie.value == 0) {
                                                PHYSICAL_ABS_X = 128;
                                        }
                                        if (ie.value == 1) {
                                                PHYSICAL_ABS_X = 254;
                                        }
                                        if (ie.value == -1) {
                                                PHYSICAL_ABS_X = 0;
                                        }
                                } else {
                                        PHYSICAL_HAT_X = ie.value;
                                }
                        }

                        // LEFT ANALOG Y
                        if (ie.code == 1) {
                                if ( * dpad_analog_swap == 1) {
                                        //PHYSICAL_ABS_Y = 0; 
                                        if (ie.value < 178 && ie.value > 78) {
                                                PHYSICAL_HAT_Y = 0;
                                        }
                                        if (ie.value >= 178) {
                                                PHYSICAL_HAT_Y = 1;
                                        }
                                        if (ie.value <= 78) {
                                                PHYSICAL_HAT_Y = -1;
                                        }
                                } else {
                                        if (* analog_sensitivity != 0) {PHYSICAL_ABS_Y = lookupValue(ie.value);} else {PHYSICAL_ABS_Y = ie.value;}
                                }
                        }

                        // LEFT ANALOG X
                        if (ie.code == 0) {
                                if ( * dpad_analog_swap == 1) {
                                        //PHYSICAL_ABS_X = 0; 
                                        if (ie.value < 178 && ie.value > 78) {
                                                PHYSICAL_HAT_X = 0;
                                        }
                                        if (ie.value >= 178) {
                                                PHYSICAL_HAT_X = 1;
                                        }
                                        if (ie.value <= 78) {
                                                PHYSICAL_HAT_X = -1;
                                        }
                                } else {
                                        if (* analog_sensitivity != 0) {PHYSICAL_ABS_X = lookupValue(ie.value);} else {PHYSICAL_ABS_X = ie.value;}
                                }
                        }

                        // RIGHT ANALOG Y
                        if (ie.code == 2) {
                                if (* analog_sensitivity != 0) {PHYSICAL_ABS_Z = lookupValue(ie.value);} else {PHYSICAL_ABS_Z = ie.value;}
                        }

                        // RIGHT ANALOG X
                        if (ie.code == 5) {
                                if (* analog_sensitivity != 0) {PHYSICAL_ABS_RZ = lookupValue(ie.value);} else {PHYSICAL_ABS_RZ = ie.value;}
                        }
                }

                ev[0].type = EV_KEY;
                if ( * abxy_layout == 0) {
                        ev[0].code = BTN_A;
                } else {
                        ev[0].code = BTN_B;
                }
                ev[0].value = PHYSICAL_BTN_A;

                ev[1].type = EV_KEY;
                if ( * abxy_layout == 0) {
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
                if ( * analog_axis == 0) {
                        ev[4].value = PHYSICAL_ABS_Y;
                } else {
                        if (PHYSICAL_ABS_Y >= 128) {ev[4].value = 128 - (PHYSICAL_ABS_Y - 128);}
                        if (PHYSICAL_ABS_Y <= 128) {ev[4].value = 128 + (128 - PHYSICAL_ABS_Y);}
                }

                ev[5].type = EV_ABS;
                ev[5].code = ABS_X;
                if ( * analog_axis == 0) {
                        ev[5].value = PHYSICAL_ABS_X;
                } else {
                        if (PHYSICAL_ABS_X >= 128) {ev[5].value = 128 - (PHYSICAL_ABS_X - 128);}
                        if (PHYSICAL_ABS_X <= 128) {ev[5].value = 128 + (128 - PHYSICAL_ABS_X);}
                }

                ev[6].type = EV_ABS;
                ev[6].code = ABS_GAS;
                ev[6].value = PHYSICAL_BTN_TR2;

                ev[7].type = EV_ABS;
                ev[7].code = ABS_BRAKE;
                ev[7].value = PHYSICAL_BTN_TL2;

                ev[8].type = EV_KEY;
                ev[8].code = KEY_HOMEPAGE;
                ev[8].value = PHYSICAL_BTN_HOME;

                ev[9].type = EV_KEY;
                ev[9].code = BTN_TL;
                ev[9].value = PHYSICAL_BTN_TL;

                ev[10].type = EV_ABS;
                ev[10].code = ABS_Z;
                if ( * rightanalog_axis == 0) {
                        ev[10].value = PHYSICAL_ABS_Z;
                } else {
                        if (PHYSICAL_ABS_Z >= 128) {ev[10].value = 128 - (PHYSICAL_ABS_Z - 128);}
                        if (PHYSICAL_ABS_Z <= 128) {ev[10].value = 128 + (128 - PHYSICAL_ABS_Z);}
                }

                ev[11].type = EV_KEY;
                ev[11].code = BTN_TR;
                ev[11].value = PHYSICAL_BTN_TR;

                ev[12].type = EV_KEY;
                ev[12].code = BTN_MODE;
                ev[12].value = VIRTUAL_BTN_MODE;

                ev[13].type = EV_KEY;
                if ( * abxy_layout == 0) {
                        ev[13].code = BTN_X;
                } else {
                        ev[13].code = BTN_Y;
                }
                ev[13].value = PHYSICAL_BTN_X;

                ev[14].type = EV_KEY;
                if ( * abxy_layout == 0) {
                        ev[14].code = BTN_Y;
                } else {
                        ev[14].code = BTN_X;
                }
                ev[14].value = PHYSICAL_BTN_Y;

                ev[15].type = EV_KEY;
                ev[15].code = BTN_THUMBL;
                ev[15].value = PHYSICAL_BTN_THUMBL;

                ev[16].type = EV_KEY;
                ev[16].code = BTN_THUMBR;
                ev[16].value = PHYSICAL_BTN_THUMBR;

                ev[17].type = EV_KEY;
                ev[17].code = BTN_SELECT;
                ev[17].value = PHYSICAL_BTN_SELECT;

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
                if ( * rightanalog_axis == 0) {
                        ev[23].value = PHYSICAL_ABS_RZ;
                } else {
                        if (PHYSICAL_ABS_RZ >= 128) {ev[23].value = 128 - (PHYSICAL_ABS_RZ - 128);}
                        if (PHYSICAL_ABS_RZ <= 128) {ev[23].value = 128 + (128 - PHYSICAL_ABS_RZ);}
                }

                ev[24].type = EV_KEY;
                ev[24].code = BTN_C;
                ev[24].value = PHYSICAL_BTN_C;

                ev[25].type = EV_KEY;
                ev[25].code = BTN_Z;
                ev[25].value = PHYSICAL_BTN_Z;

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

                // sync event tells input layer we're done with a "batch" of
                // updates

                ev[31].type = EV_SYN;
                ev[31].code = SYN_REPORT;
                ev[31].value = 0;

 //               if (screenison == 1 || (screenison == 0 && PHYSICAL_BTN_POWER == 1)) {

                        // // Fan stuff
                        // if (fan_control_isupdated_local == 1 && fan_control_isenabled_local == 1) {
                                // fanControl();
                                // fan_control_isupdated_local = 0;
                                // * fan_control_isupdated = 0;
                        // }

                        // if (count % 2500 == 0 && fan_control_isenabled_local == 1) {
                                // if (screenison == 1) {
                                        // fanControl();
                                // }
                                // if ( * fan_control == 1) {
                                        // int currentTemp = get_cpu_temp();
                                        // if (currentTemp < 60) {
                                                // send_shell_command("/system/bin/setfan_off.sh");
                                        // }
                                        // if (currentTemp >= 60 && currentTemp < 75) {
                                                // send_shell_command("/system/bin/setfan_cool.sh");
                                        // }
                                        // if (currentTemp >= 75) {
                                                // send_shell_command("/system/bin/setfan_max.sh");
                                        // }
                                // }
                        // }

                        if (write(fd, & ev, sizeof ev) < 0) {
                                perror("write");
                                return 1;
                        }

                        //Support for 353 series back button
                        // if (adckeysie.type == 1 && adckeysie.code == 139) {
                                // PHYSICAL_BTN_BACK = adckeysie.value;
                        // }

                        // Add logic for back/mode/home functionality
                        if (PHYSICAL_BTN_BACK == 1) {
                                ++backcount;
                                backpressed = 1;
                        }

                        if (backpressed == 1 && backpresscomplete == 0 && homepresscomplete == 0) {

                                // Check if back button and any other buttons are pressed, enable MODE key and stop back/home functionality until the back/home button is released. Allows for using back/home button as hotkey with other buttons.
                                if (PHYSICAL_BTN_BACK == 1 && (PHYSICAL_BTN_VOLUMEUP == 1 || PHYSICAL_BTN_VOLUMEDOWN == 1 || PHYSICAL_BTN_A == 1 || PHYSICAL_BTN_B == 1 || PHYSICAL_BTN_C == 1 || PHYSICAL_BTN_X == 1 || PHYSICAL_BTN_Y == 1 || PHYSICAL_BTN_Z == 1 || PHYSICAL_BTN_SELECT == 1 || PHYSICAL_BTN_START == 1 || PHYSICAL_BTN_TL == 1 || PHYSICAL_BTN_TL2 == 1 || PHYSICAL_BTN_TR == 1 || PHYSICAL_BTN_TR2 || PHYSICAL_BTN_THUMBL == 1 || PHYSICAL_BTN_THUMBR == 1 || PHYSICAL_HAT_X != 0 || PHYSICAL_HAT_Y != 0 || PHYSICAL_ABS_RZ > 130 || PHYSICAL_ABS_RZ < 90 || PHYSICAL_ABS_X > 130 || PHYSICAL_ABS_X < 90 || PHYSICAL_ABS_Y > 130 || PHYSICAL_ABS_Y < 90 || PHYSICAL_ABS_Z > 130 || PHYSICAL_ABS_Z < 90)) {
                                        VIRTUAL_BTN_MODE = 1;
                                        backpresscomplete = 1;
                                        homepresscomplete = 1;
                                        if (PHYSICAL_BTN_VOLUMEUP == 1 || PHYSICAL_BTN_VOLUMEDOWN == 1 || PHYSICAL_ABS_RZ > 130 || PHYSICAL_ABS_RZ < 90) {
                                                isadjustingbrightness = 1;
                                                VIRTUAL_BTN_MODE = 0;
                                        } else {
                                                isadjustingbrightness = 0;
                                        }
                                }

                                // Check if back button pressed and released quickly, send back keyevent
                                if (PHYSICAL_BTN_BACK == 0 && backcount < 300 && backpresscomplete == 0) {
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
                                if (PHYSICAL_BTN_BACK == 1 && backcount > 300 && homepresscomplete == 0) {
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
                        if (VIRTUAL_BTN_MODE == 0 && isadjustingbrightness == 1 && PHYSICAL_ABS_RZ > 130 && count % 10 == 0) {
                                lcd_brightness(0);
                        }
                        if (VIRTUAL_BTN_MODE == 0 && isadjustingbrightness == 1 && PHYSICAL_ABS_RZ < 90 && count % 10 == 0) {
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
                        if (ie.code == 139 && ie.value == 0 && PHYSICAL_BTN_VOLUMEUP == 0 && PHYSICAL_BTN_VOLUMEDOWN == 0 && PHYSICAL_ABS_RZ > 90 && PHYSICAL_ABS_RZ < 130) {
                                isadjustingbrightness = 0;
                        }
						
                        // if (adckeysie.code == 139 && adckeysie.value == 0 && PHYSICAL_BTN_VOLUMEUP == 0 && PHYSICAL_BTN_VOLUMEDOWN == 0 && PHYSICAL_ABS_RZ > 90 && PHYSICAL_ABS_RZ < 130) {
                                // isadjustingbrightness = 0;
                        // }
						

                        // Reset variables when back button no longer pressed
                        if (ie.code == 139 && ie.value == 0) {
                                PHYSICAL_BTN_BACK = 0;
                                VIRTUAL_BTN_MODE = 0;
                                VIRTUAL_BTN_1 = 0;
                                VIRTUAL_BTN_2 = 0;
                        }

                        // if (adckeysie.code == 139 && adckeysie.value == 0) {
                                // PHYSICAL_BTN_BACK = 0;
                                // VIRTUAL_BTN_MODE = 0;
                                // VIRTUAL_BTN_1 = 0;
                                // VIRTUAL_BTN_2 = 0;
                        // }

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
                                * performance_mode_isupdated = 0;
                        }

                         if (PHYSICAL_BTN_HOME == 1 && homepressed == 0) {
                                // send_shell_command("input keyevent 3");
                                 homepressed = 1;
                         }

                         if (PHYSICAL_BTN_HOME == 0) {
                                 homepressed = 0;
                         }

 //               }
                msleep(3);

                ++count;

        }

        if (ioctl(fd, UI_DEV_DESTROY)) {
                printf("UI_DEV_DESTROY");
                return 1;
        }

        close(fd);
        return 0;
}

// Enable and configure an absolute "position" analog channel

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

        if (ioctl(fd, UI_ABS_SETUP, & s))
                perror("UI_ABS_SETUP");
}

// Send shell commands and return output as string
static char * send_shell_command(char * shellcmd) {
        FILE * shell_cmd_pipe;
        char cmd_output[5000];
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

// Get current LCD brightness and set value up or down by 2
static int lcd_brightness(int value) {

        int current_brightness = atoi(send_shell_command("settings get system screen_brightness"));
        if (debug_messages_enabled == 1) {
                fprintf(stderr, "Current brightness: %i\n", current_brightness);
        }

        if (value == 1 && current_brightness < 256) {
                current_brightness = current_brightness + 10;
                if (current_brightness > 256) {
                        current_brightness = 256;
                }
        }
        if (value == 0 && current_brightness > 1) {
                current_brightness = current_brightness - 10;
                if (current_brightness < 3) {
                        current_brightness = 3;
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

// Get retroarch status
static int get_retroarch_status() {

        // Check the current value via the shell command, return 0 if not running
        int current_retroarch_status = atoi(send_shell_command("dumpsys activity activities | grep VisibleActivityProcess | grep retroarch &> /dev/null; if [ $? -eq 0 ]; then echo 1; else echo 0; fi"));
        if (debug_messages_enabled == 1) {
                fprintf(stderr, "Retroarch running status: %i\n", current_retroarch_status);
        }

        return current_retroarch_status;
}

// Set current performance mode status
static void set_performance_mode() {

        switch ( * performance_mode) {
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

// Get current screen status
static int get_screen_status() {

        int screenstatus = atoi(send_shell_command("cat /sys/devices/platform/soc/soc\\:mtk_leds/leds/lcd-backlight/brightness"));

        if (screenstatus != 0) {
                screenstatus = 1;
        } else {
                screenstatus = 0;
        }

        return screenstatus;
}

// Get current fan status
static int get_fan_status() {

        int fanstatus = atoi(send_shell_command("cat /sys/devices/platform/singleadc-joypad/fan_power"));

        if (fanstatus != 0) {
                fanstatus = 1;
        } else {
                fanstatus = 0;
        }

        return fanstatus;
}

// Create or open memory maps containing different toggles to be used by both rgp2xbox and SystemUI, allow access from any process without root required. 
// Example to update the value via shell: value=1; printf "%b" "$(printf '\\x%02x\\x%02x\\x%02x\\x%02x' $((value & 0xFF)) $((value >> 8 & 0xFF)) $((value >> 16 & 0xFF)) $((value >> 24 & 0xFF)))" > /data/rgp2xbox/ABXY_LAYOUT
static int openAndMap(const char * filePath, int ** shared_data, int * fd) {
        * fd = open(filePath, O_RDWR | O_CREAT, 0666);
        if ( * fd == -1) {
                fprintf(stderr, "Error opening file: %s\n", filePath);
                return -1;
        }

        if (chmod(filePath, 0666) == -1) {
                fprintf(stderr, "Error setting file permissions: %s\n", filePath);
                close( * fd);
                return -1;
        }

        ftruncate( * fd, BUFFER_SIZE);

        * shared_data = mmap(NULL, BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, * fd, 0);
        if ( * shared_data == MAP_FAILED) {
                fprintf(stderr, "Error mapping file: %s\n", filePath);
                close( * fd);
                return -1;
        }

        return 0;
}

static void setupMaps() {
        struct stat st = {
                0
        };
        if (stat(DIRECTORY_PATH, & st) == -1) {
                if (mkdir(DIRECTORY_PATH, 0777) == -1) {
                        fprintf(stderr, "Error creating directory: %s\n", DIRECTORY_PATH);
                        return;
                }
                chmod(DIRECTORY_PATH, 0777);
        }

        char filePath[256];

        openAndMap(strcat(strcpy(filePath, DIRECTORY_PATH), "ABXY_LAYOUT"), & abxy_layout, & fd_abxy_layout);
        openAndMap(strcat(strcpy(filePath, DIRECTORY_PATH), "ABXY_LAYOUT_ISUPDATED"), & abxy_layout_isupdated, & fd_abxy_layout_isupdated);

        openAndMap(strcat(strcpy(filePath, DIRECTORY_PATH), "PERFORMANCE_MODE"), & performance_mode, & fd_performance_mode);
        openAndMap(strcat(strcpy(filePath, DIRECTORY_PATH), "PERFORMANCE_MODE_ISUPDATED"), & performance_mode_isupdated, & fd_performance_mode_isupdated);

        openAndMap(strcat(strcpy(filePath, DIRECTORY_PATH), "ANALOG_SENSITIVITY"), & analog_sensitivity, & fd_analog_sensitivity);
        openAndMap(strcat(strcpy(filePath, DIRECTORY_PATH), "ANALOG_SENSITIVITY_ISUPDATED"), & analog_sensitivity_isupdated, & fd_analog_sensitivity_isupdated);

        openAndMap(strcat(strcpy(filePath, DIRECTORY_PATH), "ANALOG_AXIS"), & analog_axis, & fd_analog_axis);
        openAndMap(strcat(strcpy(filePath, DIRECTORY_PATH), "ANALOG_AXIS_ISUPDATED"), & analog_axis_isupdated, & fd_analog_axis_isupdated);

        openAndMap(strcat(strcpy(filePath, DIRECTORY_PATH), "RIGHTANALOG_AXIS"), & rightanalog_axis, & fd_rightanalog_axis);
        openAndMap(strcat(strcpy(filePath, DIRECTORY_PATH), "RIGHTANALOG_AXIS_ISUPDATED"), & rightanalog_axis_isupdated, & fd_rightanalog_axis_isupdated);

        openAndMap(strcat(strcpy(filePath, DIRECTORY_PATH), "DPAD_ANALOG_SWAP"), & dpad_analog_swap, & fd_dpad_analog_swap);
        openAndMap(strcat(strcpy(filePath, DIRECTORY_PATH), "DPAD_ANALOG_SWAP_ISUPDATED"), & dpad_analog_swap_isupdated, & fd_dpad_analog_swap_isupdated);

        openAndMap(strcat(strcpy(filePath, DIRECTORY_PATH), "FAN_CONTROL"), & fan_control, & fd_fan_control);
        openAndMap(strcat(strcpy(filePath, DIRECTORY_PATH), "FAN_CONTROL_ISUPDATED"), & fan_control_isupdated, & fd_fan_control_isupdated);
        openAndMap(strcat(strcpy(filePath, DIRECTORY_PATH), "FAN_CONTROL_ISENABLED"), & fan_control_isenabled, & fd_fan_control_isenabled);
}

static void updateMapVars() {
        abxy_layout_isupdated_local = * abxy_layout_isupdated;
        performance_mode_isupdated_local = * performance_mode_isupdated;
        analog_sensitivity_isupdated_local = * analog_sensitivity_isupdated;
        analog_axis_isupdated_local = * analog_axis_isupdated;
        rightanalog_axis_isupdated_local = * rightanalog_axis_isupdated;
        dpad_analog_swap_isupdated_local = * dpad_analog_swap_isupdated;
        fan_control_isupdated_local = * fan_control_isupdated;
        fan_control_isenabled_local = * fan_control_isenabled;
}

// Updates the fan behaviour depending on the set parameter
static void fanControl() {
        switch ( * fan_control) {
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

// Get CPU Package Temperature
static int get_cpu_temp() {
        // Check the current value
        int current_cpu_temp = atoi(send_shell_command("cat /sys/class/thermal/thermal_zone*/temp 2>/dev/null | awk '{sum += $1; n++} END {if (n > 0) print int((sum / n + 99) / 1000)}'"));

        return current_cpu_temp;
}

static void createAnalogSensitvityCSV() {


//------------ -15% sensitivity
    FILE *file;
    int exists = 0;
	char * filepath = "/data/rgp2xbox/DecreaseAnalogSensitivityBy15Percent.csv";

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
        for (int i = 128; i <= 256; i++) {
            double secondValue;
			double secondValue2;
			int negativeValue;
            if (i == 128) {
                // Keep 128 as is
                secondValue = 128;
            } else if (i > 128) {
                // For positive numbers
                secondValue = floor(i * 0.925); if (secondValue < 128) {secondValue = 128;}
				
				negativeValue = 256 - i;
				secondValue2 = 256 - floor(i * 0.925); if (secondValue2 > 128) {secondValue2 = 128;}
				fprintf(file, "%d,%.0f\n", negativeValue, secondValue2);
            }
            fprintf(file, "%d,%.0f\n", i, secondValue);
        }

        fclose(file);
    }
	
//------------ -25% sensitivity	
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
        for (int i = 128; i <= 256; i++) {
            double secondValue;
			double secondValue2;
			int negativeValue;
            if (i == 128) {
                // Keep 128 as is
                secondValue = 128;
            } else if (i > 128) {
                // For positive numbers
                secondValue = floor(i * 0.875); if (secondValue < 128) {secondValue = 128;}
				
				negativeValue = 256 - i;
				secondValue2 = 256 - floor(i * 0.875); if (secondValue2 > 128) {secondValue2 = 128;}
				fprintf(file, "%d,%.0f\n", negativeValue, secondValue2);
            }
            fprintf(file, "%d,%.0f\n", i, secondValue);
        }

        fclose(file);
    }	

//------------ -50% sensitivity
	
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
        for (int i = 128; i <= 256; i++) {
            double secondValue;
			double secondValue2;
			int negativeValue;
            if (i == 128) {
                // Keep 128 as is
                secondValue = 128;
            } else if (i > 128) {
                // For positive numbers
                secondValue = floor(i * 0.75); if (secondValue < 128) {secondValue = 128;}
				
				negativeValue = 256 - i;
				secondValue2 = 256 - floor(i * 0.75); if (secondValue2 > 128) {secondValue2 = 128;}
				fprintf(file, "%d,%.0f\n", negativeValue, secondValue2);
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
        for (int i = 0; i <= 256; i++) {
            fprintf(file, "%d,%d\n", i, i);
        }

        fclose(file);
    }	
	
}


// Parse CSV, create lookup table

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
				* analog_sensitivity = 0;
				* analog_axis_isupdated = 1;
                break;
        }
}
