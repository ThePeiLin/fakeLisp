#ifndef SYS_CALL_H
#define SYS_CALL_H
#include"fakedef.h"
void SYS_file(FakeVM* exe,pthread_rwlock_t* gclock);
void SYS_read(FakeVM* exe,pthread_rwlock_t* gclock);
void SYS_getb(FakeVM* exe,pthread_rwlock_t* gclock);
void SYS_write(FakeVM* exe,pthread_rwlock_t* gclock);
void SYS_putb(FakeVM* exe,pthread_rwlock_t* gclock);
void SYS_princ(FakeVM* exe,pthread_rwlock_t* gclock);
#endif
