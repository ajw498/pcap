
; Offsets within workspace struct
WRITEPTR	EQU	0
WRITEEND	EQU	4
OVERFLOW	EQU	8
RECHDR		EQU	12
DRIVERS		EQU	28
OLDSWIHANDLER	EQU	32

        AREA    |C$$data|,DATA,REL

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
msg7	DCB	"Filter SWI",0
	ALIGN
msg8	DCB	"RX call",0
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

MBUF_NEXT	EQU	0
MBUF_LIST	EQU	4
MBUF_OFF	EQU	8
MBUF_LEN	EQU	12

; a1 mbuf ptr
; returns length in a1
outputmbuf
	STMFD	sp!, {v1-v2, lr}
	LDR	a2, [a1, #MBUF_OFF]
	ADD	a2, a1, a2
	LDR	a1, [a1, #MBUF_LEN]
	TEQ	a1, #0
	LDMEQFD	sp!, {v1-v2, pc}
	LDR	v1, =|workspace|
	LDR	v1, [v1]
	LDR	a4, [v1, #WRITEPTR]
	LDR	v2, [v1, #WRITEEND]
	ADD	a3, a4, a1
	CMP	a3, v2
	BHS	overflow
	STR	a3, [v1, #WRITEPTR]

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
	STR	a2, [v1, #OVERFLOW]
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
	LDR	a1, [v1, #MBUF_NEXT]
	MOV	v1, a1
	B	chainloop

chainend
	MOV	a1, v2
	LDMFD	sp!, {v1-v2, pc}


	EXPORT	|oldswihandler|
oldswihandler
	DCD	0

	EXPORT	|swihandler|

swihandler
	STMFD	sp!,{r0-r12,lr}
	MRS	r0, CPSR
	STMFD	sp!,{r0}

	LDR	v1, =|workspace|
	LDR	v1, [v1]

	; Load SWI number
	LDR	v2, [r14, #-4]
	BIC	v2, v2, #&FF000000
	BIC	v2, v2, #&00020000 ; X bit


	TEQ	v2, #&6F ; OS_CallASWI
	MOVEQ	v2, r10
	TEQ	v2, #&71 ; OS_CallASWIR12
	MOVEQ	v2, r12

	BIC	v2, v2, #&00020000 ; X bit

	; Get the SWI chunk base
	BIC	a1, v2, #&3F

	LDR	a2, [v1, #DRIVERS]
driverloop
	TEQ	a2, #0
	BEQ	swiexit

	LDR	a3, [a2, #0] ; DIB
	LDR	a4, [a3, #0] ; SWI base
	TEQ	a1, a4
	BEQ	swibasefound

	LDR	a2, [a2, #4] ; Next driver
	B	driverloop

swibasefound
	LDR	a1, [a3, #12] ; Load MAC address from DIB
	AND	a2, v2, #&3F ; Offset within SWI base
	TEQ	a2, #4
	BEQ	txswi
	TEQ	a2, #5
	BEQ	filterswi

	B	swiexit


; Transmit data SWI
; a1 = flags, bit 0 set if v2 is src addr
; a2 = unit number
; a3 = frame type
; a4 = mbuf chains of data
; v1 = dest h/w address
; v2 = src h/w address
txswi
	; Reload original regs
	LDMIB	sp,{v1-v6}

	; If source MAC address not supplied, use the one from the DIB
	TST	v1, #1
	MOV	v6, a1

txlistloop
	MOV	a1, v3 ; frame type
	MOVS	a2, v4 ; mbuf chain
	BEQ	swiexit
	MOV	a3, v5 ; dest addr
	MOV	a4, v6 ; src addr
	BL	outputtxchain
	LDR	v4, [v4, #MBUF_LIST] ; next mbuf chain in list
	B	txlistloop


filterswi
	; Reload original regs
	LDMIB	sp,{r0-r6}

	; Only modify claims
	TST	r0, #1
	BNE	swiexit

	LDR	a1, numclaims
	MOV	a2, a1, LSL#3
	ADD	a1, a1, #1
	STR	a1, numclaims

	LDR	a3, =claims
	ADD	a3, a3, a2

	; Save the original details
	STR	v2, [a3, #0]
	STR	v3, [a3, #4]
	; Poke new details on to stack
	STR	a3, [sp, #6*4]
	ADR	a4, rxcall
	STR	a4, [sp, #7*4]

swiexit
	LDMFD	sp!,{r0}
	MSR	CPSR_cxsf, r0
	LDMFD	sp!,{r0-r12,lr}
	LDR	pc, oldswihandler

; a1 = frame type
; a2 = mbuf chain
; a3 = dest addr
; a4 = src addr
outputtxchain
	STMFD	sp!, {v1-v6, lr}

	LDR	v3, =|workspace|
	LDR	v3, [v3]
	LDR	v4, [v3, #WRITEPTR]
	LDR	v5, [v3, #WRITEEND]
	ADD	v6, v4, #16+14 ; record header + frame header lengths
	CMP	v6, v5
	BHS	txhdroverflow
	STR	v6, [v3, #WRITEPTR]

	MOV	v1, a2
	MOV	v2, a4

	; Frame type
	STRB	a1, [v4, #29]
	MOV	a1, a1, LSR#8
	STRB	a1, [v4, #28]

	; Copy destination address
	ADD	a1, v4, #16
	MOV	a2, a3
	MOV	a3, #6
	BL	memcpy

	; Copy source address
	ADD	a1, v4, #22
	ADR	a2, v2
	MOV	a3, #6
	BL	memcpy

	MOV	a1, v1
	BL	outputmbufchain
	ADD	a1, a1, #14 ; Ethernet header length
	STR	a1, [v3, #20] ; Store length in hdr
	STR	a1, [v3, #24]

	; Copy hdr to output
	MOV	a1, v4
	ADD	a2, v3, #12
	MOV	a3, #16
	BL	memcpy

	LDMFD	sp!, {v1-v6, pc}

txhdroverflow
	MOV	a2, #1
	STR	a2, [v3, #OVERFLOW]
	LDMFD	sp!, {v1-v6, pc}

; rx routine called directly from the DCI4 driver
; a1 = ptr to dib
; a2 = ptr to mbuf chains
; r12 = private word
rxcall
	STMFD	sp!, {r0-r12,lr,pc}
	MOV	v1, a2

	LDR	a1, [r12, #0] ; New r12
	STR	a1, [sp, #12*4] ; Poke onto stack
	LDR	a2, [r12, #4] ; New pc
	STR	a2, [sp, #14*4] ; Poke onto stack

rxlistloop
	MOVS	a1, v1
	BEQ	rxexit
	LDR	v1, [v1, #MBUF_LIST] ; next mbuf chain in list
	BL	outputrxchain
	B	rxlistloop

rxexit
	LDMFD	sp!, {r0-r12, lr, pc}


; a1 = ptr to mbuf chain
outputrxchain
	STMFD	sp!, {v1-v6, lr}
	MOV	v1, a1

	LDR	v3, =|workspace|
	LDR	v3, [v3]
	LDR	v4, [v3, #WRITEPTR]
	LDR	v5, [v3, #WRITEEND]
	ADD	v6, v4, #16+14 ; record header + frame header lengths
	CMP	v6, v5
	BHS	rxhdroverflow
	STR	v6, [v3, #WRITEPTR]

	LDR	v2, [v1, #MBUF_OFF] ; mbuf offset
	ADD	v2, v1, v2
	; v2 = rx header
	; +8 src addr
	; +16 dest addr
	; +24 frame type

	; Copy destination address
	ADD	a1, v4, #16
	ADD	a2, v2, #16
	MOV	a3, #6
	BL	memcpy

	; Copy source address
	ADD	a1, v4, #22
	ADD	a2, v2, #8
	MOV	a3, #6
	BL	memcpy

	; Frame type
	LDR	a3, [v2, #24]
	STRB	a3, [v4, #29]
	LDR	a3, [v2, #25]
	STRB	a3, [v4, #28]

	LDR	a1, [v1, #MBUF_NEXT] ; Next mbuf in chain, contains the payload
	BL	outputmbufchain
	ADD	a1, a1, #14 ; Ethernet header length
	STR	a1, [v3, #20] ; Store length in pcap record hdr
	STR	a1, [v3, #24]

	; Copy pcap record hdr to output
	MOV	a1, v4
	ADD	a2, v3, #12
	MOV	a3, #16
	BL	memcpy

	LDMFD	sp!, {v1-v6, pc}

rxhdroverflow
	MOV	a2, #1
	STR	a2, [v3, #OVERFLOW]
	LDMFD	sp!, {v1-v6, pc}


numclaims
	DCD	0
claims
	DCD	0
	DCD	0
	DCD	0
	DCD	0
	DCD	0
	DCD	0
	DCD	0
	DCD	0
	DCD	0
	DCD	0


        END
