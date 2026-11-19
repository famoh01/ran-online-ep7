#!/usr/bin/env python3
"""Convert VS2003/2008 .vcproj (Visual C++ 7.10/9.0 XML) to VS2022 .vcxproj.

Usage: vcproj2vcxproj.py <input.vcproj> [-o output.vcxproj]

Handles: all configurations, ClCompile/Link/Lib settings, precompiled
headers (incl. per-file Create for stdafx.cpp), MFC/charset, file lists.
"""
import re
import sys
import argparse
import xml.etree.ElementTree as ET

# ---------- attribute mapping tables ----------
RUNTIME = {"0": "MultiThreaded", "1": "MultiThreadedDebug",
           "2": "MultiThreadedDLL", "3": "MultiThreadedDebugDLL"}
OPT = {"0": "Disabled", "1": "MinSpace", "2": "MaxSpeed", "3": "Full"}
PCH = {"0": "NotUsing", "1": "Use", "2": "Create"}
DBGFMT = {"1": "OldStyle", "2": "None", "3": "ProgramDatabase", "4": "EditAndContinue"}
EXC = {"0": "false", "1": "Async", "2": "Sync"}
SUBSYS = {"1": "Console", "2": "Windows", "3": "Native"}
CFGTYPE = {"1": "Application", "2": "DynamicLibrary", "4": "StaticLibrary", "10": "Utility"}
CHARSET = {"0": "NotSet", "1": "MultiByte", "2": "Unicode"}
USEMFC = {"0": "false", "1": "Static", "2": "Dynamic"}
BOOL = {"TRUE": "true", "FALSE": "false", "1": "true", "0": "false"}
BOOL12 = {"1": "false", "2": "true"}  # vcproj 1=off 2=on style
WARNLEVEL = {"0": "TurnOffAllWarnings", "1": "Level1", "2": "Level2",
             "3": "Level3", "4": "Level4", "5": "EnableAllWarnings"}
INLINE = {"0": "Default", "1": "OnlyExplicitInline", "2": "AnySuitable"}
FAVOR = {"0": "Neither", "1": "Speed", "2": "Size"}
RTC = {"0": "Default", "1": "StackFrameRuntimeCheck",
       "2": "UninitializedLocalUsageCheck", "3": "EnableFastChecks"}
ALIGN = {"1": "1Byte", "2": "2Bytes", "3": "4Bytes", "4": "8Bytes", "5": "16Bytes"}
CALLCONV = {"0": "Cdecl", "1": "FastCall", "2": "StdCall", "3": "VectorCall"}
BROWSE = {"0": "false", "1": "true", "2": "true"}
TARGETMACHINE = {"1": "MachineX86", "17": "MachineX64", "0": ""}

# ClCompile: vcproj attr -> (vcxproj element, map or None)
CL_ATTRS = {
    "AdditionalIncludeDirectories": ("AdditionalIncludeDirectories", None),
    "PreprocessorDefinitions": ("PreprocessorDefinitions", None),
    "RuntimeLibrary": ("RuntimeLibrary", RUNTIME),
    "Optimization": ("Optimization", OPT),
    "WarningLevel": ("WarningLevel", WARNLEVEL),
    "UsePrecompiledHeader": ("PrecompiledHeader", PCH),
    "PrecompiledHeaderThrough": ("PrecompiledHeaderFile", None),
    "DebugInformationFormat": ("DebugInformationFormat", DBGFMT),
    "ExceptionHandling": ("ExceptionHandling", EXC),
    "BufferSecurityCheck": ("BufferSecurityCheck", BOOL),
    "BasicRuntimeChecks": ("BasicRuntimeChecks", RTC),
    "MinimalRebuild": ("MinimalRebuild", BOOL),
    "StructMemberAlignment": ("StructMemberAlignment", ALIGN),
    "CallingConvention": ("CallingConvention", CALLCONV),
    "DisableSpecificWarnings": ("DisableSpecificWarnings", None),
    "ForcedIncludeFiles": ("ForcedIncludeFiles", None),
    "ObjectFile": ("ObjectFileName", None),
    "ProgramDataBaseFileName": ("ProgramDataBaseFileName", None),
    "BrowseInformation": ("BrowseInformation", BROWSE),
    "SuppressStartupBanner": ("SuppressStartupBanner", BOOL),
    "CompileAs": ("CompileAs", {"0": "Default", "1": "CompileAsC", "2": "CompileAsCpp"}),
    "InlineFunctionExpansion": ("InlineFunctionExpansion", INLINE),
    "FavorSizeOrSpeed": ("FavorSizeOrSpeed", FAVOR),
    "OmitFramePointers": ("OmitFramePointers", BOOL),
    "EnableFiberSafeOptimizations": ("EnableFiberSafeOptimizations", BOOL),
    "WholeProgramOptimization": ("WholeProgramOptimization", BOOL),
    "TreatWChar_tAsBuiltInType": ("TreatWChar_tAsBuiltInType", BOOL),
    "EnableRunTimeTypeInfo": ("RuntimeTypeInfo", BOOL),
    "StringPooling": ("StringPooling", BOOL),
    "EnableFunctionLevelLinking": ("FunctionLevelLinking", BOOL),
    "FloatingPointModel": ("FloatingPointModel", None),
}
LINK_ATTRS = {
    "AdditionalDependencies": ("AdditionalDependencies", None),
    "AdditionalLibraryDirectories": ("AdditionalLibraryDirectories", None),
    "SubSystem": ("SubSystem", SUBSYS),
    "IgnoreDefaultLibraryNames": ("IgnoreSpecificDefaultLibraries", None),
    "EntryPointSymbol": ("EntryPointSymbol", None),
    "OutputFile": ("OutputFile", None),
    "GenerateDebugInformation": ("GenerateDebugInformation", BOOL),
    "ProgramDatabaseFile": ("ProgramDatabaseFile", None),
    "RandomizedBaseAddress": ("RandomizedBaseAddress", BOOL),
    "DataExecutionPrevention": ("DataExecutionPrevention", BOOL),
    "FixedBaseAddress": ("FixedBaseAddress", BOOL12),
    "BaseAddress": ("BaseAddress", None),
    "ModuleDefinitionFile": ("ModuleDefinitionFile", None),
    "LargeAddressAware": ("LargeAddressAware", BOOL12),
    "DelayLoadDLLs": ("DelayLoadDLLs", None),
    "LinkIncremental": ("LinkIncremental", BOOL12),
    "SuppressStartupBanner": ("SuppressStartupBanner", BOOL),
    "TargetMachine": ("TargetMachine", TARGETMACHINE),
    "StackReserveSize": ("StackReserveSize", None),
    "StackCommitSize": ("StackCommitSize", None),
    "EnableCOMDATFolding": ("EnableCOMDATFolding", BOOL12),
    "OptimizeReferences": ("OptimizeReferences", BOOL12),
}
LIB_ATTRS = {
    "AdditionalDependencies": ("AdditionalDependencies", None),
    "AdditionalLibraryDirectories": ("AdditionalLibraryDirectories", None),
    "OutputFile": ("OutputFile", None),
    "SuppressStartupBanner": ("SuppressStartupBanner", BOOL),
    "IgnoreDefaultLibraryNames": ("IgnoreSpecificDefaultLibraries", None),
}


def read_vcproj(path):
    raw = open(path, "rb").read()
    for enc in ("cp949", "utf-8-sig", "utf-16"):
        try:
            return ET.fromstring(raw.decode(enc))
        except (UnicodeDecodeError, ValueError):
            continue
    raise RuntimeError(f"cannot decode {path}")


def norm_list(val):
    """vcproj lists may be space-separated (with quotes); MSBuild needs ';'."""
    if ";" in val:
        return val
    toks = re.findall(r'"[^"]*"|\S+', val)
    return ";".join(t.strip('"') for t in toks)


# legacy prebuilt libs shipped in the repo's =Library dir; bare references to
# them must be rewritten to full $(SolutionDir) paths for the linker
EQLIB_NAMES = {
    "fci.lib", "fdi.lib", "lzo2.lib", "ggsrvlib.lib", "ggsrvdll.lib",
    "npgamelib.lib", "npgamelib_mtdll.lib", "ahclientinterface.lib",
    "md_ggsrvlib.lib", "mt_ggsrvlib.lib", "ggsrvlib25_md.lib",
    "ggsrvlib25_mt.lib", "ijl15.lib",
}

# bundled DirectX Dec 2005 SDK import libs (the versions this code was built
# against); bare references get rewritten to $(SolutionDir)=DX_Lib\x86\...
# libs that must not be passed to the linker at all: d3dx8dt.lib is not
# shipped; strsafe.lib (old import lib) conflicts with modern SDK strsafe
# (source-level compat handled via ServerManager/strsafe.h shim).
DROP_LIB_NAMES = {"d3dx8dt.lib", "strsafe.lib"}

DXLIB_NAMES = {
    "d3d8.lib", "d3d9.lib", "d3dx9.lib", "d3dx9d.lib", "d3dxof.lib",
    "ddraw.lib", "dinput.lib", "dinput8.lib", "dplayx.lib", "dsetup.lib",
    "dsound.lib", "dxguid.lib", "dxtrans.lib", "dxerr8.lib", "dxerr9.lib",
}


def fix_eqlib_deps(val):
    out = []
    for t in val.split(";"):
        tt = t.strip().strip('"')
        if not tt:
            continue
        base = tt.replace("/", "\\").split("\\")[-1].lower()
        if "\\" not in tt and "$" not in tt:
            if base in DROP_LIB_NAMES:
                continue
            if base in EQLIB_NAMES:
                continue
            if False:
                pass
            if base in DXLIB_NAMES:
                out.append("$(SolutionDir)=DX_Lib\\x86\\" + tt)
                continue
        out.append(tt)
    return ";".join(out)


def map_attrs(tool_el, table, out):
    """out: dict elem_name -> value (first non-empty wins, lists appended with ';')."""
    for src_attr, (elem, mapping) in table.items():
        if src_attr in tool_el.attrib:
            val = tool_el.attrib[src_attr].strip()
            if val == "":
                continue
            if mapping:
                val = mapping.get(val, val)
                if val == "":
                    # mapped-to-empty means "not set" (e.g. TargetMachine=0)
                    if elem in out:
                        del out[elem]
                    continue
            elif src_attr in ("AdditionalDependencies", "AdditionalLibraryDirectories",
                              "AdditionalIncludeDirectories", "IgnoreDefaultLibraryNames",
                              "DelayLoadDLLs", "ForcedIncludeFiles"):
                val = norm_list(val)
            if elem in out and out[elem]:
                out[elem] = out[elem] + ";" + val
            else:
                out[elem] = val
    return out


def collect_files(files_el):
    """Return (cpp, hdr, rc, other, pch_create_files, excluded_files) lists of relative paths."""
    cpp, hdr, rc, other, pch_create, idl = [], [], [], [], [], []
    excluded = set()
    pch_notusing = set()

    def walk(parent):
        for f in parent.findall("File"):
            rel = f.attrib.get("RelativePath", "").strip()
            if not rel:
                continue
            # VS2003 projects often keep dead code in the tree but
            # excluded from every configuration (e.g. legacy DX8 mesh
            # files). Respect that or modern MSVC compiles dead code
            # that collides with its replacements.
            all_cfg_excluded = True
            has_cfg = False
            for fc in f.findall("FileConfiguration"):
                has_cfg = True
                if fc.attrib.get("ExcludedFromBuild", "").lower() != "true":
                    all_cfg_excluded = False
            if has_cfg and all_cfg_excluded:
                excluded.add(rel)
            for fc in f.findall("FileConfiguration"):
                for t in fc.findall("Tool"):
                    if t.attrib.get("Name") == "VCCLCompilerTool" and \
                       t.attrib.get("UsePrecompiledHeader") == "2":
                        pch_create.append(rel)
                    if t.attrib.get("Name") == "VCCLCompilerTool" and \
                       t.attrib.get("UsePrecompiledHeader") == "0":
                        pch_notusing.add(rel)
            low = rel.lower()
            if low.endswith((".c", ".cpp", ".cxx")):
                cpp.append(rel)
            elif low.endswith((".h", ".hpp", ".inl", ".hxx")):
                hdr.append(rel)
            elif low.endswith(".rc"):
                rc.append(rel)
            elif low.endswith(".idl"):
                other.append(rel) if False else idl.append(rel)
            else:
                other.append(rel)
        for sub in parent.findall("Filter"):
            walk(sub)

    walk(files_el)
    return cpp, hdr, rc, other, pch_create, excluded, pch_notusing, idl


def split_cfg_name(name):
    # "Debug|Win32" -> ("Debug", "Win32")
    parts = name.split("|")
    return parts[0], parts[1] if len(parts) > 1 else "Win32"


def convert(src, dst):
    root = read_vcproj(src)
    proj_name = root.attrib.get("Name", "project")
    guid = root.attrib.get("ProjectGUID", "{00000000-0000-0000-0000-000000000000}").upper()

    cfgs_el = root.find("Configurations")
    files_el = root.find("Files")
    configs = cfgs_el.findall("Configuration") if cfgs_el is not None else []
    cpp, hdr, rc, other, pch_create_files, idl_files = ([], [], [], [], [], [])
    excluded_files = set()
    pch_notusing_files = set()
    if files_el is not None:
        cpp, hdr, rc, other, pch_create_files, excluded_files, pch_notusing_files, idl_files = collect_files(files_el)

    NS = "http://schemas.microsoft.com/developer/msbuild/2003"
    ET.register_namespace("", NS)
    proj = ET.Element("Project", {
        "xmlns": NS,
        "DefaultTargets": "Build",
        "ToolsVersion": "4.0",
    })

    # --- ProjectConfigurations ---
    ig = ET.SubElement(proj, "ItemGroup", {"Label": "ProjectConfigurations"})
    seen = set()
    for c in configs:
        cfg, plat = split_cfg_name(c.attrib["Name"])
        if (cfg, plat) in seen:
            continue
        seen.add((cfg, plat))
        pc = ET.SubElement(ig, "ProjectConfiguration", {"Include": f"{cfg}|{plat}"})
        ET.SubElement(pc, "Configuration").text = cfg
        ET.SubElement(pc, "Platform").text = plat

    # --- Globals ---
    pg = ET.SubElement(proj, "PropertyGroup", {"Label": "Globals"})
    ET.SubElement(pg, "ProjectGuid").text = guid
    ET.SubElement(pg, "RootNamespace").text = proj_name.replace(" ", "_")
    ET.SubElement(pg, "Keyword").text = "Win32Proj"
    ET.SubElement(pg, "WindowsTargetPlatformVersion").text = "10.0"

    ET.SubElement(proj, "Import", {"Project": "$(VCTargetsPath)\\Microsoft.Cpp.Default.props"})

    # --- per-config PropertyGroup ---
    for c in configs:
        cfg, plat = split_cfg_name(c.attrib["Name"])
        cond = f"'$(Configuration)|$(Platform)'=='{cfg}|{plat}'"
        g = ET.SubElement(proj, "PropertyGroup", {"Condition": cond, "Label": "Configuration"})
        ET.SubElement(g, "ConfigurationType").text = CFGTYPE.get(
            c.attrib.get("ConfigurationType", "1"), "Application")
        ET.SubElement(g, "PlatformToolset").text = "$(DefaultPlatformToolset)"
        mfc = c.attrib.get("UseOfMFC")
        if mfc:
            ET.SubElement(g, "UseOfMfc").text = USEMFC.get(mfc, "false")
        cs = c.attrib.get("CharacterSet")
        if cs:
            # legacy code is char-based (MBCS); force it regardless of source
            ET.SubElement(g, "CharacterSet").text = "MultiByte"
        od = c.attrib.get("OutputDirectory")
        if od:
            ET.SubElement(g, "OutDir").text = od.replace("/", "\\").rstrip("\\") + "\\"
        idir = c.attrib.get("IntermediateDirectory")
        if idir:
            ET.SubElement(g, "IntDir").text = idir.replace("/", "\\").rstrip("\\") + "\\"
        # old code: keep permissive
        ET.SubElement(g, "_ProjectFileVersion").text = "17.0.0.0"

    ET.SubElement(proj, "Import", {"Project": "$(VCTargetsPath)\\Microsoft.Cpp.props"})

    # --- per-config ItemDefinitionGroup ---
    for c in configs:
        cfg, plat = split_cfg_name(c.attrib["Name"])
        cond = f"'$(Configuration)|$(Platform)'=='{cfg}|{plat}'"
        idg = ET.SubElement(proj, "ItemDefinitionGroup", {"Condition": cond})

        cl_opts, link_opts, lib_opts, post_cmd = {}, {}, {}, None
        for tool in c.findall("Tool"):
            tn = tool.attrib.get("Name")
            if tn == "VCCLCompilerTool":
                map_attrs(tool, CL_ATTRS, cl_opts)
            elif tn == "VCLinkerTool":
                map_attrs(tool, LINK_ATTRS, link_opts)
            elif tn == "VCLibrarianTool":
                map_attrs(tool, LIB_ATTRS, lib_opts)
            elif tn == "VCPostBuildEventTool":
                pc = tool.attrib.get("CommandLine", "").strip()
                # regsvr32 of the just-built OCX cannot work on CI runners
                if "regsvr32" not in pc.lower():
                    post_cmd = pc

        # strip legacy WINVER macros (set globally via Directory.Build.props)
        if "PreprocessorDefinitions" in cl_opts:
            defs = [d for d in cl_opts["PreprocessorDefinitions"].split(";")
                    if d and not d.startswith(("WINVER=", "_WIN32_WINNT=", "NTDDI_VERSION="))]
            cl_opts["PreprocessorDefinitions"] = ";".join(defs)

        # VS2003-style project-wide PCH Create -> modern Use + stdafx.cpp Create
        if cl_opts.get("PrecompiledHeader") == "Create":
            cl_opts["PrecompiledHeader"] = "Use"
            cl_opts.setdefault("PrecompiledHeaderFile", "stdafx.h")
            for f in cpp:
                if f.replace("\\", "/").rsplit("/", 1)[-1].lower() == "stdafx.cpp":
                    if f not in pch_create_files:
                        pch_create_files.append(f)

        cl = ET.SubElement(idg, "ClCompile")
        for k, v in cl_opts.items():
            if k == "AdditionalIncludeDirectories":
                v = v + ";%(AdditionalIncludeDirectories)"
            elif k == "PreprocessorDefinitions":
                v = v + ";%(PreprocessorDefinitions)"
            elif k == "DisableSpecificWarnings":
                v = v + ";%(DisableSpecificWarnings)"
            ET.SubElement(cl, k).text = v
        # VS2022 defaults that old code needs
        ET.SubElement(cl, "LanguageStandard").text = "stdcpp14"
        ET.SubElement(cl, "ConformanceMode").text = "false"

        if CFGTYPE.get(c.attrib.get("ConfigurationType", "1")) == "StaticLibrary":
            lib = ET.SubElement(idg, "Lib")
            for k, v in lib_opts.items():
                ET.SubElement(lib, k).text = v
        else:
            lk = ET.SubElement(idg, "Link")
            for k, v in link_opts.items():
                if k == "AdditionalDependencies":
                    v = fix_eqlib_deps(v) + ";%(AdditionalDependencies)"
                elif k == "AdditionalLibraryDirectories":
                    v = v + ";%(AdditionalLibraryDirectories)"
                ET.SubElement(lk, k).text = v

        if post_cmd:
            pb = ET.SubElement(idg, "PostBuildEvent")
            ET.SubElement(pb, "Command").text = post_cmd

        # projects with .idl: RC must find the MIDL-generated .tlb
        if idl_files:
            rcg = ET.SubElement(idg, "ResourceCompile")
            ET.SubElement(rcg, "AdditionalIncludeDirectories").text = \
                "$(ProjectDir);$(IntDir);%(AdditionalIncludeDirectories)"

        # per-file PCH create handled in file ItemGroups below

    # --- files ---
    def add_group(tag, files):
        if not files:
            return
        g = ET.SubElement(proj, "ItemGroup")
        for f in sorted(set(files)):
            el = ET.SubElement(g, tag, {"Include": f})
            if f in excluded_files:
                ET.SubElement(el, "ExcludedFromBuild").text = "true"
            if tag == "ClCompile" and f in pch_notusing_files:
                ET.SubElement(el, "PrecompiledHeader").text = "NotUsing"
            # stdafx.cpp style: per-file PCH create
            if tag == "ClCompile" and f in pch_create_files:
                for c in configs:
                    cfg, plat = split_cfg_name(c.attrib["Name"])
                    ET.SubElement(el, "PrecompiledHeader", {
                        "Condition": f"'$(Configuration)|$(Platform)'=='{cfg}|{plat}'"
                    }).text = "Create"

    add_group("ClCompile", cpp)
    add_group("ClInclude", hdr)
    add_group("ResourceCompile", rc)
    # .idl -> MIDL; the typelib must land next to the .rc that references it
    if idl_files:
        g = ET.SubElement(proj, "ItemGroup")
        for f in sorted(set(idl_files)):
            el = ET.SubElement(g, "Midl", {"Include": f})
            ET.SubElement(el, "OutputDirectory").text = "$(ProjectDir)"
            base = f.replace("/", "\\").split("\\")[-1].rsplit(".", 1)[0]
            ET.SubElement(el, "TypeLibraryName").text = "$(ProjectDir)" + base + ".tlb"
    add_group("None", other)

    ET.SubElement(proj, "Import", {"Project": "$(VCTargetsPath)\\Microsoft.Cpp.targets"})

    tree = ET.ElementTree(proj)
    ET.indent(tree, space="  ")
    tree.write(dst, encoding="utf-8", xml_declaration=True)
    return proj_name, len(configs), len(cpp)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("src")
    ap.add_argument("-o", "--out")
    a = ap.parse_args()
    dst = a.out or a.src.rsplit(".", 1)[0] + ".vcxproj"
    name, ncfg, nfiles = convert(a.src, dst)
    print(f"OK {a.src} -> {dst}  [{name}] {ncfg} configs, {nfiles} source files")


if __name__ == "__main__":
    main()
