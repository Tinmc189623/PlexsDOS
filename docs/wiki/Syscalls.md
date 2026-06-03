# 系统调用

PlexsDOS 通过 INT 21h (向量 0x22) 提供 DOS 兼容系统调用接口。

## 子功能

| 功能号 | 名称 | 说明 |
|--------|------|------|
| 0x00 | 程序退出 | 终止当前程序 |
| 0x01 | 键盘输入 | 读取下一个按键 |
| 0x02 | 字符输出 | 输出字符到屏幕 |
| 0x09 | 字符串输出 | 输出 '$' 结尾的字符串 |
| 0x4C | 带返回码退出 | 终止程序并返回码 |
| 其他 | 扩展功能 | PlexsDOS 自定义扩展 |

## C++ 中断框架

```cpp
class InterruptHandler {
public:
    virtual void handle(InterruptFrame *frame) = 0;
};

class InterruptManager {
public:
    void addHandler(int vector, InterruptHandler *handler);
    void removeHandler(int vector);
};
```
