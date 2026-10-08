@echo off
setlocal DisableDelayedExpansion
set "CLAUSE_SCRIPT_ROOT=%~dp0"
rem Match Makefile clean, validating direct-child paths before recursive removal.
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command ^
    "$ErrorActionPreference = 'Stop'; try {" ^
    "$root = (Get-Item -LiteralPath $env:CLAUSE_SCRIPT_ROOT).FullName.TrimEnd('\');" ^
    "$targets = Get-ChildItem -LiteralPath $root -Directory -Force" ^
    "| Where-Object { $_.Name -eq 'build' -or $_.Name -like 'cmake-build*' };" ^
    "foreach ($target in $targets) {" ^
    "$resolved = (Resolve-Path -LiteralPath $target.FullName).ProviderPath;" ^
    "if ((Split-Path -Parent $resolved) -ne $root" ^
    "-or ($target.Attributes -band [IO.FileAttributes]::ReparsePoint)) {" ^
    "throw ('Refusing to clean a path outside the repository or a linked directory: ' + $resolved)" ^
    "}" ^
    "};" ^
    "foreach ($target in $targets) { Remove-Item -LiteralPath $target.FullName -Recurse -Force }" ^
    "} catch { [Console]::Error.WriteLine($_.Exception.Message); exit 1 }"
exit /b %errorlevel%
