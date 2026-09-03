#include "package.h"
#include "vfs.h"
#include <axiom/syscall.h>
#define PACKAGE_MAX 16
static struct package_info packages[PACKAGE_MAX];static uint32_t count;
static size_t length(const char*s){size_t n=0;while(s[n])n++;return n;}
static bool equal(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return!*a&&!*b;}
static void copy(char*d,const char*s,size_t cap){size_t i=0;while(s[i]&&i+1<cap){d[i]=s[i];i++;}d[i]=0;}
static bool valid_name(const char*s){if(!*s)return false;for(;*s;s++)if(!((*s>='a'&&*s<='z')||(*s>='0'&&*s<='9')||*s=='-'||*s=='_'))return false;return true;}
static bool valid_version(const char*s){unsigned dots=0,digits=0;for(;*s;s++){if(*s=='.'){if(!digits)return false;dots++;digits=0;}else if(*s>='0'&&*s<='9')digits++;else return false;}return dots==2&&digits>0;}
static bool prefix(const char*s,const char*p){while(*p)if(*s++!=*p++)return false;return true;}
static bool field(const char*text,const char*key,char*out,size_t cap){size_t keylen=length(key);while(*text){const char*line=text;while(*text&&*text!='\n')text++;if((size_t)(text-line)>keylen&&line[keylen]=='='){bool match=true;for(size_t i=0;i<keylen;i++)if(line[i]!=key[i])match=false;if(match){size_t n=(size_t)(text-line)-keylen-1;if(n>=cap)return false;for(size_t i=0;i<n;i++)out[i]=line[keylen+1+i];out[n]=0;return true;}}if(*text)text++;}return false;}
static bool load_manifest(const char*path){if(count>=PACKAGE_MAX)return false;int fd=vfs_open(path,VFS_READ);if(fd<0)return false;char text[512];long n=vfs_read(fd,text,sizeof(text)-1);vfs_close(fd);if(n<=0)return false;text[n]=0;struct package_info p={0};char abi[16],kind[16];if(!field(text,"name",p.name,sizeof(p.name))||!field(text,"version",p.version,sizeof(p.version))||!field(text,"executable",p.executable,sizeof(p.executable))||!field(text,"abi",abi,sizeof(abi))||!field(text,"type",kind,sizeof(kind)))return false;if(!valid_name(p.name)||!valid_version(p.version)||p.executable[0]!='/')return false;p.abi=0;for(char*c=abi;*c>='0'&&*c<='9';c++)p.abi=p.abi*10+(uint32_t)(*c-'0');if(p.abi!=AXIOM_SYSCALL_ABI_VERSION)return false;p.system_package=equal(kind,"system");bool app=equal(kind,"app");if((p.system_package&&!prefix(p.executable,"/system/"))||(app&&!prefix(p.executable,"/apps/"))||(!p.system_package&&!app))return false;struct vfs_stat stat;if(vfs_stat_path(p.executable,&stat)||stat.directory)return false;for(uint32_t i=0;i<count;i++)if(equal(packages[i].name,p.name))return false;packages[count++]=p;return true;}
void package_init(void){count=0;load_manifest("/system/packages/init.pkg");load_manifest("/system/packages/shell.pkg");load_manifest("/system/packages/hello.pkg");}
uint32_t package_count(void){return count;}
const struct package_info*package_at(uint32_t i){return i<count?&packages[i]:0;}
bool package_self_test(void){if(count!=3)return false;for(uint32_t i=0;i<count;i++)if(packages[i].abi!=AXIOM_SYSCALL_ABI_VERSION||!valid_name(packages[i].name)||!valid_version(packages[i].version))return false;char value[8];copy(value,"1.2.3",sizeof(value));return valid_version(value);}
