#ifndef AXIOM_SNAPSHOT_H
#define AXIOM_SNAPSHOT_H
#include <stddef.h>
#include <stdint.h>
#define SNAPSHOT_MAX (8u*1024u*1024u)
/* Versioned, bounded /users + /var checkpoint, never executable system trees. */
long vfs_snapshot_export(void *buffer,size_t capacity);
int vfs_snapshot_import(const void *buffer,size_t size);
#endif
