"""Check that inferred proofs remove runtime checks at -O2 and that every policy keeps them otherwise (step 59)."""
from pathlib import Path
import re
import subprocess
import sys

tool, fixture = sys.argv[1:]
source = Path(fixture) / "proofs.erl"


def bodies(*options):
    """The pre-optimization IR body of each function, by its name, under the given options."""
    result = subprocess.run([tool, *options, "--print-ir", str(source)], capture_output=True, text=True,
                            encoding="utf-8", timeout=60)
    assert result.returncode == 0, (options, result.stderr)
    found = {}
    for match in re.finditer(r"^define internal void @clausev1_70726f6f6673_([0-9a-f]+)_(\d+)\.body.*?^}$",
                             result.stdout, re.MULTILINE | re.DOTALL):
        name = bytes.fromhex(match.group(1)).decode("utf-8")
        found[f"{name}/{match.group(2)}"] = match.group(0)
    return found


def services(body):
    """The runtime services a body still calls to check or inspect values."""
    return set(re.findall(r"CLAUSE_(inspect|immediate|exact)_v\d", body))


proven = bodies("-O2")
generic = bodies("-O2", "--no-type-specialization")
baseline = bodies("-O0")
# Fully proven: no inspection, arithmetic or exact-equality service remains.
for function in ("norm/1", "squares/1", "swap/1", "first/1"):
    assert not services(proven[function]), (function, services(proven[function]))
    assert services(generic[function]) and services(baseline[function]), function
# Partly proven: the cells are read inline, but arithmetic keeps its generic fallback for unbounded operands.
assert services(proven["sum/2"]) == {"immediate"}, services(proven["sum/2"])
assert "inspect" in services(generic["sum/2"]) and "exact" in services(generic["sum/2"])
# The proven operand of edge/1 skips its tag test; the overflowing result keeps the fallback.
assert "immediate" in services(proven["edge/1"]) and proven["edge/1"].count("and i64") < generic["edge/1"].count(
    "and i64")
# Exported: callers are unknown, so nothing is proven and the body is the generic one.
assert proven["scale/2"] == generic["scale/2"]
# Only proven reads convert words to pointers; the generic bodies never do.
assert "inttoptr" not in "".join(generic.values()) and "proven.word" in proven["norm/1"]
print("proofs: removed checks in", sum(1 for name in proven if proven[name] != generic[name]), "functions")
