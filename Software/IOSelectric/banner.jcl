// JOB BANNER
// ASSGN SYS001,X'132'
// ASSGN SYS002,X'132'
// ASSGN SYS003,X'132'
// ASSGN SYSLNK,X'132'
// ASSGN SYSPCH,X'00D'
// OPTION LINK,DECK,NOXREF,LIST
// EXEC ASSEMBLY
         TITLE 'BANNER PROGRAM'
         PRINT OFF
         START 0
R0       EQU   0
R1       EQU   1
R2       EQU   2
R3       EQU   3
R4       EQU   4
R5       EQU   5
R6       EQU   6
R7       EQU   7
R8       EQU   8
R9       EQU   9
R10      EQU   10
R11      EQU   11
R12      EQU   12
R13      EQU   13
R14      EQU   14
R15      EQU   15
         PRINT ON,NOGEN
CONSOL   EQU   X'01F'
         USING *,0
INITPSW  DC    X'80',X'00',X'0000',X'00',AL3(BANNER)
CCW1     DS    D
CCW2     DS    D
EXTOPSW  DS    D
SVCOPSW  DS    D
PRGOPSW  DS    D
MCOPSW   DS    D
IOOPSW   DS    D
CSW      DS    D
CAW      DS    D
TIMER    DS    D
EXTPSW   DC    X'0002000000000004'
SVCPSW   DC    X'0002000000000008'
PRGPSW   DC    X'000200000000000C'
MCPSW    DC    X'0002000000000010'
IOPSW    DC    X'00',X'00',X'0000',X'00',AL3(IOINT)
* Start
         ORG   *+128
BANNER   BALR  15,0
         USING *,15
* Blank out input buffers
AGAIN    MVI   TEXT1,X'40'
         MVC   TEXT1+1(L'TEXT1*4-1),TEXT1
* Read in 4 lines of text
         LA    R1,PROMPT
         ST    R1,CAW
         LA    R1,PRINT
         ST    R1,CONTPSW+4
         SIO   CONSOL
WAIT     LPSW  WAITPSW
* Generate text buffers for each input line and print them
PRINT    LA    R4,TEXT1         * R4 points to our text string
         L     R6,=F'4'         * R6 is text line count
MORTXT   SR    R2,R2            * Scan line 0
         LA    R5,LINE1         * Start at first line
         L     R12,=F'8'        * Do 8 scan lines
NEXTLN   SR    R3,R3            * Character 0
         L     R13,TEXTLEN      * R13 is character counter
NEXTCH   BAL   R14,GENLINE      * Generate one character-line of output
         BCT   R13,NEXTCH       * On to next character
         LA    R2,1(R2)         * Bump scan line
         BCT   R12,NEXTLN       * On to next scan line
* Work out how long each line is
         SR    R2,R2            * Start with first line
         L     R12,=F'8'        * Line counter
         LA    R3,LINE1         * Base pointer
NEXTLEN  LR    R5,R2            * Get line number 0-7
         MH    R5,=H'120'       * Multiply by length of each line
         A     R5,=F'119'       * Point to last character in line
         L     R13,=F'120'      * Count of non-blank characters
NEXTLCH  SR    R8,R8
         IC    R8,0(R3,R5)      * Get character
         C     R8,SPACE         * Is it blank?
         BNE   FOUNDNB          * No, stop counting
         S     R5,=F'1'         * To preceding character
         BCT   R13,NEXTLCH      * Count down
         L     R13,=F'1'        * Can't use zero count
FOUNDNB  EQU   *
* Now fill in CCW with count in R13
         LR    R5,R2            * Line number 0-7
         SLA   R5,3             * 8 bytes per CCW
         STH   R13,FIRSTCCW+6(R5) * Store R13 in COUNT field of CCW
         LA    R2,1(R2)         * On to next line
         BCT   R12,NEXTLEN      * Loop for all 8 lines and CCWs
* Now print all the lines and continue at NXTXT
         LA    R1,WRITE
         ST    R1,CAW
         LA    R1,NXTXT
         ST    R1,CONTPSW+4
         SIO   CONSOL
         LPSW  WAITPSW
NXTXT    A     R4,TEXTLEN       * Bump onto next input line
         BCT   R6,MORTXT        * Loop for 4 lines
         B     AGAIN            * Start again
*
* Line generation routine
* Entry:
* R2 is scan line 0-7
* R3 is character position 0-15
* R4 is source buffer
* R5 is destination buffer
* Exit:
* R3 is incremented
* R5 is moved on 6 characters
GENLINE  LA    R1,TABLE
         SR    R8,R8
         IC    R8,0(R4,R3)     * Get source character into R8
         S     R8,SPACE        * Subtract x'40' to get table index
         BNL   CHAROK
         SR    R8,R8           * If char was less than 40, force to 00
CHAROK   EQU   *
* Get address of character in bit table into R7
* Address is TABLE+R8*48+R2*6=TABLE+(R8*8+R2)*6
         LR    R7,R8
         SLA   R7,3            * R8*8
         AR    R7,R2           * R8*8+R2
         SLA   R7,1            * (R8*8+R2)*2
         LR    R1,R7
         SLA   R7,1            * (R8*8+R2)*4
         AR    R7,R1           * (R8*8+R2)*6
         LA    R1,TABLE
         AR    R7,R1
* Copy 6 characters from R7 to R5
         MVC   0(6,R5),0(R7)
         LA    R5,6(R5)
         LA    R3,1(R3)
         BR    R14             * Return
*
* I/O Interruption handler
IOINT    L     R2,CSW+4
         N     R2,=X'08000000' * Channel End?
         BC    4,CONT          * Yes, move on
         LPSW  IOOPSW          * No, keep waiting
CONT     LPSW  CONTPSW
* Storage for 4 lines entered by the user
TEXT1    DS    CL20
TEXT2    DS    CL20
TEXT3    DS    CL20
TEXT4    DS    CL20
TEXTLEN  DC    F'20'           * Length of each TEXT item
*
PRMPT    DC    C':'            * Prompt before each line
HEADER   DC    C'http://www.ljw.me.uk/ibm360    VCF-GB 2013'
* PROMPT waits for a CR, writes out a header line,
* prints a prompt, then gets a line, four times
* then does a few blank lines, then interrupt
PROMPT   CCW   X'0A',TEXT1,X'60',L'TEXT1
         CCW   X'09',HEADER,X'60',L'HEADER
         CCW   X'01',PRMPT,X'60',L'PRMPT
         CCW   X'0A',TEXT1,X'60',L'TEXT1
         CCW   X'01',PRMPT,X'60',L'PRMPT
         CCW   X'0A',TEXT2,X'60',L'TEXT2
         CCW   X'01',PRMPT,X'60',L'PRMPT
         CCW   X'0A',TEXT3,X'60',L'TEXT3
         CCW   X'01',PRMPT,X'60',L'PRMPT
         CCW   X'0A',TEXT4,X'60',L'TEXT4
         CCW   X'09',BLANK,X'60',L'BLANK
         CCW   X'09',BLANK,X'60',L'BLANK
         CCW   X'09',BLANK,X'60',L'BLANK
         CCW   X'09',BLANK,X'28',L'BLANK
* Print the lines and then interrupt
WRITE    CCW   X'09',BLANK,X'60',L'BLANK
FIRSTCCW CCW   X'09',LINE1,X'60',L'LINE1
         CCW   X'09',LINE2,X'60',L'LINE2
         CCW   X'09',LINE3,X'60',L'LINE3
         CCW   X'09',LINE4,X'60',L'LINE4
         CCW   X'09',LINE5,X'60',L'LINE5
         CCW   X'09',LINE6,X'60',L'LINE6
         CCW   X'09',LINE7,X'60',L'LINE7
         CCW   X'09',LINE8,X'60',L'LINE8
         CCW   X'09',BLANK,X'28',L'BLANK
*
CONTPSW  DC    X'00000000',X'00',AL3(PRINT)
WAITPSW  DC    X'80020000',X'00',AL3(WAIT)
*
SPACE    DC    X'00000040'
* Lines are exactly 20*6=120 characters long
LINE1    DS    CL120
LINE2    DS    CL120
LINE3    DS    CL120
LINE4    DS    CL120
LINE5    DS    CL120
LINE6    DS    CL120
LINE7    DS    CL120
LINE8    DS    CL120
BLANK    DC    C' '
*
         LTORG
* Character tables
* 6 x 8 matrix (48 characters)
* 40-4F
TABLE    DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
*
         DC    C'      '
         DC    C'      '
         DC    C'      '
         DC    C'      '
         DC    C'      '
         DC    C'      '
         DC    C'  ..  '
         DC    C'  ..  '
*
         DC    48C' '
         DC    48C' '
*
         DC    C'      '
         DC    C'  +   '
         DC    C'  +   '
         DC    C'+++++ '
         DC    C'  +   '
         DC    C'  +   '
         DC    C'      '
         DC    C'      '
*
         DC    48C' '
* 50-5F
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
*
         DC    C'  !   '
         DC    C'  !   '
         DC    C'  !   '
         DC    C'  !   '
         DC    C'  !   '
         DC    C'      '
         DC    C'  !   '
         DC    C'      '
*
         DC    48C' '
*
         DC    C'      '
         DC    C'      '
         DC    C'*   * '
         DC    C' * *  '
         DC    C'***** '
         DC    C' * *  '
         DC    C'*   * '
         DC    C'      '
*
         DC    48C' '
*
         DC    C'      '
         DC    C'      '
         DC    C' ;;   '
         DC    C' ;;   '
         DC    C'      '
         DC    C' ;;   '
         DC    C' ;;   '
         DC    C' ;    '
*
         DC    48C' '
* 60-6F
*
         DC    C'      '
         DC    C'      '
         DC    C'      '
         DC    C'----  '
         DC    C'----  '
         DC    C'      '
         DC    C'      '
         DC    C'      '
*
         DC    C'      '
         DC    C'     /'
         DC    C'    / '
         DC    C'   /  '
         DC    C'  /   '
         DC    C' /    '
         DC    C'/     '
         DC    C'      '
*
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
*
         DC    C'      '
         DC    C'      '
         DC    C'      '
         DC    C'      '
         DC    C'      '
         DC    C' ,,   '
         DC    C' ,,   '
         DC    C' ,    '
*
         DC    48C' '
         DC    48C' '
         DC    48C' '
*
         DC    C' ???  '
         DC    C'?   ? '
         DC    C'    ? '
         DC    C'   ?  '
         DC    C'  ?   '
         DC    C'      '
         DC    C'  ?   '
         DC    C'      '
* 70-7F
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
*
         DC    C'      '
         DC    C'      '
         DC    C' ::   '
         DC    C' ::   '
         DC    C'      '
         DC    C' ::   '
         DC    C' ::   '
         DC    C'      '
*
         DC    C' # #  '
         DC    C' # #  '
         DC    C'##### '
         DC    C' # #  '
         DC    C'##### '
         DC    C' # #  '
         DC    C' # #  '
         DC    C'      '
*
         DC    C' @@@  '
         DC    C'@   @ '
         DC    C'@ @@@ '
         DC    C'@ @ @ '
         DC    C'@ @@@ '
         DC    C'@     '
         DC    C' @@@  '
         DC    C'      '
*
         DC    48C' '
         DC    48C' '
         DC    48C' '
* 80-8F
         DC    48C' '
*
         DC    C'      '
         DC    C'      '
         DC    C' aaa  '
         DC    C'    a '
         DC    C' aaaa '
         DC    C'a   a '
         DC    C' aaaa '
         DC    C'      '
*
         DC    C'b     '
         DC    C'b     '
         DC    C'bbbb  '
         DC    C'b   b '
         DC    C'b   b '
         DC    C'b   b '
         DC    C'bbbb  '
         DC    C'      '
*
         DC    C'      '
         DC    C'      '
         DC    C' ccc  '
         DC    C'c   c '
         DC    C'c     '
         DC    C'c     '
         DC    C' cccc '
         DC    C'      '
*
         DC    C'    d '
         DC    C'    d '
         DC    C' dddd '
         DC    C'd   d '
         DC    C'd   d '
         DC    C'd   d '
         DC    C' dddd '
         DC    C'      '
*
         DC    C'      '
         DC    C'      '
         DC    C' eee  '
         DC    C'e   e '
         DC    C'eeeee '
         DC    C'e     '
         DC    C' eeee '
         DC    C'      '
*
         DC    C'  ff  '
         DC    C' f    '
         DC    C'fff   '
         DC    C' f    '
         DC    C' f    '
         DC    C' f    '
         DC    C' f    '
         DC    C'      '
*
         DC    C'      '
         DC    C'      '
         DC    C' ggg  '
         DC    C'g   g '
         DC    C'g   g '
         DC    C' gggg '
         DC    C'    g '
         DC    C'gggg  '
*
         DC    C'h     '
         DC    C'h     '
         DC    C'hhhh  '
         DC    C'h   h '
         DC    C'h   h '
         DC    C'h   h '
         DC    C'h   h '
         DC    C'      '
*
         DC    C'  i   '
         DC    C'      '
         DC    C' ii   '
         DC    C'  i   '
         DC    C'  i   '
         DC    C'  i   '
         DC    C' iii  '
         DC    C'      '
*
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
* 90-9F
         DC    48C' '
         DC    C'   j  '
         DC    C'      '
         DC    C'  jj  '
         DC    C'   j  '
         DC    C'   j  '
         DC    C'   j  '
         DC    C'j  j  '
         DC    C' jj   '
*
         DC    C'k     '
         DC    C'k     '
         DC    C'k  k  '
         DC    C'k k   '
         DC    C'kk    '
         DC    C'k k   '
         DC    C'k  k  '
         DC    C'      '
*
         DC    C' ll   '
         DC    C'  l   '
         DC    C'  l   '
         DC    C'  l   '
         DC    C'  l   '
         DC    C'  l   '
         DC    C' lll  '
         DC    C'      '
*
         DC    C'      '
         DC    C'      '
         DC    C'mmmm  '
         DC    C'm m m '
         DC    C'm m m '
         DC    C'm m m '
         DC    C'm m m '
         DC    C'      '
*
         DC    C'      '
         DC    C'      '
         DC    C'nnnn  '
         DC    C'n   n '
         DC    C'n   n '
         DC    C'n   n '
         DC    C'n   n '
         DC    C'      '
*
         DC    C'      '
         DC    C'      '
         DC    C' ooo  '
         DC    C'o   o '
         DC    C'o   o '
         DC    C'o   o '
         DC    C' ooo  '
         DC    C'      '
*
         DC    C'      '
         DC    C'      '
         DC    C'pppp  '
         DC    C'p   p '
         DC    C'p   p '
         DC    C'pppp  '
         DC    C'p     '
         DC    C'p     '
*
         DC    C'      '
         DC    C'      '
         DC    C' qqqq '
         DC    C'q   q '
         DC    C'q   q '
         DC    C' qqqq '
         DC    C'    q '
         DC    C'    q '
*
         DC    C'      '
         DC    C'      '
         DC    C'r rr  '
         DC    C' r  r '
         DC    C' r    '
         DC    C' r    '
         DC    C' r    '
         DC    C'      '
*
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
* A0-AF
         DC    48C' '
         DC    48C' '
         DC    C'      '
         DC    C'      '
         DC    C' ssss '
         DC    C's     '
         DC    C' sss  '
         DC    C'    s '
         DC    C'ssss  '
         DC    C'      '
*
         DC    C' t    '
         DC    C' t    '
         DC    C'tttt  '
         DC    C' t    '
         DC    C' t    '
         DC    C' t    '
         DC    C'  tt  '
         DC    C'      '
*
         DC    C'      '
         DC    C'      '
         DC    C'u   u '
         DC    C'u   u '
         DC    C'u   u '
         DC    C'u   u '
         DC    C' uuuu '
         DC    C'      '
*
         DC    C'      '
         DC    C'      '
         DC    C'v   v '
         DC    C'v   v '
         DC    C' v v  '
         DC    C' v v  '
         DC    C'  v   '
         DC    C'      '
*
         DC    C'      '
         DC    C'      '
         DC    C'w w w '
         DC    C'w w w '
         DC    C'w w w '
         DC    C'w w w '
         DC    C' w w  '
         DC    C'      '
*
         DC    C'      '
         DC    C'      '
         DC    C'x   x '
         DC    C' x x  '
         DC    C'  x   '
         DC    C' x x  '
         DC    C'x   x '
         DC    C'      '
*
         DC    C'      '
         DC    C'      '
         DC    C'y   y '
         DC    C'y   y '
         DC    C'y   y '
         DC    C' yyyy '
         DC    C'    y '
         DC    C' yyy  '
*
         DC    C'      '
         DC    C'      '
         DC    C'zzzzz '
         DC    C'   z  '
         DC    C'  z   '
         DC    C' z    '
         DC    C'zzzzz '
         DC    C'      '
*
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
* B0-BF
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
* C0-CF
         DC    48C' '
*
         DC    C' AAA  '
         DC    C'A   A '
         DC    C'A   A '
         DC    C'A   A '
         DC    C'AAAAA '
         DC    C'A   A '
         DC    C'A   A '
         DC    C'      '
*
         DC    C'BBBB  '
         DC    C'B   B '
         DC    C'B   B '
         DC    C'BBBB  '
         DC    C'B   B '
         DC    C'B   B '
         DC    C'BBBB  '
         DC    C'      '
*
         DC    C' CCC  '
         DC    C'C   C '
         DC    C'C     '
         DC    C'C     '
         DC    C'C     '
         DC    C'C   C '
         DC    C' CCC  '
         DC    C'      '
*
         DC    C'DDD   '
         DC    C'D  D  '
         DC    C'D   D '
         DC    C'D   D '
         DC    C'D   D '
         DC    C'D  D  '
         DC    C'DDD   '
         DC    C'      '
*
         DC    C'EEEEE '
         DC    C'E     '
         DC    C'E     '
         DC    C'EEEE  '
         DC    C'E     '
         DC    C'E     '
         DC    C'EEEEE '
         DC    C'      '
*
         DC    C'FFFFF '
         DC    C'F     '
         DC    C'F     '
         DC    C'FFFF  '
         DC    C'F     '
         DC    C'F     '
         DC    C'F     '
         DC    C'      '
*
         DC    C' GGG  '
         DC    C'G   G '
         DC    C'G     '
         DC    C'G GGG '
         DC    C'G   G '
         DC    C'G   G '
         DC    C' GGGG '
         DC    C'      '
*
         DC    C'H   H '
         DC    C'H   H '
         DC    C'H   H '
         DC    C'HHHHH '
         DC    C'H   H '
         DC    C'H   H '
         DC    C'H   H '
         DC    C'      '
*
         DC    C' III  '
         DC    C'  I   '
         DC    C'  I   '
         DC    C'  I   '
         DC    C'  I   '
         DC    C'  I   '
         DC    C' III  '
         DC    C'      '
*
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
* D0-DF
         DC    48C' '
         DC    C'  JJJ '
         DC    C'   J  '
         DC    C'   J  '
         DC    C'   J  '
         DC    C'   J  '
         DC    C'J  J  '
         DC    C' JJ   '
         DC    C'      '
*
         DC    C'K   K '
         DC    C'K  K  '
         DC    C'K K   '
         DC    C'KK    '
         DC    C'K K   '
         DC    C'K  K  '
         DC    C'K   K '
         DC    C'      '
*
         DC    C'L     '
         DC    C'L     '
         DC    C'L     '
         DC    C'L     '
         DC    C'L     '
         DC    C'L     '
         DC    C'LLLLL '
         DC    C'      '
*
         DC    C'M   M '
         DC    C'MM MM '
         DC    C'M M M '
         DC    C'M M M '
         DC    C'M   M '
         DC    C'M   M '
         DC    C'M   M '
         DC    C'      '
*
         DC    C'N   N '
         DC    C'N   N '
         DC    C'NN  N '
         DC    C'N N N '
         DC    C'N  NN '
         DC    C'N   N '
         DC    C'N   N '
         DC    C'      '
*
         DC    C' OOO  '
         DC    C'O   O '
         DC    C'O   O '
         DC    C'O   O '
         DC    C'O   O '
         DC    C'O   O '
         DC    C' OOO  '
         DC    C'      '
*
         DC    C'PPPP  '
         DC    C'P   P '
         DC    C'P   P '
         DC    C'PPPP  '
         DC    C'P     '
         DC    C'P     '
         DC    C'P     '
         DC    C'      '
*
         DC    C' QQQ  '
         DC    C'Q   Q '
         DC    C'Q   Q '
         DC    C'Q   Q '
         DC    C'Q   Q '
         DC    C'Q Q Q '
         DC    C' QQQ  '
         DC    C'    Q '
*
         DC    C'RRRR  '
         DC    C'R   R '
         DC    C'R   R '
         DC    C'RRRR  '
         DC    C'R R   '
         DC    C'R  R  '
         DC    C'R   R '
         DC    C'      '
*
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
* E0-EF
         DC    48C' '
         DC    48C' '
         DC    C' SSS  '
         DC    C'S   S '
         DC    C'S     '
         DC    C' SSS  '
         DC    C'    S '
         DC    C'S   S '
         DC    C' SSS  '
         DC    C'      '
*
         DC    C'TTTTT '
         DC    C'  T   '
         DC    C'  T   '
         DC    C'  T   '
         DC    C'  T   '
         DC    C'  T   '
         DC    C'  T   '
         DC    C'      '
*
         DC    C'U   U '
         DC    C'U   U '
         DC    C'U   U '
         DC    C'U   U '
         DC    C'U   U '
         DC    C'U   U '
         DC    C' UUU  '
         DC    C'      '
*
         DC    C'V   V '
         DC    C'V   V '
         DC    C'V   V '
         DC    C'V   V '
         DC    C'V   V '
         DC    C' V V  '
         DC    C'  V   '
         DC    C'      '
*
         DC    C'W   W '
         DC    C'W   W '
         DC    C'W   W '
         DC    C'W W W '
         DC    C'W W W '
         DC    C'W W W '
         DC    C' W W  '
         DC    C'      '
*
         DC    C'X   X '
         DC    C'X   X '
         DC    C' X X  '
         DC    C'  X   '
         DC    C' X X  '
         DC    C'X   X '
         DC    C'X   X '
         DC    C'      '
*
         DC    C'Y   Y '
         DC    C'Y   Y '
         DC    C'Y   Y '
         DC    C' Y Y  '
         DC    C'  Y   '
         DC    C'  Y   '
         DC    C'  Y   '
         DC    C'      '
*
         DC    C'ZZZZZ '
         DC    C'    Z '
         DC    C'   Z  '
         DC    C'  Z   '
         DC    C' Z    '
         DC    C'Z     '
         DC    C'ZZZZZ '
         DC    C'      '
*
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
* F0-FF
         DC    C'  0   '
         DC    C' 0 0  '
         DC    C'0   0 '
         DC    C'0   0 '
         DC    C'0   0 '
         DC    C' 0 0  '
         DC    C'  0   '
         DC    C'      '
*
         DC    C'  1   '
         DC    C' 11   '
         DC    C'1 1   '
         DC    C'  1   '
         DC    C'  1   '
         DC    C'  1   '
         DC    C'11111 '
         DC    C'      '
*
         DC    C' 222  '
         DC    C'2   2 '
         DC    C'    2 '
         DC    C'  22  '
         DC    C' 2    '
         DC    C'2     '
         DC    C'22222 '
         DC    C'      '
*
         DC    C' 333  '
         DC    C'3   3 '
         DC    C'    3 '
         DC    C'  33  '
         DC    C'    3 '
         DC    C'3   3 '
         DC    C' 333  '
         DC    C'      '
*
         DC    C'   4  '
         DC    C'  44  '
         DC    C' 4 4  '
         DC    C'4  4  '
         DC    C'44444 '
         DC    C'   4  '
         DC    C'   4  '
         DC    C'      '
*
         DC    C'55555 '
         DC    C'5     '
         DC    C'5555  '
         DC    C'    5 '
         DC    C'    5 '
         DC    C'5   5 '
         DC    C' 555  '
         DC    C'      '
*
         DC    C'  66  '
         DC    C' 6    '
         DC    C'6     '
         DC    C'6666  '
         DC    C'6   6 '
         DC    C'6   6 '
         DC    C' 666  '
         DC    C'      '
*
         DC    C'77777 '
         DC    C'    7 '
         DC    C'   7  '
         DC    C'  7   '
         DC    C' 7    '
         DC    C' 7    '
         DC    C' 7    '
         DC    C'      '
*
         DC    C' 888  '
         DC    C'8   8 '
         DC    C'8   8 '
         DC    C' 888  '
         DC    C'8   8 '
         DC    C'8   8 '
         DC    C' 888  '
         DC    C'      '
*
         DC    C' 999  '
         DC    C'9   9 '
         DC    C'9   9 '
         DC    C' 9999 '
         DC    C'    9 '
         DC    C'   9  '
         DC    C' 99   '
         DC    C'      '
*
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
         DC    48C' '
*
         END   BANNER
/*
// EXEC LNKEDT
/&
