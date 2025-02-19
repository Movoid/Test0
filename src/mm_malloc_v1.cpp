#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <sys/mman.h>
#include <type_traits>

// extern int mm_init(void)
// extern void *mm_malloc(size_t size);
// extern void mm_free(void* ptr)

// chunk 16 bytes alignment

static char *mem_heap;
static char *mem_brk;
static char *mem_max_addr;

enum : std::size_t {
  MAX_HEAP = 1ULL << 32,
  WSIZE = sizeof(std::uintptr_t),
  DSIZE = 2ULL * sizeof(std::uintptr_t),
  CHUNKSIZE = 1ULL << 12,
  ALIGNMENT = DSIZE,
};

template<typename A, typename B, typename std::void_t<decltype(std::declval<A>() > std::declval<B>())> *Req = nullptr>
static inline auto max(const A &a, const B &b) -> std::common_type_t<A, B> {
  return a > b ? a : b;
}

template<typename A, typename B,
         typename std::enable_if_t<std::is_convertible_v<A, std::uintptr_t> && std::is_convertible_v<B, std::uintptr_t>>
             *Req = nullptr>
static inline auto pack(const A &val1, const B &val2) -> std::common_type_t<A, B> {
  return val1 | val2;
}

template<typename A, typename std::enable_if_t<std::is_pointer_v<A>> *Req = nullptr>
static inline auto get(const A &a) -> std::uintptr_t {
  return *reinterpret_cast<std::uintptr_t *>(a);
}

template<typename A, typename B,
         typename std::enable_if_t<std::is_pointer_v<A> && std::is_convertible_v<B, std::uintptr_t>> *Req = nullptr>
static inline auto put(const A &ptr, const B &val) -> void {
  *reinterpret_cast<std::uintptr_t *>(ptr) = val;
}

// 16 bytes 对齐
template<typename A, typename std::enable_if_t<std::is_pointer_v<A>> *Req = nullptr>
static inline auto get_size(const A &chunk_ptr) -> std::uintptr_t {
  return get(chunk_ptr) & ~((1ULL << 4) - 1);
}

template<typename A, typename std::enable_if_t<std::is_pointer_v<A>> *Req = nullptr>
static inline auto get_alloc(const A &chunk_ptr) -> std::uintptr_t {
  return get(chunk_ptr) & 0x1;
}

template<typename A, typename std::enable_if_t<std::is_pointer_v<A>> *Req = nullptr>
static inline auto get_hdr_ptr(const A &block_ptr) -> std::uintptr_t * {
  return reinterpret_cast<std::uintptr_t *>(reinterpret_cast<char *>(block_ptr) - WSIZE);
}

template<typename A, typename std::enable_if_t<std::is_pointer_v<A>> *Req = nullptr>
static inline auto get_ftr_ptr(const A &block_ptr) -> std::uintptr_t * {
  return reinterpret_cast<std::uintptr_t *>(reinterpret_cast<char *>(block_ptr) + get_size(get_hdr_ptr(block_ptr)) -
                                            DSIZE);
}

// next block in the higher addr
template<typename A, typename std::enable_if_t<std::is_pointer_v<A>> *Req = nullptr>
static inline auto get_next_blkptr(const A &block_ptr) -> std::uintptr_t * {
  return reinterpret_cast<std::uintptr_t *>(reinterpret_cast<char *>(block_ptr) +
                                            get_size(reinterpret_cast<char *>(block_ptr) - WSIZE)); // get_size(header)
}

// prev block in the lower addr
template<typename A, typename std::enable_if_t<std::is_pointer_v<A>> *Req = nullptr>
static inline auto get_prev_blkptr(const A &block_ptr) -> std::uintptr_t * {
  return reinterpret_cast<std::uintptr_t *>(reinterpret_cast<char *>(block_ptr) -
                                            get_size(reinterpret_cast<char *>(block_ptr) - DSIZE)); // get_size(footer)
}

/*====================================*\

  mem_init - init the mem system model.

\*====================================*/
void mem_init(void) {
  mem_heap = reinterpret_cast<char *>(mmap(nullptr, MAX_HEAP, PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANON, -1, 0));
  mem_brk = mem_heap;
  mem_max_addr = mem_heap + MAX_HEAP;
}

/*====================================*\

  mem_sbrk - `sbrk()` func,
  get more heap memory from kernel.

\*====================================*/
void *mem_sbrk(std::size_t incr) {
  char *old_brk{mem_brk};
  if ((incr < 0) || (mem_brk + incr) > mem_max_addr) {
    errno = ENOMEM;
    fprintf(stderr, "ERROR: mem_sbrk failed. OOM...\n");
    return reinterpret_cast<void *>(-1);
  }
  mem_brk += incr;
  return reinterpret_cast<void *>(old_brk);
}

static char *heap_listp;

/*====================================*\

  coalesce - try to combine
  with neighbour free chunks.

\*====================================*/
static void *coalesce(void *bp) {
  // bool
  std::size_t prev_alloc{get_alloc(get_ftr_ptr(get_prev_blkptr(bp)))};
  std::size_t next_alloc{get_alloc(get_hdr_ptr(get_next_blkptr(bp)))};
  std::size_t size{get_size(get_hdr_ptr(bp))};

  // no need to combine
  if (prev_alloc && next_alloc) {
    return bp;
  } else if (prev_alloc && !next_alloc) {
    // combine
    size += get_size(get_hdr_ptr(get_next_blkptr(bp)));
    put(get_hdr_ptr(bp), pack(size, 0));
    put(get_ftr_ptr(bp), pack(size, 0));
  } else if (!prev_alloc && next_alloc) {
    // combine
    size += get_size(get_hdr_ptr(get_prev_blkptr(bp)));
    put(get_hdr_ptr(bp), pack(size, 0));
    put(get_hdr_ptr(get_prev_blkptr(bp)), pack(size, 0));
    bp = get_prev_blkptr(bp);
  } else {
    // combine two
    size += get_size(get_hdr_ptr(get_prev_blkptr(bp))) + get_size(get_ftr_ptr(get_next_blkptr(bp)));
    put(get_hdr_ptr(get_prev_blkptr(bp)), pack(size, 0));
    put(get_hdr_ptr(get_next_blkptr(bp)), pack(size, 0));
    bp = get_prev_blkptr(bp);
  }
  return bp;
}

/*====================================*\

  mm_free - clean this block,
  then try to coalesce.

\*====================================*/
void mm_free(void *bp) {
  std::size_t size{get_size(get_hdr_ptr(bp))};
  put(get_hdr_ptr(bp), pack(size, 0));
  put(get_ftr_ptr(bp), pack(size, 0));
  coalesce(bp);
}

/*====================================*\

  extend_heap - to extend the heap size
  from kernel by sbrk().
  or init the heap.

\*====================================*/
static void *extend_heap(std::size_t wordcnt) {
  char *bp{};            // block ptr
  std::uintptr_t size{}; // size of words

  size = (wordcnt % 2) ? (wordcnt + 1) * WSIZE : wordcnt * WSIZE;
  // get bp, the start addr of new heap memory from kernel.
  if ((bp = reinterpret_cast<char *>(mem_sbrk(size))) == reinterpret_cast<void *>(-1)) {
    return nullptr;
  }

  // add memory to the end chunk.
  put(get_hdr_ptr(bp), pack(size, 0));
  put(get_ftr_ptr(bp), pack(size, 0));
  // create the new empty end chunk.
  put(get_hdr_ptr(get_next_blkptr(bp)), pack(0, 1));
  // try combine
  return coalesce(bp);
}

/*====================================*\

  mm_init - create init free chunk list

\*====================================*/
int mm_init(void) {
  // get 4 * WSIZE to init the heap memory model.
  if ((heap_listp = reinterpret_cast<char *>(mem_sbrk(4 * WSIZE))) == reinterpret_cast<void *>(-1)) {
    return -1;
  }

  // 4 * WSIZE

  // useless
  put(heap_listp, 0);                            // align
  put(heap_listp + (1 * WSIZE), pack(DSIZE, 1)); // align
  // first chunk

  // ** this is a ALWAYS FULL AND EMPTY chunk in the end. **
  // if heap is extended, the new memory will be in this chunk.
  put(heap_listp + (2 * WSIZE), pack(DSIZE, 1));
  put(heap_listp + (3 * WSIZE), pack(0, 1));
  heap_listp += (2 * WSIZE); // move to the first chunk addr.

  // | EMPTY | 1 A HEADER | 1 A FOOTER | 2 F HEADER | ...
  //                      ^ heap_listp

  // extend to get the first chunk
  if (extend_heap(CHUNKSIZE / WSIZE) == nullptr) {
    return -1;
  }
  return 0;
}

/*====================================*\

  find_fit - find the target chunk
  which is able to contain `real_size`.

\*====================================*/
static void *find_fit(std::size_t real_size) {
  char *bp{heap_listp};

  for (bp = heap_listp; get_size(get_hdr_ptr(bp)); bp = reinterpret_cast<char *>(get_next_blkptr(bp))) {
    if (!get_alloc(get_hdr_ptr(bp)) && get_size(get_hdr_ptr(bp)) >= real_size) {
      return bp;
    }
  }

  return nullptr;
}

/*====================================*\

  place - set this chunk,
  and try to divide to 2 chunks.

\*====================================*/
static void place(void *bp, std::size_t asize) {
  // divide this chunk.
  std::size_t cur_chunksz{get_size(get_hdr_ptr(bp))};
  // if is able to divide to 2 chunks
  if (cur_chunksz - asize >= 2 * DSIZE) {
    put(get_hdr_ptr(bp), pack(asize, 1));
    put(get_ftr_ptr(bp), pack(asize, 1));
    bp = reinterpret_cast<char *>(get_next_blkptr(bp));
    put(get_hdr_ptr(bp), pack(cur_chunksz - asize, 0));
    put(get_ftr_ptr(bp), pack(cur_chunksz - asize, 0));
  } else {
    put(get_hdr_ptr(bp), pack(asize, 1));
    put(get_ftr_ptr(bp), pack(asize, 1));
  }
}

/*====================================*\

  mm_malloc - alloc memory to user.

\*====================================*/
void *mm_malloc(std::size_t usr_size) {
  size_t real_size{};  // header + block
  size_t extendsize{}; // if free blocks is not enough, then extend_heap
  char *bp{};

  if (usr_size == 0) return nullptr;
  if (usr_size <= DSIZE) {
    real_size = 2 * DSIZE;
  } else {
    // calc ceil.
    // the min val to n*(2*DSIZE) .
    real_size = DSIZE * (usr_size + DSIZE + DSIZE - 1) / DSIZE;
  }
  if ((bp = reinterpret_cast<char *>(find_fit(real_size))) != nullptr) {
    place(bp, real_size);
    return bp;
  }
  if ((bp = reinterpret_cast<char *>(extend_heap(extendsize / WSIZE))) == nullptr) {
    return nullptr;
  }
  // then place to bp extended.
  place(bp, real_size);
  return bp;
}