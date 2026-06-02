/*
 * Nexsteaduser — PlexsDOS
 * editor.h — 全屏文本编辑器接口
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 类 DOS EDIT.COM 的全屏文本编辑器。
 * 支持: 光标移动、插入/删除、查找、保存/加载、行号显示。
 */

#ifndef _PLXSDOS_EDITOR_H
#define _PLXSDOS_EDITOR_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * editor_main — 启动文本编辑器
 * @filename: 要打开的文件名 (NULL = 新文件)
 *
 * 进入全屏编辑模式。按 ESC 后输入 Q 退出。
 */
void editor_main(const char *filename);

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_EDITOR_H */
