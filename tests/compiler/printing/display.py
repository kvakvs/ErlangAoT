"""CTest: compile erlang:display/1 calls with both drivers and every policy, and compare stdout with OTP."""
import pathlib
import sys
import values

sys.path.insert(0, str(values.ROOT / 'tests/compiler/patternmatch'))
from evidence import run  # noqa: E402
from matrix import combinations  # noqa: E402


def main():
    """Verifies the goldens, then runs the shared native consumer over the OTP-observed display calls."""
    tool, cmake, directory, settings, config, suffix = sys.argv[1:]
    manifest = values.verify()
    work = pathlib.Path(directory)
    for level, extra, mode, project in combinations():
        run([cmake, f'-DTOOL={tool}', f'-DOPTIMIZATION={level}', f'-DEXTRA_OPTIONS={extra}',
             f'-DPROJECT_MODE={project}', f'-DSOURCE_ROOT={values.ROOT.as_posix()}',
             f'-DTEST_DIR={(work / mode).as_posix()}', f'-DINPUT_ROOT={(values.FIXTURES / "display").as_posix()}',
             f'-DHOST_SETTINGS={settings}', f'-DHOST_CONFIG={config}', f'-DHOST_SUFFIX={suffix}',
             '-P', str(values.ROOT / 'tests/compiler/codegen/match.cmake')])
    print(f"{manifest['counts']['calls']} compiled erlang:display/1 calls match OTP stdout")


if __name__ == '__main__':
    main()
