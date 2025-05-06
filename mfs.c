#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include "pages.h"
#include "inode.h"
#include "bitmap.h"
#include "directory.h"
#include "mkfs.h"
#include "mfs.h"

dirent stop;

int inode_size(inode *d)
{
	return (d->size[0]+d->size[1]);
}

int calc_offset(inode *d, off_t offset)
{
	if (d->size[0]!=0) offset-=d->size[0];
	else return offset;
	if (d->size[1]==0) return offset;
	else offset-=d->size[0];
	if (offset > 0 && d->iptr!=0) {
		d = get_inode(d->iptr);
		return calc_offset(d, offset);
	} else {
		//d->iptr = alloc_inode();	// Inode_find()?
		//d = get_inode(d->iptr);
		if (offset == 0) return 0;
		return -1;
	}
}

int _remainder(inode *d, int size, off_t offset)
{
	if (d->size[0]==0) return 0;
	else if (d->size[1]==0) return 0;
	else {
		return (size - ((d->size[0]+d->size[1]) - offset));
	}
}

char* get_data_end()
{
	return (char*)get_root_start() + get_inode(1)->ptrs[0];
}

bool is_empty(inode *d)
{
	return (d->size[0]==0 || d->size[1]==0);
}

int split(const char *path, int n, char buf[DIR_NAME]) {
	int rv=0;
	if (n==0) {
		strcpy(buf, "/");
	} else {
		int c=0, i=0;
		for (; path[i] && c<n+1; i++) {
			buf[i]=path[i];
			if (path[i]=='/') c++;
		}
		if (buf[i-1]=='/') buf[i-1]='\0';
	}
	return rv;
}

int
count_l(const char *path) {
	int c=0;
	for(int i=0; path[i]; i++) {
		if (path[i]=='/') c++;
	}
	return c;
}

int
find_parent(const char *path)
{
	char trm[DIR_NAME];
	int i;
	for(i=0; path[i]; i++) trm[i]=path[i];
	if (trm[i]=='/') trm[i]='\0';
	char ptr[DIR_NAME];
	int k = count_l(trm);
	int n=0;
	for (int i=0; i<k; i++) {
		split(trm, i, ptr);
		n = tree_lookup(ptr, n);
		if (n<0) return -ENOENT;
	}
	// TODO : Locate a parent directory and return an inode, or an iptr
	return n;
}

char*
parent_path(const char *path)
{
	char trm[DIR_NAME];
	int i;
	for(i=0; path[i]; i++) trm[i]=path[i];
	if (trm[i]=='/') trm[i]='\0';
	char ptr[DIR_NAME];
	int k = count_l(trm);
	int n=0;
	for (int i=0; i<k; i++) {
		split(trm, i, ptr);
	}
	
	char *m = (char*)malloc(DIR_NAME * sizeof(char));
	strncpy(m, ptr, DIR_NAME);
	return m;
}

char *get_data(int offset)
{
	return ((char*)get_root_start()+offset);
}

int
_readdir(const char *path, int l)
{
	int rv=2;
	(l == 0) ? (l = tree_lookup(path, find_parent(path))) : l;
	inode *a = get_inode(l);
	dirent *e;
	
	if (a->ptrs[0]==0 && !!strcmp(path, "/")) return 0;
	e = (dirent*)get_data(a->ptrs[0]);
	if (!strcmp(e->name, "")) return 0;
	printf("%s\n", e->name);	// getaddr
	rv++;
	
	if (a->ptrs[1]==0) return 1;
	e = (dirent*)get_data(a->ptrs[1]);
	if (!strcmp(e->name, "")) return 1;
	printf("%s\n", e->name);	// getaddr
	rv++;
	
	int ptr = a->iptr;
	rv = (ptr==0) ? (rv) : rv+_readdir(path, ptr);
	return rv;
}

int
readdir(const char *path)
{
	int rv=_readdir(path, 0);
	printf("readdir(%d)\n", rv);
	return rv;
}

int
mknod(const char *path, int mode)
{
	int rv = 0;
	int l = (!strcmp(path, "/")) ? 0 : inode_find(path);
	char *ppath = parent_path(path);
	
	dirent e;
	inode *n = get_inode(l);
	n->mode=mode;
	
	strncpy(e.name, path, DIR_NAME);
	e.inum = l;
	
	int i=0;
	while(true) {
		if (n->ptrs[0] == 0 && !(!strcmp(ppath, "/")) || (!strcmp(path, "/"))) break;
		else if (n->ptrs[1] == 0) {
			i++;
			break;
		}
		else if (n->iptr == 0) {
			n->iptr = inode_find(ppath);
		}
		n = get_inode(n->iptr);
	}
	
	write(ppath, (char*)&e, sizeof(dirent), i*sizeof(dirent));

	free(ppath);
	printf("mknod(%s) -> %d\n", path, rv);
	return rv;
}

int
is_dir(int mode)
{
	return (mode > 040000) ? true : false;
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

int
write_sp(char *data, int inode, int ptr, const char *buf, size_t size)
{
	struct inode n; // *get_inode(inode);
	memcpy(&n, get_inode(inode), sizeof(n));
	struct inode h; // *get_inode(1);
	memcpy(&h, get_inode(1), sizeof(n));
	memcpy(data, buf, size);
	data[size] = '\0';
	n.size[ptr]=size;
	printf("ptr = %d\n", ptr);
	n.ptrs[ptr] = h.ptrs[0];
	h.ptrs[0] += size;
	memcpy(get_inode(inode), &n, sizeof(n));
	memcpy(get_inode(1), &h, sizeof(h));
}

int
_write(const char *path, const char *buf, size_t size, off_t offset, int l)
{
	int rv = 0;
	(l == 0) ? l = tree_lookup(path, find_parent(path)) : l;
	inode *n = get_inode(l), *h = get_inode(1);
	
	int s = inode_size(n);
	
	if (s == 0) write_sp(get_data_end()+offset, l, 0, buf, size);
	else if (is_empty(n)) write_sp(get_data_end()+offset-s, l, 1, buf, size);
	else {
		int r = _remainder(n, size, offset);
		if (r<=0) {
			if (offset < n->size[0]) {
				write_sp(get_data(n->ptrs[0]+offset), l, 0, buf, n->size[0]-offset);
				size-=n->size[0];
			}
			if (size > 0) {
				write_sp(get_data(n->ptrs[1]+(offset-n->size[0])), l, 1, buf, size );
			}
		}
		else {
			if (n->iptr == 0) n->iptr = inode_find(path);
			if (_remainder(n, size, offset) >= size) {
				return _write(path, buf, (size), (_remainder(n, size, offset) - size), n->iptr);
			}
			else
				return _write(path, buf, (size - _remainder(n, size, offset)), (0), n->iptr);
		}
	}
	printf("write(%s, %ld bytes, @+%ld) -> %d\n", path, size, offset, rv);
	return rv;
}


int
_read(const char *path, const char *buf, size_t size, off_t offset, int l)
{
	(l == 0) ? l = tree_lookup(path, find_parent(path)) : l;
	inode *n = get_inode(l);
	
	int r = ( size - (n->size[0]+n->size[1]) );
	// TODO : Reads larger than a single inode...
	
	if (offset < n->size[0]) {
		memcpy(buf, get_data(n->ptrs[0]+offset), n->size[0]-offset);
		if ( (size - n->size[0]) > 0 ) memcpy(buf+n->size[0], get_data(n->ptrs[1]), ( n->size[1] > (size-n->size[0]) ) ? (size) : (n->size[1]) );
	}
	else {
		if (offset < n->size[0]+n->size[1]) {
			memcpy(buf, get_data(n->ptrs[1]+(offset-n->ptrs[0])), n->size[1]-(offset-n->size[0]));
		} else if (n->iptr==0) return -1;
		else {
			return _read(path, buf, size, offset - (n->size[0]+n->size[1]), n->iptr);
		}
	}
	
	if (r>0) return _read(path, buf, r, offset - (n->size[0]+n->size[1]), n->iptr);
}

int
write(const char *path, const char *buf, size_t size, off_t offset)
{
	return _write(path, buf, size, offset, 0);
}

int
read(const char *path, char *buf, size_t size, off_t offset)
{
	int l = tree_lookup(path, find_parent(path));
	return _read(path, buf, size, offset, l);
}
