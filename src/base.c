#include <fakeLisp/base.h>
#include <fakeLisp/bytecode.h>
#include <fakeLisp/common.h>
#include <fakeLisp/utils.h>

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

FklString *fklCreateString(size_t size, const char *str) {
    FklString *tmp =
        (FklString *)malloc(sizeof(FklString) + size * sizeof(uint8_t));
    FKL_ASSERT(tmp);
    tmp->size = size;
    if (str)
        memcpy(tmp->str, str, size);
    tmp->str[size] = '\0';
    return tmp;
}

FklString *fklStringRealloc(FklString *str, size_t new_size) {
    FklString *new = (FklString *)fklRealloc(
        str, sizeof(FklString) + new_size * sizeof(uint8_t));
    FKL_ASSERT(new);
    new->str[new_size] = '\0';
    return new;
}

int fklStringEqual(const FklString *fir, const FklString *sec) {
    if (fir->size == sec->size)
        return !memcmp(fir->str, sec->str, fir->size);
    return 0;
}

int fklStringCmp(const FklString *fir, const FklString *sec) {
    return strcmp(fir->str, sec->str);
}

int fklStringCstrCmp(const FklString *fir, const char *sec) {
    return strcmp(fir->str, sec);
}

int fklStringCharBufCmp(const FklString *fir, size_t len, const char *buf) {
    size_t size = fir->size < len ? fir->size : len;
    int r = memcmp(fir->str, buf, size);
    if (r)
        return r;
    else if (fir->size < len)
        return -1;
    else if (fir->size > len)
        return 1;
    return r;
}

size_t fklStringCharBufMatch(const FklString *a, const char *b, size_t b_size) {
    return fklCharBufMatch(a->str, a->size, b, b_size);
}

size_t fklCharBufMatch(const char *a, size_t a_size, const char *b,
                       size_t b_size) {
    if (b_size < a_size)
        return 0;
    if (!memcmp(a, b, a_size))
        return a_size;
    return 0;
}

FklString *fklCopyString(const FklString *obj) {
    return fklCreateString(obj->size, obj->str);
}

char *fklStringToCstr(const FklString *str) {
    return fklCharBufToCstr(str->str, str->size);
}

FklString *fklCreateStringFromCstr(const char *cStr) {
    return fklCreateString(strlen(cStr), cStr);
}

void fklStringCharBufCat(FklString **a, const char *buf, size_t s) {
    size_t aSize = (*a)->size;
    FklString *prev = *a;
    prev = (FklString *)fklRealloc(prev, sizeof(FklString)
                                             + (aSize + s) * sizeof(char));
    FKL_ASSERT(prev);
    *a = prev;
    prev->size = aSize + s;
    memcpy(prev->str + aSize, buf, s);
    prev->str[prev->size] = '\0';
}

void fklStringCat(FklString **fir, const FklString *sec) {
    fklStringCharBufCat(fir, sec->str, sec->size);
}

void fklStringCstrCat(FklString **pfir, const char *sec) {
    fklStringCharBufCat(pfir, sec, strlen(sec));
}

FklString *fklCreateEmptyString() {
    FklString *tmp = (FklString *)calloc(1, sizeof(FklString));
    FKL_ASSERT(tmp);
    return tmp;
}

void fklPrintRawString(const FklString *str, FILE *fp) {
    fklPrintRawCharBuf((const uint8_t *)str->str, str->size, "\"", "\"", '"',
                       fp);
}

void fklPrintRawSymbol(const FklString *str, FILE *fp) {
    fklPrintRawCharBuf((const uint8_t *)str->str, str->size, "|", "|", '|', fp);
}

void fklPrintString(const FklString *str, FILE *fp) {
    fwrite(str->str, str->size, 1, fp);
}

int fklIsSpecialCharAndPrintToStringBuffer(FklStringBuffer *s, char ch) {
    int r = 0;
    if ((r = ch == '\n'))
        fklStringBufferConcatWithCstr(s, "\\n");
    else if ((r = ch == '\t'))
        fklStringBufferConcatWithCstr(s, "\\t");
    else if ((r = ch == '\v'))
        fklStringBufferConcatWithCstr(s, "\\v");
    else if ((r = ch == '\a'))
        fklStringBufferConcatWithCstr(s, "\\a");
    else if ((r = ch == '\b'))
        fklStringBufferConcatWithCstr(s, "\\b");
    else if ((r = ch == '\f'))
        fklStringBufferConcatWithCstr(s, "\\f");
    else if ((r = ch == '\r'))
        fklStringBufferConcatWithCstr(s, "\\r");
    else if ((r = ch == '\x20'))
        fklStringBufferPutc(s, ' ');
    return r;
}

void fklPrintRawCharBufToStringBuffer(struct FklStringBuffer *s, size_t size,
                                      const char *fstr, const char *begin_str,
                                      const char *end_str, char se) {
    fklStringBufferConcatWithCstr(s, begin_str);
    uint8_t *str = (uint8_t *)fstr;
    for (size_t i = 0; i < size;) {
        unsigned int l = fklGetByteNumOfUtf8(&str[i], size - i);
        if (l == 7) {
            fklStringBufferPrintf(s, "\\x%02X", (uint8_t)str[i]);
            i++;
        } else if (l == 1) {
            if (str[i] == se) {
                fklStringBufferPutc(s, '\\');
                fklStringBufferPutc(s, se);
            } else if (str[i] == '\\')
                fklStringBufferConcatWithCstr(s, "\\\\");
            else if (isgraph(str[i]))
                fklStringBufferPutc(s, str[i]);
            else if (fklIsSpecialCharAndPrintToStringBuffer(s, str[i]))
                ;
            else
                fklStringBufferPrintf(s, "\\x%02X", (uint8_t)str[i]);
            i++;
        } else {
            fklStringBufferBincpy(s, &str[i], l);
            i += l;
        }
    }
    fklStringBufferConcatWithCstr(s, end_str);
}

FklString *fklStringToRawString(const FklString *str) {
    FklStringBuffer buf = FKL_STRING_BUFFER_INIT;
    fklInitStringBuffer(&buf);
    fklPrintRawStringToStringBuffer(&buf, str, "\"", "\"", '"');
    FklString *retval = fklStringBufferToString(&buf);
    fklUninitStringBuffer(&buf);
    return retval;
}

FklString *fklStringToRawSymbol(const FklString *str) {
    FklStringBuffer buf = FKL_STRING_BUFFER_INIT;
    fklInitStringBuffer(&buf);
    fklPrintRawStringToStringBuffer(&buf, str, "|", "|", '|');
    FklString *retval = fklStringBufferToString(&buf);
    fklUninitStringBuffer(&buf);
    return retval;
}

FklString *fklStringAppend(const FklString *a, const FklString *b) {
    FklString *r = fklCopyString(a);
    fklStringCat(&r, b);
    return r;
}

char *fklCstrStringCat(char *fir, const FklString *sec) {
    if (!fir)
        return fklStringToCstr(sec);
    size_t len = strlen(fir);
    fir = (char *)fklRealloc(fir, len * sizeof(char) + sec->size + 1);
    FKL_ASSERT(fir);
    fklWriteStringToCstr(&fir[len], sec);
    return fir;
}

void fklWriteStringToCstr(char *c_str, const FklString *str) {
    size_t len = 0;
    const char *buf = str->str;
    size_t size = str->size;
    for (size_t i = 0; i < size; i++) {
        if (!buf[i])
            continue;
        c_str[len] = buf[i];
        len++;
    }
    c_str[len] = '\0';
}

size_t fklCountCharInString(FklString *s, char c) {
    return fklCountCharInBuf(s->str, s->size, c);
}

void fklInitStringBuffer(FklStringBuffer *b) {
    b->index = 0;
    b->size = 0;
    b->buf = NULL;
    fklStringBufferReverse(b, 64);
}

void fklInitStringBufferWithCapacity(FklStringBuffer *b, size_t len) {
    b->index = 0;
    b->size = 0;
    b->buf = NULL;
    fklStringBufferReverse(b, len);
}

void fklStringBufferPutc(FklStringBuffer *b, char c) {
    fklStringBufferReverse(b, 2);
    b->buf[b->index++] = c;
    b->buf[b->index] = '\0';
}

void fklStringBufferBincpy(FklStringBuffer *b, const void *p, size_t l) {
    fklStringBufferReverse(b, l + 1);
    memcpy(&b->buf[b->index], p, l);
    b->index += l;
    b->buf[b->index] = '\0';
}

FklString *fklStringBufferToString(FklStringBuffer *b) {
    return fklCreateString(b->index, b->buf);
}

FklBytevector *fklStringBufferToBytevector(FklStringBuffer *b) {
    return fklCreateBytevector(b->index, (uint8_t *)b->buf);
}

FklStringBuffer *fklCreateStringBuffer(void) {
    FklStringBuffer *r = (FklStringBuffer *)malloc(sizeof(FklStringBuffer));
    FKL_ASSERT(r);
    fklInitStringBuffer(r);
    return r;
}

void fklUninitStringBuffer(FklStringBuffer *b) {
    b->size = 0;
    b->index = 0;
    free(b->buf);
    b->buf = NULL;
}

void fklDestroyStringBuffer(FklStringBuffer *b) {
    fklUninitStringBuffer(b);
    free(b);
}

void fklStringBufferClear(FklStringBuffer *b) { b->index = 0; }

void fklStringBufferFill(FklStringBuffer *b, char c) {
    memset(b->buf, c, b->index);
}

void fklStringBufferReverse(FklStringBuffer *b, size_t s) {
    if ((b->size - b->index) < s) {
        b->size <<= 1;
        if ((b->size - b->index) < s)
            b->size += s;
        char *t = (char *)fklRealloc(b->buf, b->size);
        FKL_ASSERT(t);
        b->buf = t;
    }
}

void fklStringBufferShrinkTo(FklStringBuffer *b, size_t s) {
    char *t = (char *)fklRealloc(b->buf, s);
    FKL_ASSERT(t);
    t[s - 1] = '\0';
    b->buf = t;
    b->size = s;
}

static inline void stringbuffer_reserve(FklStringBuffer *b, size_t s) {
    if (b->size < s) {
        b->size <<= 1;
        if (b->size < s)
            b->size = s;
        char *t = (char *)fklRealloc(b->buf, b->size);
        FKL_ASSERT(t);
        b->buf = t;
    }
}

void fklStringBufferResize(FklStringBuffer *b, size_t ns, char c) {
    if (ns > b->index) {
        stringbuffer_reserve(b, ns + 1);
        memset(&b->buf[b->index], c, ns - b->index);
        b->buf[ns] = '\0';
    }
    b->index = ns;
}

static inline void string_buffer_printf_va(FklStringBuffer *b, const char *fmt,
                                           va_list ap) {
    long n;
    va_list cp;
    for (;;) {
#ifdef _WIN32
        cp = ap;
#else
        va_copy(cp, ap);
#endif
        n = vsnprintf(&b->buf[b->index], b->size - b->index, fmt, cp);
        va_end(cp);
        if ((n > -1) && n < (b->size - b->index)) {
            b->index += n;
            return;
        }
        if (n > -1)
            fklStringBufferReverse(b, n + 1);
        else
            fklStringBufferReverse(b, (b->size) * 2);
    }
}

void fklStringBufferPrintf(FklStringBuffer *b, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    string_buffer_printf_va(b, fmt, ap);
    va_end(ap);
}

void fklStringBufferConcatWithCstr(FklStringBuffer *b, const char *s) {
    fklStringBufferBincpy(b, s, strlen(s));
}

void fklStringBufferConcatWithString(FklStringBuffer *b, const FklString *s) {
    fklStringBufferBincpy(b, s->str, s->size);
}

void fklStringBufferConcatWithStringBuffer(FklStringBuffer *a,
                                           const FklStringBuffer *b) {
    fklStringBufferBincpy(a, b->buf, b->index);
}

int fklStringBufferCmp(const FklStringBuffer *a, const FklStringBuffer *b) {
    size_t size = a->index < b->index ? a->index : b->index;
    int r = memcmp(a->buf, b->buf, size);
    if (r)
        return r;
    else if (a->index < b->index)
        return -1;
    else if (a->index > b->index)
        return 1;
    return r;
}

#define DEFAULT_HASH_TABLE_SIZE (4)

void fklInitHashTable(FklHashTable *r, const FklHashTableMetaTable *t) {
    FklHashTableItem **base = (FklHashTableItem **)calloc(
        DEFAULT_HASH_TABLE_SIZE, sizeof(FklHashTableItem *));
    FKL_ASSERT(base);
    r->base = base;
    r->first = NULL;
    r->last = NULL;
    r->num = 0;
    r->size = DEFAULT_HASH_TABLE_SIZE;
    r->mask = DEFAULT_HASH_TABLE_SIZE - 1;
    r->t = t;
}

FklHashTable *fklCreateHashTable(const FklHashTableMetaTable *t) {
    FklHashTable *r = (FklHashTable *)malloc(sizeof(FklHashTable));
    FKL_ASSERT(r);
    fklInitHashTable(r, t);
    return r;
}

void fklPtrKeySet(void *k0, const void *k1) {
    *(void **)k0 = *(void *const *)k1;
}

int fklPtrKeyEqual(const void *k0, const void *k1) {
    return *(void *const *)k0 == *(void *const *)k1;
}

uintptr_t fklPtrKeyHashFunc(const void *k) {
    return (uintptr_t)*(void *const *)k;
}

#define HASH_FUNC_HEADER()                                                     \
    uintptr_t (*hashv)(const void *) = ht->t->__hashFunc;                      \
    void *(*key)(void *) = ht->t->__getKey;                                    \
    int (*keq)(const void *, const void *) = ht->t->__keyEqual;

void *fklGetHashItem(const void *pkey, const FklHashTable *ht) {
    HASH_FUNC_HEADER();

    for (FklHashTableItem *p = ht->base[hashv(pkey) & ht->mask]; p; p = p->ni)
        if (keq(pkey, key(p->data)))
            return p->data;
    return NULL;
}

FklHashTableItem **fklGetHashItemSlot(FklHashTable *ht, uintptr_t hashv) {
    return &ht->base[hashv & ht->mask];
}

#define REHASH()                                                               \
    if (isgreater((double)ht->num / ht->size, FKL_DEFAULT_HASH_LOAD_FACTOR))   \
        expandHashTable(ht);

static inline FklHashTableItem *createHashTableItem(size_t size,
                                                    FklHashTableItem *ni) {
    FklHashTableItem *node =
        (FklHashTableItem *)calloc(1, sizeof(FklHashTableItem) + size);
    FKL_ASSERT(node);
    node->ni = ni;
    return node;
}

static inline void putHashNode(FklHashTableItem *node, FklHashTable *ht) {
    void *pkey = ht->t->__getKey(node->data);
    FklHashTableItem **pp = &ht->base[ht->t->__hashFunc(pkey) & ht->mask];
    node->ni = *pp;
    *pp = node;
}

static inline uint32_t next_pow2(uint32_t n) {
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    return n + 1;
}

void fklClearHashTable(FklHashTable *ht) {
    void (*uninitFunc)(void *) = ht->t->__uninitItem;
    for (FklHashTableItem *list = ht->first; list;) {
        FklHashTableItem *cur = list;
        uninitFunc(cur->data);
        list = list->next;
        free(cur);
    }
    memset(ht->base, 0, sizeof(FklHashTableItem *) * ht->size);
    ht->num = 0;
    ht->first = NULL;
    ht->last = NULL;
    ht->size = DEFAULT_HASH_TABLE_SIZE;
    ht->mask = DEFAULT_HASH_TABLE_SIZE - 1;
}

#undef DEFAULT_HASH_TABLE_SIZE

static inline void expandHashTable(FklHashTable *table) {
    table->size = next_pow2(table->num);
    table->mask = table->size - 1;
    FklHashTableItem *list = table->first;
    free(table->base);
    FklHashTableItem **nbase =
        (FklHashTableItem **)calloc(table->size, sizeof(FklHashTableItem *));
    FKL_ASSERT(nbase);
    table->base = nbase;
    for (; list; list = list->next)
        putHashNode(list, table);
}

void fklRehashTable(FklHashTable *table) {
    FklHashTableItem *list = table->first;
    memset(table->base, 0, sizeof(FklHashTableItem *) * table->size);
    for (; list; list = list->next)
        putHashNode(list, table);
}

int fklDelHashItem(void *pkey, FklHashTable *ht, void *deleted) {
    HASH_FUNC_HEADER();
    FklHashTableItem **p = &ht->base[hashv(pkey) & ht->mask];
    for (; *p; p = &(*p)->ni)
        if (keq(pkey, key((*p)->data)))
            break;

    FklHashTableItem *item = *p;
    if (item) {
        *p = item->ni;
        if (item->prev)
            item->prev->next = item->next;
        if (item->next)
            item->next->prev = item->prev;
        if (ht->last == item)
            ht->last = item->prev;
        if (ht->first == item)
            ht->first = item->next;

        ht->num--;
        if (deleted)
            ht->t->__setVal(deleted, item->data);
        else {
            void (*uninitFunc)(void *) = ht->t->__uninitItem;
            uninitFunc(item->data);
        }
        free(item);
        return 1;
    }
    return 0;
}

void fklRemoveHashItem(FklHashTable *ht, FklHashTableItem *hi) {
    uintptr_t (*hashv)(const void *) = ht->t->__hashFunc;
    void *(*key)(void *) = ht->t->__getKey;
    void *data = hi->data;

    void *pkey = key(data);
    FklHashTableItem **p = &ht->base[hashv(pkey) & ht->mask];
    for (; *p; p = &(*p)->ni)
        if (*p == hi)
            break;

    if (*p) {
        *p = hi->ni;
        if (hi->prev)
            hi->prev->next = hi->next;
        if (hi->next)
            hi->next->prev = hi->prev;
        if (ht->last == hi)
            ht->last = hi->prev;
        if (ht->first == hi)
            ht->first = hi->next;
        ht->num--;
    }
}

void fklRemoveHashItemWithSlot(FklHashTable *ht, FklHashTableItem **p) {
    FklHashTableItem *item = *p;
    if (item) {
        *p = item->ni;
        if (item->prev)
            item->prev->next = item->next;
        if (item->next)
            item->next->prev = item->prev;
        if (ht->last == item)
            ht->last = item->prev;
        if (ht->first == item)
            ht->first = item->next;

        ht->num--;
        void (*uninitFunc)(void *) = ht->t->__uninitItem;
        uninitFunc(item->data);
        free(item);
    }
}

void *fklPutHashItemInSlot(FklHashTable *ht, FklHashTableItem **pp) {
    FklHashTableItem *node = createHashTableItem(ht->t->size, *pp);
    *pp = node;
    if (ht->first) {
        node->prev = ht->last;
        ht->last->next = node;
    } else
        ht->first = node;
    ht->last = node;
    ht->num++;
    REHASH();
    return node->data;
}

void *fklPutHashItem(const void *pkey, FklHashTable *ht) {
    HASH_FUNC_HEADER();

    FklHashTableItem **pp = &ht->base[hashv(pkey) & ht->mask];
    void *d1 = NULL;
    for (FklHashTableItem *pn = *pp; pn; pn = pn->ni)
        if (keq(key(pn->data), pkey))
            d1 = pn->data;
    if (!d1) {
        FklHashTableItem *node = createHashTableItem(ht->t->size, *pp);
        ht->t->__setKey(key(node->data), pkey);
        *pp = node;
        if (ht->first) {
            node->prev = ht->last;
            ht->last->next = node;
        } else
            ht->first = node;
        ht->last = node;
        ht->num++;
        d1 = node->data;
        REHASH();
    }
    return d1;
}

void *fklGetOrPutWithOtherKey(void *pkey, uintptr_t (*hashv)(const void *key),
                              int (*keq)(const void *k0, const void *k1),
                              void (*setVal)(void *d0, const void *d1),
                              FklHashTable *ht) {
    void *(*key)(void *) = ht->t->__getKey;
    FklHashTableItem **pp = &ht->base[hashv(pkey) & ht->mask];
    void *d1 = NULL;
    for (FklHashTableItem *pn = *pp; pn; pn = pn->ni)
        if (keq(key(pn->data), pkey)) {
            d1 = pn->data;
            break;
        }
    if (!d1) {
        FklHashTableItem *node = createHashTableItem(ht->t->size, *pp);
        setVal(node->data, pkey);
        d1 = node->data;
        *pp = node;
        if (ht->first) {
            node->prev = ht->last;
            ht->last->next = node;
        } else
            ht->first = node;
        ht->last = node;
        ht->num++;
        REHASH();
    }
    return d1;
}

void *fklGetOrPutHashItem(void *data, FklHashTable *ht) {
    HASH_FUNC_HEADER();
    void *pkey = key(data);
    FklHashTableItem **pp = &ht->base[hashv(pkey) & ht->mask];
    void *d1 = NULL;
    for (FklHashTableItem *pn = *pp; pn; pn = pn->ni)
        if (keq(key(pn->data), pkey)) {
            d1 = pn->data;
            break;
        }
    if (!d1) {
        FklHashTableItem *node = createHashTableItem(ht->t->size, *pp);
        ht->t->__setVal(node->data, data);
        d1 = node->data;
        *pp = node;
        if (ht->first) {
            node->prev = ht->last;
            ht->last->next = node;
        } else
            ht->first = node;
        ht->last = node;
        ht->num++;
        REHASH();
    }
    return d1;
}

int fklPutHashItem2(FklHashTable *ht, const void *pkey, void **pitem) {
    HASH_FUNC_HEADER();
    FklHashTableItem **pp = &ht->base[hashv(pkey) & ht->mask];
    void *d1 = NULL;
    for (FklHashTableItem *pn = *pp; pn; pn = pn->ni)
        if (keq(key(pn->data), pkey)) {
            d1 = pn->data;
            break;
        }
    if (!d1) {
        FklHashTableItem *node = createHashTableItem(ht->t->size, *pp);
        ht->t->__setKey(node->data, pkey);
        d1 = node->data;
        *pp = node;
        if (ht->first) {
            node->prev = ht->last;
            ht->last->next = node;
        } else
            ht->first = node;
        ht->last = node;
        ht->num++;
        REHASH();
        if (pitem)
            *pitem = d1;
        return 0;
    } else {
        if (pitem)
            *pitem = d1;
        return 1;
    }
}

void *fklHashDefaultGetKey(void *i) { return i; }

#undef REHASH
#undef HASH_FUNC_HEADER

void fklUninitHashTable(FklHashTable *table) {
    void (*uninitFunc)(void *) = table->t->__uninitItem;
    FklHashTableItem *list = table->first;
    while (list) {
        FklHashTableItem *cur = list;
        uninitFunc(cur->data);
        list = list->next;
        free(cur);
    }
    free(table->base);
}

void fklDestroyHashTable(FklHashTable *table) {
    fklUninitHashTable(table);
    free(table);
}

FklBytevector *fklCreateBytevector(size_t size, const uint8_t *ptr) {
    FklBytevector *tmp =
        (FklBytevector *)malloc(sizeof(FklBytevector) + size * sizeof(uint8_t));
    FKL_ASSERT(tmp);
    tmp->size = size;
    if (ptr)
        memcpy(tmp->ptr, ptr, size);
    return tmp;
}

FklBytevector *fklBytevectorRealloc(FklBytevector *bvec, size_t new_size) {
    FklBytevector *new = (FklBytevector *)fklRealloc(
        bvec, sizeof(FklBytevector) + new_size * sizeof(uint8_t));
    FKL_ASSERT(new);
    return new;
}

void fklBytevectorCat(FklBytevector **a, const FklBytevector *b) {
    size_t aSize = (*a)->size;
    FklBytevector *prev = *a;
    prev = (FklBytevector *)fklRealloc(
        prev, sizeof(FklBytevector) + (aSize + b->size) * sizeof(char));
    FKL_ASSERT(prev);
    *a = prev;
    prev->size = aSize + b->size;
    memcpy(prev->ptr + aSize, b->ptr, b->size);
}

int fklBytevectorCmp(const FklBytevector *fir, const FklBytevector *sec) {
    size_t size = fir->size < sec->size ? fir->size : sec->size;
    int r = memcmp(fir->ptr, sec->ptr, size);
    if (r)
        return r;
    else if (fir->size < sec->size)
        return -1;
    else if (fir->size > sec->size)
        return 1;
    return r;
}

int fklBytevectorEqual(const FklBytevector *fir, const FklBytevector *sec) {
    if (fir->size == sec->size)
        return !memcmp(fir->ptr, sec->ptr, fir->size);
    return 0;
}

void fklPrintRawBytevector(const FklBytevector *bv, FILE *fp) {
#define SE ('"')
    fputs("#\"", fp);
    const uint8_t *const end = bv->ptr + bv->size;
    for (const uint8_t *c = bv->ptr; c < end; c++) {
        uint8_t ch = *c;
        if (ch == SE) {
            putc('\\', fp);
            putc(SE, fp);
        } else if (ch == '\\')
            fputs("\\\\", fp);
        else if (isgraph(ch))
            putc(ch, fp);
        else if (fklIsSpecialCharAndPrint(ch, fp))
            ;
        else
            fprintf(fp, "\\x%02X", ch);
    }

    fputc('"', fp);
}

void fklPrintBytevectorToStringBuffer(FklStringBuffer *s,
                                      const FklBytevector *bvec) {
    fklStringBufferConcatWithCstr(s, "#\"");
    const uint8_t *const end = bvec->ptr + bvec->size;
    for (const uint8_t *c = bvec->ptr; c < end; c++) {
        uint8_t ch = *c;
        if (ch == SE) {
            fklStringBufferPutc(s, '\\');
            fklStringBufferPutc(s, SE);
        } else if (ch == '\\')
            fklStringBufferConcatWithCstr(s, "\\\\");
        else if (isgraph(ch))
            fklStringBufferPutc(s, ch);
        else if (fklIsSpecialCharAndPrintToStringBuffer(s, ch))
            ;
        else
            fklStringBufferPrintf(s, "\\x%02X", ch);
    }

    fklStringBufferPutc(s, '"');
#undef SE
}

FklString *fklBytevectorToString(const FklBytevector *bv) {
    FklStringBuffer buf = FKL_STRING_BUFFER_INIT;
    fklInitStringBuffer(&buf);
    fklPrintBytevectorToStringBuffer(&buf, bv);
    FklString *r = fklStringBufferToString(&buf);
    fklUninitStringBuffer(&buf);
    return r;
}

FklBytevector *fklCopyBytevector(const FklBytevector *obj) {
    if (obj == NULL)
        return NULL;
    FklBytevector *tmp =
        (FklBytevector *)malloc(sizeof(FklBytevector) + obj->size);
    FKL_ASSERT(tmp);
    memcpy(tmp->ptr, obj->ptr, obj->size);
    tmp->size = obj->size;
    return tmp;
}

uintptr_t fklBytevectorHash(const FklBytevector *bv) {
    uintptr_t h = 0;
    const uint8_t *val = bv->ptr;
    size_t size = bv->size;
    for (size_t i = 0; i < size; i++)
        h = 31 * h + val[i];
    return h;
}

static char *string_buffer_alloc(void *ptr, size_t len) {
    FklStringBuffer *buf = ptr;
    fklStringBufferReverse(buf, len + 1);
    buf->index = len;
    char *body = fklStringBufferBody(buf);
    body[len] = '\0';
    return body;
}

size_t fklBigIntToStringBuffer(const FklBigInt *a, FklStringBuffer *buf,
                               uint8_t radix, FklBigIntFmtFlags flags) {
    return fklBigIntToStr(a, string_buffer_alloc, buf, radix, flags);
}

static char *string_alloc_callback(void *ptr, size_t len) {
    FklString **pstr = ptr;
    FklString *str = fklCreateString(len, NULL);
    *pstr = str;
    return str->str;
}

FklString *fklBigIntToString(const FklBigInt *a, uint8_t radix,
                             FklBigIntFmtFlags flags) {
    FklString *str = NULL;
    fklBigIntToStr(a, string_alloc_callback, &str, radix, flags);
    return str;
}

static const FklBigIntDigit FKL_BIGINT_FIX_MAX_DIGITS[2] = {
    0xffffffff & FKL_BIGINT_DIGIT_MASK,
    0xffffffff & FKL_BIGINT_DIGIT_MASK,
};

static const FklBigIntDigit FKL_BIGINT_FIX_MIN_DIGITS[3] = {
    0x0,
    0x0,
    0x1,
};

static const FklBigInt FKL_BIGINT_FIX_MAX = {
    .digits = (FklBigIntDigit *)FKL_BIGINT_FIX_MAX_DIGITS,
    .num = 2,
    .size = 2,
    .const_size = 1,
};
static const FklBigInt FKL_BIGINT_FIX_MIN = {
    .digits = (FklBigIntDigit *)FKL_BIGINT_FIX_MIN_DIGITS,
    .num = -3,
    .size = 3,
    .const_size = 1,
};

static const FklBigInt FKL_BIGINT_SUB_1_MAX = {
    .digits = (FklBigIntDigit *)FKL_BIGINT_FIX_MIN_DIGITS,
    .num = 3,
    .size = 3,
    .const_size = 1,
};
static const FklBigInt FKL_BIGINT_ADD_1_MIN = {
    .digits = (FklBigIntDigit *)FKL_BIGINT_FIX_MAX_DIGITS,
    .num = -2,
    .size = 2,
    .const_size = 1,
};

int fklIsBigIntGtLtFix(const FklBigInt *a) {
    return fklBigIntCmp(a, &FKL_BIGINT_FIX_MAX) > 0
        || fklBigIntCmp(a, &FKL_BIGINT_FIX_MIN) < 0;
}

int fklIsBigIntAdd1InFixIntRange(const FklBigInt *a) {
    return fklBigIntEqual(a, &FKL_BIGINT_ADD_1_MIN);
}

int fklIsBigIntSub1InFixIntRange(const FklBigInt *a) {
    return fklBigIntEqual(a, &FKL_BIGINT_SUB_1_MAX);
}
