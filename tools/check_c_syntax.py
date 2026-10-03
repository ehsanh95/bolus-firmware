#!/usr/bin/env python3
"""Host syntax checks only; not an ARM build, linker check or hardware test."""
from pathlib import Path
import os
import subprocess
import xml.etree.ElementTree as ET

os.chdir(Path(__file__).resolve().parents[1])
root = ET.parse('.cproject')
includes = []
for node in root.findall('.//cconfiguration')[0].findall('.//option[@valueType="includePath"]/listOptionValue'):
    value = node.attrib['value'].strip('"').replace('${workspace_loc:/${ProjName}/', '').replace('}', '')
    if value.startswith('../'):
        value = value[3:]
    includes.append('-I' + value)
files = sorted(Path('App').rglob('*.c')) + sorted(Path('Core/Src').glob('*.c'))
failed = 0
for path in files:
    result = subprocess.run([os.environ.get('CC', 'gcc'), '-std=gnu11', '-fsyntax-only',
                             '-Werror=implicit-function-declaration', '-DUSE_HAL_DRIVER',
                             '-DSTM32L476xx', *includes, str(path)], capture_output=True, text=True)
    if result.returncode:
        failed += 1
        print(path, result.stderr)
print(f'Syntax checks: {len(files) - failed}/{len(files)} passed (host only)')
raise SystemExit(bool(failed))
