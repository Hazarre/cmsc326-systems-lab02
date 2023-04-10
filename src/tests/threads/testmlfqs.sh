#!/bin/bash

for name in \
mlfqs2-fifo mlfqs2-longproc mlfqs2-shortlong  ; do
    /bin/rm build/tests/threads/$name.result;
    make build/tests/threads/$name.result;
done
