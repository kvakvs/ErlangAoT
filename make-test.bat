@echo off
setlocal DisableDelayedExpansion
rem Match the Makefile test defaults; environment variables may override them.
if not defined CMAKE set "CMAKE=cmake"
if not defined CTEST set "CTEST=ctest"
if not defined BUILD_DIR set "BUILD_DIR=build/debug"
if not defined BUILD_TYPE set "BUILD_TYPE=Debug"
rem Development tests default to fast mode; set TEST_MODE=full to run every combination.
if not defined TEST_MODE set "TEST_MODE=fast"
rem CTest slots default to every logical CPU; each test takes two slots, so half run concurrently.
if not defined TEST_JOBS set "TEST_JOBS=%NUMBER_OF_PROCESSORS%"
set "TEST_FILTER="
if /i "%TEST_MODE%"=="fast" set "TEST_FILTER=-LE full_only"
set "CLAUSE_TEST_MODE=%TEST_MODE%"
rem An omitted count requests the native build tool's default parallelism.
set "PARALLEL_JOBS="
if defined JOBS set PARALLEL_JOBS="%JOBS%"
pushd "%~dp0" || exit /b 1
call tools\windows-toolchain.cmd
if errorlevel 1 goto finish
call "%CMAKE%" -S . -B "%BUILD_DIR%" %CLAUSE_CONFIGURE_ARGS% %CMAKE_ARGS% -DCMAKE_BUILD_TYPE="%BUILD_TYPE%" -DBUILD_TESTING=ON -DCLAUSE_BUILD_COMPILER=ON -DCLAUSE_BUILD_RUNTIME=ON
if errorlevel 1 goto finish
set "MAKEFLAGS="
call "%CMAKE%" --build "%BUILD_DIR%" --config "%BUILD_TYPE%" --parallel %PARALLEL_JOBS%
if errorlevel 1 goto finish
call "%CTEST%" --test-dir "%BUILD_DIR%" -C "%BUILD_TYPE%" --output-on-failure --no-tests=error --parallel %TEST_JOBS% %TEST_FILTER%
:finish
set "result=%errorlevel%"
popd
exit /b %result%
