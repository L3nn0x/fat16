#include "vector.h"
#include <stdlib.h>
#include <stdint.h>

size_t size_deque(Deque* d) {
    return d->end - d->start;
}

void** front_deque(Deque* d) {
    return d->data + d->start;
}

void** back_deque(Deque* d) {
    return d->data + d->end;
}

void** begin_deque(Deque* d) {
    return d->data + d->start;
}

void** end_deque(Deque* d) {
    return d->data + d->end;
}

void** rbegin_deque(Deque* d) {
    return d->data + d->end;
}

void** rend_deque(Deque* d) {
    return d->data + d->start - 1;
}

void* const* cbegin_deque(const Deque* d) {
    return d->data + d->start;
}

void* const* cend_deque(const Deque* d) {
    return d->data + d->end;
}

void* const* crbegin_deque(const Deque* d) {
    return d->data + d->end;
}

void* const* crend_deque(const Deque* d) {
    return d->data + d->start - 1;
}

Deque* create_deque() {
    Deque* res = malloc(sizeof(Deque));
    res->data = 0;
    res->start = 0;
    res->end = 0;
    res->allocated = 0;
    return res;
}

void delete_deque(Deque** d, void(*_free)(void*)) {
    if (!d) return;
    if ((*d)->data) {
        if (_free) {
            void** it;
            for (it = begin_deque(*d); it != end_deque(*d); ++it) {
                _free(*it);
            }
        }
        free((*d)->data);
    }
    free(*d);
    *d = 0;
}

void push_back_deque(Deque* d, void* s) {
    if (d->allocated > 0 && d->end + 1 < d->allocated - 1) {
        d->data[d->end++] = s;
    } else {
        if (d->allocated == 0) {
            d->allocated = 8;
            d->start = d->end = 3;
        } else {
            d->allocated *= 2;
        }
        d->data = realloc(d->data, d->allocated * sizeof(void*));
        push_back_deque(d, s);
    }
}

void push_front_deque(Deque* d, void* s) {
    if (d->start > 1) {
        d->data[--d->start] = s;
    } else {
        if (d->allocated == 0) {
            d->allocated = 8;
            d->start = d->end = 3;
        } else {
            d->allocated *= 2;
        }
        d->data = realloc(d->data, d->allocated * sizeof(void*));
        size_t size = size_deque(d);
        if (size == 0) {
            push_front_deque(d, s);
            return;
        }
        size /= 2;
        void **it, **jt;
        for (it = rbegin_deque(d), jt = rbegin_deque(d) + size; it != rend_deque(d); --it, --jt) {
            *jt = *it;
        }
        push_front_deque(d, s);
    }
}

void* pop_back_deque(Deque* d) {
    if (size_deque(d) == 0) return 0;
    return d->data[--d->end];
}

void* pop_front_deque(Deque* d) {
    if (size_deque(d) == 0) return 0;
    return d->data[d->start++];
}

void* get_elem_deque(Deque* d, size_t index) {
    return d->data[d->start + index];
}

void delete_elem_deque(Deque* d, size_t index, void(*_free)(void*)) {
    _free(d->data[d->start + index]);
    memmove(&(d->data[d->start + index]), &(d->data[d->start + index + 1]), d->end - (d->start + index + 1));
    pop_back_deque(d);
}

void reverse_deque(void** first, void** last) {
    while ((first != last) && (first != --last)) {
        *first = (void*)((uintptr_t)*first ^ (uintptr_t)*last);
        *last = (void*)((uintptr_t)*first ^ (uintptr_t)*last);
        *first = (void*)((uintptr_t)*last ^ (uintptr_t)*first);
        ++first;
    }
}
