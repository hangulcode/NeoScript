@echo off
REM Build C++ benchmark (MSVC /O2). Output: bench_cpp.exe
REM /utf-8 is required: the source is UTF-8; CP949 misreading makes a comment
REM swallow the following line, silently deleting whole functions.
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /O2 /EHsc /std:c++17 /utf-8 "%~dp0bench.cpp" /Fe:"%~dp0bench_cpp.exe" /Fo:"%~dp0bench_cpp.obj"
