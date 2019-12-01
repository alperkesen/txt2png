gcc txt2png.c -o txt2png -Wall -ansi -W -std=c99 -g -ggdb -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -lfuse
./txt2png $(pwd)/src $(pwd)/dst
 
