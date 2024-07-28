# file: isr_software.s

.extern value1

.section isr
# prekidna rutina za softverski prekid
.global isr_software
isr_software:
    push %r1
    push %r2
    ld $0xABCD, %r1
    ld $value1, %r2
    st %r1, [%r2]
    pop %r2
    pop %r1
    ret

.end
