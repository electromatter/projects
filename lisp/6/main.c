/******************************************************************************
 * Copyright (c) 2022 Eric Chai <electromatter@gmail.com>                     *
 *                                                                            *
 * Permission to use, copy, modify, and/or distribute this software for any   *
 * purpose with or without fee is hereby granted, provided that the above     *
 * copyright notice and this permission notice appear in all copies.          *
 *                                                                            *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES   *
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF           *
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR    *
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES     *
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER            *
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING     *
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.      *
 ******************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>

typedef int fixnum_t;
typedef struct val val_t;

#define FIXNUM_MIN      (INT_MIN)
#define FIXNUM_MAX      (INT_MAX)
#define VEC_MIN_SIZE    (8)
#define HASH_DEPTH      (16)
#define GC_MIN_BATCH    (100)
#define GC_MAX_BATCH    (10000)

enum tag {
    TAG_UNDEFINED = 0,
    TAG_FIXNUM,
    TAG_CHAR,
    TAG_CONS,
    TAG_STRING,
    TAG_SYMBOL,
    TAG_VECTOR,
    TAG_HASHTBL
};

struct val {
    enum tag tag;
    fixnum_t fix;
    union object *ptr;
};

struct head {
    enum tag tag;
    int mark;
    unsigned int identity;
    union object *next;
    union object *back;
};

struct cons {
    struct head head;
    val_t car;
    val_t cdr;
};

struct string {
    struct head head;
    unsigned char *slots;
    fixnum_t length;
    fixnum_t cap;
};

struct symbol {
    struct head head;
    val_t name;
};

struct vector {
    struct head head;
    fixnum_t length;
    fixnum_t cap;
    val_t *slots;
};

struct hashcell {
    int present;
    int tombstone;
    val_t hc;
    val_t key;
    val_t val;
};

struct hashtbl {
    struct head head;
    fixnum_t fill;
    fixnum_t load;
    fixnum_t cap;
    struct hashcell *slots;
};

union object {
    struct head head;
    struct cons cons;
    struct string string;
    struct symbol symbol;
    struct vector vector;
    struct hashtbl hashtbl;
};

struct frame {
    struct frame *prevframe;
    const char *name;
    int line;
    const char **vnames;
    val_t **vars;
    int nvars;
    const char *pnames[6];
    val_t *params[6];
    int nparams;
};

#define INIT_LOCAL      {TAG_UNDEFINED, 0, 0}

#define LOCAL0()                                                        \
    struct frame frame;                                                 \
    frame.prevframe = curframe;                                         \
    frame.name = __func__;                                              \
    frame.line = __LINE__;                                              \
    frame.vnames = NULL;                                                \
    frame.vars = NULL;                                                  \
    frame.nvars = 0;                                                    \
    frame.nparams = 0;                                                  \
    curframe = &frame

#define LOCAL1(a)                                                       \
    val_t a = INIT_LOCAL;                                               \
    struct frame frame;                                                 \
    val_t *localvars[1];                                                \
    const char *localvarnames[] = {#a};                                 \
    frame.vars = localvars;                                             \
    frame.vnames = localvarnames;                                       \
    frame.vars[0] = &a;                                                 \
    frame.prevframe = curframe;                                         \
    frame.name = __func__;                                              \
    frame.line = __LINE__;                                              \
    frame.nvars = 1;                                                    \
    frame.nparams = 0;                                                  \
    curframe = &frame

#define LOCAL2(a, b)                                                    \
    val_t a = INIT_LOCAL;                                               \
    val_t b = INIT_LOCAL;                                               \
    struct frame frame;                                                 \
    val_t *localvars[2];                                                \
    const char *localvarnames[] = {#a, #b};                             \
    frame.vars = localvars;                                             \
    frame.vnames = localvarnames;                                       \
    frame.vars[0] = &a;                                                 \
    frame.vars[1] = &b;                                                 \
    frame.prevframe = curframe;                                         \
    frame.name = __func__;                                              \
    frame.line = __LINE__;                                              \
    frame.nvars = 2;                                                    \
    frame.nparams = 0;                                                  \
    curframe = &frame

#define LOCAL3(a, b, c)                                                 \
    val_t a = INIT_LOCAL;                                               \
    val_t b = INIT_LOCAL;                                               \
    val_t c = INIT_LOCAL;                                               \
    struct frame frame;                                                 \
    val_t *localvars[3];                                                \
    const char *localvarnames[] = {#a, #b, #c};                         \
    frame.vars = localvars;                                             \
    frame.vnames = localvarnames;                                       \
    frame.vars[0] = &a;                                                 \
    frame.vars[1] = &b;                                                 \
    frame.vars[2] = &c;                                                 \
    frame.prevframe = curframe;                                         \
    frame.name = __func__;                                              \
    frame.line = __LINE__;                                              \
    frame.nvars = 3;                                                    \
    curframe = &frame

#define PARAM0()                                                        \
    frame.nparams = 0

#define PARAM1(a)                                                       \
    frame.params[0] = &a;                                               \
    frame.pnames[0] = #a;                                               \
    frame.nparams = 1

#define PARAM2(a, b)                                                    \
    frame.params[0] = &a;                                               \
    frame.pnames[0] = #a;                                               \
    frame.params[1] = &b;                                               \
    frame.pnames[1] = #b;                                               \
    frame.nparams = 2

#define PARAM3(a, b, c)                                                 \
    frame.params[0] = &a;                                               \
    frame.pnames[0] = #a;                                               \
    frame.params[1] = &b;                                               \
    frame.pnames[1] = #b;                                               \
    frame.params[2] = &c;                                               \
    frame.pnames[2] = #c;                                               \
    frame.nparams = 3

#define DEFINE0(func)                                                   \
static val_t func(void)
#define DEFINE1(func, a)                                                \
static val_t func(val_t a)
#define DEFINE2(func, a, b)                                             \
static val_t func(val_t a, val_t b)
#define DEFINE3(func, a, b, c)                                          \
static val_t func(val_t a, val_t b, val_t c)

#define RETURN(x)                                                       \
    do { curframe = frame.prevframe; return (x); } while (0)

#define DECLARE(x)

#define L       do { frame.line = __LINE__; } while(0);

#define TAG(x)          ((x).tag)

#define MAKEUNDEF(x)    ((x).tag = TAG_UNDEFIEND, (x).fix = 0, (x).ptr = 0)
#define MAKEFIX(x, f)   ((x).tag = TAG_FIXNUM, (x).fix = (f), (x).ptr = 0)
#define MAKECHAR(x, f)  ((x).tag = TAG_CHAR, (x).fix = (f), (x).ptr = 0)

#define EQ(x, y)        ((x).tag == (y).tag && (x).fix == (y).fix &&    \
                            (x).ptr == (y).ptr)

#define CHECK_TAG(x, t) (TAG((x)) != (t) ? die("EXPECTED " #t) : (void)0)

#define UNDEFINED(x)    (TAG((x)) == TAG_UNDEFINED)
#define FIXNUM(x)       (CHECK_TAG((x), TAG_FIXNUM), (x).fix)
#define CHAR(x)         (CHECK_TAG((x), TAG_CHAR), (x).fix)
#define CONS(x)         (CHECK_TAG((x), TAG_CONS), &(x).ptr->cons)
#define STRING(x)       (CHECK_TAG((x), TAG_STRING), &(x).ptr->string)
#define SYMBOL(x)       (CHECK_TAG((x), TAG_SYMBOL), &(x).ptr->symbol)
#define VECTOR(x)       (CHECK_TAG((x), TAG_VECTOR), &(x).ptr->vector)
#define HASHTBL(x)      (CHECK_TAG((x), TAG_HASHTBL), &(x).ptr->hashtbl)

#define IDENTITY(x)     ((x).ptr->head.identity)

#define RBARRIER(o, v)  do { mark(o); mark(v); } while (0)
#define WBARRIER(o, v)  do { mark(o); mark(v); } while (0)

DECLARE(FIXNUM)
DECLARE(CHAR)
DECLARE(CONS)
DECLARE(STRING)
DECLARE(SYMBOL)
DECLARE(VECTOR)
DECLARE(HASHTBL)

#define INTERN(cname, name)                                             \
static struct string cname##_str = {                                    \
    {TAG_STRING, 0, 0, 0, 0},                                           \
    (unsigned char *)name,                                              \
    sizeof(name) - 1,                                                   \
    sizeof(name) - 1                                                    \
};                                                                      \
static struct symbol cname##_sym = {                                    \
    {TAG_SYMBOL, 0, 0, 0, 0},                                           \
    {TAG_STRING, 0, (union object *)&cname##_str}                       \
};                                                                      \
static val_t cname = {TAG_SYMBOL, 0, (union object *)&cname##_sym};

#define INTERNS                                                         \
    INTERN(nil, "nil")                                                  \
    INTERN(t, "t")                                                      \
    INTERN(eof, "+eof+")                                                \
    INTERN(quote, "quote")                                              \
    INTERN(qquote, "quasiquote")                                        \
    INTERN(unquote, "unquote")                                          \
    INTERN(sunquote, "unquote-splicing")

#define GLOBAL(x) static val_t x;

#define GLOBALS                                                         \
    GLOBAL(undefined)                                                   \
    GLOBAL(memo)                                                        \
    GLOBAL(smacros)                                                     \
    GLOBAL(macros)                                                      \
    GLOBAL(environ)                                                     \
    GLOBAL(functions)                                                   \
    GLOBAL(labels)                                                      \
    GLOBAL(blocks)

static union object *gcobjs;
static union object *gcmark;
static union object **gcsweep;

INTERNS
GLOBALS

#undef INTERN
#define INTERN(x, s) #x,
#undef GLOBAL
#define GLOBAL(x) #x,

static const char *gnames[] = {
    INTERNS
    GLOBALS
};

#undef INTERN
#define INTERN(x, s) &x,
#undef GLOBAL
#define GLOBAL(x) &x,

static val_t *globals[] = {
    INTERNS
    GLOBALS
};

struct frame global_frame = {
    NULL,
    "<global>",
    __LINE__,
    gnames,
    globals,
    sizeof(globals) / sizeof(*globals),
    {NULL, NULL, NULL, NULL, NULL, NULL},
    {NULL, NULL, NULL, NULL, NULL, NULL},
    0
};
static struct frame *curframe = &global_frame;
static unsigned long nyoung;
static unsigned long nold;
static unsigned long identity;

static FILE *out;
static val_t write_form(val_t);

DECLARE(die)
static void die(const char *message) {
    struct frame *frame;
    int i;
    fflush(stdout);
    fprintf(stderr, "die: %s\n", message);
    fprintf(stderr, "Backtrace (most recent call first):\n");
    out = stderr;
    for (frame = curframe; frame != NULL; frame = frame->prevframe) {
        fprintf(stderr, "  %s:%d  (", frame->name, frame->line);
        for (i = 0; i < frame->nparams; i++) {
            fprintf(stderr, i > 0 ? ", %s=" : "%s=", frame->pnames[i]);
            write_form(*frame->params[i]);
        }
        fprintf(stderr, ")\n");
    }
    fflush(stderr);
    abort();
}

static void mark(val_t x) {
    if (TAG(x) < TAG_CONS || x.ptr->head.mark) {
        return;
    }
    x.ptr->head.mark = 1;
    x.ptr->head.back = gcmark;
    gcmark = x.ptr;
}

static void mark_stack(void) {
    struct frame *frame;
    int i;
    for (frame = curframe; frame != NULL; frame = frame->prevframe) {
        for (i = 0; i < frame->nvars; i++) {
            mark(*frame->vars[i]);
        }
        for (i = 0; i < frame->nparams; i++) {
            mark(*frame->params[i]);
        }
    }
}

static void free_obj(union object *o) {
    switch (o->head.tag) {
    case TAG_UNDEFINED:
        break;

    case TAG_FIXNUM:
        break;

    case TAG_CHAR:
        break;

    case TAG_CONS:
        free(o);
        return;

    case TAG_STRING:
        free(o->string.slots);
        free(o);
        return;

    case TAG_SYMBOL:
        free(o);
        return;

    case TAG_VECTOR:
        free(o->vector.slots);
        free(o);
        return;

    case TAG_HASHTBL:
        free(o->hashtbl.slots);
        free(o);
        return;
    }

    die("Invalid tag while sweeping heap");
}

static void sweep1(void) {
    union object *o;
    o = *gcsweep;
    if (o == NULL) {
        gcsweep = NULL;
        return;
    }

    if (o->head.mark) {
        o->head.mark = 0;
        gcsweep = &o->head.next;
        nold += 1;
    } else {
        *gcsweep = o->head.next;
        free_obj(o);
    }
}

static void mark1(void) {
    union object *o;
    int i;

    o = gcmark;
    gcmark = o->head.back;

    switch (o->head.tag) {
    case TAG_UNDEFINED:
        break;

    case TAG_FIXNUM:
        break;

    case TAG_CHAR:
        break;

    case TAG_CONS:
        mark(o->cons.car);
        mark(o->cons.cdr);
        return;

    case TAG_STRING:
        return;

    case TAG_SYMBOL:
        mark(o->symbol.name);
        return;

    case TAG_VECTOR:
        for (i = 0; i < o->vector.cap; i++) {
            mark(o->vector.slots[i]);
        }
        return;

    case TAG_HASHTBL:
        for (i = 0; i < o->hashtbl.cap; i++) {
            if (o->hashtbl.slots[i].present) {
                mark(o->hashtbl.slots[i].hc);
                mark(o->hashtbl.slots[i].key);
                mark(o->hashtbl.slots[i].val);
            }
        }
        return;
    }

    die("Invalid tag while marking heap");
}

static void gc(int full) {
    if (!full && (nyoung < nold || nyoung < GC_MIN_BATCH)) {
        return;
    }

    mark_stack();

    while (gcmark != NULL) {
        mark1();
    }

    gcsweep = &gcobjs;
    nold = 0;
    nyoung = 0;

    while (gcsweep != NULL) {
        sweep1();
    }
}

DECLARE(alloc)
static val_t alloc(enum tag tag) {
    val_t ret;
    union object *o;

    gc(0);

    switch (tag) {
    case TAG_CONS:
        o = (union object *)calloc(sizeof(struct cons), 1);
        break;
    case TAG_STRING:
        o = (union object *)calloc(sizeof(struct string), 1);
        break;
    case TAG_SYMBOL:
        o = (union object *)calloc(sizeof(struct symbol), 1);
        break;
    case TAG_VECTOR:
        o = (union object *)calloc(sizeof(struct vector), 1);
        break;
    case TAG_HASHTBL:
        o = (union object *)calloc(sizeof(struct hashtbl), 1);
        break;
    case TAG_UNDEFINED:
    default:
        die("Invalid tag");
    }

    if (o == NULL) {
        die("Out of memory");
    }

    o->head.tag = tag;
    o->head.mark = 0;
    o->head.identity = identity & FIXNUM_MAX;
    o->head.next = gcobjs;
    o->head.back = NULL;

    gcobjs = o;
    identity += 1;
    nyoung += 1;

    ret.tag = tag;
    ret.fix = 0;
    ret.ptr = o;

    return ret;
}

DEFINE2(cons, a, d) {
    LOCAL2(ret, z);
    PARAM2(a, d);
L   ret = alloc(TAG_CONS);

    MAKEFIX(z, 0);
L   CONS(ret)->car = z;
L   CONS(ret)->cdr = z;

L   CONS(ret)->car = a;
    WBARRIER(ret, a);

L   CONS(ret)->cdr = d;
    WBARRIER(ret, d);

    RETURN(ret);
}

DEFINE1(car, x) {
    LOCAL1(a);
    PARAM1(x);
    if (EQ(x, nil)) {
        RETURN(nil);
    }
L   a = CONS(x)->car;
    RBARRIER(x, a);
    RETURN(a);
}

DEFINE2(rplaca, x, a) {
    LOCAL0();
    PARAM2(x, a);
L   CONS(x)->car = a;
    WBARRIER(x, a);
    RETURN(x);
}

DEFINE1(cdr, x) {
    LOCAL1(d);
    PARAM1(x);
    if (EQ(x, nil)) {
        RETURN(nil);
    }
L   d = CONS(x)->cdr;
    RBARRIER(x, d);
    RETURN(d);
}


DEFINE2(rplacd, x, d) {
    LOCAL0();
    PARAM2(x, d);
L   CONS(x)->cdr = d;
    WBARRIER(x, d);
    RETURN(x);
}

DEFINE0(make_string) {
    LOCAL1(ret);
    PARAM0();
L   ret = alloc(TAG_STRING);
L   STRING(ret)->slots = 0;
L   STRING(ret)->length = 0;
L   STRING(ret)->cap = 0;
    RETURN(ret);
}

DEFINE0(make_vector) {
    LOCAL1(ret);
    PARAM0();
L   ret = alloc(TAG_VECTOR);
L   VECTOR(ret)->slots = 0;
L   VECTOR(ret)->length = 0;
L   VECTOR(ret)->cap = 0;
    RETURN(ret);
}

DEFINE1(vector_extend, v) {
    fixnum_t cap, newcap;
    void *ptr;
    LOCAL0();
    PARAM1(v);
    if (TAG(v) == TAG_VECTOR) {
L       cap = VECTOR(v)->cap;
L       if (VECTOR(v)->length < cap) {
            RETURN(v);
        }

        newcap = cap;
        if (newcap < VEC_MIN_SIZE) {
            newcap = VEC_MIN_SIZE;
        } else if (newcap > FIXNUM_MAX / 2 / (fixnum_t)sizeof(val_t)) {
L           die("Vector too large");
        }
        newcap *= 2;

L       ptr = realloc(VECTOR(v)->slots, newcap * sizeof(val_t));
        if (ptr == NULL) {
L           die("Out of memory");
        }

L       VECTOR(v)->slots = (val_t *)ptr;
L       VECTOR(v)->cap = newcap;

        for (; cap < newcap; cap++) {
L           VECTOR(v)->slots[cap] = nil;
        }
    } else if (TAG(v) == TAG_STRING) {
L       cap = STRING(v)->cap;
L       if (STRING(v)->length < cap) {
            RETURN(v);
        }

        newcap = cap;
        if (newcap < VEC_MIN_SIZE) {
            newcap = VEC_MIN_SIZE;
        } else if (cap > FIXNUM_MAX / 2) {
L           die("String too large");
        }
        newcap *= 2;

L       ptr = realloc(STRING(v)->slots, newcap);
        if (ptr == NULL) {
L           die("Out of memory");
        }

L       STRING(v)->slots = (unsigned char *)ptr;
L       STRING(v)->cap = newcap;

        for (; cap < newcap; cap++) {
L           STRING(v)->slots[cap] = 0;
        }
    } else {
L       die("Type error: expected a string or vector");
    }
    RETURN(v);
}

DEFINE2(vector_push_extend, x, v) {
    fixnum_t length;
    int ch;
    LOCAL1(ret);
    PARAM2(x, v);
L   vector_extend(v);
    if (TAG(v) == TAG_VECTOR) {
L       length = VECTOR(v)->length;
L       VECTOR(v)->slots[length] = x;
        MAKEFIX(ret, length);
        length += 1;
L       VECTOR(v)->length = length;
        WBARRIER(v, x);
    } else if (TAG(v) == TAG_STRING) {
L       length = STRING(v)->length;
L       ch = CHAR(x);
L       STRING(v)->slots[length] = ch;
        MAKEFIX(ret, length);
        length += 1;
L       STRING(v)->length = length;
    } else {
L       die("Type error. Expected a string or vector.");
    }
    RETURN(ret);
}

DEFINE1(vector_length, v) {
    fixnum_t length;
    LOCAL1(ret);
    PARAM1(v);
    if (TAG(v) == TAG_VECTOR) {
L       length = VECTOR(v)->length;
    } else if (TAG(v) == TAG_STRING) {
L       length = STRING(v)->length;
    } else {
L       die("Type error. Expected a string or vector.");
    }
    MAKEFIX(ret, length);
    RETURN(ret);
}

DEFINE2(elt, v, i) {
    fixnum_t length;
    fixnum_t index;
    int ch;
    LOCAL1(a);
    PARAM2(v, i);
    if (TAG(v) == TAG_VECTOR) {
L       length = VECTOR(v)->length;
L       index = FIXNUM(i);
        if (index < 0 || index > length) {
L           die("Index out of bounds");
        }
L       a = VECTOR(v)->slots[index];
        RBARRIER(v, a);
    } else if (TAG(v) == TAG_STRING) {
L       length = STRING(v)->length;
L       index = FIXNUM(i);
        if (index < 0 || index > length) {
L           die("Index out of bounds");
        }
L       ch = STRING(v)->slots[index];
        MAKECHAR(a, ch);
    } else {
L       die("Type error. Expected a string or vector.");
    }
    RETURN(a);
}

/*
DEFINE3(set_elt, v, i, a) {
    fixnum_t length;
    fixnum_t index;
    int ch;
    LOCAL0();
    PARAM3(v, i, a);
    if (TAG(v) == TAG_VECTOR) {
L       length = VECTOR(v)->length;
L       index = FIXNUM(i);
        if (index < 0 || index > length) {
L           die("Index out of bounds");
        }
L       VECTOR(v)->slots[index] = a;
        WBARRIER(v, a);
    } else if (TAG(v) == TAG_STRING) {
L       length = STRING(v)->length;
L       index = FIXNUM(i);
        if (index < 0 || index > length) {
L           die("Index out of bounds");
        }
L       ch = CHAR(a);
L       STRING(v)->slots[index] = ch;
    } else {
L       die("Type error. Expected a string or vector.");
    }
    RETURN(a);
}
*/

DEFINE0(make_hashtbl) {
    LOCAL1(ret);
    PARAM0();
L   ret = alloc(TAG_HASHTBL);
L   HASHTBL(ret)->fill = 0;
L   HASHTBL(ret)->load = 0;
L   HASHTBL(ret)->cap = 0;
L   HASHTBL(ret)->slots = NULL;
    RETURN(ret);
}

DEFINE2(hashval, val, depth) {
    fixnum_t i, length;
    unsigned int x;
    LOCAL3(hc, a, deeper);
    PARAM2(val, depth);

L   if (FIXNUM(depth) > HASH_DEPTH) {
        RETURN(hc);
    }

    switch (TAG(val)) {
    case TAG_UNDEFINED:
        break;

    case TAG_FIXNUM:
        RETURN(val);

    case TAG_CHAR:
L       x = CHAR(val);
        MAKEFIX(hc, x);
        RETURN(hc);

    case TAG_CONS:
L       x = FIXNUM(depth);
        x += 1;
        MAKEFIX(deeper, x);
        x = 2166136261;
L       a = car(val);
L       hc = hashval(a, deeper);
L       x = 6777619 * (FIXNUM(hc) ^ x);
L       a = cdr(val);
L       hc = hashval(a, deeper);
L       x = 6777619 * (FIXNUM(hc) ^ x);
        MAKEFIX(hc, x & FIXNUM_MAX);
        RETURN(hc);

    case TAG_SYMBOL:
        x = IDENTITY(val);
        MAKEFIX(hc, x);
        RETURN(hc);

    case TAG_STRING: case TAG_VECTOR:
L       x = FIXNUM(depth);
        x += 1;
        MAKEFIX(deeper, x);
L       hc = vector_length(val);
L       length = FIXNUM(hc);
        x = 2166136261;
        for (i = 0; i < length; i++) {
            MAKEFIX(hc, i);
L           a = elt(val, hc);
L           hc = hashval(a, deeper);
L           x = 6777619 * (FIXNUM(hc) ^ x);
        }
        MAKEFIX(hc, x & FIXNUM_MAX);
        RETURN(hc);

    case TAG_HASHTBL:
        x = IDENTITY(val);
        MAKEFIX(hc, x);
        RETURN(hc);
    }

L   die("Invalid tag");
    RETURN(nil);
}

DEFINE1(sxhash, val) {
    LOCAL2(ret, depth);
    PARAM1(val);
    MAKEFIX(depth, 0);
L   ret = hashval(val, depth);
    RETURN(ret);
}

DEFINE2(equal, x, y) {
    fixnum_t i, length;
    LOCAL3(ret, a, b);
    PARAM2(x, y);

    if (EQ(x, y)) {
        RETURN(t);
    }

    if (TAG(x) != TAG(y)) {
        RETURN(nil);
    }

    switch (TAG(x)) {
    case TAG_UNDEFINED:
        break;

    case TAG_FIXNUM:
        RETURN(nil);

    case TAG_CHAR:
        RETURN(nil);

    case TAG_CONS:
L       a = car(x);
L       b = car(y);
L       ret = equal(a, b);
        if (EQ(ret, nil)) {
            RETURN(ret);
        }
L       a = cdr(x);
L       b = cdr(y);
L       ret = equal(a, b);
        RETURN(ret);

    case TAG_STRING: case TAG_VECTOR:
L       a = vector_length(x);
L       b = vector_length(y);
        if (!EQ(a, b)) {
            RETURN(nil);
        }
L       length = FIXNUM(a);
        ret = t;
        for (i = 0; i < length; i++) {
            MAKEFIX(ret, i);
L           a = elt(x, ret);
L           b = elt(y, ret);
L           ret = equal(a, b);
            if (EQ(ret, nil)) {
                break;
            }
        }
        RETURN(ret);

    case TAG_SYMBOL:
        RETURN(nil);

    case TAG_HASHTBL:
        RETURN(nil);
    }

L   die("invalid tag");
    RETURN(nil);
}

DECLARE(HASH_FIND)
#define HASH_FIND(tbl, hc, key, i, mask)                                \
    do {                                                                \
        hc = sxhash(key);                                               \
        mask = (HASHTBL(tbl)->cap - 1);                                 \
        i = FIXNUM(hc) & mask;                                          \
        while (1) {                                                     \
            if (HASHTBL(tbl)->slots[i].present) {                       \
                if (                                                    \
                    EQ(HASHTBL(tbl)->slots[i].hc, hc)                   \
                    && !EQ(equal(HASHTBL(tbl)->slots[i].key, key), nil) \
                ) {                                                     \
                    break;                                              \
                }                                                       \
            } else if (!HASHTBL(tbl)->slots[i].tombstone) {             \
                break;                                                  \
            }                                                           \
            i = (i * 5 + 1) & mask;                                     \
        }                                                               \
    } while (0)

DEFINE1(hash_expand, tbl) {
    unsigned int i, mask;
    fixnum_t fill, load, cap, newcap, j;
    struct hashcell *slots, *newslots;
    LOCAL2(hc, key);
    PARAM1(tbl);

L   fill = HASHTBL(tbl)->fill;
L   load = HASHTBL(tbl)->load;
L   cap = HASHTBL(tbl)->cap;
L   slots = HASHTBL(tbl)->slots;

    if (load * 4 < cap * 3 && fill * 2 >= load) {
        RETURN(nil);
    }

    if (fill > FIXNUM_MAX / 16) {
L       die("Hash table too big");
    }

    newcap = 1;
    while (newcap < VEC_MIN_SIZE || newcap < (fill + (fill * 5 + 7) / 8)) {
        newcap *= 2;
    }

    newslots = (struct hashcell *)calloc(newcap, sizeof(*newslots));
    if (newslots == NULL) {
L       die("Out of memory");
    }

L   HASHTBL(tbl)->load = fill;
L   HASHTBL(tbl)->cap = newcap;
L   HASHTBL(tbl)->slots = newslots;

    for (j = 0; j < cap; j++) {
        if (!slots[j].present) {
            continue;
        }

        key = slots[j].key;
L       HASH_FIND(tbl, hc, key, i, mask);
        newslots[i].present = 1;
        newslots[i].tombstone = 1;
        newslots[i].hc = hc;
        newslots[i].key = key;
        newslots[i].val = slots[j].val;
    }

    free(slots);
    RETURN(nil);
}

DEFINE3(hash_get, tbl, key, def) {
    fixnum_t i, mask;
    LOCAL2(ret, hc);
    PARAM3(tbl, key, def);
L   hash_expand(tbl);
L   HASH_FIND(tbl, hc, key, i, mask);
L   if (HASHTBL(tbl)->slots[i].present) {
L       ret = HASHTBL(tbl)->slots[i].val;
        RBARRIER(tbl, ret);
        RETURN(ret);
    }
    RETURN(def);
}

DEFINE3(hash_set, tbl, key, val) {
    unsigned int i, mask;
    LOCAL1(hc);
    PARAM3(tbl, key, val);
L   hash_expand(tbl);
L   HASH_FIND(tbl, hc, key, i, mask);
L   if (HASHTBL(tbl)->slots[i].present) {
L       HASHTBL(tbl)->slots[i].val = val;
        WBARRIER(tbl, val);
        RETURN(val);
    } else {
L       if (!HASHTBL(tbl)->slots[i].tombstone) {
L           HASHTBL(tbl)->load += 1;
        }
L       HASHTBL(tbl)->slots[i].hc = hc;
L       HASHTBL(tbl)->slots[i].key = key;
L       HASHTBL(tbl)->slots[i].val = val;
L       HASHTBL(tbl)->slots[i].present = 1;
L       HASHTBL(tbl)->slots[i].tombstone = 1;
L       HASHTBL(tbl)->fill += 1;
        WBARRIER(tbl, key);
        WBARRIER(tbl, val);
        RETURN(val);
    }
    RETURN(val);
}

/*
DEFINE3(hash_del, tbl, key, def) {
    unsigned int i, mask;
    LOCAL2(ret, hc);
    PARAM3(tbl, key, def);
L   hash_expand(tbl);
L   HASH_FIND(tbl, hc, key, i, mask);
L   if (HASHTBL(tbl)->slots[i].present) {
L       ret = HASHTBL(tbl)->slots[i].val;
L       HASHTBL(tbl)->slots[i].hc = nil;
L       HASHTBL(tbl)->slots[i].key = nil;
L       HASHTBL(tbl)->slots[i].val = nil;
L       HASHTBL(tbl)->slots[i].present = 0;
L       HASHTBL(tbl)->fill -= 1;
        RBARRIER(tbl, ret);
        RETURN(ret);
    }
    RETURN(def);
}
*/

DEFINE1(make_symbol, name) {
    LOCAL1(ret);
    PARAM1(name);
L   ret = alloc(TAG_SYMBOL);
L   SYMBOL(ret)->name = name;
    WBARRIER(ret, name);
    RETURN(ret);
}

DEFINE1(intern, x) {
    LOCAL2(s, z);
    PARAM1(x);

    MAKEFIX(z, 0);

L   s = hash_get(memo, x, z);
    if (!EQ(s, z)) {
        RETURN(s);
    }

L   s = make_symbol(x);
L   hash_set(memo, x, s);
    RETURN(s);
}

DEFINE1(parse_token, x) {
    long int value;
    fixnum_t i;
    fixnum_t length;
    const unsigned char *s;
    char buf[32];
    LOCAL1(ret);
    PARAM1(x);

L   length = STRING(x)->length;
L   s = STRING(x)->slots;

    if (length == 0) {
L       die("Invalid token");
    }

    i = 0;
    if (s[0] == '-' || s[0] == '+') {
        buf[i] = s[i];
        i += 1;
    }

    if (i == length) {
L       ret = intern(x);
        RETURN(ret);
    }

    while (i < length && s[i] >= '0' && s[i] <= '9') {
        if (i < (fixnum_t)sizeof(buf)) {
            buf[i] = s[i];
        }
        i += 1;
    }

    if (i == length) {
        if (i >= (fixnum_t)sizeof(buf)) {
L           die("Integer too large");
        }

        buf[i] = 0;

        errno = 0;
        value = strtol(buf, NULL, 10);
        if (value < FIXNUM_MIN || value > FIXNUM_MAX || errno != 0) {
L           die("Integer out of fixnum range");
        }

        MAKEFIX(ret, value);
        RETURN(ret);
    }

L   ret = intern(x);
    RETURN(ret);
}

DEFINE1(memorize, symbol) {
    LOCAL1(name);
    PARAM1(symbol);
L   name = SYMBOL(symbol)->name;
    RBARRIER(symbol, name);
L   hash_set(memo, name, symbol);
    RETURN(nil);
}

#undef INTERN
#define INTERN(x, s) L memorize(x);

DEFINE0(init) {
    LOCAL1(h);
    PARAM0();

L   memo = make_hashtbl();

L   h = make_hashtbl();
L   smacros = cons(h, nil);

L   h = make_hashtbl();
L   macros = cons(h, nil);

L   h = make_hashtbl();
L   environ = cons(h, nil);

L   h = make_hashtbl();
L   functions = cons(h, nil);

L   labels = nil;
L   blocks = nil;

    INTERNS

    RETURN(nil);
}

static int munch(void) {
    int ch;
    while (1) {
        ch = getc(stdin);
        switch (ch) {
        case ' ': case '\t': case '\v': case '\f': case '\r': case '\n':
            break;

        case ';':
            while (ch != EOF && ch != '\n') {
                ch = getc(stdin);
            }
            break;

        default:
            return ch;
        }
    }
}

DEFINE0(read_form) {
    int ch;
    LOCAL3(ret, tail, val);
    PARAM0();

    ch = munch();

    switch (ch) {
    case EOF:
        RETURN(eof);

    case '(':
        ch = munch();
        if (ch == ')') {
            RETURN(nil);
        }

L       tail = cons(nil, nil);
        ret = tail;
        while (1) {
            ungetc(ch, stdin);

L           val = read_form();
            if (EQ(val, eof)) {
L               die("Expected )");
            }

L           rplaca(tail, val);

            ch = munch();
            if (ch == ')') {
                break;
            }

            if (ch == '.') {
L               val = read_form();
                if (EQ(val, eof)) {
L                   die("Expected form in cdr place");
                }

L               rplacd(tail, val);

                ch = munch();
                if (ch != ')') {
L                   die("Expected ) after cdr place");
                }

                break;
            }

L           val = cons(nil, nil);
L           rplacd(tail, val);
            tail = val;
        }
        RETURN(ret);

    case ')':
L       die("Extra )");
        RETURN(nil);

    case '\'':
L       val = read_form();
        if (EQ(val, eof)) {
L           die("Expected form after '");
        }
L       tail = cons(val, nil);
L       ret = cons(quote, tail);
        RETURN(ret);

    case '`':
L       val = read_form();
        if (EQ(val, eof)) {
L           die("Expected form after `");
        }
L       tail = cons(val, nil);
L       ret = cons(qquote, tail);
        RETURN(ret);

    case ',':
        ch = getc(stdin);
        if (ch == '@') {
L           val = read_form();
            if (EQ(val, eof)) {
L               die("Expected form after ,@");
            }
L           tail = cons(val, nil);
L           ret = cons(sunquote, tail);
        } else {
            ungetc(ch, stdin);
L           val = read_form();
            if (EQ(val, eof)) {
L               die("Expected form after ,");
            }
L           tail = cons(val, nil);
L           ret = cons(unquote, tail);
        }
        RETURN(ret);

    case '"':
L       ret = make_string();
        while (1) {
            ch = getc(stdin);

            if (ch == '"') {
                break;
            }

            if (ch == '\\') {
                ch = getc(stdin);
            }

            if (ch == EOF) {
L               die("Expected \"");
            }

            MAKECHAR(val, ch);
L           vector_push_extend(val, ret);
        }
        RETURN(ret);

    case '#':
        ch = getc(stdin);
        switch (ch) {
        case '(':
L           ret = make_vector();
            while (1) {
                ch = munch();
                if (ch == ')') {
                    break;
                }

                ungetc(ch, stdin);
L               val = read_form();
                if (EQ(val, eof)) {
L                   die("Expected )");
                }

L               vector_push_extend(val, ret);
            }
            RETURN(ret);

        default:
L           die("Unknown dispatching macro character");
            RETURN(ret);
        }

    default:
L       tail = make_string();
        while (1) {
            switch (ch) {
            case '!': case '$': case '%': case '&':
            case '*': case '+': case '-': case '/':
            case ':': case '<': case '=': case '>':
            case '?': case '@': case '^': case '_':
            case '~':
                break;

            default:
                if (ch >= '0' && ch <= '9') {
                    break;
                }

                if (ch >= 'a' && ch <= 'z') {
                    break;
                }

                if (ch >= 'A' && ch <= 'Z') {
                    break;
                }

                ungetc(ch, stdin);
L               ret = parse_token(tail);
                RETURN(ret);
            }

            MAKECHAR(val, ch);
L           vector_push_extend(val, tail);
            ch = getc(stdin);
        }
    }
}

    /*
     * global environment
     * return point
     * code vector
     * closure vector
     * primary value
     * values vector
     *
     * block
     * catch
     * eval-when
     * flet
     * function
     * go
     * if
     * labels
     * let
     * let*
     * load-time-value
     * locally
     * macrolet
     * multiple-value-call
     * multiple-value-prog1
     * progn
     * progv
     * quote
     * return-from
     * setq
     * symbol-macrolet
     * tagbody
     * the
     * throw
     * unwind-protect
     */

DEFINE2(find_tbl, env, key) {
    LOCAL3(e, tbl, v);
    PARAM2(env, key);
    e = env;
    while (1) {
        if (EQ(e, nil)) {
            RETURN(nil);
        }
L       tbl = car(e);
L       v = hash_get(tbl, key, undefined);
        if (!UNDEFINED(v)) {
            RETURN(tbl);
        }
L       e = cdr(e);
    }
}

DEFINE2(apply, fn, args) {
    LOCAL1(x);
    PARAM2(fn, args);
L   die("TODO");
    RETURN(x);
}

DEFINE1(macroexpand, x) {
    LOCAL3(op, args, tbl);
    PARAM1(x);
    while (1) {
        if (TAG(x) == TAG_SYMBOL) {
            /* Symbol macros */
L           tbl = find_tbl(smacros, x);
            if (EQ(tbl, nil)) {
                break;
            }
L           x = hash_get(tbl, x, nil);
        } else if (TAG(x) == TAG_CONS) {
L           op = car(x);
L           args = cdr(x);

            /* Macro */
L           tbl = find_tbl(macros, op);
            if (EQ(tbl, nil)) {
                break;
            }
L           op = hash_get(tbl, op, nil);
L           x = apply(op, args);
        } else {
            /* Literal */
            break;
        }
    }
    RETURN(x);
}

DEFINE1(compile, x) {
    LOCAL0();
    PARAM1(x);
L   x = macroexpand(x);
    RETURN(x);
}

DEFINE1(eval, x) {
    LOCAL2(fn, ret);
    PARAM1(x);
L   fn = compile(x);
L   ret = apply(fn, nil);
    RETURN(ret);
}

/*
 * block
 * catch
 * eval-when
 * flet
 * function
 * go
 * if
 * labels
 * let
 * let*
 * load-time-value
 * locally
 * macrolet
 * multiple-value-call
 * multiple-value-prog1
 * progn
 * progv
 * quote
 * return-from
 * setq
 * symbol-macrolet
 * tagbody
 * the
 * throw
 * unwind-protect
 */

DEFINE1(write_form, x) {
    unsigned long i, length;
    const unsigned char *s;
    LOCAL3(l, a, j);
    PARAM1(x);

    if (out == NULL) {
        out = stdout;
    }

    switch (TAG(x)) {
    case TAG_UNDEFINED:
        fprintf(out, "#<undefined>");
        RETURN(x);

    case TAG_FIXNUM:
L       fprintf(out, "%d", FIXNUM(x));
        RETURN(x);

    case TAG_CHAR:
L       fprintf(out, "#\\%c", CHAR(x));
        RETURN(x);

    case TAG_CONS:
        putc('(', out);
        l = x;
        while (1) {
L           a = car(l);
L           write_form(a);

L           l = cdr(l);
            if (EQ(l, nil)) {
                break;
            }

            if (TAG(l) != TAG_CONS) {
                putc(' ', out);
                putc('.', out);
                putc(' ', out);
L               write_form(l);
                break;
            }

            putc(' ', out);
        }
        putc(')', out);
        RETURN(x);

    case TAG_STRING:
        putc('"', out);
L       length = STRING(x)->length;
L       s = STRING(x)->slots;
        for (i = 0; i < length; i++) {
            if (s[i] == '\\' || s[i] == '"') {
                putc('\\', out);
            }
            putc(s[i], out);
        }
        putc('"', out);
        RETURN(x);

    case TAG_SYMBOL:
L       a = SYMBOL(x)->name;
        RBARRIER(x, a);
L       length = STRING(a)->length;
L       s = STRING(a)->slots;
        for (i = 0; i < length; i++) {
            putc(s[i], out);
        }
        RETURN(x);

    case TAG_VECTOR:
        putc('#', out);
        putc('(', out);
L       length = VECTOR(x)->length;
        for (i = 0; i < length; i++) {
            if (i > 0) {
                putc(' ', out);
            }
            MAKEFIX(j, i);
L           a = elt(x, j);
L           write_form(a);
        }
        putc(')', out);
        break;

    case TAG_HASHTBL:
        fprintf(out, "#<HASHTBL>");
        break;

    default:
L       die("Invalid value");
    }

    RETURN(x);
}

DEFINE1(print, x) {
    LOCAL0();
    PARAM1(x);
L   write_form(x);
    putc('\n', stdout);
    RETURN(x);
}

DEFINE0(repl) {
    LOCAL2(x, y);
    PARAM0();
    while (1) {
L       x = read_form();

        if (EQ(x, eof)) {
            RETURN(nil);
        }

L       y = eval(x);

L       print(y);
    }
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    init();
    repl();
    return 0;
}
