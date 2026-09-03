#ifndef AXIOM_LOG_H
#define AXIOM_LOG_H
#include <stdint.h>
#include <stdbool.h>
void log_init(void);
void log_write(const char *level, const char *message);
void log_hex(uint64_t value);
void log_dump(void);
__attribute__((noreturn)) void panic(const char *message, uint64_t vector, uint64_t error, uint64_t rip);
__attribute__((noreturn)) void panic_frame(const char *message, const uint64_t *frame);
void kernel_assert(bool condition,const char *expression,const char *file,uint64_t line);
#define KASSERT(condition) kernel_assert((condition),#condition,__FILE__,__LINE__)
#define LOG_INFO(message) log_write("INFO", message)
#define LOG_WARN(message) log_write("WARN", message)
#define LOG_ERROR(message) log_write("ERROR", message)
#endif
