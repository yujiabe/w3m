/*
 * Configuration for w3m
 */

#ifndef _CONFIGURED_
#define _CONFIGURED_

/* User Configuration */

/* 
   If you define DICT, you can use dictionary look-up function
   in w3m. See README.dict for detail.
*/
#undef DICT

/*
   If you define USE_MARK, you can use set-mark (C-SPC),
   goto-next-mark (ESC p), goto-next-mark (ESC n) and
   mark-by-regexp (").
*/
#undef USE_MARK

/*
   If you want to load and save URL history.
 */
#define USE_HISTORY

/*
   BG_COLOR enables w3m to set background color.
 */
#define BG_COLOR

/*
   VIEW_UNSEENOBJECTS enables w3m to make a link to unseen objects.
   e.g. background image.
 */
#undef VIEW_UNSEENOBJECTS

/*
   VI_PREC_NUM enables vi-like behavior for '2 SPC' or '2 b'
 */
#undef VI_PREC_NUM

/*
 * Do word fill
 */
#undef FORMAT_NICE

/*
 * Support Gopher protocol
 */
#undef USE_GOPHER

/*
 * Support NNTP
 */
#define USE_NNTP

/*
 * Support ANSI color escape sequences
 */
#define ANSI_COLOR

/*
 * Enable id attribute
 */
#define ID_EXT

/*
 * Save Current-buffer Information
 */
#define BUFINFO

/*
 * Support EGD (Entropy Gathering Daemon)
 */
#undef USE_EGD

/*
 * MENU_MAP enables w3m to show image map link with popup menu.
 */
#define MENU_MAP

/*
 * Use Emacs-like key binding for file name completion
 */
#undef EMACS_LIKE_LINEEDIT

/*
 * Remove line trailing spaces in html buffer.
 */
#undef ENABLE_REMOVE_TRAILINGSPACES

/**********************************************************/
#ifdef makefile_parameter

BIN_DIR = /usr/local/bin
HELP_DIR = /usr/local/lib/w3m
LIB_DIR = /usr/local/lib/w3m
HELP_FILE = w3mhelp-w3m_ja.html
SYS_LIBRARIES = -lgpm  -lbsd -lnsl -ltermcap  -L/usr/local/ssl/lib -lssl -lcrypto 
LOCAL_LIBRARIES = 
CC = gcc
MYCFLAGS = -g -Wall -I./gc/include  -I/usr/local/ssl/include/openssl -I/usr/local/ssl/include
GCCFLAGS = -g -Wall -I./gc/include -DATOMIC_UNCOLLECTABLE -DNO_EXECUTE_PERMISSION -DALL_INTERIOR_POINTERS -DSILENT -DNO_DEBUGGING #-DNO_SIGNALS
KEYBIND_SRC = keybind.c
KEYBIND_OBJ = keybind.o
EXT=
MATHLIB=-lm
GCLIB=gc/gc.a
GCTARGET=gc/gc.a
RANLIB=ranlib
MKDIR=mkdir -p
VERSION=0.2.1
MODEL=Linux.i686-monster-ja
#else


#define DISPLAY_CODE 'E'

#define JA 0
#define EN 1
#define LANG JA
#define KANJI_SYMBOLS
#define COLOR
#define MOUSE
#define USE_GPM
#undef USE_SYSMOUSE
#define MENU
#define USE_COOKIE
#define USE_SSL
#define USE_SSL_VERIFY
#define FTPPASS_HOSTNAMEGEN
#define SHOW_PARAMS

#define DEF_EDITOR "/bin/vi"
#define DEF_MAILER "/bin/mail"
#define DEF_EXT_BROWSER "/usr/X11R6/bin/netscape"

#define LIB_DIR      "/usr/local/lib/w3m"
#define HELP_DIR     "/usr/local/lib/w3m"
#define HELP_FILE    "w3mhelp.html"
#define W3MCONFIG    "w3mconfig"

#define RC_DIR       "~/.w3m/"
#define BOOKMARK     "bookmark.html"
#define CONFIG_FILE  "config"
#define KEYMAP_FILE  "keymap"
#define MENU_FILE    "menu"
#define COOKIE_FILE  "cookie"
#define HISTORY_FILE "history"

#define USER_MAILCAP RC_DIR "/mailcap"
#define SYS_MAILCAP  "/etc/mailcap"
#define USER_MIMETYPES "~/.mime.types"
#define SYS_MIMETYPES "/usr/lib/mime.types"

#define DEF_SAVE_FILE "index.html"

#define TERMIOS
#define DIRENT
#define STRCASECMP
#define STRCHR
#define STRERROR
#define SYS_ERRLIST
#undef NOBCOPY
#define HAVE_WAITPID
#define HAVE_WAIT3
#define HAVE_STRFTIME

#define GETCWD
#define GETWD
#define READLINK
#define HAVE_SETENV
#define HAVE_PUTENV
#define READLINK


#define SETJMP(env) sigsetjmp(env,1)
#define LONGJMP(env,val) siglongjmp(env,val)
#define JMP_BUF sigjmp_buf

typedef void MySignalHandler;
#define SIGNAL_ARG int _dummy
#define SIGNAL_ARGLIST 0
#define SIGNAL_RETURN return
/*
   If you want to use IPv6, define this symbol.
 */
#undef INET6

#undef TABLE_EXPAND
#undef TABLE_NO_COMPACT
#define NOWRAP 1
#define NEW_FORM 1
#define MATRIX 1
#undef NO_FLOAT_H

#endif /* makefile_parameter */
#endif /* _CONFIGURED_ */

