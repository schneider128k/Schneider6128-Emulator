; WPS-Z80 LESSON 4: LOGIC & FLAGS
; Generated for Milestone 4 Testing
; -----------------------------------------
ORG 0000h

START:
    LD A, 0Ah       ; Load 10 into Accumulator
    CP 0Ah          ; Compare A with 10 (Expect Z=1)
    
    LD B, 05h       ; Load 5 into B
    DEC B           ; Decrement B (4 != 0, Expect Z=0)
    
    LD A, 02h       ; Load 2 into A
    CP 05h          ; Compare A with 5 (2 < 5, Expect C=1)
    
    HALT            ; End of Test
