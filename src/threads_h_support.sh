#!/bin/sh
set -x

# If the compiler and linker support C11-style <threads.h>, this will print 1.
# Otherwise it will print 0.

# Try to compile and link a C file with <threads.h>, then check if it
# succeeded. It will compile if the system has C11-compatible threads.h. (Note
# that not all C11 compilers have threads.h. Also, some non-C11 compilers do
# have threads.h.)
echo "#include <threads.h>
int main() {
    mtx_t mutex;
    mtx_init(&mutex, mtx_plain);
    return 0;
}" | \
    ${CC} ${CPPFLAGS} ${PKG_CFLAGS} ${CFLAGS} ${PKG_LIBS}  -x c - -o /dev/null > /dev/null 2>&1

if [ "$?" -eq 0 ]; then
    echo 1
else
    echo 0
fi
