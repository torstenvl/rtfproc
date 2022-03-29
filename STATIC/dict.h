#ifndef DICT_H__
#define DICT_H__
#include <stdlib.h>
#include <string.h>



typedef struct dictentry {
    int64_t k;
    int64_t v;
} dictentry;

typedef struct dict {
    size_t n;
    size_t m;
    dictentry *kv;
} dict;

typedef struct sdictentry {
    char *k;
    char *v;
} sdictentry;

typedef struct sdict {
    size_t n;
    size_t m;
    sdictentry *kv;
} sdict;




#define dictdestroy(D) dictdestroy_(&D)
#define sdictdestroy(D) sdictdestroy_(&D)




// Return the (hypothetical) index of an element via binary search
static int64_t dicthypoindex(dict *D, int64_t key) {
    int64_t min = 0;
    int64_t max = D->n;
    int64_t mid = min + (max-min)/2;

    while (min < max) {
        if (key == D->kv[mid].k) {  // Midpoint of search space is right on
            return mid;
        } else if (key < D->kv[mid].k) { // Too high, narrow to lower half
            if (mid>min) max = mid-1; else max = min;
            mid = min + (max-min)/2;
        } else if (D->kv[mid].k < key) { // Too low, narrow to upper half
            if (mid<max) min = mid+1; else min = max;
            mid = min + (max-min)/2;
        }
    }

    while (mid < D->n && key > D->kv[mid].k) mid++;

    return mid;
}

static int64_t sdicthypoindex(sdict *D, const char *key) {
    int64_t min = 0;
    int64_t max = D->n;
    int64_t mid = min + (max-min)/2;
    int cmp;

    while (min < max) {
        cmp = strcmp(key, D->kv[mid].k);
        if (cmp == 0) {  // Midpoint of search space is right on
            return mid;
        } else if (cmp < 0) { // Too high, narrow to lower half
            if (mid>min) max = mid-1; else max = min;
            mid = min + (max-min)/2;
        } else if (0 < cmp) { // Too low, narrow to upper half
            if (mid<max) min = mid+1; else min = max;
            mid = min + (max-min)/2;
        }
    }

    while (mid < D->n && strcmp(key, D->kv[mid].k) > 0) mid++;

    return mid;
}




// Allocate more memory if we've used it all
static void dictmaybegrow(dict *D) {
    dictentry *allocation;

    if (D->n >= D->m) { // n (number of elements) >= m (memory allocated)
        // Try reallocating more memory
        allocation = realloc(D->kv, D->m * 2 * sizeof(dictentry));
        // If successful, use new allocation (and track how much we now have)
        if (allocation) {
            D->kv = allocation;
            D->m = D->m * 2;
        }
    }
}
static void sdictmaybegrow(sdict *D) {
    sdictentry *allocation;

    if (D->n >= D->m) { // n (number of elements) >= m (memory allocated)
        // Try reallocating more memory
        allocation = realloc(D->kv, D->m * 2 * sizeof(sdictentry));
        // If successful, use new allocation (and track how much we now have)
        if (allocation) {
            D->kv = allocation;
            D->m = D->m * 2;
        }
    }
}




// Allocate and initialize a new dictionary
static inline dict *dictnew(void) {
    dict *D = malloc(sizeof *D);

    if (D) {
        D->n = 0;  D->m = 8;
        D->kv = malloc(D->m * sizeof(dictentry));
        if (!D->kv)  {  free(D);  D = NULL;  }
    }

    return D;
}

static inline sdict *sdictnew(void) {
    sdict *D = malloc(sizeof *D);

    if (D) {
        D->n = 0;  D->m = 8;
        D->kv = malloc(D->m * sizeof(sdictentry));
        if (!D->kv)  {  free(D);  D = NULL;  }
    }

    return D;
}




static inline int dictadd(dict *D, int64_t key, int64_t val) {
    int64_t i = dicthypoindex(D, key);
    int64_t dst = i + 1;

    if (D->n != 0 && key != 0 && D->kv[i].k == key) {
        D->kv[i].v = val;
    } else {
        dictmaybegrow(D);
        // From the insertion points, move everything down to make room.
        memmove(&D->kv[dst], &D->kv[i], (D->n - (size_t)i) * sizeof(dictentry));
        D->kv[i].k = key;
        D->kv[i].v = val;
        D->n++;
    }

    return 1;
}

static inline int sdictadd(sdict *D, const char *key, const char *val) {
    int64_t i = sdicthypoindex(D, key);
    int64_t dst = i + 1;
    char *tmpkey;
    char *tmpval;

    if (D->n != 0 && key != NULL && !strcmp(D->kv[i].k, key)) {
        tmpval = strdup(val);
        if (!tmpval) return 0;
        free(D->kv[i].v);
        D->kv[i].v = tmpval;
    } else {
        sdictmaybegrow(D);
        tmpkey = strdup(key);
        tmpval = strdup(val);
        if (!tmpkey || !tmpval) { free(tmpkey); free(tmpval); return 0; }
        // From the insertion points, move everything down to make room.
        memmove(&D->kv[dst], &D->kv[i], (D->n - (size_t)i) * sizeof(sdictentry));
        D->kv[i].k = tmpkey;
        D->kv[i].v = tmpval;
        D->n++;
    }

    return 1;
}




static inline int64_t dictval(dict *D, int64_t key) {
    int64_t i = dicthypoindex(D, key);
    if (i < D->n && D->kv[i].k == key) return D->kv[i].v;
    return 0;
}

static inline char *sdictval(sdict *D, const char *key) {
    char *tmpval;
    int64_t i = sdicthypoindex(D, key);
    if (i < D->n && !strcmp(D->kv[i].k, key)) {
        tmpval = strdup(D->kv[i].v);
        if (tmpval) return tmpval;
    }
    return NULL;
}




static inline void dictdel(dict *D, int64_t key) {
    int64_t i = dicthypoindex(D, key);
    int64_t src = i + 1;

    if (D->kv[i].k == key) {
        memmove(&D->kv[i], &D->kv[src], (D->n - (size_t)src) * sizeof(dictentry));
        D->n--;
    }
}

static inline void sdictdel(sdict *D, const char *key) {
    int64_t i = sdicthypoindex(D, key);
    int64_t src = i + 1;

    if (!strcmp(D->kv[i].k, key)) {
        free(D->kv[i].k);
        free(D->kv[i].v);
        memmove(&D->kv[i], &D->kv[src], (D->n - (size_t)src) * sizeof(sdictentry));
        D->n--;
    }
}




static inline void dictdestroy_(dict **d) {
    dict *D = *d;
    free(D->kv);
    free(D);
    D = NULL;
}
static inline void sdictdestroy_(sdict **d) {
    int64_t i;
    sdict *D = *d;
    for (i=0; i<D->n; i++) {
        free(D->kv[i].k);
        free(D->kv[i].v);
    }
    free(D->kv);
    free(D);
    D = NULL;
}

#endif
