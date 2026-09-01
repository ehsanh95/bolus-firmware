#!/usr/bin/env python3
"""Idempotently add the LoRaWAN vendor sources to the CubeIDE Debug build.

Why a script instead of committing generated .cproject edits immediately:
- .cproject is fragile and CubeIDE-version-specific.
- the project had a known-good Phase-4/5 build configuration we want to preserve.
- this patch is small, deterministic and easy to revert with `git checkout -- .cproject`.

Run once from the repository root, then Refresh/Clean/Build in STM32CubeIDE.
"""
from pathlib import Path

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

SOURCE_ANCHOR = (
    '\t\t\t\t\t\t<entry flags="VALUE_WORKSPACE_PATH|RESOLVED" '
    'kind="sourcePath" name="Drivers"/>'
)

SOURCE_LINES = [
    'ThirdParty/I-CUBE-LRWAN/Middlewares/Third_Party/LoRaWAN/Mac',
    'ThirdParty/I-CUBE-LRWAN/Middlewares/Third_Party/LoRaWAN/Crypto',
    'ThirdParty/I-CUBE-LRWAN/Middlewares/Third_Party/LoRaWAN/Utilities',
]

EXPECTED_BUILD_SYSTEM = 'org.eclipse.cdt.managedbuilder.core.configurationDataProvider'


def fail(message: str) -> None:
    raise SystemExit(f'[LoRaWAN CubeIDE patch] ERROR: {message}')


def main() -> None:
    if not CPROJECT.exists():
        fail('run this script from the bolus-firmware repository root')

    text = CPROJECT.read_text(encoding='utf-8')

    # Guard against accidentally patching a damaged/foreign CubeIDE project file.
    if EXPECTED_BUILD_SYSTEM not in text:
        fail('known CubeIDE buildSystemId not found; refusing to rewrite .cproject')

    changed = False

    missing_includes = [p for p in INCLUDE_LINES if p not in text]
    if missing_includes:
        if INCLUDE_ANCHOR not in text:
            fail('Debug RFM95W include-path anchor not found')
        insert = ''.join(
            '\n\t\t\t\t\t\t\t\t\t<listOptionValue builtIn="false" '
            f'value="&quot;{path}&quot;"/>'
            for path in missing_includes
        )
        text = text.replace(INCLUDE_ANCHOR, INCLUDE_ANCHOR + insert, 1)
        changed = True

    missing_sources = [p for p in SOURCE_LINES if f'name="{p}"' not in text]
    if missing_sources:
        # Replace only the first Drivers source entry: that is the Debug configuration.
        if SOURCE_ANCHOR not in text:
            fail('Debug sourceEntries anchor not found')
        insert = ''.join(
            '\n\t\t\t\t\t\t<entry flags="VALUE_WORKSPACE_PATH" '
            f'kind="sourcePath" name="{path}"/>'
            for path in missing_sources
        )
        text = text.replace(SOURCE_ANCHOR, SOURCE_ANCHOR + insert, 1)
        changed = True

    if changed:
        CPROJECT.write_text(text, encoding='utf-8', newline='')
        print('[LoRaWAN CubeIDE patch] .cproject updated.')
        print('[LoRaWAN CubeIDE patch] Next: Refresh Project -> Clean Project -> Build Debug.')
    else:
        print('[LoRaWAN CubeIDE patch] already enabled; no changes made.')


if __name__ == '__main__':
    main()
