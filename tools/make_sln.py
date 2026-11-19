import re

src = open('Smtm.sln', encoding='cp949', errors='replace').read()

# upgrade header to VS2022
src = src.replace('Microsoft Visual Studio Solution File, Format Version 10.00',
                  'Microsoft Visual Studio Solution File, Format Version 12.00')
src = src.replace('# Visual Studio 2008', '# Visual Studio Version 17')

# point project refs to .vcxproj
src = re.sub(r'\.vcproj"', '.vcxproj"', src)

# strip SourceSafe section
src = re.sub(r'\tGlobalSection\(SourceCodeControl\) = preSolution.*?\tEndGlobalSection\r?\n',
             '', src, flags=re.S)

# --- drop projects that cannot build outside their original environment ---
DROP = {
    'xskinexp',   # 3ds Max exporter plugin - needs 3ds Max SDK, impossible on CI
}
for name in DROP:
    # remove the Project ... EndProject block (match up to the line with EndProject)
    pat = re.compile(r'Project\("\{8BC9CEB8[^"]*\}"\) = "[^"]*", "' + name +
                     r'\\[^"]*"[^\n]*\n(?:.*?\n)*?EndProject\r?\n', re.S)
    m = pat.search(src)
    if m:
        guid_m = re.search(r'"(\{[0-9A-Fa-f\-]+\})"\s*$', m.group(0).splitlines()[0])
        src = src.replace(m.group(0), '')
        if guid_m:
            g = re.escape(guid_m.group(1))
            # remove its config lines
            src = re.sub(r'\t\t' + g + r'\.[^\n]*\n', '', src)
            # remove dependency references to it
            src = re.sub(r'\t\t\t' + g + r' = ' + g + r'\r?\n', '', src)
        print('dropped', name)
    else:
        print('NOT FOUND in sln:', name)


# MfcExLib had no config mappings in the original sln (was built manually and
# shipped as _RLib\\MfcExLib.lib). Add mappings so CI builds it.
MFC_GUID = '{D94E8461-50B9-46AC-A08F-EFA6811CE2F5}'
if MFC_GUID + '.Release|Win32.ActiveCfg' not in src:
    marker = '\tGlobalSection(ProjectConfigurationPlatforms) = postSolution\n'
    assert marker in src
    inject = ''
    import re as _re
    cfgnames = sorted(set(_re.findall(r'\t\t\{[0-9A-Fa-f\-]+\}\.([^\.]+)\.ActiveCfg', src)))
    for cfg in cfgnames:
        inject += '\t\t%s.%s.ActiveCfg = Release|Win32\n' % (MFC_GUID, cfg)
        if cfg == 'Release|Win32':
            inject += '\t\t%s.%s.Build.0 = Release|Win32\n' % (MFC_GUID, cfg)
    src = src.replace(marker, marker + inject)
    print('added MfcExLib config mappings for', len(cfgnames), 'configs')

open('Smtm2022.sln', 'w', encoding='utf-8').write(src)
print('Smtm2022.sln written, projects:', src.count('Project("'))
