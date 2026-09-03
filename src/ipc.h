#ifndef AXIOM_IPC_H
#define AXIOM_IPC_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "memory.h"
int ipc_pipe_create(void);
long ipc_pipe_write(int pipe,const void*data,size_t size);
long ipc_pipe_read(int pipe,void*data,size_t size);
int ipc_event_send(uint32_t pid,uint32_t events);
uint32_t ipc_event_take(uint32_t pid,uint32_t mask);
int ipc_shm_create(size_t bytes);
int ipc_shm_map(int object,struct vmm_space*space,uint64_t address,bool writable);
void *ipc_shm_kernel_address(int object);
void ipc_init(void);
bool ipc_self_test(void);
#endif
