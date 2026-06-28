#!/usr/bin/env python3
"""
把 NASM 文件里的 C 风格 /* ... */ 注释转换成 NASM 风格 ; 注释。

策略:
- 多行注释: /* ... */  -> 每行替换为 ;
- 单行注释: /* ... */  -> ; ...
- 跳过字符串里的 /* 或 */  (启发式: 不在引号内)
"""
import sys
import re
import pathlib


def is_in_string(line, pos):
    """检查 pos 处的字符是否在字符串字面量中。"""
    in_str = False
    quote = None
    i = 0
    while i < pos:
        c = line[i]
        # 跳过 NASM 转义: 反斜杠 + 任意字符
        if c == '\\' and i + 1 < len(line):
            i += 2
            continue
        if not in_str:
            if c in ('"', "'"):
                in_str = True
                quote = c
        else:
            if c == quote:
                in_str = False
                quote = None
        i += 1
    return in_str


def convert_line(line):
    """把单行 /* ... */ 转成 ; ..."""
    # 找 /* 不在字符串里的位置
    result = []
    i = 0
    while i < len(line):
        if not is_in_string(line, i) and line[i:i+2] == '/*':
            # 找 */
            end = line.find('*/', i + 2)
            if end < 0:
                # 跨行注释, 当前行全部转成 ;
                comment_body = line[i+2:]
                result.append('; ' + comment_body if comment_body.strip() else ';')
                return ''.join(result)
            else:
                # 行内 /* ... */, 替换为 ;
                comment_body = line[i+2:end]
                result.append('; ' + comment_body if comment_body.strip() else ';')
                i = end + 2
        else:
            result.append(line[i])
            i += 1
    return ''.join(result)


def convert_file(path: pathlib.Path):
    content = path.read_text(encoding='utf-8')
    new_lines = []
    in_block_comment = False
    block_lines = []
    for line in content.split('\n'):
        if in_block_comment:
            end = line.find('*/')
            if end < 0:
                # 整行都是注释
                stripped = line.strip()
                if stripped:
                    new_lines.append('; ' + stripped)
                else:
                    new_lines.append('')
            else:
                # 注释在这一行结束
                before = line[:line.find('/*')].rstrip()
                after = line[end+2:].lstrip()
                comment_body = line[line.find('/*')+2:end].strip()
                if comment_body:
                    new_lines.append('; ' + comment_body)
                if after:
                    # 处理 before + after 合并
                    if before:
                        new_lines.append(before)
                    new_lines.append(convert_line(after))
                elif before:
                    new_lines.append(before)
                in_block_comment = False
        else:
            # 检查是否有 /* 但没在同一行关闭
            start = line.find('/*')
            while start >= 0 and not is_in_string(line, start):
                end = line.find('*/', start + 2)
                if end >= 0:
                    # 同一行的块注释, 用 convert_line 处理
                    converted = convert_line(line)
                    new_lines.append(converted)
                    line = None
                    break
                else:
                    # 跨行块注释开始
                    before = line[:start].rstrip()
                    comment_body = line[start+2:].strip()
                    if before:
                        new_lines.append(before)
                    if comment_body:
                        new_lines.append('; ' + comment_body)
                    else:
                        new_lines.append('')
                    in_block_comment = True
                    line = None
                    break
            if line is not None:
                # 没有块注释, 但可能有行内 /* */
                converted = convert_line(line)
                new_lines.append(converted)
    path.write_text('\n'.join(new_lines), encoding='utf-8')


def main():
    if len(sys.argv) < 2:
        print("Usage: fix_nasm_comments.py <file.asm> [file2.asm ...]")
        sys.exit(1)
    for arg in sys.argv[1:]:
        p = pathlib.Path(arg)
        if not p.exists():
            print(f"skip: {p} not found")
            continue
        convert_file(p)
        print(f"fixed: {p}")


if __name__ == '__main__':
    main()