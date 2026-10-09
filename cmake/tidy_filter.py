"""Filter one clang-tidy batch's output: drop static analyzer reports that end inside Boost headers (false positives
in Boost.Multiprecision's limb storage that rewriting our code only moves around; docs/validation.md), print the
rest, and exit 1 when a diagnostic remains or the runner failed for another reason."""
import re
import sys

# A diagnostic's first line: location, severity, message and the bracketed check names.
HEADER = re.compile(r'^(?P<path>.+?):\d+:\d+: (?P<level>error|warning): .*\[(?P<checks>[^\]]+)\]\s*$')
# Paths whose analyzer reports are not ours.
IGNORED_PATH = re.compile(r'[/\\]thirdparty[/\\]boost_[^/\\]+[/\\]')


def ignored(match):
    """Whether a diagnostic is only a static analyzer report located in Boost."""
    checks = [name for name in match['checks'].split(',') if not name.startswith('-warnings-as-errors')]
    return bool(IGNORED_PATH.search(match['path'])) and all(name.startswith('clang-analyzer-') for name in checks)


def main():
    """Reads the runner's output file and status; prints the kept output and returns the batch status."""
    output_path, status = sys.argv[1], int(sys.argv[2])
    with open(output_path, encoding='utf-8', errors='replace') as stream:
        lines = stream.read().splitlines()
    kept, dropped, remaining, skipping = [], 0, 0, False
    for line in lines:
        if match := HEADER.match(line):
            skipping = ignored(match)
            dropped += skipping
            remaining += not skipping
        if not skipping:
            kept.append(line)
    print('\n'.join(kept))
    if dropped:
        print(f'clang-tidy: ignored {dropped} static analyzer report(s) located in Boost headers')
    # A failed runner without any diagnostic (a crash) still fails; only Boost reports make a failure pass.
    sys.exit(1 if remaining or (status != 0 and not dropped) else 0)


if __name__ == '__main__':
    main()
