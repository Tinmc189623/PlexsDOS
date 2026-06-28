/* sys/resource.h — Windows MSVC stub */
#ifndef _SYS_RESOURCE_H_
#define _SYS_RESOURCE_H_
/* Minimal stub — rusage not required for xorriso */
struct rusage { int dummy; };
int getrusage(int who, struct rusage *usage);
#endif
