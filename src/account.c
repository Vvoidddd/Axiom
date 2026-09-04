#include "account.h"
#include "acpi.h"
#include "vfs.h"
#include <stddef.h>
#include <stdint.h>

struct account_record {struct account_info info;uint64_t salt,verifier;uint32_t failures;uint64_t retry_at;};
static struct account_record records[ACCOUNT_MAX];
static uint32_t used,next_uid;
static int session=-1;
static uint64_t work[8192]; /* 64 KiB bounded memory-hard verifier workspace. */
struct db_header {uint64_t magic;uint32_t version,count,next;uint64_t checksum;};
#define DB_MAGIC 0x4143434f554e5453ull
#define DB_PATH "/system/config/accounts.db"
#define DB_BACKUP "/system/config/accounts.bak"

static bool equal(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static size_t length(const char*s){size_t n=0;while(s[n])n++;return n;}
static void copy(char*d,const char*s,size_t cap){size_t i=0;while(s[i]&&i+1<cap){d[i]=s[i];i++;}d[i]=0;}
static uint64_t mix(uint64_t x){x^=x>>30;x*=0xbf58476d1ce4e5b9ull;x^=x>>27;x*=0x94d049bb133111ebull;return x^(x>>31);}
static uint64_t password_hash(const char*p,uint64_t salt){uint64_t x=mix(salt^0xa7105ec0deull);for(size_t i=0;p[i];i++)x=mix(x^(uint8_t)p[i]^(i<<24));for(uint32_t i=0;i<8192;i++)work[i]=x=mix(x+i+salt);for(uint32_t round=0;round<3;round++)for(uint32_t i=0;i<8192;i++){uint32_t j=(uint32_t)(x^work[i])&8191;x=mix(x+work[j]+i);work[i]^=x;}uint64_t out=mix(x^work[x&8191]);for(uint32_t i=0;i<8192;i++)work[i]=0;return out;}
static int find_name(const char*n){for(uint32_t i=0;i<used;i++)if(equal(records[i].info.name,n))return(int)i;return-1;}
static int find_uid(uint32_t uid){for(uint32_t i=0;i<used;i++)if(records[i].info.uid==uid)return(int)i;return-1;}
static uint64_t checksum(const void*data,size_t size,uint64_t value){const uint8_t*p=data;for(size_t i=0;i<size;i++){value^=p[i];value*=1099511628211ull;}return value;}
static bool write_db(const char*path,const struct db_header*h){int fd=vfs_open(path,VFS_WRITE|VFS_CREATE|VFS_TRUNCATE);if(fd<0)return false;bool ok=vfs_write(fd,h,sizeof(*h))==(long)sizeof(*h)&&vfs_write(fd,records,h->count*sizeof(records[0]))==(long)(h->count*sizeof(records[0]));vfs_close(fd);if(ok)vfs_chmod(path,0600);return ok;}
static bool read_db(const char*path,struct db_header*h,struct account_record*out){int fd=vfs_open(path,VFS_READ);if(fd<0)return false;bool ok=vfs_read(fd,h,sizeof(*h))==(long)sizeof(*h)&&h->magic==DB_MAGIC&&h->version==1&&h->count>=1&&h->count<=ACCOUNT_MAX&&vfs_read(fd,out,h->count*sizeof(out[0]))==(long)(h->count*sizeof(out[0]));vfs_close(fd);if(!ok)return false;uint64_t expected=h->checksum;h->checksum=0;uint64_t sum=checksum(h,sizeof(*h),1469598103934665603ull);sum=checksum(out,h->count*sizeof(out[0]),sum);h->checksum=expected;return sum==expected&&out[0].info.uid==0&&out[0].info.type==ACCOUNT_RECOVERY;}
void account_init(void){used=1;next_uid=1000;session=-1;records[0]=(struct account_record){.info={.uid=0,.gid=0,.type=ACCOUNT_RECOVERY,.enabled=true,.configured=false,.name="recovery"},.salt=entropy_seed()};vfs_set_credentials(0,0);}
int account_create(const char*n,const char*p,enum account_type type,uint32_t*uid){size_t nl=length(n),pl=length(p);if(nl<2||nl>=32||pl<8||pl>72||used>=ACCOUNT_MAX||find_name(n)>=0||type==ACCOUNT_RECOVERY)return-1;for(size_t i=0;i<nl;i++)if(!((n[i]>='a'&&n[i]<='z')||(i&&n[i]>='0'&&n[i]<='9')||n[i]=='-'))return-1;struct account_record*r=&records[used++];*r=(struct account_record){.info={.uid=next_uid++,.gid=type==ACCOUNT_ADMIN?100:1000,.type=type,.enabled=true,.configured=true},.salt=entropy_seed()};copy(r->info.name,n,sizeof(r->info.name));r->verifier=password_hash(p,r->salt);if(uid)*uid=r->info.uid;return 0;}
int account_authenticate(const char*n,const char*p,uint64_t now){int i=find_name(n);if(i<0)return-1;struct account_record*r=&records[i];if(!r->info.enabled||!r->info.configured||now<r->retry_at)return-1;uint64_t got=password_hash(p,r->salt),difference=got^r->verifier;difference|=(uint64_t)-(int64_t)difference;if(difference>>63){if(r->failures<8)r->failures++;uint64_t delay=250ull<<(r->failures>5?5:r->failures);r->retry_at=now+delay;return-1;}r->failures=0;r->retry_at=0;return(int)r->info.uid;}
int account_disable(const char*n,bool disabled){int i=find_name(n);if(i<=0)return-1;records[i].info.enabled=!disabled;if(session==i&&disabled)account_end_session();return 0;}
int account_remove(const char*n){int i=find_name(n);if(i<=0||session==i)return-1;for(uint32_t x=(uint32_t)i+1;x<used;x++)records[x-1]=records[x];used--;return 0;}
int account_set_password(const char*n,const char*p){int i=find_name(n);size_t size=length(p);if(i<0||size<8||size>72)return-1;records[i].salt=entropy_seed();records[i].verifier=password_hash(p,records[i].salt);records[i].info.configured=true;records[i].failures=0;records[i].retry_at=0;return 0;}
int account_set_type(const char*n,enum account_type type){int i=find_name(n);if(i<=0||type==ACCOUNT_RECOVERY)return-1;records[i].info.type=type;records[i].info.gid=type==ACCOUNT_ADMIN?100:(type==ACCOUNT_SERVICE?200:1000);return 0;}
bool account_save(void){vfs_set_credentials(0,0);int e=vfs_mkdir("/system/config",0700);if(e&&e!=VFS_EEXIST)return false;struct db_header old;struct account_record previous[ACCOUNT_MAX];if(read_db(DB_PATH,&old,previous)){struct account_record current[ACCOUNT_MAX];for(uint32_t i=0;i<used;i++)current[i]=records[i];uint32_t current_used=used;for(uint32_t i=0;i<old.count;i++)records[i]=previous[i];vfs_unlink(DB_BACKUP,false);write_db(DB_BACKUP,&old);for(uint32_t i=0;i<current_used;i++)records[i]=current[i];used=current_used;}struct db_header h={DB_MAGIC,1,used,next_uid,0};h.checksum=checksum(&h,sizeof(h),1469598103934665603ull);h.checksum=checksum(records,used*sizeof(records[0]),h.checksum);if(!write_db("/system/config/accounts.tmp",&h))return false;vfs_unlink(DB_PATH,false);return vfs_rename("/system/config/accounts.tmp",DB_PATH)==0;}
bool account_load(void){struct db_header h;struct account_record loaded[ACCOUNT_MAX];if(!read_db(DB_PATH,&h,loaded)&&!read_db(DB_BACKUP,&h,loaded))return false;for(uint32_t i=0;i<h.count;i++){records[i]=loaded[i];records[i].failures=0;records[i].retry_at=0;}used=h.count;next_uid=h.next;session=-1;vfs_set_credentials(0,0);return true;}
bool account_has_admin(void){for(uint32_t i=1;i<used;i++)if(records[i].info.enabled&&records[i].info.configured&&records[i].info.type==ACCOUNT_ADMIN)return true;return false;}
bool account_begin_session(uint32_t uid){int i=find_uid(uid);if(i<0||!records[i].info.enabled)return false;session=i;vfs_set_credentials(records[i].info.uid,records[i].info.gid);return true;}
void account_end_session(void){session=-1;vfs_set_credentials(0,0);}
const struct account_info*account_current(void){return session<0?0:&records[session].info;}
const struct account_info*account_at(uint32_t i){return i<used?&records[i].info:0;}
uint32_t account_count(void){return used;}
const char*account_type_name(enum account_type type){static const char*names[]={"standard","admin","service","recovery"};return type<=ACCOUNT_RECOVERY?names[type]:"invalid";}
bool account_self_test(void){struct account_record saved[ACCOUNT_MAX];for(uint32_t i=0;i<used;i++)saved[i]=records[i];uint32_t saved_used=used,saved_next=next_uid;bool ok=true;uint32_t uid;if(account_create("test-user","correct-horse",ACCOUNT_STANDARD,&uid)||uid<1000)ok=false;if(account_authenticate("test-user","wrong-pass",0)>=0)ok=false;if(account_authenticate("test-user","correct-horse",100000)!=(int)uid)ok=false;if(!account_begin_session(uid)||!account_current()||account_current()->uid!=uid)ok=false;account_end_session();if(account_disable("test-user",true)||account_authenticate("test-user","correct-horse",200000)>=0)ok=false;if(!account_save()||!account_load()||account_count()!=saved_used+1)ok=false;if(account_remove("test-user")||account_count()!=saved_used)ok=false;for(uint32_t i=0;i<saved_used;i++)records[i]=saved[i];used=saved_used;next_uid=saved_next;session=-1;vfs_set_credentials(0,0);return ok;}
