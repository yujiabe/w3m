/* $Id: rc.c,v 1.53 2002/10/27 16:59:33 ukai Exp $ */
/* 
 * Initialization file etc.
 */
#include "fm.h"
#include "myctype.h"
#include <stdio.h>
#include <errno.h>
#include "parsetag.h"
#include "local.h"
#include "terms.h"
#include <stdlib.h>

struct param_ptr {
    char *name;
    int type;
    int inputtype;
    void *varptr;
    char *comment;
    struct sel_c *select;
};

struct param_section {
    char *name;
    struct param_ptr *params;
};

struct rc_search_table {
    struct param_ptr *param;
    short uniq_pos;
};

static struct rc_search_table *RC_search_table;
static int RC_table_size;

static char *config_file = NULL;

#define P_INT      0
#define P_SHORT    1
#define P_CHARINT  2
#define P_CHAR     3
#define P_STRING   4
#if defined(USE_SSL) && defined(USE_SSL_VERIFY)
#define P_SSLPATH  5
#endif
#ifdef USE_COLOR
#define P_COLOR    6
#endif
#ifdef JP_CHARSET
#define P_CODE     7
#endif
#define P_PIXELS   8
#define P_NZINT    9
#define P_SCALE    10

#if LANG == JA
#define CMT_HELPER	 "�����ӥ塼�����Խ�"
#define CMT_TABSTOP      "������"
#define CMT_PIXEL_PER_CHAR      "ʸ���� (4.0...32.0)"
#define CMT_PIXEL_PER_LINE      "��Ԥι⤵ (4.0...64.0)"
#define CMT_PAGERLINE    "�ڡ�����Ȥ������Ѥ���������¸�����Կ�"
#define CMT_HISTSIZE     "�ݻ�����URL����ο�"
#define CMT_SAVEHIST     "URL�������¸"
#define CMT_KANJICODE    "ɽ���Ѵ���������"
#define CMT_FRAME        "�ե졼��μ�ưɽ��"
#define CMT_ARGV_IS_URL  "scheme �Τʤ������� URL �Ȥߤʤ�"
#define CMT_TSELF        "target��̤����ξ���_self����Ѥ���"
#define CMT_DISPLINK     "�����μ�ưɽ��"
#define CMT_DISPLINEINFO "���ߤι��ֹ��ɽ��"
#define CMT_DISP_IMAGE   "����饤�������ɽ��"
#ifdef USE_IMAGE
#define CMT_AUTO_IMAGE   "����饤�������ư���ɤ߹���"
#define CMT_MAX_LOAD_IMAGE "�����ɤ߹��߻��κ���ץ�����"
#define CMT_EXT_IMAGE_VIEWER   "���������ӥ塼���ɽ��"
#define CMT_IMAGE_SCALE  "�����Υ�������(%)"
#define CMT_IMGDISPLAY   "������ɽ�����뤿��Υ��ޥ��"
#define CMT_IMGSIZE      "�������礭�������뤿��Υ��ޥ��"
#endif
#define CMT_MULTICOL     "�ե�����̾�Υޥ�������ɽ��"
#define CMT_ALT_ENTITY   "����ƥ��ƥ��� ASCII ������ɽ����ɽ��"
#define CMT_FOLD_TEXTAREA "TEXTAREA �ιԤ��ޤ��֤���ɽ��"
#define CMT_COLOR        "���顼ɽ��"
#define CMT_B_COLOR      "ʸ���ο�"
#define CMT_A_COLOR      "���󥫡��ο�"
#define CMT_I_COLOR      "������󥯤ο�"
#define CMT_F_COLOR      "�ե�����ο�"
#define CMT_BG_COLOR     "�طʤο�"
#define CMT_MARK_COLOR   "�ޡ����ο�"
#define CMT_ACTIVE_STYLE "�������򤵤�Ƥ����󥯤ο�����ꤹ��"
#define CMT_C_COLOR	 "�������򤵤�Ƥ����󥯤ο�"
#define CMT_VISITED_ANCHOR	"ˬ�줿���Ȥ������󥯤Ͽ����Ѥ���"
#define CMT_V_COLOR	 "ˬ�줿���Ȥ������󥯤ο�"
#define CMT_HTTP_PROXY   "HTTP�ץ���(URL������)"
#ifdef USE_GOPHER
#define CMT_GOPHER_PROXY "GOPHER�ץ���(URL������)"
#endif				/* USE_GOPHER */
#define CMT_FTP_PROXY    "FTP�ץ���(URL������)"
#define CMT_NO_PROXY     "�ץ��������������ɥᥤ��"
#define CMT_NOPROXY_NETADDR	"�ͥåȥ�����ɥ쥹�ǥץ��������Υ����å�"
#define CMT_NO_CACHE     "Cache ��Ȥ�ʤ�"
#define CMT_DNS_ORDER	"̾�����ν��"
#define CMT_DROOT        "/ ��ɽ�����ǥ��쥯�ȥ�(document root)"
#define CMT_PDROOT       "/~user ��ɽ�����ǥ��쥯�ȥ�"
#define CMT_CGIBIN       "/cgi-bin ��ɽ�����ǥ��쥯�ȥ�"
#define CMT_CONFIRM_QQ   "q �Ǥν�λ���˳�ǧ����"
#ifdef USE_MARK
#define CMT_USE_MARK	"�ޡ�����ǽ��ͭ���ˤ���"
#endif
#ifdef EMACS_LIKE_LINEEDIT
#define CMT_EMACS_LIKE_LINEEDIT	"Emacs���ι��Խ��ˤ���"
#endif
#ifdef VI_PREC_NUM
#define CMT_VI_PREC_NUM "vi���ο��ͥץ�ե�����"
#endif
#ifdef LABEL_TOPLINE
#define CMT_LABEL_TOPLINE	"��٥�˰�ư������˥������뤬�ȥåפˤʤ�褦�ˤ���"
#endif
#ifdef NEXTPAGE_TOPLINE
#define CMT_NEXTPAGE_TOPLINE	"���Υڡ����˰�ư������˥������뤬�ȥåפˤʤ�褦�ˤ���"
#endif
#define CMT_SHOW_NUM     "���ֹ��ɽ������"
#define CMT_SHOW_SRCH_STR "����ʸ�����ɽ������"
#define CMT_MIMETYPES    "���Ѥ���mime.types"
#define CMT_MAILCAP      "���Ѥ���mailcap"
#define CMT_URIMETHODMAP "���Ѥ���urimethodmap"
#define CMT_EDITOR       "���Ѥ��륨�ǥ���"
#define CMT_MAILER       "���Ѥ���᡼��"
#define CMT_EXTBRZ       "�����֥饦��"
#define CMT_EXTBRZ2      "�����֥饦������2"
#define CMT_EXTBRZ3      "�����֥饦������3"
#define CMT_PASSWDFILE	 "�ѥ���ɥե�����"
#define CMT_FTPPASS      "FTP�Υѥ����(���̤ϼ�ʬ��mail address��Ȥ�)"
#ifdef FTPPASS_HOSTNAMEGEN
#define CMT_FTPPASS_HOSTNAMEGEN	"FTP�Υѥ���ɤΥɥᥤ��̾��ư��������"
#endif
#define CMT_USERAGENT    "User-Agent"
#define CMT_ACCEPTENCODING	"������ˡ(Accept-Encoding:)"
#define CMT_ACCEPTMEDIA	 "�����Ĥ����ǥ���������(Accept:)"
#define CMT_ACCEPTLANG   "�����Ĥ������(Accept-Language:)"
#define CMT_DOCUMENTCODE "ʸ���ʸ��������"
#define CMT_SYSTEMCODE   "�����ƥ��ʸ��������"
#define CMT_MARK_ALL_PAGES "���ƤΥڡ�����URL����ʸ������󥯤ˤ���"
#define CMT_WRAP         "�ޤ��֤�����"
#define CMT_VIEW_UNSEENOBJECTS "�طʲ������ؤΥ�󥯤���"
#ifdef __EMX__
#define CMT_BGEXTVIEW	 "�����ӥ塼�����̥��å�����ư����"
#else
#define CMT_BGEXTVIEW    "�����ӥ塼����Хå����饦��ɤ�ư����"
#endif
#define CMT_EXT_DIRLIST  "�ǥ��쥯�ȥ�ꥹ�Ȥ˳������ޥ�ɤ�Ȥ�"
#define CMT_DIRLIST_CMD  "�ǥ��쥯�ȥ�ꥹ���ѥ��ޥ��"
#ifdef USE_DICT
#define CMT_USE_DICTCOMMAND  "���������CGI��ͳ�Ǥ����ʤ�"
#define CMT_DICTCOMMAND  "����������ޥ�ɤ�URL"
#endif				/* USE_DICT */
#define CMT_IGNORE_NULL_IMG_ALT "����IMG ALT°���λ��˥��̾��ɽ������"
#define CMT_IFILE        "�ƥǥ��쥯�ȥ�Υ���ǥå����ե�����"
#define CMT_RETRY_HTTP   "URL�˼�ưŪ�� http:// ���䤦"
#define CMT_DEFAULT_URL	 "URL�򳫤����Υǥե����ʸ����"
#define CMT_DECODE_CTE   "��¸���� Content-Transfer-Encoding ��ǥ����ɤ���"
#ifdef USE_MOUSE
#define CMT_MOUSE         "�ޥ�����Ȥ�"
#define CMT_REVERSE_MOUSE "�ޥ����Υɥ�å�ư���դˤ���"
#endif				/* USE_MOUSE */
#define CMT_CLEAR_BUF     "ɽ������Ƥ��ʤ��Хåե��Υ����������"
#define CMT_NOSENDREFERER "Referer: ������ʤ��褦�ˤ���"
#define CMT_IGNORE_CASE "������������ʸ����ʸ���ζ��̤򤷤ʤ�"
#define CMT_USE_LESSOPEN "LESSOPEN�����"
#ifdef USE_SSL
#ifdef USE_SSL_VERIFY
#define CMT_SSL_VERIFY_SERVER "SSL�Υ�����ǧ�ڤ�Ԥ�"
#define CMT_SSL_CERT_FILE "SSL�Υ��饤�������PEM����������ե�����"
#define CMT_SSL_KEY_FILE "SSL�Υ��饤�������PEM������̩���ե�����"
#define CMT_SSL_CA_PATH "SSL��ǧ�ڶɤ�PEM���������񷲤Τ���ǥ��쥯�ȥ�ؤΥѥ�"
#define CMT_SSL_CA_FILE "SSL��ǧ�ڶɤ�PEM���������񷲤Υե�����"
#endif				/* USE_SSL_VERIFY */
#define CMT_SSL_FORBID_METHOD "�Ȥ�ʤ�SSL�᥽�åɤΥꥹ��(2: SSLv2, 3: SSLv3, t:TLSv1)"
#endif				/* USE_SSL */
#ifdef USE_COOKIE
#define CMT_USECOOKIE "���å�������Ѥ���"
#define CMT_ACCEPTCOOKIE "���å���������դ���"
#define CMT_ACCEPTBADCOOKIE "����Τ��륯�å����Ǥ�����դ���"
#define CMT_COOKIE_REJECT_DOMAINS "���å���������դ��ʤ��ɥᥤ��"
#define CMT_COOKIE_ACCEPT_DOMAINS "���å���������դ���ɥᥤ��"
#endif

#define CMT_FOLLOW_REDIRECTION "����������쥯�Ȥβ��"
#define CMT_META_REFRESH "meta refresh ���б�����"

#ifdef USE_MIGEMO
#define CMT_USE_MIGEMO "Migemo(���޻�����)����Ѥ���"
#define CMT_MIGEMO_COMMAND "Migemo���ޥ��"
#endif				/* USE_MIGEMO */

#define CMT_KEYMAP_FILE "keymap�ե�����"

#else				/* LANG != JA */


#define CMT_HELPER	 "External Viewer Setup"
#define CMT_TABSTOP      "Tab width in characters"
#define CMT_PIXEL_PER_CHAR      "Number of pixels per character (4.0...32.0)"
#define CMT_PIXEL_PER_LINE      "Number of pixels per line (4.0...64.0)"
#define CMT_PAGERLINE    "Number of remembered lines when used as a pager"
#define CMT_HISTSIZE     "Number of remembered URL"
#define CMT_SAVEHIST     "Save URL history"
/* #define CMT_KANJICODE    "Display Kanji Code" */
#define CMT_FRAME        "Render frames automatically"
#define CMT_ARGV_IS_URL  "Treat argument without scheme as URL"
#define CMT_TSELF        "Use _self as default target"
#define CMT_DISPLINK     "Display link URL automatically"
#define CMT_DISPLINEINFO "Display current line number"
#define CMT_DISP_IMAGE   "Display inline images"
#ifdef USE_IMAGE
#define CMT_AUTO_IMAGE   "Load inline images automatically"
#define CMT_MAX_LOAD_IMAGE "Maximum processes for parallel image loading"
#define CMT_EXT_IMAGE_VIEWER   "Use external image viewer"
#define CMT_IMAGE_SCALE  "Scale of image (%)"
#define CMT_IMGDISPLAY   "External command to display image"
#define CMT_IMGSIZE      "External command to get size of image"
#endif
#define CMT_MULTICOL     "Display file names in multi-column format"
#define CMT_ALT_ENTITY   "Use ASCII equivalents to display entities"
#define CMT_FOLD_TEXTAREA "Fold lines in TEXTAREA"
#define CMT_COLOR        "Display with color"
#define CMT_B_COLOR      "Color of normal character"
#define CMT_A_COLOR      "Color of anchor"
#define CMT_I_COLOR      "Color of image link"
#define CMT_F_COLOR      "Color of form"
#define CMT_ACTIVE_STYLE "Enable coloring of active link"
#define CMT_C_COLOR	 "Color of currently active link"
#define CMT_VISITED_ANCHOR "Use visited link color"
#define CMT_V_COLOR	 "Color of visited link"
#define CMT_BG_COLOR     "Color of background"
#define CMT_MARK_COLOR   "Color of mark"
#define CMT_HTTP_PROXY   "URL of HTTP proxy host"
#ifdef USE_GOPHER
#define CMT_GOPHER_PROXY "URL of GOPHER proxy host"
#endif				/* USE_GOPHER */
#define CMT_FTP_PROXY    "URL of FTP proxy host"
#define CMT_NO_PROXY     "Domains to be accessed directly (no proxy)"
#define CMT_NOPROXY_NETADDR	"Check noproxy by network address"
#define CMT_NO_CACHE     "Disable cache"
#define CMT_DNS_ORDER	"Order of name resolution"
#define CMT_DROOT        "Directory corresponding to / (document root)"
#define CMT_PDROOT       "Directory corresponding to /~user"
#define CMT_CGIBIN       "Directory corresponding to /cgi-bin"
#define CMT_CONFIRM_QQ   "Confirm when quitting with q"
#ifdef USE_MARK
#define CMT_USE_MARK	"Enable mark operations"
#endif
#ifdef EMACS_LIKE_LINEEDIT
#define CMT_EMACS_LIKE_LINEEDIT	"Enable Emacs-style line editing"
#endif
#ifdef VI_PREC_NUM
#define CMT_VI_PREC_NUM	 "Enable vi-like numeric prefix"
#endif
#ifdef LABEL_TOPLINE
#define CMT_LABEL_TOPLINE	"Move cursor to top line when going to label"
#endif
#ifdef NEXTPAGE_TOPLINE
#define CMT_NEXTPAGE_TOPLINE	"Move cursor to top line when moving to next page"
#endif
#define CMT_SHOW_NUM     "Show line numbers"
#define CMT_SHOW_SRCH_STR "Show search string"
#define CMT_MIMETYPES    "List of mime.types files"
#define CMT_MAILCAP      "List of mailcap files"
#define CMT_URIMETHODMAP "List of urimethodmap files"
#define CMT_EDITOR       "Editor"
#define CMT_MAILER       "Mailer"
#define CMT_EXTBRZ       "External Browser"
#define CMT_EXTBRZ2      "Second External Browser"
#define CMT_EXTBRZ3      "Third External Browser"
#define CMT_PASSWDFILE	 "Password file"
#define CMT_FTPPASS      "Password for anonymous FTP (your mail address)"
#ifdef FTPPASS_HOSTNAMEGEN
#define CMT_FTPPASS_HOSTNAMEGEN "Generate domain part of password for FTP"
#endif
#define CMT_USERAGENT    "User-Agent identification string"
#define CMT_ACCEPTENCODING	"Accept-Encoding header"
#define CMT_ACCEPTMEDIA	 "Accept header"
#define CMT_ACCEPTLANG   "Accept-Language header"
/* #define CMT_DOCUMENTCODE "Document Charset" */
/* #define CMT_SYSTEMCODE   "System Kanji Code" */
#define CMT_MARK_ALL_PAGES "Treat URL-like strings as links in all pages"
#define CMT_WRAP         "Wrap search"
#define CMT_VIEW_UNSEENOBJECTS "Display unseen objects (e.g. bgimage tag)"
#ifdef __EMX__
#define CMT_BGEXTVIEW	 "Run external viewer in a separate session"
#else
#define CMT_BGEXTVIEW    "Run external viewer in the background"
#endif
#define CMT_EXT_DIRLIST  "Use external program for directory listing"
#define CMT_DIRLIST_CMD  "URL of directory listing command"
#ifdef USE_DICT
#define CMT_USE_DICTCOMMAND  "Enable dictionary lookup through CGI"
#define CMT_DICTCOMMAND  "URL of dictionary lookup command"
#endif				/* USE_DICT */
#define CMT_IGNORE_NULL_IMG_ALT	"Display link name for images lacking ALT"
#define CMT_IFILE        "Index file for directories"
#define CMT_RETRY_HTTP   "Prepend http:// to URL automatically"
#define CMT_DEFAULT_URL  "Default value for open-URL command"
#define CMT_DECODE_CTE   "Decode Content-Transfer-Encoding when saving"
#ifdef USE_MOUSE
#define CMT_MOUSE         "Enable mouse"
#define CMT_REVERSE_MOUSE "Scroll in reverse direction of mouse drag"
#endif				/* USE_MOUSE */
#define CMT_CLEAR_BUF     "Free memory of undisplayed buffers"
#define CMT_NOSENDREFERER "Suppress `Referer:' header"
#define CMT_IGNORE_CASE "Search case-insensitively"
#define CMT_USE_LESSOPEN "Use LESSOPEN"
#ifdef USE_SSL
#ifdef USE_SSL_VERIFY
#define CMT_SSL_VERIFY_SERVER "Perform SSL server verification"
#define CMT_SSL_CERT_FILE "PEM encoded certificate file of client"
#define CMT_SSL_KEY_FILE "PEM encoded private key file of client"
#define CMT_SSL_CA_PATH "Path to directory for PEM encoded certificates of CAs"
#define CMT_SSL_CA_FILE "File consisting of PEM encoded certificates of CAs"
#endif				/* USE_SSL_VERIFY */
#define CMT_SSL_FORBID_METHOD "List of forbidden SSL methods (2: SSLv2, 3: SSLv3, t:TLSv1)"
#endif				/* USE_SSL */
#ifdef USE_COOKIE
#define CMT_USECOOKIE   "Enable cookie processing"
#define CMT_ACCEPTCOOKIE "Accept cookies"
#define CMT_ACCEPTBADCOOKIE "Action to be taken on invalid cookie"
#define CMT_COOKIE_REJECT_DOMAINS "Domains to reject cookies from"
#define CMT_COOKIE_ACCEPT_DOMAINS "Domains to accept cookies from"
#endif
#define CMT_FOLLOW_REDIRECTION "Number of redirections to follow"
#define CMT_META_REFRESH "Enable processing of meta-refresh tag"

#ifdef USE_MIGEMO
#define CMT_USE_MIGEMO "Enable Migemo (Roma-ji search)"
#define CMT_MIGEMO_COMMAND "Migemo command"
#endif				/* USE_MIGEMO */

#define CMT_KEYMAP_FILE "keymap file"

#endif				/* LANG != JA */

#define PI_TEXT    0
#define PI_ONOFF   1
#define PI_SEL_C   2

struct sel_c {
    int value;
    char *cvalue;
    char *text;
};

#ifdef JP_CHARSET
static struct sel_c kcodestr[] = {
    {CODE_EUC, "E", STR_EUC},
    {CODE_SJIS, "S", STR_SJIS},
    {CODE_JIS_j, "j", STR_JIS_j},
    {CODE_JIS_N, "N", STR_JIS_N},
    {CODE_JIS_m, "m", STR_JIS_m},
    {CODE_JIS_n, "n", STR_JIS_n},
    {0, NULL, NULL}
};

static struct sel_c dcodestr[] = {
    {'\0', "0", "auto detect"},
    {CODE_EUC, "E", STR_EUC},
    {CODE_SJIS, "S", STR_SJIS},
    {CODE_INNER_EUC, "I", STR_INNER_EUC},
    {0, NULL, NULL}
};

static struct sel_c scodestr[] = {
    {CODE_EUC, "E", STR_EUC},
    {CODE_SJIS, "S", STR_SJIS},
    {0, NULL, NULL}
};
#endif				/* JP_CHARSET */

#ifdef USE_COLOR
static struct sel_c colorstr[] = {
#if LANG == JA
    {0, "black", "��"},
    {1, "red", "��"},
    {2, "green", "��"},
    {3, "yellow", "��"},
    {4, "blue", "��"},
    {5, "magenta", "��"},
    {6, "cyan", "����"},
    {7, "white", "��"},
    {8, "terminal", "ü��"},
    {0, NULL, NULL}
#else				/* LANG != JA */
    {0, "black", "black"},
    {1, "red", "red"},
    {2, "green", "green"},
    {3, "yellow", "yellow"},
    {4, "blue", "blue"},
    {5, "magenta", "magenta"},
    {6, "cyan", "cyan"},
    {7, "white", "white"},
    {8, "terminal", "terminal"},
    {0, NULL, NULL}
#endif				/* LANG != JA */
};
#endif				/* USE_COLOR */

#if 1				/* ANSI-C ? */
#define N_STR(x)	#x
#define N_S(x)	(x), N_STR(x)
#else				/* for traditional cpp? */
static char n_s[][2] = {
    {'0', 0},
    {'1', 0},
    {'2', 0},
};
#define N_S(x) (x), n_s[(x)]
#endif


static struct sel_c defaulturls[] = {
#if LANG == JA
    {N_S(DEFAULT_URL_EMPTY), "̵��"},
    {N_S(DEFAULT_URL_CURRENT), "���ߤ�URL"},
    {N_S(DEFAULT_URL_LINK), "������URL"},
#else
    {N_S(DEFAULT_URL_EMPTY), "none"},
    {N_S(DEFAULT_URL_CURRENT), "current URL"},
    {N_S(DEFAULT_URL_LINK), "link URL"},
#endif
    {0, NULL, NULL}
};
#ifdef INET6
static struct sel_c dnsorders[] = {
    {N_S(DNS_ORDER_UNSPEC), "unspecified"},
    {N_S(DNS_ORDER_INET_INET6), "inet inet6"},
    {N_S(DNS_ORDER_INET6_INET), "inet6 inet"},
    {0, NULL, NULL}
};
#endif				/* INET6 */

#ifdef USE_COOKIE
static struct sel_c badcookiestr[] = {
    {N_S(ACCEPT_BAD_COOKIE_DISCARD), "discard"},
#if 0
    {N_S(ACCEPT_BAD_COOKIE_ACCEPT), "accept"},
#endif
    {N_S(ACCEPT_BAD_COOKIE_ASK), "ask"},
    {0, NULL, NULL}
};
#endif				/* USE_COOKIE */

struct param_ptr params1[] = {
    {"tabstop", P_NZINT, PI_TEXT, (void *)&Tabstop, CMT_TABSTOP, NULL},
    {"pixel_per_char", P_PIXELS, PI_TEXT, (void *)&pixel_per_char,
     CMT_PIXEL_PER_CHAR, NULL},
#ifdef USE_IMAGE
    {"pixel_per_line", P_PIXELS, PI_TEXT, (void *)&pixel_per_line,
     CMT_PIXEL_PER_LINE, NULL},
#endif
#ifdef JP_CHARSET
    {"kanjicode", P_CODE, PI_SEL_C, (void *)&DisplayCode, CMT_KANJICODE,
     kcodestr},
    {"document_code", P_CODE, PI_SEL_C, (void *)&DocumentCode,
     CMT_DOCUMENTCODE, dcodestr},
    {"system_code", P_CODE, PI_SEL_C, (void *)&SystemCode, CMT_SYSTEMCODE,
     scodestr},
#endif				/* JP_CHARSET */
    {"frame", P_CHARINT, PI_ONOFF, (void *)&RenderFrame, CMT_FRAME, NULL},
    {"target_self", P_CHARINT, PI_ONOFF, (void *)&TargetSelf, CMT_TSELF, NULL},
    {"display_link", P_INT, PI_ONOFF, (void *)&displayLink, CMT_DISPLINK,
     NULL},
    {"display_lineinfo", P_INT, PI_ONOFF, (void *)&displayLineInfo,
     CMT_DISPLINEINFO, NULL},
    {"ext_dirlist", P_INT, PI_ONOFF, (void *)&UseExternalDirBuffer,
     CMT_EXT_DIRLIST, NULL},
    {"dirlist_cmd", P_STRING, PI_TEXT, (void *)&DirBufferCommand,
     CMT_DIRLIST_CMD, NULL},
#ifdef USE_DICT
    {"use_dictcommand", P_INT, PI_ONOFF, (void *)&UseDictCommand,
     CMT_USE_DICTCOMMAND, NULL},
    {"dictcommand", P_STRING, PI_TEXT, (void *)&DictCommand,
     CMT_DICTCOMMAND, NULL},
#endif				/* USE_DICT */
    {"multicol", P_INT, PI_ONOFF, (void *)&multicolList, CMT_MULTICOL, NULL},
    {"alt_entity", P_CHARINT, PI_ONOFF, (void *)&UseAltEntity, CMT_ALT_ENTITY,
     NULL},
    {"fold_textarea", P_CHARINT, PI_ONOFF, (void *)&FoldTextarea,
     CMT_FOLD_TEXTAREA, NULL},
    {"ignore_null_img_alt", P_INT, PI_ONOFF, (void *)&ignore_null_img_alt,
     CMT_IGNORE_NULL_IMG_ALT, NULL},
    {"view_unseenobject", P_INT, PI_ONOFF, (void *)&view_unseenobject,
     CMT_VIEW_UNSEENOBJECTS, NULL},
    /* XXX: emacs-w3m force to off display_image even if image options off */
    {"display_image", P_INT, PI_ONOFF, (void *)&displayImage, CMT_DISP_IMAGE,
     NULL},
#ifdef USE_IMAGE
    {"auto_image", P_INT, PI_ONOFF, (void *)&autoImage, CMT_AUTO_IMAGE, NULL},
    {"max_load_image", P_INT, PI_TEXT, (void *)&maxLoadImage,
     CMT_MAX_LOAD_IMAGE, NULL},
    {"ext_image_viewer", P_INT, PI_ONOFF, (void *)&useExtImageViewer,
     CMT_EXT_IMAGE_VIEWER, NULL},
    {"image_scale", P_SCALE, PI_TEXT, (void *)&image_scale, CMT_IMAGE_SCALE,
     NULL},
    {"imgdisplay", P_STRING, PI_TEXT, (void *)&Imgdisplay, CMT_IMGDISPLAY,
     NULL},
#endif
    {"show_lnum", P_INT, PI_ONOFF, (void *)&showLineNum, CMT_SHOW_NUM, NULL},
    {"show_srch_str", P_INT, PI_ONOFF, (void *)&show_srch_str,
     CMT_SHOW_SRCH_STR, NULL},
#ifdef LABEL_TOPLINE
    {"label_topline", P_INT, PI_ONOFF, (void *)&label_topline,
     CMT_LABEL_TOPLINE, NULL},
#endif
#ifdef NEXTPAGE_TOPLINE
    {"nextpage_topline", P_INT, PI_ONOFF, (void *)&nextpage_topline,
     CMT_NEXTPAGE_TOPLINE, NULL},
#endif
    {NULL, 0, 0, NULL, NULL, NULL},
};

#ifdef USE_COLOR
struct param_ptr params2[] = {
    {"color", P_INT, PI_ONOFF, (void *)&useColor, CMT_COLOR, NULL},
    {"basic_color", P_COLOR, PI_SEL_C, (void *)&basic_color, CMT_B_COLOR,
     colorstr},
    {"anchor_color", P_COLOR, PI_SEL_C, (void *)&anchor_color, CMT_A_COLOR,
     colorstr},
    {"image_color", P_COLOR, PI_SEL_C, (void *)&image_color, CMT_I_COLOR,
     colorstr},
    {"form_color", P_COLOR, PI_SEL_C, (void *)&form_color, CMT_F_COLOR,
     colorstr},
#ifdef USE_BG_COLOR
    {"mark_color", P_COLOR, PI_SEL_C, (void *)&mark_color, CMT_MARK_COLOR,
     colorstr},
    {"bg_color", P_COLOR, PI_SEL_C, (void *)&bg_color, CMT_BG_COLOR, colorstr},
#endif				/* USE_BG_COLOR */
    {"active_style", P_INT, PI_ONOFF, (void *)&useActiveColor,
     CMT_ACTIVE_STYLE, NULL},
    {"active_color", P_COLOR, PI_SEL_C, (void *)&active_color, CMT_C_COLOR,
     colorstr},
    {"visited_anchor", P_INT, PI_ONOFF, (void *)&useVisitedColor,
     CMT_VISITED_ANCHOR, NULL},
    {"visited_color", P_COLOR, PI_SEL_C, (void *)&visited_color, CMT_V_COLOR,
     colorstr},
    {NULL, 0, 0, NULL, NULL, NULL},
};
#endif				/* USE_COLOR */


struct param_ptr params3[] = {
    {"pagerline", P_NZINT, PI_TEXT, (void *)&PagerMax, CMT_PAGERLINE, NULL},
#ifdef USE_HISTORY
    {"history", P_INT, PI_TEXT, (void *)&URLHistSize, CMT_HISTSIZE, NULL},
    {"save_hist", P_INT, PI_ONOFF, (void *)&SaveURLHist, CMT_SAVEHIST, NULL},
#endif				/* USE_HISTORY */
    {"confirm_qq", P_INT, PI_ONOFF, (void *)&confirm_on_quit, CMT_CONFIRM_QQ,
     NULL},
#ifdef USE_MARK
    {"mark", P_INT, PI_ONOFF, (void *)&use_mark, CMT_USE_MARK, NULL},
#endif
#ifdef EMACS_LIKE_LINEEDIT
    {"emacs_like_lineedit", P_INT, PI_ONOFF, (void *)&emacs_like_lineedit,
     CMT_EMACS_LIKE_LINEEDIT, NULL},
#endif
#ifdef VI_PREC_NUM
    {"vi_prec_num", P_INT, PI_ONOFF, (void *)&vi_prec_num, CMT_VI_PREC_NUM,
     NULL},
#endif
    {"mark_all_pages", P_INT, PI_ONOFF, (void *)&MarkAllPages,
     CMT_MARK_ALL_PAGES, NULL},
    {"wrap_search", P_INT, PI_ONOFF, (void *)&WrapDefault, CMT_WRAP, NULL},
    {"ignorecase_search", P_INT, PI_ONOFF, (void *)&IgnoreCase,
     CMT_IGNORE_CASE, NULL},
#ifdef USE_MIGEMO
    {"use_migemo", P_INT, PI_ONOFF, (void *)&use_migemo, CMT_USE_MIGEMO,
     NULL},
    {"migemo_command", P_STRING, PI_TEXT, (void *)&migemo_command,
     CMT_MIGEMO_COMMAND, NULL},
#endif				/* USE_MIGEMO */
#ifdef USE_MOUSE
    {"use_mouse", P_INT, PI_ONOFF, (void *)&use_mouse, CMT_MOUSE, NULL},
    {"reverse_mouse", P_INT, PI_ONOFF, (void *)&reverse_mouse,
     CMT_REVERSE_MOUSE, NULL},
#endif				/* USE_MOUSE */
    {"clear_buffer", P_INT, PI_ONOFF, (void *)&clear_buffer, CMT_CLEAR_BUF,
     NULL},
    {"decode_cte", P_CHARINT, PI_ONOFF, (void *)&DecodeCTE, CMT_DECODE_CTE,
     NULL},
    {"keymap_file", P_STRING, PI_TEXT, (void *)&keymap_file, CMT_KEYMAP_FILE,
     NULL},
    {NULL, 0, 0, NULL, NULL, NULL},
};

struct param_ptr params4[] = {
    {"http_proxy", P_STRING, PI_TEXT, (void *)&HTTP_proxy, CMT_HTTP_PROXY,
     NULL},
#ifdef USE_GOPHER
    {"gopher_proxy", P_STRING, PI_TEXT, (void *)&GOPHER_proxy,
     CMT_GOPHER_PROXY, NULL},
#endif				/* USE_GOPHER */
    {"ftp_proxy", P_STRING, PI_TEXT, (void *)&FTP_proxy, CMT_FTP_PROXY, NULL},
    {"no_proxy", P_STRING, PI_TEXT, (void *)&NO_proxy, CMT_NO_PROXY, NULL},
    {"noproxy_netaddr", P_INT, PI_ONOFF, (void *)&NOproxy_netaddr,
     CMT_NOPROXY_NETADDR, NULL},
    {"no_cache", P_CHARINT, PI_ONOFF, (void *)&NoCache, CMT_NO_CACHE, NULL},

    {NULL, 0, 0, NULL, NULL, NULL},
};

struct param_ptr params5[] = {
    {"document_root", P_STRING, PI_TEXT, (void *)&document_root, CMT_DROOT,
     NULL},
    {"personal_document_root", P_STRING, PI_TEXT,
     (void *)&personal_document_root, CMT_PDROOT, NULL},
    {"cgi_bin", P_STRING, PI_TEXT, (void *)&cgi_bin, CMT_CGIBIN, NULL},
    {"index_file", P_STRING, PI_TEXT, (void *)&index_file, CMT_IFILE, NULL},
    {NULL, 0, 0, NULL, NULL, NULL},
};

struct param_ptr params6[] = {
    {"mime_types", P_STRING, PI_TEXT, (void *)&mimetypes_files, CMT_MIMETYPES,
     NULL},
    {"mailcap", P_STRING, PI_TEXT, (void *)&mailcap_files, CMT_MAILCAP, NULL},
#ifdef USE_EXTERNAL_URI_LOADER
    {"urimethodmap", P_STRING, PI_TEXT, (void *)&urimethodmap_files,
     CMT_URIMETHODMAP, NULL},
#endif
    {"editor", P_STRING, PI_TEXT, (void *)&Editor, CMT_EDITOR, NULL},
    {"mailer", P_STRING, PI_TEXT, (void *)&Mailer, CMT_MAILER, NULL},
    {"extbrowser", P_STRING, PI_TEXT, (void *)&ExtBrowser, CMT_EXTBRZ, NULL},
    {"extbrowser2", P_STRING, PI_TEXT, (void *)&ExtBrowser2, CMT_EXTBRZ2,
     NULL},
    {"extbrowser3", P_STRING, PI_TEXT, (void *)&ExtBrowser3, CMT_EXTBRZ3,
     NULL},
    {"bgextviewer", P_INT, PI_ONOFF, (void *)&BackgroundExtViewer,
     CMT_BGEXTVIEW, NULL},
    {"use_lessopen", P_INT, PI_ONOFF, (void *)&use_lessopen, CMT_USE_LESSOPEN,
     NULL},
    {NULL, 0, 0, NULL, NULL, NULL},
};

#ifdef USE_SSL
struct param_ptr params7[] = {
    {"ssl_forbid_method", P_STRING, PI_TEXT, (void *)&ssl_forbid_method,
     CMT_SSL_FORBID_METHOD, NULL},
#ifdef USE_SSL_VERIFY
    {"ssl_verify_server", P_INT, PI_ONOFF, (void *)&ssl_verify_server,
     CMT_SSL_VERIFY_SERVER, NULL},
    {"ssl_cert_file", P_SSLPATH, PI_TEXT, (void *)&ssl_cert_file,
     CMT_SSL_CERT_FILE, NULL},
    {"ssl_key_file", P_SSLPATH, PI_TEXT, (void *)&ssl_key_file,
     CMT_SSL_KEY_FILE, NULL},
    {"ssl_ca_path", P_SSLPATH, PI_TEXT, (void *)&ssl_ca_path, CMT_SSL_CA_PATH,
     NULL},
    {"ssl_ca_file", P_SSLPATH, PI_TEXT, (void *)&ssl_ca_file, CMT_SSL_CA_FILE,
     NULL},
#endif				/* USE_SSL_VERIFY */
    {NULL, 0, 0, NULL, NULL, NULL},
};
#endif				/* USE_SSL */

#ifdef USE_COOKIE
struct param_ptr params8[] = {
    {"use_cookie", P_INT, PI_ONOFF, (void *)&use_cookie, CMT_USECOOKIE, NULL},
    {"accept_cookie", P_INT, PI_ONOFF, (void *)&accept_cookie,
     CMT_ACCEPTCOOKIE, NULL},
    {"accept_bad_cookie", P_INT, PI_SEL_C, (void *)&accept_bad_cookie,
     CMT_ACCEPTBADCOOKIE, badcookiestr},
    {"cookie_reject_domains", P_STRING, PI_TEXT,
     (void *)&cookie_reject_domains, CMT_COOKIE_REJECT_DOMAINS, NULL},
    {"cookie_accept_domains", P_STRING, PI_TEXT,
     (void *)&cookie_accept_domains, CMT_COOKIE_ACCEPT_DOMAINS, NULL},
    {NULL, 0, 0, NULL, NULL, NULL},
};
#endif
struct param_ptr params9[] = {
    {"passwd_file", P_STRING, PI_TEXT, (void *)&passwd_file, CMT_PASSWDFILE,
     NULL},
    {"ftppasswd", P_STRING, PI_TEXT, (void *)&ftppasswd, CMT_FTPPASS, NULL},
#ifdef FTPPASS_HOSTNAMEGEN
    {"ftppass_hostnamegen", P_INT, PI_ONOFF, (void *)&ftppass_hostnamegen,
     CMT_FTPPASS_HOSTNAMEGEN, NULL},
#endif
    {"user_agent", P_STRING, PI_TEXT, (void *)&UserAgent, CMT_USERAGENT, NULL},
    {"no_referer", P_INT, PI_ONOFF, (void *)&NoSendReferer, CMT_NOSENDREFERER,
     NULL},
    {"accept_language", P_STRING, PI_TEXT, (void *)&AcceptLang, CMT_ACCEPTLANG,
     NULL},
    {"accept_encoding", P_STRING, PI_TEXT, (void *)&AcceptEncoding,
     CMT_ACCEPTENCODING,
     NULL},
    {"accept_media", P_STRING, PI_TEXT, (void *)&AcceptMedia, CMT_ACCEPTMEDIA,
     NULL},
    {"argv_is_url", P_CHARINT, PI_ONOFF, (void *)&ArgvIsURL, CMT_ARGV_IS_URL,
     NULL},
    {"retry_http", P_INT, PI_ONOFF, (void *)&retryAsHttp, CMT_RETRY_HTTP,
     NULL},
    {"default_url", P_INT, PI_SEL_C, (void *)&DefaultURLString,
     CMT_DEFAULT_URL,
     defaulturls},
    {"follow_redirection", P_INT, PI_TEXT, &FollowRedirection,
     CMT_FOLLOW_REDIRECTION, NULL},
    {"meta_refresh", P_CHARINT, PI_ONOFF, (void *)&MetaRefresh,
     CMT_META_REFRESH, NULL},
#ifdef INET6
    {"dns_order", P_INT, PI_SEL_C, (void *)&DNS_order, CMT_DNS_ORDER,
     dnsorders},
#endif				/* INET6 */
    {NULL, 0, 0, NULL, NULL, NULL},
};

struct param_section sections[] = {
#if LANG == JA
    {"ɽ���ط�", params1},
#ifdef USE_COLOR
    {"ɽ����", params2},
#endif				/* USE_COLOR */
    {"��¿������", params3},
    {"�ǥ��쥯�ȥ�����", params5},
    {"�����ץ����", params6},
    {"�ͥåȥ��������", params9},
    {"�ץ���������", params4},
#ifdef USE_SSL
    {"SSL������", params7},
#endif
#ifdef USE_COOKIE
    {"���å���������", params8},
#endif
#else				/* LANG != JA */
    {"Display Settings", params1},
#ifdef USE_COLOR
    {"Color Settings", params2},
#endif				/* USE_COLOR */
    {"Miscellaneous Settings", params3},
    {"Directory Settings", params5},
    {"External Program Settings", params6},
    {"Network Settings", params9},
    {"Proxy Settings", params4},
#ifdef USE_SSL
    {"SSL Settings", params7},
#endif
#ifdef USE_COOKIE
    {"Cookie Settings", params8},
#endif
#endif				/* LANG != JA */
    {NULL, NULL}
};

static Str to_str(struct param_ptr *p);

static int
compare_table(struct rc_search_table *a, struct rc_search_table *b)
{
    return strcmp(a->param->name, b->param->name);
}

void
create_option_search_table()
{
    int i, j, k;
    int diff1, diff2;
    char *p, *q;

    /* count table size */
    RC_table_size = 0;
    for (j = 0; sections[j].name != NULL; j++) {
	i = 0;
	while (sections[j].params[i].name) {
	    i++;
	    RC_table_size++;
	}
    }

    RC_search_table = New_N(struct rc_search_table, RC_table_size);
    k = 0;
    for (j = 0; sections[j].name != NULL; j++) {
	i = 0;
	while (sections[j].params[i].name) {
	    RC_search_table[k].param = &sections[j].params[i];
	    k++;
	    i++;
	}
    }

    qsort(RC_search_table, RC_table_size, sizeof(struct rc_search_table),
	  (int (*)(const void *, const void *))compare_table);

    diff1 = diff2 = 0;
    for (i = 0; i < RC_table_size - 1; i++) {
	p = RC_search_table[i].param->name;
	q = RC_search_table[i + 1].param->name;
	for (j = 0; p[j] != '\0' && q[j] != '\0' && p[j] == q[j]; j++) ;
	diff1 = j;
	if (diff1 > diff2)
	    RC_search_table[i].uniq_pos = diff1 + 1;
	else
	    RC_search_table[i].uniq_pos = diff2 + 1;
	diff2 = diff1;
    }
}

struct param_ptr *
search_param(char *name)
{
    size_t b, e, i;
    int cmp;
    int len = strlen(name);

    for (b = 0, e = RC_table_size - 1; b <= e;) {
	i = (b + e) / 2;
	cmp = strncmp(name, RC_search_table[i].param->name, len);

	if (!cmp) {
	    if (len >= RC_search_table[i].uniq_pos) {
		return RC_search_table[i].param;
	    }
	    else {
		while ((cmp =
			strcmp(name, RC_search_table[i].param->name)) <= 0)
		    if (!cmp)
			return RC_search_table[i].param;
		    else if (i == 0)
			return NULL;
		    else
			i--;
		/* ambiguous */
		return NULL;
	    }
	}
	else if (cmp < 0) {
	    if (i == 0)
		return NULL;
	    e = i - 1;
	}
	else
	    b = i + 1;
    }
    return NULL;
}

void
show_params(FILE * fp)
{
    int i, j, l;
    char *t = NULL;
    char *cmt;

    fputs("\nconfiguration parameters\n", fp);
    for (j = 0; sections[j].name != NULL; j++) {
#ifdef JP_CHARSET
	if (InnerCode != DisplayCode)
	    cmt = conv(sections[j].name, InnerCode, DisplayCode)->ptr;
	else
#endif				/* JP_CHARSET */
	    cmt = sections[j].name;
	fprintf(fp, "  section[%d]: %s\n", j, cmt);
	i = 0;
	while (sections[j].params[i].name) {
	    switch (sections[j].params[i].type) {
	    case P_INT:
	    case P_SHORT:
	    case P_CHARINT:
	    case P_NZINT:
		t = (sections[j].params[i].inputtype ==
		     PI_ONOFF) ? "bool" : "number";
		break;
	    case P_CHAR:
		t = "char";
		break;
	    case P_STRING:
		t = "string";
		break;
#if defined(USE_SSL) && defined(USE_SSL_VERIFY)
	    case P_SSLPATH:
		t = "path";
		break;
#endif
#ifdef USE_COLOR
	    case P_COLOR:
		t = "color";
		break;
#endif
#ifdef JP_CHARSET
	    case P_CODE:
		t = "E|S|j|N|m|n";
		break;
#endif
	    case P_PIXELS:
		t = "number";
		break;
	    case P_SCALE:
		t = "percent";
		break;
	    }
#ifdef JP_CHARSET
	    if (InnerCode != DisplayCode)
		cmt = conv(sections[j].params[i].comment,
			   InnerCode, DisplayCode)->ptr;
	    else
#endif				/* JP_CHARSET */
		cmt = sections[j].params[i].comment;
	    l = 30 - (strlen(sections[j].params[i].name) + strlen(t));
	    if (l < 0)
		l = 1;
	    fprintf(fp, "    -o %s=<%s>%*s%s\n",
		    sections[j].params[i].name, t, l, " ", cmt);
	    i++;
	}
    }
}

int
str_to_bool(char *value, int old)
{
    if (value == NULL)
	return 1;
    switch (tolower(*value)) {
    case '0':
    case 'f':			/* false */
    case 'n':			/* no */
    case 'u':			/* undef */
	return 0;
    case 'o':
	if (tolower(value[1]) == 'f')	/* off */
	    return 0;
	return 1;		/* on */
    case 't':
	if (tolower(value[1]) == 'o')	/* toggle */
	    return !old;
	return 1;		/* true */
    case '!':
    case 'r':			/* reverse */
    case 'x':			/* exchange */
	return !old;
    }
    return 1;
}

#ifdef USE_COLOR
static int
str_to_color(char *value)
{
    if (value == NULL)
	return 8;		/* terminal */
    switch (tolower(*value)) {
    case '0':
	return 0;		/* black */
    case '1':
    case 'r':
	return 1;		/* red */
    case '2':
    case 'g':
	return 2;		/* green */
    case '3':
    case 'y':
	return 3;		/* yellow */
    case '4':
	return 4;		/* blue */
    case '5':
    case 'm':
	return 5;		/* magenta */
    case '6':
    case 'c':
	return 6;		/* cyan */
    case '7':
    case 'w':
	return 7;		/* white */
    case '8':
    case 't':
	return 8;		/* terminal */
    case 'b':
	if (!strncasecmp(value, "blu", 3))
	    return 4;		/* blue */
	else
	    return 0;		/* black */
    }
    return 8;			/* terminal */
}
#endif

#ifdef JP_CHARSET
char
str_to_code(char *str)
{
    if (str == NULL)
	return CODE_ASCII;
    switch (*str) {
    case CODE_ASCII:
	return CODE_ASCII;
    case CODE_EUC:
    case 'e':
	return CODE_EUC;
    case CODE_SJIS:
    case 's':
	return CODE_SJIS;
    case CODE_JIS_n:
	return CODE_JIS_n;
    case CODE_JIS_m:
	return CODE_JIS_m;
    case CODE_JIS_N:
	return CODE_JIS_N;
    case CODE_JIS_j:
	return CODE_JIS_j;
    case CODE_JIS_J:
	return CODE_JIS_J;
    case CODE_INNER_EUC:
	return CODE_INNER_EUC;
    }
    return CODE_ASCII;
}

char *
code_to_str(char code)
{
    switch (code) {
    case CODE_ASCII:
	return STR_ASCII;
    case CODE_EUC:
	return STR_EUC;
    case CODE_SJIS:
	return STR_SJIS;
    case CODE_JIS_n:
	return STR_JIS_n;
    case CODE_JIS_m:
	return STR_JIS_m;
    case CODE_JIS_N:
	return STR_JIS_N;
    case CODE_JIS_j:
	return STR_JIS_j;
    case CODE_JIS_J:
	return STR_JIS_J;
    case CODE_INNER_EUC:
	return STR_INNER_EUC;
    }
    return "unknown";
}
#endif

static int
set_param(char *name, char *value)
{
    struct param_ptr *p;
    double ppc;

    if (value == NULL)
	return 0;
    p = search_param(name);
    if (p == NULL)
	return 0;
    switch (p->type) {
    case P_INT:
	if (atoi(value) >= 0)
	    *(int *)p->varptr = (p->inputtype == PI_ONOFF)
		? str_to_bool(value, *(int *)p->varptr) : atoi(value);
	break;
    case P_NZINT:
	if (atoi(value) > 0)
	    *(int *)p->varptr = atoi(value);
	break;
    case P_SHORT:
	*(short *)p->varptr = (p->inputtype == PI_ONOFF)
	    ? str_to_bool(value, *(short *)p->varptr) : atoi(value);
	break;
    case P_CHARINT:
	*(char *)p->varptr = (p->inputtype == PI_ONOFF)
	    ? str_to_bool(value, *(char *)p->varptr) : atoi(value);
	break;
    case P_CHAR:
	*(char *)p->varptr = value[0];
	break;
    case P_STRING:
	*(char **)p->varptr = value;
	break;
#if defined(USE_SSL) && defined(USE_SSL_VERIFY)
    case P_SSLPATH:
	if (value != NULL && value[0] != '\0')
	    *(char **)p->varptr = rcFile(value);
	else
	    *(char **)p->varptr = NULL;
	ssl_path_modified = 1;
	break;
#endif
#ifdef USE_COLOR
    case P_COLOR:
	*(int *)p->varptr = str_to_color(value);
	break;
#endif
#ifdef JP_CHARSET
    case P_CODE:
	*(char *)p->varptr = str_to_code(value);
	break;
#endif
    case P_PIXELS:
	ppc = atof(value);
	if (ppc >= MINIMUM_PIXEL_PER_CHAR && ppc <= MAXIMUM_PIXEL_PER_CHAR * 2)
	    *(double *)p->varptr = ppc;
	break;
    case P_SCALE:
	ppc = atof(value);
	if (ppc >= 10 && ppc <= 1000)
	    *(double *)p->varptr = ppc;
	break;
    }
    return 1;
}

int
set_param_option(char *option)
{
    Str tmp = Strnew();
    char *p = option, *q;

    while (*p && !IS_SPACE(*p) && *p != '=')
	Strcat_char(tmp, *p++);
    while (*p && IS_SPACE(*p))
	p++;
    if (*p == '=') {
	p++;
	while (*p && IS_SPACE(*p))
	    p++;
    }
    Strlower(tmp);
    if (set_param(tmp->ptr, p))
	goto option_assigned;
    q = tmp->ptr;
    if (!strncmp(q, "no", 2)) {	/* -o noxxx, -o no-xxx, -o no_xxx */
	q += 2;
	if (*q == '-' || *q == '_')
	    q++;
    }
    else if (tmp->ptr[0] == '-')	/* -o -xxx */
	q++;
    else
	return 0;
    if (set_param(q, "0"))
	goto option_assigned;
    return 0;
  option_assigned:
    return 1;
}

char *
get_param_option(char *name)
{
    struct param_ptr *p;

    p = search_param(name);
    return p ? to_str(p)->ptr : NULL;
}

static void
interpret_rc(FILE * f)
{
    Str line;
    Str tmp;
    char *p;

    for (;;) {
	line = Strfgets(f);
	Strchop(line);
	if (line->length == 0)
	    break;
	Strremovefirstspaces(line);
	if (line->ptr[0] == '#')	/* comment */
	    continue;
	tmp = Strnew();
	p = line->ptr;
	while (*p && !IS_SPACE(*p))
	    Strcat_char(tmp, *p++);
	while (*p && IS_SPACE(*p))
	    p++;
	Strlower(tmp);
	set_param(tmp->ptr, p);
    }
}

void
parse_proxy()
{
    if (non_null(HTTP_proxy))
	parseURL(HTTP_proxy, &HTTP_proxy_parsed, NULL);
#ifdef USE_GOPHER
    if (non_null(GOPHER_proxy))
	parseURL(GOPHER_proxy, &GOPHER_proxy_parsed, NULL);
#endif				/* USE_GOPHER */
    if (non_null(FTP_proxy))
	parseURL(FTP_proxy, &FTP_proxy_parsed, NULL);
    if (non_null(NO_proxy))
	set_no_proxy(NO_proxy);
}

#ifdef USE_COOKIE
void
parse_cookie()
{
    if (non_null(cookie_reject_domains))
	Cookie_reject_domains = make_domain_list(cookie_reject_domains);
    if (non_null(cookie_accept_domains))
	Cookie_accept_domains = make_domain_list(cookie_accept_domains);
}
#endif

#ifdef __EMX__
static int
do_mkdir(const char *dir, long mode)
{
    char *r, abs[_MAX_PATH];
    size_t n;

    _abspath(abs, rc_dir, _MAX_PATH);	/* Translate '\\' to '/' */

    if (!(n = strlen(abs)))
	return -1;

    if (*(r = abs + n - 1) == '/')	/* Ignore tailing slash if it is */
	*r = 0;

    return mkdir(abs, mode);
}
#else				/* not __EMX__ */
#define do_mkdir(dir,mode) mkdir(dir,mode)
#endif				/* not __EMX__ */

void
sync_with_option(void)
{
    WrapSearch = WrapDefault;
    parse_proxy();
#ifdef USE_COOKIE
    parse_cookie();
#endif
    initMailcap();
    initMimeTypes();
#ifdef USE_EXTERNAL_URI_LOADER
    initURIMethods();
#endif
#ifdef USE_MIGEMO
    init_migemo();
#endif
#ifdef USE_IMAGE
    if (fmInitialized && displayImage)
	initImage();
#else
    displayImage = FALSE;	/* XXX */
#endif
    loadPasswd();

    if (AcceptLang == NULL || *AcceptLang == '\0') {
#if LANG == JA
	AcceptLang = "ja;q=1.0, en;q=0.5";
#else				/* LANG != JA (must be EN) */
	AcceptLang = "en;q=1.0";
#endif
    }
    if (AcceptEncoding == NULL || *AcceptEncoding == '\0')
	AcceptEncoding = acceptableEncoding();
    if (AcceptMedia == NULL || *AcceptMedia == '\0')
	AcceptMedia = acceptableMimeTypes();
    initKeymap(FALSE);
}

void
init_rc(char *config_filename)
{
    struct stat st;
    FILE *f;
    char *tmpdir;

    if (((tmpdir = getenv("TMP")) == NULL || *tmpdir == '\0')
	&& ((tmpdir = getenv("TEMP")) == NULL || *tmpdir == '\0')
	&& ((tmpdir = getenv("TMPDIR")) == NULL || *tmpdir == '\0'))
	tmpdir = "/tmp";

    if (stat(rc_dir, &st) < 0) {
	if (errno == ENOENT) {	/* no directory */
	    if (do_mkdir(rc_dir, 0700) < 0) {
		fprintf(stderr, "Can't create config directory (%s)!", rc_dir);
		rc_dir = tmpdir;
		rc_dir_is_tmp = TRUE;
		return;
	    }
	    else {
		stat(rc_dir, &st);
	    }
	}
	else {
	    fprintf(stderr, "Can't open config directory (%s)!", rc_dir);
	    rc_dir = tmpdir;
	    rc_dir_is_tmp = TRUE;
	    return;
	}
    }
    if (!S_ISDIR(st.st_mode)) {
	/* not a directory */
	fprintf(stderr, "%s is not a directory!", rc_dir);
	rc_dir = tmpdir;
	rc_dir_is_tmp = TRUE;
	return;
    }

    /* open config file */
    if ((f = fopen(etcFile(W3MCONFIG), "rt")) != NULL) {
	interpret_rc(f);
	fclose(f);
    }
    config_file = config_filename;
    if (config_file == NULL)
	config_file = rcFile(CONFIG_FILE);
    if ((f = fopen(config_file, "rt")) != NULL) {
	interpret_rc(f);
	fclose(f);
    }
    sync_with_option();
}


static char optionpanel_src1[] =
    "<html><head><title>Option Setting Panel</title></head>\
<body><center><b>Option Setting Panel</b><br><b>(w3m version %s)</b></center><p>\n" "<a href=\"file:///$LIB/" W3MHELPERPANEL_CMDNAME "?mode=panel\">%s</a>\n" "<form method=internal action=option>";

static Str
to_str(struct param_ptr *p)
{
    switch (p->type) {
    case P_INT:
#ifdef USE_COLOR
    case P_COLOR:
#endif
    case P_NZINT:
	return Sprintf("%d", *(int *)p->varptr);
    case P_SHORT:
	return Sprintf("%d", *(short *)p->varptr);
    case P_CHARINT:
	return Sprintf("%d", *(char *)p->varptr);
    case P_CHAR:
#ifdef JP_CHARSET
    case P_CODE:
#endif
	return Sprintf("%c", *(char *)p->varptr);
    case P_STRING:
#if defined(USE_SSL) && defined(USE_SSL_VERIFY)
    case P_SSLPATH:
#endif
	return Strnew_charp(*(char **)p->varptr);
    case P_PIXELS:
    case P_SCALE:
	return Sprintf("%g", *(double *)p->varptr);
    }
    /* not reached */
    return NULL;
}

Buffer *
load_option_panel(void)
{
    Str src = Sprintf(optionpanel_src1, w3m_version, CMT_HELPER);
    struct param_ptr *p;
    struct sel_c *s;
    int x, i;
    Str tmp;

    Strcat_charp(src, "<table><tr><td>");
    for (i = 0; sections[i].name != NULL; i++) {
	Strcat_m_charp(src, "<h1>", sections[i].name, "</h1>", NULL);
	p = sections[i].params;
	Strcat_charp(src, "<table width=100% cellpadding=0>");
	while (p->name) {
	    Strcat_m_charp(src, "<tr><td>", p->comment, NULL);
	    Strcat(src, Sprintf("</td><td width=%d>",
				(int)(28 * pixel_per_char)));
	    switch (p->inputtype) {
	    case PI_TEXT:
		Strcat_m_charp(src, "<input type=text name=",
			       p->name,
			       " value=\"",
			       html_quote(to_str(p)->ptr), "\">", NULL);
		break;
	    case PI_ONOFF:
		x = atoi(to_str(p)->ptr);
		Strcat_m_charp(src, "<input type=radio name=",
			       p->name,
			       " value=1",
			       (x ? " checked" : ""),
			       ">YES&nbsp;&nbsp;<input type=radio name=",
			       p->name,
			       " value=0", (x ? "" : " checked"), ">NO", NULL);
		break;
	    case PI_SEL_C:
		tmp = to_str(p);
		Strcat_m_charp(src, "<select name=", p->name, ">", NULL);
		for (s = p->select; s->text != NULL; s++) {
		    Strcat_charp(src, "<option value=");
		    Strcat(src, Sprintf("%s\n", s->cvalue));
		    if ((p->type != P_CHAR &&
#ifdef JP_CHARSET
			 p->type != P_CODE &&
#endif
			 s->value == atoi(tmp->ptr)) || ((p->type == P_CHAR
#ifdef JP_CHARSET
							  || p->type == P_CODE
#endif
							 )
							 && (char)(s->value) ==
							 *(tmp->ptr)))
			Strcat_charp(src, " selected");
		    Strcat_char(src, '>');
		    Strcat_charp(src, s->text);
		}
		Strcat_charp(src, "</select>");
	    }
	    Strcat_charp(src, "</td></tr>\n");
	    p++;
	}
	Strcat_charp(src,
		     "<tr><td></td><td><p><input type=submit value=\"OK\"></td></tr>");
	Strcat_charp(src, "</table><hr width=50%>");
    }
    Strcat_charp(src, "</table></form></body></html>");
    return loadHTMLString(src);
}

void
panel_set_option(struct parsed_tagarg *arg)
{
    FILE *f = NULL;

    if (rc_dir_is_tmp) {
	disp_message("There's no ~/.w3m directory... config not saved", FALSE);
    }
    else {
	f = fopen(config_file, "wt");
	if (f == NULL) {
	    disp_message("Can't write option!", FALSE);
	}
    }
    while (arg) {
	if (set_param(arg->arg, arg->value)) {
	    if (f)
		fprintf(f, "%s %s\n", arg->arg, arg->value);
	}
	arg = arg->next;
    }
    if (f)
	fclose(f);
    sync_with_option();
    backBf();
}

char *
rcFile(char *base)
{
    if (base &&
	(base[0] == '/' ||
	 (base[0] == '.'
	  && (base[1] == '/' || (base[1] == '.' && base[2] == '/')))
	 || (base[0] == '~' && base[1] == '/')))
	return expandName(base);
    else {
	Str file = Strnew_charp(rc_dir);

	if (Strlastchar(file) != '/')
	    Strcat_char(file, '/');
	Strcat_charp(file, base);
	return expandName(file->ptr);
    }
}

char *
libFile(char *base)
{
    Str file = Strnew_charp(w3m_lib_dir());
    Strcat_char(file, '/');
    Strcat_charp(file, base);
    return expandName(file->ptr);
}

char *
etcFile(char *base)
{
    Str file = Strnew_charp(w3m_etc_dir());
    Strcat_char(file, '/');
    Strcat_charp(file, base);
    return expandName(file->ptr);
}

char *
helpFile(char *base)
{
    Str file = Strnew_charp(w3m_help_dir());
    Strcat_char(file, '/');
    Strcat_charp(file, base);
    return expandName(file->ptr);
}
