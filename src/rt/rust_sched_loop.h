#ifndef RUST_SCHED_LOOP_H
#define RUST_SCHED_LOOP_H

#include "rust_internal.h"
#include "rust_stack.h"
#include "rust_signal.h"
#include "context.h"

enum rust_task_state {
    task_state_newborn,
    task_state_running,
    task_state_blocked,
    task_state_dead
};

/*
The result of every turn of the scheduler loop. Instructs the loop
driver how to proceed.
 */
enum rust_sched_loop_state {
    sched_loop_state_keep_going,
    sched_loop_state_block,
    sched_loop_state_exit
};

struct rust_task;

typedef indexed_list<rust_task> rust_task_list;

struct rust_sched_loop
{
private:

    lock_and_signal lock;

    // Fields known only by the runtime:
    rust_log _log;

    const int id;

    context c_context;

    bool should_exit;

    stk_seg *cached_c_stack;
    stk_seg *extra_c_stack;

    rust_task_list running_tasks;
    rust_task_list blocked_tasks;
    rust_task *dead_task;

    rust_signal *pump_signal;

    void prepare_c_stack(rust_task *task);
    void unprepare_c_stack();

    rust_task_list *state_list(rust_task_state state);
    const char *state_name(rust_task_state state);

    void pump_loop();

public:
    rust_kernel *kernel;
    rust_scheduler *sched;
    static bool tls_initialized;

#ifndef __WIN32__
    static pthread_key_t task_key;
#else
    static DWORD task_key;
#endif

    // NB: this is used to filter *runtime-originating* debug
    // logging, on a per-scheduler basis. It's not likely what
    // you want to expose to the user in terms of per-task
    // or per-module logging control. By default all schedulers
    // are set to debug-level logging here, and filtered by
    // runtime category using the pseudo-modules ::rt::foo.
    uint32_t log_lvl;

    size_t min_stack_size;
    memory_region local_region;

    randctx rctx;

    // FIXME: Neither of these are used
    int32_t list_index;
    const char *const name;

    // Only a pointer to 'name' is kept, so it must live as long as this
    // domain.
    rust_sched_loop(rust_scheduler *sched, int id);
    void activate(rust_task *task);
    void log(rust_task *task, uint32_t level, char const *fmt, ...);
    rust_log & get_log();
    void fail();

    size_t number_of_live_tasks();

    void reap_dead_tasks();
    rust_task *schedule_task();

    void on_pump_loop(rust_signal *signal);
    rust_sched_loop_state run_single_turn();

    void log_state();

    void kill_all_tasks();

    rust_task *create_task(rust_task *spawner, const char *name);

    void transition(rust_task *task,
                    rust_task_state src, rust_task_state dst,
                    rust_cond *cond, const char* cond_name);

    void init_tls();
    void place_task_in_tls(rust_task *task);

    // Called by each task when they are ready to be destroyed
    void release_task(rust_task *task);

    // Tells the scheduler to exit it's scheduling loop and thread
    void exit();

    // Called by tasks when they need a stack on which to run C code
    stk_seg *borrow_c_stack();
    void return_c_stack(stk_seg *stack);
};

inline rust_log &
rust_sched_loop::get_log() {
    return _log;
}

// NB: Runs on the Rust stack
inline stk_seg *
rust_sched_loop::borrow_c_stack() {
    assert(cached_c_stack);
    stk_seg *your_stack;
    if (extra_c_stack) {
        your_stack = extra_c_stack;
        extra_c_stack = NULL;
    } else {
        your_stack = cached_c_stack;
        cached_c_stack = NULL;
    }
    return your_stack;
}

// NB: Runs on the Rust stack
inline void
rust_sched_loop::return_c_stack(stk_seg *stack) {
    assert(!extra_c_stack);
    if (!cached_c_stack) {
        cached_c_stack = stack;
    } else {
        extra_c_stack = stack;
    }
}


//
// Local Variables:
// mode: C++
// fill-column: 78;
// indent-tabs-mode: nil
// c-basic-offset: 4
// buffer-file-coding-system: utf-8-unix
// compile-command: "make -k -C $RBUILD 2>&1 | sed -e 's/\\/x\\//x:\\//g'";
// End:
//

#endif /* RUST_SCHED_LOOP_H */
