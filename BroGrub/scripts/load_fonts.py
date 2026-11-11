from pathlib import Path

def load_fonts():
    ''' Loads fonts from the fonts directory and writes them to fonts.cfg. '''
    source_path = Path(__file__).resolve().parent.parent / "bro-efi/boot/grub"

    print("\033[1;95mEmptying fonts.cfg...\033[0m")
    # blanks the file and starts with insmod font.
    with open(source_path / "fonts.cfg", "w") as f: f.write(f"insmod font\n")

    print("\033[1;92mUpdating fonts.cfg...\033[0m")
    with open(source_path / "fonts.cfg", "a") as f:
        for font in (source_path / "fonts").rglob("*.pf2"):
            f.write(f"loadfont /boot/grub/fonts/{font.name}\n")

if __name__ == "__main__": load_fonts()