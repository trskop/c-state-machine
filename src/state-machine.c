/* Copyright (c) 2014, Peter Trško <peter.trsko@gmail.com>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *
 *     * Neither the name of Peter Trško nor the names of other
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "state-machine.h"
#include <assert.h>
#include <string.h>     /* memset() */

#if __STDC_VERSION__ >= 199901L
/* Standards C99 and C11 understand "inline" keyword. */
#define INLINE inline
#else
#define INLINE
#endif

/* Accessors for State_machine */
#define SM_MAX_STATE(sm)                (sm->max_state)
#define SM_MAX_EVENT(sm)                (sm->max_event)
#define SM_CURRENT_STATE(sm)            (sm->current_state)
#define SM_LOCK(sm)                     (sm->lock)
#define SM_DATA(sm)                     (sm->data)
#define SM_TRANSITION(sm)               (sm->transition)
#define SM_USING_TRANSITION_TABLE(sm)   (SM_TRANSITION(sm).using_table)
#define SM_TRANSITION_IMPL(sm, it)      (SM_TRANSITION(sm).implementation.it)

/* Accessors for State_machine_transition */
#define IS_TRANSITION(t)                (t->is_transition)
#define RESULT_TRANSITION(t)            (t->result.transition)
#define RESULT_NO_TRANSITION(t)         (t->result.no_transition)

/* In this macros we assume that caller will check if sm != NULL and if locking
 * is defined consistently.
 */
#define USE_LOCKING(sm)                 (SM_LOCK(sm).take != NULL)

/* Either our implementation supports locking or not, there is no middle
 * ground.
 */
#define ASSERT_LOCKING_DEFINITION_CONSISTENCY(lock)                           \
    assert((lock.take == NULL && lock.try_take == NULL && lock.give == NULL)  \
        || (lock.take != NULL && lock.try_take != NULL && lock.give != NULL))

#define ASSERT_NOT_NULL(x)              assert(x != NULL)

static INLINE void state_machine_init_common(State_machine *const sm,
    const uint32_t max_state,
    const uint32_t max_event,
    const uint32_t init_state,
    State_machine_locking lock,
    bool using_transition_table,
    void *const data)
{
    /* Pointer "data" may be NULL, since it is completely out of control of
     * this code.
     */
    ASSERT_NOT_NULL(sm);
    assert(max_state > 0);
    assert(max_event > 0);
    assert(init_state < max_state);
    ASSERT_LOCKING_DEFINITION_CONSISTENCY(SM_LOCK(sm));

    memset(sm, 0, sizeof(State_machine));
    SM_MAX_STATE(sm) = max_state;
    SM_MAX_EVENT(sm) = max_event;
    SM_CURRENT_STATE(sm) = init_state;
    SM_LOCK(sm) = lock;
    SM_USING_TRANSITION_TABLE(sm) = using_transition_table;
    SM_DATA(sm) = data;
}

void state_machine_init_table(State_machine *const sm,
    const uint32_t max_state,
    const uint32_t max_event,
    const uint32_t init_state,
    State_machine_locking locking,
    State_machine_transition *transition_table,
    void *const data)
{
    ASSERT_NOT_NULL(transition_table);

    state_machine_init_common(sm, max_state, max_event, init_state, locking,
        true, data);
    SM_TRANSITION_IMPL(sm, table) = transition_table;
}

void state_machine_init_function(State_machine *const sm,
    const uint32_t max_state,
    const uint32_t max_event,
    const uint32_t init_state,
    State_machine_locking locking,
    State_machine_transition_function transition,
    State_machine_transition_cleanup_function cleanup,
    void *const data)
{
    /* Note: Function cleanup may be NULL if no cleanup is necessary and that
     * is why there is no assert(cleanup != NULL)".
     */
    ASSERT_NOT_NULL(transition);

    state_machine_init_common(sm, max_state, max_event, init_state, locking,
        false, data);
    SM_TRANSITION_IMPL(sm, function).transition = transition;
    SM_TRANSITION_IMPL(sm, function).cleanup = cleanup;
}

/* Internal wrapper for take() and try_take() locking operations. Please bear
 * in mind that return value indicates if code is in ciritical section or not.
 */
static INLINE uint32_t lock_take(State_machine *const sm, const uint32_t flags)
{
    /* Function that calls this has to check that value of sm is not NULL.
     */
    ASSERT_LOCKING_DEFINITION_CONSISTENCY(SM_LOCK(sm));

    if (USE_LOCKING(sm))
    {
        if (flags & STATE_MACHINE_NONBLOCK)
        {
            if (!SM_LOCK(sm).try_take(sm))
            {
                return STATE_MACHINE_WOULD_BLOCK;
            }
        }
        else
        {
            SM_LOCK(sm).take(sm);
        }
    }

    return STATE_MACHINE_SUCCESS;
}

static INLINE void lock_give(State_machine *const sm)
{
    /* Function that calls this has to check that value of sm is not NULL.
     */
    ASSERT_LOCKING_DEFINITION_CONSISTENCY(SM_LOCK(sm));

    if (USE_LOCKING(sm))
    {
        SM_LOCK(sm).give(sm);
    }
}

uint32_t state_machine_current_state(State_machine *const sm,
    uint32_t *const state, const uint32_t flags)
{
    uint32_t ret;

    ASSERT_NOT_NULL(sm);
    ASSERT_NOT_NULL(state);

    if_sm_failure (ret = lock_take(sm, flags))
    {
        return ret;
    }
    *state = SM_CURRENT_STATE(sm);
    lock_give(sm);

    return ret;
}

uint32_t state_machine_event(State_machine *const sm, const uint32_t event,
    void *const event_data, const uint32_t flags)
{
    State_machine_transition *transition;

    /* Make sure that STATE_MACHINE_SUCCESS is default value of "ret". See
     * delayed return value handling of transition function later in the code.
     */
    uint32_t ret = STATE_MACHINE_SUCCESS;

    ASSERT_NOT_NULL(sm);

    /* {{{ Critical Section ************************************************ */

    /* We have to assume that multiple concurent threads of execution will have
     * access to the same event machine.
     */
    if_sm_failure (ret = lock_take(sm, flags))
    {

        /* It is safe to return from this function here since no destructive
         * operation was made. Don't ever do this inside critical section. It
         * is best to keep ratio of take() and give() calls as 1:1 to prevent
         * nasty bugs.
         */
        return ret;
    }

    /* We can make copies of values from state machine only inside critical
     * section. All bets are off if we aren't in it.
     */
    const uint32_t max_event = SM_MAX_EVENT(sm);
    const uint32_t max_state = SM_MAX_STATE(sm);
    uint32_t current_state = SM_CURRENT_STATE(sm);
    void *data = SM_DATA(sm);

    /* Setting previous_state to invalid value for easier error detection.
     */
    uint32_t previous_state = max_state;

    assert(event < max_event);
    assert(current_state < max_state);

    if (SM_USING_TRANSITION_TABLE(sm))
    {
        State_machine_transition (*table)[max_event] =
            (State_machine_transition (*)[max_event])SM_TRANSITION_IMPL(sm, table);

        transition = &(table[current_state][event]);
    }
    else
    {
        State_machine_transition_function transition_function =
            SM_TRANSITION_IMPL(sm, function).transition;

        /* If implementation of transition_function() uses its own private data
         * (meaning sm->data) or calls outside functions, then one has to be
         * aware of the fact that it is being done inside of critical section.
         *
         * It would be best if transition_function() was defined as a pure
         * total function. In other words function that would behave as array
         * lookup to the outside and for the same input would always provide
         * same output. Unfortunately that is out of our control.
         */
        ret = transition_function(current_state, event, data, &transition);
        /* We now have to delay failure handling so that we can have only one
         * lock_give() call.
         */
    }

    /* Note that since transition_function() error handling of is delayed we
     * have to check "is_sm_success(ret)" for this to work correctly only
     * return value of transition_function() may be stored in ret or ret's
     * default value which is STATE_MACHINE_SUCCESS. Make sure that this
     * invariant hodls.
     */
    if (is_sm_success(ret) && IS_TRANSITION(transition))
    {
        previous_state = current_state;
        current_state = RESULT_TRANSITION(transition).next_state;
        SM_CURRENT_STATE(sm) = current_state;
    }

    assert(current_state < max_state);
    assert(SM_CURRENT_STATE(sm) == current_state);

    lock_give(sm);

    /* }}} Critical Section ************************************************ */

    /* Now we handle transition_function() return value properly and we do so
     * outside of critical section.
     */
    if_sm_failure (ret)
    {
        /* No need for cleanup since there neither were any modifications
         * made nor there were any allocations. The later is implied by the
         * fact that function that could allocate something returned error
         * which is currently being propagated.
         */
        return ret;
    }

    /* On-enter or on-undefined-state callback function is invoked outside of
     * critical section. For this to work consistently this invariants have to
     * hold:
     *
     * - Value of transition variable is either constant or it is visible only
     *   in current context and not by other thread of execution (regardles if
     *   we are single-threaded event-driven system or multi-threaded system).
     *
     *   Most commonly it will be constant when state machine was initialized
     *   using transition table and possibly variable value when using
     *   transition function. Therefore transition function should return
     *   either constant or newly allocated value visible only in current
     *   context. Later requires that cleanup function works properly.
     *
     * - Private data stored in state machine "sm->data" pointer are either
     *   thread safe or properly handled inside on-enter or on-undefined-state
     *   callbacks.
     */
    if (IS_TRANSITION(transition))
    {
        On_state_enter on_enter = RESULT_TRANSITION(transition).on_enter;

        if (on_enter != NULL)
        {
            on_enter(event, current_state, previous_state, event_data, data);
        }
    }
    else
    {
        On_undefined_state_transition on_undefined_transition =
            RESULT_NO_TRANSITION(transition).on_undefined_transition;

        if (on_undefined_transition != NULL)
        {
            on_undefined_transition(event, current_state, event_data, data);
        }
    }

    /* When using transition function we need to call cleanup function, if
     * provided. The reason behind this is that transition function may
     * allocate memory or other resource and cleanup is then responsible to
     * deallocate it.
     */
    if (!SM_USING_TRANSITION_TABLE(sm))
    {
        State_machine_transition_cleanup_function cleanup =
            SM_TRANSITION_IMPL(sm, function).cleanup;

        if (cleanup != NULL)
        {
            ret = cleanup(data, transition);
            /* Main function return call handles this return value.
             */
        }
    }

    return ret;
}
