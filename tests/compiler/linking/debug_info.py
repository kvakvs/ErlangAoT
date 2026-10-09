"""Check -g line tables through includes and macros for every target format, without a debugger (step 60)."""
from pathlib import Path
import re
import shutil
import subprocess
import sys

tool, fixtures, directory = sys.argv[1:]
work = Path(directory)
shutil.rmtree(work, ignore_errors=True)
work.mkdir(parents=True)
for name in ("debug.erl", "debug.hrl"):
    shutil.copyfile(Path(fixtures) / name, work / name)

# Each target's debug format: its module flag in IR and a line-table section name in objects.
TARGETS = {
    "x86_64-pc-windows-msvc": ("CodeView", b".debug$S"),
    "i686-pc-windows-msvc": ("CodeView", b".debug$S"),
    "x86_64-unknown-linux-gnu": ("Dwarf Version", b".debug_line"),
    "aarch64-unknown-linux-gnu": ("Dwarf Version", b".debug_line"),
    "armv7-unknown-linux-gnueabihf": ("Dwarf Version", b".debug_line"),
    "arm64-apple-macosx": ("Dwarf Version", b"__debug_line"),
}


def compile_to(kind, triple, root, *options):
    """Emit one artifact kind for a target and return its bytes."""
    result = subprocess.run([tool, *options, "--target-triple", triple, "--emit", kind, "--artifact-dir", root,
                             "debug.erl"], cwd=work, capture_output=True, timeout=60)
    assert result.returncode == 0 and not result.stdout and not result.stderr, (triple, kind, result)
    outputs = [path for path in (work / root).rglob("*") if path.is_file()]
    assert len(outputs) == 1, outputs
    return outputs[0].read_bytes()


def metadata(ir):
    """Each numbered metadata node of a module by its number."""
    return {int(number): text for number, text in re.findall(r"^!(\d+) = (.*)$", ir, re.MULTILINE)}


def scope_file(nodes, number):
    """The file name a location scope (a subprogram or a lexical block file) names."""
    node = nodes[number]
    return nodes[int(re.search(r"file: !(\d+)", node).group(1))]


def check_ir(triple, flag):
    """Functions keep their Erlang names and declaring files; macro code is located at its invocation."""
    ir = compile_to("llvm-ir", triple, f"ir-{triple}", "-g").decode("utf-8")
    assert f'!"{flag}"' in ir and "emissionKind: FullDebug" in ir, triple
    nodes = metadata(ir)
    twice = next(number for number, node in nodes.items() if node.startswith("distinct !DISubprogram(name: \"twice\""))
    assert 'debug.hrl"' in scope_file(nodes, twice) and "line: 4," in nodes[twice], nodes[twice]
    main = next(number for number, node in nodes.items() if node.startswith("distinct !DISubprogram(name: \"main\""))
    assert 'debug.erl"' in scope_file(nodes, main) and "line: 15," in nodes[main], nodes[main]
    # ?TWICE(X) expands on debug.hrl line 5: its multiplication is located there, in the included file.
    located = [(int(line), int(scope)) for line, scope in
               re.findall(r"!DILocation\(line: (\d+), scope: !(\d+)", ir)]
    assert any(line == 5 and 'debug.hrl"' in scope_file(nodes, scope) for line, scope in located), located
    assert any(line == 12 and 'debug.erl"' in scope_file(nodes, scope) for line, scope in located), located


for triple, (flag, section) in TARGETS.items():
    check_ir(triple, flag)
    assert section in compile_to("obj", triple, f"obj-{triple}", "-g"), triple
    assert section not in compile_to("obj", triple, f"plain-{triple}"), triple
    assert section in compile_to("obj", triple, f"o2-{triple}", "-g", "-O2"), triple
print(f"debug_info: line tables for {len(TARGETS)} targets at O0/O2")
