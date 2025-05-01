
SRCS := $(wildcard *.c)
OBJS := $(SRCS:.c=.o)
HDRS := $(wildcard *.h)

CFLAGS := -g
LDLIBS := 

mkfs: $(OBJS)
	gcc $(CLFAGS) -o mkfs mkfs.o pages.o bitmap.o inode.o directory.o hash.o libmkfs.o $(LDLIBS)
	
read_root: $(OBJS)
	gcc $(CLFAGS) -o read_root read_root.o pages.o directory.o bitmap.o inode.o hash.o libmkfs.o $(LDLIBS)
	
lookup: $(OBJS)
	gcc $(CLFAGS) -o lookup lookup.o pages.o directory.o bitmap.o inode.o hash.o libmkfs.o $(LDLIBS)

%.o: %.c $(HDRS)
	gcc $(CFLAGS) -c -o $@ $<

clean: unmount
	rm -f nufs *.o test.log data.nufs mkfs read_root lookup
	rmdir mnt || true

test: nufs
	perl test.pl

open:
	gedit *.c *.h

.PHONY: clean mount unmount gdb

