/* Kept beside the VFS implementation because committing a validated tree must
 * replace nodes atomically, without exposing half-restored credentials. */
#include "snapshot.h"
#define SNAP_MAGIC 0x3150414e53565841ull
struct snap_header {uint64_t magic;uint32_t version,count;uint64_t quotas[16];};
struct snap_node {char name[48],target[256];uint32_t parent,backing,flags,uid,gid,mode;uint64_t size,created,modified,quota;uint8_t pages[32];};
_Static_assert(sizeof(struct snap_node)==392,"snapshot format v1");
static bool snapshot_member(int n){
    for(unsigned i=0;i<NODE_MAX&&n>0;i++){
        if(nodes[n].parent==0)return seq(nodes[n].name,"users")||seq(nodes[n].name,"var");
        n=nodes[n].parent;
    }return false;
}
static unsigned snapshot_pages(const struct snap_node*r){unsigned n=0;for(unsigned p=0;p<256;p++)n+=(r->pages[p/8]>>(p%8))&1;return n;}
static bool snap_string(const char*s,size_t cap,bool name);
long vfs_snapshot_export(void*buffer,size_t capacity){
    if(current_uid)return VFS_EACCES;
    if(!buffer||((uintptr_t)buffer&7)||capacity<sizeof(struct snap_header))return VFS_ENOSPC;
    unsigned map[NODE_MAX]={0},count=0,roots=0;
    for(int i=1;i<NODE_MAX;i++)if(nodes[i].used&&snapshot_member(i))map[i]=++count;
    size_t need=sizeof(struct snap_header)+count*sizeof(struct snap_node);
    for(int i=1;i<NODE_MAX;i++)if(map[i]){
        struct node*n=&nodes[i];
        if(!snap_string(n->name,sizeof(n->name),true)||!snap_string(n->target,sizeof(n->target),false)||n->size>FILE_MAX||n->quota>FILE_MAX||n->mode>07777)return VFS_EINVAL;
        if((n->directory||n->symlink||n->hardlink)&&(n->size||n->capacity))return VFS_EINVAL;
        if(!n->parent){if(!n->directory)return VFS_EINVAL;roots|=seq(n->name,"users")?1:2;}
        if(n->mounted||n->sealed||(n->hardlink&&!map[content(i)-nodes]))return VFS_EBUSY;
        if(!n->hardlink)for(unsigned p=0;p<256;p++)if(n->pages[p])need+=4096;
    }
    if(roots!=3)return VFS_EINVAL;
    if(need>capacity||need>SNAPSHOT_MAX)return VFS_ENOSPC;
    uint8_t*b=buffer;for(size_t i=0;i<need;i++)b[i]=0;
    struct snap_header*h=buffer;h->magic=SNAP_MAGIC;h->version=1;h->count=count;
    for(unsigned i=0;i<16;i++)h->quotas[i]=quotas[i];
    struct snap_node*r=(void*)(h+1);size_t offset=sizeof(*h)+count*sizeof(*r);
    for(int i=1;i<NODE_MAX;i++)if(map[i]){
        struct node*n=&nodes[i];struct snap_node*x=&r[map[i]-1];
        scopy(x->name,n->name,sizeof(x->name));scopy(x->target,n->target,sizeof(x->target));
        x->parent=map[n->parent];x->backing=n->hardlink?map[content(i)-nodes]:0;
        x->flags=n->directory|(n->symlink<<1)|(n->hardlink<<2)|(n->locked<<3)|(n->readonly<<4);
        x->uid=n->uid;x->gid=n->gid;x->mode=n->mode;x->size=n->size;x->created=n->created;x->modified=n->modified;x->quota=n->quota;
        if(!n->hardlink)for(unsigned p=0;p<256;p++)if(n->pages[p]){
            x->pages[p/8]|=1u<<(p%8);for(unsigned j=0;j<4096;j++)b[offset++]=n->pages[p][j];
        }
    }
    return (long)need;
}
static bool snap_string(const char*s,size_t cap,bool name){
    size_t i=0;for(;i<cap&&s[i];i++)if(name&&(s[i]=='/'||(unsigned char)s[i]<32))return false;
    return i<cap&&(!name||(i&&!seq(s,".")&&!seq(s,"..")));
}
int vfs_snapshot_import(const void*buffer,size_t size){
    if(current_uid)return VFS_EACCES;
    for(unsigned i=0;i<FD_MAX;i++)if(fds[i].used)return VFS_EBUSY;
    if(!buffer||((uintptr_t)buffer&7)||size<sizeof(struct snap_header)||size>SNAPSHOT_MAX)return VFS_EINVAL;
    const struct snap_header*h=buffer;
    if(h->magic!=SNAP_MAGIC||h->version!=1||h->count<2||h->count>=NODE_MAX)return VFS_EINVAL;
    size_t offset=sizeof(*h)+h->count*sizeof(struct snap_node);
    if(offset>size)return VFS_EINVAL;
    const struct snap_node*r=(const void*)(h+1);unsigned roots=0;
    for(unsigned i=0;i<h->count;i++)if(r[i].parent>h->count)return VFS_EINVAL;
    for(unsigned i=0;i<h->count;i++){
        const struct snap_node*x=&r[i];unsigned flags=x->flags;
        if(!snap_string(x->name,sizeof(x->name),true)||!snap_string(x->target,sizeof(x->target),false)||flags>31||x->mode>07777||x->size>FILE_MAX||x->quota>FILE_MAX||x->parent>h->count||x->backing>h->count)return VFS_EINVAL;
        if(((flags&1)&&(flags&6))||((flags&2)&&(flags&4)))return VFS_EINVAL;
        if(!x->parent){
            unsigned bit=seq(x->name,"users")?1:seq(x->name,"var")?2:0;
            if(!bit||(roots&bit)||!(flags&1))return VFS_EINVAL;
            roots|=bit;
        }else if(!(r[x->parent-1].flags&1))return VFS_EINVAL;
        unsigned at=i+1,hops=0;while(at&&hops++<NODE_MAX)at=r[at-1].parent;
        if(at)return VFS_ELOOP;
        for(unsigned j=0;j<i;j++)if(x->parent==r[j].parent&&seq(x->name,r[j].name))return VFS_EEXIST;
        unsigned pages=snapshot_pages(x);
        if((flags&7)&&(pages||x->size))return VFS_EINVAL;
        if(flags&4){if(!x->backing||x->backing==i+1||(r[x->backing-1].flags&7))return VFS_EINVAL;}
        else if(x->backing)return VFS_EINVAL;
        if((size-offset)/4096<pages)return VFS_EINVAL;
        offset+=(size_t)pages*4096;
    }
    if(roots!=3||offset!=size)return VFS_EINVAL;
    /* Stage every allocation before releasing the current mutable tree. */
    for(unsigned i=1;i<NODE_MAX;i++)if(nodes[i].used&&snapshot_member(i)&&(readonly_node(i)||nodes[i].mounted))return VFS_EROFS;
    struct node*staged=kmalloc(sizeof(nodes));if(!staged)return VFS_ENOSPC;
    unsigned map[NODE_MAX]={0};bool old_mutable[NODE_MAX]={false};
    for(unsigned i=0;i<NODE_MAX;i++){
        old_mutable[i]=nodes[i].used&&snapshot_member(i);staged[i]=nodes[i];
        if(old_mutable[i])staged[i].used=false;
    }
    for(unsigned i=1;i<NODE_MAX;i++)if(nodes[i].used&&!old_mutable[i]&&nodes[i].hardlink&&old_mutable[content(i)-nodes]){kfree(staged);return VFS_EBUSY;}
    unsigned next=1;int result=VFS_ENOSPC;
    for(unsigned i=0;i<h->count;i++){
        while(next<NODE_MAX&&staged[next].used)next++;
        if(next==NODE_MAX)goto failed;
        map[i+1]=next;staged[next++]=(struct node){.used=true};
    }
    offset=sizeof(*h)+h->count*sizeof(*r);
    for(unsigned i=0;i<h->count;i++){
        const struct snap_node*x=&r[i];struct node*n=&staged[map[i+1]];
        n->directory=x->flags&1;n->symlink=x->flags&2;n->hardlink=x->flags&4;n->locked=x->flags&8;n->readonly=x->flags&16;
        n->parent=map[x->parent];n->backing=map[x->backing];n->uid=x->uid;n->gid=x->gid;n->mode=x->mode;
        n->size=x->size;n->created=x->created;n->modified=x->modified;n->quota=x->quota;n->links=1;
        scopy(n->name,x->name,sizeof(n->name));scopy(n->target,x->target,sizeof(n->target));
        for(unsigned p=0;p<256;p++)if(x->pages[p/8]&(1u<<(p%8))){
            n->pages[p]=kmalloc(4096);if(!n->pages[p])goto failed;
            const uint8_t*b=buffer;for(unsigned j=0;j<4096;j++)n->pages[p][j]=b[offset++];n->capacity+=4096;
        }
    }
    for(unsigned i=0;i<h->count;i++)if(staged[map[i+1]].hardlink)staged[staged[map[i+1]].backing].links++;
    for(unsigned i=1;i<NODE_MAX;i++)if(old_mutable[i]&&!nodes[i].hardlink)release_content(&nodes[i]);
    for(unsigned i=0;i<NODE_MAX;i++)nodes[i]=staged[i];
    for(unsigned i=0;i<16;i++)quotas[i]=h->quotas[i];
    scopy(cwd,"/",sizeof(cwd));kfree(staged);return 0;
failed:
    for(unsigned i=1;i<=h->count;i++)if(map[i])release_content(&staged[map[i]]);
    kfree(staged);return result;
}
