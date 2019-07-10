project(kernel)

add_library(kernel
    fat16.h
    fat16.c
    file.h
    file.c
    list.h
    vector.h
    vector.c
    my_unistd.h
    my_unistd.c
)
