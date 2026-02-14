#ifndef FLEX_ARRAY_H
#define FLEX_ARRAY_H

#include <stdlib.h>

#define DEFINE_FLEX_ARRAY(Type, Name) \
    typedef struct { \
        uint32_t count; \
        uint32_t capacity; \
        Type data[]; \
    } Name; \
    \
    static inline Name* Name##_create(int initial_cap) { \
        int cap = initial_cap > 0 ? initial_cap : 8; \
        Name* arr = (Name*)malloc(sizeof(Name) + (cap * sizeof(Type))); \
        arr->count = 0; \
        arr->capacity = cap; \
        return arr; \
    } \
    \
    static inline Name* Name##_push(Name* arr, Type item) { \
        if (arr->count >= arr->capacity) { \
            arr->capacity *= 2; \
            arr = (Name*)realloc(arr, sizeof(Name) + (arr->capacity * sizeof(Type))); \
        } \
        arr->data[arr->count++] = item; \
        return arr; \
    } \
    \
    static inline void Name##_free(Name* arr) { \
        free(arr); \
    } \

#endif // !FLEX_ARRAY_H
