#!/usr/bin/env python3
"""
Generate C++ glue code from C# [NativeProperty] and [NativeCall] attributes.

Scans component wrapper files in engine/scripting/managed/src/Components/ and
produces three files in the output directory:
  - script_glue_generated.h          (declarations for NativeProperty getters/setters + NativeCall stubs)
  - script_glue_generated.cpp        (getter/setter implementations + NativeCall stub bodies)
  - script_glue_generated_reg.inl    (AddInternalCall registration snippet)

Usage:
    python tools/generate_glue.py --cs-dir engine/scripting/managed/src/Components \
                                  --output-dir engine/scripting/generated
"""

import argparse
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional


# ── Type mapping: C# type -> C++ type ──────────────────────────────────────────

CS_TO_CPP = {
    "float":   "float",
    "double":  "double",
    "int":     "int",
    "uint":    "uint32_t",
    "long":    "int64_t",
    "ulong":   "uint64_t",
    "byte":    "uint8_t",
    "bool":    "uint8_t",
    "char*":   "const Coral::UCChar*",
    "Vector2": "glm::vec2",
    "Vector3": "glm::vec3",
    "Vector4": "glm::vec4",
    "Vector2*":"glm::vec2*",
    "Vector3*":"glm::vec3*",
    "Vector4*":"glm::vec4*",
}

STRUCT_TYPES = {"Vector2", "Vector3", "Vector4"}
BOOL_TYPES = {"bool"}
STRING_TYPES = {"string"}


@dataclass
class NativeProperty:
    cs_name: str           # e.g. "MovementSpeed"
    cs_type: str           # e.g. "float", "Vector3", "bool", "string"
    getter: Optional[str]  # e.g. "PlayerComponent_GetMovementSpeed"
    setter: Optional[str]  # e.g. "PlayerComponent_SetMovementSpeed"
    cs_class: str = ""     # filled later, e.g. "PlayerComponent"

    @property
    def cpp_type(self) -> str:
        return CS_TO_CPP.get(self.cs_type, self.cs_type)

    @property
    def is_struct(self) -> bool:
        return self.cs_type in STRUCT_TYPES

    @property
    def is_bool(self) -> bool:
        return self.cs_type in BOOL_TYPES

    @property
    def is_string(self) -> bool:
        return self.cs_type in STRING_TYPES


@dataclass
class NativeCall:
    cs_class: str          # e.g. "AudioComponent"
    func_name: str         # e.g. "Audio_Play"
    return_type: str       # C# return type: "void", "bool", "float", etc.
    param_types: list[str] = field(default_factory=list)  # e.g. ["ulong", "float"]
    cs_class_full: str = "" # e.g. "Chained.AudioComponent"

    @property
    def cpp_return_type(self) -> str:
        return CS_TO_CPP.get(self.return_type, self.return_type)

    @property
    def cpp_params(self) -> str:
        parts = ["uint64_t entityID"]
        for pt in self.param_types[1:]:  # skip first "ulong" (entityID)
            cpp = CS_TO_CPP.get(pt, pt)
            if pt.endswith('*') and pt != 'char*':
                parts.append(f"{cpp} outVal")
            elif pt == "char*":
                parts.append(f"{cpp} value")
            else:
                parts.append(f"{cpp} value")
        return ", ".join(parts)


# ── Parsing ────────────────────────────────────────────────────────────────────

NATIVE_PROPERTY_RE = re.compile(
    r'\[NativeProperty\s*\(\s*"([^"]+)"\s*,\s*"([^"]+)"'
    r'(?:\s*,\s*(?:"([^"]*)"|null))?'
    r'(?:\s*,\s*(?:"([^"]*)"|null))?\s*\)\s*\]'
)

NATIVE_CALL_RE = re.compile(
    r'\[NativeCall\s*\(\s*"([^"]+)"\s*,\s*"([^"]+)"\s*,\s*"([^"]+)"'
    r'((?:\s*,\s*"([^"]*)")*)'
    r'\s*\)\s*\]'
)

NATIVE_CALL_PARAM_RE = re.compile(r'"([^"]*)"')

CLASS_RE = re.compile(
    r'(?:public|internal)\s+partial\s+(?:class|struct)\s+(\w+)'
)


def parse_cs_file(filepath: Path) -> tuple[list[NativeProperty], list[NativeCall]]:
    content = filepath.read_text(encoding="utf-8")
    properties = []
    calls = []

    # Find class name
    class_match = CLASS_RE.search(content)
    class_name = class_match.group(1) if class_match else filepath.stem

    # Parse [NativeProperty] attributes
    for m in NATIVE_PROPERTY_RE.finditer(content):
        prop_name = m.group(1)
        prop_type = m.group(2)
        getter = m.group(3) if m.group(3) and m.group(3) != "null" else None
        setter = m.group(4) if m.group(4) and m.group(4) != "null" else None

        # Skip empty strings
        getter = getter or None
        setter = setter or None

        properties.append(NativeProperty(
            cs_name=prop_name,
            cs_type=prop_type,
            getter=getter,
            setter=setter,
            cs_class=class_name,
        ))

    # Parse [NativeCall] attributes
    for m in NATIVE_CALL_RE.finditer(content):
        cs_class_full = m.group(1)  # e.g. "Chained.AudioComponent"
        func_name = m.group(2)
        return_type = m.group(3)
        # Extract all param types from the raw matched string
        raw_params = m.group(4) or ""
        param_types = NATIVE_CALL_PARAM_RE.findall(raw_params)
        # First param is always ulong (entityID) — included for completeness
        if not param_types:
            param_types = ["ulong"]

        # Extract short class name from full namespace
        short_class = cs_class_full.rsplit(".", 1)[-1] if "." in cs_class_full else cs_class_full

        calls.append(NativeCall(
            cs_class=short_class,
            func_name=func_name,
            return_type=return_type,
            param_types=param_types,
            cs_class_full=cs_class_full,
        ))

    return properties, calls


# ── C++ Code Generation: NativeProperty ────────────────────────────────────────

def generate_getter(prop: NativeProperty, ns: str) -> str:
    """Generate a single C++ getter function."""
    cpp_type = prop.cpp_type
    func_name = prop.getter

    if prop.is_struct:
        return f"""\
\tCH_SCRIPT_FUNC void {func_name}(uint64_t entityID, {cpp_type}* outVal)
\t{{
\t\tEntity entity = GetEntity(entityID);
\t\tif (entity && entity.HasComponent<{ns}::{prop.cs_class}>() && outVal)
\t\t{{
\t\t\t*outVal = entity.GetComponent<{ns}::{prop.cs_class}>().{prop.cs_name};
\t\t}}
\t\telse if (outVal)
\t\t{{
\t\t\t*outVal = {{}};
\t\t}}
\t}}"""

    elif prop.is_bool:
        return f"""\
\tCH_SCRIPT_FUNC uint8_t {func_name}(uint64_t entityID)
\t{{
\t\tEntity entity = GetEntity(entityID);
\t\treturn entity && entity.HasComponent<{ns}::{prop.cs_class}>()
\t\t\t   ? entity.GetComponent<{ns}::{prop.cs_class}>().{prop.cs_name}
\t\t\t   : false;
\t}}"""

    elif prop.is_string:
        return f"""\
\tconst Coral::UCChar* {func_name}(uint64_t entityID)
\t{{
\t\tEntity entity = GetEntity(entityID);
\t\tstd::string val = entity && entity.HasComponent<{ns}::{prop.cs_class}>()
\t\t\t\t\t\t  ? entity.GetComponent<{ns}::{prop.cs_class}>().{prop.cs_name}
\t\t\t\t\t\t  : "";
\t\treturn GlueStringPool::ReturnString(val);
\t}}"""

    else:
        default = "0" if cpp_type in ("float", "double", "int", "uint32_t", "int64_t", "uint64_t") else "0"
        return f"""\
\tCH_SCRIPT_FUNC {cpp_type} {func_name}(uint64_t entityID)
\t{{
\t\tEntity entity = GetEntity(entityID);
\t\treturn entity && entity.HasComponent<{ns}::{prop.cs_class}>()
\t\t\t   ? entity.GetComponent<{ns}::{prop.cs_class}>().{prop.cs_name}
\t\t\t   : {default};
\t}}"""


def generate_setter(prop: NativeProperty, ns: str) -> str:
    """Generate a single C++ setter function."""
    cpp_type = prop.cpp_type
    func_name = prop.setter

    if prop.is_struct:
        return f"""\
\tCH_SCRIPT_FUNC void {func_name}(uint64_t entityID, {cpp_type}* inVal)
\t{{
\t\tEntity entity = GetEntity(entityID);
\t\tif (entity && entity.HasComponent<{ns}::{prop.cs_class}>() && inVal)
\t\t{{
\t\t\tentity.GetComponent<{ns}::{prop.cs_class}>().{prop.cs_name} = *inVal;
\t\t}}
\t}}"""

    elif prop.is_bool:
        return f"""\
\tCH_SCRIPT_FUNC void {func_name}(uint64_t entityID, uint8_t value)
\t{{
\t\tEntity entity = GetEntity(entityID);
\t\tif (entity && entity.HasComponent<{ns}::{prop.cs_class}>())
\t\t{{
\t\t\tentity.GetComponent<{ns}::{prop.cs_class}>().{prop.cs_name} = value;
\t\t}}
\t}}"""

    elif prop.is_string:
        return f"""\
\tvoid {func_name}(uint64_t entityID, const Coral::UCChar* value)
\t{{
\t\tEntity entity = GetEntity(entityID);
\t\tif (entity && entity.HasComponent<{ns}::{prop.cs_class}>() && value)
\t\t{{
\t\t\tentity.GetComponent<{ns}::{prop.cs_class}>().{prop.cs_name} = ch_u16_to_string(value);
\t\t}}
\t}}"""

    else:
        return f"""\
\tCH_SCRIPT_FUNC void {func_name}(uint64_t entityID, {cpp_type} value)
\t{{
\t\tEntity entity = GetEntity(entityID);
\t\tif (entity && entity.HasComponent<{ns}::{prop.cs_class}>())
\t\t{{
\t\t\tentity.GetComponent<{ns}::{prop.cs_class}>().{prop.cs_name} = value;
\t\t}}
\t}}"""


# ── C++ Code Generation: NativeCall stubs ──────────────────────────────────────

def generate_call_stub(call: NativeCall, ns: str) -> str:
    """Generate a stub C++ function for a NativeCall.
    
    These are placeholders — the actual implementations should be hand-written
    in separate script_glue_*.cpp files and this stub will be overridden at link time.
    """
    func_name = call.func_name
    ret = call.cpp_return_type

    # Build param list: first is always uint64_t entityID
    params = ["uint64_t entityID"]
    for pt in call.param_types[1:]:  # skip first "ulong"
        cpp = CS_TO_CPP.get(pt, pt)
        if pt.endswith('*') and pt != 'char*':
            params.append(f"{cpp} outVal")
        else:
            params.append(f"{cpp} value")

    param_str = ", ".join(params)

    if ret == "void":
        body = f"\t// TODO: hand-write {func_name} implementation"
    else:
        default_val = "0" if ret in ("float", "double", "int", "uint32_t", "int64_t", "uint64_t") else "false" if ret == "uint8_t" else "{}"
        body = f"\t// TODO: hand-write {func_name} implementation\n\treturn {default_val};"

    return f"""\t// STUB — override in script_glue_*.cpp
\tCH_SCRIPT_FUNC {ret} {func_name}({param_str})
\t{{
{body}
\t}}"""


# ── C++ Code Generation: Header / Cpp / Registration ──────────────────────────

def prop_return_type(prop: NativeProperty) -> str:
    if prop.is_struct:
        return "void"
    if prop.is_bool:
        return "uint8_t"
    if prop.is_string:
        return "const Coral::UCChar*"
    return prop.cpp_type


def prop_getter_params(prop: NativeProperty) -> str:
    if prop.is_struct:
        return f"uint64_t entityID, {prop.cpp_type}* outVal"
    return "uint64_t entityID"


def prop_setter_params(prop: NativeProperty) -> str:
    if prop.is_struct:
        return f"uint64_t entityID, {prop.cpp_type}* inVal"
    if prop.is_string:
        return f"uint64_t entityID, const Coral::UCChar* value"
    return f"uint64_t entityID, {prop.cpp_type} value"


def generate_header(properties: list[NativeProperty], calls: list[NativeCall], ns: str) -> str:
    lines = [
        "// <auto-generated/>",
        "// Generated by tools/generate_glue.py — do not edit by hand.",
        "#ifndef SCRIPT_GLUE_GENERATED_H",
        "#define SCRIPT_GLUE_GENERATED_H",
        "",
        '#include "script_glue_internal.h"',
        '#include "engine/scene/components.h"',
        '#include "engine/scene/entity.h"',
        "",
        "namespace Chained",
        "{",
        "",
    ]

    # Group properties by class
    props_by_class: dict[str, list[NativeProperty]] = {}
    for p in properties:
        props_by_class.setdefault(p.cs_class, []).append(p)

    # Group calls by class
    calls_by_class: dict[str, list[NativeCall]] = {}
    for c in calls:
        calls_by_class.setdefault(c.cs_class, []).append(c)

    # Merge class lists
    all_classes = set(list(props_by_class.keys()) + list(calls_by_class.keys()))

    for class_name in sorted(all_classes):
        lines.append(f"\t// ── {class_name} ──────────────────────────────────────────────")
        for prop in props_by_class.get(class_name, []):
            if prop.getter:
                lines.append(f"\tCH_SCRIPT_FUNC {prop_return_type(prop)} {prop.getter}({prop_getter_params(prop)});")
            if prop.setter:
                lines.append(f"\tCH_SCRIPT_FUNC void {prop.setter}({prop_setter_params(prop)});")
        for call in calls_by_class.get(class_name, []):
            lines.append(f"\t// STUB — override in script_glue_*.cpp")
            lines.append(f"\tCH_SCRIPT_FUNC {call.cpp_return_type} {call.func_name}({call.cpp_params});")
        lines.append("")

    lines.append("} // namespace Chained")
    lines.append("#endif // SCRIPT_GLUE_GENERATED_H")
    return "\n".join(lines) + "\n"


def generate_cpp(properties: list[NativeProperty], calls: list[NativeCall], ns: str) -> str:
    lines = [
        "// <auto-generated/>",
        "// Generated by tools/generate_glue.py — do not edit by hand.",
        "",
        '#include "script_glue_generated.h"',
        "",
        "namespace Chained",
        "{",
        "",
    ]

    # Group properties by class
    props_by_class: dict[str, list[NativeProperty]] = {}
    for p in properties:
        props_by_class.setdefault(p.cs_class, []).append(p)

    # Group calls by class
    calls_by_class: dict[str, list[NativeCall]] = {}
    for c in calls:
        calls_by_class.setdefault(c.cs_class, []).append(c)

    all_classes = set(list(props_by_class.keys()) + list(calls_by_class.keys()))

    for class_name in sorted(all_classes):
        lines.append(f"\t// ── {class_name} ──────────────────────────────────────────────")
        for prop in props_by_class.get(class_name, []):
            if prop.getter:
                lines.append(generate_getter(prop, ns))
                lines.append("")
            if prop.setter:
                lines.append(generate_setter(prop, ns))
                lines.append("")
        for call in calls_by_class.get(class_name, []):
            lines.append(generate_call_stub(call, ns))
            lines.append("")

    lines.append("} // namespace Chained")
    return "\n".join(lines) + "\n"


def generate_registration(properties: list[NativeProperty], calls: list[NativeCall]) -> str:
    lines = [
        "// <auto-generated/>",
        "// Generated by tools/generate_glue.py — do not edit by hand.",
        "",
    ]

    # Group properties by class
    props_by_class: dict[str, list[NativeProperty]] = {}
    for p in properties:
        props_by_class.setdefault(p.cs_class, []).append(p)

    # Group calls by class
    calls_by_class: dict[str, list[NativeCall]] = {}
    for c in calls:
        calls_by_class.setdefault(c.cs_class, []).append(c)

    all_classes = set(list(props_by_class.keys()) + list(calls_by_class.keys()))

    for class_name in sorted(all_classes):
        lines.append(f"\t// ── {class_name} ──────────────────────────────────────────────")
        for prop in props_by_class.get(class_name, []):
            if prop.getter:
                lines.append(f'\tassembly.AddInternalCall("Chained.{class_name}", "{prop.getter}_Ptr",')
                lines.append(f'\t\t\t\t\t\t\t\t (void*)&{prop.getter});')
            if prop.setter:
                lines.append(f'\tassembly.AddInternalCall("Chained.{class_name}", "{prop.setter}_Ptr",')
                lines.append(f'\t\t\t\t\t\t\t\t (void*)&{prop.setter});')
        for call in calls_by_class.get(class_name, []):
            lines.append(f'\tassembly.AddInternalCall("Chained.{class_name}", "{call.func_name}_Ptr",')
            lines.append(f'\t\t\t\t\t\t\t\t (void*)&{call.func_name});')
        lines.append("")

    return "\n".join(lines) + "\n"


# ── Main ───────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="Generate C++ glue from C# NativeProperty/NativeCall attributes")
    parser.add_argument("--cs-dir", required=True, help="Directory containing C# component wrappers")
    parser.add_argument("--output-dir", required=True, help="Output directory for generated C++ files")
    parser.add_argument("--namespace", default="Chained", help="C++ namespace (default: Chained)")
    parser.add_argument("--classes", nargs="*", default=None,
                        help="Only process these C# class names (e.g. PlayerComponent SpawnComponent). "
                             "If omitted, processes all classes found.")
    args = parser.parse_args()

    cs_dir = Path(args.cs_dir)
    out_dir = Path(args.output_dir)
    ns = args.namespace

    if not cs_dir.exists():
        print(f"Error: C# directory not found: {cs_dir}", file=sys.stderr)
        sys.exit(1)

    out_dir.mkdir(parents=True, exist_ok=True)

    all_properties: list[NativeProperty] = []
    all_calls: list[NativeCall] = []

    for cs_file in sorted(cs_dir.glob("*.cs")):
        props, calls = parse_cs_file(cs_file)
        if args.classes:
            props = [p for p in props if p.cs_class in args.classes]
            calls = [c for c in calls if c.cs_class in args.classes]
        all_properties.extend(props)
        all_calls.extend(calls)

    if not all_properties and not all_calls:
        print("Warning: No [NativeProperty] or [NativeCall] attributes found.", file=sys.stderr)
        (out_dir / "script_glue_generated.h").write_text(
            "// <auto-generated/> — no attributes found\n", encoding="utf-8")
        (out_dir / "script_glue_generated.cpp").write_text(
            "// <auto-generated/> — no attributes found\n", encoding="utf-8")
        (out_dir / "script_glue_generated_reg.inl").write_text(
            "// <auto-generated/> — no attributes found\n", encoding="utf-8")
        return

    header = generate_header(all_properties, all_calls, ns)
    cpp = generate_cpp(all_properties, all_calls, ns)
    reg = generate_registration(all_properties, all_calls)

    (out_dir / "script_glue_generated.h").write_text(header, encoding="utf-8")
    (out_dir / "script_glue_generated.cpp").write_text(cpp, encoding="utf-8")
    (out_dir / "script_glue_generated_reg.inl").write_text(reg, encoding="utf-8")

    print(f"Generated glue for {len(all_properties)} properties + {len(all_calls)} native calls")
    print(f"  -> {out_dir / 'script_glue_generated.h'}")
    print(f"  -> {out_dir / 'script_glue_generated.cpp'}")
    print(f"  -> {out_dir / 'script_glue_generated_reg.inl'}")


if __name__ == "__main__":
    main()
