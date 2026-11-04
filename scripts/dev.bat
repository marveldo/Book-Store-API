@echo off
SETLOCAL

:: Run watchexec targeting cmd.exe as the execution shell
watchexec -r --shell=cmd -e cc,h,cpp,hpp,txt,env -i build -i logs -- ^
    "cmake --build build && .\build\cpp_server.exe"

ENDLOCAL
