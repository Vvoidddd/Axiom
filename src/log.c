#include "log.h"
#include "console.h"
#include "io.h"

#define LOG_LINES 32
#define LOG_LENGTH 80
static char history[LOG_LINES][LOG_LENGTH];
static unsigned next_line, line_count;

static void serial_char(char c) {
    while ((inb(0x3fd) & 0x20u) == 0) {}
    outb(0x3f8, (uint8_t)c);
}
static void serial_text(const char *s) { while (*s) serial_char(*s++); }
static void copy_line(char *out, const char *level, const char *message) {
    unsigned n=0; out[n++]='[';
    while (*level && n+1<LOG_LENGTH) out[n++]=*level++;
    if (n+2<LOG_LENGTH) { out[n++]=']'; out[n++]=' '; }
    while (*message && n+1<LOG_LENGTH) out[n++]=*message++;
    out[n]=0;
}
void log_init(void) {
    outb(0x3f9,0); outb(0x3fb,0x80); outb(0x3f8,3); outb(0x3f9,0);
    outb(0x3fb,3); outb(0x3fa,0xc7); outb(0x3fc,0x0b);
    LOG_INFO("serial logger online");
}
void log_write(const char *level, const char *message) {
    copy_line(history[next_line],level,message);
    serial_text(history[next_line]); serial_text("\r\n");
    next_line=(next_line+1)%LOG_LINES; if(line_count<LOG_LINES) line_count++;
}
void log_hex(uint64_t value) {
    static const char h[]="0123456789ABCDEF"; serial_text("0x");
    for(int shift=60;shift>=0;shift-=4) serial_char(h[(value>>shift)&15]);
}
void log_dump(void) {
    unsigned start=(next_line+LOG_LINES-line_count)%LOG_LINES;
    for(unsigned i=0;i<line_count;i++){ console_write(history[(start+i)%LOG_LINES]); console_putc('\n'); }
}
__attribute__((noreturn)) void panic(const char *message, uint64_t vector, uint64_t error, uint64_t rip) {
    __asm__ volatile("cli"); LOG_ERROR(message);
    serial_text("vector="); log_hex(vector); serial_text(" error="); log_hex(error);
    serial_text(" rip="); log_hex(rip); serial_text("\r\n");
    console_write("\nKERNEL PANIC: "); console_write(message); console_write("\nVECTOR: ");
    console_write_u64(vector); console_write(" ERROR: "); console_write_hex(error);
    console_write("\nRIP: "); console_write_hex(rip); console_putc('\n');
    for(;;) __asm__ volatile("hlt");
}
__attribute__((noreturn)) void panic_frame(const char *message,const uint64_t *f){
    __asm__ volatile("cli");LOG_ERROR(message);static const char *names[]={"R15","R14","R13","R12","R11","R10","R9","R8","RSI","RDI","RBP","RDX","RCX","RBX","RAX"};
    for(unsigned i=0;i<15;i++){serial_text(names[i]);serial_char('=');log_hex(f[i]);serial_text((i%3)==2?"\r\n":" ");}
    serial_text("VECTOR=");log_hex(f[15]);serial_text(" ERROR=");log_hex(f[16]);serial_text(" RIP=");log_hex(f[17]);serial_text("\r\n");
    console_write("\nKERNEL PANIC: ");console_write(message);console_write("\nVECTOR: ");console_write_u64(f[15]);console_write(" ERROR: ");console_write_hex(f[16]);console_write("\nRIP: ");console_write_hex(f[17]);console_write("\nRAX: ");console_write_hex(f[14]);console_write(" RBX: ");console_write_hex(f[13]);console_write("\nRCX: ");console_write_hex(f[12]);console_write(" RDX: ");console_write_hex(f[11]);console_putc('\n');for(;;)__asm__ volatile("hlt");
}
void kernel_assert(bool condition,const char *expression,const char *file,uint64_t line){if(condition)return;log_write("ASSERT",expression);serial_text(file);serial_char(':');log_hex(line);serial_text("\r\n");panic("kernel assertion failed",0xffff,0,(uint64_t)(uintptr_t)__builtin_return_address(0));}
