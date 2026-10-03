#!/usr/bin/env python3
"""Idempotently configure the CubeIDE Debug and Release builds for the staged LoRaWAN stack.

This script deliberately adds only the LoRaWAN middleware source roots required by
Bolus. It also removes stale/broad ThirdParty source entries which can make
STM32CubeIDE recursively compile the complete I-CUBE-LRWAN package (reference
projects, MDK startup files, ST utility templates, BSP examples, etc.).

Why a script instead of committing generated .cproject edits directly:
- .cproject is fragile and CubeIDE-version-specific.
- the project had a known-good Phase-4/5 build configuration we want to preserve.
- this patch is deterministic and easy to revert with `git checkout -- .cproject`.

Run from the repository root, then Refresh/Clean/Build Debug/Release in STM32CubeIDE.
"""
from pathlib import Path
import re

CPROJECT = Path('.cproject')

INCLUDE_ANCHOR = (
    '\t\t\t\t\t\t\t\t\t<listOptionValue builtIn="false" '
    'value="&quot;${workspace_loc:/${ProjName}/App/Drivers/RF/RFM95W}&quot;"/>'
)

INCLUDE_LINES = [
    '${workspace_loc:/${ProjName}/ThirdParty/I-CUBE-LRWAN/Middlewares/Third_Party/LoRaWAN/Mac}',
    '${workspace_loc:/${ProjName}/ThirdParty/I-CUBE-LRWAN/Middlewares/Third_Party/LoRaWAN/Mac/Region}',
    '${workspace_loc:/${ProjName}/ThirdParty/I-CUBE-LRWAN/Middlewares/Third_Party/LoRaWAN/Crypto}',
    '${workspace_loc:/${ProjName}/ThirdParty/I-CUBE-LRWAN/Middlewares/Third_Party/LoRaWAN/Utilities}',
    '${workspace_loc:/${ProjName}/ThirdParty/I-CUBE-LRWAN/Reference/NUCLEO-L476RG/LoRaWAN/LoRaWAN_End_Node/LoRaWAN/App}',
]

# IMPORTANT: these are middleware source roots, not the package-level
# ThirdParty/I-CUBE-LRWAN/Utilities and not any Reference project folder.
SOURCE_LINES = [
    'ThirdParty/I-CUBE-LRWAN/Middlewares/Third_Party/LoRaWAN/Mac',
    'ThirdParty/I-CUBE-LRWAN/Middlewares/Third_Party/LoRaWAN/Crypto',
    'ThirdParty/I-CUBE-LRWAN/Middlewares/Third_Party/LoRaWAN/Utilities',
]

EXPECTED_BUILD_SYSTEM = 'org.eclipse.cdt.managedbuilder.core.configurationDataProvider'


def fail(message: str) -> None:
    raise SystemExit(f'[LoRaWAN CubeIDE patch] ERROR: {message}')


def _sanitize_debug_source_entries(text: str) -> tuple[str, int, bool]:
    """Replace Debug ThirdParty source entries with the exact Bolus allow-list.

    The first <sourceEntries> block in this project is the Debug configuration.
    Non-ThirdParty entries (App/Core/Drivers and any future project sources) are
    preserved exactly. Every existing ThirdParty sourcePath is removed first so
    stale entries such as `ThirdParty`, `ThirdParty/I-CUBE-LRWAN`, `Reference/...`
    or package-level `Utilities/...` cannot leak into the managed build.
    """
    match = re.search(r'<sourceEntries>(.*?)</sourceEntries>', text, flags=re.DOTALL)
    if match is None:
        fail('Debug sourceEntries block not found')

    body = match.group(1)
    entry_re = re.compile(r'\n\s*<entry\b[^>]*/>')
    removed = 0
    kept_parts = []
    cursor = 0

    for entry_match in entry_re.finditer(body):
        kept_parts.append(body[cursor:entry_match.start()])
        entry = entry_match.group(0)
        name_match = re.search(r'\bname="([^"]+)"', entry)
        name = name_match.group(1) if name_match else ''
        if name == 'ThirdParty' or name.startswith('ThirdParty/'):
            removed += 1
        else:
            kept_parts.append(entry)
        cursor = entry_match.end()
    kept_parts.append(body[cursor:])
    clean_body = ''.join(kept_parts)

    indent = '\n\t\t\t\t\t\t'
    allowed_entries = ''.join(
        indent + '<entry flags="VALUE_WORKSPACE_PATH" kind="sourcePath" '
        f'name="{path}"/>'
        for path in SOURCE_LINES
    )

    # Put the allow-listed middleware entries immediately before the closing tag.
    new_body = clean_body.rstrip() + allowed_entries + '\n\t\t\t\t\t'
    new_block = '<sourceEntries>' + new_body + '</sourceEntries>'
    changed = new_block != match.group(0)
    return text[:match.start()] + new_block + text[match.end():], removed, changed


def main() -> None:
    if not CPROJECT.exists():
        fail('run from the repository root')
    text = CPROJECT.read_text(encoding='utf-8')
    if EXPECTED_BUILD_SYSTEM not in text:
        fail('unknown build system')
    blocks = list(re.finditer(r'<cconfiguration\b.*?</cconfiguration>', text, re.DOTALL))
    if len(blocks) != 2:
        fail('expected Debug and Release configurations')
    # Use Debug's complete application include set for both configurations.
    include_re = r'(<option\b[^>]*valueType="includePath"[^>]*>)(.*?)(</option>)'
    debug_includes = re.search(include_re, blocks[0].group(), re.DOTALL)
    if debug_includes is None:
        fail('Debug include option missing')
    values = re.findall(r'<listOptionValue[^>]*/>', debug_includes.group(2))
    for path in INCLUDE_LINES:
        entry = f'<listOptionValue builtIn="false" value="&quot;{path}&quot;"/>'
        if entry not in values:
            values.append(entry)
    include_body = ''.join('\n\t\t\t\t\t\t\t\t\t' + v for v in values) + '\n\t\t\t\t\t\t\t\t'
    for block in reversed(blocks):
        old = block.group()
        new, count = re.subn(include_re, lambda m: m[1] + include_body + m[3], old, flags=re.DOTALL)
        if count != 1:
            fail('expected one C include-path option per configuration')
        new, _, _ = _sanitize_debug_source_entries(new)
        if not re.search(r'kind="sourcePath" name="App"', new):
            new = new.replace('<sourceEntries>', '<sourceEntries>\n\t\t\t\t\t\t<entry flags="VALUE_WORKSPACE_PATH" kind="sourcePath" name="App"/>')
        text = text[:block.start()] + new + text[block.end():]
    if text != CPROJECT.read_text(encoding='utf-8'):
        CPROJECT.write_text(text, encoding='utf-8', newline='')
        print('[LoRaWAN CubeIDE patch] Debug and Release updated.')
    else:
        print('[LoRaWAN CubeIDE patch] already configured.')


if __name__ == '__main__':
    main()
