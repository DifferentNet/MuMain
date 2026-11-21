#!/usr/bin/env python3
"""Every translation in src/Localization has the printf specifiers of its English text, in the same order.

    python tests/text/test_resx_format_specifiers.py <repository root>

A translation fed to mu_swprintf with other specifiers than the English one reads the wrong arguments: a
missing %d shifts every later one, a %d where the code passes a string prints garbage or crashes. The
comparison is on what the game gets: in the wide groups (src/CMakeLists.txt --wide-groups) ResxGen turns a
plain %s or %S into %ls (tools/ResxGen/Naming.cs NormalizePrintfSpecifiersForWide), so there %s and %ls are
the same. "%%" is a percent sign, and so is a "%" before a space ("5% increase", "120 % of"): no text of the
game uses the space flag.
"""
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[2]
LOCALIZATION = ROOT / "src/Localization"
SRC_CMAKE = ROOT / "src/CMakeLists.txt"
DEFAULT_LOCALE = "en"

SPECIFIER = re.compile(r"%(?:\d+\$)?[-+#0]*(?:\d+|\*)?(?:\.(?:\d+|\*))?(hh|h|ll|l|L|I64|I32|j|z|t|w)?([diouxXeEfFgGaAcCspnS])")
WIDE_GROUPS = re.compile(r"--wide-groups\s+(\S+)")


def wide_groups():
    match = WIDE_GROUPS.search(SRC_CMAKE.read_text(encoding="utf-8"))
    if not match:
        raise SystemExit(f"no --wide-groups in {SRC_CMAKE}")
    return set(match.group(1).split(","))


def specifiers(value, wide):
    found = []
    for match in SPECIFIER.finditer(value.replace("%%", "")):
        length, conversion = match.group(1), match.group(2)
        if wide and conversion in "sS" and not length:
            found.append("%ls")
        else:
            found.append(match.group(0))
    return found


def values(path):
    """{key: value} of a .resx file."""
    entries = {}
    for data in ET.parse(path).getroot().iter("data"):
        value = data.find("value")
        entries[data.get("name")] = (value.text or "") if value is not None else ""
    return entries


def main():
    # Keys and values are not ASCII; a Windows console would refuse to print them.
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    wide = wide_groups()
    files = sorted(LOCALIZATION.glob("*.resx"))
    if not files:
        raise SystemExit(f"no .resx files in {LOCALIZATION}")
    groups = {}
    for path in files:
        group, locale = path.name[:-len(".resx")].split(".", 1)
        groups.setdefault(group, {})[locale] = path
    mismatches = []
    checked = 0
    for group, locales in sorted(groups.items()):
        if DEFAULT_LOCALE not in locales:
            continue
        is_wide = group in wide
        english = values(locales[DEFAULT_LOCALE])
        for locale, path in sorted(locales.items()):
            if locale == DEFAULT_LOCALE:
                continue
            for key, value in values(path).items():
                if key not in english:
                    continue
                checked += 1
                expected = specifiers(english[key], is_wide)
                got = specifiers(value, is_wide)
                if got != expected:
                    mismatches.append(f"{path.name}: {key!r}: en {expected}, {locale} {got}")
    for line in mismatches:
        print(line)
    print(f"{checked} translations checked, {len(mismatches)} with other specifiers than en")
    return 1 if mismatches else 0


if __name__ == "__main__":
    sys.exit(main())
