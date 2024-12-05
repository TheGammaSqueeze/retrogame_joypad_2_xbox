./make.sh
adb root
adb shell killall rgp2xbox
adb shell setprop ctl.stop rgp2xbox
adb shell killall rgp2xbox
adb shell mkdir -p /data/test/
adb shell chmod 777 /data/test
adb push a.out /data/test/rgp2xbox
adb shell chmod +x /data/test/rgp2xbox
adb shell /data/test/rgp2xbox
