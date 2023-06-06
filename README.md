# retrogame_joypad_2_xbox
A uinput solution to fix issues with some button mappings on Anbernic Android devices. Works on both Stock RG405M/RG505 and with GammaOS. This is not a permanent change, needs to be run again after reboot or after the application is closed. This will be included in future GammaOS releases as standard so no need to run separately. For StockOS, extract the rgp2xbox.zip file from the releases to the root of your internal storage. Execute in Termux using the following commands:
- su
- sh /sdcard/rgp2xbox/rgp2xbox.sh

Features:
- Presents the physical gamepad as an xbox controller
- Fixes L2/R2 issues on some games and apps
- Toggle xbox button layout by holding L3+L1+R1 for 3 seconds, toggle back by using the same button combo (GammaOS only)
- Can use home/back button as a hotkey for RetroArch (only when pressed with another button, otherwise acts as normal home/back button)
- Adjust screen brightness by holding home/back button and using the right analog stick UP/DOWN to adjust.
