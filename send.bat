cmake . -G Ninja
ninja
adb push .\touch /data/local/tmp
adb shell chmod 777 /data/local/tmp/touch