/*
 * Copyright (c) 2020 Sekhar Bhattacharya
 *
 * SPDX-License-Identifier: MIT
 */

#include <kernel/proc/proc_thread.h>
#include <kernel/proc/proc_scheduler.h>
#include <kernel/kassert.h>
#include <kernel/lock.h>

void lock_acquire_exclusive(lock_t *lock) {
    kassert(lock != NULL);

    spinlock_acquire_irq(&lock->interlock);

    // Exclusive locks can only be owned if no one else has the lock
    while (lock->state != LOCK_STATE_FREE) {
        // If the lock is owned in shared state then it needs to be upgraded
        // Track the thread with the lowest virtual runtime requesting exclusive upgrade
        if (lock->state == LOCK_STATE_SHARED) {
            lock->thread = proc_thread_current();
            lock->state = LOCK_STATE_EXCLUSIVE_UPGRADE;
        } else if (lock->state == LOCK_STATE_EXCLUSIVE_UPGRADE
            && proc_scheduler_deserve(proc_thread_current(), lock->thread)) {
            lock->thread = proc_thread_current();
        }

        proc_thread_sleep(lock, &lock->interlock, false);
        spinlock_acquire_irq(&lock->interlock);
    }

    lock->thread = proc_thread_current();
    lock->state = LOCK_STATE_EXCLUSIVE;

    spinlock_release_irq(&lock->interlock);
}

void lock_acquire_shared(lock_t *lock) {
    kassert(lock != NULL);

    spinlock_acquire_irq(&lock->interlock);

    // Shared locks can only be owned if the lock is free or already being shared. Special case for locks waiting to be
    // upgraded to exclusive; only threads with a lower virtual runtime then the thread requesting exclusive access may
    // acquire shared ownership of the lock
    while (lock->state == LOCK_STATE_EXCLUSIVE
        || (lock->state == LOCK_STATE_EXCLUSIVE_UPGRADE
        && proc_scheduler_deserve(lock->thread, proc_thread_current()))) {
        proc_thread_sleep(lock, &lock->interlock, false);
        spinlock_acquire_irq(&lock->interlock);
    }

    lock->shared_count++;
    if (lock->state != LOCK_STATE_EXCLUSIVE_UPGRADE) lock->state = LOCK_STATE_SHARED;

    spinlock_release_irq(&lock->interlock);
}

bool lock_try_acquire_exclusive(lock_t *lock) {
    kassert(lock != NULL);

    spinlock_acquire_irq(&lock->interlock);

    if (lock->state != LOCK_STATE_FREE) return false;

    lock->thread = proc_thread_current();
    lock->state = LOCK_STATE_EXCLUSIVE;

    spinlock_release_irq(&lock->interlock);

    return true;
}

bool lock_try_acquire_shared(lock_t *lock) {
    kassert(lock != NULL);

    spinlock_acquire_irq(&lock->interlock);

    if (lock->state == LOCK_STATE_EXCLUSIVE
        || (lock->state == LOCK_STATE_EXCLUSIVE_UPGRADE
        && proc_scheduler_deserve(lock->thread, proc_thread_current()))) {
        return false;
    }

    lock->shared_count++;
    if (lock->state != LOCK_STATE_EXCLUSIVE_UPGRADE) lock->state = LOCK_STATE_SHARED;

    spinlock_release_irq(&lock->interlock);

    return true;
}

void lock_release_exclusive(lock_t *lock) {
    kassert(lock != NULL);

    spinlock_acquire_irq(&lock->interlock);

    kassert(lock->state == LOCK_STATE_EXCLUSIVE && lock->thread == proc_thread_current());

    lock->thread = NULL;
    lock->state = LOCK_STATE_FREE;

    spinlock_release_irq(&lock->interlock);

    // Wakeup threads waiting for this lock
    proc_thread_wake(lock, -1);
}

void lock_release_shared(lock_t *lock) {
    kassert(lock != NULL);

    proc_thread_t *thread = NULL;
    bool do_wake = false;

    spinlock_acquire_irq(&lock->interlock);

    kassert(lock->shared_count > 0);

    // If the lock needs to be upgraded to exclusive from shared then capture the one thread to wake
    if (lock->state == LOCK_STATE_EXCLUSIVE_UPGRADE) thread = lock->thread;

    // Only set the lock state to free once all shared owners have released the lock
    lock->shared_count--;
    if (lock->shared_count == 0) {
        lock->state = LOCK_STATE_FREE;
        do_wake = true;
    }

    spinlock_release_irq(&lock->interlock);

    // Wakeup only the single thread waiting for the exclusive ownership of the (previously) shared lock
    // Otherwise wakeup all the threads waiting for this lock
    if (do_wake) {
        if (thread != NULL) {
            proc_thread_wake(thread, 1);
        } else {
            proc_thread_wake(lock, -1);
        }
    }
}
