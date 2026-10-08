@echo off
rem Select the supported Windows toolchain (clang-cl, Ninja Multi-Config) for the repository batch scripts.
rem Called from the repository root with BUILD_DIR set; leaves configure arguments in CLAUSE_CONFIGURE_ARGS.
rem CLAUSE_TOOLCHAIN=default keeps CMake's own generator and compiler choice (for example MSVC cl).
set "CLAUSE_CONFIGURE_ARGS="
if /i "%CLAUSE_TOOLCHAIN%"=="default" exit /b 0

rem Enter the x64 developer environment unless the caller already runs in one.
if defined VCINSTALLDIR goto have_msvc
set "vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%vswhere%" goto no_msvc
set "vs_root="
for /f "usebackq delims=" %%I in (`"%vswhere%" -latest -products * -property installationPath`) do set "vs_root=%%I"
if not defined vs_root goto no_msvc
call "%vs_root%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if not defined VCINSTALLDIR goto no_msvc
:have_msvc

rem The globally installed LLVM provides clang-cl; Visual Studio's environment provides Ninja.
where clang-cl >nul 2>&1 || set "PATH=%ProgramFiles%\LLVM\bin;%PATH%"
where clang-cl >nul 2>&1 || (echo clang-cl was not found; install LLVM/Clang globally. 1>&2 & exit /b 1)
where ninja >nul 2>&1 || (echo ninja was not found; install Ninja or the Visual Studio CMake tools. 1>&2 & exit /b 1)
set CLAUSE_CONFIGURE_ARGS=-G "Ninja Multi-Config" -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

rem A cache made by another generator or compiler cannot switch in place; configure it afresh.
set "cache=%BUILD_DIR%\CMakeCache.txt"
if not exist "%cache%" exit /b 0
findstr /b /c:"CMAKE_GENERATOR:INTERNAL=Ninja Multi-Config" "%cache%" >nul 2>&1 || goto fresh
findstr /i /r /c:"^CMAKE_CXX_COMPILER:.*clang-cl" "%cache%" >nul 2>&1 || goto fresh
exit /b 0

:fresh
echo Reconfiguring %BUILD_DIR% from scratch for clang-cl and Ninja Multi-Config. 1>&2
set CLAUSE_CONFIGURE_ARGS=--fresh %CLAUSE_CONFIGURE_ARGS%
exit /b 0

:no_msvc
echo Visual Studio with the C++ workload was not found; run from a Visual Studio developer shell. 1>&2
exit /b 1
