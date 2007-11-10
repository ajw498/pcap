

        AREA    |C$$data|,DATA,REL

errbuf	DCD	&804d38
errmess	%	256

workspace	DCD	0

        AREA    |C$$code|,CODE,REL,READONLY

	EXPORT	|semiprint|
semiprint
	STMFD	r13!,{lr}
	SWI	&56ac5
	LDMFD	r13!, {pc}

	EXPORT	|claimswi|
claimswi
	STMFD	r13!,{lr}
	LDR	r1, =|workspace|
	STR	r0, [r1]
	MOV	r0, #2
	ORR	r0, r0, #&100
	ADR	r1, swihandler
	SWI	&69+&20000 ;XOS_ClaimProcessorVector
	MOVVC	r0, #0
	STR	r1, oldswihandler
	LDMFD	r13!, {pc}

	EXPORT	|releaseswi|
releaseswi
	STMFD	r13!,{lr}
	MOV	r0, #2
	LDR	r1, oldswihandler
	ADR	r2, swihandler
	SWI	&69+&20000 ;XOS_ClaimProcessorVector
	MOVVC	r0, #0
	LDMFD	r13!, {pc}

msg1	DCB	"Msg 1",0
	ALIGN
msg2	DCB	"Msg 2",0
	ALIGN
msg3	DCB	"Msg 3",0
	ALIGN
msg4	DCB	"Msg 4",0
	ALIGN
msg5	DCB	"Msg 5",0
	ALIGN
msg6	DCB	"Msg 6",0
	ALIGN

	; a1 = dest
	; a2 = src
	; a3 = len (assumes len > 0)
memcpy
	LDRB	a4, [a2], #1
	SUBS	a3, a3, #1
	STRB	a4, [a1], #1
	BGT	memcpy
	MOV	pc, lr

;struct mbuf {
;    uint32_t m_next;
;    uint32_t m_list;
;    uint32_t m_off;
;    uint32_t m_len;

; a1 mbuf ptr
; returns length in a1
outputmbuf
	STMFD	sp!, {v1-v2, lr}
	LDR	a2, [a1, #8] ; Offset
	ADD	a2, a1, a2
	LDR	a1, [a1, #12] ; Length
	TEQ	a1, #0
	LDMEQFD	sp!, {v1-v2, pc}
	LDR	v1, =|workspace|
	LDR	v1, [v1]
	LDR	a4, [v1, #0] ; writeptr
	LDR	v2, [v1, #4] ; writeend
	ADD	a3, a4, a1
	CMP	a3, v2
	BHS	overflow
	STR	a3, [v1, #0] ; new writeptr

	; a2 = src
	; a1 = len
	; a4 = dest
	MOV	a3, a1
;	SWI	&56ac8
copyloop
	LDRB	v1, [a2], #1
	SUBS	a3, a3, #1
	STRB	v1, [a4], #1
	BGT	copyloop

	LDMFD	sp!, {v1-v2, pc}

overflow
	MOV	a2, #1
	STR	a2, [v1, #8] ; overflow
	MOV	a1, #0
	LDMFD	sp!, {v1-v2, pc}

; a1 mbuf ptr
; Returns length in a1
outputmbufchain
	STMFD	sp!, {v1-v2, lr}
	MOV	v1, a1
	MOV	v2, #0 ; Total length
chainloop
	TEQ	a1, #0
	BEQ	chainend
	BL	outputmbuf
	ADD	v2, v2, a1
	LDR	a1, [v1, #0]
	MOV	v1, a1
	B	chainloop

chainend
	MOV	a1, v2
	LDMFD	sp!, {v1-v2, pc}

dummymac
	DCB	10,11,12,13,14,15
	ALIGN

	EXPORT	|oldswihandler|
oldswihandler
	DCD	0

	EXPORT	|swihandler|

	IMPORT	|swilist|
	IMPORT	|numswis|
swihandler
;	SWI	&56ac7
	STMFD	sp!,{r0-r12,lr}
	MRS	r0, CPSR
	STMFD	sp!,{r0}
	LDR	r0, [r14, #-4]
;	LDR	r1, [r14, #-4]
;	LDR	r2, [r14, #0]
;	MOV	r3, r14


;	LDR	r2, =|numswis|
;	LDR	r1, =|swilist|
;	LDR	r2, [r2]
	BIC	r0, r0, #&FF000000
	BIC	r0, r0, #&00020000 ; X bit
;	BIC	r4, r0, #&3F


	TEQ	r0, #&6F ; OS_CallASWI
	MOVEQ	r0, r10
	TEQ	r0, #&71 ; OS_CallASWIR12
	MOVEQ	r0, r12

	BIC	r0, r0, #&00020000 ; X bit

	LDR	r5, =&55484
	TEQ	r0, r5
;	SWI	&56ac8
	BNE	exit

;loop
;;	SWI	&56ac8
;	SUBS	r2, r2, #1
;	BLT	exit
;	LDR	r3, [r1], #4
;	SWI	&56ac8
;	TEQ	r3, r4
;	BNE	loop
;	; Found match
;	; copy tx data from mbufs
	ADR	r0, msg1
	SWI	&56ac5

	LDR	v3, =|workspace|
	LDR	v3, [v3]
	LDR	v4, [v3, #0] ; writeptr
	LDR	v5, [v3, #4] ; writeend
	ADD	v6, v4, #16+14 ; record header + frame header lengths
	CMP	v6, v5
	BHS	hdroverflow
	STR	v6, [v3, #0] ; writeptr

	; Preamble
;	MOV	a1, #0
;	STRB	a1, [v4, #16]
;	STRB	a1, [v4, #17]
;	STRB	a1, [v4, #18]
;	STRB	a1, [v4, #19]

	; Reload original regs
	LDMIB	sp,{r0-r5}

	; Copy destination address
	ADD	a1, v4, #16
	MOV	a2, r4
	MOV	a3, #6
	BL	memcpy

	ADR	r0, msg2
	SWI	&56ac5

	; Copy source address
	ADD	a1, v4, #22
	ADR	a2, dummymac
	MOV	a3, #6
	BL	memcpy

	ADR	r0, msg3
	SWI	&56ac5

	; Reload original regs
	LDMIB	sp,{r0-r5}

	; Frame type
	STRB	a3, [v4, #29]
	MOV	a3, a3, LSR#8
	STRB	a3, [v4, #28]

	ADR	r0, msg4
	SWI	&56ac5

	; Reload original regs
	LDMIB	sp,{r0-r5}

	MOV	a1, r3
	BL	outputmbufchain
	ADD	a1, a1, #14 ; Ethernet header length
	STR	a1, [v3, #20] ; Store length in hdr
	STR	a1, [v3, #24]

	ADR	r0, msg5
	SWI	&56ac5

	; Copy hdr to output
	MOV	a1, v4
	ADD	a2, v3, #12
	MOV	a3, #16
	BL	memcpy

	ADR	r0, msg6
	SWI	&56ac5

	B	exit

hdroverflow
	MOV	a2, #1
	STR	a2, [v3, #8] ; overflow

exit
	LDMFD	sp!,{r0}
	MSR	CPSR_cxsf, r0
	LDMFD	sp!,{r0-r12,lr}
;	SWI	&56ac6
	LDR	pc, oldswihandler


        END
