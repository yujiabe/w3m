/* $Id: history.h,v 1.4 2001/12/10 17:02:44 ukai Exp $ */
#ifndef HISTORY_H
#define HISTORY_H

#include "textlist.h"
#include "hash.h"

#define HIST_HASH_SIZE 127

typedef ListItem HistItem;

typedef GeneralList HistList;

typedef struct {
    HistList *list;
    HistItem *current;
    Hash_sv *hash;
} Hist;

extern Hist *newHist();
extern HistItem *unshiftHist(Hist *hist, char *ptr);
extern HistItem *pushHist(Hist *hist, char *ptr);
extern HistItem *pushHashHist(Hist *hist, char *ptr);
extern HistItem *getHashHist(Hist *hist, char *ptr);
extern char *lastHist(Hist *hist);
extern char *nextHist(Hist *hist);
extern char *prevHist(Hist *hist);

#endif				/* HISTORY_H */
