@echo off
title SwiftRoute Launcher
if not exist bin mkdir bin
echo Compiling SwiftRoute Desktop App...
g++ -std=c++17 -O2 src\main.cpp -o bin\SwiftRoute.exe -mwindows -lgdi32 -lcomctl32
if %errorlevel% neq 0 (
    echo [ERROR] Compilation failed!
    pause
    exit /b %errorlevel%
)
echo Launching SwiftRoute...
start "" "bin\SwiftRoute.exe"
