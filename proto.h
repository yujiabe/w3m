/* $Id: proto.h,v 1.27 2002/01/17 11:06:45 ukai Exp $ */
/* 
 *   This file was automatically generated by version 1.7 of cextract.
 *   Manual editing not recommended.
 *
 *   Created: Wed Feb 10 12:47:03 1999
 */
extern int main(int argc, char **argv, char **envp);
extern void nulcmd(void);
extern void pushEvent(int event, void *user_data);
extern MySignalHandler intTrap(SIGNAL_ARG);
extern void pgFore(void);
extern void pgBack(void);
extern void lup1(void);
extern void ldown1(void);
extern void ctrCsrV(void);
extern void ctrCsrH(void);
extern void rdrwSc(void);
extern void srchfor(void);
extern void isrchfor(void);
extern void srchbak(void);
extern void isrchbak(void);
extern void srchnxt(void);
extern void srchprv(void);
extern void shiftl(void);
extern void shiftr(void);
extern void col1R(void);
extern void col1L(void);
extern void setEnv(void);
extern void pipeBuf(void);
extern void pipesh(void);
extern void readsh(void);
extern void execsh(void);
extern void ldfile(void);
extern void ldhelp(void);
extern void movL(void);
extern void movL1(void);
extern void movD(void);
extern void movD1(void);
extern void movU(void);
extern void movU1(void);
extern void movR(void);
extern void movR1(void);
extern void movLW(void);
extern void movRW(void);
extern void qquitfm(void);
extern void quitfm(void);
extern void selBuf(void);
extern void susp(void);
extern void goLine(void);
extern void goLineF(void);
extern void goLineL(void);
extern void linbeg(void);
extern void linend(void);
extern void editBf(void);
extern void editScr(void);
extern void followA(void);
extern void bufferA(void);
extern void followI(void);
extern void submitForm(void);
extern void followForm(void);
extern void topA(void);
extern void lastA(void);
extern void onA(void);

extern void nextA(void);
extern void prevA(void);
extern void nextL(void);
extern void nextLU(void);
extern void nextR(void);
extern void nextRD(void);
extern void nextD(void);
extern void nextU(void);
extern void backBf(void);
extern void deletePrevBuf(void);
extern void goURL(void);
extern void ldBmark(void);
extern void adBmark(void);
extern void ldOpt(void);
extern void setOpt(void);
extern void pginfo(void);
extern void msgs(void);
extern void svA(void);
extern void svI(void);
extern void svBuf(void);
extern void svSrc(void);
extern void peekURL(void);
extern void peekIMG(void);
extern void curURL(void);
extern void vwSrc(void);
extern void reload(void);
extern void chkURL(void);
extern void chkURLBuffer(Buffer *buf);
#ifdef USE_NNTP
extern void chkNMID(void);
extern void chkNMIDBuffer(Buffer *buf);
#else
#define chkNMID nulcmd
#endif
extern void rFrame(void);
extern void extbrz(void);
extern void linkbrz(void);
extern void curlno(void);
#ifdef USE_ALARM
extern void setAlarm(void);
extern void setAlarmEvent(int sec, short status, int cmd, void *data);
#else
#define setAlarm nulcmd
#endif
extern int currentLn(Buffer *buf);
extern void tmpClearBuffer(Buffer *buf);
extern char *filename_extension(char *patch, int is_url);
#ifdef USE_EXTERNAL_URI_LOADER
extern void initURIMethods();
extern Str searchURIMethods(ParsedURL *pu);
extern void chkExternalURIBuffer(Buffer *buf);
#endif
extern void examineFile(char *path, URLFile *uf);
extern char *acceptableEncoding();
extern int dir_exist(char *path);
extern Str convertLine(URLFile *uf, Str line, char *code, int mode);
extern Buffer *loadFile(char *path);
extern Buffer *loadGeneralFile(char *path, ParsedURL *current, char *referer,
			       int flag, FormList *request);
extern int is_boundary(int, int);
extern int is_blank_line(char *line, int indent);
extern void push_render_image(Str str, int width, int limit,
			      struct html_feed_environ *h_env);
extern void flushline(struct html_feed_environ *h_env, struct readbuffer *obuf,
		      int indent, int force, int width);
extern void do_blankline(struct html_feed_environ *h_env,
			 struct readbuffer *obuf, int indent, int indent_incr,
			 int width);
extern void purgeline(struct html_feed_environ *h_env);
extern void save_fonteffect(struct html_feed_environ *h_env,
			    struct readbuffer *obuf);
extern void restore_fonteffect(struct html_feed_environ *h_env,
			       struct readbuffer *obuf);
extern Str process_img(struct parsed_tag *tag);
extern Str process_anchor(struct parsed_tag *tag, char *tagbuf);
extern Str process_input(struct parsed_tag *tag);
extern void process_select(struct parsed_tag *tag);
extern Str process_n_select(void);
extern void feed_select(char *str);
extern void process_option(void);
extern Str process_textarea(struct parsed_tag *tag, int width);
extern Str process_n_textarea(void);
extern void feed_textarea(char *str);
extern Str process_form(struct parsed_tag *tag);
extern Str process_n_form(void);
extern int HTMLtagproc1(struct parsed_tag *tag,
			struct html_feed_environ *h_env);
extern void HTMLlineproc2(Buffer *buf, TextLineList *tl);
extern void HTMLlineproc0(char *istr, struct html_feed_environ *h_env,
			  int internal);
#define HTMLlineproc1(x,y) HTMLlineproc0(x,y,TRUE)
extern Buffer *loadHTMLBuffer(URLFile *f, Buffer *newBuf);
extern void showProgress(int *linelen, int *trbyte);
extern void init_henv(struct html_feed_environ *, struct readbuffer *,
		      struct environment *, int, TextLineList *, int, int);
extern void completeHTMLstream(struct html_feed_environ *,
			       struct readbuffer *);
extern void loadHTMLstream(URLFile *f, Buffer *newBuf, FILE * src,
			   int internal);
extern Buffer *loadHTMLString(Str page);
#ifdef USE_GOPHER
extern Buffer *loadGopherDir(URLFile *uf, Buffer *newBuf);
#endif				/* USE_GOPHER */
extern Buffer *loadBuffer(URLFile *uf, Buffer *newBuf);
extern void saveBuffer(Buffer *buf, FILE * f);
extern void saveBufferDelNum(Buffer *buf, FILE * f, int del);
extern Buffer *getshell(char *cmd);
extern Buffer *getpipe(char *cmd);
extern Buffer *openPagerBuffer(InputStream stream, Buffer *buf);
extern Buffer *openGeneralPagerBuffer(InputStream stream);
extern Line *getNextPage(Buffer *buf, int plen);
extern int save2tmp(URLFile uf, char *tmpf);
extern int doExternal(URLFile uf, char *path, char *type, Buffer **bufp,
		      Buffer *defaultbuf);
extern void doFileCopy(char *tmpf, char *defstr);
extern void doFileMove(char *tmpf, char *defstr);
extern void doFileSave(URLFile uf, char *defstr);
extern int checkCopyFile(char *path1, char *path2);
extern int checkSaveFile(InputStream stream, char *path);
extern int checkOverWrite(char *path);
extern int matchattr(char *p, char *attr, int len, Str *value);
extern char *checkHeader(Buffer *buf, char *field);
extern Buffer *newBuffer(int width);
extern Buffer *nullBuffer(void);
extern void clearBuffer(Buffer *buf);
extern void discardBuffer(Buffer *buf);
extern Buffer *namedBuffer(Buffer *first, char *name);
extern Buffer *deleteBuffer(Buffer *first, Buffer *delbuf);
extern Buffer *replaceBuffer(Buffer *first, Buffer *delbuf, Buffer *newbuf);
extern Buffer *nthBuffer(Buffer *firstbuf, int n);
extern void gotoRealLine(Buffer *buf, int n);
extern void gotoLine(Buffer *buf, int n);
extern Buffer *selectBuffer(Buffer *firstbuf, Buffer *currentbuf,
			    char *selectchar);
extern void reshapeBuffer(Buffer *buf);
extern void copyBuffer(Buffer *a, Buffer *b);
extern Buffer *prevBuffer(Buffer *first, Buffer *buf);
extern int writeBufferCache(Buffer *buf);
extern int readBufferCache(Buffer *buf);
extern void fmTerm(void);
extern void fmInit(void);
extern void displayBuffer(Buffer *buf, int mode);
extern void redrawBuffer(Buffer *buf);
extern void redrawNLine(Buffer *buf, int n);
extern Line *redrawLine(Buffer *buf, Line *l, int i);
extern int redrawLineRegion(Buffer *buf, Line *l, int i, int bpos, int epos);
extern void do_effects(Lineprop m);
#ifdef USE_ANSI_COLOR
extern void do_color(Linecolor c);
#endif
extern void addChar(char c, Lineprop mode);
extern GeneralList *message_list;
extern void record_err_message(char *s);
extern Buffer *message_list_panel(void);
extern void message(char *s, int return_x, int return_y);
#define disp_err_message(s, f) (record_err_message((s)), disp_message((s), (f)))
extern void disp_message_nsec(char *s, int redraw_current, int sec, int purge,
			      int mouse);
extern void disp_message(char *s, int redraw_current);
#ifdef USE_MOUSE
extern void disp_message_nomouse(char *s, int redraw_current);
#else
#define disp_message_nomouse disp_message
#endif
extern void set_delayed_message(char *s);
extern void cursorUp(Buffer *buf, int n);
extern void cursorDown(Buffer *buf, int n);
extern void cursorUpDown(Buffer *buf, int n);
extern void cursorRight(Buffer *buf, int n);
extern void cursorLeft(Buffer *buf, int n);
extern void cursorHome(Buffer *buf);
extern void arrangeCursor(Buffer *buf);
extern void arrangeLine(Buffer *buf);
extern void cursorXY(Buffer *buf, int x, int y);
extern void restorePosition(Buffer *buf, Buffer *orig);
extern int columnSkip(Buffer *buf, int offset);
extern int columnPos(Line *line, int column);
extern Line *lineSkip(Buffer *buf, Line *line, int offset, int last);
extern Line *currentLineSkip(Buffer *buf, Line *line, int offset, int last);
extern int gethtmlcmd(char **s);
extern Str checkType(Str s, Lineprop *oprop,
#ifdef USE_ANSI_COLOR
		     Linecolor *ocolor, int *check_color,
#endif
		     int len);
extern int calcPosition(char *l, Lineprop *pr, int len, int pos, int bpos,
			int mode);
extern char *lastFileName(char *path);
extern char *mybasename(char *s);
extern char *mydirname(char *s);
extern int next_status(char c, int *status);
extern int read_token(Str buf, char **instr, int *status, int pre, int append);
extern Str correct_irrtag(int status);
#ifdef USE_MIGEMO
extern void init_migemo(void);
#endif
extern int forwardSearch(Buffer *buf, char *str);
extern int backwardSearch(Buffer *buf, char *str);
extern void pcmap(void);
extern void escmap(void);
extern void escbmap(void);
extern void escdmap(char c);
extern char *inputLineHistSearch(char *prompt, char *def_str, int flag,
				 Hist *hist, int (*incfunc) (int ch, Str buf));
#ifdef USE_HISTORY
extern Buffer *historyBuffer(Hist *hist);
extern void loadHistory(Hist *hist);
extern void saveHistory(Hist *hist, size_t size);
extern void ldHist(void);
#else				/* not USE_HISTORY */
#define ldHist nulcmd
#endif				/* not USE_HISTORY */
extern double log_like(int x);
extern struct table *newTable(void);
extern void pushdata(struct table *t, int row, int col, char *data);
extern int visible_length(char *str);
extern void align(TextLine *lbuf, int width, int mode);
extern void print_item(struct table *t, int row, int col, int width, Str buf);
extern void print_sep(struct table *t, int row, int type, int maxcol, Str buf);
extern void do_refill(struct table *tbl, int row, int col, int maxlimit);
extern void renderTable(struct table *t, int max_width,
			struct html_feed_environ *h_env);
extern struct table *begin_table(int border, int spacing, int padding,
				 int vspace);
extern void end_table(struct table *tbl);
extern void check_rowcol(struct table *tbl, struct table_mode *mode);
extern int minimum_length(char *line);
extern int feed_table(struct table *tbl, char *line, struct table_mode *mode,
		      int width, int internal);
extern void feed_table1(struct table *tbl, Str tok, struct table_mode *mode,
			int width);
extern void pushTable(struct table *, struct table *);
extern struct form_list *newFormList(char *action, char *method, char *charset,
				     char *enctype, char *target, char *name,
				     struct form_list *_next);
extern struct form_item_list *formList_addInput(struct form_list *fl,
						struct parsed_tag *tag);
extern char *form2str(FormItemList *fi);
extern int formtype(char *typestr);
extern void formRecheckRadio(Anchor *a, Buffer *buf, FormItemList *form);
extern void formResetBuffer(Buffer *buf, AnchorList *formitem);
extern void formUpdateBuffer(Anchor *a, Buffer *buf, FormItemList *form);
extern Str textfieldrep(Str s, int width);
extern void input_textarea(FormItemList *fi);
extern void do_internal(char *action, char *data);
extern void form_write_data(FILE * f, char *boundary, char *name, char *value);
extern void form_write_from_file(FILE * f, char *boundary, char *name,
				 char *filename, char *file);
extern void follow_map(struct parsed_tagarg *arg);
#ifdef MENU_MAP
extern char *follow_map_menu(Buffer *buf, struct parsed_tagarg *arg, int x,
			     int y);
#else
extern Buffer *follow_map_panel(Buffer *buf, struct parsed_tagarg *arg);
#endif
extern Buffer *page_info_panel(Buffer *buf);
extern struct frame_body *newFrame(struct parsed_tag *tag, Buffer *buf);
extern struct frameset *newFrameSet(struct parsed_tag *tag);
extern void addFrameSetElement(struct frameset *f,
			       union frameset_element element);
extern void deleteFrame(struct frame_body *b);
extern void deleteFrameSet(struct frameset *f);
extern void deleteFrameSetElement(union frameset_element e);
extern struct frameset *copyFrameSet(struct frameset *of);
extern void pushFrameTree(struct frameset_queue **fqpp, struct frameset *fs,
			  Buffer *buf);
extern struct frameset *popFrameTree(struct frameset_queue **fqpp);
extern void resetFrameElement(union frameset_element *f_element, Buffer *buf,
			      char *referer, FormList *request);
extern Buffer *renderFrame(Buffer *Cbuf, int force_reload);
extern union frameset_element *search_frame(struct frameset *fset, char *name);
extern int set_tty(void);
extern void set_cc(int spec, int val);
extern void close_tty(void);
extern void reset_tty(void);
extern MySignalHandler reset_exit(SIGNAL_ARG);
extern MySignalHandler error_dump(SIGNAL_ARG);
extern void set_int(void);
extern void getTCstr(void);
extern void setlinescols(void);
extern void setupscreen(void);
extern int initscr(void);
extern int write1(char c);
extern void endline(void);
extern void switch_ascii(FILE * f);
extern void switch_wchar(FILE * f);
extern void putchars(unsigned char c1, unsigned char c2, FILE * f);
extern void move(int line, int column);
extern void addch(char c);
extern void wrap(void);
extern void touch_line(void);
extern void standout(void);
extern void standend(void);
extern void bold(void);
extern void boldend(void);
extern void underline(void);
extern void underlineend(void);
extern void graphstart(void);
extern void graphend(void);
extern int graph_ok(void);
#ifdef USE_COLOR
extern void setfcolor(int color);
#ifdef USE_BG_COLOR
extern void setbcolor(int color);
#endif				/* USE_BG_COLOR */
#endif				/* USE_COLOR */
extern void refresh(void);
extern void clear(void);
extern void scroll(int);
extern void rscroll(int);
extern void need_clrtoeol(void);
extern void clrtoeol(void);
extern void clrtoeolx(void);
extern void clrtobot(void);
extern void clrtobotx(void);
extern void no_clrtoeol(void);
extern void addstr(char *s);
extern void addnstr(char *s, int n);
extern void addnstr_sup(char *s, int n);
extern void crmode(void);
extern void nocrmode(void);
extern void term_echo(void);
extern void term_noecho(void);
extern void term_raw(void);
extern void term_cooked(void);
extern void term_cbreak(void);
extern void flush_tty(void);
extern void toggle_stand(void);
extern char getch(void);
extern void bell(void);
extern void sleep_till_anykey(int sec, int purge);
#ifdef JP_CHARSET
extern char *GetSICode(char key);
extern char *GetSOCode(char key);
extern Str conv_str(Str is, char fc, char tc);
extern Str conv(char *is, char fc, char tc);
extern char checkShiftCode(Str buf, unsigned char hint);
extern char str_to_code(char *str);
extern char *code_to_str(char code);
extern void put_sjis(Str os, unsigned char ub, unsigned char lb);
#endif				/* JP_CHARSET */
extern void initMimeTypes();
extern void free_ssl_ctx();
extern ParsedURL *baseURL(Buffer *buf);
extern int openSocket(char *hostname, char *remoteport_name,
		      unsigned short remoteport_num);
extern void parseURL(char *url, ParsedURL *p_url, ParsedURL *current);
extern void copyParsedURL(ParsedURL *p, ParsedURL *q);
extern void parseURL2(char *url, ParsedURL *pu, ParsedURL *current);
extern Str parsedURL2Str(ParsedURL *pu);
extern int getURLScheme(char **url);
extern void init_stream(URLFile *uf, int scheme, InputStream stream);
extern URLFile openURL(char *url, ParsedURL *pu, ParsedURL *current,
		       URLOption *option, FormList *request,
		       TextList *extra_header, URLFile *ouf,
		       unsigned char *status);
extern int mailcapMatch(struct mailcap *mcap, char *type);
extern struct mailcap *searchMailcap(struct mailcap *table, char *type);
extern void initMailcap();
extern char *acceptableMimeTypes();
extern struct mailcap *searchExtViewer(char *type);
extern Str unquote_mailcap(char *qstr, char *type, char *name, char *attr,
			   int *mc_stat);
extern char *guessContentType(char *filename);
extern TextList *make_domain_list(char *domain_list);
extern int check_no_proxy(char *domain);
extern FILE *openFTP(ParsedURL *pu);
extern Str readFTPDir(ParsedURL *pu);
extern void closeFTP(FILE * f);
extern int Ftpfclose(FILE * f);
extern AnchorList *putAnchor(AnchorList *al, char *url, char *target,
			     Anchor **anchor_return, char *referer, int line,
			     int pos);
extern Anchor *registerHref(Buffer *buf, char *url, char *target,
			    char *referer, int line, int pos);
extern Anchor *registerName(Buffer *buf, char *url, int line, int pos);
extern Anchor *registerImg(Buffer *buf, char *url, int line, int pos);
extern Anchor *registerForm(Buffer *buf, FormList *flist,
			    struct parsed_tag *tag, int line, int pos);
extern int onAnchor(Anchor *a, int line, int pos);
extern Anchor *retrieveAnchor(AnchorList *al, int line, int pos);
extern Anchor *retrieveCurrentAnchor(Buffer *buf);
extern Anchor *retrieveCurrentImg(Buffer *buf);
extern Anchor *retrieveCurrentForm(Buffer *buf);
extern Anchor *searchAnchor(AnchorList *al, char *str);
extern Anchor *searchURLLabel(Buffer *buf, char *url);
extern char *reAnchor(Buffer *buf, char *re);
#ifdef USE_NNTP
extern char *reAnchorNews(Buffer *buf, char *re);
#endif				/* USE_NNTP */
extern void addMultirowsForm(Buffer *buf, AnchorList *al);
extern Anchor *closest_next_anchor(AnchorList *a, Anchor *an, int x, int y);
extern Anchor *closest_prev_anchor(AnchorList *a, Anchor *an, int x, int y);
extern HmarkerList *putHmarker(HmarkerList *ml, int line, int pos, int seq);
extern Str decodeB(char **ww);
extern Str decodeQ(char **ww);
extern Str decodeQP(char **ww);
extern Str decodeU(char **ww);
extern Str decodeWord(char **ow);
extern Str decodeMIME(char *orgstr);
extern Str encodeB(char *a);
extern int set_param_option(char *option);
extern char *get_param_option(char *name);
extern void create_option_search_table();
extern void init_rc(char *config_filename);
extern Buffer *load_option_panel(void);
extern void panel_set_option(struct parsed_tagarg *);
extern void sync_with_option(void);
extern char *rcFile(char *base);
extern char *etcFile(char *base);
extern char *libFile(char *base);
extern char *helpFile(char *base);
extern void setLocalCookie(void);
extern Buffer *dirBuffer(char *dirname);
extern void set_environ(char *var, char *value);
extern FILE *localcgi_post(char *, char *, FormList *, char *);
extern FILE *localcgi_get(char *, char *, char *);
extern Str find_auth_cookie(char *host, int port, char *realm);
extern void add_auth_cookie(char *host, int port, char *realm, Str cookie);
extern char *last_modified(Buffer *buf);
extern Str romanNumeral(int n);
extern Str romanAlphabet(int n);
extern void mySystem(char *command, int background);
extern Str myExtCommand(char *cmd, char *arg, int redirect);
extern Str myEditor(char *cmd, char *file, int line);
extern char *file_to_url(char *file);
extern char *expandName(char *name);
extern Str tmpfname(int type, char *ext);
#ifdef USE_COOKIE
extern time_t mymktime(char *timestr);
extern char *FQDN(char *host);
extern Str find_cookie(ParsedURL *pu);
extern int add_cookie(ParsedURL *pu, Str name, Str value, time_t expires,
		      Str domain, Str path, int flag, Str comment, int version,
		      Str port, Str commentURL);
extern void save_cookies(void);
extern void load_cookies(void);
extern void initCookie(void);
extern void cooLst(void);
extern Buffer *cookie_list_panel(void);
extern void set_cookie_flag(struct parsed_tagarg *arg);
extern int check_cookie_accept_domain(char *domain);
#else				/* not USE_COOKIE */
#define cooLst nulcmd
#endif				/* not USE_COOKIE */

#ifdef USE_MARK
extern void _mark(void);
extern void nextMk(void);
extern void prevMk(void);
extern void reMark(void);
#else				/* not USE_MARK */
#define _mark  nulcmd
#define nextMk nulcmd
#define prevMk nulcmd
#define reMark nulcmd
#endif				/* not USE_MARK */

#ifdef USE_MOUSE
extern void mouse(void);
extern void mouse_init(void);
extern void mouse_end(void);
extern void mouse_active(void);
extern void mouse_inactive(void);
extern void msToggle(void);
#else				/* not USE_MOUSE */
#define mouse nulcmd
#define msToggle nulcmd
#endif				/* not USE_MOUSE */

extern char *searchKeyData(void);

extern void initKeymap(void);
extern int getFuncList(char *id);
extern int getKey(char *s);
extern char *getKeyData(int key);
extern char *getWord(char **str);
extern char *getQWord(char **str);

#ifdef USE_MENU
extern void new_menu(Menu *menu, MenuItem *item);
extern void geom_menu(Menu *menu, int x, int y, int mselect);
extern void draw_all_menu(Menu *menu);
extern void draw_menu(Menu *menu);
extern void draw_menu_item(Menu *menu, int mselect);
extern int select_menu(Menu *menu, int mselect);
extern void goto_menu(Menu *menu, int mselect, int down);
extern void up_menu(Menu *menu, int n);
extern void down_menu(Menu *menu, int n);
extern int action_menu(Menu *menu);
extern void popup_menu(Menu *parent, Menu *menu);
extern void guess_menu_xy(Menu *menu, int width, int *x, int *y);
extern void new_option_menu(Menu *menu, char **label, int *variable,
			    void (*func) ());

extern int setMenuItem(MenuItem *item, char *type, char *line);
extern int addMenuList(MenuList **list, char *id);
extern int getMenuN(MenuList *list, char *id);

extern void popupMenu(int x, int y, Menu *menu);
extern void mainMenu(int x, int y);
extern void mainMn(void);
extern void selMn(void);
extern void optionMenu(int x, int y, char **label, int *variable, int initial,
		       void (*func) ());
extern void initMenu(void);
#else				/* not USE_MENU */
#define mainMn nulcmd
#define selMn selBuf
#endif				/* not USE_MENU */

#ifdef USE_DICT
extern void dictword(void);
extern void dictwordat(void);
#else				/* not USE_DICT */
#define dictword nulcmd
#define dictwordat nulcmd
#endif				/* not USE_DICT */

extern void reloadBuffer(Buffer *buf);

extern char *guess_save_name(Buffer *buf, char *file);

extern void wrapToggle(void);
extern void saveBufferInfo(void);

extern void dispVer(void);

#ifdef USE_INCLUDED_SRAND48
void srand48(long);
long lrand48(void);
#endif

#include "indep.h"
