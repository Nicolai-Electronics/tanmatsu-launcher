
// SPDX-License-Identifier: MIT

#include "arrays.h"

// Get the address of an entry.
static inline void const* array_index_const(void const* array, size_t ent_size, size_t index) {
    return (void*)((size_t)array + ent_size * index);
}

// Binary search for a value in a sorted (ascending order) array.
array_binsearch_t array_binsearch(void const* array, size_t ent_size, size_t ent_count, void const* value,
                                  array_sort_comp_t comparator) {
    size_t ent_start = 0;
    while (ent_count > 0) {
        size_t midpoint = ent_count >> 1;
        int    res      = comparator(array_index_const(array, ent_size, midpoint), value);
        if (res > 0) {
            // The value is to the left of the midpoint.
            ent_count = midpoint;

        } else if (res < 0) {
            // The value is to the right of the midpoint.
            ent_start += midpoint + 1;
            ent_count -= midpoint + 1;
            array      = array_index_const(array, ent_size, midpoint + 1);

        } else /* res == 0 */ {
            // The value was found.
            return (array_binsearch_t){ent_start + midpoint, true};
        }
    }
    // The value was not found.
    return (array_binsearch_t){ent_start, false};
}
