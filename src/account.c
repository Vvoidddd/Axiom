#include "account.h"
#include "acpi.h"
#include "vfs.h"
#include <stddef.h>

struct account_record {struct account_info info;uint64_t salt,verifier;uint32_t failures;uint64_t retry_at;};
static struct account_record records[ACCOUNT_MAX];
static uint32_t used,next_uid;
static int session=-1;
static uint64_t work[8192]; /* 64 KiB bounded memory-hard verifier workspace. */

static bool equal(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static size_t length(const char*s){size_t n=0;while(s[n])n++;return n;}
static void copy(char*d,const char*s,size_t cap){size_t i=0;while(s[i]&&i+1<cap){d[i]=s[i];i++;}d[i]=0;}
static uint64_t mix(uint64_t x){x^=x>>30;x*=0xbf58476d1ce4e5b9ull;x^=x>>27;x*=0x94d049bb133111ebull;return x^(x>>31);}
static uint64_t password_hash(const char*p,uint64_t salt){uint64_t x=mix(salt^0xa7105ec0deull);for(size_t i=0;p[i];i++)x=mix(x^(uint8_t)p[i]^(i<<24));for(uint32_t i=0;i<8192;i++)work[i]=x=mix(x+i+salt);for(uint32_t round=0;round<3;round++)for(uint32_t i=0;i<8192;i++){uint32_t j=(uint32_t)(x^work[i])&8191;x=mix(x+work[j]+i);work[i]^=x;}uint64_t out=mix(x^work[x&8191]);for(uint32_t i=0;i<8192;i++)work[i]=0;return out;}
static int find_name(const char*n){for(uint32_t i=0;i<used;i++)if(equal(records[i].info.name,n))return(int)i;return-1;}
static int find_uid(uint32_t uid){for(uint32_t i=0;i<used;i++)if(records[i].info.uid==uid)return(int)i;return-1;}
void account_init(void){used=1;next_uid=1000;session=-1;records[0]=(struct account_record){.info={.uid=0,.gid=0,.type=ACCOUNT_RECOVERY,.enabled=true,.configured=false,.name="recovery"},.salt=entropy_seed()};vfs_set_credentials(0,0);}
int account_create(const char*n,const char*p,enum account_type type,uint32_t*uid){size_t nl=length(n),pl=length(p);if(nl<2||nl>=32||pl<8||pl>72||used>=ACCOUNT_MAX||find_name(n)>=0||type==ACCOUNT_RECOVERY)return-1;for(size_t i=0;i<nl;i++)if(!((n[i]>='a'&&n[i]<='z')||(i&&n[i]>='0'&&n[i]<='9')||n[i]=='-'))return-1;struct account_record*r=&records[used++];*r=(struct account_record){.info={.uid=next_uid++,.gid=type==ACCOUNT_ADMIN?100:1000,.type=type,.enabled=true,.configured=true},.salt=entropy_seed()};copy(r->info.name,n,sizeof(r->info.name));r->verifier=password_hash(p,r->salt);if(uid)*uid=r->info.uid;return 0;}
int account_authenticate(const char*n,const char*p,uint64_t now){int i=find_name(n);if(i<0)return-1;struct account_record*r=&records[i];if(!r->info.enabled||!r->info.configured||now<r->retry_at)return-1;uint64_t got=password_hash(p,r->salt),difference=got^r->verifier;difference|=(uint64_t)-(int64_t)difference;if(difference>>63){if(r->failures<8)r->failures++;uint64_t delay=250ull<<(r->failures>5?5:r->failures);r->retry_at=now+delay;return-1;}r->failures=0;r->retry_at=0;return(int)r->info.uid;}
int account_disable(const char*n,bool disabled){int i=find_name(n);if(i<=0)return-1;records[i].info.enabled=!disabled;if(session==i&&disabled)account_end_session();return 0;}
bool account_begin_session(uint32_t uid){int i=find_uid(uid);if(i<0||!records[i].info.enabled)return false;session=i;vfs_set_credentials(records[i].info.uid,records[i].info.gid);return true;}
void account_end_session(void){session=-1;vfs_set_credentials(0,0);}
const struct account_info*account_current(void){return session<0?0:&records[session].info;}
const struct account_info*account_at(uint32_t i){return i<used?&records[i].info:0;}
uint32_t account_count(void){return used;}
bool account_self_test(void){uint32_t uid;if(account_create("test-user","correct-horse",ACCOUNT_STANDARD,&uid)||uid!=1000)return false;if(account_authenticate("test-user","wrong-pass",0)>=0)return false;if(account_authenticate("test-user","correct-horse",100000)!=(int)uid)return false;if(!account_begin_session(uid)||!account_current()||account_current()->uid!=uid)return false;account_end_session();if(account_disable("test-user",true)||account_authenticate("test-user","correct-horse",200000)>=0)return false;return account_count()==2;}
