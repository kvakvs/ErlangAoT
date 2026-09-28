@echo off
setlocal DisableDelayedExpansion
rem Match the Makefile test defaults; environment variables may override them.
if not defined CMAKE set "CMAKE=cmake"
if not defined CTEST set "CTEST=ctest"
if not defined BUILD_DIR set "BUILD_DIR=build/debug"
if not defined BUILD_TYPE set "BUILD_TYPE=Debug"
rem An omitted count requests the native build tool's default parallelism.
set "PARALLEL_JOBS="
if defined JOBS set PARALLEL_JOBS="%JOBS%"
pushd "%~dp0" || exit /b 1
call "%CMAKE%" -S . -B "%BUILD_DIR%" %CMAKE_ARGS% -DCMAKE_BUILD_TYPE="%BUILD_TYPE%" -DBUILD_TESTING=ON -DERLANG_AOT_BUILD_COMPILER=ON -DERLANG_AOT_BUILD_RUNTIME=ON
if errorlevel 1 goto finish
set "MAKEFLAGS="
call "%CMAKE%" --build "%BUILD_DIR%" --config "%BUILD_TYPE%" --parallel %PARALLEL_JOBS%
if errorlevel 1 goto finish
call "%CTEST%" --test-dir "%BUILD_DIR%" -C "%BUILD_TYPE%" --output-on-failure --no-tests=error
:finish
set "result=%errorlevel%"
popd
exit /b %result%
