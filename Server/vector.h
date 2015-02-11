#ifndef VECTOR_H
#define VECTOR_H

#define VECTOR_INIT_CAPACITY 10

typedef int cmp(void *item, void *pattern);
typedef void actions(void *item, void *data);

#define VECTOR_INIT(vec) vector vec; vector_init(&vec)
#define VECTOR_ADD(vec, item) vector_add(&vec, (void *) item)
#define VECTOR_SET(vec, id, item) vector_set(&vec, id, (void *) item)
#define VECTOR_GET(vec, type, id) (type) vector_get(&vec, id)
#define VECTOR_DELETE(vec, id) vector_delete(&vec, id)
#define VECTOR_TOTAL(vec) vector_total(&vec)
#define VECTOR_FREE(vec) vector_free(&vec)

typedef struct vector {
    void **items;
    int capacity;
    int total;
} vector;

vector* vector_create(void);
int vector_total(vector *);
static void vector_resize(vector *, int);
void vector_add(vector *, void *);
void vector_set(vector *, int, void *);
void *vector_get(vector *, int);
void* vector_delete(vector *, int);
int vector_is_contains(vector *v, void *pattern, cmp *cmp_func);
void* vector_delete_first_equal(vector *v, void *pattern, cmp *cmp_func);
void* vector_get_first_equal(vector *v, void *pattern, cmp *cmp_func);
void vector_foreach(vector *v, void *data, actions *func);
void vector_free(vector *);

#endif // VECTOR_H
