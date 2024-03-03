.extern isr_handler

/* 
 * This macro creates a stub for an ISR which does not pass it's own
 * error code. Adds a dummy error code.
 */
.macro ISR num
    .globl isr_\num
    isr_\num:
        pushq $0
        # Store  the interrupt number in the highest four bytes of the erro code.
        # This way we can always increment rsp by 8 before iretq and no memory is wasted.
        movl $\num, 4(%rsp)
        jmp isr_common
.endm

/*
 * This macro creates a stub for an ISR which passes it's own error code
 */
.macro ISR_ERR num
    .globl isr_\num
    isr_\num:
        # Store  the interrupt number in the highest four bytes of the erro code.
        # This way we can always increment rsp by 8 before iretq and no memory is wasted.
        movl $\num, 4(%rsp)
        jmp isr_common
.endm

/**
 * This macro creates a stub for IRQ related interruptions
 */
.macro ISR_IRQ num
    .globl isr_\num
    isr_\num:
        pushq $0
        # Store  the interrupt number in the highest four bytes of the erro code.
        # This way we can always increment rsp by 8 before iretq and no memory is wasted.
        movl $\num, 4(%rsp)
        jmp isr_common
.endm

.section .text

.code64

isr_common:
    # Preserve Scratch Register in order
    pushq %r11
    pushq %r10
    pushq %r9
    pushq %r8
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rax

    mov %rsp, %rdi
    call isr_handler

    # Pop back register
    popq %rax
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %r8
    popq %r9
    popq %r10
    popq %r11

    # Remove info from the stack
    add $8, %rsp

    iretq

# Exceptions
ISR 0
ISR 1
ISR 2
ISR 3
ISR 4
ISR 5
ISR 6
ISR 7
ISR_ERR 8
ISR 9
ISR_ERR 10
ISR_ERR 11
ISR_ERR 12
ISR_ERR 13
ISR_ERR 14
ISR 15
ISR 16
ISR 17
ISR 18
ISR 19
ISR 20
ISR 21
ISR 22
ISR 23
ISR 24
ISR 25
ISR 26
ISR 27
ISR 28
ISR 29
ISR 30
ISR 31

# TODO: Maybe clean up the unused ones?
ISR_IRQ 32
ISR_IRQ 33
ISR_IRQ 34
ISR_IRQ 35
ISR_IRQ 36
ISR_IRQ 37
ISR_IRQ 38
ISR_IRQ 39
ISR_IRQ 40
ISR_IRQ 41
ISR_IRQ 42
ISR_IRQ 43
ISR_IRQ 44
ISR_IRQ 45
ISR_IRQ 46
ISR_IRQ 47

ISR_IRQ 48
ISR_IRQ 49
ISR_IRQ 50
ISR_IRQ 51
ISR_IRQ 52
ISR_IRQ 53
ISR_IRQ 54
ISR_IRQ 55
ISR_IRQ 56
ISR_IRQ 57
ISR_IRQ 58
ISR_IRQ 59
ISR_IRQ 60
ISR_IRQ 61
ISR_IRQ 62
ISR_IRQ 63
ISR_IRQ 64
ISR_IRQ 65
ISR_IRQ 66
ISR_IRQ 67
ISR_IRQ 68
ISR_IRQ 69
ISR_IRQ 70
ISR_IRQ 71
ISR_IRQ 72
ISR_IRQ 73
ISR_IRQ 74
ISR_IRQ 75
ISR_IRQ 76
ISR_IRQ 77
ISR_IRQ 78
ISR_IRQ 79
ISR_IRQ 80
ISR_IRQ 81
ISR_IRQ 82
ISR_IRQ 83
ISR_IRQ 84
ISR_IRQ 85
ISR_IRQ 86
ISR_IRQ 87
ISR_IRQ 88
ISR_IRQ 89
ISR_IRQ 90
ISR_IRQ 91
ISR_IRQ 92
ISR_IRQ 93
ISR_IRQ 94
ISR_IRQ 95
ISR_IRQ 96
ISR_IRQ 97
ISR_IRQ 98
ISR_IRQ 99
ISR_IRQ 100
ISR_IRQ 101
ISR_IRQ 102
ISR_IRQ 103
ISR_IRQ 104
ISR_IRQ 105
ISR_IRQ 106
ISR_IRQ 107
ISR_IRQ 108
ISR_IRQ 109
ISR_IRQ 110
ISR_IRQ 111
ISR_IRQ 112
ISR_IRQ 113
ISR_IRQ 114
ISR_IRQ 115
ISR_IRQ 116
ISR_IRQ 117
ISR_IRQ 118
ISR_IRQ 119
ISR_IRQ 120
ISR_IRQ 121
ISR_IRQ 122
ISR_IRQ 123
ISR_IRQ 124
ISR_IRQ 125
ISR_IRQ 126
ISR_IRQ 127
ISR_IRQ 128
ISR_IRQ 129
ISR_IRQ 130
ISR_IRQ 131
ISR_IRQ 132
ISR_IRQ 133
ISR_IRQ 134
ISR_IRQ 135
ISR_IRQ 136
ISR_IRQ 137
ISR_IRQ 138
ISR_IRQ 139
ISR_IRQ 140
ISR_IRQ 141
ISR_IRQ 142
ISR_IRQ 143
ISR_IRQ 144
ISR_IRQ 145
ISR_IRQ 146
ISR_IRQ 147
ISR_IRQ 148
ISR_IRQ 149
ISR_IRQ 150
ISR_IRQ 151
ISR_IRQ 152
ISR_IRQ 153
ISR_IRQ 154
ISR_IRQ 155
ISR_IRQ 156
ISR_IRQ 157
ISR_IRQ 158
ISR_IRQ 159
ISR_IRQ 160
ISR_IRQ 161
ISR_IRQ 162
ISR_IRQ 163
ISR_IRQ 164
ISR_IRQ 165
ISR_IRQ 166
ISR_IRQ 167
ISR_IRQ 168
ISR_IRQ 169
ISR_IRQ 170
ISR_IRQ 171
ISR_IRQ 172
ISR_IRQ 173
ISR_IRQ 174
ISR_IRQ 175
ISR_IRQ 176
ISR_IRQ 177
ISR_IRQ 178
ISR_IRQ 179
ISR_IRQ 180
ISR_IRQ 181
ISR_IRQ 182
ISR_IRQ 183
ISR_IRQ 184
ISR_IRQ 185
ISR_IRQ 186
ISR_IRQ 187
ISR_IRQ 188
ISR_IRQ 189
ISR_IRQ 190
ISR_IRQ 191
ISR_IRQ 192
ISR_IRQ 193
ISR_IRQ 194
ISR_IRQ 195
ISR_IRQ 196
ISR_IRQ 197
ISR_IRQ 198
ISR_IRQ 199
ISR_IRQ 200
ISR_IRQ 201
ISR_IRQ 202
ISR_IRQ 203
ISR_IRQ 204
ISR_IRQ 205
ISR_IRQ 206
ISR_IRQ 207
ISR_IRQ 208
ISR_IRQ 209
ISR_IRQ 210
ISR_IRQ 211
ISR_IRQ 212
ISR_IRQ 213
ISR_IRQ 214
ISR_IRQ 215
ISR_IRQ 216
ISR_IRQ 217
ISR_IRQ 218
ISR_IRQ 219
ISR_IRQ 220
ISR_IRQ 221
ISR_IRQ 222
ISR_IRQ 223
ISR_IRQ 224
ISR_IRQ 225
ISR_IRQ 226
ISR_IRQ 227
ISR_IRQ 228
ISR_IRQ 229
ISR_IRQ 230
ISR_IRQ 231
ISR_IRQ 232
ISR_IRQ 233
ISR_IRQ 234
ISR_IRQ 235
ISR_IRQ 236
ISR_IRQ 237
ISR_IRQ 238
ISR_IRQ 239
ISR_IRQ 240
ISR_IRQ 241
ISR_IRQ 242
ISR_IRQ 243
ISR_IRQ 244
ISR_IRQ 245
ISR_IRQ 246
ISR_IRQ 247

ISR_IRQ 248
ISR_IRQ 249
ISR_IRQ 250
ISR_IRQ 251 # NMI
ISR_IRQ 252 # LINT0
ISR_IRQ 253 # LINT1
ISR_IRQ 254 # LAPIC timer
ISR_IRQ 255 # APIC Spurious interrupt vector