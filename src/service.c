#include "service.h"
#include "account.h"
#include "hardware.h"
#include "process.h"
#include "vfs.h"
#include "log.h"
#include "config.h"
#include "device.h"

static struct service_info units[SERVICE_MAX];
static uint32_t count;

static bool equal(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return!*a&&!*b;}
static void copy(char*d,const char*s,uint32_t cap){uint32_t i=0;while(s[i]&&i+1<cap){d[i]=s[i];i++;}d[i]=0;}
static uint32_t decimal(const char*s,uint32_t fallback){uint32_t value=0;if(!s||!*s)return fallback;while(*s){if(*s<'0'||*s>'9')return fallback;value=value*10+(uint32_t)(*s++-'0');}return value;}
static void lifecycle(const char*verb,const char*name){char message[72];uint32_t at=0;while(verb[at]&&at+1<sizeof(message)){message[at]=verb[at];at++;}if(at+1<sizeof(message))message[at++]=' ';for(uint32_t i=0;name[i]&&at+1<sizeof(message);i++)message[at++]=name[i];message[at]=0;log_write("SERVICE",message);}
static int find(const char*name){for(uint32_t i=0;i<count;i++)if(equal(units[i].name,name))return(int)i;return-1;}
static void define(const char*name,const char*executable,const char*account,bool always,uint32_t timeout,uint32_t limit){struct service_info*u=&units[count++];*u=(struct service_info){.state=SERVICE_STOPPED,.enabled=true,.restart_always=always,.heartbeat_timeout_ms=timeout,.restart_limit=limit};copy(u->name,name,sizeof(u->name));copy(u->executable,executable,sizeof(u->executable));copy(u->account,account,sizeof(u->account));}

static bool load_unit(const char*path){struct config_document d;if(!config_load(path,1,0,&d))return false;const char*name=config_get(&d,"name"),*executable=config_get(&d,"executable"),*account=config_get(&d,"account"),*restart=config_get(&d,"restart");uint32_t timeout=decimal(config_get(&d,"heartbeat_ms"),2000),limit=decimal(config_get(&d,"restart_limit"),3);if(!name||!executable||!account||!restart||(!equal(restart,"always")&&!equal(restart,"never"))||timeout<500||timeout>30000||limit>10||count>=SERVICE_MAX)return false;define(name,executable,account,equal(restart,"always"),timeout,limit);return true;}
bool service_init(void){count=0;return load_unit("/system/services/system-logger.svc")&&load_unit("/system/services/device-manager.svc");}
int service_start(const char*name){int i=find(name);if(i<0)return-1;struct service_info*u=&units[i];if(u->state==SERVICE_RUNNING||u->state==SERVICE_STARTING)return 0;const struct account_info*a=account_named(u->account);struct vfs_stat st;if(!a||a->type!=ACCOUNT_SERVICE||a->configured||vfs_stat_path(u->executable,&st)||st.directory||!(st.mode&0111))return-1;u->state=SERVICE_STARTING;int pid=process_spawn_service(u->executable,a->uid,a->gid);if(pid<0){u->state=SERVICE_FAILED;u->last_status=pid;LOG_ERROR("service spawn failed");return-1;}u->pid=(uint32_t)pid;u->state=SERVICE_RUNNING;u->manual_stop=false;u->started_ms=hardware_uptime_ms();u->last_heartbeat_ms=u->started_ms;lifecycle("started",u->name);return 0;}
int service_stop(const char*name){int i=find(name);if(i<0)return-1;struct service_info*u=&units[i];if(u->state==SERVICE_RUNNING&&!process_stop_service(u->pid))return-1;u->pid=0;u->state=SERVICE_STOPPED;u->manual_stop=true;lifecycle("stopped",u->name);return 0;}
int service_restart(const char*name){int i=find(name);if(i<0)return-1;if(service_stop(name))return-1;units[i].restarts++;return service_start(name);}
bool service_ensure_started(void){bool ok=true;for(uint32_t i=0;i<count;i++){struct service_info*u=&units[i];if(!u->enabled||u->state==SERVICE_RUNNING||u->manual_stop)continue;if(u->state==SERVICE_FAILED){if(!u->restart_always||u->restarts>=u->restart_limit){ok=false;continue;}u->restarts++;}if(service_start(u->name))ok=false;}return ok;}
bool service_supervise(uint64_t now){bool ok=true;for(uint32_t i=0;i<count;i++){struct service_info*u=&units[i];if(u->state==SERVICE_RUNNING&&now-u->last_heartbeat_ms>u->heartbeat_timeout_ms){process_stop_service(u->pid);u->pid=0;u->state=SERVICE_FAILED;u->last_status=-110;lifecycle("heartbeat-timeout",u->name);}if(u->state==SERVICE_FAILED&&!u->manual_stop){if(!u->restart_always||u->restarts>=u->restart_limit){ok=false;continue;}u->restarts++;if(service_start(u->name))ok=false;}}return ok;}
void service_heartbeat(uint32_t pid,uint64_t now){for(uint32_t i=0;i<count;i++)if(units[i].pid==pid&&units[i].state==SERVICE_RUNNING){units[i].last_heartbeat_ms=now;if(equal(units[i].name,"system-logger"))log_service_tick();else if(equal(units[i].name,"device-manager"))device_manager_scan();service_supervise(now);return;}}
void service_process_exited(uint32_t pid,int status){for(uint32_t i=0;i<count;i++)if(units[i].pid==pid){units[i].pid=0;units[i].last_status=status;units[i].state=units[i].restart_always?SERVICE_FAILED:SERVICE_STOPPED;lifecycle("exited",units[i].name);return;}}
bool service_stop_all(void){bool ok=true;for(uint32_t i=0;i<count;i++)if(units[i].state==SERVICE_RUNNING&&service_stop(units[i].name))ok=false;return ok;}
const struct service_info*service_at(uint32_t i){return i<count?&units[i]:0;}
uint32_t service_count(void){return count;}
const char*service_state_name(enum service_state state){static const char*names[]={"stopped","starting","running","failed"};return state<=SERVICE_FAILED?names[state]:"invalid";}
bool service_self_test(void){if(count!=2)return false;for(uint32_t i=0;i<count;i++)if(!units[i].enabled||units[i].state!=SERVICE_STOPPED||!units[i].restart_always||units[i].heartbeat_timeout_ms<500||!units[i].restart_limit||!account_named(units[i].account))return false;return find("system-logger")==0&&find("device-manager")==1&&find("missing")<0;}
