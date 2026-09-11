#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Chained Engine - Cross-Platform Release Packaging Script
Packages binaries, dependencies, C# managed assemblies, resources, and a starter template
into portable distribution archives (.zip for Windows, .tar.gz for Linux).
"""

import argparse
import os
import platform
import shutil
import sys
import tarfile
import zipfile
from pathlib import Path


def create_release_package(build_dir: Path, config: str, version: str, output_dir: Path, include_template: bool = True):
    root_dir = Path(__file__).resolve().parent.parent
    bin_dir = build_dir / "bin" / config

    if not bin_dir.exists():
        if (build_dir / "bin" / "ChainedEditor.exe").exists() or (build_dir / "bin" / "ChainedEditor").exists():
            bin_dir = build_dir / "bin"
        else:
            print(f"Error: Binary directory '{bin_dir}' not found.")
            sys.exit(1)

    system_name = platform.system().lower()
    is_windows = system_name == "windows"
    target_os = "windows-x64" if is_windows else "linux-x64"

    clean_version = version.replace('/', '-').replace('\\', '-').lstrip('v')
    package_name = f"ChainedEngine-v{clean_version}-{target_os}"
    staging_dir = output_dir / package_name

    if staging_dir.exists():
        shutil.rmtree(staging_dir, ignore_errors=True)
    staging_dir.mkdir(parents=True, exist_ok=True)

    print(f"==> Packaging Chained Engine v{clean_version} ({target_os}) [{config}]...")
    print(f"    Source Binaries: {bin_dir}")
    print(f"    Staging: {staging_dir}")

    # 1. Native Executables & Dynamic Libraries
    if is_windows:
        native_files = ["ChainedEditor.exe", "ChainedDecos.exe", "assimp.dll", "assimpd.dll"]
    else:
        native_files = ["ChainedEditor", "ChainedDecos", "libassimp.so", "libassimp.so.5"]

    for name in native_files:
        src = bin_dir / name
        if src.exists():
            shutil.copy2(src, staging_dir / name)
            if not is_windows and not name.endswith(".so"):
                (staging_dir / name).chmod(0o755)
            print(f"  [+] Native binary: {name}")

    # 2. Managed Assemblies & Runtime Configs (C# Scripting)
    for pattern in ["Coral.Managed.*", "Chained.Managed.*", "Chained.Managed.Generator.*"]:
        for f in bin_dir.glob(pattern):
            if f.suffix.lower() in (".dll", ".json"):
                shutil.copy2(f, staging_dir / f.name)
                print(f"  [+] Managed assembly: {f.name}")

    # 3. Engine Resources
    resources_src = root_dir / "resources"
    if resources_src.exists():
        resources_dst = staging_dir / "resources"
        shutil.copytree(resources_src, resources_dst, dirs_exist_ok=True)
        print(f"  [+] Resources: {resources_dst}")

    # 4. Starter Project Template (Clean lightweight starter)
    if include_template:
        template_src = root_dir / "game" / "NewProject5"
        if template_src.exists():
            template_dst = staging_dir / "projects" / "starter_project"
            def ignore_temp(folder, files):
                return [f for f in files if f in (".vs", "bin", "obj", ".texture_cache", ".chcache")]
            shutil.copytree(template_src, template_dst, ignore=ignore_temp, dirs_exist_ok=True)
            print(f"  [+] Starter template: projects/starter_project")

    # 5. README instructions
    readme_content = f"""Chained Engine v{clean_version} ({target_os})
==============================================

REQUIREMENTS:
- OS: {'Windows 10/11 64-bit' if is_windows else 'Linux (x86_64, OpenGL 4.5+)'}
- GPU: OpenGL 4.5+ capable graphics card
- .NET: .NET 9.0 SDK or higher (required for C# game scripting)
  Download from: https://dotnet.microsoft.com/download

HOW TO RUN:
{'1. Run ChainedEditor.exe' if is_windows else '1. Run ./ChainedEditor'}
2. Open projects/starter_project/NewProject5.chproject or create a new project.
3. Press Play (F5) to test the scene.

GitHub: https://github.com/IOleg-crypto/Chained-Engine
"""
    (staging_dir / "README.txt").write_text(readme_content, encoding="utf-8")
    print("  [+] Generated README.txt")

    # 6. Archive Creation
    output_dir.mkdir(parents=True, exist_ok=True)
    if is_windows:
        archive_path = output_dir / f"{package_name}.zip"
        print(f"==> Compressing into {archive_path.name}...")
        with zipfile.ZipFile(archive_path, "w", zipfile.ZIP_DEFLATED) as zf:
            for root, _, files in os.walk(staging_dir):
                for file in files:
                    file_path = Path(root) / file
                    arcname = file_path.relative_to(staging_dir)
                    zf.write(file_path, arcname)
    else:
        archive_path = output_dir / f"{package_name}.tar.gz"
        print(f"==> Compressing into {archive_path.name}...")
        with tarfile.open(archive_path, "w:gz") as tf:
            tf.add(staging_dir, arcname=package_name)

    shutil.rmtree(staging_dir, ignore_errors=True)
    size_mb = archive_path.stat().st_size / (1024 * 1024)
    print(f"==> Release package ready: {archive_path} ({size_mb:.2f} MB)")
    return archive_path


def main():
    parser = argparse.ArgumentParser(description="Package Chained Engine release.")
    parser.add_argument("--build-dir", type=Path, default=Path("build/windows-clang"), help="Path to build directory")
    parser.add_argument("--config", type=str, default="Release", help="Build configuration")
    parser.add_argument("--version", type=str, default="1.0.0-beta", help="Version tag")
    parser.add_argument("--output", type=Path, default=Path("dist"), help="Output directory")
    parser.add_argument("--no-template", action="store_true", help="Skip starter template")

    args = parser.parse_args()
    create_release_package(
        build_dir=args.build_dir,
        config=args.config,
        version=args.version,
        output_dir=args.output,
        include_template=not args.no_template
    )


if __name__ == "__main__":
    main()
