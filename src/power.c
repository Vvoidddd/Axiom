#include "power.h"
#include "account.h"
#include "block.h"
#include "log.h"
#include "process.h"
#include "service.h"
#include "vfs.h"

static struct power_status status;

void power_init(void){status=(struct power_status){.state=POWER_RUNNING};}
const struct power_status*power_get_status(void){return&status;}
const char*power_state_name(enum power_state state){static const char*names[]={"running","preparing","ready","failed"};return state<=POWER_FAILED?names[state]:"invalid";}

static bool prepare(enum power_action action,bool probe){if(action!=POWER_ACTION_REBOOT&&action!=POWER_ACTION_SHUTDOWN)return false;if(status.state==POWER_PREPARING)return false;status=(struct power_status){.state=POWER_PREPARING,.action=action};log_write("POWER",action==POWER_ACTION_REBOOT?"coordinated reboot started":"coordinated shutdown started");status.quiesced_processes=process_quiesce_for_power();log_write("POWER","user processes quiesced");status.accounts_saved=account_save();if(status.accounts_saved)log_write("POWER","account and credential state saved");status.services_stopped=service_stop_all();if(status.services_stopped)log_write("POWER","system services stopped");status.filesystem_checked=vfs_check(false)==0;if(status.filesystem_checked)log_write("POWER","filesystem metadata synchronized");status.caches_flushed=block_flush_all()==0;bool ok=status.accounts_saved&&status.services_stopped&&status.filesystem_checked&&status.caches_flushed;if(ok){status.state=POWER_READY;log_write("POWER","all application and storage writes flushed");ok=log_flush();}else{status.state=POWER_FAILED;log_write("ERROR","power coordination refused unsafe transition");}if(probe)status=(struct power_status){.state=POWER_RUNNING};return ok;}
bool power_prepare(enum power_action action){return prepare(action,false);}
bool power_self_test(void){if(status.state!=POWER_RUNNING)return false;bool ok=prepare(POWER_ACTION_REBOOT,true);return ok&&status.state==POWER_RUNNING;}
