/* $Id: file.c,v 1.15 2001/11/25 17:41:57 ukai Exp $ */
#include "fm.h"
#include <sys/types.h>
#include "myctype.h"
#include <signal.h>
#include <setjmp.h>
#include <sys/wait.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
/* foo */

#include "html.h"
#include "parsetagx.h"
#include "local.h"
#include "regex.h"

#ifndef max
#define max(a,b)        ((a) > (b) ? (a) : (b))
#endif				/* not max */
#ifndef min
#define min(a,b)        ((a) > (b) ? (b) : (a))
#endif				/* not min */

static void gunzip_stream(URLFile *uf);
static FILE *lessopen_stream(char *path);
static Buffer *loadcmdout(char *cmd,
			  Buffer *(*loadproc) (URLFile *, Buffer *),
			  Buffer *defaultbuf);
static void close_textarea(struct html_feed_environ *h_env);
static void addnewline(Buffer *buf, char *line, Lineprop *prop,
#ifdef USE_ANSI_COLOR
		       Linecolor *color,
#endif
		       int pos, int nlines);

static Lineprop propBuffer[LINELEN];
#ifdef USE_ANSI_COLOR
static Linecolor colorBuffer[LINELEN];
#endif

static JMP_BUF AbortLoading;

static struct table *tables[MAX_TABLE];
static struct table_mode table_mode[MAX_TABLE];

static Str cur_select;
static Str select_str;
static int select_is_multiple;
static int n_selectitem;
static Str cur_option;
static Str cur_option_value;
static Str cur_option_label;
static int cur_option_selected;
static int cur_status;
#ifdef MENU_SELECT
/* menu based <select>  */
FormSelectOption *select_option;
static int max_select = MAX_SELECT;
static int n_select;
static int cur_option_maxwidth;
#endif				/* MENU_SELECT */

static Str cur_textarea;
Str *textarea_str;
static int cur_textarea_size;
static int cur_textarea_rows;
static int cur_textarea_readonly;
static int n_textarea;
static int ignore_nl_textarea;
static int max_textarea = MAX_TEXTAREA;

static int http_response_code;

#ifdef JP_CHARSET
static char content_charset = '\0';
static char meta_charset = '\0';
static char guess_charset(char *p);
static char check_charset(char *s);
static char check_accept_charset(char *s);
#endif

static Str save_line = NULL;
static int save_prevchar = ' ';

struct link_stack {
    int cmd;
    short offset;
    short pos;
    struct link_stack *next;
};

static struct link_stack *link_stack = NULL;

#define FORMSTACK_SIZE 10
#define FRAMESTACK_SIZE 10

#ifdef USE_NNTP
#define Str_news_endline(s) ((s)->ptr[0]=='.'&&((s)->ptr[1]=='\n'||(s)->ptr[1]=='\r'||(s)->ptr[1]=='\0'))
#endif				/* USE_NNTP */

#define INITIAL_FORM_SIZE 10
static FormList **forms;
static int *form_stack;
static int form_max = 0;
static int forms_size = 0;
#define cur_form_id ((form_sp >= 0)? form_stack[form_sp] : -1)
static int form_sp = 0;

static int current_content_length;

static int cur_hseq;

#define MAX_UL_LEVEL 9
#ifdef KANJI_SYMBOLS
char *ullevel[MAX_UL_LEVEL] = {
    "��", "��", "��", "��", "��", "��", "��", "��", "��"
};
#define HR_RULE "��"
#define HR_RULE_WIDTH 2
#else				/* not KANJI_SYMBOLS */
char *ullevel[MAX_UL_LEVEL] = {
    NBSP "*", NBSP "+", NBSP "o", NBSP "#", NBSP "@", NBSP "-",
    NBSP "=", "**", "--"
};
#define HR_RULE "-"
#define HR_RULE_WIDTH 1
#endif				/* not KANJI_SYMBOLS */

#ifdef USE_COOKIE
/* This array should be somewhere else */
char *violations[COO_EMAX] = {
    "internal error",
    "tail match failed",
    "wrong number of dots",
    "RFC 2109 4.3.2 rule 1",
    "RFC 2109 4.3.2 rule 2.1",
    "RFC 2109 4.3.2 rule 2.2",
    "RFC 2109 4.3.2 rule 3",
    "RFC 2109 4.3.2 rule 4",
    "RFC XXXX 4.3.2 rule 5"
};
#endif

#define SAVE_BUF_SIZE 1536

static MySignalHandler
KeyAbort(SIGNAL_ARG)
{
    LONGJMP(AbortLoading, 1);
    SIGNAL_RETURN;
}

int
currentLn(Buffer *buf)
{
    if (buf->currentLine)
	/*     return buf->currentLine->real_linenumber + 1;      */
	return buf->currentLine->linenumber + 1;
    else
	return 1;
}

static Buffer *
loadSomething(URLFile *f,
	      char *path,
	      Buffer *(*loadproc) (URLFile *, Buffer *), Buffer *defaultbuf)
{
    Buffer *buf;

    if ((buf = loadproc(f, defaultbuf)) == NULL)
	return NULL;

    buf->filename = path;
    if (buf->buffername == NULL || buf->buffername[0] == '\0')
	buf->buffername = conv_from_system(lastFileName(path));
    if (buf->currentURL.scheme == SCM_UNKNOWN)
	buf->currentURL.scheme = f->scheme;
    buf->real_scheme = f->scheme;
    if (f->scheme == SCM_LOCAL && buf->sourcefile == NULL)
	buf->sourcefile = path;
    UFclose(f);
    return buf;
}

int
dir_exist(char *path)
{
    struct stat stbuf;

    if (path == NULL || *path == '\0')
	return 0;
    if (stat(path, &stbuf) == -1)
	return 0;
    return IS_DIRECTORY(stbuf.st_mode);
}

static int
is_dump_text_type(char *type)
{
    struct mailcap *mcap;
    return (type && (mcap = searchExtViewer(type)) &&
	    (mcap->flags & (MAILCAP_HTMLOUTPUT | MAILCAP_COPIOUSOUTPUT)));
}

static int
is_text_type(char *type)
{
    return (type == NULL || type[0] == '\0' ||
	    strncasecmp(type, "text/", 5) == 0 ||
	    strncasecmp(type, "message/", sizeof("message/") - 1) == 0);
}

static int
is_plain_text_type(char *type)
{
    return ((type && strcasecmp(type, "text/plain") == 0) ||
	    (is_text_type(type) && !is_dump_text_type(type)));
}

static void
check_compression(char *path, URLFile *uf)
{
    int len;

    if (path == NULL)
	return;

    len = strlen(path);
    uf->compression = CMP_NOCOMPRESS;
    if (len > 2 && strcasecmp(&path[len - 2], ".Z") == 0) {
	uf->compression = CMP_COMPRESS;
	uf->guess_type = "application/x-compress";
    }
    else if (len > 3 && strcasecmp(&path[len - 3], ".gz") == 0) {
	uf->compression = CMP_GZIP;
	uf->guess_type = "application/x-gzip";
    }
    else if (len > 4 && strcasecmp(&path[len - 4], ".bz2") == 0) {
	uf->compression = CMP_BZIP2;
	uf->guess_type = "application/x-bzip";
    }
}

static char *
compress_application_type(int compression)
{
    switch (compression) {
    case CMP_COMPRESS:
	return "application/x-compress";
    case CMP_GZIP:
	return "application/x-gzip";
    case CMP_BZIP2:
	return "application/x-bzip";
    case CMP_DEFLATE:
	return "application/x-deflate";
    default:
	return NULL;
    }
}

static char *
uncompressed_file_type(char *path, char **ext)
{
    int len, slen;
    Str fn;
    char *t0;

    if (path == NULL)
	return NULL;

    len = strlen(path);
    if (len > 2 && strcasecmp(&path[len - 2], ".Z") == 0) {
	slen = 2;
    }
    else if (len > 3 && strcasecmp(&path[len - 3], ".gz") == 0) {
	slen = 3;
    }
    else if (len > 4 && strcasecmp(&path[len - 4], ".bz2") == 0) {
	slen = 4;
    }
    else
	return NULL;

    fn = Strnew_charp(path);
    Strshrink(fn, slen);
    if (ext)
	*ext = filename_extension(fn->ptr, 0);
    t0 = guessContentType(fn->ptr);
    if (t0 == NULL)
	t0 = "text/plain";
    return t0;
}

void
examineFile(char *path, URLFile *uf)
{
    struct stat stbuf;

    uf->guess_type = NULL;
    if (path == NULL || *path == '\0' ||
	stat(path, &stbuf) == -1 || NOT_REGULAR(stbuf.st_mode)) {
	uf->stream = NULL;
	return;
    }
    uf->stream = openIS(path);
    if (!do_download) {
	if (use_lessopen && getenv("LESSOPEN") != NULL) {
	    FILE *fp;
	    uf->guess_type = guessContentType(path);
	    if (uf->guess_type == NULL)
		uf->guess_type = "text/plain";
	    if (strcasecmp(uf->guess_type, "text/html") == 0)
		return;
	    if ((fp = lessopen_stream(path))) {
		UFclose(uf);
		uf->stream = newFileStream(fp, (void (*)())pclose);
		uf->guess_type = "text/plain";
		return;
	    }
	}
	check_compression(path, uf);
	if (uf->compression != CMP_NOCOMPRESS) {
	    char *ext = uf->ext;
	    char *t0 = uncompressed_file_type(path, &ext);
	    uf->guess_type = t0;
	    uf->ext = ext;
	    gunzip_stream(uf);
	    return;
	}
    }
}

/* 
 * convert line
 */
Str
convertLine(URLFile *uf, Str line, char *code, int mode)
{
#ifdef JP_CHARSET
    char ic;
    if ((ic = checkShiftCode(line, *code)) != '\0') {
	if (UseAutoDetect)
	    *code = ic;
	line = conv_str(line, *code, InnerCode);
    }
#endif				/* JP_CHARSET */
    cleanup_line(line, mode);
#ifdef USE_NNTP
    if (uf->scheme == SCM_NEWS)
	Strchop(line);
#endif				/* USE_NNTP */
    return line;
}

/* 
 * loadFile: load file to buffer
 */
Buffer *
loadFile(char *path)
{
    URLFile uf;
    init_stream(&uf, SCM_LOCAL, NULL);
    examineFile(path, &uf);
    if (uf.stream == NULL)
	return NULL;
    current_content_length = 0;
#ifdef JP_CHARSET
    content_charset = '\0';
#endif
    return loadSomething(&uf, path, loadBuffer, NULL);
}

int
matchattr(char *p, char *attr, int len, Str *value)
{
    int quoted;
    char *q = NULL;

    if (strncasecmp(p, attr, len) == 0) {
	p += len;
	SKIP_BLANKS(p);
	if (value) {
	    *value = Strnew();
	    if (*p == '=') {
		p++;
		SKIP_BLANKS(p);
		quoted = 0;
		while (!IS_ENDL(*p) && (quoted || *p != ';')) {
		    if (!IS_SPACE(*p))
			q = p;
		    if (*p == '"')
			quoted = (quoted) ? 0 : 1;
		    else
			Strcat_char(*value, *p);
		    p++;
		}
		if (q)
		    Strshrink(*value, p - q - 1);
	    }
	    return 1;
	}
	else {
	    if (IS_ENDT(*p)) {
		return 1;
	    }
	}
    }
    return 0;
}

void
readHeader(URLFile *uf, Buffer *newBuf, int thru, ParsedURL *pu)
{
    char *p, *q, *emsg;
    char c;
    Str lineBuf2 = NULL;
    Str tmp;
    TextList *headerlist;
#ifdef JP_CHARSET
    char code = DocumentCode, ic;
#endif

    headerlist = newBuf->document_header = newTextList();
    if (uf->scheme == SCM_HTTP
#ifdef USE_SSL
	|| uf->scheme == SCM_HTTPS
#endif				/* USE_SSL */
	)
	http_response_code = -1;
    else
	http_response_code = 0;

    while ((tmp = StrmyUFgets(uf))->length) {
#ifdef HTTP_DEBUG
	{
	    FILE *ff;
	    ff = fopen("zzrequest", "a");
	    Strfputs(tmp, ff);
	    fclose(ff);
	}
#endif				/* HTTP_DEBUG */
	cleanup_line(tmp, HEADER_MODE);
	if ((tmp->ptr[0] == '\n' || tmp->ptr[0] == '\r' || tmp->ptr[0] == '\0')
#ifdef USE_NNTP
	    ||
	    (uf->scheme == SCM_NEWS &&
	     Str_news_endline(tmp) && (iseos(uf->stream) = TRUE))
#endif				/* USE_NNTP */
	    ) {
	    if (!lineBuf2)
		/* there is no header */
		break;
	    /* last header */
	}
	else if (!(w3m_dump & DUMP_HEAD)) {
	    if (lineBuf2) {
		Strcat(lineBuf2, tmp);
	    }
	    else {
		lineBuf2 = tmp;
	    }
	    c = UFgetc(uf);
	    UFundogetc(uf);
	    if (c == ' ' || c == '\t')
		/* header line is continued */
		continue;
	    lineBuf2 = decodeMIME(lineBuf2->ptr);
#ifdef JP_CHARSET
	    if ((ic = checkShiftCode(lineBuf2, code)) != '\0') {
		if (UseAutoDetect)
		    code = ic;
		lineBuf2 = conv_str(lineBuf2, code, InnerCode);
	    }
#endif				/* JP_CHARSET */
	    /* separated with line and stored */
	    tmp = Strnew_size(lineBuf2->length);
	    for (p = lineBuf2->ptr; *p; p = q) {
		for (q = p; *q && *q != '\r' && *q != '\n'; q++) ;
		lineBuf2 = checkType(Strnew_charp(p), propBuffer,
#ifdef USE_ANSI_COLOR
				     NULL, NULL,
#endif
				     min(LINELEN, q - p));
		Strcat(tmp, lineBuf2);
		if (thru)
		    addnewline(newBuf, lineBuf2->ptr, propBuffer,
#ifdef USE_ANSI_COLOR
			       NULL,
#endif
			       lineBuf2->length, -1);
		for (; *q && (*q == '\r' || *q == '\n'); q++) ;
	    }
	    lineBuf2 = tmp;
	}
	else {
	    lineBuf2 = tmp;
	}
	if ((uf->scheme == SCM_HTTP
#ifdef USE_SSL
	     || uf->scheme == SCM_HTTPS
#endif				/* USE_SSL */
	    ) && http_response_code == -1) {
	    p = lineBuf2->ptr;
	    while (*p && !IS_SPACE(*p))
		p++;
	    while (*p && IS_SPACE(*p))
		p++;
	    http_response_code = atoi(p);
	    if (fmInitialized) {
		message(lineBuf2->ptr, 0, 0);
		refresh();
	    }
	}
	if (!strncasecmp(lineBuf2->ptr, "content-transfer-encoding:", 26)) {
	    p = lineBuf2->ptr + 26;
	    while (IS_SPACE(*p))
		p++;
	    if (!strncasecmp(p, "base64", 6))
		uf->encoding = ENC_BASE64;
	    else if (!strncasecmp(p, "quoted-printable", 16))
		uf->encoding = ENC_QUOTE;
	    else if (!strncasecmp(p, "uuencode", 8) ||
		     !strncasecmp(p, "x-uuencode", 10))
		uf->encoding = ENC_UUENCODE;
	    else
		uf->encoding = ENC_7BIT;
	}
	else if (!strncasecmp(lineBuf2->ptr, "content-encoding:", 17)) {
	    p = lineBuf2->ptr + 17;
	    while (IS_SPACE(*p))
		p++;
	    if ((!strncasecmp(p, "x-gzip", 6) || !strncasecmp(p, "gzip", 4)) ||
		(!strncasecmp(p, "x-compress", 10)
		 || !strncasecmp(p, "compress", 8))) {
		uf->compression = CMP_GZIP;
	    }
	    else if (!strncasecmp(p, "x-bzip", 6) ||
		     !strncasecmp(p, "bzip", 4) ||
		     !strncasecmp(p, "bzip2", 5)) {
		uf->compression = CMP_BZIP2;
	    }
	    else if (!strncasecmp(p, "x-deflate", 9) ||
		     !strncasecmp(p, "deflate", 7)) {
		uf->compression = CMP_DEFLATE;
	    }
	}
#ifdef USE_COOKIE
	else if (use_cookie && accept_cookie &&
		 pu && check_cookie_accept_domain(pu->host) &&
		 (!strncasecmp(lineBuf2->ptr, "Set-Cookie:", 11) ||
		  !strncasecmp(lineBuf2->ptr, "Set-Cookie2:", 12))) {
	    Str name = Strnew(), value = Strnew(), domain = NULL, path = NULL,
		comment = NULL, commentURL = NULL, port = NULL, tmp;
	    int version, quoted, flag = 0;
	    time_t expires = (time_t) - 1;

	    q = NULL;
	    if (lineBuf2->ptr[10] == '2') {
		p = lineBuf2->ptr + 12;
		version = 1;
	    }
	    else {
		p = lineBuf2->ptr + 11;
		version = 0;
	    }
#ifdef DEBUG
	    fprintf(stderr, "Set-Cookie: [%s]\n", p);
#endif				/* DEBUG */
	    SKIP_BLANKS(p);
	    while (*p != '=' && !IS_ENDT(*p))
		Strcat_char(name, *(p++));
	    Strremovetrailingspaces(name);
	    if (*p == '=') {
		p++;
		SKIP_BLANKS(p);
		quoted = 0;
		while (!IS_ENDL(*p) && (quoted || *p != ';')) {
		    if (!IS_SPACE(*p))
			q = p;
		    if (*p == '"')
			quoted = (quoted) ? 0 : 1;
		    Strcat_char(value, *(p++));
		}
		if (q)
		    Strshrink(value, p - q - 1);
	    }
	    while (*p == ';') {
		p++;
		SKIP_BLANKS(p);
		if (matchattr(p, "expires", 7, &tmp)) {
		    /* version 0 */
		    expires = mymktime(tmp->ptr);
		}
		else if (matchattr(p, "max-age", 7, &tmp)) {
		    /* XXX Is there any problem with max-age=0? (RFC 2109 ss. 4.2.1, 4.2.2 */
		    expires = time(NULL) + atol(tmp->ptr);
		}
		else if (matchattr(p, "domain", 6, &tmp)) {
		    domain = tmp;
		}
		else if (matchattr(p, "path", 4, &tmp)) {
		    path = tmp;
		}
		else if (matchattr(p, "secure", 6, NULL)) {
		    flag |= COO_SECURE;
		}
		else if (matchattr(p, "comment", 7, &tmp)) {
		    comment = tmp;
		}
		else if (matchattr(p, "version", 7, &tmp)) {
		    version = atoi(tmp->ptr);
		}
		else if (matchattr(p, "port", 4, &tmp)) {
		    /* version 1, Set-Cookie2 */
		    port = tmp;
		}
		else if (matchattr(p, "commentURL", 10, &tmp)) {
		    /* version 1, Set-Cookie2 */
		    commentURL = tmp;
		}
		else if (matchattr(p, "discard", 7, NULL)) {
		    /* version 1, Set-Cookie2 */
		    flag |= COO_DISCARD;
		}
		quoted = 0;
		while (!IS_ENDL(*p) && (quoted || *p != ';')) {
		    if (*p == '"')
			quoted = (quoted) ? 0 : 1;
		    p++;
		}
	    }
	    if (pu && name->length > 0) {
		int err;
		if (flag & COO_SECURE)
		    disp_message_nsec("Received a secured cookie", FALSE, 1,
				      TRUE, FALSE);
		else
		    disp_message_nsec(Sprintf("Received cookie: %s=%s",
					      name->ptr, value->ptr)->ptr,
				      FALSE, 1, TRUE, FALSE);
		err =
		    add_cookie(pu, name, value, expires, domain, path, flag,
			       comment, version, port, commentURL);
		if (err) {
		    char *ans = (accept_bad_cookie == TRUE) ? "y" : NULL;
		    if (fmInitialized && (err & COO_OVERRIDE_OK) &&
			accept_bad_cookie == PERHAPS) {
			Str msg =
			    Sprintf
			    ("Accept bad cookie from %s for %s? (y or n) ",
			     pu->host,
			     domain
			     && domain->ptr ? domain->ptr : "<localdomain>");
			if (msg->length > COLS - 4)
			    Strshrink(msg, msg->length - (COLS - 4));
			term_raw();
			ans = inputChar(msg->ptr);
		    }
		    if (ans == NULL || tolower(*ans) != 'y' ||
			(err =
			 add_cookie(pu, name, value, expires, domain, path,
				    flag | COO_OVERRIDE, comment, version,
				    port, commentURL))) {
			err = (err & ~COO_OVERRIDE_OK) - 1;
			if (err >= 0 && err < COO_EMAX)
			    emsg = Sprintf("This cookie was rejected "
					   "to prevent security violation. [%s]",
					   violations[err])->ptr;
			else
			    emsg =
				"This cookie was rejected to prevent security violation.";
			record_err_message(emsg);
			disp_message_nsec(emsg, FALSE, 10, TRUE, FALSE);
		    }
		    else
			disp_message_nsec(Sprintf
					  ("Accepting invalid cookie: %s=%s",
					   name->ptr, value->ptr)->ptr, FALSE,
					  1, TRUE, FALSE);
		}
	    }
	}
#endif				/* USE_COOKIE */
	else if (!strncasecmp(lineBuf2->ptr, "w3m-control:", 12) &&
		 uf->scheme == SCM_LOCAL_CGI) {
	    Str funcname = Strnew();
	    int f;
	    extern int w3mNFuncList;

	    p = lineBuf2->ptr + 12;
	    SKIP_BLANKS(p);
	    while (*p && !IS_SPACE(*p))
		Strcat_char(funcname, *(p++));
	    SKIP_BLANKS(p);
	    f = getFuncList(funcname->ptr, w3mFuncList, w3mNFuncList);
	    if (f >= 0) {
		tmp = Strnew_charp(p);
		Strchop(tmp);
		pushEvent(f, tmp->ptr);
	    }
	}
	if (headerlist)
	    pushText(headerlist, lineBuf2->ptr);
	Strfree(lineBuf2);
	lineBuf2 = NULL;
    }
    if (thru)
	addnewline(newBuf, "", propBuffer,
#ifdef USE_ANSI_COLOR
		   NULL,
#endif
		   0, -1);
}

char *
checkHeader(Buffer *buf, char *field)
{
    int len = strlen(field);
    TextListItem *i;
    char *p;

    if (buf == NULL || field == NULL)
	return NULL;
    for (i = buf->document_header->first; i != NULL; i = i->next) {
	if (!strncasecmp(i->ptr, field, len)) {
	    p = i->ptr + len;
	    SKIP_BLANKS(p);
	    return p;
	}
    }
    return NULL;
}

char *
checkContentType(Buffer *buf)
{
    char *p;
    Str r;
    p = checkHeader(buf, "Content-Type:");
    if (p == NULL)
	return NULL;
    r = Strnew();
    while (*p && *p != ';' && !IS_SPACE(*p))
	Strcat_char(r, *p++);
#ifdef JP_CHARSET
    if ((p = strcasestr(p, "charset")) != NULL) {
	p += 7;
	SKIP_BLANKS(p);
	if (*p == '=') {
	    p++;
	    SKIP_BLANKS(p);
	    content_charset = guess_charset(p);
	}
    }
#endif
    return r->ptr;
}

static Str
extractRealm(char *q)
{
    Str p = Strnew();
    char c;

    SKIP_BLANKS(q);
    if (strncasecmp(q, "Basic ", 6) != 0) {
	/* complicated authentication... not implemented */
	return NULL;
    }
    q += 6;
    SKIP_BLANKS(q);
    if (strncasecmp(q, "realm=", 6) != 0) {
	/* no realm attribute... get confused */
	return NULL;
    }
    q += 6;
    SKIP_BLANKS(q);
    c = '\0';
    if (*q == '"')
	c = *q++;
    while (*q != '\0' && *q != c)
	Strcat_char(p, *q++);
    return p;
}

static Str
getAuthCookie(char *realm, char *auth_header, TextList *extra_header,
	      ParsedURL *pu)
{
    Str ss;
    Str uname, pwd;
    Str tmp;
    TextListItem *i, **i0;
    int a_found;
    int auth_header_len = strlen(auth_header);

    a_found = FALSE;
    for (i0 = &(extra_header->first), i = *i0; i != NULL;
	 i0 = &(i->next), i = *i0) {
	if (!strncasecmp(i->ptr, auth_header, auth_header_len)) {
	    a_found = TRUE;
	    break;
	}
    }
    if (a_found) {
	/* This means that *-Authenticate: header is received after *
	 * Authorization: header is sent to the server. */
	if (fmInitialized) {
	    message("Wrong username or password", 0, 0);
	    refresh();
	}
	else
	    fprintf(stderr, "Wrong username or password\n");
	sleep(1);
	ss = NULL;
	*i0 = i->next;		/* delete Authenticate: header from *
				 * extra_header */
    }
    else
	ss = find_auth_cookie(pu->host, realm);
    if (ss == NULL) {
	/* input username and password */
	sleep(2);
	if (fmInitialized) {
	    char *pp;
	    term_raw();
	    if ((pp =
		 inputStr(Sprintf("Username for %s: ", realm)->ptr,
			  NULL)) == NULL)
		return NULL;
	    uname = Str_conv_to_system(Strnew_charp(pp));
	    if ((pp =
		 inputLine(Sprintf("Password for %s: ", realm)->ptr, NULL,
			   IN_PASSWORD)) == NULL)
		return NULL;
	    pwd = Str_conv_to_system(Strnew_charp(pp));
	    term_cbreak();
	}
	else {
	    int proxy = !strncasecmp("Proxy-Authorization:", auth_header,
				     auth_header_len);

	    /*
	     * If post file is specified as '-', stdin is closed at this point.
	     * In this case, w3m cannot read username from stdin.
	     * So exit with error message.
	     * (This is same behavior as lwp-request.)
	     */
	    if (feof(stdin) || ferror(stdin)) {
		fprintf(stderr, "w3m: Authorization required for %s\n", realm);
		exit(1);
	    }

	    printf(proxy ? "Proxy Username for %s: " : "Username for %s: ",
		   realm);
	    fflush(stdout);
	    uname = Strfgets(stdin);
	    Strchop(uname);
#ifdef HAVE_GETPASSPHRASE
	    pwd = Strnew_charp((char *)
			       getpassphrase(proxy ? "Proxy Password: " :
					     "Password: "));
#else
	    pwd = Strnew_charp((char *)
			       getpass(proxy ? "Proxy Password: " :
				       "Password: "));
#endif
	}
	Strcat_char(uname, ':');
	Strcat(uname, pwd);
	ss = encodeB(uname->ptr);
    }
    tmp = Strnew_charp(auth_header);
    Strcat_charp(tmp, " Basic ");
    Strcat(tmp, ss);
    Strcat_charp(tmp, "\r\n");
    pushText(extra_header, tmp->ptr);
    return ss;
}

static int
same_url_p(ParsedURL *pu1, ParsedURL *pu2)
{
    return (pu1->scheme == pu2->scheme && pu1->port == pu2->port &&
	    (pu1->host ? pu2->host ? !strcasecmp(pu1->host, pu2->host) : 0 : 1)
	    && (pu1->file ? pu2->
		file ? !strcmp(pu1->file, pu2->file) : 0 : 1));
}

/* 
 * loadGeneralFile: load file to buffer
 */
Buffer *
loadGeneralFile(char *path, ParsedURL *current, char *referer, int flag,
		FormList *request)
{
    URLFile f, *of = NULL;
    ParsedURL pu, *volatile puv = NULL;
    int volatile nredir = 0;
    int volatile nredir_size = 0;
    Buffer *b = NULL, *(*proc) ();
    char *tpath;
    char *t, *p, *real_type = NULL;
    Buffer *t_buf = NULL;
    int searchHeader = SearchHeader;
    int searchHeader_through = TRUE;
    MySignalHandler(*prevtrap) ();
    TextList *extra_header = newTextList();
    Str ss;
    Str realm;
    int add_auth_cookie_flag;
    unsigned char status = HTST_NORMAL;
    URLOption url_option;
    Str tmp;

    tpath = path;
    prevtrap = NULL;
    add_auth_cookie_flag = 0;

    if (proxy_auth_cookie != NULL) {
	pushText(extra_header, Sprintf("Proxy-Authorization: Basic %s\r\n",
				       proxy_auth_cookie->ptr)->ptr);
    }
  load_doc:
    url_option.referer = referer;
    url_option.flag = flag;
    f = openURL(tpath, &pu, current, &url_option, request, extra_header, of,
		&status);
    of = NULL;
#ifdef JP_CHARSET
    content_charset = '\0';
#endif
    if (f.stream == NULL) {
	/* openURL failure: it means either (1) the requested URL is a directory name
	 * on an FTP server, or (2) is a local directory name. 
	 */
	extern Str FTPDIRtmp;
	if (fmInitialized && prevtrap) {
	    term_raw();
	    signal(SIGINT, prevtrap);
	}
	switch (f.scheme) {
	case SCM_FTPDIR:
	    if (FTPDIRtmp->length > 0) {
		b = loadHTMLString(FTPDIRtmp);
		if (b) {
		    if (b->currentURL.host == NULL
			&& b->currentURL.file == NULL)
			copyParsedURL(&b->currentURL, &pu);
		    b->real_scheme = pu.scheme;
		}
		return b;
	    }
	    break;
	case SCM_LOCAL:
	    {
		struct stat st;
		if (stat(pu.real_file, &st) < 0)
		    return NULL;
		if (S_ISDIR(st.st_mode)) {
		    if (UseExternalDirBuffer) {
			Str tmp = Strnew_charp(DirBufferCommand);
			Strcat_m_charp(tmp, "?dir=",
				       pu.real_file, "#current", NULL);
			b = loadGeneralFile(tmp->ptr, NULL, NO_REFERER, 0,
					    NULL);
			if (b != NULL && b != NO_BUFFER) {
			    copyParsedURL(&b->currentURL, &pu);
			    b->filename = b->currentURL.real_file;
			}
			return b;
		    }
		    else {
			b = dirBuffer(pu.real_file);
			if (b == NULL)
			    return NULL;
			t = "text/html";
			b->real_scheme = pu.scheme;
			goto loaded;
		    }
		}
	    }
	}
	return NULL;
    }

    /* openURL() succeeded */
    if (SETJMP(AbortLoading) != 0) {
	/* transfer interrupted */
	if (fmInitialized) {
	    term_raw();
	    signal(SIGINT, prevtrap);
	}
	if (b)
	    discardBuffer(b);
	UFclose(&f);
	return NULL;
    }

    b = NULL;
    if (f.is_cgi) {
	/* local CGI */
	searchHeader = TRUE;
	searchHeader_through = FALSE;
    }
    if (header_string)
	header_string = NULL;
    if (fmInitialized) {
	prevtrap = signal(SIGINT, KeyAbort);
	term_cbreak();
    }
    if (pu.scheme == SCM_HTTP ||
#ifdef USE_SSL
	pu.scheme == SCM_HTTPS ||
#endif				/* USE_SSL */
	((
#ifdef USE_GOPHER
	     (pu.scheme == SCM_GOPHER && non_null(GOPHER_proxy)) ||
#endif				/* USE_GOPHER */
	     (pu.scheme == SCM_FTP && non_null(FTP_proxy))
	 ) && !Do_not_use_proxy && !check_no_proxy(pu.host))) {

	if (fmInitialized) {
	    message(Sprintf("%s contacted. Waiting for reply...", pu.host)->
		    ptr, 0, 0);
	    refresh();
	}
	if (t_buf == NULL)
	    t_buf = newBuffer(INIT_BUFFER_WIDTH);
#ifdef USE_SSL
	if (IStype(f.stream) == IST_SSL) {
	    Str s = ssl_get_certificate(f.stream);
	    if (s != NULL)
		t_buf->ssl_certificate = s->ptr;
	}
#endif
	readHeader(&f, t_buf, FALSE, &pu);
	t = checkContentType(t_buf);
	if (t == NULL)
	    t = "text/plain";
	if (http_response_code >= 301 && http_response_code <= 303
	    && (p = checkHeader(t_buf, "Location:")) != NULL) {
	    /* document moved */
	    if (nredir >= FollowRedirection) {
		tmp =
		    Sprintf("Number of redirections exceeded %d at %s",
			    FollowRedirection, parsedURL2Str(&pu)->ptr);
		disp_err_message(tmp->ptr, FALSE);
	    }
	    else if (nredir_size > 0 &&
		     (same_url_p(&pu, &puv[(nredir - 1) % nredir_size]) ||
		      (!(nredir % 2)
		       && same_url_p(&pu, &puv[(nredir / 2) % nredir_size])))) {
		tmp =
		    Sprintf("Redirection loop detected (%s)",
			    parsedURL2Str(&pu)->ptr);
		disp_err_message(tmp->ptr, FALSE);
	    }
	    else {
		if (!puv) {
		    nredir_size = FollowRedirection / 2 + 1;
		    puv = New_N(ParsedURL, nredir_size);
		    memset(puv, 0, sizeof(ParsedURL) * nredir_size);
		}

		copyParsedURL(&puv[nredir % nredir_size], &pu);
		++nredir;
		tmp = Strnew_charp(p);
		Strchop(tmp);
		tpath = tmp->ptr;
		request = NULL;
		UFclose(&f);
		add_auth_cookie_flag = 0;
		current = New(ParsedURL);
		copyParsedURL(current, &pu);
		t_buf->bufferprop |= BP_REDIRECTED;
		status = HTST_NORMAL;
		goto load_doc;
	    }
	}
	if ((p = checkHeader(t_buf, "WWW-Authenticate:")) != NULL &&
	    http_response_code == 401) {
	    /* Authentication needed */
	    realm = extractRealm(p);
	    if (realm != NULL) {
		ss = getAuthCookie(realm->ptr, "Authorization:", extra_header,
				   &pu);
		if (ss == NULL) {
		    /* abort */
		    UFclose(&f);
		    signal(SIGINT, prevtrap);
		    return NULL;
		}
		UFclose(&f);
		add_auth_cookie_flag = 1;
		status = HTST_NORMAL;
		goto load_doc;
	    }
	}
	if ((p = checkHeader(t_buf, "Proxy-Authenticate:")) != NULL &&
	    http_response_code == 407) {
	    /* Authentication needed */
	    realm = extractRealm(p);
	    if (realm != NULL) {
		ss = getAuthCookie(realm->ptr,
				   "Proxy-Authorization:",
				   extra_header, &HTTP_proxy_parsed);
		proxy_auth_cookie = ss;
		if (ss == NULL) {
		    /* abort */
		    UFclose(&f);
		    signal(SIGINT, prevtrap);
		    return NULL;
		}
		UFclose(&f);
		status = HTST_NORMAL;
		goto load_doc;
	    }
	}
	if (add_auth_cookie_flag)
	    /* If authorization is required and passed */
	    add_auth_cookie(pu.host, realm->ptr, ss);
	if (status == HTST_CONNECT) {
	    of = &f;
	    goto load_doc;
	}
    }
#ifdef USE_NNTP
    else if (pu.scheme == SCM_NEWS) {
	t_buf = newBuffer(INIT_BUFFER_WIDTH);
	readHeader(&f, t_buf, TRUE, &pu);
	t = checkContentType(t_buf);
	if (t == NULL)
	    t = "text/plain";
    }
#endif				/* USE_NNTP */
#ifdef USE_GOPHER
    else if (pu.scheme == SCM_GOPHER) {
	switch (*pu.file) {
	case '0':
	    t = "text/plain";
	    break;
	case '1':
	    t = "gopher:directory";
	    break;
	case 'm':
	    t = "gopher:directory";
	    break;
	case 's':
	    t = "audio/basic";
	    break;
	case 'g':
	    t = "image/gif";
	    break;
	case 'h':
	    t = "text/html";
	    break;
	}
    }
#endif				/* USE_GOPHER */
    else if (pu.scheme == SCM_FTP) {
	check_compression(path, &f);
	if (f.compression != CMP_NOCOMPRESS) {
	    char *t1 = uncompressed_file_type(pu.file, NULL);
	    real_type = f.guess_type;
#if 0
	    if (t1 && strncasecmp(t1, "application/", 12) == 0) {
		f.compression = CMP_NOCOMPRESS;
		t = real_type;
	    }
	    else
#endif
	    if (t1)
		t = t1;
	    else
		t = real_type;
	}
	else {
	    real_type = guessContentType(pu.file);
	    if (real_type == NULL)
		real_type = "text/plain";
	    t = real_type;
	}
#if 0
	if (!strncasecmp(t, "application/", 12)) {
	    char *tmpf = tmpfname(TMPF_DFL, NULL)->ptr;
	    current_content_length = 0;
	    if (save2tmp(f, tmpf) < 0)
		UFclose(&f);
	    else {
		if (fmInitialized) {
		    term_raw();
		    signal(SIGINT, prevtrap);
		}
		doFileMove(tmpf, guess_save_name(t_buf, pu.file));
	    }
	    return NO_BUFFER;
	}
#endif
    }
    else if (searchHeader) {
	t_buf = newBuffer(INIT_BUFFER_WIDTH);
	readHeader(&f, t_buf, searchHeader_through, &pu);
	t = checkContentType(t_buf);
	if (t == NULL)
	    t = "text/plain";
	if (f.is_cgi && (p = checkHeader(t_buf, "Location:")) != NULL) {
	    /* document moved */
	    tmp = Strnew_charp(p);
	    Strchop(tmp);
	    tpath = tmp->ptr;
	    request = NULL;
	    UFclose(&f);
	    add_auth_cookie_flag = 0;
	    current = New(ParsedURL);
	    copyParsedURL(current, &pu);
	    t_buf->bufferprop |= BP_REDIRECTED;
	    status = HTST_NORMAL;
	    goto load_doc;
	}
	searchHeader = SearchHeader = FALSE;
    }
    else if (DefaultType) {
	t = DefaultType;
	DefaultType = NULL;
    }
    else {
	t = guessContentType(pu.file);
	if (t == NULL)
	    t = "text/plain";
	real_type = t;
	if (f.guess_type)
	    t = f.guess_type;
    }
    if (real_type == NULL)
	real_type = t;
    proc = loadBuffer;

    if (do_download) {
	/* download only */
	char *file;
	if (fmInitialized) {
	    term_raw();
	    signal(SIGINT, prevtrap);
	}
	if (DecodeCTE && IStype(f.stream) != IST_ENCODED)
	    f.stream = newEncodedStream(f.stream, f.encoding);
	if (pu.scheme == SCM_LOCAL)
	    file = conv_from_system(guess_save_name(NULL, pu.real_file));
	else
	    file = guess_save_name(t_buf, pu.file);
	doFileSave(f, file);
	UFclose(&f);
	return NO_BUFFER;
    }

    if (f.compression != CMP_NOCOMPRESS) {
	if (!(w3m_dump & DUMP_SOURCE) &&
	    (w3m_dump & ~DUMP_FRAME || is_text_type(t)
	     || searchExtViewer(t))) {
	    gunzip_stream(&f);
	    uncompressed_file_type(pu.file, &f.ext);
	}
	else {
	    t = compress_application_type(f.compression);
	    f.compression = CMP_NOCOMPRESS;
	}
    }

    if (!strcasecmp(t, "text/html"))
	proc = loadHTMLBuffer;
    else if (is_plain_text_type(t))
	proc = loadBuffer;
#ifdef USE_GOPHER
    else if (!strcasecmp(t, "gopher:directory")) {
	proc = loadGopherDir;
    }
#endif				/* USE_GOPHER */
    else if (w3m_backend) ;
    else if (!(w3m_dump & ~DUMP_FRAME) || is_dump_text_type(t)) {
	if (!do_download && doExternal(f,
				       pu.real_file ? pu.real_file : pu.file,
				       t, &b, t_buf)) {
	    if (b && b != NO_BUFFER) {
		b->real_scheme = f.scheme;
		b->real_type = real_type;
		if (b->currentURL.host == NULL && b->currentURL.file == NULL)
		    copyParsedURL(&b->currentURL, &pu);
	    }
	    UFclose(&f);
	    if (fmInitialized) {
		term_raw();
		signal(SIGINT, prevtrap);
	    }
	    return b;
	}
	else {
	    if (fmInitialized) {
		term_raw();
		signal(SIGINT, prevtrap);
	    }
	    if (pu.scheme == SCM_LOCAL) {
		UFclose(&f);
		doFileCopy(pu.real_file,
			   conv_from_system(guess_save_name
					    (NULL, pu.real_file)));
	    }
	    else {
		if (DecodeCTE && IStype(f.stream) != IST_ENCODED)
		    f.stream = newEncodedStream(f.stream, f.encoding);
		doFileSave(f, guess_save_name(t_buf, pu.file));
		UFclose(&f);
	    }
	    return NO_BUFFER;
	}
    }

    current_content_length = 0;
    if ((p = checkHeader(t_buf, "Content-Length:")) != NULL)
	current_content_length = atoi(p);

    if (flag & RG_FRAME) {
	t_buf = newBuffer(INIT_BUFFER_WIDTH);
	t_buf->bufferprop |= BP_FRAME;
    }
#ifdef USE_SSL
    if (IStype(f.stream) == IST_SSL) {
	Str s = ssl_get_certificate(f.stream);
	if (s != NULL)
	    t_buf->ssl_certificate = s->ptr;
    }
#endif
    b = loadSomething(&f, pu.real_file ? pu.real_file : pu.file, proc, t_buf);
    UFclose(&f);
    if (b) {
	b->real_scheme = f.scheme;
	b->real_type = real_type;
      loaded:
	if (b->currentURL.host == NULL && b->currentURL.file == NULL)
	    copyParsedURL(&b->currentURL, &pu);
	if (!strcasecmp(t, "text/html"))
	    b->type = "text/html";
	else if (w3m_backend) {
	    Str s = Strnew_charp(t);
	    b->type = s->ptr;
	}
	else
	    b->type = "text/plain";
	if (pu.label) {
	    if (proc == loadHTMLBuffer) {
		Anchor *a;
#ifdef JP_CHARSET
		a = searchURLLabel(b,
				   conv(pu.label, b->document_code,
					InnerCode)->ptr);
#else				/* not JP_CHARSET */
		a = searchURLLabel(b, pu.label);
#endif				/* not JP_CHARSET */
		if (a != NULL) {
		    gotoLine(b, a->start.line);
#ifdef LABEL_TOPLINE
		    if (label_topline)
			b->topLine = lineSkip(b, b->topLine,
					      b->currentLine->linenumber
					      - b->topLine->linenumber, FALSE);
#endif
		    b->pos = a->start.pos;
		    arrangeCursor(b);
		}
	    }
	    else {		/* plain text */
		int l = atoi(pu.label);
		gotoLine(b, l);
		b->pos = 0;
		arrangeCursor(b);
	    }
	}
    }
    if (header_string)
	header_string = NULL;
    if (fmInitialized) {
	term_raw();
	signal(SIGINT, prevtrap);
    }
    return b;
}

#define TAG_IS(s,tag,len)\
  (strncasecmp(s,tag,len)==0&&(s[len] == '>' || IS_SPACE((int)s[len])))

static char *
has_hidden_link(struct readbuffer *obuf, int cmd)
{
    Str line = obuf->line;
    struct link_stack *p;

    if (Strlastchar(line) != '>')
	return NULL;

    for (p = link_stack; p; p = p->next)
	if (p->cmd == cmd)
	    break;
    if (!p)
	return NULL;

    if (obuf->pos == p->pos)
	return line->ptr + p->offset;

    return NULL;
}

static void
push_link(int cmd, int offset, int pos)
{
    struct link_stack *p;
    p = New(struct link_stack);
    p->cmd = cmd;
    p->offset = offset;
    p->pos = pos;
    p->next = link_stack;
    link_stack = p;
}

static int
is_period_char(int ch)
{
    switch (ch) {
    case ',':
    case '.':
    case ':':
    case ';':
    case '?':
    case '!':
    case ')':
    case ']':
    case '}':
    case '>':
	return 1;
    default:
	return 0;
    }
}

static int
is_beginning_char(int ch)
{
    switch (ch) {
    case '(':
    case '[':
    case '{':
    case '`':
    case '<':
	return 1;
    default:
	return 0;
    }
}

static int
is_word_char(int ch)
{
#ifdef JP_CHARSET
    if (is_wckanji(ch) || IS_CNTRL(ch))
	return 0;
#else
    if (IS_CNTRL(ch))
	return 0;
#endif

    if (IS_ALNUM(ch))
	return 1;

    switch (ch) {
    case ',':
    case '.':
    case '\"':			/* " */
    case '\'':
    case '$':
    case '%':
    case '+':
    case '-':
    case '@':
    case '~':
    case '_':
	return 1;
    }
    if (ch == TIMES_CODE || ch == DIVIDE_CODE || ch == ANSP_CODE)
	return 0;
    if (ch >= AGRAVE_CODE || ch == NBSP_CODE)
	return 1;
    return 0;
}

int
is_boundary(int ch1, int ch2)
{
    if (!ch1 || !ch2)
	return 1;

    if (ch1 == ' ' && ch2 == ' ')
	return 0;

    if (ch1 != ' ' && is_period_char(ch2))
	return 0;

    if (ch2 != ' ' && is_beginning_char(ch1))
	return 0;

    if (is_word_char(ch1) && is_word_char(ch2))
	return 0;

    return 1;
}


static void
set_breakpoint(struct readbuffer *obuf, int tag_length)
{
    obuf->bp.len = obuf->line->length;
    obuf->bp.pos = obuf->pos;
    obuf->bp.tlen = tag_length;
    obuf->bp.flag = obuf->flag;
#ifdef FORMAT_NICE
    obuf->bp.flag &= ~RB_FILL;
#endif				/* FORMAT_NICE */
    obuf->bp.top_margin = obuf->top_margin;
    obuf->bp.bottom_margin = obuf->bottom_margin;

    if (!obuf->bp.init_flag)
	return;

    obuf->bp.anchor = obuf->anchor;
    obuf->bp.anchor_target = obuf->anchor_target;
    obuf->bp.anchor_hseq = obuf->anchor_hseq;
    obuf->bp.img_alt = obuf->img_alt;
    obuf->bp.in_bold = obuf->in_bold;
    obuf->bp.in_under = obuf->in_under;
    obuf->bp.nobr_level = obuf->nobr_level;
    obuf->bp.prev_ctype = obuf->prev_ctype;
    obuf->bp.init_flag = 0;
}

static void
back_to_breakpoint(struct readbuffer *obuf)
{
    obuf->flag = obuf->bp.flag;
    obuf->anchor = obuf->bp.anchor;
    obuf->anchor_target = obuf->bp.anchor_target;
    obuf->anchor_hseq = obuf->bp.anchor_hseq;
    obuf->img_alt = obuf->bp.img_alt;
    obuf->in_bold = obuf->bp.in_bold;
    obuf->in_under = obuf->bp.in_under;
    obuf->prev_ctype = obuf->bp.prev_ctype;
    obuf->pos = obuf->bp.pos;
    obuf->top_margin = obuf->bp.top_margin;
    obuf->bottom_margin = obuf->bp.bottom_margin;
    if (obuf->flag & RB_NOBR)
	obuf->nobr_level = obuf->bp.nobr_level;
}

static void
append_tags(struct readbuffer *obuf)
{
    int i;
    int len = obuf->line->length;
    int set_bp = 0;

    for (i = 0; i < obuf->tag_sp; i++) {
	switch (obuf->tag_stack[i]->cmd) {
	case HTML_A:
	case HTML_IMG_ALT:
	case HTML_B:
	case HTML_U:
	    push_link(obuf->tag_stack[i]->cmd, obuf->line->length, obuf->pos);
	    break;
	}
	Strcat_charp(obuf->line, obuf->tag_stack[i]->cmdname);
	switch (obuf->tag_stack[i]->cmd) {
	case HTML_NOBR:
	    if (obuf->nobr_level > 1)
		break;
	case HTML_WBR:
	    set_bp = 1;
	    break;
	}
    }
    obuf->tag_sp = 0;
    if (set_bp)
	set_breakpoint(obuf, obuf->line->length - len);
}

static void
push_tag(struct readbuffer *obuf, char *cmdname, int cmd)
{
    obuf->tag_stack[obuf->tag_sp] = New(struct cmdtable);
    obuf->tag_stack[obuf->tag_sp]->cmdname = allocStr(cmdname, 0);
    obuf->tag_stack[obuf->tag_sp]->cmd = cmd;
    obuf->tag_sp++;
    if (obuf->tag_sp >= TAG_STACK_SIZE || obuf->flag & (RB_SPECIAL & ~RB_NOBR))
	append_tags(obuf);
}

static void
push_nchars(struct readbuffer *obuf, int width,
	    char *str, int len, Lineprop mode)
{
    int delta = get_mclen(mode);
    append_tags(obuf);
    Strcat_charp_n(obuf->line, str, len);
    obuf->pos += width;
    if (width > 0 && len >= delta) {
	obuf->prevchar = mctowc(&str[len - delta], mode);
	obuf->prev_ctype = mode;
    }
    obuf->flag |= RB_NFLUSHED;
}

#define push_charp(obuf, width, str, mode)\
push_nchars(obuf, width, str, strlen(str), mode)

#define push_str(obuf, width, str, mode)\
push_nchars(obuf, width, str->ptr, str->length, mode)

static void
check_breakpoint(struct readbuffer *obuf, int pre_mode, int ch)
{
    int tlen, len = obuf->line->length;

    append_tags(obuf);
    if (pre_mode)
	return;
    tlen = obuf->line->length - len;
    if (tlen > 0 || is_boundary(obuf->prevchar, ch))
	set_breakpoint(obuf, tlen);
}

static void
push_char(struct readbuffer *obuf, int pre_mode, char ch)
{
    check_breakpoint(obuf, pre_mode, (unsigned char)ch);
    Strcat_char(obuf->line, ch);
    obuf->pos++;
    obuf->prevchar = (unsigned char)ch;
    if (ch != ' ')
	obuf->prev_ctype = PC_ASCII;
    obuf->flag |= RB_NFLUSHED;
}

#define PUSH(c) push_char(obuf, obuf->flag & RB_SPECIAL, c)

static void
push_spaces(struct readbuffer *obuf, int pre_mode, int width)
{
    int i;

    if (width <= 0)
	return;
    check_breakpoint(obuf, pre_mode, ' ');
    for (i = 0; i < width; i++)
	Strcat_char(obuf->line, ' ');
    obuf->pos += width;
    obuf->prevchar = ' ';
    obuf->flag |= RB_NFLUSHED;
}

static void
proc_mchar(struct readbuffer *obuf, int pre_mode,
	   int width, char **str, Lineprop mode)
{
    int ch = mctowc(*str, mode);

    check_breakpoint(obuf, pre_mode, ch);
    obuf->pos += width;
#ifdef JP_CHARSET
    if (IS_KANJI1(**str) && mode == PC_ASCII)
	Strcat_char(obuf->line, ' ');
    else if (mode == PC_KANJI)
	Strcat_charp_n(obuf->line, *str, 2);
    else
#endif
	Strcat_char(obuf->line, **str);
    if (width > 0) {
	obuf->prevchar = ch;
	if (ch != ' ')
	    obuf->prev_ctype = mode;
    }
    (*str) += get_mclen(mode);
    obuf->flag |= RB_NFLUSHED;
}

void
push_render_image(Str str, int width, int limit,
		  struct html_feed_environ *h_env)
{
    struct readbuffer *obuf = h_env->obuf;
    int indent = h_env->envs[h_env->envc].indent;

    push_spaces(obuf, 1, (limit - width) / 2);
    push_str(obuf, width, str, PC_ASCII);
    push_spaces(obuf, 1, (limit - width + 1) / 2);
    if (width > 0)
	flushline(h_env, obuf, indent, 0, h_env->limit);
}

static int
sloppy_parse_line(char **str)
{
    if (**str == '<') {
	while (**str && **str != '>')
	    (*str)++;
	if (**str == '>')
	    (*str)++;
	return 1;
    }
    else {
	while (**str && **str != '<')
	    (*str)++;
	return 0;
    }
}

static void
passthrough(struct readbuffer *obuf, char *str, int back)
{
    int status, cmd;
    Str tok = Strnew();
    char *str_bak;

    if (back) {
	Str str_save = Strnew_charp(str);
	Strshrink(obuf->line, obuf->line->ptr + obuf->line->length - str);
	str = str_save->ptr;
    }
    while (*str) {
	str_bak = str;
	if (sloppy_parse_line(&str)) {
	    char *q = str_bak;
	    cmd = gethtmlcmd(&q, &status);
	    if (back) {
		struct link_stack *p;
		for (p = link_stack; p; p = p->next) {
		    if (p->cmd == cmd) {
			link_stack = p->next;
			break;
		    }
		}
		back = 0;
	    }
	    else {
		Strcat_charp_n(tok, str_bak, str - str_bak);
		push_tag(obuf, tok->ptr, cmd);
		Strclear(tok);
	    }
	}
	else {
	    push_nchars(obuf, 0, str_bak, str - str_bak, obuf->prev_ctype);
	}
    }
}

#if 0
int
is_blank_line(char *line, int indent)
{
    int i, is_blank = 0;

    for (i = 0; i < indent; i++) {
	if (line[i] == '\0') {
	    is_blank = 1;
	}
	else if (line[i] != ' ') {
	    break;
	}
    }
    if (i == indent && line[i] == '\0')
	is_blank = 1;
    return is_blank;
}
#endif

void
fillline(struct readbuffer *obuf, int indent)
{
    push_spaces(obuf, 1, indent - obuf->pos);
    obuf->flag &= ~RB_NFLUSHED;
}

void
flushline(struct html_feed_environ *h_env, struct readbuffer *obuf, int indent,
	  int force, int width)
{
    TextLineList *buf = h_env->buf;
    FILE *f = h_env->f;
    Str line = obuf->line, pass = NULL;
    char *hidden_anchor = NULL, *hidden_img = NULL, *hidden_bold = NULL,
	*hidden_under = NULL, *hidden = NULL;

    if (w3m_debug) {
	FILE *f = fopen("zzzproc1", "a");
	fprintf(f, "flushline(%s,%d,%d,%d)\n", obuf->line->ptr, indent, force,
		width);
	if (buf) {
	    TextLineListItem *p;
	    for (p = buf->first; p; p = p->next) {
		fprintf(f, "buf=\"%s\"\n", p->ptr->line->ptr);
	    }
	}
	fclose(f);
    }

    if (!(obuf->flag & (RB_SPECIAL & ~RB_NOBR)) && Strlastchar(line) == ' ') {
	Strshrink(line, 1);
	obuf->pos--;
    }

    append_tags(obuf);

    if (obuf->anchor)
	hidden = hidden_anchor = has_hidden_link(obuf, HTML_A);
    if (obuf->img_alt) {
	if ((hidden_img = has_hidden_link(obuf, HTML_IMG_ALT)) != NULL) {
	    if (!hidden || hidden_img < hidden)
		hidden = hidden_img;
	}
    }
    if (obuf->in_bold) {
	if ((hidden_bold = has_hidden_link(obuf, HTML_B)) != NULL) {
	    if (!hidden || hidden_bold < hidden)
		hidden = hidden_bold;
	}
    }
    if (obuf->in_under) {
	if ((hidden_under = has_hidden_link(obuf, HTML_U)) != NULL) {
	    if (!hidden || hidden_under < hidden)
		hidden = hidden_under;
	}
    }
    if (hidden) {
	pass = Strnew_charp(hidden);
	Strshrink(line, line->ptr + line->length - hidden);
    }

    if (!(obuf->flag & (RB_SPECIAL & ~RB_NOBR)) && obuf->pos > width) {
	char *tp = &line->ptr[obuf->bp.len - obuf->bp.tlen];
	char *ep = &line->ptr[line->length];

	if (obuf->bp.pos == obuf->pos && tp <= ep &&
	    tp > line->ptr && tp[-1] == ' ') {
	    bcopy(tp, tp - 1, ep - tp + 1);
	    line->length--;
	    obuf->pos--;
	}
    }

    if (obuf->anchor && !hidden_anchor)
	Strcat_charp(line, "</a>");
    if (obuf->img_alt && !hidden_img)
	Strcat_charp(line, "</img_alt>");
    if (obuf->in_bold && !hidden_bold)
	Strcat_charp(line, "</b>");
    if (obuf->in_under && !hidden_under)
	Strcat_charp(line, "</u>");

    if (obuf->top_margin > 0) {
	int i;
	struct html_feed_environ h;
	struct readbuffer o;
	struct environment e[1];

	init_henv(&h, &o, e, 1, NULL, width, indent);
	o.line = Strnew_size(width + 20);
	o.pos = obuf->pos;
	o.flag = obuf->flag;
	o.top_margin = -1;
	o.bottom_margin = -1;
	Strcat_charp(o.line, "<pre_int>");
	for (i = 0; i < o.pos; i++)
	    Strcat_char(o.line, ' ');
	Strcat_charp(o.line, "</pre_int>");
	for (i = 0; i < obuf->top_margin; i++)
	    flushline(h_env, &o, indent, force, width);
    }

    if (force == 1 || obuf->flag & RB_NFLUSHED) {
	TextLine *lbuf = newTextLine(line, obuf->pos);
	if (RB_GET_ALIGN(obuf) == RB_CENTER) {
	    align(lbuf, width, ALIGN_CENTER);
	}
	else if (RB_GET_ALIGN(obuf) == RB_RIGHT) {
	    align(lbuf, width, ALIGN_RIGHT);
	}
#ifdef FORMAT_NICE
	else if (obuf->flag & RB_FILL) {
	    char *p;
	    int rest, rrest;
	    int nspace, d, i;

	    rest = width - line->length;
	    if (rest > 1) {
		nspace = 0;
		for (p = line->ptr + indent; *p; p++) {
		    if (*p == ' ')
			nspace++;
		}
		if (nspace > 0) {
		    int indent_here = 0;
		    d = rest / nspace;
		    p = line->ptr;
		    while (IS_SPACE(*p)) {
			p++;
			indent_here++;
		    }
		    rrest = rest - d * nspace;
		    line = Strnew_size(width + 1);
		    for (i = 0; i < indent_here; i++)
			Strcat_char(line, ' ');
		    for (; *p; p++) {
			Strcat_char(line, *p);
			if (*p == ' ') {
			    for (i = 0; i < d; i++)
				Strcat_char(line, ' ');
			    if (rrest > 0) {
				Strcat_char(line, ' ');
				rrest--;
			    }
			}
		    }
		    lbuf = newTextLine(line, width);
		}
	    }
	}
#endif				/* FORMAT_NICE */
#ifdef TABLE_DEBUG
	if (w3m_debug) {
	    FILE *f = fopen("zzzproc1", "a");
	    fprintf(f, "pos=%d,%d, maxlimit=%d\n",
		    visible_length(lbuf->line->ptr), lbuf->pos,
		    h_env->maxlimit);
	    fclose(f);
	}
#endif
	if (lbuf->pos > h_env->maxlimit)
	    h_env->maxlimit = lbuf->pos;
	if (buf) {
	    pushTextLine(buf, lbuf);
	    if (w3m_backend) {
		Strcat(backend_halfdump_str, lbuf->line);
		Strcat_char(backend_halfdump_str, '\n');
	    }
	}
	else {
	    Strfputs(lbuf->line, f);
	    fputc('\n', f);
	}
	if (obuf->flag & RB_SPECIAL || obuf->flag & RB_NFLUSHED)
	    h_env->blank_lines = 0;
	else
	    h_env->blank_lines++;
    }
    else {
	char *p = line->ptr, *q;
	Str tmp = Strnew(), tmp2 = Strnew();

#define APPEND(str) \
	if (buf) { \
	    appendTextLine(buf,(str),0); \
	    if (w3m_backend) \
		Strcat(backend_halfdump_str, (str)); \
	} else \
	    Strfputs((str),f)

	while (*p) {
	    q = p;
	    if (sloppy_parse_line(&p)) {
		Strcat_charp_n(tmp, q, p - q);
		if (force == 2)
		    APPEND(tmp);
		else
		    Strcat(tmp2, tmp);
		Strclear(tmp);
	    }
	}
	if (force == 2) {
	    if (pass) {
		APPEND(pass);
	    }
	    pass = NULL;
	}
	else {
	    if (pass)
		Strcat(tmp2, pass);
	    pass = tmp2;
	}
    }

    if (obuf->bottom_margin > 0) {
	int i;
	struct html_feed_environ h;
	struct readbuffer o;
	struct environment e[1];

	init_henv(&h, &o, e, 1, NULL, width, indent);
	o.line = Strnew_size(width + 20);
	o.pos = obuf->pos;
	o.flag = obuf->flag;
	o.top_margin = -1;
	o.bottom_margin = -1;
	Strcat_charp(o.line, "<pre_int>");
	for (i = 0; i < o.pos; i++)
	    Strcat_char(o.line, ' ');
	Strcat_charp(o.line, "</pre_int>");
	for (i = 0; i < obuf->bottom_margin; i++)
	    flushline(h_env, &o, indent, force, width);
    }
    if (obuf->top_margin < 0 || obuf->bottom_margin < 0)
	return;

    obuf->line = Strnew_size(256);
    obuf->pos = 0;
    obuf->top_margin = 0;
    obuf->bottom_margin = 0;
    obuf->prevchar = ' ';
    obuf->bp.init_flag = 1;
    obuf->flag &= ~RB_NFLUSHED;
    set_breakpoint(obuf, 0);
    obuf->prev_ctype = PC_ASCII;
    link_stack = NULL;
    fillline(obuf, indent);
    if (pass)
	passthrough(obuf, pass->ptr, 0);
    if (!hidden_anchor && obuf->anchor) {
	Str tmp;
	if (obuf->anchor_hseq > 0)
	    obuf->anchor_hseq = -obuf->anchor_hseq;
	tmp = Sprintf("<A HSEQ=\"%d\" HREF=\"", obuf->anchor_hseq);
	Strcat_charp(tmp, html_quote(obuf->anchor->ptr));
	if (obuf->anchor_target) {
	    Strcat_charp(tmp, "\" TARGET=\"");
	    Strcat_charp(tmp, html_quote(obuf->anchor_target->ptr));
	}
	Strcat_charp(tmp, "\">");
	push_tag(obuf, tmp->ptr, HTML_A);
    }
    if (!hidden_img && obuf->img_alt) {
	Str tmp = Strnew_charp("<IMG_ALT SRC=\"");
	Strcat_charp(tmp, html_quote(obuf->img_alt->ptr));
	Strcat_charp(tmp, "\">");
	push_tag(obuf, tmp->ptr, HTML_IMG_ALT);
    }
    if (!hidden_bold && obuf->in_bold)
	push_tag(obuf, "<B>", HTML_B);
    if (!hidden_under && obuf->in_under)
	push_tag(obuf, "<U>", HTML_U);
}

static void
discardline(struct readbuffer *obuf, int indent)
{
    append_tags(obuf);
    Strclear(obuf->line);
    obuf->pos = 0;
    obuf->prevchar = ' ';
    obuf->bp.init_flag = 1;
    set_breakpoint(obuf, 0);
    obuf->prev_ctype = PC_ASCII;
    fillline(obuf, indent);
}

void
do_blankline(struct html_feed_environ *h_env, struct readbuffer *obuf,
	     int indent, int indent_incr, int width)
{
    if (h_env->buf && h_env->blank_lines == 0)
	flushline(h_env, obuf, indent, 1, width);
    else if (h_env->f)
	flushline(h_env, obuf, indent, 1, width);
}

void
purgeline(struct html_feed_environ *h_env)
{
    char *p, *q;
    Str tmp;

    if (h_env->buf == NULL || h_env->blank_lines == 0)
	return;

    p = rpopTextLine(h_env->buf)->line->ptr;
    tmp = Strnew();
    while (*p) {
	q = p;
	if (sloppy_parse_line(&p)) {
	    Strcat_charp_n(tmp, q, p - q);
	}
    }
    appendTextLine(h_env->buf, tmp, 0);
    h_env->blank_lines--;
}

static int
close_effect0(struct readbuffer *obuf, int cmd)
{
    int i;
    char *p;

    for (i = obuf->tag_sp - 1; i >= 0; i--) {
	if (obuf->tag_stack[i]->cmd == cmd)
	    break;
    }
    if (i >= 0) {
	obuf->tag_sp--;
	bcopy(&obuf->tag_stack[i + 1], &obuf->tag_stack[i],
	      (obuf->tag_sp - i) * sizeof(struct cmdtable *));
	return 1;
    }
    else if ((p = has_hidden_link(obuf, cmd)) != NULL) {
	passthrough(obuf, p, 1);
	return 1;
    }
    return 0;
}

static void
close_anchor(struct html_feed_environ *h_env, struct readbuffer *obuf)
{
    if (obuf->anchor) {
	int i;
	char *p = NULL;
	int is_erased = 0;

	for (i = obuf->tag_sp - 1; i >= 0; i--) {
	    if (obuf->tag_stack[i]->cmd == HTML_A)
		break;
	}
	if (i < 0 && obuf->anchor_hseq > 0 && Strlastchar(obuf->line) == ' ') {
	    Strshrink(obuf->line, 1);
	    obuf->pos--;
	    is_erased = 1;
	}

	if (i >= 0 || (p = has_hidden_link(obuf, HTML_A))) {
	    if (obuf->anchor_hseq > 0) {
		HTMLlineproc1(ANSP, h_env);
		obuf->prevchar = ' ';
	    }
	    else {
		if (i >= 0) {
		    obuf->tag_sp--;
		    bcopy(&obuf->tag_stack[i + 1], &obuf->tag_stack[i],
			  (obuf->tag_sp - i) * sizeof(struct cmdtable *));
		}
		else {
		    passthrough(obuf, p, 1);
		}
		obuf->anchor = NULL;
		obuf->anchor_target = NULL;
		return;
	    }
	    is_erased = 0;
	}
	if (is_erased) {
	    Strcat_char(obuf->line, ' ');
	    obuf->pos++;
	}

	push_tag(obuf, "</a>", HTML_N_A);
	obuf->anchor = NULL;
    }
    obuf->anchor_target = NULL;
}

void
save_fonteffect(struct html_feed_environ *h_env, struct readbuffer *obuf)
{
    if (obuf->fontstat_sp < FONT_STACK_SIZE)
	bcopy(obuf->fontstat, obuf->fontstat_stack[obuf->fontstat_sp],
	      FONTSTAT_SIZE);
    obuf->fontstat_sp++;
    if (obuf->in_bold)
	push_tag(obuf, "</b>", HTML_N_B);
    if (obuf->in_under)
	push_tag(obuf, "</u>", HTML_N_U);
    bzero(obuf->fontstat, FONTSTAT_SIZE);
}

void
restore_fonteffect(struct html_feed_environ *h_env, struct readbuffer *obuf)
{
    if (obuf->fontstat_sp > 0)
	obuf->fontstat_sp--;
    if (obuf->fontstat_sp < FONT_STACK_SIZE)
	bcopy(obuf->fontstat_stack[obuf->fontstat_sp], obuf->fontstat,
	      FONTSTAT_SIZE);
    if (obuf->in_bold)
	push_tag(obuf, "<b>", HTML_B);
    if (obuf->in_under)
	push_tag(obuf, "<u>", HTML_U);
}


Str
process_img(struct parsed_tag *tag)
{
    char *p, *q, *r, *r2, *s;
    int w, i;
    Str tmp = Strnew();

    if (!parsedtag_get_value(tag, ATTR_SRC, &p))
	return tmp;
    q = NULL;
    parsedtag_get_value(tag, ATTR_ALT, &q);
    w = -1;
    parsedtag_get_value(tag, ATTR_WIDTH, &w);
    i = -1;
    parsedtag_get_value(tag, ATTR_HEIGHT, &i);
    r = NULL;
    parsedtag_get_value(tag, ATTR_USEMAP, &r);

    tmp = Strnew_size(128);
    if (r) {
	r2 = strchr(r, '#');
	s = "<form_int method=internal action=map>";
	process_form(parse_tag(&s, TRUE));
	Strcat(tmp, Sprintf("<pre_int><input_alt fid=\"%d\" "
			    "type=hidden name=link value=\"", cur_form_id));
	Strcat_charp(tmp, html_quote((r2) ? r2 + 1 : r));
	Strcat(tmp, Sprintf("\"><input_alt hseq=\"%d\" fid=\"%d\" "
			    "type=submit no_effect=true>",
			    cur_hseq++, cur_form_id));
    }
    if (q != NULL && *q == '\0' && ignore_null_img_alt)
	q = NULL;
    if (q != NULL || r != NULL)
	Strcat_charp(tmp, "<img_alt src=\"");
    else
	Strcat_charp(tmp, "<nobr><img_alt src=\"");
    Strcat_charp(tmp, html_quote(p));
    Strcat_charp(tmp, "\">");
    if (q != NULL) {
	Strcat_charp(tmp, html_quote(q));
	Strcat_charp(tmp, "</img_alt> ");
	goto img_end2;
    }
    if (w > 0 && i > 0) {
	/* guess what the image is! */
	if (w < 32 && i < 48) {
	    /* must be an icon or space */
	    if (strcasestr(p, "space") || strcasestr(p, "blank"))
		Strcat_charp(tmp, "_</img_alt>");
	    else {
		if (w * i < 8 * 16)
		    Strcat_charp(tmp, "*</img_alt>");
		else {
#ifdef KANJI_SYMBOLS
		    Strcat_charp(tmp, "��</img_alt>");
#else				/* not KANJI_SYMBOLS */
		    Strcat_charp(tmp, "#</img_alt>");
#endif				/* not KANJI_SYMBOLS */
		}
	    }
	    goto img_end1;
	}
	if (w > 200 && i < 13) {
	    /* must be a horizontal line */
#ifndef KANJI_SYMBOLS
	    Strcat_charp(tmp, "<_RULE TYPE=10>");
#endif				/* not KANJI_SYMBOLS */
	    w /= pixel_per_char;
	    for (i = 0; i < w - (HR_RULE_WIDTH - 1); i += HR_RULE_WIDTH)
		Strcat_charp(tmp, HR_RULE);
#ifndef KANJI_SYMBOLS
	    Strcat_charp(tmp, "</_RULE>");
#endif				/* not KANJI_SYMBOLS */
	    Strcat_charp(tmp, "</img_alt>");
	    goto img_end1;
	}
    }
    for (q = p; *q; q++) ;
    while (q > p && *q != '/')
	q--;
    if (*q == '/')
	q++;
    Strcat_char(tmp, '[');
    p = q;
    for (; *q; q++) {
	if (!IS_ALNUM(*q) && *q != '_' && *q != '-') {
	    break;
	}
	else if (w > 0 && !IS_ALNUM(*q)
		 && q - p + 2 > (int)(w / pixel_per_char)) {
	    Strcat_charp(tmp, "..");
	    break;
	}
	Strcat_char(tmp, *q);
    }
    Strcat_charp(tmp, "]</img_alt>");
  img_end1:
    if (r == NULL)
	Strcat_charp(tmp, "</nobr>");
  img_end2:
    if (r) {
	Strcat_charp(tmp, "</input_alt></pre_int>");
	process_n_form();
    }
    return tmp;
}

Str
process_anchor(struct parsed_tag *tag, char *tagbuf)
{
    if (parsedtag_need_reconstruct(tag)) {
	parsedtag_set_value(tag, ATTR_HSEQ, Sprintf("%d", cur_hseq++)->ptr);
	return parsedtag2str(tag);
    }
    else {
	Str tmp = Sprintf("<a hseq=\"%d\"", cur_hseq++);
	Strcat_charp(tmp, tagbuf + 2);
	return tmp;
    }
}

Str
process_input(struct parsed_tag *tag)
{
    int i, w, v, x, y, z;
    char *q, *p, *r, *p2;
    Str tmp;
    char *qq = "";
    int qlen = 0;

    p = "text";
    parsedtag_get_value(tag, ATTR_TYPE, &p);
    q = NULL;
    parsedtag_get_value(tag, ATTR_VALUE, &q);
    r = "";
    parsedtag_get_value(tag, ATTR_NAME, &r);
    w = 20;
    parsedtag_get_value(tag, ATTR_SIZE, &w);
    i = 20;
    parsedtag_get_value(tag, ATTR_MAXLENGTH, &i);
    p2 = NULL;
    parsedtag_get_value(tag, ATTR_ALT, &p2);
    x = parsedtag_exists(tag, ATTR_CHECKED);
    y = parsedtag_exists(tag, ATTR_ACCEPT);
    z = parsedtag_exists(tag, ATTR_READONLY);

    v = formtype(p);
    if (v == FORM_UNKNOWN)
	return NULL;

    if (!q) {
	switch (v) {
	case FORM_INPUT_IMAGE:
	case FORM_INPUT_SUBMIT:
	case FORM_INPUT_BUTTON:
	    q = "SUBMIT";
	    break;
	case FORM_INPUT_RESET:
	    q = "RESET";
	    break;
	    /* if no VALUE attribute is specified in * <INPUT
	     * TYPE=CHECKBOX> tag, then the value "on" is used * as a
	     * default value. It is not a part of HTML4.0 * specification, 
	     * but an imitation of Netscape * behaviour. */
	case FORM_INPUT_CHECKBOX:
	    q = "on";
	}
    }
    /* VALUE attribute is not allowed in <INPUT TYPE=FILE> tag. */
    if (v == FORM_INPUT_FILE)
	q = NULL;
    if (q) {
	qq = html_quote(q);
	qlen = strlen(q);
    }

    tmp = Strnew_charp("<pre_int>");
    switch (v) {
    case FORM_INPUT_PASSWORD:
    case FORM_INPUT_TEXT:
    case FORM_INPUT_FILE:
    case FORM_INPUT_CHECKBOX:
	Strcat_char(tmp, '[');
	break;
    case FORM_INPUT_RADIO:
	Strcat_char(tmp, '(');
    }
    Strcat(tmp, Sprintf("<input_alt hseq=\"%d\" fid=\"%d\" type=%s "
			"name=\"%s\" width=%d maxlength=%d value=\"%s\"",
			cur_hseq++, cur_form_id, p, html_quote(r), w, i, qq));
    if (x)
	Strcat_charp(tmp, " checked");
    if (y)
	Strcat_charp(tmp, " accept");
    if (z)
	Strcat_charp(tmp, " readonly");
    Strcat_char(tmp, '>');

    if (v == FORM_INPUT_HIDDEN)
	Strcat_charp(tmp, "</input_alt></pre_int>");
    else {
	switch (v) {
	case FORM_INPUT_PASSWORD:
	case FORM_INPUT_TEXT:
	case FORM_INPUT_FILE:
	    Strcat_charp(tmp, "<u>");
	    break;
	case FORM_INPUT_IMAGE:
	case FORM_INPUT_SUBMIT:
	case FORM_INPUT_BUTTON:
	case FORM_INPUT_RESET:
	    Strcat_charp(tmp, "[");
	    break;
	}
	switch (v) {
	case FORM_INPUT_PASSWORD:
	    i = 0;
	    if (q) {
		for (; i < qlen && i < w; i++)
		    Strcat_char(tmp, '*');
	    }
	    for (; i < w; i++)
		Strcat_char(tmp, ' ');
	    break;
	case FORM_INPUT_TEXT:
	case FORM_INPUT_FILE:
	    if (q)
		Strcat(tmp, textfieldrep(Strnew_charp(q), w));
	    else {
		for (i = 0; i < w; i++)
		    Strcat_char(tmp, ' ');
	    }
	    break;
	case FORM_INPUT_IMAGE:
	case FORM_INPUT_SUBMIT:
	case FORM_INPUT_BUTTON:
	    if (p2) {
		Strcat_charp(tmp, html_quote(p2));
		i = strlen(p2);
	    }
	    else {
		Strcat_charp(tmp, qq);
		i = qlen;
	    }
	    break;
	case FORM_INPUT_RESET:
	    Strcat_charp(tmp, qq);
	    i = qlen;
	    break;
	case FORM_INPUT_RADIO:
	case FORM_INPUT_CHECKBOX:
	    if (x)
		Strcat_char(tmp, '*');
	    else
		Strcat_char(tmp, ' ');
	    break;
	}
	switch (v) {
	case FORM_INPUT_PASSWORD:
	case FORM_INPUT_TEXT:
	case FORM_INPUT_FILE:
	    Strcat_charp(tmp, "</u>");
	    break;
	case FORM_INPUT_IMAGE:
	case FORM_INPUT_SUBMIT:
	case FORM_INPUT_BUTTON:
	case FORM_INPUT_RESET:
	    Strcat_charp(tmp, "]");
	}
	Strcat_charp(tmp, "</input_alt>");
	switch (v) {
	case FORM_INPUT_PASSWORD:
	case FORM_INPUT_TEXT:
	case FORM_INPUT_FILE:
	case FORM_INPUT_CHECKBOX:
	    Strcat_char(tmp, ']');
	    break;
	case FORM_INPUT_RADIO:
	    Strcat_char(tmp, ')');
	}
	Strcat_charp(tmp, "</pre_int>");
    }
    return tmp;
}

void
process_select(struct parsed_tag *tag)
{
    char *p;

    p = "";
    parsedtag_get_value(tag, ATTR_NAME, &p);
    cur_select = Strnew_charp(p);
    select_is_multiple = parsedtag_exists(tag, ATTR_MULTIPLE);

#ifdef MENU_SELECT
    if (!select_is_multiple) {
	select_str = Sprintf("<pre_int>[<input_alt hseq=\"%d\" "
			     "fid=\"%d\" type=select name=\"%s\" selectnumber=%d",
			     cur_hseq++, cur_form_id, html_quote(p), n_select);
	Strcat_charp(select_str, ">");
	if (n_select == max_select) {
	    max_select *= 2;
	    select_option =
		New_Reuse(FormSelectOption, select_option, max_select);
	}
	select_option[n_select].first = NULL;
	select_option[n_select].last = NULL;
	cur_option_maxwidth = 0;
    }
    else
#endif				/* MENU_SELECT */
	select_str = Strnew();
    cur_option = NULL;
    cur_status = R_ST_NORMAL;
    n_selectitem = 0;
}

Str
process_n_select(void)
{
    if (cur_select == NULL)
	return NULL;
    process_option();
#ifdef MENU_SELECT
    if (!select_is_multiple) {
	if (select_option[n_select].first) {
	    FormItemList sitem;
	    chooseSelectOption(&sitem, select_option[n_select].first);
	    Strcat(select_str, textfieldrep(sitem.label, cur_option_maxwidth));
	}
	Strcat_charp(select_str, "</input_alt>]</pre_int>");
	n_select++;
    }
    else
#endif				/* MENU_SELECT */
	Strcat_charp(select_str, "<br>");
    cur_select = NULL;
    n_selectitem = 0;
    return select_str;
}

void
feed_select(char *str)
{
    Str tmp = Strnew();
    int prev_status = cur_status;
    static int prev_spaces = -1;
    char *p;

    if (cur_select == NULL)
	return;
    while (read_token(tmp, &str, &cur_status, 0, 0)) {
	if (cur_status != R_ST_NORMAL || prev_status != R_ST_NORMAL)
	    continue;
	p = tmp->ptr;
	if (tmp->ptr[0] == '<' && Strlastchar(tmp) == '>') {
	    struct parsed_tag *tag;
	    char *q;
	    if (!(tag = parse_tag(&p, FALSE)))
		continue;
	    switch (tag->tagid) {
	    case HTML_OPTION:
		process_option();
		cur_option = Strnew();
		if (parsedtag_get_value(tag, ATTR_VALUE, &q))
		    cur_option_value = Strnew_charp(q);
		else
		    cur_option_value = NULL;
		if (parsedtag_get_value(tag, ATTR_LABEL, &q))
		    cur_option_label = Strnew_charp(q);
		else
		    cur_option_label = NULL;
		cur_option_selected = parsedtag_exists(tag, ATTR_SELECTED);
		prev_spaces = -1;
		break;
	    case HTML_N_OPTION:
		/* do nothing */
		break;
	    default:
		/* never happen */
		break;
	    }
	}
	else if (cur_option) {
	    while (*p) {
		if (IS_SPACE(*p) && prev_spaces != 0) {
		    p++;
		    if (prev_spaces > 0)
			prev_spaces++;
		}
		else {
		    if (IS_SPACE(*p))
			prev_spaces = 1;
		    else
			prev_spaces = 0;
		    if (*p == '&')
			Strcat_charp(cur_option, getescapecmd(&p));
		    else
			Strcat_char(cur_option, *(p++));
		}
	    }
	}
    }
}

void
process_option(void)
{
    char begin_char = '[', end_char = ']';

    if (cur_select == NULL || cur_option == NULL)
	return;
    while (cur_option->length > 0 && IS_SPACE(Strlastchar(cur_option)))
	Strshrink(cur_option, 1);
    if (cur_option_value == NULL)
	cur_option_value = cur_option;
    if (cur_option_label == NULL)
	cur_option_label = cur_option;
#ifdef MENU_SELECT
    if (!select_is_multiple) {
	if (cur_option_label->length > cur_option_maxwidth)
	    cur_option_maxwidth = cur_option_label->length;
	addSelectOption(&select_option[n_select],
			cur_option_value,
			cur_option_label, cur_option_selected);
	return;
    }
#endif				/* MENU_SELECT */
    if (!select_is_multiple) {
	begin_char = '(';
	end_char = ')';
    }
    Strcat(select_str, Sprintf("<br><pre_int>%c<input_alt hseq=\"%d\" "
			       "fid=\"%d\" type=%s name=\"%s\" value=\"%s\"",
			       begin_char, cur_hseq++, cur_form_id,
			       select_is_multiple ? "checkbox" : "radio",
			       html_quote(cur_select->ptr),
			       html_quote(cur_option_value->ptr)));
    if (cur_option_selected)
	Strcat_charp(select_str, " checked>*</input_alt>");
    else
	Strcat_charp(select_str, "> </input_alt>");
    Strcat_char(select_str, end_char);
    Strcat_charp(select_str, html_quote(cur_option_label->ptr));
    Strcat_charp(select_str, "</pre_int>");
    n_selectitem++;
}

Str
process_textarea(struct parsed_tag *tag, int width)
{
    char *p;

    p = "";
    parsedtag_get_value(tag, ATTR_NAME, &p);
    cur_textarea = Strnew_charp(p);
    cur_textarea_size = 20;
    if (parsedtag_get_value(tag, ATTR_COLS, &p)) {
	cur_textarea_size = atoi(p);
	if (p[strlen(p) - 1] == '%')
	    cur_textarea_size = width * cur_textarea_size / 100 - 2;
	if (cur_textarea_size <= 0)
	    cur_textarea_size = 20;
    }
    cur_textarea_rows = 1;
    if (parsedtag_get_value(tag, ATTR_ROWS, &p)) {
	cur_textarea_rows = atoi(p);
	if (cur_textarea_rows <= 0)
	    cur_textarea_rows = 1;
    }
    cur_textarea_readonly = parsedtag_exists(tag, ATTR_READONLY);
    if (n_textarea >= max_textarea) {
	max_textarea *= 2;
	textarea_str = New_Reuse(Str, textarea_str, max_textarea);
    }
    textarea_str[n_textarea] = Strnew();
    ignore_nl_textarea = TRUE;

    return NULL;
}

Str
process_n_textarea(void)
{
    Str tmp;
    int i;

    if (cur_textarea == NULL)
	return NULL;

    tmp = Strnew();
    Strcat(tmp, Sprintf("<pre_int>[<input_alt hseq=\"%d\" fid=\"%d\" "
			"type=textarea name=\"%s\" size=%d rows=%d "
			"top_margin=%d textareanumber=%d",
			cur_hseq, cur_form_id,
			html_quote(cur_textarea->ptr),
			cur_textarea_size, cur_textarea_rows,
			cur_textarea_rows - 1, n_textarea));
    if (cur_textarea_readonly)
	Strcat_charp(tmp, " readonly");
    Strcat_charp(tmp, "><u>");
    for (i = 0; i < cur_textarea_size; i++)
	Strcat_char(tmp, ' ');
    Strcat_charp(tmp, "</u></input_alt>]</pre_int>\n");
    cur_hseq++;
    n_textarea++;
    cur_textarea = NULL;

    return tmp;
}

void
feed_textarea(char *str)
{
    if (cur_textarea == NULL)
	return;
    if (ignore_nl_textarea) {
	if (*str == '\r')
	    str++;
	if (*str == '\n')
	    str++;
    }
    ignore_nl_textarea = FALSE;
    while (*str) {
	if (*str == '&')
	    Strcat_charp(textarea_str[n_textarea], getescapecmd(&str));
	else if (*str == '\n') {
	    Strcat_charp(textarea_str[n_textarea], "\r\n");
	    str++;
	}
	else if (*str != '\r')
	    Strcat_char(textarea_str[n_textarea], *(str++));
    }
}

Str
process_hr(struct parsed_tag *tag, int width, int indent_width)
{
    Str tmp = Strnew_charp("<nobr>");
    int i, w = 0;
    int x = ALIGN_CENTER;

    if (width > indent_width)
	width -= indent_width;
    if (parsedtag_get_value(tag, ATTR_WIDTH, &w))
	w = REAL_WIDTH(w, width);
    else
	w = width;

    parsedtag_get_value(tag, ATTR_ALIGN, &x);
    switch (x) {
    case ALIGN_CENTER:
	Strcat_charp(tmp, "<div align=center>");
	break;
    case ALIGN_RIGHT:
	Strcat_charp(tmp, "<div align=right>");
	break;
    case ALIGN_LEFT:
	Strcat_charp(tmp, "<div align=left>");
	break;
    }
#ifndef KANJI_SYMBOLS
    Strcat_charp(tmp, "<_RULE TYPE=10>");
#endif				/* not KANJI_SYMBOLS */
    w -= HR_RULE_WIDTH - 1;
    if (w <= 0)
	w = 1;
    for (i = 0; i < w; i += HR_RULE_WIDTH) {
	Strcat_charp(tmp, HR_RULE);
    }
#ifndef KANJI_SYMBOLS
    Strcat_charp(tmp, "</_RULE>");
#endif				/* not KANJI_SYMBOLS */
    Strcat_charp(tmp, "</div></nobr>");
    return tmp;
}

#ifdef JP_CHARSET
static char
check_charset(char *s)
{
    switch (*s) {
    case CODE_EUC:
    case CODE_SJIS:
    case CODE_JIS_n:
    case CODE_JIS_m:
    case CODE_JIS_N:
    case CODE_JIS_j:
    case CODE_JIS_J:
    case CODE_INNER_EUC:
	return *s;
    }
    return 0;
}

static char
check_accept_charset(char *s)
{
    char *e;
    char c;

    while (*s) {
	while (*s && (IS_SPACE(*s) || *s == ','))
	    s++;
	if (!*s)
	    break;
	e = s;
	while (*e && !(IS_SPACE(*e) || *e == ','))
	    e++;
	c = guess_charset(Strnew_charp_n(s, e - s)->ptr);
	if (c)
	    return c;
	s = e;
    }
    return 0;
}
#endif				/* JP_CHARSET */

Str
process_form(struct parsed_tag *tag)
{
    char *p, *q, *r, *s, *tg, *n;
    char cs = 0;

    p = "get";
    parsedtag_get_value(tag, ATTR_METHOD, &p);
    q = "!CURRENT_URL!";
    parsedtag_get_value(tag, ATTR_ACTION, &q);
    r = NULL;
#ifdef JP_CHARSET
    if (parsedtag_get_value(tag, ATTR_ACCEPT_CHARSET, &r))
	cs = check_accept_charset(r);
    if (!cs && parsedtag_get_value(tag, ATTR_CHARSET, &r))
	cs = check_charset(r);
#endif				/*JP_CHARSET */
    s = NULL;
    parsedtag_get_value(tag, ATTR_ENCTYPE, &s);
    tg = NULL;
    parsedtag_get_value(tag, ATTR_TARGET, &tg);
    n = NULL;
    parsedtag_get_value(tag, ATTR_NAME, &n);
    form_max++;
    form_sp++;
    if (forms_size == 0) {
	forms_size = INITIAL_FORM_SIZE;
	forms = New_N(FormList *, forms_size);
	form_stack = NewAtom_N(int, forms_size);
    }
    else if (forms_size <= form_max) {
	forms_size += form_max;
	forms = New_Reuse(FormList *, forms, forms_size);
	form_stack = New_Reuse(int, form_stack, forms_size);
    }
    forms[form_max] =
	newFormList(q, p, &cs, s, tg, n,
		    (form_max > 0) ? forms[form_max - 1] : NULL);
    form_stack[form_sp] = form_max;

    return NULL;
}

Str
process_n_form(void)
{
    if (form_sp >= 0)
	form_sp--;
    return NULL;
}

static void
clear_ignore_p_flag(int cmd, struct readbuffer *obuf)
{
    static int clear_flag_cmd[] = {
	HTML_HR, HTML_UNKNOWN
    };
    int i;

    for (i = 0; clear_flag_cmd[i] != HTML_UNKNOWN; i++) {
	if (cmd == clear_flag_cmd[i]) {
	    obuf->flag &= ~RB_IGNORE_P;
	    return;
	}
    }
}

static void
set_alignment(struct readbuffer *obuf, struct parsed_tag *tag)
{
    long flag = -1;
    int align;

    if (parsedtag_get_value(tag, ATTR_ALIGN, &align)) {
	switch (align) {
	case ALIGN_CENTER:
	    flag = RB_CENTER;
	    break;
	case ALIGN_RIGHT:
	    flag = RB_RIGHT;
	    break;
	case ALIGN_LEFT:
	    flag = RB_LEFT;
	}
    }
    RB_SAVE_FLAG(obuf);
    if (flag != -1) {
	RB_SET_ALIGN(obuf, flag);
    }
}

#ifdef ID_EXT
static void
process_idattr(struct readbuffer *obuf, int cmd, struct parsed_tag *tag)
{
    char *id = NULL, *framename = NULL;
    Str idtag = NULL;

    /* 
     * HTML_TABLE is handled by the other process.
     */
    if (cmd == HTML_TABLE)
	return;

    parsedtag_get_value(tag, ATTR_ID, &id);
    parsedtag_get_value(tag, ATTR_FRAMENAME, &framename);
    if (id == NULL)
	return;
    if (framename)
	idtag = Sprintf("<_id id=\"%s\" framename=\"%s\">",
			html_quote(id), html_quote(framename));
    else
	idtag = Sprintf("<_id id=\"%s\">", html_quote(id));
    push_tag(obuf, idtag->ptr, HTML_NOP);
}
#endif				/* ID_EXT */

#define CLOSE_P if (obuf->flag & RB_P) { \
      flushline(h_env, obuf, envs[h_env->envc].indent,0,h_env->limit);\
      RB_RESTORE_FLAG(obuf);\
      close_anchor(h_env, obuf);\
      obuf->flag &= ~RB_P;\
    }

#define CLOSE_DT \
    if (obuf->flag & RB_IN_DT) { \
      obuf->flag &= ~RB_IN_DT; \
      HTMLlineproc1("</b>", h_env); \
    }

#define PUSH_ENV(cmd) \
    if (++h_env->envc_real < h_env->nenv) { \
      ++h_env->envc; \
      envs[h_env->envc].env = cmd; \
      envs[h_env->envc].count = 0; \
      if (h_env->envc <= MAX_INDENT_LEVEL) \
        envs[h_env->envc].indent = envs[h_env->envc - 1].indent + INDENT_INCR; \
      else \
        envs[h_env->envc].indent = envs[h_env->envc - 1].indent; \
    }

#define POP_ENV \
    if (h_env->envc_real-- < h_env->nenv) \
      h_env->envc--;

static int
ul_type(struct parsed_tag *tag, int default_type)
{
    char *p;
    if (parsedtag_get_value(tag, ATTR_TYPE, &p)) {
	if (!strcasecmp(p, "disc"))
	    return (int)'d';
	else if (!strcasecmp(p, "circle"))
	    return (int)'c';
	else if (!strcasecmp(p, "square"))
	    return (int)'s';
    }
    return default_type;
}

int
HTMLtagproc1(struct parsed_tag *tag, struct html_feed_environ *h_env)
{
    char *p, *q, *r;
    int i, w, x, y, z, count, width;
    struct readbuffer *obuf = h_env->obuf;
    struct environment *envs = h_env->envs;
    Str tmp;
    int hseq;
    int cmd;
#ifdef ID_EXT
    char *id = NULL;
#endif				/* ID_EXT */

    cmd = tag->tagid;

    if (obuf->flag & RB_PRE) {
	switch (cmd) {
	case HTML_NOBR:
	case HTML_N_NOBR:
	case HTML_PRE_INT:
	case HTML_N_PRE_INT:
	    return 1;
	}
    }

    switch (cmd) {
    case HTML_B:
	obuf->in_bold++;
	if (obuf->in_bold > 1)
	    return 1;
	return 0;
    case HTML_N_B:
	if (obuf->in_bold == 1 && close_effect0(obuf, HTML_B))
	    obuf->in_bold = 0;
	if (obuf->in_bold > 0) {
	    obuf->in_bold--;
	    if (obuf->in_bold == 0)
		return 0;
	}
	return 1;
    case HTML_U:
	obuf->in_under++;
	if (obuf->in_under > 1)
	    return 1;
	return 0;
    case HTML_N_U:
	if (obuf->in_under == 1 && close_effect0(obuf, HTML_U))
	    obuf->in_under = 0;
	if (obuf->in_under > 0) {
	    obuf->in_under--;
	    if (obuf->in_under == 0)
		return 0;
	}
	return 1;
    case HTML_EM:
	HTMLlineproc1("<b>", h_env);
	return 1;
    case HTML_N_EM:
	HTMLlineproc1("</b>", h_env);
	return 1;
    case HTML_P:
    case HTML_N_P:
	CLOSE_P;
	if (!(obuf->flag & RB_IGNORE_P)) {
	    flushline(h_env, obuf, envs[h_env->envc].indent, 1, h_env->limit);
	    do_blankline(h_env, obuf, envs[h_env->envc].indent, 0,
			 h_env->limit);
	}
	obuf->flag |= RB_IGNORE_P;
	if (cmd == HTML_P) {
	    set_alignment(obuf, tag);
	    obuf->flag |= RB_P;
	}
	return 1;
    case HTML_BR:
	flushline(h_env, obuf, envs[h_env->envc].indent, 1, h_env->limit);
	h_env->blank_lines = 0;
	return 1;
    case HTML_EOL:
	if ((obuf->flag & RB_PREMODE) && obuf->pos > envs[h_env->envc].indent)
	    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	return 1;
    case HTML_H:
	if (!(obuf->flag & (RB_PREMODE | RB_IGNORE_P))) {
	    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	    do_blankline(h_env, obuf, envs[h_env->envc].indent, 0,
			 h_env->limit);
	}
	HTMLlineproc1("<b>", h_env);
	set_alignment(obuf, tag);
	return 1;
    case HTML_N_H:
	HTMLlineproc1("</b>", h_env);
	if (!(obuf->flag & RB_PREMODE)) {
	    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	}
	do_blankline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	RB_RESTORE_FLAG(obuf);
	close_anchor(h_env, obuf);
	obuf->flag |= RB_IGNORE_P;
	return 1;
    case HTML_UL:
    case HTML_OL:
    case HTML_BLQ:
	CLOSE_P;
	if (!(obuf->flag & RB_IGNORE_P)) {
	    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	    if (!(obuf->flag & RB_PREMODE) &&
		(h_env->envc == 0 || cmd == HTML_BLQ))
		do_blankline(h_env, obuf, envs[h_env->envc].indent, 0,
			     h_env->limit);
	}
	PUSH_ENV(cmd);
	if (cmd == HTML_UL || cmd == HTML_OL) {
	    if (parsedtag_get_value(tag, ATTR_START, &count) && count > 0) {
		envs[h_env->envc].count = count - 1;
	    }
	}
	if (cmd == HTML_OL) {
	    envs[h_env->envc].type = '1';
	    if (parsedtag_get_value(tag, ATTR_TYPE, &p)) {
		envs[h_env->envc].type = (int)*p;
	    }
	}
	if (cmd == HTML_UL)
	    envs[h_env->envc].type = ul_type(tag, 0);
	flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	return 1;
    case HTML_N_UL:
    case HTML_N_OL:
    case HTML_N_DL:
    case HTML_N_BLQ:
	CLOSE_DT;
	CLOSE_P;
	if (h_env->envc > 0) {
	    flushline(h_env, obuf, envs[h_env->envc - 1].indent, 0,
		      h_env->limit);
	    POP_ENV;
	    if (!(obuf->flag & RB_PREMODE) &&
		(h_env->envc == 0 || cmd == HTML_N_DL || cmd == HTML_N_BLQ)) {
		do_blankline(h_env, obuf,
			     envs[h_env->envc].indent,
			     INDENT_INCR, h_env->limit);
		obuf->flag |= RB_IGNORE_P;
	    }
	}
	close_anchor(h_env, obuf);
	return 1;
    case HTML_DL:
	CLOSE_P;
	if (!(obuf->flag & RB_IGNORE_P)) {
	    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	    if (!(obuf->flag & RB_PREMODE))
		do_blankline(h_env, obuf, envs[h_env->envc].indent, 0,
			     h_env->limit);
	}
	PUSH_ENV(cmd);
	if (parsedtag_exists(tag, ATTR_COMPACT))
	    envs[h_env->envc].env = HTML_DL_COMPACT;
	obuf->flag |= RB_IGNORE_P;
	return 1;
    case HTML_LI:
	CLOSE_P;
	CLOSE_DT;
	if (h_env->envc > 0) {
	    Str num;
	    flushline(h_env, obuf,
		      envs[h_env->envc - 1].indent, 0, h_env->limit);
	    envs[h_env->envc].count++;
	    if (parsedtag_get_value(tag, ATTR_VALUE, &p)) {
		count = atoi(p);
		if (count > 0)
		    envs[h_env->envc].count = count;
	    }
	    switch (envs[h_env->envc].env) {
	    case HTML_UL:
		envs[h_env->envc].type = ul_type(tag, envs[h_env->envc].type);
		for (i = 0; i < INDENT_INCR - 3; i++)
		    push_charp(obuf, 1, NBSP, PC_ASCII);
		switch (envs[h_env->envc].type) {
#ifdef KANJI_SYMBOLS
		case 'd':
		    push_charp(obuf, 2, "��", PC_ASCII);
		    break;
		case 'c':
		    push_charp(obuf, 2, "��", PC_ASCII);
		    break;
		case 's':
		    push_charp(obuf, 2, "��", PC_ASCII);
		    break;
#endif				/* KANJI_SYMBOLS */
		default:
		    push_charp(obuf, 2,
			       ullevel[(h_env->envc_real - 1) % MAX_UL_LEVEL],
			       PC_ASCII);
		    break;
		}
		push_charp(obuf, 1, NBSP, PC_ASCII);
		obuf->prevchar = ' ';
		break;
	    case HTML_OL:
		if (parsedtag_get_value(tag, ATTR_TYPE, &p))
		    envs[h_env->envc].type = (int)*p;
		switch (envs[h_env->envc].type) {
		case 'i':
		    num = romanNumeral(envs[h_env->envc].count);
		    break;
		case 'I':
		    num = romanNumeral(envs[h_env->envc].count);
		    Strupper(num);
		    break;
		case 'a':
		    num = romanAlphabet(envs[h_env->envc].count);
		    break;
		case 'A':
		    num = romanAlphabet(envs[h_env->envc].count);
		    Strupper(num);
		    break;
		default:
		    num = Sprintf("%d", envs[h_env->envc].count);
		    break;
		}
#if INDENT_INCR >= 4
		Strcat_charp(num, ". ");
#else				/* INDENT_INCR < 4 */
		Strcat_char(num, '.');
#endif				/* INDENT_INCR < 4 */
		push_spaces(obuf, 1, INDENT_INCR - num->length);
		push_str(obuf, num->length, num, PC_ASCII);
		break;
	    default:
		push_spaces(obuf, 1, INDENT_INCR);
		break;
	    }
	}
	else {
	    flushline(h_env, obuf, 0, 0, h_env->limit);
	}
	obuf->flag |= RB_IGNORE_P;
	return 1;
    case HTML_DT:
	CLOSE_P;
	if (h_env->envc == 0 ||
	    (h_env->envc_real < h_env->nenv &&
	     envs[h_env->envc].env != HTML_DL &&
	     envs[h_env->envc].env != HTML_DL_COMPACT)) {
	    PUSH_ENV(HTML_DL);
	}
	if (h_env->envc > 0) {
	    flushline(h_env, obuf,
		      envs[h_env->envc - 1].indent, 0, h_env->limit);
	}
	if (!(obuf->flag & RB_IN_DT)) {
	    HTMLlineproc1("<b>", h_env);
	    obuf->flag |= RB_IN_DT;
	}
	obuf->flag |= RB_IGNORE_P;
	return 1;
    case HTML_DD:
	CLOSE_P;
	CLOSE_DT;
	if (envs[h_env->envc].env == HTML_DL_COMPACT) {
	    if (obuf->pos > envs[h_env->envc].indent)
		flushline(h_env, obuf, envs[h_env->envc].indent, 0,
			  h_env->limit);
	    else
		push_spaces(obuf, 1, envs[h_env->envc].indent - obuf->pos);
	}
	else
	    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	/* obuf->flag |= RB_IGNORE_P; */
	return 1;
    case HTML_TITLE:
	append_tags(obuf);
	save_line = obuf->line;
	save_prevchar = obuf->prevchar;
	set_breakpoint(obuf, 0);
	obuf->line = Strnew();
	discardline(obuf, 0);
	obuf->flag |= (RB_NOBR | RB_TITLE);
	return 1;
    case HTML_N_TITLE:
	if (!(obuf->flag & RB_TITLE))
	    return 1;
	obuf->flag &= ~(RB_NOBR | RB_TITLE);
	append_tags(obuf);
	tmp = Strnew_charp(obuf->line->ptr);
	Strremovetrailingspaces(tmp);
	h_env->title = html_unquote(tmp->ptr);
	obuf->line = save_line;
	obuf->prevchar = save_prevchar;
	back_to_breakpoint(obuf);
	tmp = Strnew_m_charp("<title_alt title=\"",
			     html_quote(h_env->title), "\">", NULL);
	push_tag(obuf, tmp->ptr, HTML_TITLE_ALT);
	return 1;
    case HTML_FRAMESET:
	PUSH_ENV(cmd);
	push_charp(obuf, 9, "--FRAME--", PC_ASCII);
	flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	return 0;
    case HTML_N_FRAMESET:
	if (h_env->envc > 0) {
	    POP_ENV;
	    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	}
	return 0;
    case HTML_FRAME:
	q = r = NULL;
	parsedtag_get_value(tag, ATTR_SRC, &q);
	parsedtag_get_value(tag, ATTR_NAME, &r);
	if (q) {
	    q = html_quote(q);
	    push_tag(obuf, Sprintf("<a hseq=\"%d\" href=\"%s\">",
				   cur_hseq++, q)->ptr, HTML_A);
	    if (r)
		q = html_quote(r);
	    push_charp(obuf, strlen(q), q, PC_ASCII);
	    push_tag(obuf, "</a>", HTML_N_A);
	}
	flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	return 0;
    case HTML_HR:
	tmp = process_hr(tag, h_env->limit, envs[h_env->envc].indent);
	HTMLlineproc1(tmp->ptr, h_env);
	obuf->prevchar = ' ';
	close_anchor(h_env, obuf);
	return 1;
    case HTML_PRE:
	if (!parsedtag_exists(tag, ATTR_FOR_TABLE))
	    CLOSE_P;
	if (!(obuf->flag & RB_IGNORE_P))
	    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	else
	    fillline(obuf, envs[h_env->envc].indent);
	obuf->flag |= (RB_PRE | RB_IGNORE_P);
	/* istr = str; */
	return 1;
    case HTML_N_PRE:
	flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	obuf->flag &= ~RB_PRE;
	close_anchor(h_env, obuf);
	return 1;
    case HTML_PRE_INT:
	i = obuf->line->length;
	append_tags(obuf);
	if (!(obuf->flag & RB_SPECIAL)) {
	    set_breakpoint(obuf, obuf->line->length - i);
	}
	obuf->flag |= RB_PRE_INT;
	return 0;
    case HTML_N_PRE_INT:
	push_tag(obuf, "</pre_int>", HTML_N_PRE_INT);
	obuf->flag &= ~RB_PRE_INT;
	if (!(obuf->flag & RB_SPECIAL) && obuf->pos > obuf->bp.pos) {
	    obuf->prevchar = '\0';
	    obuf->prev_ctype = PC_CTRL;
	}
	return 1;
    case HTML_NOBR:
	obuf->flag |= RB_NOBR;
	obuf->nobr_level++;
	return 0;
    case HTML_N_NOBR:
	if (obuf->nobr_level > 0)
	    obuf->nobr_level--;
	if (obuf->nobr_level == 0)
	    obuf->flag &= ~RB_NOBR;
	return 0;
    case HTML_LISTING:
	CLOSE_P;
	flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	obuf->flag |= (RB_LSTMODE | RB_IGNORE_P);
	/* istr = str; */
	return 1;
    case HTML_N_LISTING:
	CLOSE_P;
	flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	obuf->flag &= ~RB_LSTMODE;
	return 1;
    case HTML_XMP:
	CLOSE_P;
	flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	obuf->flag |= (RB_XMPMODE | RB_IGNORE_P);
	/* istr = str; */
	return 1;
    case HTML_N_XMP:
	CLOSE_P;
	flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	obuf->flag &= ~RB_XMPMODE;
	return 1;
    case HTML_SCRIPT:
	obuf->flag |= RB_IGNORE;
	obuf->ignore_tag = Strnew_charp("</script>");
	return 1;
    case HTML_N_SCRIPT:
	/* should not be reached */
	return 1;
    case HTML_STYLE:
	obuf->flag |= RB_IGNORE;
	obuf->ignore_tag = Strnew_charp("</style>");
	return 1;
    case HTML_N_STYLE:
	/* should not be reached */
	return 1;
    case HTML_PLAINTEXT:
	flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	obuf->flag |= RB_PLAIN;
	/* istr = str; */
	return 1;
    case HTML_A:
	if (obuf->anchor)
	    close_anchor(h_env, obuf);

	hseq = 0;

	if (parsedtag_get_value(tag, ATTR_HREF, &p))
	    obuf->anchor = Strnew_charp(p);
	if (parsedtag_get_value(tag, ATTR_TARGET, &p))
	    obuf->anchor_target = Strnew_charp(p);
	if (parsedtag_get_value(tag, ATTR_HSEQ, &hseq))
	    obuf->anchor_hseq = hseq;

	if (hseq == 0 && obuf->anchor) {
	    obuf->anchor_hseq = cur_hseq;
	    tmp = process_anchor(tag, h_env->tagbuf->ptr);
	    push_tag(obuf, tmp->ptr, HTML_A);
	    return 1;
	}
	return 0;
    case HTML_N_A:
	close_anchor(h_env, obuf);
	return 1;
    case HTML_IMG:
	tmp = process_img(tag);
	HTMLlineproc1(tmp->ptr, h_env);
	return 1;
    case HTML_IMG_ALT:
	if (parsedtag_get_value(tag, ATTR_SRC, &p))
	    obuf->img_alt = Strnew_charp(p);
	return 0;
    case HTML_N_IMG_ALT:
	if (obuf->img_alt) {
	    if (!close_effect0(obuf, HTML_IMG_ALT))
		push_tag(obuf, "</img_alt>", HTML_N_IMG_ALT);
	    obuf->img_alt = NULL;
	}
	return 1;
    case HTML_INPUT_ALT:
	i = 0;
	if (parsedtag_get_value(tag, ATTR_TOP_MARGIN, &i)) {
	    if (i > obuf->top_margin)
		obuf->top_margin = i;
	}
	i = 0;
	if (parsedtag_get_value(tag, ATTR_BOTTOM_MARGIN, &i)) {
	    if (i > obuf->bottom_margin)
		obuf->bottom_margin = i;
	}
	return 0;
    case HTML_TABLE:
	obuf->table_level++;
	if (obuf->table_level >= MAX_TABLE)
	    break;
	w = BORDER_NONE;
	/* x: cellspacing, y: cellpadding */
	x = 2;
	y = 1;
	z = 0;
	width = 0;
	if (parsedtag_exists(tag, ATTR_BORDER)) {
	    if (parsedtag_get_value(tag, ATTR_BORDER, &w)) {
		if (w > 2)
		    w = BORDER_THICK;
		else if (w < 0) {	/* weird */
		    w = BORDER_THIN;
		}
	    }
	    else
		w = BORDER_THIN;
	}
	if (parsedtag_get_value(tag, ATTR_WIDTH, &i)) {
	    if (obuf->table_level == 0)
		width = REAL_WIDTH(i, h_env->limit - envs[h_env->envc].indent);
	    else
		width = RELATIVE_WIDTH(i);
	}
	if (parsedtag_exists(tag, ATTR_HBORDER))
	    w = BORDER_NOWIN;
	parsedtag_get_value(tag, ATTR_CELLSPACING, &x);
	parsedtag_get_value(tag, ATTR_CELLPADDING, &y);
	parsedtag_get_value(tag, ATTR_VSPACE, &z);
#ifdef ID_EXT
	parsedtag_get_value(tag, ATTR_ID, &id);
#endif				/* ID_EXT */
	tables[obuf->table_level] = begin_table(w, x, y, z);
#ifdef ID_EXT
	if (id != NULL)
	    tables[obuf->table_level]->id = Strnew_charp(id);
#endif				/* ID_EXT */
	table_mode[obuf->table_level].pre_mode = 0;
	table_mode[obuf->table_level].indent_level = 0;
	table_mode[obuf->table_level].nobr_level = 0;
	table_mode[obuf->table_level].caption = 0;
#ifndef TABLE_EXPAND
	tables[obuf->table_level]->total_width = width;
#else
	tables[obuf->table_level]->real_width = width;
	tables[obuf->table_level]->total_width = 0;
#endif
	return 1;
    case HTML_N_TABLE:
	/* should be processed in HTMLlineproc() */
	return 1;
    case HTML_CENTER:
	CLOSE_P;
	if (!(obuf->flag & (RB_PREMODE | RB_IGNORE_P)))
	    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	RB_SAVE_FLAG(obuf);
	RB_SET_ALIGN(obuf, RB_CENTER);
	return 1;
    case HTML_N_CENTER:
	CLOSE_P;
	if (!(obuf->flag & RB_PREMODE))
	    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	RB_RESTORE_FLAG(obuf);
	return 1;
    case HTML_DIV:
	CLOSE_P;
	if (!(obuf->flag & RB_IGNORE_P))
	    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	set_alignment(obuf, tag);
	return 1;
    case HTML_N_DIV:
	CLOSE_P;
	flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
	RB_RESTORE_FLAG(obuf);
	return 1;
    case HTML_FORM:
    case HTML_FORM_INT:
	process_form(tag);
	return 1;
    case HTML_N_FORM:
    case HTML_N_FORM_INT:
	process_n_form();
	return 1;
    case HTML_INPUT:
	tmp = process_input(tag);
	if (tmp)
	    HTMLlineproc1(tmp->ptr, h_env);
	return 1;
    case HTML_SELECT:
	process_select(tag);
	obuf->flag |= RB_INSELECT;
	return 1;
    case HTML_N_SELECT:
	obuf->flag &= ~RB_INSELECT;
	tmp = process_n_select();
	if (tmp)
	    HTMLlineproc1(tmp->ptr, h_env);
	return 1;
    case HTML_OPTION:
	/* nothing */
	return 1;
    case HTML_TEXTAREA:
	process_textarea(tag, h_env->limit);
	obuf->flag |= RB_INTXTA;
	return 1;
    case HTML_N_TEXTAREA:
	close_textarea(h_env);
	return 1;
    case HTML_ISINDEX:
	p = "";
	q = "!CURRENT_URL!";
	parsedtag_get_value(tag, ATTR_PROMPT, &p);
	parsedtag_get_value(tag, ATTR_ACTION, &q);
	tmp = Strnew_m_charp("<form method=get action=\"",
			     html_quote(q),
			     "\">",
			     html_quote(p),
			     "<input type=text name=\"\" accept></form>",
			     NULL);
	HTMLlineproc1(tmp->ptr, h_env);
	return 1;
    case HTML_META:
	p = q = NULL;
	parsedtag_get_value(tag, ATTR_HTTP_EQUIV, &p);
	parsedtag_get_value(tag, ATTR_CONTENT, &q);
#ifdef JP_CHARSET
	if (p && q && !strcasecmp(p, "Content-Type") &&
	    (q = strcasestr(q, "charset")) != NULL) {
	    q += 7;
	    SKIP_BLANKS(q);
	    if (*q == '=') {
		q++;
		SKIP_BLANKS(q);
		meta_charset = guess_charset(q);
	    }
	}
	else
#endif
	if (p && q && !strcasecmp(p, "refresh")) {
	    int refresh = atoi(q);
	    Str s_tmp = NULL;

	    while (*q) {
		if (!strncasecmp(q, "url=", 4)) {
		    q += 4;
		    if (*q == '\"')	/* " */
			q++;
		    r = q;
		    while (*r && !IS_SPACE(*r) && *r != ';')
			r++;
		    s_tmp = Strnew_charp_n(q, r - q);

		    if (s_tmp->ptr[s_tmp->length - 1] == '\"') {	/* " 
									 */
			s_tmp->length--;
			s_tmp->ptr[s_tmp->length] = '\0';
		    }
		    q = r;
		}
		while (*q && *q != ';')
		    q++;
		if (*q == ';')
		    q++;
		while (*q && *q == ' ')
		    q++;
	    }
	    if (s_tmp) {
		q = html_quote(s_tmp->ptr);
		tmp =
		    Sprintf
		    ("Refresh (%d sec) <a hseq=\"%d\" href=\"%s\">%s</a>",
		     refresh, cur_hseq++, q, q);
		push_str(obuf, s_tmp->length, tmp, PC_ASCII);
		flushline(h_env, obuf, envs[h_env->envc].indent, 0,
			  h_env->limit);
		if (!is_redisplay && refresh == 0 && MetaRefresh) {
		    pushEvent(FUNCNAME_goURL, s_tmp->ptr);
		    /* pushEvent(deletePrevBuf,NULL); */
		}
#ifdef USE_ALARM
		else if (!is_redisplay && refresh > 0 && MetaRefresh) {
		    setAlarmEvent(refresh, AL_IMPLICIT, FUNCNAME_goURL,
				  s_tmp->ptr);
		}
#endif
	    }
#ifdef USE_ALARM
	    else if (!is_redisplay && refresh > 0 && MetaRefresh) {
		tmp = Sprintf("Refresh (%d sec)", refresh);
		push_str(obuf, 0, tmp, PC_ASCII);
		flushline(h_env, obuf, envs[h_env->envc].indent, 0,
			  h_env->limit);
		setAlarmEvent(refresh, AL_IMPLICIT, FUNCNAME_reload, NULL);
	    }
#endif
	}
	return 1;
    case HTML_BASE:
    case HTML_MAP:
    case HTML_N_MAP:
    case HTML_AREA:
	return 0;
    case HTML_DEL:
	HTMLlineproc1("<U>[DEL:</U>", h_env);
	return 1;
    case HTML_N_DEL:
	HTMLlineproc1("<U>:DEL]</U>", h_env);
	return 1;
    case HTML_INS:
	HTMLlineproc1("<U>[INS:</U>", h_env);
	return 1;
    case HTML_N_INS:
	HTMLlineproc1("<U>:INS]</U>", h_env);
	return 1;
    case HTML_FONT:
    case HTML_N_FONT:
    case HTML_NOP:
	return 1;
    case HTML_BGSOUND:
	if (view_unseenobject) {
	    if (parsedtag_get_value(tag, ATTR_SRC, &p)) {
		Str s;
		q = html_quote(p);
		s = Sprintf("<A HREF=\"%s\">bgsound(%s)</A>", q, q);
		HTMLlineproc1(s->ptr, h_env);
	    }
	}
	return 1;
    case HTML_EMBED:
	if (view_unseenobject) {
	    if (parsedtag_get_value(tag, ATTR_SRC, &p)) {
		Str s;
		q = html_quote(p);
		s = Sprintf("<A HREF=\"%s\">embed(%s)</A>", q, q);
		HTMLlineproc1(s->ptr, h_env);
	    }
	}
	return 1;
    case HTML_APPLET:
	if (view_unseenobject) {
	    if (parsedtag_get_value(tag, ATTR_ARCHIVE, &p)) {
		Str s;
		q = html_quote(p);
		s = Sprintf("<A HREF=\"%s\">applet archive(%s)</A>", q, q);
		HTMLlineproc1(s->ptr, h_env);
	    }
	}
	return 1;
    case HTML_BODY:
	if (view_unseenobject) {
	    if (parsedtag_get_value(tag, ATTR_BACKGROUND, &p)) {
		Str s;
		q = html_quote(p);
		s = Sprintf("<IMG SRC=\"%s\" ALT=\"bg image(%s)\"><BR>", q, q);
		HTMLlineproc1(s->ptr, h_env);
	    }
	}
    case HTML_N_BODY:
	obuf->flag |= RB_IGNORE_P;
	return 1;
    default:
	/* obuf->prevchar = '\0'; */
	return 0;
    }
    /* not reached */
    return 0;
}

#define PPUSH(p,c) {outp[pos]=(p);outc[pos]=(c);pos++;}

static TextLineListItem *_tl_lp2;

static Str
textlist_feed()
{
    TextLine *p;
    if (_tl_lp2 != NULL) {
	p = _tl_lp2->ptr;
	_tl_lp2 = _tl_lp2->next;
	return p->line;
    }
    return NULL;
}

static void
HTMLlineproc2body(Buffer *buf, Str (*feed) (), int llimit)
{
    Anchor *a_href = NULL, *a_img = NULL, *a_form = NULL;
    char outc[LINELEN];
    char *p, *q, *r, *str;
    Lineprop outp[LINELEN], mode, effect;
    int pos;
    int nlines;
    FILE *debug;
    struct frameset *frameset_s[FRAMESTACK_SIZE];
    int frameset_sp = -1;
    union frameset_element *idFrame = NULL;
    char *id = NULL;
    Str tmp;
    int hseq;
    Str line;
    char *endp;
#ifndef KANJI_SYMBOLS
    char rule;
#endif

    if (w3m_debug)
	debug = fopen("zzzerr", "a");

    effect = 0;
    nlines = 0;
    buf->formlist = (form_max >= 0) ? forms[form_max] : NULL;
    while ((line = feed()) != NULL) {
	if (w3m_debug) {
	    Strfputs(line, debug);
	    fputc('\n', debug);
	}
      proc_again:
	if (++nlines == llimit)
	    break;
	pos = 0;
#ifdef ENABLE_REMOVE_TRAILINGSPACES
	Strremovetrailingspaces(line);
#endif
	str = line->ptr;
	endp = str + line->length;
	while (str < endp && pos < LINELEN) {
	    mode = get_mctype(str);
#ifndef KANJI_SYMBOLS
	    if (effect & PC_RULE && *str != '<') {
		PPUSH(PC_ASCII | effect, rule | 0x80);
		str++;
	    }
	    else
#endif
	    if (mode == PC_CTRL || IS_INTSPACE(*str)) {
		PPUSH(PC_ASCII | effect, ' ');
		str++;
	    }
#ifdef JP_CHARSET
	    else if (mode == PC_KANJI) {
		PPUSH(PC_KANJI1 | effect, str[0]);
		PPUSH(PC_KANJI2 | effect, str[1]);
		str += 2;
	    }
#endif
	    else if (mode == PC_ASCII && *str != '<' && *str != '&') {
		PPUSH(mode | effect, *(str++));
	    }
	    else if (*str == '&') {
		/* 
		 * & escape processing
		 */
		int emode;
		p = getescapecmd(&str);
		while (*p) {
		    emode = get_mctype(p);
#ifdef JP_CHARSET
		    if (emode == PC_KANJI) {
			PPUSH(PC_KANJI1 | effect, p[0]);
			PPUSH(PC_KANJI2 | effect, p[1]);
			p += 2;
		    }
		    else
#endif
		    {
			PPUSH(emode | effect, *(p++));
		    }
		}
	    }
	    else {
		/* tag processing */
		struct parsed_tag *tag;
		if (!(tag = parse_tag(&str, TRUE)))
		    continue;
		switch (tag->tagid) {
		case HTML_B:
		    effect |= PE_BOLD;
		    break;
		case HTML_N_B:
		    effect &= ~PE_BOLD;
		    break;
		case HTML_U:
		    effect |= PE_UNDER;
		    break;
		case HTML_N_U:
		    effect &= ~PE_UNDER;
		    break;
		case HTML_A:
		    if (renderFrameSet &&
			parsedtag_get_value(tag, ATTR_FRAMENAME, &p)) {
			p = url_quote_conv(p, buf->document_code);
			if (!idFrame || strcmp(idFrame->body->name, p)) {
			    idFrame = search_frame(renderFrameSet, p);
			    if (idFrame && idFrame->body->attr != F_BODY)
				idFrame = NULL;
			}
		    }
		    p = r = NULL;
		    q = buf->baseTarget;
		    hseq = 0;
		    id = NULL;
		    if (parsedtag_get_value(tag, ATTR_NAME, &id)) {
			id = url_quote_conv(id, buf->document_code);
			registerName(buf, id, currentLn(buf), pos);
		    }
		    if (parsedtag_get_value(tag, ATTR_HREF, &p)) {
			p = remove_space(p);
			p = url_quote_conv(p, buf->document_code);
		    }
		    if (parsedtag_get_value(tag, ATTR_TARGET, &q))
			q = url_quote_conv(q, buf->document_code);
		    if (parsedtag_get_value(tag, ATTR_REFERER, &r))
			r = url_quote_conv(r, buf->document_code);
		    parsedtag_get_value(tag, ATTR_HSEQ, &hseq);
		    if (hseq > 0)
			buf->hmarklist =
			    putHmarker(buf->hmarklist, currentLn(buf),
				       pos, hseq - 1);
		    if (id && idFrame)
			idFrame->body->nameList =
			    putAnchor(idFrame->body->nameList,
				      id,
				      NULL,
				      (Anchor **)NULL,
				      NULL, currentLn(buf), pos);
		    if (p) {
			effect |= PE_ANCHOR;
			a_href = registerHref(buf, remove_space(p), q,
					      r, currentLn(buf), pos);
			a_href->hseq = ((hseq > 0) ? hseq : -hseq) - 1;
		    }
		    break;
		case HTML_N_A:
		    effect &= ~PE_ANCHOR;
		    if (a_href) {
			a_href->end.line = currentLn(buf);
			a_href->end.pos = pos;
			if (a_href->start.line == a_href->end.line &&
			    a_href->start.pos == a_href->end.pos)
			    a_href->hseq = -1;
			a_href = NULL;
		    }
		    break;
		case HTML_IMG_ALT:
		    if (parsedtag_get_value(tag, ATTR_SRC, &p)) {
			p = remove_space(p);
			p = url_quote_conv(p, buf->document_code);
			a_img = registerImg(buf, p, currentLn(buf), pos);
		    }
		    effect |= PE_IMAGE;
		    break;
		case HTML_N_IMG_ALT:
		    effect &= ~PE_IMAGE;
		    if (a_img) {
			a_img->end.line = currentLn(buf);
			a_img->end.pos = pos;
		    }
		    a_img = NULL;
		    break;
		case HTML_INPUT_ALT:
		    {
			FormList *form;
			int top = 0, bottom = 0;
			int form_id = -1;

			hseq = 0;
			parsedtag_get_value(tag, ATTR_HSEQ, &hseq);
			parsedtag_get_value(tag, ATTR_FID, &form_id);
			parsedtag_get_value(tag, ATTR_TOP_MARGIN, &top);
			parsedtag_get_value(tag, ATTR_BOTTOM_MARGIN, &bottom);
			if (form_id < 0 || form_id > form_max || forms == NULL)
			    break;	/* outside of <form>..</form> */
			form = forms[form_id];
			if (hseq > 0) {
			    int hpos = pos;
			    if (*str == '[')
				hpos++;
			    buf->hmarklist =
				putHmarker(buf->hmarklist, currentLn(buf),
					   hpos, hseq - 1);
			}
			if (!form->target)
			    form->target = buf->baseTarget;
			a_form =
			    registerForm(buf, form, tag, currentLn(buf), pos);
			if (a_form) {
			    a_form->hseq = hseq - 1;
			    a_form->y = currentLn(buf) - top;
			    a_form->rows = 1 + top + bottom;
			    if (!parsedtag_exists(tag, ATTR_NO_EFFECT))
				effect |= PE_FORM;
			    break;
			}
		    }
		case HTML_N_INPUT_ALT:
		    effect &= ~PE_FORM;
		    if (a_form) {
			a_form->end.line = currentLn(buf);
			a_form->end.pos = pos;
			if (a_form->start.line == a_form->end.line &&
			    a_form->start.pos == a_form->end.pos)
			    a_form->hseq = -1;
		    }
		    a_form = NULL;
		    break;
		case HTML_MAP:
		    if (parsedtag_get_value(tag, ATTR_NAME, &p)) {
			MapList *m = New(MapList);
			m->name = Strnew_charp(p);
			m->next = buf->maplist;
			m->urls = newTextList();
			m->alts = newTextList();
			buf->maplist = m;
		    }
		    break;
		case HTML_N_MAP:
		    /* nothing to do */
		    break;
		case HTML_AREA:
		    if (buf->maplist == NULL)	/* outside of *
						 * * * * * <map>..</map> */
			break;
		    if (parsedtag_get_value(tag, ATTR_HREF, &p)) {
			p = remove_space(p);
			p = url_quote_conv(p, buf->document_code);
			pushText(buf->maplist->urls, p);
			if (parsedtag_get_value(tag, ATTR_ALT, &q))
			    pushText(buf->maplist->alts, q);
			else
			    pushText(buf->maplist->alts, "");
		    }
		    break;
		case HTML_FRAMESET:
		    frameset_sp++;
		    if (frameset_sp >= FRAMESTACK_SIZE)
			break;
		    frameset_s[frameset_sp] = newFrameSet(tag);
		    if (frameset_s[frameset_sp] == NULL)
			break;
		    if (frameset_sp == 0) {
			if (buf->frameset == NULL) {
			    buf->frameset = frameset_s[frameset_sp];
			}
			else
			    pushFrameTree(&(buf->frameQ),
					  frameset_s[frameset_sp], NULL);
		    }
		    else
			addFrameSetElement(frameset_s[frameset_sp - 1],
					   *(union frameset_element *)
					   &frameset_s[frameset_sp]);
		    break;
		case HTML_N_FRAMESET:
		    if (frameset_sp >= 0)
			frameset_sp--;
		    break;
		case HTML_FRAME:
		    if (frameset_sp >= 0 && frameset_sp < FRAMESTACK_SIZE) {
			union frameset_element element;

			element.body = newFrame(tag, buf);
			addFrameSetElement(frameset_s[frameset_sp], element);
		    }
		    break;
		case HTML_BASE:
		    if (parsedtag_get_value(tag, ATTR_HREF, &p)) {
			p = remove_space(p);
			p = url_quote_conv(p, buf->document_code);
			if (!buf->baseURL)
			    buf->baseURL = New(ParsedURL);
			parseURL(p, buf->baseURL, NULL);
		    }
		    if (parsedtag_get_value(tag, ATTR_TARGET, &p))
			buf->baseTarget =
			    url_quote_conv(p, buf->document_code);
		    break;
		case HTML_TITLE_ALT:
		    if (parsedtag_get_value(tag, ATTR_TITLE, &p))
			buf->buffername = html_unquote(p);
		    break;
#ifndef KANJI_SYMBOLS
		case HTML_RULE:
		    effect |= PC_RULE;
		    if (parsedtag_get_value(tag, ATTR_TYPE, &p))
			rule = (char)atoi(p);
		    break;
		case HTML_N_RULE:
		    effect &= ~PC_RULE;
		    break;
#endif				/* not KANJI_SYMBOLS */
		}
#ifdef	ID_EXT
		id = NULL;
		if (parsedtag_get_value(tag, ATTR_ID, &id)) {
		    id = url_quote_conv(id, buf->document_code);
		    registerName(buf, id, currentLn(buf), pos);
		}
		if (renderFrameSet &&
		    parsedtag_get_value(tag, ATTR_FRAMENAME, &p)) {
		    p = url_quote_conv(p, buf->document_code);
		    if (!idFrame || strcmp(idFrame->body->name, p)) {
			idFrame = search_frame(renderFrameSet, p);
			if (idFrame && idFrame->body->attr != F_BODY)
			    idFrame = NULL;
		    }
		}
		if (id && idFrame)
		    idFrame->body->nameList =
			putAnchor(idFrame->body->nameList,
				  id,
				  NULL,
				  (Anchor **)NULL, NULL, currentLn(buf), pos);
#endif				/* ID_EXT */
	    }
	}
	/* end of processing for one line */
	addnewline(buf, outc, outp,
#ifdef USE_ANSI_COLOR
		   NULL,
#endif
		   pos, nlines);
	if (str != endp) {
	    line = Strsubstr(line, str - line->ptr, endp - str);
	    goto proc_again;
	}
    }
    if (w3m_debug)
	fclose(debug);
    if (n_textarea)
	addMultirowsForm(buf, buf->formitem);
}

void
HTMLlineproc2(Buffer *buf, TextLineList *tl)
{
    _tl_lp2 = tl->first;
    HTMLlineproc2body(buf, textlist_feed, -1);
}

static InputStream _file_lp2;

static Str
file_feed()
{
    Str s;
    s = StrISgets(_file_lp2);
    if (s->length == 0) {
	ISclose(_file_lp2);
	return NULL;
    }
    return s;
}

void
HTMLlineproc3(Buffer *buf, InputStream stream)
{
    _file_lp2 = stream;
    HTMLlineproc2body(buf, file_feed, -1);
}

static void
proc_escape(struct readbuffer *obuf, char **str_return)
{
    char *str = *str_return, *estr;
    int ech = getescapechar(str_return);
    int width, n_add = *str_return - str;
    Lineprop mode;

    if (ech < 0) {
	*str_return = str;
	proc_mchar(obuf, obuf->flag & RB_SPECIAL, 1, str_return, PC_ASCII);
	return;
    }
    mode = IS_CNTRL(ech) ? PC_CTRL : PC_ASCII;

    check_breakpoint(obuf, obuf->flag & RB_SPECIAL, ech);
    estr = conv_entity(ech);
    width = strlen(estr);
    if (width == 1 && ech == (unsigned char)*estr &&
	ech != '&' && ech != '<' && ech != '>')
	push_charp(obuf, width, estr, mode);
    else
	push_nchars(obuf, width, str, n_add, mode);
    obuf->prevchar = ech;
    obuf->prev_ctype = mode;
}


static int
need_flushline(struct html_feed_environ *h_env, struct readbuffer *obuf,
	       Lineprop mode)
{
    char ch;

    if (obuf->flag & RB_PRE_INT) {
	if (obuf->pos > h_env->limit)
	    return 1;
	else
	    return 0;
    }

    ch = Strlastchar(obuf->line);
    /* if (ch == ' ' && obuf->tag_sp > 0) */
    if (ch == ' ')
	return 0;

    if (obuf->pos > h_env->limit)
	return 1;

    return 0;
}

static int
table_width(struct html_feed_environ *h_env, int table_level)
{
    int width;
    if (table_level < 0)
	return 0;
    width = tables[table_level]->total_width;
    if (table_level > 0 || width > 0)
	return width;
    return h_env->limit - h_env->envs[h_env->envc].indent;
}

/* HTML processing first pass */
void
HTMLlineproc0(char *istr, struct html_feed_environ *h_env, int internal)
{
    Lineprop mode;
    char *str = istr, *q;
    int cmd;
    struct readbuffer *obuf = h_env->obuf;
    int indent, delta;
    struct parsed_tag *tag;
    Str tokbuf;
    struct table *tbl = NULL;
    struct table_mode *tbl_mode;
    int tbl_width;

    if (w3m_debug) {
	FILE *f = fopen("zzzproc1", "a");
	fprintf(f, "%c%c%c%c",
		(obuf->flag & RB_PREMODE) ? 'P' : ' ',
		(obuf->table_level >= 0) ? 'T' : ' ',
		(obuf->flag & RB_INTXTA) ? 'X' : ' ',
		(obuf->flag & RB_IGNORE) ? 'I' : ' ');
	fprintf(f, "HTMLlineproc1(\"%s\",%d,%lx)\n", istr, h_env->limit,
		(unsigned long)h_env);
	fclose(f);
    }

    /* comment processing */
    if (obuf->status == R_ST_CMNT || obuf->status == R_ST_NCMNT3 ||
	obuf->status == R_ST_IRRTAG) {
	while (*str != '\0' && obuf->status != R_ST_NORMAL) {
	    next_status(*str, &obuf->status);
	    str++;
	}
	if (obuf->status != R_ST_NORMAL)
	    return;
    }

    tokbuf = Strnew();

  table_start:
    if (obuf->table_level >= 0) {
	int level = min(obuf->table_level, MAX_TABLE - 1);
	tbl = tables[level];
	tbl_mode = &table_mode[level];
	tbl_width = table_width(h_env, level);
    }

    while (*str != '\0') {
	int is_tag = FALSE;

	if (obuf->flag & RB_PLAIN)
	    goto read_as_plain;	/* don't process tag */

	if (*str == '<' || ST_IS_TAG(obuf->status)) {
	    int pre_mode = (obuf->table_level >= 0) ?
		tbl_mode->pre_mode & TBLM_PLAIN : obuf->flag & RB_PLAINMODE;
	    /* 
	     * Tag processing
	     */
	    if (ST_IS_TAG(obuf->status)) {
/*** continuation of a tag ***/
		read_token(h_env->tagbuf, &str, &obuf->status, pre_mode, 1);
	    }
	    else {
		if (!REALLY_THE_BEGINNING_OF_A_TAG(str)) {
		    /* this is NOT a beginning of a tag */
		    obuf->status = R_ST_NORMAL;
		    HTMLlineproc1("&lt;", h_env);
		    str++;
		    continue;
		}
		read_token(h_env->tagbuf, &str, &obuf->status, pre_mode, 0);
	    }
	    if (ST_IS_COMMENT(obuf->status)) {
		if (obuf->flag & RB_IGNORE)
		    /* within ignored tag, such as *
		     * <script>..</script>, don't process comment.  */
		    obuf->status = R_ST_NORMAL;
		return;
	    }
	    if (h_env->tagbuf->length == 0)
		continue;
	    if (obuf->status != R_ST_NORMAL) {
		if (!pre_mode) {
		    if (Strlastchar(h_env->tagbuf) == '\n')
			Strchop(h_env->tagbuf);
		    if (ST_IS_REAL_TAG(obuf->status))
			Strcat_char(h_env->tagbuf, ' ');
		}
		if ((obuf->flag & RB_IGNORE) &&
		    !TAG_IS(h_env->tagbuf->ptr, obuf->ignore_tag->ptr,
			    obuf->ignore_tag->length - 1))
		    /* within ignored tag, such as *
		     * <script>..</script>, don't process tag.  */
		    obuf->status = R_ST_NORMAL;
		continue;
	    }
	    is_tag = TRUE;
	    q = h_env->tagbuf->ptr;
	}

	if (obuf->flag & (RB_INTXTA | RB_INSELECT | RB_IGNORE)) {
	    cmd = HTML_UNKNOWN;
	    if (!is_tag) {
		read_token(tokbuf, &str, &obuf->status,
			   (obuf->flag & RB_INTXTA) ? 1 : 0, 0);
		if (obuf->status != R_ST_NORMAL)
		    continue;
		q = tokbuf->ptr;
	    }
	    else {
		char *p = q;
		cmd = gethtmlcmd(&p, NULL);
	    }

	    /* textarea */
	    if (obuf->flag & RB_INTXTA) {
		if (cmd == HTML_N_TEXTAREA)
		    goto proc_normal;
		feed_textarea(q);
	    }
	    else if (obuf->flag & RB_INSELECT) {
		if (cmd == HTML_N_SELECT || cmd == HTML_N_FORM)
		    goto proc_normal;
		feed_select(q);
	    }
	    /* script */
	    else if (obuf->flag & RB_IGNORE) {
		if (TAG_IS(q, obuf->ignore_tag->ptr,
			   obuf->ignore_tag->length - 1)) {
		    obuf->flag &= ~RB_IGNORE;
		}
	    }
	    continue;
	}

	if (obuf->table_level >= 0) {
	    /* 
	     * within table: in <table>..</table>, all input tokens
	     * are fed to the table renderer, and then the renderer
	     * makes HTML output.
	     */

	    if (!is_tag) {
		read_token(tokbuf, &str, &obuf->status,
			   tbl_mode->pre_mode & TBLM_PREMODE, 0);
		if (obuf->status != R_ST_NORMAL)
		    continue;
		q = tokbuf->ptr;
	    }

	    switch (feed_table(tbl, q, tbl_mode, tbl_width, internal)) {
	    case 0:
		/* </table> tag */
		obuf->table_level--;
		if (obuf->table_level >= MAX_TABLE - 1)
		    continue;
		end_table(tbl);
		if (obuf->table_level >= 0) {
		    Str tmp;
		    struct table *tbl0 = tables[obuf->table_level];
		    tmp = Sprintf("<table_alt tid=%d>", tbl0->ntable);
		    pushTable(tbl0, tbl);
		    tbl = tbl0;
		    tbl_mode = &table_mode[obuf->table_level];
		    tbl_width = table_width(h_env, obuf->table_level);
		    feed_table(tbl, tmp->ptr, tbl_mode, tbl_width, TRUE);
		    continue;
		    /* continue to the next */
		}
		/* all tables have been read */
		if (tbl->vspace > 0 && !(obuf->flag & RB_IGNORE_P)) {
		    int indent = h_env->envs[h_env->envc].indent;
		    flushline(h_env, obuf, indent, 0, h_env->limit);
		    do_blankline(h_env, obuf, indent, 0, h_env->limit);
		}
		save_fonteffect(h_env, obuf);
		renderTable(tbl, tbl_width, h_env);
		restore_fonteffect(h_env, obuf);
		obuf->flag &= ~RB_IGNORE_P;
		if (tbl->vspace > 0) {
		    int indent = h_env->envs[h_env->envc].indent;
		    do_blankline(h_env, obuf, indent, 0, h_env->limit);
		    obuf->flag |= RB_IGNORE_P;
		}
		obuf->prevchar = ' ';
		continue;
	    case 1:
		/* <table> tag */
		goto proc_normal;
	    default:
		continue;
	    }
	}

      proc_normal:
	if (is_tag) {
/*** Beginning of a new tag ***/
	    if ((tag = parse_tag(&q, internal)))
		cmd = tag->tagid;
	    else
		cmd = HTML_UNKNOWN;
	    if (((obuf->flag & RB_XMPMODE) && cmd != HTML_N_XMP) ||
		((obuf->flag & RB_LSTMODE) && cmd != HTML_N_LISTING)) {
		Str tmp = Strdup(h_env->tagbuf);
		Strcat_charp(tmp, str);
		str = tmp->ptr;
		goto read_as_plain;
	    }
	    if (cmd == HTML_UNKNOWN)
		continue;
	    /* process tags */
	    if (HTMLtagproc1(tag, h_env) == 0) {
		/* preserve the tag for second-stage processing */
		if (parsedtag_need_reconstruct(tag))
		    h_env->tagbuf = parsedtag2str(tag);
		push_tag(obuf, h_env->tagbuf->ptr, cmd);
	    }
#ifdef ID_EXT
	    else {
		process_idattr(obuf, cmd, tag);
	    }
#endif				/* ID_EXT */
	    obuf->bp.init_flag = 1;
	    clear_ignore_p_flag(cmd, obuf);
	    if (cmd == HTML_TABLE)
		goto table_start;
	    else
		continue;
	}

      read_as_plain:
	mode = get_mctype(str);
	delta = get_mclen(mode);
	if (obuf->flag & (RB_SPECIAL & ~RB_NOBR)) {
	    char ch = *str;
	    if (!(obuf->flag & RB_PLAINMODE) && (*str == '&')) {
		char *p = str;
		int ech = getescapechar(&p);
		if (ech == '\n' || ech == '\r') {
		    ch = '\n';
		    str = p - 1;
		}
		else if (ech == '\t') {
		    ch = '\t';
		    str = p - 1;
		}
	    }
	    if (ch != '\n')
		obuf->flag &= ~RB_IGNORE_P;
	    if (ch == '\n') {
		str++;
		if (obuf->flag & RB_IGNORE_P) {
		    obuf->flag &= ~RB_IGNORE_P;
		    continue;
		}
		if (obuf->flag & RB_PRE_INT)
		    PUSH(' ');
		else
		    flushline(h_env, obuf, h_env->envs[h_env->envc].indent, 1,
			      h_env->limit);
	    }
	    else if (ch == '\t') {
		do {
		    PUSH(' ');
		} while (obuf->pos % Tabstop != 0);
		str++;
	    }
	    else if (obuf->flag & RB_PLAINMODE) {
		char *p = html_quote_char(*str);
		if (p) {
		    push_charp(obuf, 1, p, PC_ASCII);
		    str++;
		}
		else {
		    proc_mchar(obuf, 1, delta, &str, mode);
		}
	    }
	    else {
		if (*str == '&')
		    proc_escape(obuf, &str);
		else
		    proc_mchar(obuf, 1, delta, &str, mode);
	    }
	    if (obuf->flag & (RB_SPECIAL & ~RB_PRE_INT))
		continue;
	}
	else {
	    if (!IS_SPACE(*str))
		obuf->flag &= ~RB_IGNORE_P;
	    if ((mode == PC_ASCII || mode == PC_CTRL) && IS_SPACE(*str)) {
		if (obuf->prevchar != ' ') {
		    PUSH(' ');
		}
		str++;
	    }
	    else {
#ifdef JP_CHARSET
		if (mode == PC_KANJI &&
		    obuf->pos > h_env->envs[h_env->envc].indent &&
		    Strlastchar(obuf->line) == ' ') {
		    while (obuf->line->length >= 2 &&
			   !strncmp(obuf->line->ptr + obuf->line->length - 2,
				    "  ", 2)
			   && obuf->pos >= h_env->envs[h_env->envc].indent) {
			Strshrink(obuf->line, 1);
			obuf->pos--;
		    }
		    if (obuf->line->length >= 3 &&
			obuf->prev_ctype == PC_KANJI &&
			Strlastchar(obuf->line) == ' ' &&
			obuf->pos >= h_env->envs[h_env->envc].indent) {
			Strshrink(obuf->line, 1);
			obuf->pos--;
		    }
		}
#endif				/* JP_CHARSET */
		if (*str == '&')
		    proc_escape(obuf, &str);
		else
		    proc_mchar(obuf, obuf->flag & RB_SPECIAL, delta, &str,
			       mode);
	    }
	}
	if (need_flushline(h_env, obuf, mode)) {
	    char *bp = obuf->line->ptr + obuf->bp.len;
	    char *tp = bp - obuf->bp.tlen;
	    int i = 0;

	    if (tp > obuf->line->ptr && tp[-1] == ' ')
		i = 1;

	    indent = h_env->envs[h_env->envc].indent;
	    if (obuf->bp.pos - i > indent) {
		Str line;
		append_tags(obuf);
		line = Strnew_charp(bp);
		Strshrink(obuf->line, obuf->line->length - obuf->bp.len);
#ifdef FORMAT_NICE
		if (obuf->pos - i > h_env->limit)
		    obuf->flag |= RB_FILL;
#endif				/* FORMAT_NICE */
		back_to_breakpoint(obuf);
		flushline(h_env, obuf, indent, 0, h_env->limit);
#ifdef FORMAT_NICE
		obuf->flag &= ~RB_FILL;
#endif				/* FORMAT_NICE */
		HTMLlineproc1(line->ptr, h_env);
	    }
	}
    }
    if (!(obuf->flag & (RB_PREMODE | RB_NOBR | RB_INTXTA | RB_INSELECT
			| RB_PLAINMODE | RB_IGNORE))) {
	char *tp;
	int i = 0;

	if (obuf->bp.pos == obuf->pos) {
	    tp = &obuf->line->ptr[obuf->bp.len - obuf->bp.tlen];
	}
	else {
	    tp = &obuf->line->ptr[obuf->line->length];
	}

	if (tp > obuf->line->ptr && tp[-1] == ' ')
	    i = 1;
	indent = h_env->envs[h_env->envc].indent;
	if (obuf->pos - i > h_env->limit) {
#ifdef FORMAT_NICE
	    obuf->flag |= RB_FILL;
#endif				/* FORMAT_NICE */
	    flushline(h_env, obuf, indent, 0, h_env->limit);
#ifdef FORMAT_NICE
	    obuf->flag &= ~RB_FILL;
#endif				/* FORMAT_NICE */
	}
    }
}

static void
close_textarea(struct html_feed_environ *h_env)
{
    Str tmp;

    h_env->obuf->flag &= ~RB_INTXTA;
    tmp = process_n_textarea();
    if (tmp != NULL)
	HTMLlineproc1(tmp->ptr, h_env);
}

extern char *NullLine;
extern Lineprop NullProp[];

static void
addnewline(Buffer *buf, char *line, Lineprop *prop,
#ifdef USE_ANSI_COLOR
	   Linecolor *color,
#endif
	   int pos, int nlines)
{
    Line *l;
    l = New(Line);
    l->next = NULL;
    if (pos > 0) {
	l->lineBuf = allocStr(line, pos);
	l->propBuf = NewAtom_N(Lineprop, pos);
	bcopy((void *)prop, (void *)l->propBuf, pos * sizeof(Lineprop));
    }
    else {
	l->lineBuf = NullLine;
	l->propBuf = NullProp;
    }
#ifdef USE_ANSI_COLOR
    if (pos > 0 && color) {
	l->colorBuf = NewAtom_N(Linecolor, pos);
	bcopy((void *)color, (void *)l->colorBuf, pos * sizeof(Linecolor));
    }
    else {
	l->colorBuf = NULL;
    }
#endif
    l->len = pos;
    l->width = -1;
    l->prev = buf->currentLine;
    if (buf->currentLine) {
	l->next = buf->currentLine->next;
	buf->currentLine->next = l;
    }
    else
	l->next = NULL;
    if (buf->lastLine == NULL || buf->lastLine == buf->currentLine)
	buf->lastLine = l;
    buf->currentLine = l;
    if (buf->firstLine == NULL)
	buf->firstLine = l;
    l->linenumber = ++buf->allLine;
    if (nlines < 0) {
	/*     l->real_linenumber = l->linenumber;     */
	l->real_linenumber = 0;
    }
    else {
	l->real_linenumber = nlines;
    }
    l = NULL;
}

/* 
 * loadHTMLBuffer: read file and make new buffer
 */
Buffer *
loadHTMLBuffer(URLFile *f, Buffer *newBuf)
{
    FILE *src = NULL;
    Str tmp;

    if (newBuf == NULL)
	newBuf = newBuffer(INIT_BUFFER_WIDTH);
    if (newBuf->sourcefile == NULL &&
	(f->scheme != SCM_LOCAL || newBuf->mailcap)) {
	tmp = tmpfname(TMPF_SRC, ".html");
	pushText(fileToDelete, tmp->ptr);
	src = fopen(tmp->ptr, "w");
	if (src)
	    newBuf->sourcefile = tmp->ptr;
    }

    loadHTMLstream(f, newBuf, src, newBuf->bufferprop & BP_FRAME);

    newBuf->topLine = newBuf->firstLine;
    newBuf->lastLine = newBuf->currentLine;
    newBuf->currentLine = newBuf->firstLine;
    if (n_textarea)
	formResetBuffer(newBuf, newBuf->formitem);
    if (src)
	fclose(src);

    return newBuf;
}

static char *_size_unit[] = { "b", "kb", "Mb", "Gb", "Tb",
    "Pb", "Eb", "Zb", "Bb", "Yb", NULL
};

char *
convert_size(int size, int usefloat)
{
    float csize;
    int sizepos = 0;
    char **sizes = _size_unit;

    csize = (float)size;
    while (csize >= 999.495 && sizes[sizepos + 1]) {
	csize = csize / 1024.0;
	sizepos++;
    }
    return Sprintf(usefloat ? "%.3g%s" : "%.0f%s",
		   floor(csize * 100.0 + 0.5) / 100.0, sizes[sizepos])->ptr;
}

char *
convert_size2(int size1, int size2, int usefloat)
{
    char **sizes = _size_unit;
    float csize, factor = 1;
    int sizepos = 0;

    csize = (float)((size1 > size2) ? size1 : size2);
    while (csize / factor >= 999.495 && sizes[sizepos + 1]) {
	factor *= 1024.0;
	sizepos++;
    }
    return Sprintf(usefloat ? "%.3g/%.3g%s" : "%.0f/%.0f%s",
		   floor(size1 / factor * 100.0 + 0.5) / 100.0,
		   floor(size2 / factor * 100.0 + 0.5) / 100.0,
		   sizes[sizepos])->ptr;
}

void
showProgress(int *linelen, int *trbyte)
{
    int i, j, rate, duration, eta, pos;
    static time_t last_time, start_time;
    time_t cur_time;
    Str messages;
    char *fmtrbyte, *fmrate;

    if (!fmInitialized)
	return;

    if (current_content_length > 0) {
	double ratio;
	cur_time = time(0);
	if (cur_time == last_time)
	    return;
	last_time = cur_time;
	if (*trbyte == 0) {
	    move(LASTLINE, 0);
	    clrtoeolx();
	    start_time = cur_time;
	}
	*trbyte += *linelen;
	*linelen = 0;
	move(LASTLINE, 0);
	ratio = 100.0 * (*trbyte) / current_content_length;
	fmtrbyte = convert_size2(*trbyte, current_content_length, 1);
	duration = cur_time - start_time;
	if (duration) {
	    rate = *trbyte / duration;
	    fmrate = convert_size(rate, 1);
	    eta = rate ? (current_content_length - *trbyte) / rate : -1;
	    messages = Sprintf("%11s %3.0f%% "
			       "%7s/s "
			       "eta %02d:%02d:%02d     ",
			       fmtrbyte, ratio,
			       fmrate,
			       eta / (60 * 60), (eta / 60) % 60, eta % 60);
	}
	else {
	    messages = Sprintf("%11s %3.0f%%                          ",
			       fmtrbyte, ratio);
	}
	addstr(messages->ptr);
	pos = 42;
	i = pos + (COLS - pos - 1) * (*trbyte) / current_content_length;
	move(LASTLINE, pos);
#if 0				/* def KANJI_SYMBOLS */
	for (j = pos; j <= i; j += 2)
	    addstr("��");
#else				/* not 0 */
	standout();
	addch(' ');
	for (j = pos + 1; j <= i; j++)
	    addch('|');
	standend();
#endif				/* not 0 */
	/* no_clrtoeol(); */
	refresh();
    }
    else if (*linelen > 1000) {
	cur_time = time(0);
	if (cur_time == last_time)
	    return;
	last_time = cur_time;
	if (*trbyte == 0) {
	    move(LASTLINE, 0);
	    clrtoeolx();
	    start_time = cur_time;
	}
	*trbyte += *linelen;
	*linelen = 0;
	move(LASTLINE, 0);
	fmtrbyte = convert_size(*trbyte, 1);
	duration = cur_time - start_time;
	if (duration) {
	    fmrate = convert_size(*trbyte / duration, 1);
	    messages = Sprintf("%7s loaded %7s/s", fmtrbyte, fmrate);
	}
	else {
	    messages = Sprintf("%7s loaded", fmtrbyte);
	}
	message(messages->ptr, 0, 0);
	refresh();
    }
}

void
init_henv(struct html_feed_environ *h_env, struct readbuffer *obuf,
	  struct environment *envs, int nenv, TextLineList *buf,
	  int limit, int indent)
{
    envs[0].indent = indent;

    obuf->line = Strnew();
    obuf->cprop = 0;
    obuf->pos = 0;
    obuf->prevchar = ' ';
    obuf->flag = RB_IGNORE_P;
    obuf->flag_sp = 0;
    obuf->status = R_ST_NORMAL;
    obuf->table_level = -1;
    obuf->nobr_level = 0;
    obuf->anchor = 0;
    obuf->anchor_target = 0;
    obuf->anchor_hseq = 0;
    obuf->img_alt = 0;
    obuf->in_bold = 0;
    obuf->in_under = 0;
    obuf->prev_ctype = PC_ASCII;
    obuf->tag_sp = 0;
    obuf->fontstat_sp = 0;
    obuf->top_margin = 0;
    obuf->bottom_margin = 0;
    obuf->bp.init_flag = 1;
    set_breakpoint(obuf, 0);

    h_env->buf = buf;
    h_env->f = NULL;
    h_env->obuf = obuf;
    h_env->tagbuf = Strnew();
    h_env->limit = limit;
    h_env->maxlimit = 0;
    h_env->envs = envs;
    h_env->nenv = nenv;
    h_env->envc = 0;
    h_env->envc_real = 0;
    h_env->title = NULL;
    h_env->blank_lines = 0;
}

void
completeHTMLstream(struct html_feed_environ *h_env, struct readbuffer *obuf)
{
    close_anchor(h_env, obuf);
    if (obuf->img_alt) {
	push_tag(obuf, "</img_alt>", HTML_N_IMG_ALT);
	obuf->img_alt = NULL;
    }
    if (obuf->in_bold) {
	push_tag(obuf, "</b>", HTML_N_B);
	obuf->in_bold = 0;
    }
    if (obuf->in_under) {
	push_tag(obuf, "</u>", HTML_N_U);
	obuf->in_under = 0;
    }
    /* for unbalanced select tag */
    if (obuf->flag & RB_INSELECT)
	HTMLlineproc1("</select>", h_env);

    /* for unbalanced table tag */
    while (obuf->table_level >= 0) {
	table_mode[obuf->table_level].pre_mode
	    &= ~(TBLM_IGNORE | TBLM_XMP | TBLM_LST);
	HTMLlineproc1("</table>", h_env);
    }
}

void
loadHTMLstream(URLFile *f, Buffer *newBuf, FILE * src, int internal)
{
    struct environment envs[MAX_ENV_LEVEL];
    int linelen = 0;
    int trbyte = 0;
    Str lineBuf2 = Strnew();
    char code;
    struct html_feed_environ htmlenv1;
    struct readbuffer obuf;
    MySignalHandler(*prevtrap) ();

    n_textarea = 0;
    cur_textarea = NULL;
    max_textarea = MAX_TEXTAREA;
    textarea_str = New_N(Str, max_textarea);
#ifdef MENU_SELECT
    n_select = 0;
    max_select = MAX_SELECT;
    select_option = New_N(FormSelectOption, max_select);
#endif				/* MENU_SELECT */
    cur_select = NULL;
    form_sp = -1;
    form_max = -1;
    forms_size = 0;
    forms = NULL;
    cur_hseq = 1;

    if (w3m_halfload) {
	newBuf->buffername = "---";
#ifdef JP_CHARSET
	newBuf->document_code = InnerCode;
#endif				/* JP_CHARSET */
	HTMLlineproc3(newBuf, f->stream);
	w3m_halfload = FALSE;
	if (fmInitialized) {
	    term_raw();
	    signal(SIGINT, prevtrap);
	}
	return;
    }

    init_henv(&htmlenv1, &obuf, envs, MAX_ENV_LEVEL, NULL, newBuf->width, 0);

    if (w3m_halfdump)
	htmlenv1.f = stdout;
    else
	htmlenv1.buf = newTextLineList();

    if (SETJMP(AbortLoading) != 0) {
	HTMLlineproc1("<br>Transfer Interrupted!<br>", &htmlenv1);
	goto phase2;
    }
    if (fmInitialized) {
	prevtrap = signal(SIGINT, KeyAbort);
	term_cbreak();
    }

#ifdef JP_CHARSET
    if (newBuf != NULL && newBuf->document_code != '\0')
	code = newBuf->document_code;
    else if (content_charset != '\0' && UseContentCharset)
	code = content_charset;
    else
	code = DocumentCode;
    meta_charset = '\0';
#endif
#if	0
    do_blankline(&htmlenv1, &obuf, 0, 0, htmlenv1.limit);
    obuf.flag = RB_IGNORE_P;
#endif
    if (IStype(f->stream) != IST_ENCODED)
	f->stream = newEncodedStream(f->stream, f->encoding);
    while ((lineBuf2 = StrmyUFgets(f))->length) {
	if (src)
	    Strfputs(lineBuf2, src);
	linelen += lineBuf2->length;
	showProgress(&linelen, &trbyte);
#ifdef JP_CHARSET
	if (meta_charset != '\0') {	/* <META> */
	    if (content_charset == '\0' && UseContentCharset)
		code = meta_charset;
	    meta_charset = '\0';
	}
#endif
	if (!internal)
	    lineBuf2 = convertLine(f, lineBuf2, &code, HTML_MODE);
#ifdef USE_NNTP
	if (f->scheme == SCM_NEWS) {
	    if (Str_news_endline(lineBuf2)) {
		iseos(f->stream) = TRUE;
		break;
	    }
	}
#endif				/* USE_NNTP */
	HTMLlineproc0(lineBuf2->ptr, &htmlenv1, internal);
    }
    if (obuf.status != R_ST_NORMAL)
	HTMLlineproc1(correct_irrtag(obuf.status)->ptr, &htmlenv1);
    obuf.status = R_ST_NORMAL;
    completeHTMLstream(&htmlenv1, &obuf);
    flushline(&htmlenv1, &obuf, 0, 2, htmlenv1.limit);
    if (htmlenv1.title)
	newBuf->buffername = htmlenv1.title;
    if (w3m_halfdump) {
	if (fmInitialized) {
	    term_raw();
	    signal(SIGINT, prevtrap);
	}
	return;
    }
  phase2:
    newBuf->trbyte = trbyte + linelen;
    if (fmInitialized) {
	term_raw();
	signal(SIGINT, prevtrap);
    }
#ifdef JP_CHARSET
    newBuf->document_code = code;
    content_charset = '\0';
#endif				/* JP_CHARSET */
    HTMLlineproc2(newBuf, htmlenv1.buf);
}

/* 
 * loadHTMLString: read string and make new buffer
 */
Buffer *
loadHTMLString(Str page)
{
    URLFile f;
    MySignalHandler(*prevtrap) ();
    Buffer *newBuf;
    Str tmp;
    FILE *src = NULL;

    newBuf = newBuffer(INIT_BUFFER_WIDTH);
    if (SETJMP(AbortLoading) != 0) {
	discardBuffer(newBuf);
	return NULL;
    }
    init_stream(&f, SCM_LOCAL, newStrStream(page));

    if (fmInitialized) {
	prevtrap = signal(SIGINT, KeyAbort);
	term_cbreak();
    }
    if (w3m_dump & DUMP_FRAME) {
	tmp = tmpfname(TMPF_SRC, ".html");
	pushText(fileToDelete, tmp->ptr);
	src = fopen(tmp->ptr, "w");
	if (src)
	    newBuf->sourcefile = tmp->ptr;
    }

    loadHTMLstream(&f, newBuf, src, TRUE);

    if (fmInitialized) {
	term_raw();
	signal(SIGINT, prevtrap);
    }
    newBuf->topLine = newBuf->firstLine;
    newBuf->lastLine = newBuf->currentLine;
    newBuf->currentLine = newBuf->firstLine;
#ifdef JP_CHARSET
    newBuf->document_code = InnerCode;
#endif				/* JP_CHARSET */
    if (n_textarea)
	formResetBuffer(newBuf, newBuf->formitem);
    if (src)
	fclose(src);

    return newBuf;
}

#ifdef USE_GOPHER

/* 
 * loadGopherDir: get gopher directory
 */
Buffer *
loadGopherDir(URLFile *uf, Buffer *newBuf)
{
#ifdef JP_CHARSET
    char code, ic;
#endif
    Str name, file, host, port;
    char type;
    char *p;
    TextLineList *tl = newTextLineList();
    Str lbuf;
    int hseq = 1;

    if (newBuf == NULL)
	newBuf = newBuffer(INIT_BUFFER_WIDTH);
#ifdef JP_CHARSET
    if (newBuf->document_code != '\0')
	code = newBuf->document_code;
    else if (content_charset != '\0' && UseContentCharset)
	code = content_charset;
    else
	code = DocumentCode;
    content_charset = '\0';
#endif
    while (1) {
	if (lbuf = StrUFgets(uf), lbuf->length == 0)
	    break;
	if (lbuf->ptr[0] == '.' &&
	    (lbuf->ptr[1] == '\n' || lbuf->ptr[1] == '\r'))
	    break;
#ifdef JP_CHARSET
	if ((ic = checkShiftCode(lbuf, code)) != '\0') {
	    if (UseAutoDetect)
		code = ic;
	    lbuf = conv_str(lbuf, code, InnerCode);
	}
#endif				/* JP_CHARSET */
	cleanup_line(lbuf, HTML_MODE);

	p = lbuf->ptr;
	for (name = Strnew(); *p && *p != '\t'; p++)
	    Strcat_char(name, *p);
	p++;
	for (file = Strnew(); *p && *p != '\t'; p++)
	    Strcat_char(file, *p);
	p++;
	for (host = Strnew(); *p && *p != '\t'; p++)
	    Strcat_char(host, *p);
	p++;
	for (port = Strnew(); *p &&
	     *p != '\t' && *p != '\r' && *p != '\n'; p++)
	    Strcat_char(port, *p);
	p++;
	type = name->ptr[0];
	switch (type) {
	case '0':
	    p = "[text file]  ";
	    break;
	case '1':
	    p = "[directory]  ";
	    break;
	case 'm':
	    p = "[message]    ";
	    break;
	case 's':
	    p = "[sound]      ";
	    break;
	case 'g':
	    p = "[gif]        ";
	    break;
	case 'h':
	    p = "[HTML]       ";
	    break;
	default:
	    p = "[unsupported]";
	    break;
	}
	lbuf = Sprintf("<A HSEQ=\"%d\" HREF=\"gopher://", hseq++);
	Strcat(lbuf, host);
	Strcat_char(lbuf, ':');
	Strcat(lbuf, port);
	Strcat_char(lbuf, '/');
	Strcat(lbuf, file);
	Strcat_charp(lbuf, "\">");
	Strcat_charp(lbuf, p);
	Strcat_charp(lbuf, name->ptr + 1);
	pushTextLine(tl, newTextLine(lbuf, visible_length(lbuf->ptr)));
    }
#ifdef JP_CHARSET
    newBuf->document_code = code;
#endif				/* JP_CHARSET */
    HTMLlineproc2(newBuf, tl);
    newBuf->topLine = newBuf->firstLine;
    newBuf->lastLine = newBuf->currentLine;
    newBuf->currentLine = newBuf->firstLine;

    return newBuf;
}
#endif				/* USE_GOPHER */

/* 
 * loadBuffer: read file and make new buffer
 */
Buffer *
loadBuffer(URLFile *uf, Buffer *newBuf)
{
    FILE *src = NULL;
    char code;
    Str lineBuf2;
    char pre_lbuf = '\0';
    int nlines;
    Str tmpf;
    int linelen = 0, trbyte = 0;
#ifdef USE_ANSI_COLOR
    int check_color;
#endif
    MySignalHandler(*prevtrap) ();

    if (newBuf == NULL)
	newBuf = newBuffer(INIT_BUFFER_WIDTH);
    lineBuf2 = Strnew();

    if (SETJMP(AbortLoading) != 0) {
	goto _end;
    }
    if (fmInitialized) {
	prevtrap = signal(SIGINT, KeyAbort);
	term_cbreak();
    }

    if (newBuf->sourcefile == NULL &&
	(uf->scheme != SCM_LOCAL || newBuf->mailcap)) {
	tmpf = tmpfname(TMPF_SRC, NULL);
	src = fopen(tmpf->ptr, "w");
	if (src)
	    newBuf->sourcefile = tmpf->ptr;
    }
#ifdef JP_CHARSET
    if (newBuf->document_code != '\0')
	code = newBuf->document_code;
    else if (content_charset != '\0' && UseContentCharset)
	code = content_charset;
    else
	code = DocumentCode;
    content_charset = '\0';
#endif

    nlines = 0;
    if (IStype(uf->stream) != IST_ENCODED)
	uf->stream = newEncodedStream(uf->stream, uf->encoding);
    while ((lineBuf2 = StrmyISgets(uf->stream))->length) {
	if (src)
	    Strfputs(lineBuf2, src);
	linelen += lineBuf2->length;
	showProgress(&linelen, &trbyte);
	lineBuf2 = convertLine(uf, lineBuf2, &code, PAGER_MODE);
	if (squeezeBlankLine) {
	    if (lineBuf2->ptr[0] == '\n' && pre_lbuf == '\n') {
		++nlines;
		continue;
	    }
	    pre_lbuf = lineBuf2->ptr[0];
	}
	++nlines;
#ifdef USE_NNTP
	if (uf->scheme == SCM_NEWS) {
	    if (Str_news_endline(lineBuf2)) {
		iseos(uf->stream) = TRUE;
		break;
	    }
	}
#endif				/* USE_NNTP */
	Strchop(lineBuf2);
	lineBuf2 = checkType(lineBuf2, propBuffer,
#ifdef USE_ANSI_COLOR
			     colorBuffer, &check_color,
#endif
			     LINELEN);
	addnewline(newBuf, lineBuf2->ptr, propBuffer,
#ifdef USE_ANSI_COLOR
		   check_color ? colorBuffer : NULL,
#endif
		   lineBuf2->length, nlines);
    }
  _end:
    if (fmInitialized) {
	signal(SIGINT, prevtrap);
	term_raw();
    }
    newBuf->topLine = newBuf->firstLine;
    newBuf->lastLine = newBuf->currentLine;
    newBuf->currentLine = newBuf->firstLine;
    newBuf->trbyte = trbyte + linelen;
#ifdef JP_CHARSET
    newBuf->document_code = code;
#endif				/* JP_CHARSET */
    if (src)
	fclose(src);

    return newBuf;
}

/* 
 * saveBuffer: write buffer to file
 */

void
saveBuffer(Buffer *buf, FILE * f)
{
    saveBufferDelNum(buf, f, FALSE);
}

#ifndef KANJI_SYMBOLS
static Str
conv_rule(Line *l)
{
    Str tmp = NULL;
    char *p = l->lineBuf, *ep = p + l->len;
    Lineprop *pr = l->propBuf;

    for (; p < ep; p++, pr++) {
	if (*pr & PC_RULE) {
	    if (tmp == NULL) {
		tmp = Strnew_size(l->len);
		Strcopy_charp_n(tmp, l->lineBuf, p - l->lineBuf);
	    }
	    Strcat_char(tmp, alt_rule[*p & 0xF]);
	}
	else if (tmp != NULL)
	    Strcat_char(tmp, *p);
    }
    if (tmp)
	return tmp;
    else
	return Strnew_charp_n(l->lineBuf, l->len);
}
#endif

void
saveBufferDelNum(Buffer *buf, FILE * f, int del)
{
    Line *l = buf->firstLine;
    Str tmp;
    char *p;

#ifndef KANJI_SYMBOLS
    int is_html = FALSE;

    if (buf->type && !strcasecmp(buf->type, "text/html"))
	is_html = TRUE;
#endif

  pager_next:
    for (; l != NULL; l = l->next) {
#ifndef KANJI_SYMBOLS
	if (is_html)
	    tmp = conv_rule(l);
	else
#endif
	    tmp = Strnew_charp_n(l->lineBuf, l->len);
#ifdef JP_CHARSET
	tmp = conv_str(tmp, InnerCode, DisplayCode);
#endif
	Strfputs(tmp, f);
	if (Strlastchar(tmp) != '\n')
	    putc('\n', f);
    }
    if (buf->pagerSource && !(buf->bufferprop & BP_CLOSE)) {
	l = getNextPage(buf, PagerMax);
	goto pager_next;
    }
}

static Buffer *
loadcmdout(char *cmd,
	   Buffer *(*loadproc) (URLFile *, Buffer *), Buffer *defaultbuf)
{
    FILE *f, *popen(const char *, const char *);
    Buffer *buf;
    URLFile uf;

    if (cmd == NULL || *cmd == '\0')
	return NULL;
    f = popen(cmd, "r");
    if (f == NULL)
	return NULL;
    init_stream(&uf, SCM_UNKNOWN, newFileStream(f, (void (*)())pclose));
    buf = loadproc(&uf, defaultbuf);
    UFclose(&uf);
    if (buf == NULL)
	return NULL;
    return buf;
}

/* 
 * getshell: execute shell command and get the result into a buffer
 */
Buffer *
getshell(char *cmd)
{
    Buffer *buf;

    buf = loadcmdout(cmd, loadBuffer, NULL);
    buf->filename = cmd;
    buf->buffername = Sprintf("%s %s", SHELLBUFFERNAME,
			      conv_from_system(cmd))->ptr;
    return buf;
}

/* 
 * getpipe: execute shell command and connect pipe to the buffer
 */
Buffer *
getpipe(char *cmd)
{
    FILE *f, *popen(const char *, const char *);
    Buffer *buf;

    if (cmd == NULL || *cmd == '\0')
	return NULL;
    f = popen(cmd, "r");
    if (f == NULL)
	return NULL;
    buf = newBuffer(INIT_BUFFER_WIDTH);
    buf->pagerSource = newFileStream(f, (void (*)())pclose);
    buf->filename = cmd;
    buf->buffername = Sprintf("%s %s", PIPEBUFFERNAME,
			      conv_from_system(cmd))->ptr;
    buf->bufferprop |= BP_PIPE;
    return buf;
}

/* 
 * Open pager buffer
 */
Buffer *
openPagerBuffer(InputStream stream, Buffer *buf)
{

    if (buf == NULL)
	buf = newBuffer(INIT_BUFFER_WIDTH);
    buf->pagerSource = stream;
    buf->buffername = getenv("MAN_PN");
    if (buf->buffername == NULL)
	buf->buffername = PIPEBUFFERNAME;
    else
	buf->buffername = conv_from_system(buf->buffername);
    buf->bufferprop |= BP_PIPE;
#ifdef JP_CHARSET
    buf->document_code = DocumentCode;
#endif
    buf->currentLine = buf->firstLine;

    return buf;
}

Buffer *
openGeneralPagerBuffer(InputStream stream)
{
    Buffer *buf;
    char *t = "text/plain";
    Buffer *t_buf = NULL;
    URLFile uf;

    init_stream(&uf, SCM_UNKNOWN, stream);

#ifdef JP_CHARSET
    content_charset = '\0';
#endif
    if (SearchHeader) {
	t_buf = newBuffer(INIT_BUFFER_WIDTH);
	readHeader(&uf, t_buf, TRUE, NULL);
	t = checkContentType(t_buf);
	if (t == NULL)
	    t = "text/plain";
	if (t_buf) {
	    t_buf->topLine = t_buf->firstLine;
	    t_buf->currentLine = t_buf->lastLine;
	}
	SearchHeader = FALSE;
    }
    else if (DefaultType) {
	t = DefaultType;
	DefaultType = NULL;
    }
    if (!strcasecmp(t, "text/html")) {
	buf = loadHTMLBuffer(&uf, t_buf);
	buf->type = "text/html";
    }
    else if (is_plain_text_type(t)) {
	if (IStype(stream) != IST_ENCODED)
	    stream = newEncodedStream(stream, uf.encoding);
	buf = openPagerBuffer(stream, t_buf);
	buf->type = "text/plain";
    }
    else {
	if (doExternal(uf, "-", t, &buf, t_buf)) {
	    if (buf == NULL || buf == NO_BUFFER)
		return buf;
	}
	else {			/* unknown type is regarded as text/plain */
	    if (IStype(stream) != IST_ENCODED)
		stream = newEncodedStream(stream, uf.encoding);
	    buf = openPagerBuffer(stream, t_buf);
	    buf->type = "text/plain";
	}
    }
    buf->real_type = t;
    buf->currentURL.scheme = SCM_LOCAL;
    buf->currentURL.file = "-";
    return buf;
}

Line *
getNextPage(Buffer *buf, int plen)
{
    Line *l, *fl, *pl = buf->lastLine;
    Line *rl = NULL;
    int len, i, nlines = 0;
    int linelen = buf->linelen, trbyte = buf->trbyte;
    Str lineBuf2;
    char pre_lbuf = '\0';
    URLFile uf;
    char code;
    int squeeze_flag = 0;
#ifdef USE_ANSI_COLOR
    int check_color;
#endif

    if (buf->pagerSource == NULL)
	return NULL;

    if (fmInitialized)
	crmode();
    if (pl != NULL) {
	nlines = pl->real_linenumber;
	pre_lbuf = *(pl->lineBuf);
	if (pre_lbuf == '\0')
	    pre_lbuf = '\n';
    }

#ifdef JP_CHARSET
    code = buf->document_code;
#endif
    init_stream(&uf, SCM_UNKNOWN, NULL);
    for (i = 0; i < plen; i++) {
	lineBuf2 = StrmyISgets(buf->pagerSource);
	if (lineBuf2->length == 0) {
	    /* Assume that `cmd == buf->filename' */
	    if (buf->filename)
		buf->buffername = Sprintf("%s %s",
					  CPIPEBUFFERNAME,
					  conv_from_system(buf->filename))->
		    ptr;
	    else if (getenv("MAN_PN") == NULL)
		buf->buffername = CPIPEBUFFERNAME;
	    buf->bufferprop |= BP_CLOSE;
	    trbyte += linelen;
	    linelen = 0;
	    break;
	}
	linelen += lineBuf2->length;
	showProgress(&linelen, &trbyte);
	lineBuf2 = convertLine(&uf, lineBuf2, &code, PAGER_MODE);
	if (squeezeBlankLine) {
	    squeeze_flag = 0;
	    if (lineBuf2->ptr[0] == '\n' && pre_lbuf == '\n') {
		++nlines;
		--i;
		squeeze_flag = 1;
		continue;
	    }
	    pre_lbuf = lineBuf2->ptr[0];
	}
	++nlines;
	Strchop(lineBuf2);
	lineBuf2 = checkType(lineBuf2, propBuffer,
#ifdef USE_ANSI_COLOR
			     colorBuffer, &check_color,
#endif
			     LINELEN);
	len = lineBuf2->length;
	l = New(Line);
	l->lineBuf = lineBuf2->ptr;
	l->propBuf = NewAtom_N(Lineprop, len);
	bcopy((void *)propBuffer, (void *)l->propBuf, len * sizeof(Lineprop));
#ifdef USE_ANSI_COLOR
	if (check_color) {
	    l->colorBuf = NewAtom_N(Linecolor, len);
	    bcopy((void *)colorBuffer, (void *)l->colorBuf,
		  len * sizeof(Linecolor));
	}
	else {
	    l->colorBuf = NULL;
	}
#endif
	l->len = len;
	l->width = -1;
	l->prev = pl;
#if 0
	if (squeezeBlankLine) {
#endif
	    l->real_linenumber = nlines;
	    l->linenumber = (pl == NULL ? nlines : pl->linenumber + 1);
#if 0
	}
	else {
	    l->real_linenumber = l->linenumber = nlines;
	}
#endif
	if (pl == NULL) {
	    pl = l;
	    buf->firstLine = buf->topLine = buf->currentLine = l;
	}
	else {
	    pl->next = l;
	    pl = l;
	}
	if (rl == NULL)
	    rl = l;
	if (nlines > PagerMax) {
	    fl = buf->firstLine;
	    buf->firstLine = fl->next;
	    fl->next->prev = NULL;
	    if (buf->topLine == fl)
		buf->topLine = fl->next;
	    if (buf->currentLine == fl)
		buf->currentLine = fl->next;
	}
    }
    if (pl != NULL)
	pl->next = NULL;
    buf->lastLine = pl;
    if (rl == NULL && squeeze_flag) {
	rl = pl;
    }
    if (fmInitialized)
	term_raw();
    buf->linelen = linelen;
    buf->trbyte = trbyte;
#ifdef JP_CHARSET
    buf->document_code = code;
#endif
    return rl;
}

static void
FTPhalfclose(InputStream stream)
{
    if (IStype(stream) == IST_FILE && file_of(stream)) {
	Ftpfclose(file_of(stream));
	file_of(stream) = NULL;
    }
}

int
save2tmp(URLFile uf, char *tmpf)
{
    FILE *ff;
    int check;
    int linelen = 0, trbyte = 0;
    MySignalHandler(*prevtrap) ();
    static JMP_BUF env_bak;

    ff = fopen(tmpf, "wb");
    if (ff == NULL) {
	/* fclose(f); */
	return -1;
    }
    bcopy(AbortLoading, env_bak, sizeof(JMP_BUF));
    if (SETJMP(AbortLoading) != 0) {
	goto _end;
    }
    if (fmInitialized) {
	prevtrap = signal(SIGINT, KeyAbort);
	term_cbreak();
    }
    check = 0;
    current_content_length = 0;
#ifdef USE_NNTP
    if (uf.scheme == SCM_NEWS) {
	char c;
	while (c = UFgetc(&uf), !iseos(uf.stream)) {
	    if (c == '\n') {
		if (check == 0)
		    check++;
		else if (check == 3)
		    break;
	    }
	    else if (c == '.' && check == 1)
		check++;
	    else if (c == '\r' && check == 2)
		check++;
	    else
		check = 0;
	    putc(c, ff);
	    linelen += sizeof(c);
	    showProgress(&linelen, &trbyte);
	}
    }
    else
#endif				/* USE_NNTP */
    {
	Str buf = Strnew_size(SAVE_BUF_SIZE);
	while (UFread(&uf, buf, SAVE_BUF_SIZE)) {
	    Strfputs(buf, ff);
	    linelen += buf->length;
	    showProgress(&linelen, &trbyte);
	}
    }
  _end:
    bcopy(env_bak, AbortLoading, sizeof(JMP_BUF));
    if (fmInitialized) {
	term_raw();
	signal(SIGINT, prevtrap);
    }
    fclose(ff);
    if (uf.scheme == SCM_FTP)
	FTPhalfclose(uf.stream);
    return 0;
}

int
doExternal(URLFile uf, char *path, char *type, Buffer **bufp,
	   Buffer *defaultbuf)
{
    Str tmpf, command;
    struct mailcap *mcap;
    int stat;
    Buffer *buf = NULL;
    char *header;

    if (!(mcap = searchExtViewer(type)))
	return 0;

    tmpf = tmpfname(TMPF_DFL, NULL);

    if (mcap->nametemplate) {
	Str tmp =
	    unquote_mailcap(mcap->nametemplate, NULL, tmpf->ptr, NULL, NULL);
	if (Strncmp(tmpf, tmp, tmpf->length) == 0) {
	    tmpf = tmp;
	    goto _save;
	}
    }
    if (uf.ext && *uf.ext) {
	Strcat_charp(tmpf, uf.ext);
    }
  _save:
    if (IStype(uf.stream) != IST_ENCODED)
	uf.stream = newEncodedStream(uf.stream, uf.encoding);
    if (save2tmp(uf, tmpf->ptr) < 0)
	return 0;
    header = checkHeader(defaultbuf, "Content-Type:");
    if (header)
	header = conv_to_system(header);
    command = unquote_mailcap(mcap->viewer, type, tmpf->ptr, header, &stat);
#ifndef __EMX__
    if (!(stat & MCSTAT_REPNAME)) {
	Str tmp = Sprintf("(%s) < %s", command->ptr, shell_quote(tmpf->ptr));
	command = tmp;
    }
#endif
    if (mcap->flags & (MAILCAP_HTMLOUTPUT | MAILCAP_COPIOUSOUTPUT)) {
	if (defaultbuf == NULL)
	    defaultbuf = newBuffer(INIT_BUFFER_WIDTH);
	defaultbuf->mailcap = mcap;
    }
    if (mcap->flags & MAILCAP_HTMLOUTPUT) {
	buf = loadcmdout(command->ptr, loadHTMLBuffer, defaultbuf);
	if (buf && buf != NO_BUFFER) {
	    buf->type = "text/html";
	    buf->mailcap_source = buf->sourcefile;
	    buf->sourcefile = tmpf->ptr;
	}
    }
    else if (mcap->flags & MAILCAP_COPIOUSOUTPUT) {
	buf = loadcmdout(command->ptr, loadBuffer, defaultbuf);
	if (buf && buf != NO_BUFFER) {
	    buf->type = "text/plain";
	    buf->mailcap_source = buf->sourcefile;
	    buf->sourcefile = tmpf->ptr;
	}
    }
    else {
	if (mcap->flags & MAILCAP_NEEDSTERMINAL || !BackgroundExtViewer) {
	    fmTerm();
	    mySystem(command->ptr, 0);
	    fmInit();
	    if (Currentbuf)
		displayBuffer(Currentbuf, B_FORCE_REDRAW);
	}
	else {
	    mySystem(command->ptr, 1);
	}
	buf = NO_BUFFER;
    }
    if (buf && buf != NO_BUFFER) {
	buf->filename = path;
	if (buf->buffername == NULL || buf->buffername[0] == '\0')
	    buf->buffername = conv_from_system(lastFileName(path));
	buf->edit = mcap->edit;
	buf->mailcap = mcap;
    }
    *bufp = buf;
    pushText(fileToDelete, tmpf->ptr);
    return 1;
}

static int
_MoveFile(char *path1, char *path2)
{
    InputStream f1;
    FILE *f2;
    int is_pipe;
    int linelen = 0, trbyte = 0;
    Str buf;

    f1 = openIS(path1);
    if (f1 == NULL)
	return -1;
    if (*path2 == '|' && PermitSaveToPipe) {
	is_pipe = TRUE;
	f2 = popen(path2 + 1, "w");
    }
    else {
	is_pipe = FALSE;
	f2 = fopen(path2, "wb");
    }
    if (f2 == NULL) {
	ISclose(f1);
	return -1;
    }
    current_content_length = 0;
    buf = Strnew_size(SAVE_BUF_SIZE);
    while (ISread(f1, buf, SAVE_BUF_SIZE)) {
	Strfputs(buf, f2);
	linelen += buf->length;
	showProgress(&linelen, &trbyte);
    }
    ISclose(f1);
    if (is_pipe)
	pclose(f2);
    else
	fclose(f2);
    return 0;
}

void
doFileCopy(char *tmpf, char *defstr)
{
    Str msg;
    char filen[256];
    char *p, *q;

    if (fmInitialized) {
	p = searchKeyData();
	if (p == NULL || *p == '\0') {
	    p = inputLineHist("(Download)Save file to: ",
			      defstr, IN_COMMAND, SaveHist);
	    if (p == NULL || *p == '\0')
		return;
	    p = conv_to_system(p);
	}
	if (*p != '|' || !PermitSaveToPipe) {
	    p = expandName(p);
	    if (checkOverWrite(p) < 0)
		return;
	}
	if (checkCopyFile(tmpf, p) < 0) {
	    msg = Sprintf("Can't copy. %s and %s are identical.", tmpf, p);
	    disp_err_message(msg->ptr, FALSE);
	    return;
	}
	if (_MoveFile(tmpf, p) < 0) {
	    msg = Sprintf("Can't save to %s", p);
	    disp_err_message(msg->ptr, FALSE);
	}
    }
    else {
	q = searchKeyData();
	if (q == NULL || *q == '\0') {
	    printf("(Download)Save file to: ");
	    fflush(stdout);
	    p = fgets(filen, sizeof(filen), stdin);
	    if (p == NULL || filen[0] == '\0')
		return;
	    q = filen;
	}
	for (p = q + strlen(q) - 1; IS_SPACE(*p); p--) ;
	*(p + 1) = '\0';
	if (*q == '\0')
	    return;
	p = q;
	if (*p != '|' || !PermitSaveToPipe) {
	    p = expandName(p);
	    if (checkOverWrite(p) < 0)
		return;
	}
	if (checkCopyFile(tmpf, p) < 0) {
	    printf("Can't copy. %s and %s are identical.", tmpf, p);
	    return;
	}
	if (_MoveFile(tmpf, p) < 0) {
	    printf("Can't save to %s\n", p);
	}
    }
}

void
doFileMove(char *tmpf, char *defstr)
{
    doFileCopy(tmpf, defstr);
    unlink(tmpf);
}

void
doFileSave(URLFile uf, char *defstr)
{
    Str msg;
    char filen[256];
    char *p, *q;

    if (fmInitialized) {
	p = searchKeyData();
	if (p == NULL || *p == '\0') {
	    p = inputLineHist("(Download)Save file to: ",
			      defstr, IN_FILENAME, SaveHist);
	    if (p == NULL || *p == '\0')
		return;
	    p = conv_to_system(p);
	}
	if (checkOverWrite(p) < 0)
	    return;
	if (checkSaveFile(uf.stream, p) < 0) {
	    msg = Sprintf("Can't save. Load file and %s are identical.", p);
	    disp_err_message(msg->ptr, FALSE);
	    return;
	}
	if (save2tmp(uf, p) < 0) {
	    msg = Sprintf("Can't save to %s", p);
	    disp_err_message(msg->ptr, FALSE);
	}
    }
    else {
	q = searchKeyData();
	if (q == NULL || *q == '\0') {
	    printf("(Download)Save file to: ");
	    fflush(stdout);
	    p = fgets(filen, sizeof(filen), stdin);
	    if (p == NULL || filen[0] == '\0')
		return;
	    q = filen;
	}
	for (p = q + strlen(q) - 1; IS_SPACE(*p); p--) ;
	*(p + 1) = '\0';
	if (*q == '\0')
	    return;
	p = expandName(q);
	if (checkOverWrite(p) < 0)
	    return;
	if (checkSaveFile(uf.stream, p) < 0) {
	    printf("Can't save. Load file and %s are identical.", p);
	    return;
	}
	if (save2tmp(uf, p) < 0) {
	    printf("Can't save to %s\n", p);
	}
    }
}

int
checkCopyFile(char *path1, char *path2)
{
    struct stat st1, st2;

    if (*path2 == '|' && PermitSaveToPipe)
	return 0;
    if ((stat(path1, &st1) == 0) && (stat(path2, &st2) == 0))
	if (st1.st_ino == st2.st_ino)
	    return -1;
    return 0;
}

int
checkSaveFile(InputStream stream, char *path2)
{
    struct stat st1, st2;
    int des = ISfileno(stream);

    if (des < 0)
	return 0;
    if (*path2 == '|' && PermitSaveToPipe)
	return 0;
    if ((fstat(des, &st1) == 0) && (stat(path2, &st2) == 0))
	if (st1.st_ino == st2.st_ino)
	    return -1;
    return 0;
}

int
checkOverWrite(char *path)
{
    struct stat st;
    char buf[2];
    char *ans = NULL;

    if (stat(path, &st) < 0)
	return 0;
    if (fmInitialized) {
	ans = inputChar("File exists. Overwrite? (y or n)");
    }
    else {
	printf("File exists. Overwrite? (y or n)");
	ans = fgets(buf, 2, stdin);
    }
    if (ans != NULL && (*ans == '\0' || tolower(*ans) == 'y'))
	return 0;
    else
	return -1;
}

#ifdef __EMX__
#define GUNZIP_CMD  "gzip"
#define BUNZIP2_CMD "bzip2"
#define INFLATE_CMD  libFile("inflate.exe")
#else				/* not __EMX__ */
#define GUNZIP_CMD  "gunzip"
#define BUNZIP2_CMD "bunzip2"
#define INFLATE_CMD  libFile("inflate")
#endif				/* not __EMX__ */
#define GUNZIP_NAME  "gunzip"
#define BUNZIP2_NAME "bunzip2"
#define INFLATE_NAME "inflate"

static void
gunzip_stream(URLFile *uf)
{
    int pid1;
    int fd1[2];
    char *expand_cmd = GUNZIP_CMD;
    char *expand_name = GUNZIP_NAME;
    char *tmpf = NULL;

    if (IStype(uf->stream) != IST_ENCODED) {
	uf->stream = newEncodedStream(uf->stream, uf->encoding);
	uf->encoding = ENC_7BIT;
    }
    switch (uf->compression) {
    case CMP_COMPRESS:
    case CMP_GZIP:
	expand_cmd = GUNZIP_CMD;
	expand_name = GUNZIP_NAME;
	break;
    case CMP_BZIP2:
	expand_cmd = BUNZIP2_CMD;
	expand_name = BUNZIP2_NAME;
	break;
    case CMP_DEFLATE:
	expand_cmd = INFLATE_CMD;
	expand_name = INFLATE_NAME;
	break;
    }
    uf->compression = CMP_NOCOMPRESS;

    if (pipe(fd1) < 0) {
	UFclose(uf);
	return;
    }

    if (uf->scheme != SCM_HTTP && uf->scheme != SCM_LOCAL) {
	tmpf = tmpfname(TMPF_DFL, NULL)->ptr;
	if (save2tmp(*uf, tmpf) < 0) {
	    UFclose(uf);
	    return;
	}
#if 0
	if (uf->scheme != SCM_FTP)
#endif
	    UFclose(uf);
	uf->scheme = SCM_LOCAL;
	pushText(fileToDelete, tmpf);
    }

    flush_tty();
    /* fd1[0]: read, fd1[1]: write */
    if ((pid1 = fork()) == 0) {
	signal(SIGINT, SIG_DFL);
	close(fd1[0]);
	if (tmpf) {
#ifdef __CYGWIN__
	    int tmpfd = open(tmpf, O_RDONLY | O_BINARY);
#else
	    int tmpfd = open(tmpf, O_RDONLY);
#endif
	    if (tmpfd < 0) {
		close(fd1[1]);
		exit(1);
	    }
	    dup2(tmpfd, 0);
	}
	else {
	    /* child */
	    int pid2;
	    int fd2[2];
	    if (fmInitialized) {
		close_tty();
		fmInitialized = FALSE;
	    }
	    if (pipe(fd2) < 0) {
		close(fd1[1]);
		UFclose(uf);
		exit(1);
	    }
	    if ((pid2 = fork()) == 0) {
		/* child */
		Str buf = Strnew_size(SAVE_BUF_SIZE);
		close(fd2[0]);
		while (UFread(uf, buf, SAVE_BUF_SIZE)) {
		    if (write(fd2[1], buf->ptr, buf->length) < 0) {
			close(fd2[1]);
			exit(0);
		    }
		}
		close(fd2[1]);
		exit(0);
	    }
	    close(fd2[1]);
	    dup2(fd2[0], 0);
	}
	dup2(fd1[1], 1);
	execlp(expand_cmd, expand_name, NULL);
	exit(0);
    }
    close(fd1[1]);
    if (tmpf == NULL)
	UFclose(uf);
    uf->stream = newFileStream(fdopen(fd1[0], "rb"), (void (*)())pclose);
}

static FILE *
lessopen_stream(char *path)
{
    char *lessopen;
    FILE *fp;

    lessopen = getenv("LESSOPEN");
    if (lessopen == NULL) {
	return NULL;
    }
    if (lessopen[0] == '\0') {
	return NULL;
    }

    if (lessopen[0] == '|') {
	/* pipe mode */
	Str tmpf;
	int c;

	++lessopen;
	tmpf = Sprintf(lessopen, path);
	fp = popen(tmpf->ptr, "r");
	if (fp == NULL) {
	    return NULL;
	}
	c = getc(fp);
	if (c == EOF) {
	    fclose(fp);
	    return NULL;
	}
	ungetc(c, fp);
    }
    else {
	/* filename mode */
	/* not supported m(__)m */
	fp = NULL;
    }
    return fp;
}

#if 0
void
reloadBuffer(Buffer *buf)
{
    URLFile uf;

    if (buf->sourcefile == NULL || buf->pagerSource != NULL)
	return;
    init_stream(&uf, SCM_UNKNOWN, NULL);
    examineFile(buf->mailcap_source ? buf->mailcap_source : buf->sourcefile,
		&uf);
    if (uf.stream == NULL)
	return;
    is_redisplay = TRUE;
    buf->allLine = 0;
    buf->href = NULL;
    buf->name = NULL;
    buf->img = NULL;
    buf->formitem = NULL;
    if (!strcasecmp(buf->type, "text/html"))
	loadHTMLBuffer(&uf, buf);
    else
	loadBuffer(&uf, buf);
    UFclose(&uf);
    is_redisplay = FALSE;
}
#endif

#ifdef JP_CHARSET
static char
guess_charset(char *p)
{
    Str c = Strnew_size(strlen(p));
    if (strncasecmp(p, "x-", 2) == 0)
	p += 2;
    while (*p != '\0') {
	if (*p != '-' && *p != '_')
	    Strcat_char(c, tolower(*p));
	p++;
    }
    if (strncmp(c->ptr, "euc", 3) == 0)
	return CODE_EUC;
    if (strncmp(c->ptr, "shiftjis", 8) == 0 || strncmp(c->ptr, "sjis", 4) == 0)
	return CODE_SJIS;
    if (strncmp(c->ptr, "iso2022jp", 9) == 0 || strncmp(c->ptr, "jis", 3) == 0)
	return CODE_JIS_n;
    return CODE_ASCII;
}
#endif

static char *
guess_filename(char *file)
{
    char *p = NULL, *s;

    if (file != NULL)
	p = mybasename(file);
    if (p == NULL || *p == '\0')
	return DEF_SAVE_FILE;
    s = p;
    if (*p == '#')
	p++;
    while (*p != '\0') {
	if ((*p == '#' && *(p + 1) != '\0') || *p == '?') {
	    *p = '\0';
	    break;
	}
	p++;
    }
    return s;
}

char *
guess_save_name(Buffer *buf, char *path)
{
    if (buf && buf->document_header) {
	Str name = NULL;
	char *p, *q;
	if ((p = checkHeader(buf, "Content-Disposition:")) != NULL &&
	    (q = strcasestr(p, "filename")) != NULL &&
	    (q == p || IS_SPACE(*(q - 1)) || *(q - 1) == ';')) {
	    if (matchattr(q, "filename", 8, &name))
		return name->ptr;
	}
	if ((p = checkHeader(buf, "Content-Type:")) != NULL &&
	    (q = strcasestr(p, "name")) != NULL &&
	    (q == p || IS_SPACE(*(q - 1)) || *(q - 1) == ';')) {
	    if (matchattr(q, "name", 4, &name))
		return name->ptr;
	}
    }
    return guess_filename(path);
}

/* Local Variables:    */
/* c-basic-offset: 4   */
/* tab-width: 8        */
/* End:                */
