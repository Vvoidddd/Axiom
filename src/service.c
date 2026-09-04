#include "service.h"
#include "account.h"
#include "hardware.h"
#include "process.h"
#include "vfs.h"
#include "log.h"

static struct service_info units[SERVICE_MAX];
static uint32_t count;

static bool equal(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return!*a&&!*b;}
static void copy(char*d,const char*s,uint32_t cap){uint32_t i=0;while(s[i]&&i+1<cap){d[i]=s[i];i++;}d[i]=0;}
static int find(const char*name){for(uint32_t i=0;i<count;i++)if(equal(units[i].name,name))return(int)i;return-1;}
static void define(const char*name,const char*executable,const char*account){struct service_info*u=&units[count++];*u=(struct service_info){.state=SERVICE_STOPPED,.enabled=true};copy(u->name,name,sizeof(u->name));copy(u->executable,executable,sizeof(u->executable));copy(u->account,account,sizeof(u->account));}

void service_init(void){count=0;define("system-logger","/system/bin/loggerd","system-logger");define("device-manager","/system/bin/deviced","device-manager");}
int service_start(const char*name){int i=find(name);if(i<0)return-1;struct service_info*u=&units[i];if(u->state==SERVICE_RUNNING||u->state==SERVICE_STARTING)return 0;const struct account_info*a=account_named(u->account);struct vfs_stat st;if(!a||a->type!=ACCOUNT_SERVICE||a->configured||vfs_stat_path(u->executable,&st)||st.directory||!(st.mode&0111))return-1;u->state=SERVICE_STARTING;int pid=process_spawn_service(u->executable,a->uid,a->gid);if(pid<0){u->state=SERVICE_FAILED;u->last_status=pid;LOG_ERROR("service spawn failed");return-1;}u->pid=(uint32_t)pid;u->state=SERVICE_RUNNING;u->last_heartbeat_ms=0;log_write("SERVICE","started");return 0;}
int service_stop(const char*name){int i=find(name);if(i<0)return-1;struct service_info*u=&units[i];if(u->state==SERVICE_RUNNING&&!process_stop_service(u->pid))return-1;u->pid=0;u->state=SERVICE_STOPPED;log_write("SERVICE","stopped");return 0;}
int service_restart(const char*name){int i=find(name);if(i<0)return-1;if(service_stop(name))return-1;units[i].restarts++;return service_start(name);}
bool service_ensure_started(void){bool ok=true;for(uint32_t i=0;i<count;i++)if(units[i].enabled&&units[i].state!=SERVICE_RUNNING){if(units[i].state==SERVICE_FAILED)units[i].restarts++;if(units[i].restarts>3||service_start(units[i].name))ok=false;}return ok;}
void service_heartbeat(uint32_t pid,uint64_t now){for(uint32_t i=0;i<count;i++)if(units[i].pid==pid&&units[i].state==SERVICE_RUNNING){units[i].last_heartbeat_ms=now;return;}}
void service_process_exited(uint32_t pid,int status){for(uint32_t i=0;i<count;i++)if(units[i].pid==pid){units[i].pid=0;units[i].last_status=status;units[i].state=status?SERVICE_FAILED:SERVICE_STOPPED;return;}}
const struct service_info*service_at(uint32_t i){return i<count?&units[i]:0;}
uint32_t service_count(void){return count;}
const char*service_state_name(enum service_state state){static const char*names[]={"stopped","starting","running","failed"};return state<=SERVICE_FAILED?names[state]:"invalid";}
bool service_self_test(void){if(count!=2)return false;for(uint32_t i=0;i<count;i++)if(!units[i].enabled||units[i].state!=SERVICE_STOPPED||!account_named(units[i].account))return false;return find("system-logger")==0&&find("missing")<0;}
