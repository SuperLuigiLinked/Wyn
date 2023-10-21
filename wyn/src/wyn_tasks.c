/**
 * @file wyn_tasks.c
 * @brief Implementation of an Intrusive MPSC Queue.
 * @see
 *   - https://www.1024cores.net/home/lock-free-algorithms/queues/intrusive-mpsc-node-based-queue
 *   - https://www.1024cores.net/home/code-license
 */

#include <wyn.h>
#include <stdatomic.h>

// ================================================================================================================================

extern void wyn_init_tasks(void);
extern void wyn_clear_tasks(bool cancel);

// ================================================================================================================================

struct wyn_mpscq_t
{
    _Atomic(wyn_task_t*) last;
    wyn_task_t* next;
    wyn_task_t stub;
};
typedef struct wyn_mpscq_t wyn_mpscq_t;

static wyn_mpscq_t wyn_tasks;

// ================================================================================================================================

extern void wyn_init_tasks(void)
{
    wyn_tasks.stub = (wyn_task_t){};
    wyn_tasks.next = &wyn_tasks.stub;
    atomic_store_explicit(&wyn_tasks.last, &wyn_tasks.stub, memory_order_relaxed);
}

extern void wyn_clear_tasks(const bool cancel)
{
    const wyn_status_t status = (cancel ? wyn_status_canceled : wyn_status_complete);

    wyn_task_t* next = wyn_task_dequeue();
    while (next)
    {
        wyn_task_t* const task = next;
        if (!cancel) task->func(task->args);
        next = wyn_task_dequeue();
        atomic_store_explicit(&task->status, status, memory_order_relaxed);
    }
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_task_enqueue(wyn_task_t* const task)
{
    atomic_store_explicit(&task->next, NULL, memory_order_relaxed);
    atomic_store_explicit(&task->status, wyn_status_pending, memory_order_relaxed);
    
    wyn_task_t* const prev = atomic_exchange_explicit(&wyn_tasks.last, task, memory_order_relaxed);
    atomic_store_explicit(&prev->next, NULL, memory_order_relaxed);
}

extern wyn_task_t* wyn_task_dequeue(void)
{
    wyn_task_t* next;

    if (wyn_tasks.next == &wyn_tasks.stub)
    {
	    next = atomic_load_explicit(&wyn_tasks.next->next, memory_order_relaxed);
        if (next == NULL) return NULL;

        wyn_tasks.next = next;
    }

    next = atomic_load_explicit(&wyn_tasks.next->next, memory_order_relaxed);
    if (next == NULL)
    {
	    if (wyn_tasks.next != atomic_load_explicit(&wyn_tasks.last, memory_order_relaxed)) return NULL;
	    wyn_task_enqueue(&wyn_tasks.stub);

        next = atomic_load_explicit(&wyn_tasks.next->next, memory_order_relaxed);
	    if (next == NULL) return NULL;
    }

    wyn_task_t* const task = wyn_tasks.next;	
    wyn_tasks.next = next;
    return task;
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyn_status_t wyn_task_poll(const wyn_task_t* const task)
{
    return atomic_load_explicit(&task->status, memory_order_relaxed);
}

extern wyn_status_t wyn_task_await(const wyn_task_t* const task)
{
    wyn_status_t status;
    while ((status = atomic_load_explicit(&task->status, memory_order_relaxed)) == wyn_status_pending);
    return status;
}

// ================================================================================================================================
