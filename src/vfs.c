#include "vfs.h"
#include "memory.h"
#include "hardware.h"
#include "security.h"
#define NODE_MAX 256
#define FD_MAX 32
#define NAME_MAX 47
#define PATH_MAX 255
#define FILE_MAX (1024u*1024u)
struct node{bool used,directory,symlink,hardlink,locked,mounted,readonly;char name[48],target[256];int parent,backing;uint8_t*pages[256];uint64_t size,capacity,created,modified,quota;uint32_t uid,gid,mode,links;char type[12],source[32];};
struct descriptor{bool used;int node;uint64_t offset;uint32_t flags;};
static struct node nodes[NODE_MAX];static struct descriptor fds[FD_MAX];static char cwd[PATH_MAX+1]="/";static uint32_t current_uid,current_gid,current_groups[8],current_group_count,current_umask;static uint64_t current_caps;static uint64_t quotas[16];
static size_t slen(const char*s){size_t n=0;while(s[n])n++;return n;}static bool seq(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return!*a&&!*b;}static void scopy(char*d,const char*s,size_t n){if(!n)return;while(*s&&n>1){*d++=*s++;n--;}*d=0;}
static uint64_t now(void){return hardware_uptime_ms()/1000;}
static struct node*content(int n){unsigned hops=0;while(nodes[n].hardlink&&hops++<NODE_MAX)n=nodes[n].backing;return&nodes[n];}
static bool in_group(uint32_t gid){if(current_gid==gid)return true;for(uint32_t i=0;i<current_group_count;i++)if(current_groups[i]==gid)return true;return false;}
static bool privileged(uint64_t capability){return current_uid==0||(current_caps&capability)!=0;}
static bool device_tree(const struct node*n){int at=(int)(n-nodes);while(at>0){if(nodes[at].parent==0&&seq(nodes[at].name,"devices"))return true;at=nodes[at].parent;}return false;}
static bool access_ok(const struct node*n,uint32_t bit){if(privileged(CAP_DAC)||(device_tree(n)&&privileged(CAP_DEVICE)))return true;uint32_t bits=current_uid==n->uid?(n->mode>>6):(in_group(n->gid)?(n->mode>>3):n->mode);return(bits&bit)==bit;}
static int child(int parent,const char*name){for(int i=1;i<NODE_MAX;i++)if(nodes[i].used&&nodes[i].parent==parent&&seq(nodes[i].name,name))return i;return VFS_ENOENT;}
static int resolve(const char*path,bool follow){if(!path||!*path)return VFS_EINVAL;int at=0;const char*p=path;if(*p=='/')p++;else{at=0;const char*c=cwd+1;char part[48];while(*c){size_t n=0;while(*c&&*c!='/'&&n<47)part[n++]=*c++;part[n]=0;if(*c)c++;if(n){if(!access_ok(&nodes[at],1))return VFS_EACCES;at=child(at,part);if(at<0)return at;}}}char part[48];while(*p){while(*p=='/')p++;if(!*p)break;size_t n=0;while(*p&&*p!='/'&&n<47)part[n++]=*p++;part[n]=0;if(*p&&*p!='/')return VFS_ENAMETOOLONG;if(seq(part,"."))continue;if(seq(part,"..")){if(at)at=nodes[at].parent;continue;}if(!nodes[at].directory)return VFS_ENOTDIR;if(!access_ok(&nodes[at],1))return VFS_EACCES;at=child(at,part);if(at<0)return at;}if(follow&&nodes[at].symlink)return resolve(nodes[at].target,false);return at;}
static int parent_of(const char*path,char*name){char temp[PATH_MAX+1];scopy(temp,path,sizeof(temp));size_t n=slen(temp);while(n>1&&temp[n-1]=='/')temp[--n]=0;char*slash=0;for(char*p=temp;*p;p++)if(*p=='/')slash=p;if(!slash){scopy(name,temp,48);return resolve(cwd,true);}scopy(name,slash+1,48);if(slen(slash+1)>NAME_MAX)return VFS_ENAMETOOLONG;if(slash==temp)slash[1]=0;else*slash=0;return resolve(temp,true);}
static int create(const char*path,bool directory,uint32_t mode){char name[48];int parent=parent_of(path,name);if(parent<0)return parent;if(!nodes[parent].directory)return VFS_ENOTDIR;if(nodes[parent].readonly)return VFS_EROFS;if(!access_ok(&nodes[parent],3))return VFS_EACCES;if(!*name)return VFS_EINVAL;if(child(parent,name)>=0)return VFS_EEXIST;for(int i=1;i<NODE_MAX;i++)if(!nodes[i].used){uint32_t gid=(nodes[parent].mode&02000)?nodes[parent].gid:current_gid;uint32_t final_mode=(mode&~current_umask)|((directory&&((nodes[parent].mode&02000)!=0))?02000:0);nodes[i]=(struct node){.used=true,.directory=directory,.parent=parent,.created=now(),.modified=now(),.mode=final_mode,.links=1,.quota=FILE_MAX,.uid=current_uid,.gid=gid};scopy(nodes[i].name,name,sizeof(nodes[i].name));return i;}return VFS_ENOSPC;}
void vfs_init(void){current_uid=current_gid=current_group_count=current_umask=0;current_caps=0;for(int i=0;i<NODE_MAX;i++)nodes[i].used=false;for(unsigned i=0;i<16;i++)quotas[i]=FILE_MAX;nodes[0]=(struct node){.used=true,.directory=true,.parent=0,.mode=0755,.links=1,.quota=FILE_MAX};const char*dirs[]={"/boot","/system","/apps","/users","/users/admin","/tmp","/var","/devices","/var/log"};for(unsigned i=0;i<sizeof(dirs)/sizeof(dirs[0]);i++)vfs_mkdir(dirs[i],i==3?0755:(i==4?0700:(i==5?01777:(i==7||i==8?0700:0755))));}
int vfs_open(const char*path,uint32_t flags){int n=resolve(path,true);if(n<0&&(flags&VFS_CREATE))n=create(path,false,0644);if(n<0)return n;struct node*c=content(n);if(c->directory&&!(flags&VFS_DIRECTORY))return VFS_EISDIR;if((flags&VFS_READ)&&!access_ok(c,4))return VFS_EACCES;if((flags&VFS_WRITE)&&!access_ok(c,2))return VFS_EACCES;if((flags&VFS_WRITE)&&(c->readonly||c->locked))return c->locked?VFS_EBUSY:VFS_EROFS;if((flags&VFS_TRUNCATE)&&(flags&VFS_WRITE)){for(unsigned p=0;p<256;p++)if(c->pages[p]){kfree(c->pages[p]);c->pages[p]=0;}c->size=c->capacity=0;c->modified=now();}for(int i=0;i<FD_MAX;i++)if(!fds[i].used){fds[i]=(struct descriptor){.used=true,.node=n,.flags=flags};return i;}return VFS_ENOSPC;}
int vfs_close(int fd){if(fd<0||fd>=FD_MAX||!fds[fd].used)return VFS_EBADF;fds[fd].used=false;return 0;}
long vfs_read(int fd,void*b,size_t size){if(fd<0||fd>=FD_MAX||!fds[fd].used)return VFS_EBADF;struct descriptor*f=&fds[fd];struct node*n=content(f->node);if(!access_ok(n,4))return VFS_EACCES;if(n->directory)return VFS_EISDIR;if(f->offset>=n->size)return 0;if(size>n->size-f->offset)size=n->size-f->offset;uint8_t*d=b;for(size_t i=0;i<size;i++){uint64_t pos=f->offset+i;d[i]=n->pages[pos/4096]?n->pages[pos/4096][pos%4096]:0;}f->offset+=size;return size;}
uint64_t vfs_user_usage(uint32_t uid){uint64_t total=0;for(int i=1;i<NODE_MAX;i++)if(nodes[i].used&&!nodes[i].hardlink&&nodes[i].uid==uid)total+=nodes[i].capacity;return total;}
long vfs_write(int fd,const void*b,size_t size){if(fd<0||fd>=FD_MAX||!fds[fd].used)return VFS_EBADF;struct descriptor*f=&fds[fd];struct node*n=content(f->node);if(!(f->flags&VFS_WRITE)||!access_ok(n,2))return VFS_EACCES;if(n->readonly)return VFS_EROFS;if(f->offset+size>n->quota)return VFS_EDQUOT;if(f->offset+size>FILE_MAX)return VFS_ENOSPC;uint64_t first=f->offset/4096,last=size?(f->offset+size-1)/4096:first,new_pages=0;for(uint64_t p=first;p<=last;p++)if(!n->pages[p])new_pages++;uint64_t limit=n->uid<16?quotas[n->uid]:FILE_MAX;if(vfs_user_usage(n->uid)+new_pages*4096>limit)return VFS_EDQUOT;for(uint64_t p=first;p<=last;p++)if(!n->pages[p]){n->pages[p]=kmalloc(4096);if(!n->pages[p])return VFS_ENOSPC;for(unsigned x=0;x<4096;x++)n->pages[p][x]=0;n->capacity+=4096;}const uint8_t*s=b;for(size_t i=0;i<size;i++){uint64_t pos=f->offset+i;n->pages[pos/4096][pos%4096]=s[i];}f->offset+=size;if(f->offset>n->size)n->size=f->offset;n->modified=now();return size;}
long vfs_seek(int fd,uint64_t o){if(fd<0||fd>=FD_MAX||!fds[fd].used)return VFS_EBADF;fds[fd].offset=o;return o;}
int vfs_mkdir(const char*p,uint32_t m){int n=create(p,true,m);return n<0?n:0;}
static void release_content(struct node*n){for(unsigned p=0;p<256;p++)if(n->pages[p]){kfree(n->pages[p]);n->pages[p]=0;}n->capacity=0;}
static void remove_tree(int n){for(int i=1;i<NODE_MAX;i++)if(nodes[i].used&&nodes[i].parent==n)remove_tree(i);if(nodes[n].hardlink){struct node*t=content(n);if(t->links)t->links--;}else if(nodes[n].links>1){int promote=-1;for(int i=1;i<NODE_MAX;i++)if(nodes[i].used&&nodes[i].hardlink&&nodes[i].backing==n){promote=i;break;}if(promote>=0){char name[48];int parent=nodes[promote].parent;scopy(name,nodes[promote].name,sizeof(name));nodes[promote]=nodes[n];nodes[promote].parent=parent;scopy(nodes[promote].name,name,sizeof(nodes[promote].name));nodes[promote].links--;for(int i=1;i<NODE_MAX;i++)if(nodes[i].used&&nodes[i].hardlink&&nodes[i].backing==n)nodes[i].backing=promote;}else release_content(&nodes[n]);}else release_content(&nodes[n]);nodes[n].used=false;}
static bool sticky_denied(int parent,int target){return(nodes[parent].mode&01000)&&!privileged(CAP_DAC)&&current_uid!=nodes[parent].uid&&current_uid!=content(target)->uid;}
int vfs_unlink(const char*p,bool recursive){int n=resolve(p,false);if(n<=0)return n?n:VFS_EINVAL;int parent=nodes[n].parent;if(!access_ok(&nodes[parent],3)||sticky_denied(parent,n))return VFS_EACCES;if(nodes[n].locked)return VFS_EBUSY;for(int i=1;i<NODE_MAX;i++)if(nodes[i].used&&nodes[i].parent==n&&!recursive)return VFS_EBUSY;remove_tree(n);return 0;}
int vfs_rename(const char*a,const char*b){int n=resolve(a,false);if(n<0)return n;char name[48];int parent=parent_of(b,name),old_parent=nodes[n].parent;if(parent<0)return parent;if(!access_ok(&nodes[old_parent],3)||!access_ok(&nodes[parent],3)||sticky_denied(old_parent,n))return VFS_EACCES;if(nodes[n].locked)return VFS_EBUSY;if(child(parent,name)>=0)return VFS_EEXIST;nodes[n].parent=parent;scopy(nodes[n].name,name,48);nodes[n].modified=now();return 0;}
int vfs_link(const char*t,const char*l,bool symbolic){int target=resolve(t,true);if(target<0)return target;int n=create(l,false,0777);if(n<0)return n;if(symbolic){nodes[n].symlink=true;scopy(nodes[n].target,t,256);}else{struct node*c=content(target);if(c->directory){nodes[n].used=false;return VFS_EISDIR;}nodes[n].hardlink=true;nodes[n].backing=(int)(c-nodes);c->links++;nodes[n].links=c->links;}return 0;}
static void fill(int n,struct vfs_stat*s){struct node*c=content(n);*s=(struct vfs_stat){.size=c->size,.allocated=c->capacity,.created=c->created,.modified=c->modified,.uid=c->uid,.gid=c->gid,.mode=c->mode,.links=c->links,.directory=c->directory,.symlink=nodes[n].symlink,.sparse=c->capacity<((c->size+4095)&~4095ull)};}
int vfs_stat_path(const char*p,struct vfs_stat*s){int n=resolve(p,false);if(n<0)return n;fill(n,s);return 0;}
int vfs_list(const char*p,vfs_list_fn cb,void*c){int n=resolve(p,true);if(n<0)return n;if(!nodes[n].directory)return VFS_ENOTDIR;if(!access_ok(&nodes[n],5))return VFS_EACCES;for(int i=1;i<NODE_MAX;i++)if(nodes[i].used&&nodes[i].parent==n){struct vfs_stat s;fill(i,&s);cb(nodes[i].name,&s,c);}return 0;}
int vfs_chmod(const char*p,uint32_t m){int n=resolve(p,true);if(n<0)return n;struct node*c=content(n);if(current_uid!=c->uid&&!privileged(CAP_CHOWN))return VFS_EACCES;if(!privileged(CAP_CHOWN))m&=~06000u;c->mode=m&07777;return 0;}int vfs_chown(const char*p,uint32_t u,uint32_t g){int n=resolve(p,true);if(n<0)return n;if(!privileged(CAP_CHOWN))return VFS_EACCES;struct node*c=content(n);c->uid=u;c->gid=g;c->mode&=~06000u;return 0;}
void vfs_set_credentials(uint32_t uid,uint32_t gid){vfs_set_security_context(uid,gid,0,0,uid?0022:0,0);}
void vfs_set_security_context(uint32_t uid,uint32_t gid,const uint32_t*g,uint32_t count,uint32_t mask,uint64_t caps){current_uid=uid;current_gid=gid;current_group_count=count>8?8:count;for(uint32_t i=0;i<current_group_count;i++)current_groups[i]=g[i];current_umask=mask&0777;current_caps=caps;}
void vfs_get_security_context(uint32_t*uid,uint32_t*gid,uint32_t*mask,uint64_t*caps){if(uid)*uid=current_uid;if(gid)*gid=current_gid;if(mask)*mask=current_umask;if(caps)*caps=current_caps;}
void vfs_capture_security_context(struct vfs_security_context*c){if(!c)return;c->uid=current_uid;c->gid=current_gid;c->group_count=current_group_count;c->creation_mask=current_umask;c->capabilities=current_caps;for(uint32_t i=0;i<current_group_count;i++)c->groups[i]=current_groups[i];}
void vfs_restore_security_context(const struct vfs_security_context*c){if(c)vfs_set_security_context(c->uid,c->gid,c->groups,c->group_count,c->creation_mask,c->capabilities);}
bool vfs_can_access(const char*p,uint32_t access){int n=resolve(p,true);if(n<0)return false;uint32_t bits=0;if(access&VFS_READ)bits|=4;if(access&VFS_WRITE)bits|=2;if(access&VFS_EXECUTE)bits|=1;return access_ok(content(n),bits);}
int vfs_lock(const char*p,bool locked){int n=resolve(p,true);if(n<0)return n;struct node*c=content(n);if(current_uid!=c->uid&&!privileged(CAP_CHOWN))return VFS_EACCES;c->locked=locked;return 0;}int vfs_set_quota(uint32_t uid,uint64_t bytes){if(!privileged(CAP_USERS)||uid>=16)return VFS_EACCES;quotas[uid]=bytes;return 0;}
static void make_path(int n,char*out,size_t cap){if(n==0){scopy(out,"/",cap);return;}int chain[NODE_MAX],count=0;while(n&&count<NODE_MAX){chain[count++]=n;n=nodes[n].parent;}size_t at=0;out[at++]='/';for(int i=count-1;i>=0;i--){for(size_t j=0;nodes[chain[i]].name[j]&&at+1<cap;j++)out[at++]=nodes[chain[i]].name[j];if(i&&at+1<cap)out[at++]='/';}out[at]=0;}
int vfs_chdir(const char*p){int n=resolve(p,true);if(n<0)return n;if(!nodes[n].directory)return VFS_ENOTDIR;if(!access_ok(&nodes[n],1))return VFS_EACCES;make_path(n,cwd,sizeof(cwd));return 0;}const char*vfs_cwd(void){return cwd;}
int vfs_mount(const char*s,const char*p,const char*t,bool ro){if(!privileged(CAP_MOUNT))return VFS_EACCES;int n=resolve(p,true);if(n<0)return n;if(nodes[n].mounted)return VFS_EBUSY;nodes[n].mounted=true;nodes[n].readonly=ro;scopy(nodes[n].source,s,32);scopy(nodes[n].type,t,12);return 0;}int vfs_unmount(const char*p){if(!privileged(CAP_MOUNT))return VFS_EACCES;int n=resolve(p,true);if(n<0)return n;if(!nodes[n].mounted)return VFS_EINVAL;nodes[n].mounted=false;nodes[n].readonly=false;return 0;}
uint64_t vfs_used_bytes(void){uint64_t n=0;for(int i=0;i<NODE_MAX;i++)if(nodes[i].used)n+=nodes[i].capacity;return n;}uint64_t vfs_capacity_bytes(void){return (uint64_t)NODE_MAX*FILE_MAX;}
int vfs_check(bool repair){(void)repair;for(int i=1;i<NODE_MAX;i++)if(nodes[i].used&&(!nodes[nodes[i].parent].used||!nodes[nodes[i].parent].directory||(nodes[i].hardlink&&(!nodes[i].backing||!nodes[nodes[i].backing].used))))return VFS_EIO;return 0;}
const char*vfs_error_string(int e){switch(e){case 0:return"OK";case VFS_ENOENT:return"NOT FOUND";case VFS_EACCES:return"PERMISSION DENIED";case VFS_EBUSY:return"BUSY";case VFS_EEXIST:return"ALREADY EXISTS";case VFS_ENOTDIR:return"NOT A DIRECTORY";case VFS_EISDIR:return"IS A DIRECTORY";case VFS_ENOSPC:return"NO SPACE";case VFS_EROFS:return"READ ONLY";case VFS_ENAMETOOLONG:return"NAME TOO LONG";case VFS_EDQUOT:return"QUOTA EXCEEDED";default:return"INVALID OR I/O ERROR";}}
static uint32_t self_test_failure;
static void test_result(bool passed,uint32_t step,bool*ok){if(!passed){if(!self_test_failure)self_test_failure=step;*ok=false;}}
static bool vfs_security_self_test(void){
    bool ok=true;struct vfs_stat st;uint32_t team=42;
    vfs_set_credentials(0,0);
    int fd=vfs_open("/tmp/owner-only",VFS_CREATE|VFS_WRITE|VFS_TRUNCATE);
    test_result(fd>=0,20,&ok);if(fd>=0)vfs_close(fd);
    test_result(vfs_chown("/tmp/owner-only",1,1)==0&&vfs_chmod("/tmp/owner-only",0600)==0,21,&ok);
    vfs_set_security_context(2,2,0,0,0022,0);
    test_result(vfs_open("/tmp/owner-only",VFS_READ)==VFS_EACCES,22,&ok);
    vfs_set_security_context(1,1,0,0,0077,0);
    fd=vfs_open("/tmp/masked",VFS_CREATE|VFS_WRITE);test_result(fd>=0,23,&ok);if(fd>=0)vfs_close(fd);
    fd=vfs_open("/tmp/sticky-owned",VFS_CREATE|VFS_WRITE);test_result(fd>=0,24,&ok);if(fd>=0)vfs_close(fd);
    vfs_set_security_context(2,2,0,0,0022,0);
    test_result(vfs_unlink("/tmp/sticky-owned",false)==VFS_EACCES,25,&ok);
    vfs_set_security_context(1,1,0,0,0022,0);
    test_result(vfs_unlink("/tmp/sticky-owned",false)==0,26,&ok);
    vfs_set_credentials(0,0);
    test_result(vfs_stat_path("/tmp/masked",&st)==0&&st.mode==0600,27,&ok);
    test_result(vfs_mkdir("/tmp/team",02770)==0&&vfs_chown("/tmp/team",0,team)==0&&vfs_chmod("/tmp/team",02770)==0,28,&ok);
    vfs_set_security_context(3,3,&team,1,0002,0);
    test_result(vfs_mkdir("/tmp/team/sub",0777)==0,29,&ok);
    vfs_set_credentials(0,0);
    test_result(vfs_stat_path("/tmp/team/sub",&st)==0&&st.gid==team&&(st.mode&02000)!=0,30,&ok);
    fd=vfs_open("/tmp/executable",VFS_CREATE|VFS_WRITE);if(fd>=0)vfs_close(fd);
    test_result(fd>=0&&vfs_chmod("/tmp/executable",0750)==0&&vfs_chown("/tmp/executable",0,team)==0,31,&ok);
    vfs_set_security_context(3,3,&team,1,0022,0);
    test_result(vfs_can_access("/tmp/executable",VFS_EXECUTE),32,&ok);
    vfs_set_security_context(4,4,0,0,0022,0);
    test_result(!vfs_can_access("/tmp/executable",VFS_EXECUTE)&&vfs_chdir("/devices")==VFS_EACCES,33,&ok);
    vfs_set_security_context(4,4,0,0,0022,CAP_DEVICE);
    test_result(vfs_chdir("/devices")==0,34,&ok);
    vfs_set_credentials(0,0);vfs_chdir("/");vfs_unlink("/tmp/owner-only",false);vfs_unlink("/tmp/masked",false);vfs_unlink("/tmp/team",true);vfs_unlink("/tmp/executable",false);
    return ok;
}
bool vfs_self_test(void){
    bool ok=true;self_test_failure=0;vfs_set_credentials(0,0);
    int fd=vfs_open("/tmp/sparse",VFS_CREATE|VFS_WRITE);char text[5]={'A','X','I','O','M'};
    test_result(fd>=0,1,&ok);if(fd>=0){vfs_seek(fd,8192);test_result(vfs_write(fd,text,5)==5,2,&ok);vfs_close(fd);}
    struct vfs_stat st;test_result(vfs_stat_path("/tmp/sparse",&st)==0&&st.sparse&&st.size==8197&&st.allocated==4096,3,&ok);
    test_result(vfs_link("/tmp/sparse","/tmp/hard",false)==0,4,&ok);
    fd=vfs_open("/tmp/hard",VFS_WRITE);test_result(fd>=0,5,&ok);if(fd>=0){vfs_seek(fd,8192);char x='X';test_result(vfs_write(fd,&x,1)==1,6,&ok);vfs_close(fd);}
    test_result(vfs_unlink("/tmp/sparse",false)==0,7,&ok);
    fd=vfs_open("/tmp/hard",VFS_READ);test_result(fd>=0,8,&ok);if(fd>=0){vfs_seek(fd,8192);char x=0;test_result(vfs_read(fd,&x,1)==1&&x=='X',9,&ok);vfs_close(fd);}
    test_result(vfs_lock("/tmp/hard",true)==0&&vfs_open("/tmp/hard",VFS_WRITE)==VFS_EBUSY&&vfs_lock("/tmp/hard",false)==0,10,&ok);
    fd=vfs_open("/tmp/private",VFS_CREATE|VFS_WRITE);test_result(fd>=0,11,&ok);if(fd>=0)vfs_close(fd);
    test_result(vfs_chmod("/tmp/private",0600)==0,12,&ok);vfs_set_credentials(1,1);test_result(vfs_open("/tmp/private",VFS_READ)==VFS_EACCES,13,&ok);
    vfs_set_credentials(0,0);test_result(vfs_set_quota(1,4096)==0,14,&ok);vfs_set_credentials(1,1);
    fd=vfs_open("/tmp/quota",VFS_CREATE|VFS_WRITE);test_result(fd>=0,15,&ok);if(fd>=0){char one=1;test_result(vfs_write(fd,&one,1)==1,16,&ok);vfs_seek(fd,4096);test_result(vfs_write(fd,&one,1)==VFS_EDQUOT,17,&ok);vfs_close(fd);}
    vfs_set_credentials(0,0);vfs_unlink("/tmp/hard",false);vfs_unlink("/tmp/private",false);vfs_unlink("/tmp/quota",false);
    test_result(vfs_security_self_test(),35,&ok);test_result(vfs_check(false)==0,36,&ok);return ok;
}
uint32_t vfs_self_test_failure(void){return self_test_failure;}
