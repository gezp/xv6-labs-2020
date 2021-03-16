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
  int *ref_count;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);

}


void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  // physical memory for ref_count[] ,the length of ref_count[] is total_pagenum
  int total_pagenum= PGROUNDUP(PHYSTOP-KERNBASE)/PGSIZE;
  // 4B for each ref_count,so PGROUNDUP(4B * total_pagenum )/PGSIZE pages
  // physical memory for ref_count[].
  int ref_pagenum=PGROUNDUP(total_pagenum*sizeof(int))/PGSIZE;
  acquire(&kmem.lock);
  kmem.ref_count=(int *)p;
  release(&kmem.lock);
  p=(char *)(p+ref_pagenum*PGSIZE);
  //printf("ref page num:%d/%d\n",ref_pagenum,total_pagenum);
  //init free physical memory
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    kmem.ref_count[PGNUM((uint64)p-KERNBASE)]=1;
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

  acquire(&kmem.lock);
  if(kmem.ref_count[PGNUM((uint64)pa-KERNBASE)]==0){
    printf("pa:%p\n",pa);
    panic("kfree,ref is 0");
  }
  if(kmem.ref_count[PGNUM((uint64)pa-KERNBASE)]==1){
    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);
    r = (struct run*)pa;
    r->next = kmem.freelist;
    kmem.freelist = r;
  }
  
  kmem.ref_count[PGNUM((uint64)pa-KERNBASE)]--;
  
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
  r = kmem.freelist;
  if(r){
    kmem.ref_count[PGNUM((uint64)r-KERNBASE)]=1;
    kmem.freelist = r->next;
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}


void kincref(void *pa){
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kref");

  acquire(&kmem.lock);
  kmem.ref_count[PGNUM((uint64)pa-KERNBASE)]++;;
  release(&kmem.lock);
}


int kref(void *pa){
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kref");
  int ref;
  acquire(&kmem.lock);
  ref=kmem.ref_count[PGNUM((uint64)pa-KERNBASE)];
  release(&kmem.lock);
  return ref;
}