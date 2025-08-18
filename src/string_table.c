#include <fakeLisp/string_table.h>

void fklStrKeyFree(FklString *s) { fklZfree(s); }

void fklInitStringTable(FklStringTable *tmp) { fklStrHashSetInit(tmp); }

FklStringTable *fklCreateStringTable() {
    FklStringTable *tmp = (FklStringTable *)fklZmalloc(sizeof(FklStringTable));
    FKL_ASSERT(tmp);
    fklInitStringTable(tmp);
    return tmp;
}

const FklString *fklAddString(FklStringTable *st, const FklString *s) {
    return fklAddStringCharBuf(st, s->str, s->size);
}

const FklString *fklAddStringCstr(FklStringTable *st, const char *s) {
    return fklAddStringCharBuf(st, s, strlen(s));
}

const FklString *
fklAddStringCharBuf(FklStringTable *ht, const char *buf, size_t len) {
    uintptr_t hashv = fklCharBufHash(buf, len);
    FklStrHashSetNode *const *bkt = fklStrHashSetBucket(ht, hashv);
    for (; *bkt; bkt = &(*bkt)->bkt_next) {
        FklStrHashSetNode *cur = *bkt;
        if (!fklStringCharBufCmp(cur->k, len, buf))
            return cur->k;
    }

    FklStrHashSetNode *node =
            fklStrHashSetCreateNode2(hashv, fklCreateString(len, buf));
    fklStrHashSetInsertNode(ht, node);
    return node->k;
}

const FklString *add_string(FklStringTable *ht, FklString *str) {
    uintptr_t hashv = fklStringHash(str);
    FklStrHashSetNode *const *bkt = fklStrHashSetBucket(ht, hashv);
    for (; *bkt; bkt = &(*bkt)->bkt_next) {
        FklStrHashSetNode *cur = *bkt;
        if (!fklStringCmp(cur->k, str)) {
            fklZfree(str);
            return cur->k;
        }
    }

    FklStrHashSetNode *node = fklStrHashSetCreateNode2(hashv, str);
    fklStrHashSetInsertNode(ht, node);
    return node->k;
}

void fklUninitStringTable(FklStringTable *table) { fklStrHashSetUninit(table); }

void fklClearStringTable(FklStringTable *t) { fklStrHashSetClear(t); }

void fklDestroyStringTable(FklStringTable *table) {
    fklUninitStringTable(table);
    fklZfree(table);
}

void fklWriteStringTable(const FklStringTable *t, FILE *fp) {
    uint32_t count = t->count;
    fwrite(&count, sizeof(count), 1, fp);
    for (const FklStrHashSetNode *cur = t->first; cur; cur = cur->next) {
        fwrite(cur->k, cur->k->size + sizeof(cur->k->size), 1, fp);
    }
}

void fklLoadStringTable(FILE *fp, FklStringTable *table) {
    uint32_t count = 0;
    fread(&count, sizeof(count), 1, fp);
    for (uint32_t i = 0; i < count; ++i) {
        uint64_t len = 0;
        fread(&len, sizeof(len), 1, fp);
        FklString *buf = fklCreateString(len, NULL);
        fread(buf->str, len, 1, fp);
        add_string(table, buf);
    }
}
