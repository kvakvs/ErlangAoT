@echo off
setlocal DisableDelayedExpansion
set "ERLANG_AOT_SCRIPT_ROOT=%~dp0"
rem Enumerate only project source trees, preserving spaces and special characters in paths.
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command ^
    "$ErrorActionPreference = 'Stop'; try {" ^
    "$formatter = $env:CLANG_FORMAT;" ^
    "if (-not $formatter) {" ^
    "$command = Get-Command clang-format.exe -ErrorAction SilentlyContinue;" ^
    "if ($command) { $formatter = $command.Source }" ^
    "else { $formatter = Join-Path $env:ProgramFiles 'LLVM/bin/clang-format.exe' }" ^
    "};" ^
    "$extensions = '.cpp', '.cc', '.cxx', '.hpp', '.h', '.hh', '.hxx';" ^
    "foreach ($tree in 'compiler', 'runtime', 'abi', 'tests') {" ^
    "$files = Get-ChildItem -LiteralPath (Join-Path $env:ERLANG_AOT_SCRIPT_ROOT $tree) -Recurse -File;" ^
    "foreach ($file in $files) {" ^
    "if ($extensions -ccontains $file.Extension) {" ^
    "& $formatter -i --style=file $file.FullName;" ^
    "if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }" ^
    "}" ^
    "}" ^
    "}" ^
    "} catch { [Console]::Error.WriteLine($_.Exception.Message); exit 1 }"
exit /b %errorlevel%
