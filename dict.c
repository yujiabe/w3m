
/* 
 * From g96p0935@mse.waseda.ac.jp Mon Jun 14 09:34:15 1999 Received: from
 * ei5sun.yz.yamagata-u.ac.jp (ei5sun.yz.yamagata-u.ac.jp [133.24.114.42])
 * by ei5nazha.yz.yamagata-u.ac.jp (8.9.3/8.9.3) with ESMTP id JAA20673 for 
 * <aito@ei5nazha.yz.yamagata-u.ac.jp>; Mon, 14 Jun 1999 09:34:14 +0900
 * (JST) Received: from pandora.mse.waseda.ac.jp
 * (root@pandora.mse.waseda.ac.jp [133.9.5.9]) by
 * ei5sun.yz.yamagata-u.ac.jp (8.8.0/3.5Wbeta) with ESMTP id JAA23968 for
 * <aito@ei5sun.yz.yamagata-u.ac.jp>; Mon, 14 Jun 1999 09:35:30 +0900 (JST)
 * Received: from localhost (root@[133.9.85.55]) by pandora.mse.waseda.ac.jp 
 * (8.9.1+3.0W/3.7W) with ESMTP id JAA18473; Mon, 14 Jun 1999 09:30:31 +0900 
 * (JST) Message-Id: <199906140030.JAA18473@pandora.mse.waseda.ac.jp> To:
 * aito@ei5sun.yz.yamagata-u.ac.jp Subject: w3m:$B1QOB<-E58!:w5!G=Ec:\(B
 * Cc: g96p0935@mse.waseda.ac.jp From: Takashi Nishimoto
 * <g96p0935@mse.waseda.ac.jp> X-Mailer: Mew version 1.93 on Emacs 19.34 /
 * Mule 2.3 (SUETSUMUHANA) Mime-Version: 1.0 Content-Type: Text/Plain;
 * charset=iso-2022-jp Content-Transfer-Encoding: 7bit Date: Mon, 14 Jun
 * 1999 09:29:56 +0900 X-Dispatcher: imput version 980506 Lines: 150
 * 
 * $B@>K\(B@$BAaBg$G$9!#(B
 * 
 * Quick Hack $B$G(B w3m
 * $B$K1QOB<-E58!:w5!G=$HC18lC10L$N%+!<%=%k0\F0$r<BAu$7$^(B $B$7$?!#(B
 * 
 * Unix $B$r;H$C$F$$$k$H!"1QJ8$rFI$`5!2q$,B?$$$G$9$M!#(B Emacs
 * $BFb$G$O%o%s%?%C%A$G1QOB<-E5$r8!:w$9$k(B sdic
 * $B$N$h$&$J%D!<%k$,$"$j$^(B
 * $B$9$,!"$A$g$C$H$7$?J8=q$rFI$`$@$1$K$$$A$$$A(B Emacs
 * $B$KFI$_9~$`$N$O$+$C$?(B $B$k$$$N$G!"$J$s$H$+(B w3m
 * $B$G$G$-$J$$$+$H;W$$!":n6H$KF'$_@Z$j$^$7$?!#(B
 * 
 * $B$9$k$H0U30$K4JC1$K<BAu$G$-$^$7$?!#KM$O(B C
 * $B%W%m%0%i%`$N2~B$$O=i$a$F$G$9(B $B$,!"(B Emacs Lisp
 * $BJB$N<j7Z$5$G<BAu$G$-$?$3$H$K$O46F0$7$^$7$?!#(B
 * 
 * dictword $B$,D4$Y$kC18l$rJ9$$$F8!:w$9$k4X?t$G!"(B dictwordat
 * $B$,%+!<%=%k0L(B
 * $BCV$NC18l$r8!:w$9$k4X?t$G$9!#%=!<%9$r8+$l$PL@$i$+$J$h$&$K8!:w$9$k30It%3(B
 * $B%^%s%I$O(B w3mdict $B$G$9!#(B Unix
 * $B$J$N$G!"IaCJ;H$C$F$$$k%3%^%s%I$X$N(B  symlink $B$K$7$F$$$^$9!#(Bw
 * $B$K(B dictword$B!"(B W $B$K(B dictwordat $B$r3d$jEv$F$F$$$^(B
 * $B$9!#$^$?!"1&<j$GFI$a$k$h$&$K(B ; $B$K$b(B dictwordat
 * $B$r3d$jEv$F$F$$$^$9!#(B */
#include "fm.h"
#include <signal.h>

#ifdef DICT

#define DICTCMD "w3mdict "
#define DICTBUFFERNAME "*dictionary*"
/* char *DICTBUFFERNAME="*dictionary*"; */

char *
GetWord(char *word)
{
    Line *l = Currentbuf->currentLine;
    char *lb = l->lineBuf;
    int i, b, e, pos = Currentbuf->pos;

    i = pos;
    while (!IS_ALPHA(lb[i]) && i >= 0)
	i--;
    pos = i;
    while (IS_ALPHA(lb[i]) && i >= 0)
	i--;
    i++;
    if (!IS_ALPHA(lb[i]))
	return NULL;
    b = i;
    i = pos;
    while (IS_ALPHA(lb[i]) && i <= l->len - 1)
	i++;
    e = i - 1;
    strncpy(word, &lb[b], e - b + 1);
    word[e - b + 1] = '\0';
    return word;
}

void
execdict(char *word)
{
    Buffer *buf;
    static char cmd[100], bufname[100];
    MySignalHandler(*prevtrap) ();

    if (word == NULL)
	return;
    strcpy(cmd, DICTCMD);
    strcat(cmd, word);
    buf = namedBuffer(Firstbuf, SHELLBUFFERNAME);
    if (buf != NULL)
	Firstbuf = deleteBuffer(Firstbuf, buf);

    if (cmd == NULL || *cmd == '\0') {
	displayBuffer(Currentbuf, B_NORMAL);
	return;
    }
    prevtrap = signal(SIGINT, intTrap);
    crmode();
    buf = getshell(cmd);
/* sprintf(bufname,"*dictionary(%s)*",word); */
/* buf->buffername = bufname; */
    buf->buffername = DICTBUFFERNAME;
    buf->filename = word;
    signal(SIGINT, prevtrap);
    term_raw();
    if (buf == NULL) {
	disp_message("Execution failed", FALSE);
    }
    else if (buf->firstLine == NULL) {
	/* if the dictionary doesn't describe the word. */
	char msg[100];
	sprintf(msg, "Word \"%s\" Not Found", word);
	disp_message(msg, FALSE);

    }
    else {
	buf->nextBuffer = Firstbuf;
	Currentbuf = Firstbuf = buf;
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

void
dictword(void)
{
    execdict(inputStr("(dictionary)!", ""));
}

void
dictwordat(void)
{
    static char word[100];
    execdict(GetWord(word));
}
#endif				/* DICT */
