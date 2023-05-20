// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

// struct me to lock kai ton pinaka gia ta reference counters
struct {
  int ref_count[PHYSTOP/PGSIZE];
  struct spinlock rlock;
} RefCount;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&RefCount.rlock, "reflock"); // init ref lock
  freerange(end, (void*)PHYSTOP);
}
// find index of ref_array
int hash(char* pa){
  return (pa - (char*)PGROUNDUP((uint64)end)) >> 12 ; // bitwise shift by the offset
}
// increase reference counter
void increase(char* pa){
  acquire(&RefCount.rlock);
  RefCount.ref_count[hash(pa)]++;
  release(&RefCount.rlock);
}
// reduce reference counter
void reduce(char* pa){
  acquire(&RefCount.rlock);
  RefCount.ref_count[hash(pa)]--;
  release(&RefCount.rlock);
}
// return reference
int get_ref(char* pa){
  acquire(&RefCount.rlock);
  int ref = RefCount.ref_count[hash(pa)];
  release(&RefCount.rlock);
  return ref;
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    acquire(&RefCount.rlock);
    RefCount.ref_count[hash(p)] = 1; // set ref count = 1 for kfree()
    release(&RefCount.rlock);
    kfree(p);
  }
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");
  
  // if ref counter > 1 we just reduce the counter
  // else we continue with kfree and we set the counter to 0
  acquire(&RefCount.rlock);
  if(RefCount.ref_count[hash((char*)pa)] > 1){
    RefCount.ref_count[hash((char*)pa)] -= 1;
    release(&RefCount.rlock);
    return;
  }

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);
  RefCount.ref_count[hash((char*)pa)] = 0; //
  release(&RefCount.rlock);               //

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  acquire(&RefCount.rlock); // acquire lock
  r = kmem.freelist;
  if(r){
    kmem.freelist = r->next;
    RefCount.ref_count[hash((char*)r)] = 1; // set ref_count of page to 1 
  }
  release(&RefCount.rlock); // release lock
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
