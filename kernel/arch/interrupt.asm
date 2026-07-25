; Nexsteaduser — PlexsDOS
; 中断处理入口 (32-bit 保护模式)
; 作者: Tinmc189623 | 团队: Nexlyh
;
; ISR_SAVE_REGS 后栈布局 (ESP 指向 GS, 即栈顶):
;   0  : GS
;   4  : FS
;   8  : ES
;   12 : DS
;   16 : EDI
;   20 : ESI
;   24 : EBP
;   28 : ESP (原始值, ignored by popad)
;   32 : EBX
;   36 : EDX
;   40 : ECX
;   44 : EAX
;   48 : EIP    (iret 帧)
;   52 : CS
;   56 : EFLAGS
;   60 : ESP3   (仅 Ring 3)
;   64 : SS3    (仅 Ring 3)
;
; 语法: NASM Intel, 32-bit 保护模式。
; Copyright © 2026 Nexsteaduser. All Rights Reserved

bits 32
section .text

global _isr_default
global _isr_keyboard
global _isr_timer
global _idt_load
global _isr_syscall
global _isr_fdc
global _isr_mouse
global _isr_exception_common

; 外部 C/C++ 函数 (cdecl 调用约定)
extern _kbd_interrupt_handler
extern _fdc_interrupt_handler
extern _mouse_interrupt_handler
extern _pit_interrupt_handler
extern _cpp_syscall_dispatch
extern _panic_exception_handler
extern _sched_need_resched
extern _sched_do_switch_asm
extern _sched_exit_asm

; ----------------------------------------------------------------------------
; ISR_SAVE_REGS — 保存所有通用寄存器和段寄存器
; 保存后 ESP 指向 GS (栈顶)。
; ----------------------------------------------------------------------------
%macro ISR_SAVE_REGS 0
    pushad
    push ds
    push es
    push fs
    push gs
    mov ax, 0x10               ; 内核数据段选择子
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
%endmacro

; ----------------------------------------------------------------------------
; ISR_RESTORE_REGS — 恢复所有通用寄存器和段寄存器
; ----------------------------------------------------------------------------
%macro ISR_RESTORE_REGS 0
    pop gs
    pop fs
    pop es
    pop ds
    popad
%endmacro

; ----------------------------------------------------------------------------
; 宏: ISR_RETURN_FAST — 快速中断返回 (不调度)
; ----------------------------------------------------------------------------
%macro ISR_RETURN_FAST 0
    ISR_RESTORE_REGS
    iret
%endmacro

; ----------------------------------------------------------------------------
; 宏: ISR_RETURN — 中断返回 (可调度版本)
; 在调用 C 处理程序后使用。
; 进入此宏时: EAX = 0 (正常) 或 1 (进程退出)
;            EBX = 退出状态码 (当 EAX=1 时)
; 检查是否需要调度或退出, 如需要则切换进程。
; ----------------------------------------------------------------------------
%macro ISR_RETURN 0
    test eax, eax
    jnz .exit_process

    ; 检查是否需要重新调度
    call _sched_need_resched
    test eax, eax
    jz .no_resched

    ; 需要调度: ESP 此刻指向 GS (栈顶), 正是 sched_do_switch_asm 需要的 context_esp
    push esp
    call _sched_do_switch_asm
    add esp, 4
    ; 如果返回, 表示不需要切换 (next == current), 继续恢复

.no_resched:
    ISR_RESTORE_REGS
    iret

.exit_process:
    ; ebx = status
    ; 计算 push ebx 之前的 context_esp (即 ESP, 因为我们还没 push)
    push ebx                   ; arg2: status
    mov eax, esp
    add eax, 4                 ; arg1: context_esp = push ebx 前的 esp
    push eax
    call _sched_exit_asm
    add esp, 8
    ; 不返回
    jmp $
%endmacro

; ----------------------------------------------------------------------------
; _isr_default — 默认中断处理程序
; ----------------------------------------------------------------------------
_isr_default:
    ISR_SAVE_REGS
    mov al, 0x20
    out 0x20, al               ; 发送 EOI
    ISR_RETURN_FAST

; ----------------------------------------------------------------------------
; _isr_timer — 定时器中断 (IRQ0, INT 0x20)
; ----------------------------------------------------------------------------
_isr_timer:
    ISR_SAVE_REGS
    call _pit_interrupt_handler
    xor eax, eax
    xor ebx, ebx
    ISR_RETURN

; ----------------------------------------------------------------------------
; _isr_keyboard — 键盘中断 (IRQ1, INT 0x21)
; ----------------------------------------------------------------------------
_isr_keyboard:
    ISR_SAVE_REGS
    call _kbd_interrupt_handler
    mov al, 0x20
    out 0x20, al               ; 发送 EOI
    xor eax, eax
    xor ebx, ebx
    ISR_RETURN

; ----------------------------------------------------------------------------
; _isr_fdc — 软盘控制器中断 (IRQ6, INT 0x26)
; ----------------------------------------------------------------------------
_isr_fdc:
    ISR_SAVE_REGS
    call _fdc_interrupt_handler
    xor eax, eax
    xor ebx, ebx
    ISR_RETURN

; ----------------------------------------------------------------------------
; _isr_mouse — PS/2 鼠标中断 (IRQ12, INT 0x2C)
; ----------------------------------------------------------------------------
_isr_mouse:
    ISR_SAVE_REGS
    call _mouse_interrupt_handler
    mov al, 0x20
    out 0xA0, al               ; 从片 EOI
    out 0x20, al               ; 主片 EOI
    xor eax, eax
    xor ebx, ebx
    ISR_RETURN

; ----------------------------------------------------------------------------
; _idt_load — 加载 IDT 寄存器
; C 原型: void idt_load(void *idt_ptr);
; ----------------------------------------------------------------------------
_idt_load:
    push ebp
    mov ebp, esp
    mov eax, [ebp + 8]
    lidt [eax]
    pop ebp
    ret

; ----------------------------------------------------------------------------
; _isr_syscall — INT 0x22 系统调用入口
;
; 调用 C: cpp_syscall_dispatch(eax, edx, esi)
; 返回值 eax: 0 = 正常, 非0 = 处理结果
; 如果用户 AH = 0x4C (SYS_EXIT), 请求退出进程。
; ----------------------------------------------------------------------------
_isr_syscall:
    ISR_SAVE_REGS

    ; 从保存的上下文读取用户寄存器:
    ;   EAX 在 [esp + 44]
    ;   EDX 在 [esp + 36]
    ;   ESI 在 [esp + 20]
    mov eax, [esp + 44]        ; 用户 EAX (AH=功能号, AL=子参数)
    mov edx, [esp + 36]        ; 用户 EDX
    mov esi, [esp + 20]        ; 用户 ESI

    ; cdecl: 参数从右到左压栈
    push esi
    push edx
    push eax
    call _cpp_syscall_dispatch
    add esp, 12
    ; 返回值在 EAX 中; ECX/EDX 可能被破坏 (cdecl 约定)

    ; 将返回值临时保存到 ECX
    mov ecx, eax

    ; 从栈上读取用户原始 EAX (未被修改, 因为 ESP 已恢复)
    mov edx, [esp + 44]        ; EDX = 用户原始 EAX
    mov eax, edx
    shr eax, 8                 ; AL = AH = 功能号
    and eax, 0xFF
    cmp al, 0x4C
    je .syscall_exit

    ; 非退出系统调用: 将返回值写回栈上保存的 EAX 位置
    mov [esp + 44], ecx
    ; eax=0, ebx=0 (ISR_RETURN 正常返回)
    xor eax, eax
    xor ebx, ebx
    jmp .syscall_done

.syscall_exit:
    ; 退出进程: eax=1 (请求退出), ebx=DL=用户原始 AL=退出状态码
    mov eax, 1
    movzx ebx, dl

.syscall_done:
    ISR_RETURN

; ----------------------------------------------------------------------------
; _isr_exception_common — CPU 异常通用入口
;
; 异常桩预先压入: [vector(4)] [err(4)]
; 然后 CPU 压入 iret 帧。
; ISR_SAVE_REGS 再压入通用寄存器和段寄存器。
;
; 栈布局 (ISR_SAVE_REGS 后):
;   0-47  : GS/FS/ES/DS/EDI..EAX (同普通中断)
;   48    : DS (最后一个 push 的段寄存器)
;   等等, 让我重新计算 (异常比普通中断多 8 字节: vector + err):
;
; ISR_SAVE_REGS 压入 48 字节 (pushad 32 + segregs 16)
; 然后异常桩预先压入了 8 字节 (vector, err)
; 然后 CPU 压入 iret 帧
;
; 所以:
;   0-47  : GS,FS,ES,DS,EDI..EAX (ISR_SAVE_REGS)
;   48    : vector (异常桩压入)
;   52    : error_code
;   56    : EIP   (CPU iret 帧)
;   60    : CS
;   64    : EFLAGS
;   Ring3: 68: ESP3, 72: SS3
; ----------------------------------------------------------------------------
_isr_exception_common:
    ; VGA 调试标记: 第 3 行显示 'EX'
    push eax
    push ebx
    mov ebx, 0xB8000
    mov word [ebx + 320], 0x4F45    ; 'E' 红底白字
    mov word [ebx + 322], 0x4F58    ; 'X'
    pop ebx
    pop eax

    ISR_SAVE_REGS

    ; 读取 CS 判断特权级: CS 在 [esp + 60] (Ring 0) 或 [esp + 68] (Ring 3)
    ; 但是因为有异常桩的 8 字节, 偏移要加8
    ; 普通中断 CS 在 +52, 异常在 +52+8 = +60
    mov eax, [esp + 60]
    test eax, 3
    jnz .exc_ring3

    ; Ring 0 异常
    sub esp, 12
    lea eax, [esp + 12]             ; arg3 = regs (指向 GS)
    mov [esp + 8], eax
    mov eax, [esp + 52 + 12]        ; arg2 = err at 52
    mov [esp + 4], eax
    mov eax, [esp + 48 + 12]        ; arg1 = vector at 48
    mov [esp], eax
    jmp .exc_call

.exc_ring3:
    sub esp, 12
    lea eax, [esp + 12]
    mov [esp + 8], eax
    mov eax, [esp + 60 + 12]        ; err at 60 (Ring 3 多 8 字节)
    mov [esp + 4], eax
    mov eax, [esp + 56 + 12]        ; vector at 56
    mov [esp], eax

.exc_call:
    call _panic_exception_handler
.exc_halt:
    hlt
    jmp .exc_halt

; ----------------------------------------------------------------------------
; 异常专用桩 — 无错误码异常压入 0 占位
; ----------------------------------------------------------------------------
%macro EXCEPTION_STUB_NO_ERROR 1
global _isr_exception_%1
_isr_exception_%1:
    push dword 0
    push dword %1
    jmp _isr_exception_common
%endmacro

; ----------------------------------------------------------------------------
; 异常专用桩 — 有错误码异常 (CPU 已压入)
; ----------------------------------------------------------------------------
%macro EXCEPTION_STUB_WITH_ERROR 1
global _isr_exception_%1
_isr_exception_%1:
    push dword %1
    jmp _isr_exception_common
%endmacro

; 无错误码异常
EXCEPTION_STUB_NO_ERROR 0x00    ; #DE
EXCEPTION_STUB_NO_ERROR 0x01    ; #DB
EXCEPTION_STUB_NO_ERROR 0x02    ; NMI
EXCEPTION_STUB_NO_ERROR 0x03    ; #BP
EXCEPTION_STUB_NO_ERROR 0x04    ; #OF
EXCEPTION_STUB_NO_ERROR 0x05    ; #BR
EXCEPTION_STUB_NO_ERROR 0x06    ; #UD
EXCEPTION_STUB_NO_ERROR 0x07    ; #NM
EXCEPTION_STUB_NO_ERROR 0x09    ; Coprocessor Overrun
EXCEPTION_STUB_NO_ERROR 0x0F    ; Reserved
EXCEPTION_STUB_NO_ERROR 0x10    ; #MF
EXCEPTION_STUB_NO_ERROR 0x12    ; #MC
EXCEPTION_STUB_NO_ERROR 0x13    ; #XM
EXCEPTION_STUB_NO_ERROR 0x14    ; #VE

; 有错误码异常
EXCEPTION_STUB_WITH_ERROR 0x08  ; #DF
EXCEPTION_STUB_WITH_ERROR 0x0A  ; #TS
EXCEPTION_STUB_WITH_ERROR 0x0B  ; #NP
EXCEPTION_STUB_WITH_ERROR 0x0C  ; #SS
EXCEPTION_STUB_WITH_ERROR 0x0D  ; #GP
EXCEPTION_STUB_WITH_ERROR 0x0E  ; #PF
EXCEPTION_STUB_WITH_ERROR 0x11  ; #AC
EXCEPTION_STUB_WITH_ERROR 0x15  ; #CP
