    .code16
    .text
    .global _start
#define invop   .byte
//#define invop   #

_start:
    // 00
    add    %dl,0x9090(%bx)
    add    %dx,0x9090(%bx)
    add    0x9090(%bx),%dl
    add    0x9090(%bx),%dx
    add    $0x90,%al
    add    $0x9090,%ax
    push   %es
    pop    %es
    or     %dl,0x9090(%bx)
    or     %dx,0x9090(%bx)
    or     0x9090(%bx),%dl
    or     0x9090(%bx),%dx
    or     $0x90,%al
    or     $0x9090,%ax
    push   %cs
    invop  0x0f

    // 10
    adc    %dl,0x9090(%bx)
    adc    %dx,0x9090(%bx)
    adc    0x9090(%bx),%dl
    adc    0x9090(%bx),%dx
    adc    $0x90,%al
    adc    $0x9090,%ax
    push   %ss
    pop    %ss
    sbb    %dl,0x9090(%bx)
    sbb    %dx,0x9090(%bx)
    sbb    0x9090(%bx),%dl
    sbb    0x9090(%bx),%dx
    sbb    $0x90,%al
    sbb    $0x9090,%ax
    push   %ds
    pop    %ds

    // 20
    and    %dl,0x9090(%bx)
    and    %dx,0x9090(%bx)
    and    0x9090(%bx),%dl
    and    0x9090(%bx),%dx
    and    $0x90,%al
    and    $0x9090,%ax
    es
    daa
    sub    %dl,0x9090(%bx)
    sub    %dx,0x9090(%bx)
    sub    0x9090(%bx),%dl
    sub    0x9090(%bx),%dx
    sub    $0x90,%al
    sub    $0x9090,%ax
    cs
    das

    // 30
    xor    %dl,0x9090(%bx)
    xor    %dx,0x9090(%bx)
    xor    0x9090(%bx),%dl
    xor    0x9090(%bx),%dx
    xor    $0x90,%al
    xor    $0x9090,%ax
    ss
    aaa
    cmp    %dl,0x9090(%bx)
    cmp    %dx,0x9090(%bx)
    cmp    0x9090(%bx),%dl
    cmp    0x9090(%bx),%dx
    cmp    $0x90,%al
    cmp    $0x9090,%ax
    ds
    aas

    // 40
    inc    %ax
    inc    %cx
    inc    %dx
    inc    %bx
    inc    %sp
    inc    %bp
    inc    %si
    inc    %di
    dec    %ax
    dec    %cx
    dec    %dx
    dec    %bx
    dec    %sp
    dec    %bp
    dec    %si
    dec    %di

    // 50
    push   %ax
    push   %cx
    push   %dx
    push   %bx
    push   %sp
    push   %bp
    push   %si
    push   %di
    pop    %ax
    pop    %cx
    pop    %dx
    pop    %bx
    pop    %sp
    pop    %bp
    pop    %si
    pop    %di

    // 60
    invop  0x60
    invop  0x61
    invop  0x62
    invop  0x63
    invop  0x64
    invop  0x65
    invop  0x66
    invop  0x67
    invop  0x68
    invop  0x69
    invop  0x6a
    invop  0x6b
    invop  0x6c
    invop  0x6d
    invop  0x6e
    invop  0x6f
    //pusha
    //popa
    //bound  %dx,0x9090(%bx)
    //arpl   %dx,0x9090(%bx)
    //push   $0x9090
    //imul   $0x9090,0x9090(%bx),%dx
    //push   $0xff90
    //imul   $0xff90,0x9090(%bx),%dx
    //insb   (%dx),%es:(%di)
    //insw   (%dx),%es:(%di)
    //outsb  %ds:(%si),(%dx)
    //outsw  %ds:(%si),(%dx)

    // 70
    jo     .+2-0x70
    jno    .+2-0x70
    jb     .+2-0x70
    jae    .+2-0x70
    je     .+2-0x70
    jne    .+2-0x70
    jbe    .+2-0x70
    ja     .+2-0x70
    js     .+2-0x70
    jns    .+2-0x70
    jp     .+2-0x70
    jnp    .+2-0x70
    jl     .+2-0x70
    jge    .+2-0x70
    jle    .+2-0x70
    jg     .+2-0x70

    // 80
    adcb   $0x90,0x9090(%bx)
    adcw   $0x9090,0x9090(%bx)
    // 82 is same as 80 for 8086?
    invop  0x82,0x87,0x90,0x90,0x90 // disassembles as addb $0x90,0x9090(%bx)
    //addb   $0x90,0x9090(%bx)      // generates 0x80 opcode
    adcw   $0xff90,0x9090(%bx)
    test   %dl,0x9090(%bx)
    test   %dx,0x9090(%bx)
    xchg   %dl,0x9090(%bx)
    xchg   %dx,0x9090(%bx)
    mov    %dl,0x9090(%bx)
    mov    %dx,0x9090(%bx)
    mov    0x9090(%bx),%dl
    mov    0x9090(%bx),%dx
    movw   %ss,0x9090(%bx)
    lea    0x9090(%bx),%dx
    movw   0x9090(%bx),%ss
    popw   0x9090(%bx)

    // 90
    xchg   %ax,%ax                  // FIXME display as NOP?
    xchg   %ax,%cx
    xchg   %ax,%dx
    xchg   %ax,%bx
    xchg   %ax,%sp
    xchg   %ax,%bp
    xchg   %ax,%si
    xchg   %ax,%di
    cbtw
    cwtd
    lcall  $0x9090,$0x9090
    fwait
    pushf
    popf
    sahf
    lahf

    // A0
    mov    0x9090,%al
    mov    0x9090,%ax
    mov    %al,0x9090
    mov    %ax,0x9090
    movsb  %ds:(%si),%es:(%di)
    movsw  %ds:(%si),%es:(%di)
    cmpsb  %es:(%di),%ds:(%si)
    cmpsw  %es:(%di),%ds:(%si)
    test   $0x90,%al
    test   $0x9090,%ax
    stos   %al,%es:(%di)
    stos   %ax,%es:(%di)
    lods   %ds:(%si),%al
    lods   %ds:(%si),%ax
    scas   %es:(%di),%al
    scas   %es:(%di),%ax

    // B0
    mov    $0x90,%al
    mov    $0x90,%cl
    mov    $0x90,%dl
    mov    $0x90,%bl
    mov    $0x90,%ah
    mov    $0x90,%ch
    mov    $0x90,%dh
    mov    $0x90,%bh
    mov    $0x9090,%ax
    mov    $0x9090,%cx
    mov    $0x9090,%dx
    mov    $0x9090,%bx
    mov    $0x9090,%sp
    mov    $0x9090,%bp
    mov    $0x9090,%si
    mov    $0x9090,%di

    // C0
    invop  0xc0
    invop  0xc1
    //rclb   $0x90,0x9090(%bx)
    //rclw   $0x90,0x9090(%bx)
    ret    $0x9090
    ret
    les    0x9090(%bx),%dx
    lds    0x9090(%bx),%dx
    movb   $0x90,0x9090(%bx)
    movw   $0x9090,0x9090(%bx)
    invop  0xc8
    invop  0xc9
    //enter  $0x9090,$0x90
    //leave
    lret   $0x9090
    lret
    int3
    int    $0x90
    into
    iret

    // D0
    rclb   0x9090(%bx)
    rclw   0x9090(%bx)
    rclb   %cl,0x9090(%bx)
    rclw   %cl,0x9090(%bx)
    aam    $0x90        // FIXME disasm arg
    aad    $0x90        // FIXME disasm arg
    //salc              // undocumented STC in AL (0xd6)
    invop  0xd6
    xlat   %ds:(%bx)
    invop  0xd8
    invop  0xd9
    invop  0xda
    invop  0xdb
    invop  0xdc
    invop  0xdd
    invop  0xde
    invop  0xdf

    // E0
    loopne .+2-0x70
    loope  .+2-0x70
    loop   .+2-0x70
    jcxz  .+2-0x70
    in     $0x90,%al
    in     $0x90,%ax
    out    %al,$0x90
    out    %ax,$0x90
    call   .+5+0x9090
    jmp    .+5+0x9090
    ljmp   $0x9090,$0x9090
    jmp    .+2-0x70
    in     (%dx),%al
    in     (%dx),%ax
    out    %al,(%dx)
    out    %ax,(%dx)

    // F0
    lock
    invop  0xf1
    repnz
    repz
    hlt
    cmc
    notb   0x9090(%bx)
    notw   0x9090(%bx)
    clc
    stc
    cli
    sti
    cld
    std
    incb   %al
    decb   0x9090(%bx)
    call   *0x9090(%bx)
    push   0x0000
    push   0x7fff
    push   0x8000
    push   0x8001

 // syscall exit
    mov     $1,%ax
    mov     $0,%bx
    int     $0x80

#if 0
    push   $0x0000
    push   $0x7fff
    push   $0x8000
    push   $0x8001
    add    %dx,0x9090(%bx)
    add    0x9090(%bx),%dx
    add    $0x9090,%ax
    pushw  %es
    popw   %es
    or     %dx,0x9090(%bx)
    or     0x9090(%bx),%dx
    or     $0x9090,%ax
    pushw  %cs
    adc    %dx,0x9090(%bx)
    adc    0x9090(%bx),%dx
    adc    $0x9090,%ax
    pushw  %ss
    popw   %ss
    sbb    %dx,0x9090(%bx)
    sbb    0x9090(%bx),%dx
    sbb    $0x9090,%ax
    pushw  %ds
    popw   %ds
    and    %dx,0x9090(%bx)
    and    0x9090(%bx),%dx
    and    $0x9090,%ax
    sub    %dx,0x9090(%bx)
    sub    0x9090(%bx),%dx
    sub    $0x9090,%ax
    xor    %dx,0x9090(%bx)
    xor    0x9090(%bx),%dx
    xor    $0x9090,%ax
    cmp    %dx,0x9090(%bx)
    cmp    0x9090(%bx),%dx
    cmp    $0x9090,%ax
    inc    %ax
    inc    %cx
    inc    %dx
    inc    %bx
    inc    %sp
    inc    %bp
    inc    %si
    inc    %di
    dec    %ax
    dec    %cx
    dec    %dx
    dec    %bx
    dec    %sp
    dec    %bp
    dec    %si
    dec    %di
    push   %ax
    push   %cx
    push   %dx
    push   %bx
    push   %sp
    push   %bp
    push   %si
    push   %di
    pop    %ax
    pop    %cx
    pop    %dx
    pop    %bx
    pop    %sp
    pop    %bp
    pop    %si
    pop    %di
    //bound  %dx,0x9090(%bx)
    //pushw  $0x9090
    //imul   $0x9090,0x9090(%bx),%dx
    //pushw  $0xff90
    //imul   $0xff90,0x9090(%bx),%dx
    //insw   (%dx),%es:(%di)
    //outsw  %ds:(%si),(%dx)
    adcw   $0x9090,0x9090(%bx)
    adcw   $0xff90,0x9090(%bx)
    test   %dx,0x9090(%bx)
    xchg   %dx,0x9090(%bx)
    mov    %dx,0x9090(%bx)
    mov    0x9090(%bx),%dx
    movw   %ss,0x9090(%bx)
    lea    0x9090(%bx),%dx
    popw   0x9090(%bx)
    xchg   %ax,%ax
    xchg   %ax,%cx
    xchg   %ax,%dx
    xchg   %ax,%bx
    xchg   %ax,%sp
    xchg   %ax,%bp
    xchg   %ax,%si
    xchg   %ax,%di
    cbtw
    cwtd
    lcallw $0x9090,$0x9090
    pushf
    popf
    mov    0x9090,%ax
    mov    %ax,0x9090
    movsw  %ds:(%si),%es:(%di)
    cmpsw  %es:(%di),%ds:(%si)
    test   $0x9090,%ax
    stos   %ax,%es:(%di)
    lods   %ds:(%si),%ax
    scas   %es:(%di),%ax
    mov    $0x9090,%ax
    mov    $0x9090,%cx
    mov    $0x9090,%dx
    mov    $0x9090,%bx
    mov    $0x9090,%sp
    mov    $0x9090,%bp
    mov    $0x9090,%si
    mov    $0x9090,%di
    rclw   $0x90,0x9090(%bx)
    retw   $0x9090
    retw
    les    0x9090(%bx),%dx
    lds    0x9090(%bx),%dx
    movw   $0x9090,0x9090(%bx)
    enterw $0x9090,$0x90
    leavew
    lretw  $0x9090
    lretw
    iretw
    rclw   0x9090(%bx)
    rclw   %cl,0x9090(%bx)
    in     $0x90,%ax
    out    %ax,$0x90
    callw  .+3+0x9090
    ljmpw  $0x9090,$0x9090
    in     (%dx),%ax
    out    %ax,(%dx)
    notw   0x9090(%bx)
    callw  *0x9090(%bx)

    test   %ax,%bx
    test   %bx,%ax
    test   (%bx),%bx

    .byte 0x82, 0xc3, 0x01
    .byte 0x82, 0xf3, 0x01
    .byte 0x82, 0xd3, 0x01
    .byte 0x82, 0xdb, 0x01
    .byte 0x82, 0xe3, 0x01
    .byte 0x82, 0xeb, 0x01
    .byte 0x82, 0xf3, 0x01
    .byte 0x82, 0xfb, 0x01

    .byte 0xf6, 0xc9, 0x01
    //.byte 0x66, 0xf7, 0xc9, 0x02, 0x00
    //.byte 0xf7, 0xc9, 0x04, 0x00, 0x00, 0x00
    .byte 0xc0, 0xf0, 0x02
    .byte 0xc1, 0xf0, 0x01
    .byte 0xd0, 0xf0
    .byte 0xd1, 0xf0
    .byte 0xd2, 0xf0
    .byte 0xd3, 0xf0
#endif
