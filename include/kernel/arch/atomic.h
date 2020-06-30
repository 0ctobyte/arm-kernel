#ifndef __ATOMIC_H__
#define __ATOMIC_H__

typedef long atomic_t;

// Atomically tests if value at ptr is 0 and if so sets it to val
// Returns 0 on success, otherwise failure to set
int atomic_test_and_set(atomic_t *ptr, long val);

// Atomically tests specified bit in the value at ptr is 0 and if so sets it to 1
// Returns 0 on success, otherwise failure to set
int atomic_test_and_set_bit(atomic_t *ptr, unsigned long bit);

// Atomically increment the value
atomic_t atomic_inc(atomic_t *v);

// Atomically decrement the value
atomic_t atomic_dec(atomic_t *v);

#endif // __ATOMIC_H__