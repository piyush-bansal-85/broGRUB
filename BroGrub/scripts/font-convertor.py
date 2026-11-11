from pathlib import Path
import subprocess
import re
from load_fonts import load_fonts

root = Path.home()

fileName: str = "Bricolage_Grotesque"
fontName: str = "BricolageGrotesque"

FONT_SOURCE = root / f"Downloads/{fileName}"
FONT_DEST = root / "BroOS/BroGrub/bro-efi/boot/grub/fonts"
GRUB_MKFONT = root / "BroOS/BroGrub/build/grub-mkfont"


# Remove previously generated PF2 files
for p in FONT_DEST.rglob("*.pf2"):
    if p.is_file() and p.stem.lower().startswith(fontName.lower()):
        print(f"\033[1;93mRemoving {p.name}\033[0m")
        p.unlink()


if not FONT_SOURCE.exists():
    raise FileNotFoundError(
        f"Font folder not found: {FONT_SOURCE}"
    )

if not GRUB_MKFONT.exists():
    raise FileNotFoundError(
        f"grub-mkfont not found: {GRUB_MKFONT}"
    )


FONT_DEST.mkdir(parents=True, exist_ok=True)


for ttf in FONT_SOURCE.rglob("*.ttf"):

    # Example:
    # BricolageGrotesque_24pt-Regular.ttf
    # BricolageGrotesque_24pt_Condensed-Bold.ttf

    match = re.search(r"_(\d+)pt", ttf.stem)

    font_size = int(match.group(1)) if match else 16

    output = FONT_DEST / (
        f"{ttf.stem}_{font_size}.pf2"
    )

    print(
        f"\033[1;94mConverting: "
        f"{ttf.name} → {output.name} "
        f"({font_size}pt)\033[0m"
    )

    subprocess.run([
        str(GRUB_MKFONT),
        "-s", str(font_size),
        "-o", str(output),
        str(ttf)
    ], check=True)


print("\n\033[1;92mDone!\033[0m")

load_fonts()