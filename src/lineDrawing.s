.segment "BANKRAM08"
b8LineTable: .res PICTURE_HEIGHT * 2
.proc b8SetupLineTable 
    ; each entry is a 16 bit value
    stz b8LineTable
    stz b8LineTable+1
    ldx #0
    ; load the previous value
    lda b8LineTable+1,x
    tay
    lda b8LineTable,x
@loop1:
    clc
    inc ZP_TMP_10
    inx
    inx
    adc #<LINE_LENGTH   ; add LINE_LENGTH
    sta b8LineTable,x
    tya
    adc #>LINE_LENGTH   ; add carry
    sta b8LineTable+1,x
    tay                 ; load previous value for next iteration
    lda b8LineTable,x
    cpx #PICTURE_HEIGHT - 2          ; did we reach 168 lines? 
    bcc @loop1
    ldx #0
@loop2:
    clc
    inc ZP_TMP_12
    inx
    inx
    adc #<LINE_LENGTH   ; add LINE_LENGTH
    sta b8LineTable+PICTURE_HEIGHT,x
    tya
    adc #>LINE_LENGTH   ; add carry
    sta b8LineTable+PICTURE_HEIGHT + 1,x
    tay                 ; load previous value for next iteration
    lda b8LineTable+PICTURE_HEIGHT,x
    cpx #PICTURE_HEIGHT - 2            ; did we reach 168 lines?
    bcc @loop2
lda b8LineTable
rts

.endproc ; s