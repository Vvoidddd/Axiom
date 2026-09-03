#include "ipc.h"

#define PIPE_MAX 16
#define PIPE_CAPACITY 1024
#define EVENT_SLOTS 32
#define SHM_MAX 8
#define SHM_PAGES 16

struct pipe_object{bool used;uint16_t read_at,write_at,count;uint8_t data[PIPE_CAPACITY];};
struct event_slot{uint32_t pid,pending;};
struct shared_object{bool used;uint16_t pages;uint64_t physical[SHM_PAGES];};
static struct pipe_object pipes[PIPE_MAX];
static struct event_slot events[EVENT_SLOTS];
static struct shared_object shared[SHM_MAX];

void ipc_init(void){for(unsigned i=0;i<PIPE_MAX;i++)pipes[i].used=false;for(unsigned i=0;i<EVENT_SLOTS;i++)events[i]=(struct event_slot){0};for(unsigned i=0;i<SHM_MAX;i++)shared[i].used=false;}
int ipc_pipe_create(void){for(int i=0;i<PIPE_MAX;i++)if(!pipes[i].used){pipes[i]=(struct pipe_object){.used=true};return i;}return-28;}
long ipc_pipe_write(int id,const void*data,size_t size){if(id<0||id>=PIPE_MAX||!pipes[id].used||(!data&&size))return-22;struct pipe_object*p=&pipes[id];const uint8_t*in=data;size_t done=0;while(done<size&&p->count<PIPE_CAPACITY){p->data[p->write_at]=in[done++];p->write_at=(p->write_at+1)%PIPE_CAPACITY;p->count++;}return(long)done;}
long ipc_pipe_read(int id,void*data,size_t size){if(id<0||id>=PIPE_MAX||!pipes[id].used||(!data&&size))return-22;struct pipe_object*p=&pipes[id];uint8_t*out=data;size_t done=0;while(done<size&&p->count){out[done++]=p->data[p->read_at];p->read_at=(p->read_at+1)%PIPE_CAPACITY;p->count--;}return(long)done;}
int ipc_event_send(uint32_t pid,uint32_t bits){if(!pid||!bits)return-22;for(unsigned i=0;i<EVENT_SLOTS;i++)if(events[i].pid==pid||!events[i].pid){events[i].pid=pid;events[i].pending|=bits;return 0;}return-28;}
uint32_t ipc_event_take(uint32_t pid,uint32_t mask){for(unsigned i=0;i<EVENT_SLOTS;i++)if(events[i].pid==pid){uint32_t result=events[i].pending&mask;events[i].pending&=~result;return result;}return 0;}
int ipc_shm_create(size_t bytes){if(!bytes||bytes>SHM_PAGES*4096)return-22;for(int i=0;i<SHM_MAX;i++)if(!shared[i].used){shared[i]=(struct shared_object){.used=true,.pages=(uint16_t)((bytes+4095)/4096)};for(unsigned p=0;p<shared[i].pages;p++)if(!(shared[i].physical[p]=pmm_alloc()))return-12;return i;}return-28;}
int ipc_shm_map(int id,struct vmm_space*space,uint64_t address,bool writable){if(id<0||id>=SHM_MAX||!shared[id].used||!space||(address&0xfff))return-22;for(unsigned p=0;p<shared[id].pages;p++)if(!vmm_space_map_user(space,address+(uint64_t)p*4096,shared[id].physical[p],writable,false))return-12;return 0;}
void*ipc_shm_kernel_address(int id){return id>=0&&id<SHM_MAX&&shared[id].used?pmm_direct_map(shared[id].physical[0]):0;}
bool ipc_self_test(void){ipc_init();int pipe=ipc_pipe_create();const char message[]="axiom-ipc";char output[sizeof(message)]={0};if(pipe<0||ipc_pipe_write(pipe,message,sizeof(message))!=(long)sizeof(message)||ipc_pipe_read(pipe,output,sizeof(output))!=(long)sizeof(output))return false;for(unsigned i=0;i<sizeof(message);i++)if(message[i]!=output[i])return false;if(ipc_event_send(2,5)||ipc_event_take(2,1)!=1||ipc_event_take(2,7)!=4)return false;int object=ipc_shm_create(4096);if(object<0)return false;uint8_t*memory=ipc_shm_kernel_address(object);if(!memory)return false;memory[0]=0x5a;struct vmm_space space;if(!vmm_space_create(&space)||ipc_shm_map(object,&space,0x500000,true)||!vmm_space_user_range_valid(&space,0x500000,4096,true)||memory[0]!=0x5a)return false;return true;}
