// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13  

struct {
  struct spinlock lock;
  struct spinlock evict_lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  //struct buf head;
  //
  struct buf *table[NBUCKET];
  struct spinlock locks[NBUCKET];
} bcache;

//insert and remove are unsafe (need lock bcache.table[bucket])
void insert(int bucket,struct buf *b){
  if((b->blockno%NBUCKET) != bucket){
    panic("insert");
  }
  if(bcache.table[bucket]==0){
    b->prev=0;
    b->next=0;
    bcache.table[bucket]=b;
  }else{
    b->next=bcache.table[bucket];
    b->prev=0;
    bcache.table[bucket]->prev=b;
    bcache.table[bucket]=b;
  }
}

void remove(int bucket,struct buf *b){
  if((b->blockno%NBUCKET) != bucket){
    panic("remove");
  }
  if(b==bcache.table[bucket]){
    bcache.table[bucket]=b->next;
  }else if(b->next==0){
      b->prev->next=0;
  }else{
    b->prev->next=b->next;
    b->next->prev=b->prev;
  }
}

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");
  initlock(&bcache.evict_lock, "bcache_evict");
  char name[20];
  for(int i=0;i<NBUCKET;i++){
    snprintf(name,8,"bcache%d",i);
    initlock(&bcache.locks[i], name);
  }
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    initsleeplock(&b->lock, "buffer");
    insert(0,b);
  }

}

static struct buf*
bgetcached(uint dev, uint blockno){
  struct buf *b;
  uint bucket;
  bucket=blockno%NBUCKET;
  acquire(&bcache.locks[bucket]);
  //Is the block already cached?
  for (b = bcache.table[bucket]; b != 0; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.locks[bucket]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.locks[bucket]);
  return 0;
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  uint bucket,bucket2;
  acquire(&bcache.lock);
  //Is the block already cached?
  bucket=blockno%NBUCKET;
  b=bgetcached(dev,blockno);
  if(b){
    release(&bcache.lock);
    return b;
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  int min_val=0;
  struct buf *lrub;
  acquire(&bcache.evict_lock);
  //此时，可能已经缓存
  b=bgetcached(dev,blockno);
  if(b){
    release(&bcache.lock);
    return b;
  }
  
  //寻找目标项
eviction:
  lrub=0;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    bucket2=(b->blockno)%NBUCKET;
    acquire(&bcache.locks[bucket2]);
    if(b->refcnt == 0) {
      if(lrub==0 || b->lru <= min_val){
          lrub=b;
         min_val=b->lru;
      }
    }
    release(&bcache.locks[bucket2]);
  }
  bucket2=(lrub->blockno)%NBUCKET;
  acquire(&bcache.locks[bucket2]);
  if(lrub==0){
    panic("bget: no buffers");
  }
  if(lrub->refcnt!=0){
    //目标项发生改变，重新找
    release(&bcache.locks[bucket2]);
    goto eviction;
  }
  //找到目标项，如果不在一个桶，需要额外加一把锁，并发生移动操作（移除和插入）
  if(bucket!=bucket2){
    acquire(&bcache.locks[bucket]);
    remove(bucket2,lrub);
  }
  lrub->dev = dev;
  lrub->blockno = blockno;
  lrub->valid = 0;
  lrub->refcnt = 1;
  if(bucket!=bucket2){
    insert(bucket,lrub);
    release(&bcache.locks[bucket]);
  }
  release(&bcache.locks[bucket2]);
  release(&bcache.evict_lock);
  acquiresleep(&lrub->lock);  
  release(&bcache.lock);
  return lrub;
  // // Is the block already cached?
  // for(b = bcache.head.next; b != &bcache.head; b = b->next){
  //   if(b->dev == dev && b->blockno == blockno){
  //     b->refcnt++;
  //     release(&bcache.lock);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }

  // // Not cached.
  // // Recycle the least recently used (LRU) unused buffer.
  // for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
  //   if(b->refcnt == 0) {
  //     b->dev = dev;
  //     b->blockno = blockno;
  //     b->valid = 0;
  //     b->refcnt = 1;
  //     release(&bcache.lock);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }
  // panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  acquire(&bcache.lock);
  int bucket=b->blockno%NBUCKET;
  acquire(&bcache.locks[bucket]);
  b->refcnt--;
  if(b->refcnt==0){
    //tick
    acquire(&tickslock);
    b->lru = ticks;
    release(&tickslock);
  }
  release(&bcache.locks[bucket]);
  release(&bcache.lock);
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  int bucket=b->blockno%NBUCKET;
  acquire(&bcache.locks[bucket]);
  b->refcnt++;
  release(&bcache.locks[bucket]);
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  int bucket=b->blockno%NBUCKET;
  acquire(&bcache.locks[bucket]);
  b->refcnt--;
  release(&bcache.locks[bucket]);
  release(&bcache.lock);
}
