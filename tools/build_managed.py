"""Build managed C# projects for Chained Engine."""
import argparse
import os
import platform
import shutil
import subprocess
from pathlib import Path
from typing import Optional

CORAL_ARTIFACTS = [
    "Coral.Managed.dll",
    "Coral.Managed.runtimeconfig.json",
    "Coral.Managed.deps.json",
    "Coral.Managed.pdb",
]

PROPS_TEMPLATE = (
    "<Project>\n"
    "  <PropertyGroup>\n"
    "    <CoralManagedDir>$(MSBuildThisFileDirectory){relative_coral_dir}</CoralManagedDir>\n"
    "  </PropertyGroup>\n"
    "</Project>\n"
)


def find_dotnet() -> Optional[str]:
    if dotnet_root := os.environ.get("DOTNET_ROOT"):
        for name in ("dotnet.exe", "dotnet"):
            if (candidate := Path(dotnet_root) / name).exists():
                return str(candidate)
    if dotnet := shutil.which("dotnet"):
        return dotnet
    common = [
        "C:/Program Files/dotnet/dotnet.exe",
        "C:/Program Files (x86)/dotnet/dotnet.exe",
        "/usr/bin/dotnet",
        "/usr/share/dotnet/dotnet",
    ]
    return next((str(p) for path in common if (p := Path(path)).exists()), None)


def write_if_changed(path: Path, content: str) -> None:
    if not (path.exists() and path.read_text(encoding="utf-8") == content):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")


def _is_ntfs_mount(path: Path) -> bool:
    """Return True when path lives on a Windows NTFS mount under WSL2/Linux (/mnt/)."""
    if platform.system() != "Linux":
        return False
    try:
        return str(path.resolve()).startswith("/mnt/")
    except Exception:
        return False


def build_managed(
    project: Path,
    output_dir: Path,
    coral_dir: Path,
    configuration: str,
    copy_coral: bool,
    parallel: bool,
    write_props: bool,
    intermediate_dir: Optional[Path] = None,
) -> None:
    if not (dotnet := find_dotnet()):
        raise FileNotFoundError("Could not find dotnet. Set DOTNET_ROOT or add dotnet to PATH.")

    # On WSL2 the source tree lives on an NTFS mount (/mnt/d/).
    # Writing ManagedDependencies.props into the source directory fails there;
    # the .csproj already imports it conditionally so skipping is safe.
    if write_props and not _is_ntfs_mount(project.parent):
        relative = os.path.normpath(os.path.relpath(coral_dir, start=project.parent))
        write_if_changed(
            project.parent / "ManagedDependencies.props",
            PROPS_TEMPLATE.format(relative_coral_dir=relative),
        )

    output_dir.mkdir(parents=True, exist_ok=True)
    command = [
        dotnet, "build", str(project),
        "-c", configuration,
        "--output", str(output_dir),
        f"-p:CoralManagedDir={coral_dir}",
    ]

    # On WSL2, the source tree lives on an NTFS DrvFs mount (/mnt/d/…).
    # DrvFs does not support utime()/utimes() — MSBuild's WriteStateFile writes
    # a *.Up2Date sentinel and calls SetLastWriteTime on it, which raises MSB3374.
    #
    # Fix: set CH_MANAGED_OBJ_DIR env var before invoking dotnet.
    # engine/scripting/managed/Directory.Build.props reads this var and sets
    #   BaseIntermediateOutputPath = $(CH_MANAGED_OBJ_DIR)/$(MSBuildProjectName)/
    # Each project (Chained.Managed, Chained.Managed.Generator, …) gets its own
    # named subfolder, so project.assets.json is never shared → no NETSDK1005.
    # On Windows CH_MANAGED_OBJ_DIR is never set, so behavior is unchanged.
    build_env: Optional[dict] = None

    if intermediate_dir is not None:
        # Explicit --intermediate-dir CLI override.
        intermediate_dir.mkdir(parents=True, exist_ok=True)
        build_env = {**os.environ, "CH_MANAGED_OBJ_DIR": str(intermediate_dir)}
    elif _is_ntfs_mount(project):
        managed_obj = output_dir.parent / "managed-obj"
        managed_obj.mkdir(parents=True, exist_ok=True)
        build_env = {**os.environ, "CH_MANAGED_OBJ_DIR": str(managed_obj)}

    if build_env is not None:
        # Restore all projects with the redirected obj path first so that
        # project.assets.json lands in the right location before build reads it.
        restore_cmd = [dotnet, "restore", str(project), "--force"]
        subprocess.run(restore_cmd, env=build_env, cwd=str(project.parent), check=True)
        command.append("--no-restore")

    if parallel:
        command.append("-m")

    subprocess.run(command, env=build_env, cwd=str(project.parent), check=True)

    if copy_coral:
        for name in CORAL_ARTIFACTS:
            if not (src := coral_dir / name).exists():
                raise FileNotFoundError(f"Missing Coral artifact: {src}")
            shutil.copy2(src, output_dir / name)


def main() -> None:
    parser = argparse.ArgumentParser(description="Build managed C# projects")
    parser.add_argument("--project", required=True, help="Path to .csproj")
    parser.add_argument("--output", required=True, help="Output directory for DLLs")
    parser.add_argument("--coral-dir", required=True, help="Directory containing Coral.Managed artifacts")
    parser.add_argument("--configuration", default="Debug", help="Build configuration (Debug/Release)")
    parser.add_argument("--copy-coral", action="store_true", help="Copy Coral.Managed artifacts to output")
    parser.add_argument("--write-props", action="store_true", help="Write ManagedDependencies.props")
    parser.add_argument("--parallel", action="store_true", help="Use parallel MSBuild")
    parser.add_argument(
        "--intermediate-dir",
        default=None,
        help="Override MSBuild BaseIntermediateOutputPath (useful on WSL2 to keep obj/ off NTFS mounts)",
    )

    args = parser.parse_args()

    build_managed(
        Path(args.project).resolve(),
        Path(args.output).resolve(),
        Path(args.coral_dir).resolve(),
        args.configuration,
        args.copy_coral,
        args.parallel,
        args.write_props,
        Path(args.intermediate_dir).resolve() if args.intermediate_dir else None,
    )


if __name__ == "__main__":
    main()
