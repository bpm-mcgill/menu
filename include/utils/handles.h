#ifndef HANDLES_H
#define HANDLES_H

/*
 * This is used for generating handles with generation tracking
 * 
 * HANDLE ID explanation:
 * A handle id stores both an index and a generation number
 *
 * The index:
 *      The index is the index in a flex_array that the handle points to
 *      The first 20 bits are the handle index | 16 bit example: 1000    000000000100
 *                                                               gen(8)  index(4)
 * The generation:
 *      This is a counter that is stored in an element in the flex_array
 *      Whenever an element is "removed", it is really just marked as inactive and kept there
 *      If this slot is then ever reused for a new resource,
 *        the generation number is incremented.
 * 
 * Slot reusage:
 *      In the case of the a slot being reused, a new handle is created for this element.
 *        If an old handle tries to get the element, the generations between the old and
 *         new element will be different meaning the element at the old generation's 
 *         index has been deleted and the slot was reused. Whatever resource the old handle
 *         was pointing to is now gone and replaced.
 *
 * This system assumes a lot of shit, and so it should only be used internally
 *
 * Specifically, this was created for handling resources, like Shaders and Textures
*/

/*
 * Assuming Name is the type the handle is used for
 * Assuming that type stores an `id` and `gen` fields that are uint32_t
 *
 * The first 20 bits in the uint32_t is used for storing the actual handle id
 * The last 12 bits are used to hold the generation number
*/
#define DECLARE_HANDLE(Name) \
    typedef struct { uint32_t id; } Name; \
    static inline uint32_t Name##_index(Name h) { return h.id & 0xFFFFF; } \
    static inline uint32_t Name##_gen(Name h)   { return h.id >> 20; } \
    static inline bool Name##_is_null(Name h)         { return h.id == 0; } \
    static inline Name Name##_null()            { return (Name){0}; } \
    static inline Name Name##_pack(uint32_t idx, uint32_t gen) { \
        return (Name){ .id = (gen << 20) | (idx & 0xFFFFF) }; \
    }

/*
 * Assuming ArrayPtr is a flex_array
 *   - Assuming ArrayPtr stores a fallback at the first index
 * Assuming HandleType is a type defined with the `DECLARE_HANDLE` macro
*/
// PERF: idfk
#define DEFINE_RESOURCE_GETTER(ReturnType, HandleType, ArrayPtr, LogName) \
    ReturnType* ReturnType##_get(HandleType h) { \
        uint32_t index = HandleType##_index(h); \
        uint32_t gen   = HandleType##_gen(h); \
        \
        if (index >= (ArrayPtr)->count) { \
            printf("[%s] ERROR: Handle index %u out of bounds. Returning fallback.\n", LogName, index); \
            return &(ArrayPtr)->data[0]; \
        } \
        ReturnType* item = &(ArrayPtr)->data[index]; \
        \
        if (index == 0) return item; \
        \
        if (!item->active || item->generation != gen) { \
            printf("[%s] WARNING: Accessed deleted resource handle (Idx: %u, Gen %u). Returning fallback.", LogName, index, gen); \
            return &(ArrayPtr)->data[0]; \
        } \
        \
        return item; \
    }

#endif // !HANDLES_H
