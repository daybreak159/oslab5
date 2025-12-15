
obj/__user_cowtest.out:     file format elf64-littleriscv


Disassembly of section .text:

0000000000800020 <_start>:
.text
.globl _start
_start:
    # call user-program function
    call umain
  800020:	0c8000ef          	jal	ra,8000e8 <umain>
1:  j 1b
  800024:	a001                	j	800024 <_start+0x4>

0000000000800026 <cputch>:
/* *
 * cputch - writes a single character @c to stdout, and it will
 * increace the value of counter pointed by @cnt.
 * */
static void
cputch(int c, int *cnt) {
  800026:	1141                	addi	sp,sp,-16
  800028:	e022                	sd	s0,0(sp)
  80002a:	e406                	sd	ra,8(sp)
  80002c:	842e                	mv	s0,a1
    sys_putc(c);
  80002e:	098000ef          	jal	ra,8000c6 <sys_putc>
    (*cnt) ++;
  800032:	401c                	lw	a5,0(s0)
}
  800034:	60a2                	ld	ra,8(sp)
    (*cnt) ++;
  800036:	2785                	addiw	a5,a5,1
  800038:	c01c                	sw	a5,0(s0)
}
  80003a:	6402                	ld	s0,0(sp)
  80003c:	0141                	addi	sp,sp,16
  80003e:	8082                	ret

0000000000800040 <cprintf>:
 *
 * The return value is the number of characters which would be
 * written to stdout.
 * */
int
cprintf(const char *fmt, ...) {
  800040:	711d                	addi	sp,sp,-96
    va_list ap;

    va_start(ap, fmt);
  800042:	02810313          	addi	t1,sp,40
cprintf(const char *fmt, ...) {
  800046:	8e2a                	mv	t3,a0
  800048:	f42e                	sd	a1,40(sp)
  80004a:	f832                	sd	a2,48(sp)
  80004c:	fc36                	sd	a3,56(sp)
    vprintfmt((void*)cputch, &cnt, fmt, ap);
  80004e:	00000517          	auipc	a0,0x0
  800052:	fd850513          	addi	a0,a0,-40 # 800026 <cputch>
  800056:	004c                	addi	a1,sp,4
  800058:	869a                	mv	a3,t1
  80005a:	8672                	mv	a2,t3
cprintf(const char *fmt, ...) {
  80005c:	ec06                	sd	ra,24(sp)
  80005e:	e0ba                	sd	a4,64(sp)
  800060:	e4be                	sd	a5,72(sp)
  800062:	e8c2                	sd	a6,80(sp)
  800064:	ecc6                	sd	a7,88(sp)
    va_start(ap, fmt);
  800066:	e41a                	sd	t1,8(sp)
    int cnt = 0;
  800068:	c202                	sw	zero,4(sp)
    vprintfmt((void*)cputch, &cnt, fmt, ap);
  80006a:	0f6000ef          	jal	ra,800160 <vprintfmt>
    int cnt = vcprintf(fmt, ap);
    va_end(ap);

    return cnt;
}
  80006e:	60e2                	ld	ra,24(sp)
  800070:	4512                	lw	a0,4(sp)
  800072:	6125                	addi	sp,sp,96
  800074:	8082                	ret

0000000000800076 <syscall>:
#include <syscall.h>

#define MAX_ARGS            5

static inline int
syscall(int64_t num, ...) {
  800076:	7175                	addi	sp,sp,-144
  800078:	f8ba                	sd	a4,112(sp)
    va_list ap;
    va_start(ap, num);
    uint64_t a[MAX_ARGS];
    int i, ret;
    for (i = 0; i < MAX_ARGS; i ++) {
        a[i] = va_arg(ap, uint64_t);
  80007a:	e0ba                	sd	a4,64(sp)
  80007c:	0118                	addi	a4,sp,128
syscall(int64_t num, ...) {
  80007e:	e42a                	sd	a0,8(sp)
  800080:	ecae                	sd	a1,88(sp)
  800082:	f0b2                	sd	a2,96(sp)
  800084:	f4b6                	sd	a3,104(sp)
  800086:	fcbe                	sd	a5,120(sp)
  800088:	e142                	sd	a6,128(sp)
  80008a:	e546                	sd	a7,136(sp)
        a[i] = va_arg(ap, uint64_t);
  80008c:	f42e                	sd	a1,40(sp)
  80008e:	f832                	sd	a2,48(sp)
  800090:	fc36                	sd	a3,56(sp)
  800092:	f03a                	sd	a4,32(sp)
  800094:	e4be                	sd	a5,72(sp)
    }
    va_end(ap);

    asm volatile (
  800096:	6522                	ld	a0,8(sp)
  800098:	75a2                	ld	a1,40(sp)
  80009a:	7642                	ld	a2,48(sp)
  80009c:	76e2                	ld	a3,56(sp)
  80009e:	6706                	ld	a4,64(sp)
  8000a0:	67a6                	ld	a5,72(sp)
  8000a2:	00000073          	ecall
  8000a6:	00a13e23          	sd	a0,28(sp)
        "sd a0, %0"
        : "=m" (ret)
        : "m"(num), "m"(a[0]), "m"(a[1]), "m"(a[2]), "m"(a[3]), "m"(a[4])
        :"memory");
    return ret;
}
  8000aa:	4572                	lw	a0,28(sp)
  8000ac:	6149                	addi	sp,sp,144
  8000ae:	8082                	ret

00000000008000b0 <sys_exit>:

int
sys_exit(int64_t error_code) {
  8000b0:	85aa                	mv	a1,a0
    return syscall(SYS_exit, error_code);
  8000b2:	4505                	li	a0,1
  8000b4:	b7c9                	j	800076 <syscall>

00000000008000b6 <sys_fork>:
}

int
sys_fork(void) {
    return syscall(SYS_fork);
  8000b6:	4509                	li	a0,2
  8000b8:	bf7d                	j	800076 <syscall>

00000000008000ba <sys_wait>:
}

int
sys_wait(int64_t pid, int *store) {
  8000ba:	862e                	mv	a2,a1
    return syscall(SYS_wait, pid, store);
  8000bc:	85aa                	mv	a1,a0
  8000be:	450d                	li	a0,3
  8000c0:	bf5d                	j	800076 <syscall>

00000000008000c2 <sys_getpid>:
    return syscall(SYS_kill, pid);
}

int
sys_getpid(void) {
    return syscall(SYS_getpid);
  8000c2:	4549                	li	a0,18
  8000c4:	bf4d                	j	800076 <syscall>

00000000008000c6 <sys_putc>:
}

int
sys_putc(int64_t c) {
  8000c6:	85aa                	mv	a1,a0
    return syscall(SYS_putc, c);
  8000c8:	4579                	li	a0,30
  8000ca:	b775                	j	800076 <syscall>

00000000008000cc <exit>:
#include <syscall.h>
#include <stdio.h>
#include <ulib.h>

void
exit(int error_code) {
  8000cc:	1141                	addi	sp,sp,-16
  8000ce:	e406                	sd	ra,8(sp)
    sys_exit(error_code);
  8000d0:	fe1ff0ef          	jal	ra,8000b0 <sys_exit>
    cprintf("BUG: exit failed.\n");
  8000d4:	00000517          	auipc	a0,0x0
  8000d8:	5a450513          	addi	a0,a0,1444 # 800678 <main+0x17a>
  8000dc:	f65ff0ef          	jal	ra,800040 <cprintf>
    while (1);
  8000e0:	a001                	j	8000e0 <exit+0x14>

00000000008000e2 <fork>:
}

int
fork(void) {
    return sys_fork();
  8000e2:	bfd1                	j	8000b6 <sys_fork>

00000000008000e4 <waitpid>:
    return sys_wait(0, NULL);
}

int
waitpid(int pid, int *store) {
    return sys_wait(pid, store);
  8000e4:	bfd9                	j	8000ba <sys_wait>

00000000008000e6 <getpid>:
    return sys_kill(pid);
}

int
getpid(void) {
    return sys_getpid();
  8000e6:	bff1                	j	8000c2 <sys_getpid>

00000000008000e8 <umain>:
#include <ulib.h>

int main(void);

void
umain(void) {
  8000e8:	1141                	addi	sp,sp,-16
  8000ea:	e406                	sd	ra,8(sp)
    int ret = main();
  8000ec:	412000ef          	jal	ra,8004fe <main>
    exit(ret);
  8000f0:	fddff0ef          	jal	ra,8000cc <exit>

00000000008000f4 <printnum>:
 * */
static void
printnum(void (*putch)(int, void*), void *putdat,
        unsigned long long num, unsigned base, int width, int padc) {
    unsigned long long result = num;
    unsigned mod = do_div(result, base);
  8000f4:	02069813          	slli	a6,a3,0x20
        unsigned long long num, unsigned base, int width, int padc) {
  8000f8:	7179                	addi	sp,sp,-48
    unsigned mod = do_div(result, base);
  8000fa:	02085813          	srli	a6,a6,0x20
        unsigned long long num, unsigned base, int width, int padc) {
  8000fe:	e052                	sd	s4,0(sp)
    unsigned mod = do_div(result, base);
  800100:	03067a33          	remu	s4,a2,a6
        unsigned long long num, unsigned base, int width, int padc) {
  800104:	f022                	sd	s0,32(sp)
  800106:	ec26                	sd	s1,24(sp)
  800108:	e84a                	sd	s2,16(sp)
  80010a:	f406                	sd	ra,40(sp)
  80010c:	e44e                	sd	s3,8(sp)
  80010e:	84aa                	mv	s1,a0
  800110:	892e                	mv	s2,a1
    // first recursively print all preceding (more significant) digits
    if (num >= base) {
        printnum(putch, putdat, result, base, width - 1, padc);
    } else {
        // print any needed pad characters before first digit
        while (-- width > 0)
  800112:	fff7041b          	addiw	s0,a4,-1
    unsigned mod = do_div(result, base);
  800116:	2a01                	sext.w	s4,s4
    if (num >= base) {
  800118:	03067e63          	bgeu	a2,a6,800154 <printnum+0x60>
  80011c:	89be                	mv	s3,a5
        while (-- width > 0)
  80011e:	00805763          	blez	s0,80012c <printnum+0x38>
  800122:	347d                	addiw	s0,s0,-1
            putch(padc, putdat);
  800124:	85ca                	mv	a1,s2
  800126:	854e                	mv	a0,s3
  800128:	9482                	jalr	s1
        while (-- width > 0)
  80012a:	fc65                	bnez	s0,800122 <printnum+0x2e>
    }
    // then print this (the least significant) digit
    putch("0123456789abcdef"[mod], putdat);
  80012c:	1a02                	slli	s4,s4,0x20
  80012e:	00000797          	auipc	a5,0x0
  800132:	56278793          	addi	a5,a5,1378 # 800690 <main+0x192>
  800136:	020a5a13          	srli	s4,s4,0x20
  80013a:	9a3e                	add	s4,s4,a5
    // Crashes if num >= base. No idea what going on here
    // Here is a quick fix
    // update: Stack grows downward and destory the SBI
    // sbi_console_putchar("0123456789abcdef"[mod]);
    // (*(int *)putdat)++;
}
  80013c:	7402                	ld	s0,32(sp)
    putch("0123456789abcdef"[mod], putdat);
  80013e:	000a4503          	lbu	a0,0(s4)
}
  800142:	70a2                	ld	ra,40(sp)
  800144:	69a2                	ld	s3,8(sp)
  800146:	6a02                	ld	s4,0(sp)
    putch("0123456789abcdef"[mod], putdat);
  800148:	85ca                	mv	a1,s2
  80014a:	87a6                	mv	a5,s1
}
  80014c:	6942                	ld	s2,16(sp)
  80014e:	64e2                	ld	s1,24(sp)
  800150:	6145                	addi	sp,sp,48
    putch("0123456789abcdef"[mod], putdat);
  800152:	8782                	jr	a5
        printnum(putch, putdat, result, base, width - 1, padc);
  800154:	03065633          	divu	a2,a2,a6
  800158:	8722                	mv	a4,s0
  80015a:	f9bff0ef          	jal	ra,8000f4 <printnum>
  80015e:	b7f9                	j	80012c <printnum+0x38>

0000000000800160 <vprintfmt>:
 *
 * Call this function if you are already dealing with a va_list.
 * Or you probably want printfmt() instead.
 * */
void
vprintfmt(void (*putch)(int, void*), void *putdat, const char *fmt, va_list ap) {
  800160:	7119                	addi	sp,sp,-128
  800162:	f4a6                	sd	s1,104(sp)
  800164:	f0ca                	sd	s2,96(sp)
  800166:	ecce                	sd	s3,88(sp)
  800168:	e8d2                	sd	s4,80(sp)
  80016a:	e4d6                	sd	s5,72(sp)
  80016c:	e0da                	sd	s6,64(sp)
  80016e:	fc5e                	sd	s7,56(sp)
  800170:	f06a                	sd	s10,32(sp)
  800172:	fc86                	sd	ra,120(sp)
  800174:	f8a2                	sd	s0,112(sp)
  800176:	f862                	sd	s8,48(sp)
  800178:	f466                	sd	s9,40(sp)
  80017a:	ec6e                	sd	s11,24(sp)
  80017c:	892a                	mv	s2,a0
  80017e:	84ae                	mv	s1,a1
  800180:	8d32                	mv	s10,a2
  800182:	8a36                	mv	s4,a3
    register int ch, err;
    unsigned long long num;
    int base, width, precision, lflag, altflag;

    while (1) {
        while ((ch = *(unsigned char *)fmt ++) != '%') {
  800184:	02500993          	li	s3,37
            putch(ch, putdat);
        }

        // Process a %-escape sequence
        char padc = ' ';
        width = precision = -1;
  800188:	5b7d                	li	s6,-1
  80018a:	00000a97          	auipc	s5,0x0
  80018e:	53aa8a93          	addi	s5,s5,1338 # 8006c4 <main+0x1c6>
        case 'e':
            err = va_arg(ap, int);
            if (err < 0) {
                err = -err;
            }
            if (err > MAXERROR || (p = error_string[err]) == NULL) {
  800192:	00000b97          	auipc	s7,0x0
  800196:	74eb8b93          	addi	s7,s7,1870 # 8008e0 <error_string>
        while ((ch = *(unsigned char *)fmt ++) != '%') {
  80019a:	000d4503          	lbu	a0,0(s10)
  80019e:	001d0413          	addi	s0,s10,1
  8001a2:	01350a63          	beq	a0,s3,8001b6 <vprintfmt+0x56>
            if (ch == '\0') {
  8001a6:	c121                	beqz	a0,8001e6 <vprintfmt+0x86>
            putch(ch, putdat);
  8001a8:	85a6                	mv	a1,s1
        while ((ch = *(unsigned char *)fmt ++) != '%') {
  8001aa:	0405                	addi	s0,s0,1
            putch(ch, putdat);
  8001ac:	9902                	jalr	s2
        while ((ch = *(unsigned char *)fmt ++) != '%') {
  8001ae:	fff44503          	lbu	a0,-1(s0)
  8001b2:	ff351ae3          	bne	a0,s3,8001a6 <vprintfmt+0x46>
        switch (ch = *(unsigned char *)fmt ++) {
  8001b6:	00044603          	lbu	a2,0(s0)
        char padc = ' ';
  8001ba:	02000793          	li	a5,32
        lflag = altflag = 0;
  8001be:	4c81                	li	s9,0
  8001c0:	4881                	li	a7,0
        width = precision = -1;
  8001c2:	5c7d                	li	s8,-1
  8001c4:	5dfd                	li	s11,-1
  8001c6:	05500513          	li	a0,85
                if (ch < '0' || ch > '9') {
  8001ca:	4825                	li	a6,9
        switch (ch = *(unsigned char *)fmt ++) {
  8001cc:	fdd6059b          	addiw	a1,a2,-35
  8001d0:	0ff5f593          	zext.b	a1,a1
  8001d4:	00140d13          	addi	s10,s0,1
  8001d8:	04b56263          	bltu	a0,a1,80021c <vprintfmt+0xbc>
  8001dc:	058a                	slli	a1,a1,0x2
  8001de:	95d6                	add	a1,a1,s5
  8001e0:	4194                	lw	a3,0(a1)
  8001e2:	96d6                	add	a3,a3,s5
  8001e4:	8682                	jr	a3
            for (fmt --; fmt[-1] != '%'; fmt --)
                /* do nothing */;
            break;
        }
    }
}
  8001e6:	70e6                	ld	ra,120(sp)
  8001e8:	7446                	ld	s0,112(sp)
  8001ea:	74a6                	ld	s1,104(sp)
  8001ec:	7906                	ld	s2,96(sp)
  8001ee:	69e6                	ld	s3,88(sp)
  8001f0:	6a46                	ld	s4,80(sp)
  8001f2:	6aa6                	ld	s5,72(sp)
  8001f4:	6b06                	ld	s6,64(sp)
  8001f6:	7be2                	ld	s7,56(sp)
  8001f8:	7c42                	ld	s8,48(sp)
  8001fa:	7ca2                	ld	s9,40(sp)
  8001fc:	7d02                	ld	s10,32(sp)
  8001fe:	6de2                	ld	s11,24(sp)
  800200:	6109                	addi	sp,sp,128
  800202:	8082                	ret
            padc = '0';
  800204:	87b2                	mv	a5,a2
            goto reswitch;
  800206:	00144603          	lbu	a2,1(s0)
        switch (ch = *(unsigned char *)fmt ++) {
  80020a:	846a                	mv	s0,s10
  80020c:	00140d13          	addi	s10,s0,1
  800210:	fdd6059b          	addiw	a1,a2,-35
  800214:	0ff5f593          	zext.b	a1,a1
  800218:	fcb572e3          	bgeu	a0,a1,8001dc <vprintfmt+0x7c>
            putch('%', putdat);
  80021c:	85a6                	mv	a1,s1
  80021e:	02500513          	li	a0,37
  800222:	9902                	jalr	s2
            for (fmt --; fmt[-1] != '%'; fmt --)
  800224:	fff44783          	lbu	a5,-1(s0)
  800228:	8d22                	mv	s10,s0
  80022a:	f73788e3          	beq	a5,s3,80019a <vprintfmt+0x3a>
  80022e:	ffed4783          	lbu	a5,-2(s10)
  800232:	1d7d                	addi	s10,s10,-1
  800234:	ff379de3          	bne	a5,s3,80022e <vprintfmt+0xce>
  800238:	b78d                	j	80019a <vprintfmt+0x3a>
                precision = precision * 10 + ch - '0';
  80023a:	fd060c1b          	addiw	s8,a2,-48
                ch = *fmt;
  80023e:	00144603          	lbu	a2,1(s0)
        switch (ch = *(unsigned char *)fmt ++) {
  800242:	846a                	mv	s0,s10
                if (ch < '0' || ch > '9') {
  800244:	fd06069b          	addiw	a3,a2,-48
                ch = *fmt;
  800248:	0006059b          	sext.w	a1,a2
                if (ch < '0' || ch > '9') {
  80024c:	02d86463          	bltu	a6,a3,800274 <vprintfmt+0x114>
                ch = *fmt;
  800250:	00144603          	lbu	a2,1(s0)
                precision = precision * 10 + ch - '0';
  800254:	002c169b          	slliw	a3,s8,0x2
  800258:	0186873b          	addw	a4,a3,s8
  80025c:	0017171b          	slliw	a4,a4,0x1
  800260:	9f2d                	addw	a4,a4,a1
                if (ch < '0' || ch > '9') {
  800262:	fd06069b          	addiw	a3,a2,-48
            for (precision = 0; ; ++ fmt) {
  800266:	0405                	addi	s0,s0,1
                precision = precision * 10 + ch - '0';
  800268:	fd070c1b          	addiw	s8,a4,-48
                ch = *fmt;
  80026c:	0006059b          	sext.w	a1,a2
                if (ch < '0' || ch > '9') {
  800270:	fed870e3          	bgeu	a6,a3,800250 <vprintfmt+0xf0>
            if (width < 0)
  800274:	f40ddce3          	bgez	s11,8001cc <vprintfmt+0x6c>
                width = precision, precision = -1;
  800278:	8de2                	mv	s11,s8
  80027a:	5c7d                	li	s8,-1
  80027c:	bf81                	j	8001cc <vprintfmt+0x6c>
            if (width < 0)
  80027e:	fffdc693          	not	a3,s11
  800282:	96fd                	srai	a3,a3,0x3f
  800284:	00ddfdb3          	and	s11,s11,a3
        switch (ch = *(unsigned char *)fmt ++) {
  800288:	00144603          	lbu	a2,1(s0)
  80028c:	2d81                	sext.w	s11,s11
  80028e:	846a                	mv	s0,s10
            goto reswitch;
  800290:	bf35                	j	8001cc <vprintfmt+0x6c>
            precision = va_arg(ap, int);
  800292:	000a2c03          	lw	s8,0(s4)
        switch (ch = *(unsigned char *)fmt ++) {
  800296:	00144603          	lbu	a2,1(s0)
            precision = va_arg(ap, int);
  80029a:	0a21                	addi	s4,s4,8
        switch (ch = *(unsigned char *)fmt ++) {
  80029c:	846a                	mv	s0,s10
            goto process_precision;
  80029e:	bfd9                	j	800274 <vprintfmt+0x114>
    if (lflag >= 2) {
  8002a0:	4705                	li	a4,1
            precision = va_arg(ap, int);
  8002a2:	008a0593          	addi	a1,s4,8
    if (lflag >= 2) {
  8002a6:	01174463          	blt	a4,a7,8002ae <vprintfmt+0x14e>
    else if (lflag) {
  8002aa:	1a088e63          	beqz	a7,800466 <vprintfmt+0x306>
        return va_arg(*ap, unsigned long);
  8002ae:	000a3603          	ld	a2,0(s4)
  8002b2:	46c1                	li	a3,16
  8002b4:	8a2e                	mv	s4,a1
            printnum(putch, putdat, num, base, width, padc);
  8002b6:	2781                	sext.w	a5,a5
  8002b8:	876e                	mv	a4,s11
  8002ba:	85a6                	mv	a1,s1
  8002bc:	854a                	mv	a0,s2
  8002be:	e37ff0ef          	jal	ra,8000f4 <printnum>
            break;
  8002c2:	bde1                	j	80019a <vprintfmt+0x3a>
            putch(va_arg(ap, int), putdat);
  8002c4:	000a2503          	lw	a0,0(s4)
  8002c8:	85a6                	mv	a1,s1
  8002ca:	0a21                	addi	s4,s4,8
  8002cc:	9902                	jalr	s2
            break;
  8002ce:	b5f1                	j	80019a <vprintfmt+0x3a>
    if (lflag >= 2) {
  8002d0:	4705                	li	a4,1
            precision = va_arg(ap, int);
  8002d2:	008a0593          	addi	a1,s4,8
    if (lflag >= 2) {
  8002d6:	01174463          	blt	a4,a7,8002de <vprintfmt+0x17e>
    else if (lflag) {
  8002da:	18088163          	beqz	a7,80045c <vprintfmt+0x2fc>
        return va_arg(*ap, unsigned long);
  8002de:	000a3603          	ld	a2,0(s4)
  8002e2:	46a9                	li	a3,10
  8002e4:	8a2e                	mv	s4,a1
  8002e6:	bfc1                	j	8002b6 <vprintfmt+0x156>
        switch (ch = *(unsigned char *)fmt ++) {
  8002e8:	00144603          	lbu	a2,1(s0)
            altflag = 1;
  8002ec:	4c85                	li	s9,1
        switch (ch = *(unsigned char *)fmt ++) {
  8002ee:	846a                	mv	s0,s10
            goto reswitch;
  8002f0:	bdf1                	j	8001cc <vprintfmt+0x6c>
            putch(ch, putdat);
  8002f2:	85a6                	mv	a1,s1
  8002f4:	02500513          	li	a0,37
  8002f8:	9902                	jalr	s2
            break;
  8002fa:	b545                	j	80019a <vprintfmt+0x3a>
        switch (ch = *(unsigned char *)fmt ++) {
  8002fc:	00144603          	lbu	a2,1(s0)
            lflag ++;
  800300:	2885                	addiw	a7,a7,1
        switch (ch = *(unsigned char *)fmt ++) {
  800302:	846a                	mv	s0,s10
            goto reswitch;
  800304:	b5e1                	j	8001cc <vprintfmt+0x6c>
    if (lflag >= 2) {
  800306:	4705                	li	a4,1
            precision = va_arg(ap, int);
  800308:	008a0593          	addi	a1,s4,8
    if (lflag >= 2) {
  80030c:	01174463          	blt	a4,a7,800314 <vprintfmt+0x1b4>
    else if (lflag) {
  800310:	14088163          	beqz	a7,800452 <vprintfmt+0x2f2>
        return va_arg(*ap, unsigned long);
  800314:	000a3603          	ld	a2,0(s4)
  800318:	46a1                	li	a3,8
  80031a:	8a2e                	mv	s4,a1
  80031c:	bf69                	j	8002b6 <vprintfmt+0x156>
            putch('0', putdat);
  80031e:	03000513          	li	a0,48
  800322:	85a6                	mv	a1,s1
  800324:	e03e                	sd	a5,0(sp)
  800326:	9902                	jalr	s2
            putch('x', putdat);
  800328:	85a6                	mv	a1,s1
  80032a:	07800513          	li	a0,120
  80032e:	9902                	jalr	s2
            num = (unsigned long long)(uintptr_t)va_arg(ap, void *);
  800330:	0a21                	addi	s4,s4,8
            goto number;
  800332:	6782                	ld	a5,0(sp)
  800334:	46c1                	li	a3,16
            num = (unsigned long long)(uintptr_t)va_arg(ap, void *);
  800336:	ff8a3603          	ld	a2,-8(s4)
            goto number;
  80033a:	bfb5                	j	8002b6 <vprintfmt+0x156>
            if ((p = va_arg(ap, char *)) == NULL) {
  80033c:	000a3403          	ld	s0,0(s4)
  800340:	008a0713          	addi	a4,s4,8
  800344:	e03a                	sd	a4,0(sp)
  800346:	14040263          	beqz	s0,80048a <vprintfmt+0x32a>
            if (width > 0 && padc != '-') {
  80034a:	0fb05763          	blez	s11,800438 <vprintfmt+0x2d8>
  80034e:	02d00693          	li	a3,45
  800352:	0cd79163          	bne	a5,a3,800414 <vprintfmt+0x2b4>
            for (; (ch = *p ++) != '\0' && (precision < 0 || -- precision >= 0); width --) {
  800356:	00044783          	lbu	a5,0(s0)
  80035a:	0007851b          	sext.w	a0,a5
  80035e:	cf85                	beqz	a5,800396 <vprintfmt+0x236>
  800360:	00140a13          	addi	s4,s0,1
                if (altflag && (ch < ' ' || ch > '~')) {
  800364:	05e00413          	li	s0,94
            for (; (ch = *p ++) != '\0' && (precision < 0 || -- precision >= 0); width --) {
  800368:	000c4563          	bltz	s8,800372 <vprintfmt+0x212>
  80036c:	3c7d                	addiw	s8,s8,-1
  80036e:	036c0263          	beq	s8,s6,800392 <vprintfmt+0x232>
                    putch('?', putdat);
  800372:	85a6                	mv	a1,s1
                if (altflag && (ch < ' ' || ch > '~')) {
  800374:	0e0c8e63          	beqz	s9,800470 <vprintfmt+0x310>
  800378:	3781                	addiw	a5,a5,-32
  80037a:	0ef47b63          	bgeu	s0,a5,800470 <vprintfmt+0x310>
                    putch('?', putdat);
  80037e:	03f00513          	li	a0,63
  800382:	9902                	jalr	s2
            for (; (ch = *p ++) != '\0' && (precision < 0 || -- precision >= 0); width --) {
  800384:	000a4783          	lbu	a5,0(s4)
  800388:	3dfd                	addiw	s11,s11,-1
  80038a:	0a05                	addi	s4,s4,1
  80038c:	0007851b          	sext.w	a0,a5
  800390:	ffe1                	bnez	a5,800368 <vprintfmt+0x208>
            for (; width > 0; width --) {
  800392:	01b05963          	blez	s11,8003a4 <vprintfmt+0x244>
  800396:	3dfd                	addiw	s11,s11,-1
                putch(' ', putdat);
  800398:	85a6                	mv	a1,s1
  80039a:	02000513          	li	a0,32
  80039e:	9902                	jalr	s2
            for (; width > 0; width --) {
  8003a0:	fe0d9be3          	bnez	s11,800396 <vprintfmt+0x236>
            if ((p = va_arg(ap, char *)) == NULL) {
  8003a4:	6a02                	ld	s4,0(sp)
  8003a6:	bbd5                	j	80019a <vprintfmt+0x3a>
    if (lflag >= 2) {
  8003a8:	4705                	li	a4,1
            precision = va_arg(ap, int);
  8003aa:	008a0c93          	addi	s9,s4,8
    if (lflag >= 2) {
  8003ae:	01174463          	blt	a4,a7,8003b6 <vprintfmt+0x256>
    else if (lflag) {
  8003b2:	08088d63          	beqz	a7,80044c <vprintfmt+0x2ec>
        return va_arg(*ap, long);
  8003b6:	000a3403          	ld	s0,0(s4)
            if ((long long)num < 0) {
  8003ba:	0a044d63          	bltz	s0,800474 <vprintfmt+0x314>
            num = getint(&ap, lflag);
  8003be:	8622                	mv	a2,s0
  8003c0:	8a66                	mv	s4,s9
  8003c2:	46a9                	li	a3,10
  8003c4:	bdcd                	j	8002b6 <vprintfmt+0x156>
            err = va_arg(ap, int);
  8003c6:	000a2783          	lw	a5,0(s4)
            if (err > MAXERROR || (p = error_string[err]) == NULL) {
  8003ca:	4761                	li	a4,24
            err = va_arg(ap, int);
  8003cc:	0a21                	addi	s4,s4,8
            if (err < 0) {
  8003ce:	41f7d69b          	sraiw	a3,a5,0x1f
  8003d2:	8fb5                	xor	a5,a5,a3
  8003d4:	40d786bb          	subw	a3,a5,a3
            if (err > MAXERROR || (p = error_string[err]) == NULL) {
  8003d8:	02d74163          	blt	a4,a3,8003fa <vprintfmt+0x29a>
  8003dc:	00369793          	slli	a5,a3,0x3
  8003e0:	97de                	add	a5,a5,s7
  8003e2:	639c                	ld	a5,0(a5)
  8003e4:	cb99                	beqz	a5,8003fa <vprintfmt+0x29a>
                printfmt(putch, putdat, "%s", p);
  8003e6:	86be                	mv	a3,a5
  8003e8:	00000617          	auipc	a2,0x0
  8003ec:	2d860613          	addi	a2,a2,728 # 8006c0 <main+0x1c2>
  8003f0:	85a6                	mv	a1,s1
  8003f2:	854a                	mv	a0,s2
  8003f4:	0ce000ef          	jal	ra,8004c2 <printfmt>
  8003f8:	b34d                	j	80019a <vprintfmt+0x3a>
                printfmt(putch, putdat, "error %d", err);
  8003fa:	00000617          	auipc	a2,0x0
  8003fe:	2b660613          	addi	a2,a2,694 # 8006b0 <main+0x1b2>
  800402:	85a6                	mv	a1,s1
  800404:	854a                	mv	a0,s2
  800406:	0bc000ef          	jal	ra,8004c2 <printfmt>
  80040a:	bb41                	j	80019a <vprintfmt+0x3a>
                p = "(null)";
  80040c:	00000417          	auipc	s0,0x0
  800410:	29c40413          	addi	s0,s0,668 # 8006a8 <main+0x1aa>
                for (width -= strnlen(p, precision); width > 0; width --) {
  800414:	85e2                	mv	a1,s8
  800416:	8522                	mv	a0,s0
  800418:	e43e                	sd	a5,8(sp)
  80041a:	0c8000ef          	jal	ra,8004e2 <strnlen>
  80041e:	40ad8dbb          	subw	s11,s11,a0
  800422:	01b05b63          	blez	s11,800438 <vprintfmt+0x2d8>
                    putch(padc, putdat);
  800426:	67a2                	ld	a5,8(sp)
  800428:	00078a1b          	sext.w	s4,a5
                for (width -= strnlen(p, precision); width > 0; width --) {
  80042c:	3dfd                	addiw	s11,s11,-1
                    putch(padc, putdat);
  80042e:	85a6                	mv	a1,s1
  800430:	8552                	mv	a0,s4
  800432:	9902                	jalr	s2
                for (width -= strnlen(p, precision); width > 0; width --) {
  800434:	fe0d9ce3          	bnez	s11,80042c <vprintfmt+0x2cc>
            for (; (ch = *p ++) != '\0' && (precision < 0 || -- precision >= 0); width --) {
  800438:	00044783          	lbu	a5,0(s0)
  80043c:	00140a13          	addi	s4,s0,1
  800440:	0007851b          	sext.w	a0,a5
  800444:	d3a5                	beqz	a5,8003a4 <vprintfmt+0x244>
                if (altflag && (ch < ' ' || ch > '~')) {
  800446:	05e00413          	li	s0,94
  80044a:	bf39                	j	800368 <vprintfmt+0x208>
        return va_arg(*ap, int);
  80044c:	000a2403          	lw	s0,0(s4)
  800450:	b7ad                	j	8003ba <vprintfmt+0x25a>
        return va_arg(*ap, unsigned int);
  800452:	000a6603          	lwu	a2,0(s4)
  800456:	46a1                	li	a3,8
  800458:	8a2e                	mv	s4,a1
  80045a:	bdb1                	j	8002b6 <vprintfmt+0x156>
  80045c:	000a6603          	lwu	a2,0(s4)
  800460:	46a9                	li	a3,10
  800462:	8a2e                	mv	s4,a1
  800464:	bd89                	j	8002b6 <vprintfmt+0x156>
  800466:	000a6603          	lwu	a2,0(s4)
  80046a:	46c1                	li	a3,16
  80046c:	8a2e                	mv	s4,a1
  80046e:	b5a1                	j	8002b6 <vprintfmt+0x156>
                    putch(ch, putdat);
  800470:	9902                	jalr	s2
  800472:	bf09                	j	800384 <vprintfmt+0x224>
                putch('-', putdat);
  800474:	85a6                	mv	a1,s1
  800476:	02d00513          	li	a0,45
  80047a:	e03e                	sd	a5,0(sp)
  80047c:	9902                	jalr	s2
                num = -(long long)num;
  80047e:	6782                	ld	a5,0(sp)
  800480:	8a66                	mv	s4,s9
  800482:	40800633          	neg	a2,s0
  800486:	46a9                	li	a3,10
  800488:	b53d                	j	8002b6 <vprintfmt+0x156>
            if (width > 0 && padc != '-') {
  80048a:	03b05163          	blez	s11,8004ac <vprintfmt+0x34c>
  80048e:	02d00693          	li	a3,45
  800492:	f6d79de3          	bne	a5,a3,80040c <vprintfmt+0x2ac>
                p = "(null)";
  800496:	00000417          	auipc	s0,0x0
  80049a:	21240413          	addi	s0,s0,530 # 8006a8 <main+0x1aa>
            for (; (ch = *p ++) != '\0' && (precision < 0 || -- precision >= 0); width --) {
  80049e:	02800793          	li	a5,40
  8004a2:	02800513          	li	a0,40
  8004a6:	00140a13          	addi	s4,s0,1
  8004aa:	bd6d                	j	800364 <vprintfmt+0x204>
  8004ac:	00000a17          	auipc	s4,0x0
  8004b0:	1fda0a13          	addi	s4,s4,509 # 8006a9 <main+0x1ab>
  8004b4:	02800513          	li	a0,40
  8004b8:	02800793          	li	a5,40
                if (altflag && (ch < ' ' || ch > '~')) {
  8004bc:	05e00413          	li	s0,94
  8004c0:	b565                	j	800368 <vprintfmt+0x208>

00000000008004c2 <printfmt>:
printfmt(void (*putch)(int, void*), void *putdat, const char *fmt, ...) {
  8004c2:	715d                	addi	sp,sp,-80
    va_start(ap, fmt);
  8004c4:	02810313          	addi	t1,sp,40
printfmt(void (*putch)(int, void*), void *putdat, const char *fmt, ...) {
  8004c8:	f436                	sd	a3,40(sp)
    vprintfmt(putch, putdat, fmt, ap);
  8004ca:	869a                	mv	a3,t1
printfmt(void (*putch)(int, void*), void *putdat, const char *fmt, ...) {
  8004cc:	ec06                	sd	ra,24(sp)
  8004ce:	f83a                	sd	a4,48(sp)
  8004d0:	fc3e                	sd	a5,56(sp)
  8004d2:	e0c2                	sd	a6,64(sp)
  8004d4:	e4c6                	sd	a7,72(sp)
    va_start(ap, fmt);
  8004d6:	e41a                	sd	t1,8(sp)
    vprintfmt(putch, putdat, fmt, ap);
  8004d8:	c89ff0ef          	jal	ra,800160 <vprintfmt>
}
  8004dc:	60e2                	ld	ra,24(sp)
  8004de:	6161                	addi	sp,sp,80
  8004e0:	8082                	ret

00000000008004e2 <strnlen>:
 * @len if there is no '\0' character among the first @len characters
 * pointed by @s.
 * */
size_t
strnlen(const char *s, size_t len) {
    size_t cnt = 0;
  8004e2:	4781                	li	a5,0
    while (cnt < len && *s ++ != '\0') {
  8004e4:	e589                	bnez	a1,8004ee <strnlen+0xc>
  8004e6:	a811                	j	8004fa <strnlen+0x18>
        cnt ++;
  8004e8:	0785                	addi	a5,a5,1
    while (cnt < len && *s ++ != '\0') {
  8004ea:	00f58863          	beq	a1,a5,8004fa <strnlen+0x18>
  8004ee:	00f50733          	add	a4,a0,a5
  8004f2:	00074703          	lbu	a4,0(a4)
  8004f6:	fb6d                	bnez	a4,8004e8 <strnlen+0x6>
  8004f8:	85be                	mv	a1,a5
    }
    return cnt;
}
  8004fa:	852e                	mv	a0,a1
  8004fc:	8082                	ret

00000000008004fe <main>:

#define ARRAY_SIZE 1024

int shared_data[ARRAY_SIZE];  // 全局数组，测试COW

int main(void) {
  8004fe:	7139                	addi	sp,sp,-64
    cprintf("COW Test: Starting Copy-on-Write test...\n");
  800500:	00000517          	auipc	a0,0x0
  800504:	4a850513          	addi	a0,a0,1192 # 8009a8 <error_string+0xc8>
int main(void) {
  800508:	f822                	sd	s0,48(sp)
  80050a:	ec4e                	sd	s3,24(sp)
  80050c:	00001417          	auipc	s0,0x1
  800510:	af440413          	addi	s0,s0,-1292 # 801000 <shared_data>
  800514:	fc06                	sd	ra,56(sp)
  800516:	f426                	sd	s1,40(sp)
  800518:	f04a                	sd	s2,32(sp)
    cprintf("COW Test: Starting Copy-on-Write test...\n");
  80051a:	b27ff0ef          	jal	ra,800040 <cprintf>
  80051e:	89a2                	mv	s3,s0
  800520:	8722                	mv	a4,s0

    // 初始化共享数据
    int i;
    for (i = 0; i < ARRAY_SIZE; i++) {
  800522:	4781                	li	a5,0
  800524:	40000693          	li	a3,1024
        shared_data[i] = i;
  800528:	c31c                	sw	a5,0(a4)
    for (i = 0; i < ARRAY_SIZE; i++) {
  80052a:	2785                	addiw	a5,a5,1
  80052c:	0711                	addi	a4,a4,4
  80052e:	fed79de3          	bne	a5,a3,800528 <main+0x2a>
    }
    cprintf("COW Test: Initialized %d elements\n", ARRAY_SIZE);
  800532:	40000593          	li	a1,1024
  800536:	00000517          	auipc	a0,0x0
  80053a:	4a250513          	addi	a0,a0,1186 # 8009d8 <error_string+0xf8>
  80053e:	b03ff0ef          	jal	ra,800040 <cprintf>

    // fork创建子进程
    int pid = fork();
  800542:	ba1ff0ef          	jal	ra,8000e2 <fork>
  800546:	84aa                	mv	s1,a0

    if (pid == 0) {
  800548:	c52d                	beqz	a0,8005b2 <main+0xb4>
        }

        cprintf("COW Test: Child process completed successfully\n");
        exit(0);

    } else if (pid > 0) {
  80054a:	10a05263          	blez	a0,80064e <main+0x150>
        // 父进程
        cprintf("COW Test: Parent process forked child pid=%d\n", pid);
  80054e:	85aa                	mv	a1,a0
  800550:	00000517          	auipc	a0,0x0
  800554:	60050513          	addi	a0,a0,1536 # 800b50 <error_string+0x270>
  800558:	ae9ff0ef          	jal	ra,800040 <cprintf>

        // 等待子进程完成
        cprintf("COW Test: Parent waiting for child...\n");
  80055c:	00000517          	auipc	a0,0x0
  800560:	62450513          	addi	a0,a0,1572 # 800b80 <error_string+0x2a0>
  800564:	addff0ef          	jal	ra,800040 <cprintf>
        int exit_code;
        waitpid(pid, &exit_code);
  800568:	006c                	addi	a1,sp,12
  80056a:	8526                	mv	a0,s1
  80056c:	b79ff0ef          	jal	ra,8000e4 <waitpid>

        // 验证父进程的数据未被修改
        int parent_val = shared_data[100];
  800570:	1909a403          	lw	s0,400(s3)
        cprintf("COW Test: Parent's shared_data[100] = %d (expected 100)\n", parent_val);
  800574:	00000517          	auipc	a0,0x0
  800578:	63450513          	addi	a0,a0,1588 # 800ba8 <error_string+0x2c8>
  80057c:	85a2                	mv	a1,s0
  80057e:	ac3ff0ef          	jal	ra,800040 <cprintf>

        if (parent_val != 100) {
  800582:	06400793          	li	a5,100
  800586:	0cf41d63          	bne	s0,a5,800660 <main+0x162>
            cprintf("COW Test: FAILED - Parent data was modified by child!\n");
            exit(-1);
        }

        cprintf("COW Test: Parent process completed successfully\n");
  80058a:	00000517          	auipc	a0,0x0
  80058e:	69650513          	addi	a0,a0,1686 # 800c20 <error_string+0x340>
  800592:	aafff0ef          	jal	ra,800040 <cprintf>
        cprintf("COW Test: *** PASSED *** Copy-on-Write works correctly!\n");
  800596:	00000517          	auipc	a0,0x0
  80059a:	6c250513          	addi	a0,a0,1730 # 800c58 <error_string+0x378>
  80059e:	aa3ff0ef          	jal	ra,800040 <cprintf>
        cprintf("COW Test: Fork failed!\n");
        exit(-1);
    }

    return 0;
}
  8005a2:	70e2                	ld	ra,56(sp)
  8005a4:	7442                	ld	s0,48(sp)
  8005a6:	74a2                	ld	s1,40(sp)
  8005a8:	7902                	ld	s2,32(sp)
  8005aa:	69e2                	ld	s3,24(sp)
  8005ac:	4501                	li	a0,0
  8005ae:	6121                	addi	sp,sp,64
  8005b0:	8082                	ret
        cprintf("COW Test: Child process (pid=%d) started\n", getpid());
  8005b2:	b35ff0ef          	jal	ra,8000e6 <getpid>
  8005b6:	85aa                	mv	a1,a0
  8005b8:	00000517          	auipc	a0,0x0
  8005bc:	44850513          	addi	a0,a0,1096 # 800a00 <error_string+0x120>
  8005c0:	a81ff0ef          	jal	ra,800040 <cprintf>
        cprintf("COW Test: Child reading data...\n");
  8005c4:	00000517          	auipc	a0,0x0
  8005c8:	46c50513          	addi	a0,a0,1132 # 800a30 <error_string+0x150>
  8005cc:	a75ff0ef          	jal	ra,800040 <cprintf>
        for (i = 0; i < ARRAY_SIZE; i++) {
  8005d0:	00002917          	auipc	s2,0x2
  8005d4:	a3090913          	addi	s2,s2,-1488 # 802000 <shared_data+0x1000>
        cprintf("COW Test: Child reading data...\n");
  8005d8:	00001797          	auipc	a5,0x1
  8005dc:	a2878793          	addi	a5,a5,-1496 # 801000 <shared_data>
            sum += shared_data[i];
  8005e0:	4398                	lw	a4,0(a5)
        for (i = 0; i < ARRAY_SIZE; i++) {
  8005e2:	0791                	addi	a5,a5,4
            sum += shared_data[i];
  8005e4:	9cb9                	addw	s1,s1,a4
        for (i = 0; i < ARRAY_SIZE; i++) {
  8005e6:	ff279de3          	bne	a5,s2,8005e0 <main+0xe2>
        cprintf("COW Test: Child read sum = %d\n", sum);
  8005ea:	85a6                	mv	a1,s1
  8005ec:	00000517          	auipc	a0,0x0
  8005f0:	46c50513          	addi	a0,a0,1132 # 800a58 <error_string+0x178>
  8005f4:	a4dff0ef          	jal	ra,800040 <cprintf>
        cprintf("COW Test: Child writing data (should trigger COW)...\n");
  8005f8:	00000517          	auipc	a0,0x0
  8005fc:	48050513          	addi	a0,a0,1152 # 800a78 <error_string+0x198>
  800600:	a41ff0ef          	jal	ra,800040 <cprintf>
  800604:	4781                	li	a5,0
            shared_data[i] = i * 2;  // 修改数据
  800606:	c01c                	sw	a5,0(s0)
        for (i = 0; i < ARRAY_SIZE; i++) {
  800608:	0411                	addi	s0,s0,4
  80060a:	2789                	addiw	a5,a5,2
  80060c:	ff241de3          	bne	s0,s2,800606 <main+0x108>
        int child_val = shared_data[100];
  800610:	1909a403          	lw	s0,400(s3)
        cprintf("COW Test: Child modified shared_data[100] = %d (expected 200)\n", child_val);
  800614:	00000517          	auipc	a0,0x0
  800618:	49c50513          	addi	a0,a0,1180 # 800ab0 <error_string+0x1d0>
  80061c:	85a2                	mv	a1,s0
  80061e:	a23ff0ef          	jal	ra,800040 <cprintf>
        if (child_val != 200) {
  800622:	0c800793          	li	a5,200
  800626:	00f40b63          	beq	s0,a5,80063c <main+0x13e>
            cprintf("COW Test: FAILED - Child data incorrect!\n");
  80062a:	00000517          	auipc	a0,0x0
  80062e:	4c650513          	addi	a0,a0,1222 # 800af0 <error_string+0x210>
  800632:	a0fff0ef          	jal	ra,800040 <cprintf>
            exit(-1);
  800636:	557d                	li	a0,-1
  800638:	a95ff0ef          	jal	ra,8000cc <exit>
        cprintf("COW Test: Child process completed successfully\n");
  80063c:	00000517          	auipc	a0,0x0
  800640:	4e450513          	addi	a0,a0,1252 # 800b20 <error_string+0x240>
  800644:	9fdff0ef          	jal	ra,800040 <cprintf>
        exit(0);
  800648:	4501                	li	a0,0
  80064a:	a83ff0ef          	jal	ra,8000cc <exit>
        cprintf("COW Test: Fork failed!\n");
  80064e:	00000517          	auipc	a0,0x0
  800652:	64a50513          	addi	a0,a0,1610 # 800c98 <error_string+0x3b8>
  800656:	9ebff0ef          	jal	ra,800040 <cprintf>
        exit(-1);
  80065a:	557d                	li	a0,-1
  80065c:	a71ff0ef          	jal	ra,8000cc <exit>
            cprintf("COW Test: FAILED - Parent data was modified by child!\n");
  800660:	00000517          	auipc	a0,0x0
  800664:	58850513          	addi	a0,a0,1416 # 800be8 <error_string+0x308>
  800668:	9d9ff0ef          	jal	ra,800040 <cprintf>
            exit(-1);
  80066c:	557d                	li	a0,-1
  80066e:	a5fff0ef          	jal	ra,8000cc <exit>
