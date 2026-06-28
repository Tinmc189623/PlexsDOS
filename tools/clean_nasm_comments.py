#!/usr/bin/env python3
"""
清理已转换注释里的多余 * 装饰字符:
- '; *'   -> ';'
- ';*'    -> ';'
- '; * '  -> '; '
- ' ;*'   -> ';'
- 空注释行 '; ' (无内容) -> 删除
"""
import sys
import pathlib
import re


def clean_line(line: str) -> str:
    stripped = line.lstrip()
    if not stripped.startswith(';'):
        return line
    # 找到 ; 位置
    semi_pos = line.index(';')
    prefix = line[:semi_pos]
    rest = line[semi_pos+1:]
    # 跳过空白
    rest_stripped = rest.lstrip()
    # 如果是 *, 删除
    if rest_stripped.startswith('*'):
        rest_stripped = rest_stripped[1:].lstrip()
    # 重组
    if not rest_stripped:
        # 空注释
        return prefix.rstrip() + '\n' if prefix.strip() else ''
    else:
        # 保留一个空格
        return prefix + '; ' + rest_stripped


def clean_file(path: pathlib.Path):
    content = path.read_text(encoding='utf-8')
    lines = content.split('\n')
    new_lines = [clean_line(l) for l in lines]
    # 去除连续空行
    result = []
    prev_empty = False
    for l in new_lines:
        is_empty = (l.strip() == '')
        if is_empty and prev_empty:
            continue
        result.append(l)
        prev_empty = is_empty
    # 确保末尾单换行
    while result and result[-1] == '':
        result.pop()
    path.write_text('\n'.join(result) + '\n', encoding='utf-8')


def main():
    for arg in sys.argv[1:]:
        p = pathlib.Path(arg)
        if not p.exists():
            print(f"skip: {p}")
            continue
        clean_file(p)
        print(f"cleaned: {p}")


if __name__ == '__main__':
    main()