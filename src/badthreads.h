#ifndef _BADTHREADS_H_
#define _BADTHREADS_H_

/*
 * This file contains functions and symbols that are defined in C11 threads.h.
 * If any of these symbols are used in a file that includes badthreads.h, it
 * should throw an error at compile time.
 *
 * The purpose of this file is to make sure that code does not accidentally
 * use symbols from threads.h. If this happens, and the system C library has
 * C11-style thread support, then the resulting object could link to the
 * system's functions that have the same name, instead of the local functions.
*/

#define thrd_t              THREADS_H_ERROR
#define thrd_create         THREADS_H_ERROR
#define thrd_equal          THREADS_H_ERROR
#define thrd_current        THREADS_H_ERROR
#define thrd_sleep          THREADS_H_ERROR
#define thrd_yield          THREADS_H_ERROR
#define thrd_exit           THREADS_H_ERROR
#define thrd_detach         THREADS_H_ERROR
#define thrd_join           THREADS_H_ERROR
#define thrd_success        THREADS_H_ERROR
#define thrd_timedout       THREADS_H_ERROR
#define thrd_busy           THREADS_H_ERROR
#define thrd_nomem          THREADS_H_ERROR
#define thrd_error          THREADS_H_ERROR
#define thrd_start_t        THREADS_H_ERROR
#define mtx_t               THREADS_H_ERROR
#define mtx_init            THREADS_H_ERROR
#define mtx_lock            THREADS_H_ERROR
#define mtx_timedlock       THREADS_H_ERROR
#define mtx_trylock         THREADS_H_ERROR
#define mtx_unlock          THREADS_H_ERROR
#define mtx_destroy         THREADS_H_ERROR
#define mtx_plain           THREADS_H_ERROR
#define mtx_recursive       THREADS_H_ERROR
#define mtx_timed           THREADS_H_ERROR
#define call_once           THREADS_H_ERROR
#define cnd_t               THREADS_H_ERROR
#define cnd_init            THREADS_H_ERROR
#define cnd_signal          THREADS_H_ERROR
#define cnd_broadcast       THREADS_H_ERROR
#define cnd_wait            THREADS_H_ERROR
#define cnd_timedwait       THREADS_H_ERROR
#define cnd_destroy         THREADS_H_ERROR
#define thread_local        THREADS_H_ERROR
#define tss_t               THREADS_H_ERROR
#define TSS_DTOR_ITERATIONS THREADS_H_ERROR
#define tss_dtor_t          THREADS_H_ERROR
#define tss_create          THREADS_H_ERROR
#define tss_get             THREADS_H_ERROR
#define tss_set             THREADS_H_ERROR
#define tss_delete          THREADS_H_ERROR


#endif
