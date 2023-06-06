#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/uinput.h>
#define msleep(ms) usleep((ms) * 1000)

const int debug_messages_enabled = 0;

static void setup_abs(int fd, unsigned chan, int min, int max);
static char * send_shell_command(char * shellcmd);
static int lcd_brightness(int value);

///////////////////

int main(void) {

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
        ioctl(fd, UI_SET_KEYBIT, BTN_X);
        ioctl(fd, UI_SET_KEYBIT, BTN_Y);
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

        ioctl(fd, UI_SET_EVBIT, EV_ABS); // enable analog absolute position handling

        setup_abs(fd, ABS_X, -1800, 1800);
        setup_abs(fd, ABS_Y, -1800, 1800);

        setup_abs(fd, ABS_Z, -1800, 1800);
        setup_abs(fd, ABS_RZ, -1800, 1800);

        setup_abs(fd, ABS_GAS, 0, 1);
        setup_abs(fd, ABS_BRAKE, 0, 1);

        setup_abs(fd, ABS_HAT0X, -1, 1);
        setup_abs(fd, ABS_HAT0Y, -1, 1);

        fprintf(stderr, "Assign virtual controller as Xbox360 Controller...\n");
        struct uinput_setup setup = {
                .name = "Xbox360 Controller",
                .id = {
                        .bustype = 0x0019,
                        .vendor = 0x145E,
                        .product = 0x128E,
                        .version = 0x1114,
                }
        };

        fprintf(stderr, "Finalize virtual controller configuration...\n");
        if (ioctl(fd, UI_DEV_SETUP, & setup)) {
                perror("UI_DEV_SETUP");
                return 1;
        }

        fprintf(stderr, "Create virtual controller uinput device...\n");
        if (ioctl(fd, UI_DEV_CREATE)) {
                perror("UI_DEV_CREATE");
                return 1;
        }

        // Unbind retrogame_joypad and rebind
        fprintf(stderr, "Unbinding retrogame_joypad...\n");
        send_shell_command("echo singleadc-joypad > /sys/bus/platform/drivers/singleadc-joypad/unbind");
        send_shell_command("echo singleadc-joypad > /sys/bus/platform/drivers/singleadc-joypad/unbind");
        sleep(1);
        fprintf(stderr, "Rebinding retrogame_joypad, force a failure\n");
        send_shell_command("echo singleadc-joypad > /sys/bus/platform/drivers/singleadc-joypad/bind");
        sleep(1);
        fprintf(stderr, "Clean up any left over retrogame_joypad files\n");
        send_shell_command("rm -rf /sys/devices/platform/singleadc-joypad");
        send_shell_command("rm -rf /sys/devices/platform/singleadc-joypad");
        sleep(1);
        fprintf(stderr, "Finally bind the physical retrogame_joypad again\n");
        send_shell_command("echo singleadc-joypad > /sys/bus/platform/drivers/singleadc-joypad/bind");
        sleep(1);

        // Create /dev/input/event# string by using grep to get physical retrogame_joypad event number
        char openrgp[1000] = "/dev/input/";
        strcat(openrgp, send_shell_command("grep -E 'Name|Handlers|Phys=' /proc/bus/input/devices | grep -A1 retrogame_joypad/ | grep -Eo 'event[0-9]+'"));
        fprintf(stderr, "Physical retrogame_joypad: %s\nReady.\n", openrgp);

        // Open physical_retrogame_joypad, with exclusive access to this application only
        int physical_retrogame_joypad = open(openrgp, O_RDWR | O_NONBLOCK, S_IRUSR | S_IWUSR);
        ioctl(physical_retrogame_joypad, EVIOCGRAB, 1);

        // Define data structure to capture physical inputs
        struct input_event ie;

        // you can write events one at a time, but to save overhead we'll
        // update all of them in a single write

        unsigned count = 0;

        int PHYSICAL_BTN_A = 0;
        int PHYSICAL_BTN_B = 0;
        int PHYSICAL_BTN_X = 0;
        int PHYSICAL_BTN_Y = 0;
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
        int VIRTUAL_BTN_MODE = 0;

        int PHYSICAL_HAT_X = 0;
        int PHYSICAL_HAT_Y = 0;

        int PHYSICAL_ABS_Y = 0;
        int PHYSICAL_ABS_X = 0;

        int PHYSICAL_ABS_Z = 0;
        int PHYSICAL_ABS_RZ = 0;

        int backpressed = 0;
        int backcount = 0;
        int backpresscomplete = 0;
        int homepresscomplete = 0;

        int xboxtogglecount = 0;
        int xboxtogglepresscomplete = 0;
        int xboxtoggle = 0;

        while (1) {

                struct input_event ev[25];
                memset( & ev, 0, sizeof ev);

                read(physical_retrogame_joypad, & ie, sizeof(struct input_event));

                // Read physical retrogame_joypad inputs and update our virtual gamepad inputs accordingly
                if (ie.type != 0) {
                        if (debug_messages_enabled == 1) {
                                fprintf(stderr, "time:%ld.%06ld\ttype:%u\tcode:%u\tvalue:%d\n", ie.time.tv_sec, ie.time.tv_usec, ie.type, ie.code, ie.value);
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

                        // X
                        if (ie.code == 307) {
                                PHYSICAL_BTN_X = ie.value;
                        }

                        // Y
                        if (ie.code == 308) {
                                PHYSICAL_BTN_Y = ie.value;
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

                        // MODE - HYBRID USAGE WITH BACK BUTTON
                        if (VIRTUAL_BTN_MODE == 1) {
                                PHYSICAL_BTN_BACK = 0;
                        }

                        // DPAD UP/DOWN
                        if (ie.code == 17) {
                                PHYSICAL_HAT_Y = ie.value;
                        }

                        // DPAD LEFT/RIGHT
                        if (ie.code == 16) {
                                PHYSICAL_HAT_X = ie.value;
                        }

                        // LEFT ANALOG Y
                        if (ie.code == 1) {
                                PHYSICAL_ABS_Y = ie.value;
                        }

                        // LEFT ANALOG X
                        if (ie.code == 0) {
                                PHYSICAL_ABS_X = ie.value;
                        }

                        // RIGHT ANALOG Y
                        if (ie.code == 2) {
                                PHYSICAL_ABS_Z = ie.value;
                        }

                        // RIGHT ANALOG X
                        if (ie.code == 5) {
                                PHYSICAL_ABS_RZ = ie.value;
                        }
                }

                ev[0].type = EV_KEY;
                ev[0].code = BTN_A;
                ev[0].value = PHYSICAL_BTN_A;

                ev[1].type = EV_KEY;
                ev[1].code = BTN_B;
                ev[1].value = PHYSICAL_BTN_B;

                ev[2].type = EV_KEY;
                ev[2].code = BTN_TL2;
                ev[2].value = PHYSICAL_BTN_TL2;

                ev[3].type = EV_KEY;
                ev[3].code = BTN_TR2;
                ev[3].value = PHYSICAL_BTN_TR2;

                ev[4].type = EV_ABS;
                ev[4].code = ABS_Y;
                ev[4].value = PHYSICAL_ABS_Y;

                ev[5].type = EV_ABS;
                ev[5].code = ABS_X;
                ev[5].value = PHYSICAL_ABS_X;

                ev[6].type = EV_ABS;
                ev[6].code = ABS_GAS;
                ev[6].value = PHYSICAL_BTN_TR2;

                ev[7].type = EV_ABS;
                ev[7].code = ABS_BRAKE;
                ev[7].value = PHYSICAL_BTN_TL2;

                //      ev[8].type = EV_ABS;
                //      ev[8].code = ABS_THROTTLE;
                //      ev[8].value = PHYSICAL_BTN_TR2;

                ev[9].type = EV_KEY;
                ev[9].code = BTN_TL;
                ev[9].value = PHYSICAL_BTN_TL;

                ev[10].type = EV_ABS;
                ev[10].code = ABS_Z;
                ev[10].value = PHYSICAL_ABS_Z;

                ev[11].type = EV_KEY;
                ev[11].code = BTN_TR;
                ev[11].value = PHYSICAL_BTN_TR;

                ev[12].type = EV_KEY;
                ev[12].code = BTN_MODE;
                ev[12].value = VIRTUAL_BTN_MODE;

                ev[13].type = EV_KEY;
                ev[13].code = BTN_X;
                ev[13].value = PHYSICAL_BTN_X;

                ev[14].type = EV_KEY;
                ev[14].code = BTN_Y;
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
                ev[23].value = PHYSICAL_ABS_RZ;

                // sync event tells input layer we're done with a "batch" of
                // updates

                ev[24].type = EV_SYN;
                ev[24].code = SYN_REPORT;
                ev[24].value = 0;

                if (write(fd, & ev, sizeof ev) < 0) {
                        perror("write");
                        return 1;
                }

                // Add logic for back/mode/home functionality
                if (PHYSICAL_BTN_BACK == 1) {
                        ++backcount;
                        backpressed = 1;
                }

                if (backpressed == 1 && backpresscomplete == 0 && homepresscomplete == 0) {
						// Check if back button pressed and released quickly, send back keyevent
                        if (PHYSICAL_BTN_BACK == 0 && backcount < 120 && backpresscomplete == 0) {
                                send_shell_command("input keyevent 4");
                                backpresscomplete = 1;
                                homepresscomplete = 1;
                        }
						// Check if back button and any other buttons are pressed, enable MODE key and stop back/home functionality until the back/home button is released. Allows for using back/home button as hotkey with other buttons.
                        if (PHYSICAL_BTN_BACK == 1 && (PHYSICAL_BTN_A == 1 || PHYSICAL_BTN_B == 1 || PHYSICAL_BTN_X == 1 || PHYSICAL_BTN_Y == 1 || PHYSICAL_BTN_SELECT == 1 || PHYSICAL_BTN_START == 1 || PHYSICAL_BTN_TL == 1 || PHYSICAL_BTN_TL2 == 1 || PHYSICAL_BTN_TR == 1 || PHYSICAL_BTN_TR2 || PHYSICAL_BTN_THUMBL == 1 || PHYSICAL_BTN_THUMBR == 1 || PHYSICAL_HAT_X != 0 || PHYSICAL_HAT_Y != 0 || PHYSICAL_ABS_RZ > 1500 || PHYSICAL_ABS_RZ < -1500 || PHYSICAL_ABS_X > 1500 || PHYSICAL_ABS_X < -1500 || PHYSICAL_ABS_Y > 1500 || PHYSICAL_ABS_Y < -1500 || PHYSICAL_ABS_Z > 1500 || PHYSICAL_ABS_Z < -1500)) {
                                VIRTUAL_BTN_MODE = 1;
                                backpresscomplete = 1;
                                homepresscomplete = 1;
                        }
						// Check if back button held down with no other buttons pressed, send home keyevent
                        if (PHYSICAL_BTN_BACK == 1 && backcount > 120 && homepresscomplete == 0) {
                                send_shell_command("input keyevent 3");
                                backpresscomplete = 1;
                                homepresscomplete = 1;
                        }
                }


                // Add brightness control
                if (VIRTUAL_BTN_MODE == 1 && PHYSICAL_ABS_RZ > 1500 && count % 10 == 0) {
                        lcd_brightness(0);
                }
                if (VIRTUAL_BTN_MODE == 1 && PHYSICAL_ABS_RZ < -1500 && count % 10 == 0) {
                        lcd_brightness(1);
                }


                // Reset variables when back button no longer pressed
                if (ie.code == 158 && ie.value == 0) {
                        PHYSICAL_BTN_BACK = 0;
                        VIRTUAL_BTN_MODE = 0;
                }

                if (PHYSICAL_BTN_BACK == 0 && VIRTUAL_BTN_MODE == 0) {
                        backpressed = 0;
                        backcount = 0;
                        backpresscomplete = 0;
                        homepresscomplete = 0;
                }
				
				
				

                // Add logic for switching between xbox controller layout
                if (PHYSICAL_BTN_THUMBL == 1 && PHYSICAL_BTN_TL == 1 && PHYSICAL_BTN_TR == 1 && xboxtogglepresscomplete == 0) {
                        ++xboxtogglecount;

                        if (xboxtogglecount > 200) {
                                if (xboxtoggle == 0) {

                                        send_shell_command("echo 1 > /sys/devices/platform/singleadc-joypad/remapkey_xbox_switch");
                                        send_shell_command("su -lp 2000 -c \"cmd notification post -S bigtext -t 'Remapping' 'Remapping' 'Xbox Mapping Activated' \"");

                                        xboxtogglepresscomplete = 1;
                                        xboxtoggle = 1;
                                } else {

                                        send_shell_command("echo 0 > /sys/devices/platform/singleadc-joypad/remapkey_xbox_switch");
                                        send_shell_command("su -lp 2000 -c \"cmd notification post -S bigtext -t 'Remapping' 'Remapping' 'Xbox Mapping Deactivated' \"");

                                        xboxtogglepresscomplete = 1;
                                        xboxtoggle = 0;
                                }
                        }
                }

                if (PHYSICAL_BTN_THUMBL == 0 && PHYSICAL_BTN_TL == 0 && PHYSICAL_BTN_TR == 0) {
                        xboxtogglecount = 0;
                        xboxtogglepresscomplete = 0;
                }

                msleep(5);

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

// Get current LCD brightness and set value up or down by 10
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