@echo off
setlocal DisableDelayedExpansion
rem Match the Makefile build defaults; environment variables may override them.
if not defined CMAKE set "CMAKE=cmake"
if not defined BUILD_DIR set "BUILD_DIR=build/debug"
if not defined BUILD_TYPE set "BUILD_TYPE=Debug"
if not defined JOBS set "JOBS=2"
pushd "%~dp0" || exit /b 1
call "%CMAKE%" -S . -B "%BUILD_DIR%" %CMAKE_ARGS% -DCMAKE_BUILD_TYPE="%BUILD_TYPE%" -DBUILD_TESTING=OFF -DERLANG_AOT_BUILD_COMPILER=ON -DERLANG_AOT_BUILD_RUNTIME=ON
if errorlevel 1 goto finish
set "MAKEFLAGS="
call "%CMAKE%" --build "%BUILD_DIR%" --config "%BUILD_TYPE%" --target erlang_aot --parallel "%JOBS%"
:finish
set "result=%errorlevel%"
popd
exit /b %result%
