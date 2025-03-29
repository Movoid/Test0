#include <cstdio>
#include <cstdlib>

struct Slab {
  void *freelist;
  void *memory;
  size_t obj_size;
};

// 初始化 slab
void slab_init(struct Slab *slab, void *memory, size_t obj_size, size_t num_objs) {
  slab->freelist = memory;
  slab->memory = memory;
  slab->obj_size = obj_size;

  char *ptr = (char *)memory;
  for (size_t i = 0; i < num_objs - 1; ++i) {
    void *curr = ptr + i * obj_size;
    void *next = ptr + (i + 1) * obj_size;
    *(void **)curr = next;
  }

  void *last = ptr + (num_objs - 1) * obj_size;
  *(void **)last = NULL;
}

void *slab_alloc(struct Slab *slab) {
  if (!slab->freelist) return NULL;
  void *obj = slab->freelist;
  slab->freelist = *(void **)obj;
  return obj;
}

void slab_free(struct Slab *slab, void *obj) {
  *(void **)obj = slab->freelist;
  slab->freelist = obj;
}
