gcc txt2png.c -o txt2png -Wall -ansi -W -std=c99 -g -ggdb -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -lfuse -lmagic
./txt2png -d $(pwd)/src $(pwd)/dst
 
