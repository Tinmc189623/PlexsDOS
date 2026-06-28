#!/usr/bin/env python3
"""Strip CJK chars from source files, save as UTF-8 BOM to fix C4819."""
import os
import re

ROOT = os.path.dirname(os.path.abspath(__file__))
FILES = ['win-compat.h', 'config.win.h', 'win-compat.c']

for fname in FILES:
    path = os.path.join(ROOT, fname)
    try:
        with open(path, 'r', encoding='utf-8') as f:
            text = f.read()
    except UnicodeDecodeError:
        with open(path, 'r', encoding='utf-8', errors='replace') as f:
            text = f.read()

    before = len(text)

    # Replace any character above U+007F (ASCII range)
    # with its nearest ASCII equivalent or remove
    cleaned = []
    for ch in text:
        cp = ord(ch)
        if cp < 128:
            cleaned.append(ch)
        elif cp in (0x2014, 0x2015, 0x2018, 0x2019, 0x201c, 0x201d):
            cleaned.append(' ')
        elif cp == 0x3001:
            cleaned.append(',')
        elif cp == 0x3002:
            cleaned.append('.')
        elif cp == 0x300a:
            cleaned.append('<')
        elif cp == 0x300b:
            cleaned.append('>')
        elif cp == 0x2018 or cp == 0x2019:
            cleaned.append("'")
        elif cp == 0x201c or cp == 0x201d:
            cleaned.append('"')
        elif cp == 0x2026:
            cleaned.append('...')
        elif cp >= 0x4e00 and cp <= 0x9fff:
            cleaned.append(' ')
        elif cp >= 0x2000 and cp <= 0x206f:
            cleaned.append(' ')
        else:
            cleaned.append(' ')

    result = ''.join(cleaned)
    # Collapse multiple spaces
    result = re.sub(r'  +', ' ', result)

    after = len(result)

    with open(path, 'wb') as f:
        f.write(b'\xef\xbb\xbf')        # UTF-8 BOM
        f.write(result.encode('utf-8'))

    print(f'{fname}: {before - after} chars removed, {before} -> {after}')

print('Done.')
