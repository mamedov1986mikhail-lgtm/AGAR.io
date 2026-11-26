@echo off
cd /d "%~dp0"

start "AGAR server" cmd /k C:\Users\mamed\source\repos\AGAR.io\out\build\x64-Debug\agar_server.exe
timeout /t 1 /nobreak >nul
start "" C:\Users\mamed\source\repos\AGAR.io\out\build\x64-Debug\agar_client.exe