@echo off
adb shell mkdir -p /sdcard/Android/data/sogang.graphics.WaveFrontPathTracer/files/models
if errorlevel 1 exit /b 1
adb push assets/models/. /sdcard/Android/data/sogang.graphics.WaveFrontPathTracer/files/models/
