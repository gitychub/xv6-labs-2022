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
#define HASH(x) ((x) % NBUCKET)

struct {
  struct spinlock locks[NBUCKET];
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf heads[NBUCKET];
} bcache;

void remove_from_chain(struct buf *b) {
  b->next->prev = b->prev;
  b->prev->next = b->next;
}

void insert_into_chain(struct buf *b, uint bucket) {
  b->next = bcache.heads[bucket].next;
  b->prev = &bcache.heads[bucket];
  bcache.heads[bucket].next->prev = b;
  bcache.heads[bucket].next = b;
}

void edit_metadata(struct buf *b, uint dev, uint blockno) {
  b->dev = dev;
  b->blockno = blockno;
  b->valid = 0;
  b->refcnt = 1;
}

void
binit(void)
{
  struct buf *b;

  for (uint i = 0; i < NBUCKET; i++) {
    initlock(&bcache.locks[i], "bcache");

    // Create linked list of buffers
    bcache.heads[i].prev = &bcache.heads[i];
    bcache.heads[i].next = &bcache.heads[i];
  }

  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    initsleeplock(&b->lock, "buffer");
    uint bucket = HASH(b->dev + b->blockno);
    insert_into_chain(b, bucket);
  }
}

struct buf *bget_other_bucket(uint bucket, uint bbucket, uint dev, uint blockno) {
  struct buf *b;
  acquire(&bcache.locks[bbucket]);

  for(b = bcache.heads[bbucket].next; b != &bcache.heads[bbucket]; b = b->next){
    if(b->refcnt == 0) {
      remove_from_chain(b);
      release(&bcache.locks[bbucket]);

      acquire(&bcache.locks[bucket]);
      insert_into_chain(b, bucket);
      edit_metadata(b, dev, blockno);
      release(&bcache.locks[bucket]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  release(&bcache.locks[bbucket]);
  return 0;
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  uint bucket = HASH(dev + blockno);
  acquire(&bcache.locks[bucket]);
  struct buf *bempty = 0;
  for(b = bcache.heads[bucket].next; b != &bcache.heads[bucket]; b = b->next){
    // Is the block already cached?
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.locks[bucket]);
      acquiresleep(&b->lock);
      return b;
    }

    if (bempty == 0 && b->refcnt == 0) {
      bempty = b;
    }
  }

  // Not cached. Recycle unused buffer in the same bucket.
  if (bempty) {
    edit_metadata(bempty, dev, blockno);
    release(&bcache.locks[bucket]);
    acquiresleep(&bempty->lock);
    return bempty;
  }
  release(&bcache.locks[bucket]);

  // Not cached. Recycle unused buffer from other buckets
  for (uint bbucket = bucket + 1; bbucket < NBUCKET; bbucket++) {
    if ((b = bget_other_bucket(bucket, bbucket, dev, blockno))) {
      return b;
    }
  }
  for (uint bbucket = 0; bbucket < bucket; bbucket++) {
    if ((b = bget_other_bucket(bucket, bbucket, dev, blockno))) {
      return b;
    }
  }
  panic("bget: no buffers");
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

  uint bucket = HASH(b->dev + b->blockno);
  acquire(&bcache.locks[bucket]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    remove_from_chain(b);
    insert_into_chain(b, bucket);
  }
  release(&bcache.locks[bucket]);
}

void
bpin(struct buf *b) {
  uint bucket = HASH(b->dev + b->blockno);
  acquire(&bcache.locks[bucket]);
  b->refcnt++;
  release(&bcache.locks[bucket]);
}

void
bunpin(struct buf *b) {
  uint bucket = HASH(b->dev + b->blockno);
  acquire(&bcache.locks[bucket]);
  b->refcnt--;
  release(&bcache.locks[bucket]);
}


