/* Small C bridge for the libretro variadic logger in ctypes tests. */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#define API __declspec(dllexport)
#else
#define API __attribute__((visibility("default")))
#endif
static char log_data[131072];
API void capture_log(int level, const char *fmt, ...)
{
   size_t n = strlen(log_data);
   va_list ap; (void)level;
   va_start(ap,fmt);
   vsnprintf(log_data+n,sizeof(log_data)-n,fmt,ap);
   va_end(ap);
}
API const char *captured_log(void) { return log_data; }
API void clear_log(void) { log_data[0] = 0; }
