@echo off
setlocal DisableDelayedExpansion
rem Match the Makefile build defaults; environment variables may override them.
if not defined CMAKE set "CMAKE=cmake"
if not defined BUILD_DIR set "BUILD_DIR=build/debug"
if not defined BUILD_TYPE set "BUILD_TYPE=Debug"
rem An omitted count requests the native build tool's default parallelism.
set "PARALLEL_JOBS="
if defined JOBS set PARALLEL_JOBS="%JOBS%"
pushd "%~dp0" || exit /b 1
call "%CMAKE%" -S . -B "%BUILD_DIR%" %CMAKE_ARGS% -DCMAKE_BUILD_TYPE="%BUILD_TYPE%" -DBUILD_TESTING=OFF -DERLANG_AOT_BUILD_COMPILER=ON -DERLANG_AOT_BUILD_RUNTIME=ON
if errorlevel 1 goto finish
set "MAKEFLAGS="
call "%CMAKE%" --build "%BUILD_DIR%" --config "%BUILD_TYPE%" --target erlang_aot --parallel %PARALLEL_JOBS%
:finish
set "result=%errorlevel%"
popd
exit /b %result%
