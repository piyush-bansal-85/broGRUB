from pathlib import Path

source_path = Path(__file__).resolve().parent.parent

makefile = source_path / 'grub/grub-core/Makefile.core.def'
src = source_path / "src"

text = makefile.read_text()

# Find the bro_main module
start = text.find("module = {\n  name = bro_main;")

if start == -1: raise SystemExit("bro_main module not found")

end = text.find("\n};", start)

if end == -1: raise SystemExit("End of bro_main module not found")

module = text[start:end]

# Keep everything before the first generated source entry
marker = "  common = bro/grub_main.c;"
marker_pos = module.find(marker)

if marker_pos == -1: raise SystemExit("bro/grub_main.c entry not found")

base = module[:marker_pos + len(marker)]

# Find all .cpp files in src/
cpp_files = sorted(list(src.rglob("*.cpp")) + list(src.rglob("*.c")))

entries = []

for cpp in cpp_files:
    relative = cpp.relative_to(src)
    entries.append(f"  common = bro/src/{relative};")

new_module = base

if entries: new_module += "\n\n" + "\n".join(entries)

new_module += "\n"

text = text[:start] + new_module + text[end:]

makefile.write_text(text)

print(f"\033[92mRegistered {len(cpp_files)} C++ source file(s).\033[0m")