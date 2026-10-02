"""Execute normalized immediate patterns against OTP in both CLI modes/four policies."""
import json
import struct
import pathlib
import re
import sys
from stored import load
from evidence import digest, provenance, run


def token(value):
    """Encode runtime-admitted values independently of native representation and allocation identity."""
    if isinstance(value, tuple):
        return "t(" + ",".join(map(token, value)) + ")" if value else "tuple"
    if isinstance(value, list):
        return "c(" + token(value[0]) + "," + token(value[1:]) + ")" if value else "nil"
    if isinstance(value, dict) and "bits" in value:
        return "b" + str(value["length"]) + ":" + value["bits"]
    if isinstance(value, dict) and "map" in value:
        return "m(" + ",".join(token(tuple(entry)) for entry in value["map"]) + ")"
    if isinstance(value, dict):
        return "c(" + ",".join(map(token, value['cons'])) + ")"
    if isinstance(value, float):
        return "f" + struct.pack(">d", value).hex()
    if isinstance(value, int):
        return f"i{value}"
    return value if value in ["nil", "tuple"] else "a" + value.encode().hex()


def erl(value):
    """Supply independent Erlang terms to the OTP execution oracle."""
    if isinstance(value, tuple):
        return "{" + ",".join(map(erl, value)) + "}"
    if isinstance(value, list):
        return "[" + ",".join(map(erl, value)) + "]"
    if isinstance(value, dict) and "bits" in value:
        data = bytes.fromhex(value['bits'])
        count = value['length']
        full = count // 8
        parts = list(map(str,data[:full]))
        if count % 8:
            parts.append(str(data[full] >> (8-count%8)) + ':' + str(count%8))
        return '<<' + ','.join(parts) + '>>'
    if isinstance(value, dict) and "map" in value:
        return "#{" + ",".join(erl(k) + "=>" + erl(v) for k,v in value["map"]) + "}"
    if isinstance(value, dict):
        return "[" + "|".join(map(erl, value['cons'])) + "]"
    if isinstance(value, float):
        text = repr(value)
        if "e" in text and "." not in text.split("e")[0]:
            text = text.replace("e", ".0e")
        return text
    if isinstance(value, int):
        return str(value)
    return {"nil": "[]", "tuple": "{}"}.get(value, "'" + value + "'")


def prepare(source, otp, work):
    """Pin suite adaptations with exact source, clause and wrapper bytes and retained notices."""
    fixture = source / "tests/fixtures/patternmatch/immediate.json"
    data = json.loads(fixture.read_text(encoding="utf-8"))
    suite = otp / "lib/compiler/test/match_SUITE.erl"
    text = suite.read_text(encoding="utf-8")
    declarations = "-module(answer).\n-export([" + ",".join(
        f'{re.search(r"(\w+)\(", clause).group(1)}/{2 if clause.startswith(("repeat", "wildcard", "-spec")) else 1}'
        for clause in data["functions"]) + "]).\n"
    for witness in ['char_alias_1(118=$v) -> ok;', 'str_alias_1(""=[]) -> ok;',
                    'multiple_aliases_2((A=B) = (A=C)) ->']:
        assert witness in text, witness
    (work / "answer.erl").write_bytes((text.split("-module(")[0] + declarations + "\n".join(data["functions"]) + "\n").encode())
    client = "-module(client).\n-export([nested/1, retry/1]).\nnested(X) -> answer:repeat(answer:literal(X), no).\nretry(X) -> answer:aliases(X).\n"
    (work / "client.erl").write_bytes(client.encode())
    calls = [("answer", name, args) for name, args in data["calls"]]
    calls += [("client", "nested", [119]), ("client", "nested", [118]), ("client", "retry", ["ok"])]
    (work / "calls.txt").write_bytes(("\n".join(f'{module} {name} {len(args)} ' + " ".join(map(token, args))
        for module, name, args in calls) + "\n").encode())
    (work / "calls.term").write_bytes(("\n".join(f'{{{module},{name},[' + ",".join(map(erl, args)) + "]}."
        for module, name, args in calls) + "\n").encode())
    (work / "project.toml").write_bytes(b'schema_version=1\n[[targets]]\nname="matching"\nsources=["answer.erl","client.erl"]\n')
    return {"source": str(suite.relative_to(otp)), "source_sha256": digest(suite),
        "functions": "char_alias_1/1; str_alias_1/1; multiple_aliases_2/1",
        "adaptations": "selected immediate clauses remove later/fallback clauses (step 9); rename helpers; empty alias returns []; multiple_aliases_2 returns C instead of tuple, retaining all alias constraints; authored repeat/wildcard/constant/boundaries/wrong-spec/nested-call kernels",
        "fixture_sha256": digest(fixture), "wrapper_sha256": digest(work / "answer.erl"), "calls": len(calls)}


def native(tool, cmake, source, work, settings, config, suffix):
    """Use the separate real CLI object consumer, covering positional/project publication."""
    for level, extra, mode in [("O0", "", "O0"), ("O0", "--no-type-specialization", "O0-off"),
                               ("O2", "", "O2"), ("O2", "--no-type-specialization", "O2-off")]:
        for project in [False, True]:
            run([cmake, f"-DTOOL={tool}", f"-DOPTIMIZATION={level}", f"-DEXTRA_OPTIONS={extra}",
                 f"-DPROJECT_MODE={project}", f"-DSOURCE_ROOT={source.as_posix()}",
                 f"-DTEST_DIR={(work / mode).as_posix()}", f"-DINPUT_ROOT={work.as_posix()}",
                 f"-DHOST_SETTINGS={settings}", f"-DHOST_CONFIG={config}", f"-DHOST_SUFFIX={suffix}",
                 "-P", str(source / "tests/compiler/codegen/match.cmake")])


def widths(tool, work):
    """Both-width immediate endpoints and promoted literals are object/IR evidence, not foreign execution."""
    records = []
    for bits, triple, bound in [(32, "i686-pc-windows-msvc", 1 << 27), (64, "x86_64-pc-windows-msvc", 1 << 59)]:
        path = work / "width.erl"
        path.write_text(f"-module(width).\nf(-{bound}, {bound-1}) -> ok.\n", encoding="utf-8")
        ir = run([tool, "--target-triple", triple, "--print-ir", str(path)])
        assert f"i{bits}" in ir and "exact.outcome" in ir and "match.mismatch" in ir
        run([tool, "--target-triple", triple, "--emit", "obj", "--artifact-dir", str(work / f"width{bits}"), str(path)])
        path.write_text(f"-module(width).\nf({bound}) -> ok.\n", encoding="utf-8")
        ir = run([tool, "--target-triple", triple, "--print-ir", str(path)])
        assert "integer.outcome" in ir
        run([tool, "--target-triple", triple, "--emit", "obj", "--artifact-dir", str(work / f"promoted{bits}"), str(path)])
        records.append({"bits": bits, "triple": triple, "execution": "not attempted", "endpoints": "accepted", "overflow": "promoted"})
    return records


def main():
    tool, cmake, root, directory, settings, config, suffix = sys.argv[1:]
    source, work = pathlib.Path(root), pathlib.Path(directory)
    work.mkdir(parents=True, exist_ok=True)
    records = load(source, "immediate", work)
    native(tool, cmake, source, work, settings, config, suffix)
    records["widths"] = widths(tool, work)
    (work / "evidence.json").write_text(json.dumps(records, indent=2) + "\n", encoding="utf-8")
    print(f'{records["calls"]} OTP/native immediate calls; four policies; ownership/recovery; both-width objects passed.')


if __name__ == "__main__":
    main()
