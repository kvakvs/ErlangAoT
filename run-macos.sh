#!/bin/bash
set -euo pipefail

# Resolve build paths from the repository while preserving the caller's working directory.
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
build_dir=${BUILD_DIR:-build/debug}
if [[ "$build_dir" != /* ]]; then
    build_dir="$project_dir/$build_dir"
fi

# Keep build output off the compiler's stdout and stop if configuration or compilation fails.
make -C "$project_dir" build BUILD_DIR="$build_dir" >&2

# Support both single-configuration and configuration-specific CMake output directories.
executable=
while IFS= read -r -d '' candidate; do
    if [[ -x "$candidate" && ( -z "$executable" || "$candidate" -nt "$executable" ) ]]; then
        executable=$candidate
    fi
done < <(find "$build_dir/bin" -type f -name erlangaot -print0)

if [[ -z "$executable" ]]; then
    printf 'No executable erlangaot found under %s/bin\n' "$build_dir" >&2
    exit 1
fi

# Preserve argument boundaries, exit status, signals, and caller-relative input paths.
exec "$executable" "$@"
