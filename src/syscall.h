#ifndef SYS_CALL_H
#define SYS_CALL_H
#include"fakedef.h"
void SYS_car(FakeVM* exe,pthread_rwlock_t* gclock);
void SYS_cdr(FakeVM* exe,pthread_rwlock_t* gclock);
void SYS_cons(FakeVM* exe,pthread_rwlock_t* gclock);
void SYS_append(FakeVM* exe,pthread_rwlock_t* gclock);
void SYS_atom(FakeVM* exe,pthread_rwlock_t* gclock);
void SYS_null(FakeVM* exe,pthread_rwlock_t* gclock);
void SYS_not(FakeVM* exe,pthread_rwlock_t* gclock);
void SYS_eq(FakeVM* exe,pthread_rwlock_t* gclock);
void SYS_equal(FakeVM* exe,pthread_rwlock_t* gclock);
void SYS_eqn(FakeVM* exe,pthread_rwlock_t* gclock);
void SYS_add(FakeVM* exe,pthread_rwlock_t* gclock);
void SYS_sub(FakeVM* exe,pthread_rwlock_t* gclock);
void SYS_mul(FakeVM* exe,pthread_rwlock_t* gclock);
void SYS_div(FakeVM* exe,pthread_rwlock_t* gclock);
void SYS_rem(FakeVM* exe,pthread_rwlock_t* gclock);
void SYS_gt(FakeVM* exe,pthread_rwlock_t* gclock);
void SYS_ge(FakeVM* exe,pthread_rwlock_t* gclock);
void SYS_lt(FakeVM* exe,pthread_rwlock_t* gclock);
void SYS_le(FakeVM* exe,pthread_rwlock_t* gclock);
void SYS_chr(FakeVM* exe,pthread_rwlock_t* gclock);
void SYS_int(FakeVM* exe,pthread_rwlock_t* gclock);
void SYS_dbl(FakeVM* exe,pthread_rwlock_t* gclock);
void SYS_str(FakeVM* exe,pthread_rwlock_t* gclock);
void SYS_sym(FakeVM* exe,pthread_rwlock_t* gclock);
void SYS_byts(FakeVM* exe,pthread_rwlock_t* gclock);
void SYS_type(FakeVM* exe,pthread_rwlock_t* gclock);
void SYS_nth(FakeVM* exe,pthread_rwlock_t* gclock);
void SYS_length(FakeVM* exe,pthread_rwlock_t* gclock);
void SYS_file(FakeVM* exe,pthread_rwlock_t* gclock);
void SYS_read(FakeVM* exe,pthread_rwlock_t* gclock);
void SYS_getb(FakeVM* exe,pthread_rwlock_t* gclock);
void SYS_write(FakeVM* exe,pthread_rwlock_t* gclock);
void SYS_putb(FakeVM* exe,pthread_rwlock_t* gclock);
void SYS_princ(FakeVM* exe,pthread_rwlock_t* gclock);
void SYS_dll(FakeVM*,pthread_rwlock_t*);
void SYS_dlsym(FakeVM*,pthread_rwlock_t*);
void SYS_argv(FakeVM*,pthread_rwlock_t*);
void SYS_go(FakeVM*,pthread_rwlock_t*);
void SYS_chanl(FakeVM*,pthread_rwlock_t*);
void SYS_send(FakeVM*,pthread_rwlock_t*);
void SYS_recv(FakeVM*,pthread_rwlock_t*);
void SYS_error(FakeVM*,pthread_rwlock_t*);
void SYS_raise(FakeVM*,pthread_rwlock_t*);
#endif
