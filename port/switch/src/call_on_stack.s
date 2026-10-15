.section .text.call_on_stack, "ax", @progbits
.global call_on_stack
.type call_on_stack, %function
call_on_stack:
    and x0, x0, #~15
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    stp x19, x20, [sp, #16]
    mov x19, sp
    mov sp, x0
    mov x16, x1
    mov x0, x2
    mov x1, x3
    mov x2, x4
    mov x3, x5
    blr x16
    mov sp, x19
    ldp x19, x20, [sp, #16]
    ldp x29, x30, [sp], #32
    ret
