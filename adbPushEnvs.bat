@echo off
adb shell mkdir -p /sdcard/Android/data/sogang.graphics.WaveFrontPathTracer/files/envs
if errorlevel 1 exit /b 1
adb push envs/. /sdcard/Android/data/sogang.graphics.WaveFrontPathTracer/files/envs/
