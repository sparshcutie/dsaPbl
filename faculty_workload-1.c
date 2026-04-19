/*
    Faculty Workload Management System
    DSA-I PBL Project
    NIET Greater Noida
    Sem 2

    Data structures used:
    - struct arrays for faculty and subjects
    - linked list for log
    - hash map (linear probing) for specialisation lookup
    - min heap for auto assignment
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define RED    "\033[1;31m"
#define YEL    "\033[1;33m"
#define GRN    "\033[1;32m"
#define CYN    "\033[1;36m"
#define WHT    "\033[1;37m"
#define DIM    "\033[2m"
#define RST    "\033[0m"
#define BGRED  "\033[41m"
#define BGYEL  "\033[43m"
#define BGGRN  "\033[42m"

#define MAX_FAC   50
#define MAX_SUB   100
#define MAX_ASSGN 20
#define NLEN      60
#define SLEN      50
#define CLEN      15

// if hours > this = overloaded (red)
#define OVER_LIMIT  20
// if hours < this = underloaded (yellow)
#define UNDER_LIMIT 12

// log node - singly linked list
typedef struct LogNode {
    char time[30];
    char fid[10];
    char code[CLEN];
    char action[20];
    int prev;
    int curr;
    struct LogNode *next;
} LogNode;

typedef struct {
    char fid[10];
    char name[NLEN];
    char desig[30];
    char spec[SLEN];
    char qual[15];
    int maxhrs;
    int curhrs;
    int subcnt;
    char subs[MAX_ASSGN][CLEN];
    int alive; // 1 means exists
} Faculty;

typedef struct {
    char code[CLEN];
    char title[80];
    char type[10];
    int hrs;
    char spec[SLEN];
    char facid[10]; // blank if not assigned
    int alive;
} Subject;

// min heap node for greedy distribution
typedef struct {
    int idx;
    int hrs;
} HNode;

// globals
Faculty fac[MAX_FAC];
int     fcnt = 0;

Subject sub[MAX_SUB];
int     scnt = 0;

LogNode *loghead = NULL;
LogNode *logtail = NULL;
int      logcnt  = 0;

// hash map: spec string -> list of fac indices
#define HM 128
typedef struct {
    char key[SLEN];
    int  ids[MAX_FAC];
    int  cnt;
    int  used;
} HEntry;
HEntry hmap[HM];

// min heap array
HNode heap[MAX_FAC];
int   hsz = 0;

// ------- small helpers --------

void cls() { printf("\033[2J\033[H"); }

// strips newline, reads string safely
void getstr(const char *msg, char *buf, int maxn) {
    printf("%s", msg);
    fflush(stdout);
    if (fgets(buf, maxn, stdin)) {
        int n = strlen(buf);
        if (n > 0 && buf[n-1] == '\n') buf[n-1] = '\0';
    }
}

int getint(const char *msg) {
    char tmp[20];
    getstr(msg, tmp, 20);
    return atoi(tmp);
}

void waitkey() {
    printf(DIM "\n  (enter to go back)" RST);
    while (getchar() != '\n');
    getchar();
}

void timestamp(char *buf) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    strftime(buf, 30, "%d-%m-%Y %H:%M", tm);
}

// make first letter capital for each word
void titlecase(char *s) {
    int nw = 1;
    for (int i = 0; s[i]; i++) {
        if (s[i] == ' ') { nw = 1; continue; }
        if (nw && s[i] >= 'a' && s[i] <= 'z') s[i] -= 32;
        nw = 0;
    }
}

void addlog(const char *fid, const char *code, const char *act, int p, int c) {
    LogNode *n = malloc(sizeof(LogNode));
    if (!n) return;
    timestamp(n->time);
    strncpy(n->fid,    fid,  9);
    strncpy(n->code,   code, CLEN-1);
    strncpy(n->action, act,  19);
    n->prev = p; n->curr = c;
    n->next = NULL;
    if (!loghead) loghead = logtail = n;
    else { logtail->next = n; logtail = n; }
    logcnt++;
}

int findfac(const char *id) {
    for (int i = 0; i < fcnt; i++)
        if (fac[i].alive && strcmp(fac[i].fid, id) == 0) return i;
    return -1;
}

int findsub(const char *code) {
    for (int i = 0; i < scnt; i++)
        if (sub[i].alive && strcmp(sub[i].code, code) == 0) return i;
    return -1;
}

void makeid(char *buf) {
    sprintf(buf, "F%03d", fcnt + 1);
}

const char *getcolor(Faculty *f) {
    if (f->curhrs > OVER_LIMIT)  return RED;
    if (f->curhrs < UNDER_LIMIT) return YEL;
    return GRN;
}

const char *getstatus(Faculty *f) {
    if (f->curhrs > OVER_LIMIT)  return "OVER";
    if (f->curhrs < UNDER_LIMIT) return "LOW";
    return "OK";
}

// ------- hash map --------

unsigned int hfn(const char *k) {
    unsigned int h = 5381;
    while (*k) h = ((h << 5) + h) + (unsigned char)(*k++);
    return h % HM;
}

void hmadd(const char *spec, int fi) {
    unsigned int h = hfn(spec);
    while (hmap[h].used && strcmp(hmap[h].key, spec) != 0)
        h = (h + 1) % HM;
    if (!hmap[h].used) {
        strncpy(hmap[h].key, spec, SLEN-1);
        hmap[h].cnt  = 0;
        hmap[h].used = 1;
    }
    hmap[h].ids[hmap[h].cnt++] = fi;
}

HEntry *hmget(const char *spec) {
    unsigned int h = hfn(spec);
    for (int t = 0; t < HM; t++) {
        if (!hmap[h].used) return NULL;
        if (strcmp(hmap[h].key, spec) == 0) return &hmap[h];
        h = (h + 1) % HM;
    }
    return NULL;
}

void rebuildmap() {
    memset(hmap, 0, sizeof(hmap));
    for (int i = 0; i < fcnt; i++)
        if (fac[i].alive) hmadd(fac[i].spec, i);
}

// ------- min heap --------

void hswap(int a, int b) { HNode t = heap[a]; heap[a] = heap[b]; heap[b] = t; }

void hup(int i) {
    while (i > 0) {
        int p = (i-1)/2;
        if (heap[p].hrs > heap[i].hrs) { hswap(p, i); i = p; }
        else break;
    }
}

void hdown(int i) {
    while (1) {
        int l = 2*i+1, r = 2*i+2, sm = i;
        if (l < hsz && heap[l].hrs < heap[sm].hrs) sm = l;
        if (r < hsz && heap[r].hrs < heap[sm].hrs) sm = r;
        if (sm == i) break;
        hswap(i, sm); i = sm;
    }
}

void hpush(int fi) {
    heap[hsz].idx = fi;
    heap[hsz].hrs = fac[fi].curhrs;
    hup(hsz++);
}

HNode hpop() {
    HNode mn = heap[0];
    heap[0] = heap[--hsz];
    hdown(0);
    return mn;
}

void buildheap() {
    hsz = 0;
    for (int i = 0; i < fcnt; i++)
        if (fac[i].alive) hpush(i);
}

// ------- header --------

void header(const char *title) {
    cls();
    printf("\n" CYN "  =============================================\n");
    printf("    Faculty Workload Management System\n");
    printf("    NIET Greater Noida | DSA-I PBL\n");
    printf("  =============================================\n" RST);
    printf(WHT "  %s\n" RST, title);
    printf(DIM "  ---------------------------------------------\n" RST);
}

// ======================================================
//   FEATURES
// ======================================================

void add_faculty() {
    header("Add New Faculty");

    if (fcnt >= MAX_FAC) {
        printf(RED "  max limit reached\n" RST);
        waitkey(); return;
    }

    Faculty f;
    memset(&f, 0, sizeof(f));
    makeid(f.fid);
    printf(GRN "  Faculty ID will be: %s\n\n" RST, f.fid);

    getstr("  Name              : ", f.name, NLEN);
    titlecase(f.name);
    if (strlen(f.name) == 0) {
        printf(RED "  name cant be empty\n" RST);
        waitkey(); return;
    }

    printf("  Designation:\n");
    printf("    1. Professor\n    2. Associate Professor\n    3. Assistant Professor\n");
    switch (getint("  choice: ")) {
        case 1: strcpy(f.desig, "Professor"); break;
        case 2: strcpy(f.desig, "Assoc Prof"); break;
        default: strcpy(f.desig, "Asst Prof"); break;
    }

    getstr("  Specialisation    : ", f.spec, SLEN);
    titlecase(f.spec);

    printf("  Qualification:\n    1. PhD  2. MTech  3. MSc  4. BE/BTech\n");
    switch (getint("  choice: ")) {
        case 1: strcpy(f.qual, "PhD"); break;
        case 2: strcpy(f.qual, "MTech"); break;
        case 3: strcpy(f.qual, "MSc"); break;
        default: strcpy(f.qual, "BE/BTech"); break;
    }

    f.maxhrs = getint("  Max weekly hrs    : ");
    if (f.maxhrs <= 0) f.maxhrs = 18;
    f.curhrs = 0;
    f.subcnt = 0;
    f.alive  = 1;

    fac[fcnt++] = f;
    rebuildmap();

    printf(GRN "\n  done! %s added.\n" RST, f.name);
    addlog(f.fid, "-", "ADDED", 0, 0);
    waitkey();
}

void view_faculty() {
    header("All Faculty");

    if (fcnt == 0) {
        printf("  no faculty added yet\n");
        waitkey(); return;
    }

    printf(WHT "  %-6s %-22s %-13s %-13s  %4s/%-4s  STATUS\n" RST,
           "ID", "Name", "Designation", "Specialisation", "Cur", "Max");
    printf(DIM "  -----------------------------------------------------------------------\n" RST);

    for (int i = 0; i < fcnt; i++) {
        Faculty *f = &fac[i];
        if (!f->alive) continue;

        const char *col = getcolor(f);
        const char *st  = getstatus(f);

        printf("  %s%-6s %-22s %-13s %-13s  %3dh/%-3dh  " RST,
               col, f->fid, f->name, f->desig, f->spec,
               f->curhrs, f->maxhrs);

        if (strcmp(st, "OVER") == 0)
            printf(BGRED WHT " OVER " RST);
        else if (strcmp(st, "LOW") == 0)
            printf(BGYEL "\033[30m" " LOW  " RST);
        else
            printf(BGGRN "\033[30m" "  OK  " RST);

        printf("\n");
    }

    printf(DIM "  -----------------------------------------------------------------------\n" RST);
    printf(RED  "  red = overloaded (>%dh)\n" RST, OVER_LIMIT);
    printf(YEL  "  yellow = underloaded (<%dh)\n" RST, UNDER_LIMIT);
    printf(GRN  "  green = fine\n" RST);
    waitkey();
}

void add_subject() {
    header("Add Subject");

    if (scnt >= MAX_SUB) {
        printf(RED "  subject limit reached\n" RST); waitkey(); return;
    }

    Subject s;
    memset(&s, 0, sizeof(s));

    getstr("  Course Code (eg CS301) : ", s.code, CLEN);
    if (strlen(s.code) == 0) {
        printf(RED "  code cant be empty\n" RST); waitkey(); return;
    }
    if (findsub(s.code) != -1) {
        printf(RED "  this code already exists\n" RST); waitkey(); return;
    }

    getstr("  Subject Title         : ", s.title, 80);
    titlecase(s.title);

    printf("  Type:  1. Theory  2. Lab\n");
    strcpy(s.type, getint("  choice: ") == 2 ? "Lab" : "Theory");

    s.hrs = getint("  Weekly hours          : ");
    if (s.hrs <= 0) s.hrs = 3;

    getstr("  Required Specialisation: ", s.spec, SLEN);
    titlecase(s.spec);

    s.facid[0] = '\0';
    s.alive = 1;
    sub[scnt++] = s;

    printf(GRN "\n  added %s - %s\n" RST, s.code, s.title);
    addlog("SYS", s.code, "SUB_ADDED", 0, 0);
    waitkey();
}

void view_subjects() {
    header("All Subjects");

    if (scnt == 0) {
        printf("  no subjects added yet\n"); waitkey(); return;
    }

    printf(WHT "  %-8s %-30s %-7s %4s  %-13s  %-10s\n" RST,
           "Code", "Title", "Type", "Hrs", "Specialisation", "Assigned");
    printf(DIM "  ---------------------------------------------------------------------------\n" RST);

    for (int i = 0; i < scnt; i++) {
        Subject *s = &sub[i];
        if (!s->alive) continue;
        int asgn = strlen(s->facid) > 0;
        printf("  %-8s %-30s %-7s %3dh  %-13s  ",
               s->code, s->title, s->type, s->hrs, s->spec);
        if (asgn) printf(GRN "%-10s" RST, s->facid);
        else       printf(YEL "%-10s" RST, "not assigned");
        printf("\n");
    }
    printf(DIM "  ---------------------------------------------------------------------------\n" RST);
    waitkey();
}

void manual_assign() {
    header("Assign Subject to Faculty");

    char fid[10], code[CLEN];
    getstr("  Faculty ID   : ", fid, 10);
    int fi = findfac(fid);
    if (fi == -1) { printf(RED "  faculty not found\n" RST); waitkey(); return; }

    getstr("  Subject Code : ", code, CLEN);
    int si = findsub(code);
    if (si == -1) { printf(RED "  subject not found\n" RST); waitkey(); return; }

    Faculty *f = &fac[fi];
    Subject *s = &sub[si];

    if (strlen(s->facid) > 0) {
        printf(YEL "  this subject is already with %s, reassign? (y/n): " RST, s->facid);
        char ch[4]; getstr("", ch, 4);
        if (ch[0] != 'y' && ch[0] != 'Y') { waitkey(); return; }

        int pfi = findfac(s->facid);
        if (pfi != -1) {
            fac[pfi].curhrs -= s->hrs;
            if (fac[pfi].curhrs < 0) fac[pfi].curhrs = 0;
            for (int k = 0; k < fac[pfi].subcnt; k++) {
                if (strcmp(fac[pfi].subs[k], code) == 0) {
                    for (int m = k; m < fac[pfi].subcnt - 1; m++)
                        strcpy(fac[pfi].subs[m], fac[pfi].subs[m+1]);
                    fac[pfi].subcnt--;
                    break;
                }
            }
        }
    }

    if (f->curhrs + s->hrs > f->maxhrs) {
        printf(RED "  warning: %s will go over limit (%dh + %dh > %dh max)\n" RST,
               f->name, f->curhrs, s->hrs, f->maxhrs);
        printf("  still assign? (y/n): ");
        char ch[4]; getstr("", ch, 4);
        if (ch[0] != 'y' && ch[0] != 'Y') { waitkey(); return; }
    }

    int prev = f->curhrs;
    f->curhrs += s->hrs;
    if (f->subcnt < MAX_ASSGN) strcpy(f->subs[f->subcnt++], code);
    strncpy(s->facid, f->fid, 9);
    addlog(f->fid, code, "ASSIGNED", prev, f->curhrs);

    printf(GRN "\n  assigned %s to %s\n" RST, code, f->name);
    printf("  load: %dh -> %s%dh" RST "\n", prev, getcolor(f), f->curhrs);
    waitkey();
}

void auto_assign() {
    header("Auto-Distribute Subjects (Greedy using Min Heap)");

    if (fcnt == 0) {
        printf(RED "  no faculty to assign to\n" RST); waitkey(); return;
    }

    int pending = 0;
    for (int i = 0; i < scnt; i++)
        if (sub[i].alive && strlen(sub[i].facid) == 0) pending++;

    if (pending == 0) {
        printf(GRN "  all subjects already assigned\n" RST); waitkey(); return;
    }

    printf(CYN "  distributing %d subjects...\n\n" RST, pending);

    buildheap();

    int done = 0, skip = 0;

    for (int si = 0; si < scnt; si++) {
        Subject *s = &sub[si];
        if (!s->alive || strlen(s->facid) > 0) continue;

        // look up eligible faculty from hash map
        HEntry *e = hmget(s->spec);
        if (!e || e->cnt == 0) {
            printf(YEL "  no faculty with spec '%s' for %s - skipped\n" RST, s->spec, s->code);
            skip++;
            continue;
        }

        // from eligible, pick lowest load
        int best = -1, besthrs = 9999;
        for (int k = 0; k < e->cnt; k++) {
            int fi = e->ids[k];
            if (!fac[fi].alive) continue;
            if (fac[fi].curhrs < fac[fi].maxhrs && fac[fi].curhrs < besthrs) {
                besthrs = fac[fi].curhrs;
                best    = fi;
            }
        }

        if (best == -1) {
            printf(YEL "  all '%s' faculty are full for %s - skipped\n" RST, s->spec, s->code);
            skip++;
            continue;
        }

        Faculty *f = &fac[best];
        int prev = f->curhrs;
        f->curhrs += s->hrs;
        if (f->subcnt < MAX_ASSGN) strcpy(f->subs[f->subcnt++], s->code);
        strncpy(s->facid, f->fid, 9);
        addlog(f->fid, s->code, "AUTO", prev, f->curhrs);

        printf(GRN "  %s" RST " -> %-20s  %dh -> %s%dh" RST "\n",
               s->code, f->name, prev, getcolor(f), f->curhrs);
        done++;
    }

    printf(DIM "\n  ----------------------------\n" RST);
    printf("  assigned: %d   skipped: %d\n", done, skip);
    waitkey();
}

void dashboard() {
    header("Workload Dashboard");

    if (fcnt == 0) {
        printf("  no faculty yet\n"); waitkey(); return;
    }

    int over = 0, under = 0, ok = 0;

    for (int i = 0; i < fcnt; i++) {
        Faculty *f = &fac[i];
        if (!f->alive) continue;

        const char *col = getcolor(f);
        const char *st  = getstatus(f);

        // simple bar
        int bar = 28;
        int fill = f->maxhrs > 0 ? (f->curhrs * bar) / f->maxhrs : 0;
        if (fill > bar) fill = bar;

        printf("\n  %s%-22s" RST " ", col, f->name);
        printf("[");
        for (int b = 0; b < bar; b++) printf(b < fill ? "#" : ".");
        printf("] %dh/%dh ", f->curhrs, f->maxhrs);

        if (strcmp(st, "OVER") == 0) { printf(BGRED WHT "OVER" RST); over++; }
        else if (strcmp(st, "LOW") == 0) { printf(BGYEL "\033[30m" "LOW " RST); under++; }
        else { printf(BGGRN "\033[30m" " OK " RST); ok++; }

        if (f->subcnt > 0) {
            printf("\n    " DIM);
            for (int k = 0; k < f->subcnt; k++) {
                printf("%s", f->subs[k]);
                if (k < f->subcnt-1) printf(", ");
            }
            printf(RST);
        } else {
            printf("\n    " DIM "(no subjects)" RST);
        }
    }

    printf("\n\n" DIM "  ----------------------------\n" RST);
    printf("  " RED "over: %d  " RST YEL "under: %d  " RST GRN "ok: %d\n" RST,
           over, under, ok);
    waitkey();
}

void unassign_sub() {
    header("Unassign Subject");

    char code[CLEN];
    getstr("  Subject code: ", code, CLEN);
    int si = findsub(code);
    if (si == -1) { printf(RED "  not found\n" RST); waitkey(); return; }

    Subject *s = &sub[si];
    if (strlen(s->facid) == 0) {
        printf("  subject is already unassigned\n"); waitkey(); return;
    }

    int fi = findfac(s->facid);
    if (fi != -1) {
        Faculty *f = &fac[fi];
        int prev = f->curhrs;
        f->curhrs -= s->hrs;
        if (f->curhrs < 0) f->curhrs = 0;
        for (int k = 0; k < f->subcnt; k++) {
            if (strcmp(f->subs[k], code) == 0) {
                for (int m = k; m < f->subcnt - 1; m++)
                    strcpy(f->subs[m], f->subs[m+1]);
                f->subcnt--;
                break;
            }
        }
        addlog(f->fid, code, "REMOVED", prev, f->curhrs);
        printf(GRN "  removed %s from %s (%dh -> %dh)\n" RST,
               code, f->name, prev, f->curhrs);
    }
    s->facid[0] = '\0';
    waitkey();
}

void fac_detail() {
    header("Faculty Details");

    char fid[10];
    getstr("  Faculty ID: ", fid, 10);
    int fi = findfac(fid);
    if (fi == -1) { printf(RED "  not found\n" RST); waitkey(); return; }

    Faculty *f = &fac[fi];
    const char *col = getcolor(f);
    const char *st  = getstatus(f);

    printf("\n");
    printf(WHT "  Name         : " CYN "%s\n" RST, f->name);
    printf(WHT "  ID           : " RST "%s\n", f->fid);
    printf(WHT "  Designation  : " RST "%s\n", f->desig);
    printf(WHT "  Specialisation: " RST "%s\n", f->spec);
    printf(WHT "  Qualification : " RST "%s\n", f->qual);
    printf(WHT "  Max load     : " RST "%dh/week\n", f->maxhrs);
    printf(WHT "  Current load : " RST "%s%dh/week  [%s]\n" RST, col, f->curhrs, st);
    printf(WHT "  Subjects (%d):\n" RST, f->subcnt);
    for (int k = 0; k < f->subcnt; k++) {
        int si = findsub(f->subs[k]);
        if (si != -1)
            printf("    - %s  %s  (%dh)\n", sub[si].code, sub[si].title, sub[si].hrs);
        else
            printf("    - %s\n", f->subs[k]);
    }
    if (f->subcnt == 0) printf("    (none)\n");
    waitkey();
}

void delete_fac() {
    header("Delete Faculty");

    char fid[10];
    getstr("  Faculty ID: ", fid, 10);
    int fi = findfac(fid);
    if (fi == -1) { printf(RED "  not found\n" RST); waitkey(); return; }

    Faculty *f = &fac[fi];

    if (f->subcnt > 0) {
        printf(YEL "  %s has %d subject(s). unassign and delete? (y/n): " RST,
               f->name, f->subcnt);
        char ch[4]; getstr("", ch, 4);
        if (ch[0] != 'y' && ch[0] != 'Y') { waitkey(); return; }
        for (int k = 0; k < f->subcnt; k++) {
            int si = findsub(f->subs[k]);
            if (si != -1) sub[si].facid[0] = '\0';
        }
    } else {
        printf(RED "  delete %s? (y/n): " RST, f->name);
        char ch[4]; getstr("", ch, 4);
        if (ch[0] != 'y' && ch[0] != 'Y') { waitkey(); return; }
    }

    addlog(f->fid, "-", "DELETED", f->curhrs, 0);
    f->alive = 0;
    rebuildmap();
    printf(GRN "  deleted.\n" RST);
    waitkey();
}

void view_log() {
    header("Assignment Log");

    if (!loghead) {
        printf("  no entries yet\n"); waitkey(); return;
    }

    printf(WHT "  %-17s %-6s %-10s %-12s  %s\n" RST,
           "Time", "FacID", "Subject", "Action", "Hours");
    printf(DIM "  ----------------------------------------------------------\n" RST);

    LogNode *cur = loghead;
    int n = 0;
    while (cur) {
        const char *col = (strcmp(cur->action,"ASSIGNED")==0 || strcmp(cur->action,"AUTO")==0)
                          ? GRN : (strcmp(cur->action,"REMOVED")==0 ? RED : DIM);
        printf("  " DIM "%-17s" RST " %-6s %-10s %s%-12s" RST "  %d->%d\n",
               cur->time, cur->fid, cur->code, col, cur->action, cur->prev, cur->curr);
        cur = cur->next;
        n++;
        if (n % 18 == 0 && cur) {
            printf(DIM "  -- enter for more --" RST);
            getchar();
        }
    }
    printf(DIM "  ----------------------------------------------------------\n" RST);
    printf("  %d entries total\n", logcnt);
    waitkey();
}

void stats() {
    header("Statistics");

    int tf = 0, ts = 0, asgn = 0, thrs = 0, ov = 0, un = 0, ok = 0;
    for (int i = 0; i < fcnt; i++) {
        if (!fac[i].alive) continue;
        tf++;
        thrs += fac[i].curhrs;
        if (fac[i].curhrs > OVER_LIMIT)  ov++;
        else if (fac[i].curhrs < UNDER_LIMIT) un++;
        else ok++;
    }
    for (int i = 0; i < scnt; i++) {
        if (!sub[i].alive) continue;
        ts++;
        if (strlen(sub[i].facid) > 0) asgn++;
    }

    printf("\n");
    printf("  Faculty\n");
    printf("  --------\n");
    printf("  total         : %d\n", tf);
    printf("  " RED "overloaded    : %d\n" RST, ov);
    printf("  " YEL "underloaded   : %d\n" RST, un);
    printf("  " GRN "normal        : %d\n" RST, ok);
    printf("  avg load      : %.1fh/week\n", tf > 0 ? (float)thrs/tf : 0.0f);
    printf("\n  Subjects\n");
    printf("  --------\n");
    printf("  total         : %d\n", ts);
    printf("  " GRN "assigned      : %d\n" RST, asgn);
    printf("  " YEL "unassigned    : %d\n" RST, ts - asgn);
    printf("  log entries   : %d\n", logcnt);
    waitkey();
}

// sample data so the program isn't empty on first run
void loadsamples() {
    struct { char *n, *d, *sp, *q; int mx; } fd[] = {
        {"Dr. Priya Sharma",  "Professor",  "Dsa",      "PhD",    20},
        {"Dr. Amit Verma",    "Assoc Prof", "Networks", "PhD",    18},
        {"Ms. Neha Gupta",    "Asst Prof",  "Dbms",     "MTech",  16},
        {"Mr. Rahul Singh",   "Asst Prof",  "Dsa",      "MTech",  16},
        {"Dr. Sunita Patel",  "Professor",  "Ai",       "PhD",    20},
        {"Mr. Arjun Mehta",   "Asst Prof",  "Os",       "MTech",  16},
        {"Ms. Pooja Yadav",   "Asst Prof",  "Maths",    "MSc",    14},
    };
    for (int i = 0; i < 7; i++) {
        Faculty f; memset(&f, 0, sizeof(f));
        makeid(f.fid);
        strcpy(f.name,  fd[i].n);
        strcpy(f.desig, fd[i].d);
        strcpy(f.spec,  fd[i].sp);
        strcpy(f.qual,  fd[i].q);
        f.maxhrs = fd[i].mx;
        f.alive  = 1;
        fac[fcnt++] = f;
    }
    rebuildmap();

    struct { char *co, *ti, *ty, *sp; int h; } sd[] = {
        {"CS201", "Data Structures",         "Theory", "Dsa",      3},
        {"CS202", "DSA Lab",                 "Lab",    "Dsa",      2},
        {"CS301", "Computer Networks",       "Theory", "Networks", 3},
        {"CS302", "Networks Lab",            "Lab",    "Networks", 2},
        {"CS401", "Database Management",     "Theory", "Dbms",     3},
        {"CS402", "DBMS Lab",               "Lab",    "Dbms",     2},
        {"CS501", "Artificial Intelligence", "Theory", "Ai",       3},
        {"CS502", "AI Lab",                  "Lab",    "Ai",       2},
        {"CS601", "Operating Systems",       "Theory", "Os",       3},
        {"CS602", "OS Lab",                  "Lab",    "Os",       2},
        {"MA101", "Engineering Maths I",    "Theory", "Maths",    4},
        {"MA102", "Engineering Maths II",   "Theory", "Maths",    4},
        {"CS701", "Algorithm Design",        "Theory", "Dsa",      3},
    };
    for (int i = 0; i < 13; i++) {
        Subject s; memset(&s, 0, sizeof(s));
        strcpy(s.code,  sd[i].co);
        strcpy(s.title, sd[i].ti);
        strcpy(s.type,  sd[i].ty);
        s.hrs = sd[i].h;
        strcpy(s.spec, sd[i].sp);
        s.alive = 1;
        sub[scnt++] = s;
    }
}

// ======================================================
//  MAIN
// ======================================================

int main() {
    cls();
    printf(CYN "\n  loading...\n" RST);
    loadsamples();
    printf(GRN "  %d faculty, %d subjects loaded\n" RST, fcnt, scnt);
    printf(DIM "  tip: press 8 to auto-assign all subjects\n" RST);
    printf(DIM "\n  (press enter)" RST);
    while (getchar() != '\n');
    getchar();

    while (1) {
        cls();
        printf("\n" CYN "  =========================================\n");
        printf("    Faculty Workload Management System\n");
        printf("    NIET Greater Noida | DSA-I PBL\n");
        printf("  =========================================\n" RST);

        printf("\n" WHT "  Faculty\n" RST);
        printf("   1. Add faculty\n");
        printf("   2. View all faculty\n");
        printf("   3. Faculty detail\n");
        printf("   4. Delete faculty\n");

        printf("\n" WHT "  Subjects\n" RST);
        printf("   5. Add subject\n");
        printf("   6. View all subjects\n");

        printf("\n" WHT "  Assignment\n" RST);
        printf("   7. Assign subject (manual)\n");
        printf("   8. Auto-distribute subjects\n");
        printf("   9. Unassign subject\n");

        printf("\n" WHT "  Reports\n" RST);
        printf("   10. Workload dashboard\n");
        printf("   11. Statistics\n");
        printf("   12. Assignment log\n");

        printf("\n   0. Exit\n");
        printf(DIM "  -----------------------------------------\n" RST);

        int ch = getint("  choice: ");
        switch (ch) {
            case 1:  add_faculty();   break;
            case 2:  view_faculty();  break;
            case 3:  fac_detail();    break;
            case 4:  delete_fac();    break;
            case 5:  add_subject();   break;
            case 6:  view_subjects(); break;
            case 7:  manual_assign(); break;
            case 8:  auto_assign();   break;
            case 9:  unassign_sub();  break;
            case 10: dashboard();     break;
            case 11: stats();         break;
            case 12: view_log();      break;
            case 0:
                cls();
                printf("\n  bye!\n\n");
                {
                    LogNode *cur = loghead;
                    while (cur) { LogNode *nx = cur->next; free(cur); cur = nx; }
                }
                return 0;
            default:
                printf(RED "  invalid\n" RST);
                break;
        }
    }
}
