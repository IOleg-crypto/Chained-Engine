"""Sync C# script DLLs from build output to game assets directories.

After CMake builds C# scripts into build/<preset>/bin/<Config>/scripts/<Game>/,
this script copies them to:
  1. game/<game>/assets/bin/         — for the editor (source tree)
  2. build/<preset>/bin/<Config>/assets/bin/ — for runtime (build output)

Usage:
    python tools/sync_scripts.py sync \\
        --build-dir build/windows-clang/bin/Debug \\
        --game-dir game/chaineddecos
"""
import argparse
import filecmp
import os
import shutil
from pathlib import Path


def sync_scripts(build_dir: Path, game_dir: Path) -> None:
    """Copy C# DLLs from scripts/<Game>/ to assets/bin/ in both source and build."""
    scripts_src = build_dir / "scripts"

    if not scripts_src.is_dir():
        print(f"Warning: Scripts directory '{scripts_src}' does not exist. Skipping.")
        return

    # Find game script directories under scripts/
    game_dirs = [d for d in scripts_src.iterdir() if d.is_dir()]
    if not game_dirs:
        print("Warning: No game script directories found in scripts/. Skipping.")
        return

    for game_scripts_dir in game_dirs:
        game_name = game_scripts_dir.name

        # Destination 1: source tree (for editor)
        game_assets_bin = game_dir / "assets" / "bin"

        # Destination 2: build output (for runtime)
        build_assets_bin = build_dir / "assets" / "bin"

        for dest in [game_assets_bin, build_assets_bin]:
            _sync_dlls(game_scripts_dir, dest)

    print("Success: scripts synced!")


def _safe_copy(src: Path, dst: Path) -> None:
    """Copy src to dst without failing on NTFS DrvFs utime() restrictions.

    shutil.copy2 copies file metadata including timestamps via utime().  On
    WSL2 the destination may live on an NTFS mount (/mnt/…) where DrvFs does
    not support utime(), raising PermissionError.  This helper copies data and
    tries to copy permissions, silently skipping the timestamp update if it
    would fail — which is safe for script deployment.
    """
    shutil.copyfile(src, dst)
    try:
        shutil.copystat(src, dst)
    except (PermissionError, OSError):
        # Timestamp copy not supported on NTFS DrvFs — data is already copied.
        pass


def _sync_dlls(src: Path, dst: Path) -> None:
    """Copy DLL/PDB/runtimeconfig files from src to dst."""
    if not src.is_dir():
        return

    dst.mkdir(parents=True, exist_ok=True)

    copied = 0
    for file in src.iterdir():
        if file.suffix.lower() in (".dll", ".pdb", ".json"):
            dest_file = dst / file.name
            if not dest_file.exists() or not filecmp.cmp(file, dest_file, shallow=False):
                _safe_copy(file, dest_file)
                copied += 1

    print(f"  {src.name} -> {dst}  ({copied} files synced)")


def main() -> None:
    parser = argparse.ArgumentParser(description="Sync C# script DLLs to game assets")
    parser.add_argument("--build-dir", required=True, help="Build output directory (build/<preset>/bin/<Config>)")
    parser.add_argument("--game-dir", required=True, help="Game project directory (game/<game>)")

    args = parser.parse_args()

    sync_scripts(
        Path(args.build_dir).resolve(),
        Path(args.game_dir).resolve(),
    )


if __name__ == "__main__":
    main()
