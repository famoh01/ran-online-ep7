#!/usr/bin/env python3
"""Parse original VS2008 Smtm.sln ProjectDependencies and inject matching
<ProjectReference> entries into the converted .vcxproj files so MSBuild
auto-links dependent static libs (VS2003 did this implicitly)."""
import io, os, re, sys

ROOT = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(ROOT, "ran-src", "ran-online-ep7-main")
SLN = os.path.join(SRC, "Smtm.sln")

def parse_sln(path):
    s = io.open(path, encoding="cp949", errors="replace").read()
    projs = {}   # guid -> (name, relpath)
    deps = {}    # guid -> [dep guids]
    for m in re.finditer(
            r'Project\("[^"]+"\) = "([^"]+)", "([^"]+\.vcproj)", "(\{[^}]+\})"(.*?)\nEndProject\n',
            s, re.S):
        name, rel, guid = m.group(1), m.group(2), m.group(3).upper()
        projs[guid] = (name, rel)
        body = m.group(4)
        dm = re.search(r'ProjectSection\(ProjectDependencies\)(.*?)EndProjectSection', body, re.S)
        if dm:
            deps[guid] = sorted(set(g.upper() for g in
                                    re.findall(r'(\{[0-9A-Fa-f\-]+\}) = \{', dm.group(1))))
    return projs, deps

def main():
    projs, deps = parse_sln(SLN)
    print(f"parsed {len(projs)} projects, {len(deps)} with dependencies")
    changed = 0
    for guid, depguids in deps.items():
        name, rel = projs[guid]
        rel = rel.replace("\\", os.sep)
        vcx = os.path.join(SRC, rel[:-7] + ".vcxproj")  # .vcproj -> .vcxproj
        if not os.path.exists(vcx):
            print("  missing:", vcx); continue
        # filter to deps that exist as converted vcxproj
        refs = []
        for dg in depguids:
            if dg not in projs:
                continue
            dname, drel = projs[dg]
            drel2 = drel.replace("\\", os.sep)
            dpath = os.path.join(SRC, drel2[:-7] + ".vcxproj")
            if os.path.exists(dpath):
                refs.append((drel[:-7] + ".vcxproj", dg))  # keep backslashes for MSBuild
        if not refs:
            continue
        s = io.open(vcx, encoding="utf-8").read()
        # drop any previously injected group
        s = re.sub(r'\s*<ItemGroup>\s*<!-- injected project references -->.*?</ItemGroup>', '', s, flags=re.S)
        items = ['  <ItemGroup>\n    <!-- injected project references -->']
        for drel, dg in refs:
            extra = ''
            # MfcExLib is build-order only: it outputs to $(SolutionDir)_RLib\
            # and consumers link that path explicitly; auto-linking the project
            # output would reference the wrong path.
            if dg.upper() == '{D94E8461-50B9-46AC-A08F-EFA6811CE2F5}':
                extra = '\n      <LinkLibraryDependencies>false</LinkLibraryDependencies>'
            items.append(f'    <ProjectReference Include="..\\{drel}">\n'
                         f'      <Project>{dg}</Project>{extra}\n    </ProjectReference>')
        items.append('  </ItemGroup>')
        block = "\n".join(items) + "\n"
        marker = '  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.targets" />'
        assert marker in s, vcx
        s = s.replace(marker, block + marker, 1)
        io.open(vcx, "w", encoding="utf-8").write(s)
        changed += 1
        print(f"  {name}: +{len(refs)} refs")
    print("projects patched:", changed)

if __name__ == "__main__":
    main()
