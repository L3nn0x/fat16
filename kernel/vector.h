#ifndef VECTOR
#define VECTOR

#include <string.h>

typedef struct Deque {
    void** data;
    size_t start;
    size_t end;
    size_t allocated;
} Deque;

size_t size_deque(Deque* d);
void** front_deque(Deque* d);
void** back_deque(Deque* d);
void** begin_deque(Deque* d);
void** end_deque(Deque* d);
void** rbegin_deque(Deque* d);
void** rend_deque(Deque* d);
void* const* cbegin_deque(const Deque* d);
void* const* cend_deque(const Deque* d);
void* const* crbegin_deque(const Deque* d);
void* const* crend_deque(const Deque* d);
Deque* create_deque();
void delete_deque(Deque** d, void(*_free)(void*));
void push_back_deque(Deque* d, void* s);
void push_front_deque(Deque* d, void* s);
void* pop_back_deque(Deque* d);
void* pop_front_deque(Deque* d);
void* get_elem_deque(Deque* d, size_t index);
void delete_elem_deque(Deque* d, size_t index, void(*_free)(void*));
void reverse_deque(void** first, void** last);

#endif
