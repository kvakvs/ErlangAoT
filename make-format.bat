@echo off
setlocal DisableDelayedExpansion
set "ERLANG_AOT_SCRIPT_ROOT=%~dp0"
rem FORMAT_SCOPE=changed (default) formats C++ files changed since HEAD plus untracked files; all formats every file.
rem Enumerate only project source trees, preserving spaces and special characters in paths.
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command ^
    "$ErrorActionPreference = 'Stop'; try {" ^
    "$formatter = $env:CLANG_FORMAT;" ^
    "if (-not $formatter) {" ^
    "$command = Get-Command clang-format.exe -ErrorAction SilentlyContinue;" ^
    "if ($command) { $formatter = $command.Source }" ^
    "else { $formatter = Join-Path $env:ProgramFiles 'LLVM/bin/clang-format.exe' }" ^
    "};" ^
    "$root = $env:ERLANG_AOT_SCRIPT_ROOT;" ^
    "$scope = if ($env:FORMAT_SCOPE) { $env:FORMAT_SCOPE } else { 'changed' };" ^
    "if ($scope -eq 'all') {" ^
    "$paths = foreach ($tree in 'compiler', 'runtime', 'abi', 'tests') {" ^
    "Get-ChildItem -LiteralPath (Join-Path $root $tree) -Recurse -File | ForEach-Object FullName" ^
    "}" ^
    "} elseif ($scope -eq 'changed') {" ^
    "$names = @(git -C $root diff --name-only HEAD --) + @(git -C $root ls-files --others --exclude-standard);" ^
    "if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE };" ^
    "$paths = $names | Sort-Object -Unique | Where-Object { $_ -match '^(compiler|runtime|abi|tests)/' } |" ^
    "ForEach-Object { Join-Path $root $_ } | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf }" ^
    "} else { throw 'FORMAT_SCOPE must be changed or all' };" ^
    "$extensions = '.cpp', '.cc', '.cxx', '.hpp', '.h', '.hh', '.hxx';" ^
    "foreach ($path in $paths) {" ^
    "if ($extensions -ccontains [IO.Path]::GetExtension($path)) {" ^
    "& $formatter -i --style=file $path;" ^
    "if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }" ^
    "}" ^
    "}" ^
    "} catch { [Console]::Error.WriteLine($_.Exception.Message); exit 1 }"
exit /b %errorlevel%
