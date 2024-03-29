;*******************************************************************************
;   Code for application report - "DCO Library"
;*******************************************************************************
; THIS PROGRAM IS PROVIDED "AS IS". TI MAKES NO WARRANTIES OR
; REPRESENTATIONS, EITHER EXPRESS, IMPLIED OR STATUTORY,
; INCLUDING ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
; FOR A PARTICULAR PURPOSE, LACK OF VIRUSES, ACCURACY OR
; COMPLETENESS OF RESPONSES, RESULTS AND LACK OF NEGLIGENCE.
; TI DISCLAIMS ANY WARRANTY OF TITLE, QUIET ENJOYMENT, QUIET
; POSSESSION, AND NON-INFRINGEMENT OF ANY THIRD PARTY
; INTELLECTUAL PROPERTY RIGHTS WITH REGARD TO THE PROGRAM OR
; YOUR USE OF THE PROGRAM.
;
; IN NO EVENT SHALL TI BE LIABLE FOR ANY SPECIAL, INCIDENTAL,
; CONSEQUENTIAL OR INDIRECT DAMAGES, HOWEVER CAUSED, ON ANY
; THEORY OF LIABILITY AND WHETHER OR NOT TI HAS BEEN ADVISED
; OF THE POSSIBILITY OF SUCH DAMAGES, ARISING IN ANY WAY OUT
; OF THIS AGREEMENT, THE PROGRAM, OR YOUR USE OF THE PROGRAM.
; EXCLUDED DAMAGES INCLUDE, BUT ARE NOT LIMITED TO, COST OF
; REMOVAL OR REINSTALLATION, COMPUTER TIME, LABOR COSTS, LOSS
; OF GOODWILL, LOSS OF PROFITS, LOSS OF SAVINGS, OR LOSS OF
; USE OR INTERRUPTION OF BUSINESS. IN NO EVENT WILL TI'S
; AGGREGATE LIABILITY UNDER THIS AGREEMENT OR ARISING OUT OF
; YOUR USE OF THE PROGRAM EXCEED FIVE HUNDRED DOLLARS
; (U.S.$500).
;
; Unless otherwise stated, the Program written and copyrighted
; by Texas Instruments is distributed as "freeware".  You may,
; only under TI's copyright in the Program, use and modify the
; Program without any charge or restriction.  You may
; distribute to third parties, provided that you transfer a
; copy of this license to the third party and the third party
; agrees to these terms by its first use of the Program. You
; must reproduce the copyright notice and any other legend of
; ownership on each copy or partial copy, of the Program.
;
; You acknowledge and agree that the Program contains
; copyrighted material, trade secrets and other TI proprietary
; information and is protected by copyright laws,
; international copyright treaties, and trade secret laws, as
; well as other intellectual property laws.  To protect TI's
; rights in the Program, you agree not to decompile, reverse
; engineer, disassemble or otherwise translate any object code
; versions of the Program to a human-readable form.  You agree
; that in no event will you alter, remove or destroy any
; copyright notice included in the Program.  TI reserves all
; rights not specifically granted under this license. Except
; as specifically provided herein, nothing in this agreement
; shall be construed as conferring by implication, estoppel,
; or otherwise, upon you, any license or other right under any
; TI patents, copyrights or trade secrets.
;
; You may not use the Program in non-TI devices.
;
;*******************************************************************************
;   DCO Library
;
;   Description; Allows the DCO to be calibrated to a multiple of ACLK
;
;
;   Memory Usage:               <184 bytes
;
;   L. Westlund
;   Version    1.1
;   Texas Instruments, Inc
;   August 2007
;   Built with Code Composer Essentials 2.0
;*******************************************************************************
;Change log
;
;1.0 - Inital version - L.Westlund
;1.1 - Misc. updates - A.Dannenberg
;      * Fixed PUSH/POP inconsistency
;*******************************************************************************
 .cdecls C,LIST,    "msp430g2231.h"

            ;Functions
            .def      TI_SetDCO
            .align 2
            .text
; CCE requires you change the header file, as well as the variable below
; uncomment the line below which coresponds to the device you are using
;DEVICE_TYPE .set 1                          ; All 1xx devices
;DEVICE_TYPE .set 2                          ; 2xx devices with a Timer_A3
DEVICE_TYPE .set 3                          ; 2xx devices with a Timer_A2

 .if DEVICE_TYPE = 3                        ; For Timer_A2 devices
TACCTLX .set TACCTL0
TACCRX .set TACCR0
 .else
TACCTLX .set TACCTL2
TACCRX .set TACCR2
 .endif

;-------------------------------------------------------------------------------
TI_SetDCO;     Subroutine: Sets DCO to selected frequency based on Delta.
;           R14 and R15 are used, ACLK = 32768/8 Timer_A clocked by DCOCLK
;           R13 used to hold old BCSCTL1 value
;           R12 is the parameter specifying the delta value, MCLK = delta * 4096
;           returns in R12:
;           0 - DCO Set Correctly
;           1 - DCO running at slowest setting
;           2 - DCO running at fastest setting
;           0xff - Timeout Occured while setting DCO
;-------------------------------------------------------------------------------
            push.w  r10
            push.w  &TACTL
            push.w  &TACCTLX
            mov.w   #10000, r11             ; max number of times to loop
            clr.w   r15                     ;		
            mov.b   &BCSCTL1, r13           ; Copy BC settings to preserve
            .if DEVICE_TYPE = 1
            and.w   #0xF8,    r13           ; Mask out bits which will change (0-2) 1xx
            .else
            and.w   #0xF0,    r13           ; Mask out bits which will change (0-3) 2xx
            .endif
SetupBC     bis.b   #XT2OFF+DIVA_3,&BCSCTL1 ; ACLK=LFXT1CLK/8, XT2OFF to make a known state			
SetupCC2    mov.w   #CM_1+CCIS_1+CAP,&TACCTLX ; CAP, ACLK
            mov.w   #TASSEL_2+MC_2+TACLR, &TACTL ; SMCLK, cont-mode, clear
firstTest   bit.w   #CCIFG,&TACCTLX         ; Test capture flag, skip first short ACLK to avoid error
            jz      firstTest               ;
            bic.w   #CCIFG,&TACCTLX         ; Clear capture flag
TestDCO     dec.w   r11
            jz      ExitTimeOutError        ;
TestDCO2    bit.w   #CCIFG,&TACCTLX         ; Test capture flag
            jz      TestDCO2                ;
            bic.w   #CCIFG,&TACCTLX         ; Clear capture flag
                                            ;
AdjDCO      mov.w   &TACCRX,r14             ; R14 = captured SMCLK
            sub.w   r15,r14                 ; R14 = capture difference
            mov.w   &TACCRX,r15             ; R15 = captured SMCLK
            mov.w   r14, r10                ; r10 = capture difference
            sub.w   r12, r10                ; r10 = difference between desired and actual
            jz      ExitNoError             ; if r12 = r10, DCO set exactly
            cmp.w   #0xFFFF, r10            ; DCO can be off by 1 for a tolerance
            jeq     ExitNoError
            cmp.w   r12,   r14              ; Delta = SMCLK/(32768/8)
            jlo     IncDCO                  ;
DecDCO      dec.b   &DCOCTL                 ; Slow DCO with DCO and MOD
            jc      TestDCO                 ; Slower?
            .if DEVICE_TYPE = 1
            bit.b   #0x7, &BCSCTL1          ; Can RSEL.x be decremented? 1xx
            .else
            bit.b   #0xF, &BCSCTL1          ; Can RSEL.x be decremented? 2xx
            .endif
            jz      ExitSlowestSetting      ; jmp>DCO at slowest setting
            dec.b   &BCSCTL1                ; Decrement RSEL.x
            jmp     TestDCO                 ;
IncDCO
            .if DEVICE_TYPE = 1
            inc.b   &DCOCTL                 ; Speed DCO with DCO and MOD
            jnc     TestDCO                 ; Faster?
            cmp.b   #XT2OFF+DIVA_3+0x7, &BCSCTL1 ; Can RSEL.x be increased? 1xx
            jeq     ExitFastestSetting      ; jmp> DCO at fastest setting
            inc.b   &BCSCTL1                ; Increment RSEL.x
            jmp     TestDCO                 ;
            .else
            inc.b   &DCOCTL                 ; Speed DCO with DCO and MOD
            jnc     TestFor16MHz
            cmp.b   #XT2OFF+DIVA_3+0xF, &BCSCTL1 ; Can RSEL.x be increased? 2xx
            jeq     ExitFastestSetting      ; jmp> DCO at fastest setting
            inc.b   &BCSCTL1                ; Increment RSEL.x
            jmp     TestDCO                 ;
            ;-------Compare to built in values
TestFor16MHz

;            cmp.b   &CALDCO_16MHZ, &DCOCTL  ; compare current DCO to built in DCO constants
;            jne     TestDCO                 ; if they are not equal, leave
;            mov.b   &CALBC1_16MHZ, r14      ; move cal values into R14 for manipulation
;            bis.b   #XT2OFF+DIVA_3, r14     ; make sure BCSCTL1 cal has same state
;            cmp.b   r14, &BCSCTL1           ; if they are the same
;            jeq     ExitFastestSetting2xx   ; leave all values in place and exit
            jmp     TestDCO
            ;-------Compare to built in values
            .endif
ExitSlowestSetting
            clr.b   &DCOCTL
            mov.w   #0x01, r12
            jmp     SetDCO_Exit
ExitFastestSetting
            mov.b   #0xFF, &DCOCTL          ; since the DCO previously rolled over
ExitFastestSetting2xx
            mov.w   #0x02, r12
            jmp     SetDCO_Exit
ExitTimeOutError
            mov.w   #0xff, r12
            jmp     SetDCO_Exit
ExitNoError clr.w   r12
SetDCO_Exit clr.w   &TACCTLX                ; Stop CCR2
            .if DEVICE_TYPE = 1
            bic.b   #0xF8,    &BCSCTL1      ; Mask out bits which will change (0-2) 1xx
            .else
            bic.b   #0xF0,    &BCSCTL1      ; Mask out bits which will change (0-3) 2xx
            .endif
            bis.b   r13, &BCSCTL1           ; copy in old non-DCO BC settings
            pop.w   &TACCTLX
            pop.w   &TACTL
            pop.w   r10
            ret                             ; Return from subroutine
                                            ;
            .end