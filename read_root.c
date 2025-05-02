#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "pages.h"
#include "inode.h"
#include "bitmap.h"
#include "directory.h"

dirent stop;

char *split(const char *path, int n) {
	int rv=0;
	char *splt = (char*)calloc(DIR_NAME, sizeof(char));
	if (n==0) {
		strcpy(splt, "/");
	} else {
		int c=0, i=0;
		for (; path[i] && c<n+1; i++) {
			splt[i]=path[i];
			if (path[i]=='/') c++;
		}
		if (splt[i-1]=='/') splt[i-1]='\0';
	}
	return splt;
}

int
count_l(const char *path) {
	int c=0;
	for(int i=0; path[i]; i++) {
		if (path[i]=='/') c++;
	}
	return c;
}

int readdir()
{
    int rv=0;
    
    inode* n = get_inode(0);
    dirent *e0;
    dirent *e1;
    
    while (true) {
    	e0 = (dirent*)((char*)get_root_start()+n->ptrs[0]);
    	e1 = (dirent*)((char*)get_root_start()+n->ptrs[1]);
    	if (!strcmp(e0->name, "*")) break;
    	printf("%s\n", e0->name);	// getaddr
    	if (!strcmp(e1->name, "*")) break;
    	printf("%s\n", e1->name);
    	if (n->iptr != 0) n = get_inode(n->iptr);
    	else return 0;
    }
    printf("readdir(%d)\n", rv);
}

int
mknod(const char *path, int mode)
{
    int rv = 0;
    int count = 0;
    int l = inode_find(path);
    inode *n = get_inode(1);
    inode *p = get_inode(0);	// TODO: Get this to point to sub-directories...have to reimplement directories... <- parent directory
    inode *h = get_inode(l);
    dirent *p0, *p1, *w;
    dirent data;
    data.inum=l;
    strcpy(data.name, path);
    h->mode=mode;
    data.active=true;
mk_loop:
	p0 = (dirent*)((char*)get_root_start()+p->ptrs[0]);
	p1 = (dirent*)((char*)get_root_start()+p->ptrs[1]);
	w = (dirent*)((char*)get_root_start()+n->ptrs[0]);
	if (!strcmp(p0->name, "*")) {
		//printf("found stop!\n");
		strcpy(stop.name, "*");
		memcpy(p0, &data, sizeof(data));
		p->ptrs[1] = n->ptrs[0];
		n->ptrs[0] += sizeof(stop);
		memcpy(w, &stop, sizeof(stop));	
	} else if (p0->active==false) {
		memcpy(p0, &data, sizeof(data));
	} else if (p1->active==false) {
		memcpy(p1, &data, sizeof(data));
	} else if (!strcmp(p1->name, "*")) {
		memcpy(p1, &data, sizeof(data));
		if (n->iptr==0) n->iptr = inode_find("*");
		get_inode(n->iptr)->ptrs[0] = n->ptrs[0];
		n->ptrs[0] += sizeof(data);
		memcpy(w, &data, sizeof(data));
	} else {
		p = get_inode(p->iptr);
		goto mk_loop;
	}
    printf("mknod(%s) -> %d\n", path, rv);
    return rv;
}

// most of the following callbacks implement
// another system call; see section 2 of the manual
int
mkdir(const char *path, int mode)
{
    int rv = mknod(path, mode | 040000);
    //printf("%d\n", count_l(path));
    // TODO: Nested Directories
    /*for (int i=0; i<count_l(path)) {
    }*/
    printf("mkdir(%s) -> %d\n", path, rv);
    return rv;
}

// Actually write data
int
write(const char *path, const char *buf, size_t size, off_t offset)
{
    int rv = 0;
    int l = tree_lookup(path);
    bool start = true;
    int p0=0, p1=0, i=0;
    inode* n = get_inode(l);
    inode* h = get_inode(1);
    char *data0, *data1;
    int i0, i1;
write_loop:
    if (start) {
    	data0 = ((char*)get_root_start()+h->ptrs[0]+offset);
    	start = false;
    } else {
    	data0 = ((char*)get_root_start()+h->ptrs[0]);
    }
    if (offset > n->size[0]) {
    	offset -= n->size[0];
    	data1 = ((char*)get_root_start()+h->ptrs[1]+offset);
    	for(; i < size; p0++, i++) data0[p0] = buf[i];
    	n->size[0]=p0;
    	n->ptrs[0] = h->ptrs[0];
    	h->ptrs[0] += size;
    } else {
    	data1 = ((char*)get_root_start()+h->ptrs[1]);
    	//data0[0]='\0';	//TODO : Worry about write collision later...
    	if (n->size[0] > 0) {
    		strncpy(data0, buf, n->size[0]);
    		strncpy(data1 + (int)n->size, buf+n->size[0], n->size[1]);
    		n->size[1]=p1;
    		n->ptrs[0] = h->ptrs[0];
    		h->ptrs[0] += n->size[0];
    		n->ptrs[1] = h->ptrs[1];
    		h->ptrs[0] += n->size[1];
    	} else {
    		strncpy(data0, buf, size);
    		n->size[0]=size;
    		n->ptrs[0] = h->ptrs[0];
    		h->ptrs[0] += size;
    	}
    }
    printf("write(%s, %ld bytes, @+%ld) -> %d\n", path, size, offset, rv);
    return rv;
}

// Actually read data
int
read(const char *path, char *buf, size_t size, off_t offset)
{
    int rv = 4096;
    int l = tree_lookup(path);
    bool start = true;
    int p0=0, p1=0, i=0;
    inode* n = get_inode(l);
    char *data0, *data1;
    if (l>-1) {
read_loop:
    	if (start) {
    		data0 = ((char*)get_root_start()+n->ptrs[0]+offset);
    		start = false;
    	} else {
    		data0 = ((char*)get_root_start()+n->ptrs[0]);
    	}
    	data1 = ((char*)get_root_start()+n->ptrs[1]);
    	// TODO : Bounds checking
    	strncpy(buf, data0, n->size[0]);
    	strncat(buf, data1, n->size[1]);
    	/*if (n->size[0] + n->size[1] < size) {
    		n = get_inode(n->iptr);
    		goto read_loop;
    	}*/
    }
    printf("read(%s, %ld bytes, @+%ld) -> %d\n", path, size, offset, rv);
    return rv;
}

int
main(int argc, char *argv[])
{
	char buf[256];
	storage_init("data.nufs");
	readdir();	// Empty
	mknod("/hello.txt", 755);
	readdir();
	write("/hello.txt", "hello!", 6, 0);
	read("/hello.txt", buf, 0, 0);
	//mkdir("/dir", 755);
	readdir();
	//printf("%s\n", buf);	// hello!
	pages_free();
}
