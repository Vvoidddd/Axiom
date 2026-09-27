#include "log.h"
#include "block.h"
#include "console.h"
#include "hardware.h"
#include "io.h"
#include "security.h"
#include "vfs.h"
#include <stddef.h>

#define LOG_LINES 32
#define LOG_LENGTH 80
#define JOURNAL_ROTATE_BYTES 4096u
#define JOURNAL_GENERATIONS 3u

static char history[LOG_LINES][LOG_LENGTH];
static unsigned next_line, line_count;
static bool persistent_ready;
static uint32_t rotations;
static const char *journal_path = "/var/log/journal/system.log";

static size_t length(const char*s){size_t n=0;while(s&&s[n])n++;return n;}
static void copy(char*d,const char*s,size_t cap){size_t n=0;if(!cap)return;while(s&&s[n]&&n+1<cap){d[n]=s[n];n++;}d[n]=0;}
static void append(char*d,const char*s,size_t cap){size_t n=length(d);copy(d+n,s,n<cap?cap-n:0);}
static void serial_char(char c){while((inb(0x3fd)&0x20u)==0){}outb(0x3f8,(uint8_t)c);}
static void serial_text(const char*s){while(*s)serial_char(*s++);}
static void copy_line(char*out,const char*level,const char*message){unsigned n=0;out[n++]='[';while(*level&&n+1<LOG_LENGTH)out[n++]=*level++;if(n+2<LOG_LENGTH){out[n++]=']';out[n++]=' ';}while(*message&&n+1<LOG_LENGTH)out[n++]=*message++;out[n]=0;}
static void decimal(uint64_t value,char*out,size_t cap){char r[24];size_t n=0;do{r[n++]=(char)('0'+value%10);value/=10;}while(value&&n<sizeof(r));size_t at=0;while(n&&at+1<cap)out[at++]=r[--n];out[at]=0;}
static void root(struct vfs_security_context*saved){vfs_capture_security_context(saved);vfs_set_security_context(0,0,0,0,0022,CAP_ALL);}
static bool make_generation(unsigned generation,char*out,size_t cap){copy(out,journal_path,cap);size_t n=length(out);if(generation){if(n+3>cap)return false;out[n++]='.';out[n++]=(char)('0'+generation);out[n]=0;}return true;}

static bool rotate_journal(bool force){if(!persistent_ready)return false;struct vfs_security_context saved;root(&saved);struct vfs_stat stat;bool needed=vfs_stat_path(journal_path,&stat)==0&&(force||stat.size>=JOURNAL_ROTATE_BYTES);if(!needed){vfs_restore_security_context(&saved);return true;}char source[96],destination[96];make_generation(JOURNAL_GENERATIONS,destination,sizeof(destination));vfs_unlink(destination,false);for(unsigned n=JOURNAL_GENERATIONS;n>1;n--){make_generation(n-1,source,sizeof(source));make_generation(n,destination,sizeof(destination));if(vfs_stat_path(source,&stat)==0&&vfs_rename(source,destination)){vfs_restore_security_context(&saved);return false;}}make_generation(1,destination,sizeof(destination));if(vfs_rename(journal_path,destination)){vfs_restore_security_context(&saved);return false;}rotations++;vfs_restore_security_context(&saved);return true;}

static bool journal_append(const char*line){if(!persistent_ready)return true;if(!rotate_journal(false))return false;struct vfs_security_context saved;root(&saved);struct vfs_stat stat;uint64_t offset=vfs_stat_path(journal_path,&stat)?0:stat.size;int fd=vfs_open(journal_path,VFS_WRITE|VFS_CREATE);bool ok=fd>=0;if(ok){char timestamp[24],entry[160];decimal(hardware_uptime_ms(),timestamp,sizeof(timestamp));entry[0]=0;append(entry,timestamp,sizeof(entry));append(entry," ",sizeof(entry));append(entry,line,sizeof(entry));append(entry,"\n",sizeof(entry));ok=vfs_seek(fd,offset)>=0&&vfs_write(fd,entry,length(entry))==(long)length(entry);vfs_close(fd);if(ok){vfs_chmod(journal_path,0600);vfs_chown(journal_path,0,0);}}vfs_restore_security_context(&saved);return ok;}

void log_init(void){outb(0x3f9,0);outb(0x3fb,0x80);outb(0x3f8,3);outb(0x3f9,0);outb(0x3fb,3);outb(0x3fa,0xc7);outb(0x3fc,0x0b);LOG_INFO("serial logger online");}
void log_write(const char*level,const char*message){copy_line(history[next_line],level,message);serial_text(history[next_line]);serial_text("\r\n");journal_append(history[next_line]);next_line=(next_line+1)%LOG_LINES;if(line_count<LOG_LINES)line_count++;}
void log_hex(uint64_t value){static const char h[]="0123456789ABCDEF";serial_text("0x");for(int shift=60;shift>=0;shift-=4)serial_char(h[(value>>shift)&15]);}
void log_dump(void){unsigned start=(next_line+LOG_LINES-line_count)%LOG_LINES;for(unsigned i=0;i<line_count;i++){console_write(history[(start+i)%LOG_LINES]);console_putc('\n');}}

bool log_enable_persistence(void){if(persistent_ready)return true;struct vfs_security_context saved;root(&saved);int e=vfs_mkdir("/var/log/journal",0700);bool ok=e==0||e==VFS_EEXIST;if(ok){vfs_chown("/var/log/journal",0,0);vfs_chmod("/var/log/journal",0700);}vfs_restore_security_context(&saved);if(!ok)return false;persistent_ready=true;unsigned start=(next_line+LOG_LINES-line_count)%LOG_LINES;for(unsigned i=0;i<line_count;i++)if(!journal_append(history[(start+i)%LOG_LINES])){persistent_ready=false;return false;}LOG_INFO("persistent system journal online");return true;}

static bool contains_level(const char*line,const char*filter){if(!filter||!*filter)return true;char wanted[24];wanted[0]='[';size_t n=0;while(filter[n]&&n+2<sizeof(wanted)){wanted[n+1]=filter[n];n++;}wanted[n+1]=']';wanted[n+2]=0;for(size_t i=0;line[i];i++){size_t j=0;while(wanted[j]&&line[i+j]==wanted[j])j++;if(!wanted[j])return true;}return false;}
static void dump_file(const char*path,const char*filter){int fd=vfs_open(path,VFS_READ);if(fd<0)return;char line[192];size_t used=0;char c;while(vfs_read(fd,&c,1)==1){if(c=='\n'){line[used]=0;if(contains_level(line,filter)){console_write(line);console_putc('\n');}used=0;}else if(used+1<sizeof(line))line[used++]=c;}if(used){line[used]=0;if(contains_level(line,filter)){console_write(line);console_putc('\n');}}vfs_close(fd);}
void log_journal_dump(const char*filter){struct vfs_security_context saved;root(&saved);char path[96];for(unsigned n=JOURNAL_GENERATIONS;n>0;n--){make_generation(n,path,sizeof(path));dump_file(path,filter);}dump_file(journal_path,filter);vfs_restore_security_context(&saved);}
void log_service_tick(void){if(persistent_ready){rotate_journal(false);block_flush_all();}}
bool log_flush(void){return block_flush_all()==0;}
uint32_t log_rotation_count(void){return rotations;}
bool log_self_test(void){if(!persistent_ready)return false;struct vfs_stat before,after,rotated;if(vfs_stat_path(journal_path,&before))return false;log_write("LOGTEST","journal filter and append probe");if(vfs_stat_path(journal_path,&after)||after.size<=before.size||after.mode!=0600)return false;if(!rotate_journal(true)||vfs_stat_path("/var/log/journal/system.log.1",&rotated)||!rotated.size)return false;log_write("PASS","persistent journal rotation and permissions verified");return vfs_stat_path(journal_path,&after)==0&&after.size>0&&rotations>0;}

__attribute__((noreturn)) void panic(const char*message,uint64_t vector,uint64_t error,uint64_t rip){__asm__ volatile("cli");LOG_ERROR(message);serial_text("vector=");log_hex(vector);serial_text(" error=");log_hex(error);serial_text(" rip=");log_hex(rip);serial_text("\r\n");console_write("\nKERNEL PANIC: ");console_write(message);console_write("\nVECTOR: ");console_write_u64(vector);console_write(" ERROR: ");console_write_hex(error);console_write("\nRIP: ");console_write_hex(rip);console_putc('\n');for(;;)__asm__ volatile("hlt");}
__attribute__((noreturn)) void panic_frame(const char*message,const uint64_t*f){__asm__ volatile("cli");LOG_ERROR(message);static const char*names[]={"R15","R14","R13","R12","R11","R10","R9","R8","RSI","RDI","RBP","RDX","RCX","RBX","RAX"};for(unsigned i=0;i<15;i++){serial_text(names[i]);serial_char('=');log_hex(f[i]);serial_text((i%3)==2?"\r\n":" ");}serial_text("VECTOR=");log_hex(f[15]);serial_text(" ERROR=");log_hex(f[16]);serial_text(" RIP=");log_hex(f[17]);serial_text("\r\n");console_write("\nKERNEL PANIC: ");console_write(message);console_write("\nVECTOR: ");console_write_u64(f[15]);console_write(" ERROR: ");console_write_hex(f[16]);console_write("\nRIP: ");console_write_hex(f[17]);console_write("\nRAX: ");console_write_hex(f[14]);console_write(" RBX: ");console_write_hex(f[13]);console_write("\nRCX: ");console_write_hex(f[12]);console_write(" RDX: ");console_write_hex(f[11]);console_putc('\n');for(;;)__asm__ volatile("hlt");}
void kernel_assert(bool condition,const char*expression,const char*file,uint64_t line){if(condition)return;log_write("ASSERT",expression);serial_text(file);serial_char(':');log_hex(line);serial_text("\r\n");panic("kernel assertion failed",0xffff,0,(uint64_t)(uintptr_t)__builtin_return_address(0));}
