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

#ifndef STATE_MACHINE_H_102424114459923656945136905231318306313
#define STATE_MACHINE_H_102424114459923656945136905231318306313

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*On_state_enter)(uint32_t cause, uint32_t current_state,
    uint32_t previous_state, void *data);

typedef void (*On_undefined_state_transition)(uint32_t cause,
    uint32_t current_state, void *data);

typedef struct State_machine_transition_s
{
    /* Event that caused state transition.
     */
    uint32_t cause;

    /** State we are currently in and probably transitioning from.
     */
    uint32_t current_state;

    /** Flag that indicates if there is transition from <tt>current_state</tt>
     * under event <tt>cause</tt>.
     */
    bool is_transition;

    /** Result is either <tt>transition</tt> or <tt>no_transition</tt>,
     * depending on <tt>is_transition</tt> flag.
     */
    union
    {
        struct
        {
            /** State in tho which state machine will transition.
             */
            uint32_t next_state;

            /** Callback function called after state transition.
             */
            On_state_enter on_enter;
        } transition;

        struct
        {
            /** Callback function called on undefined transition from
             * <tt>current_state</tt> under event <tt>cause</tt>.
             */
            On_undefined_state_transition on_undefined_transition;
        } no_transition;
    } result;
} State_machine_transition;

/** Macro for static initialization of <tt>State_machine_transition</tt> in
 * case of valid (i.e. defined) state transition.
 */
#define STATE_MACHINE_TRANSITION(s, e, ns, h)   \
    {                                           \
        .cause = e,                             \
        .current_state = s,                     \
        .is_transition = true,                  \
        .result =                               \
        {                                       \
            .transition =                       \
            {                                   \
                .next_state = ns,               \
                .on_enter = h                   \
            }                                   \
        }                                       \
    }

/** Macro for static initialization of <tt>State_machine_transition</tt> in
 * case of invalid (i.e. undefined) state transition.
 */
#define STATE_MACHINE_NO_TRANSITION(s, e, h)    \
    {                                           \
        .cause = e,                             \
        .current_state = s,                     \
        .is_transition = false,                 \
        .result =                               \
        {                                       \
            .no_transition =                    \
            {                                   \
                .on_undefined_transition = h    \
            }                                   \
        }                                       \
    }

struct State_machine_s;     /* Forward declaration. */

/** Interface for locking primitives.
 *
 * Usage of locking is not mandatory, but event-driven and multi-threaded
 * application should use it.
 */
typedef struct
{
    bool (*try_take)(struct State_machine_s *);
    void (*take)(struct State_machine_s *);
    void (*give)(struct State_machine_s *);
} State_machine_locking;

/** Use this macro to statically initialize
 * <tt>State_machine_transition_function</tt> when locking is not
 * necessary.
 */
#define STATE_MACHINE_NO_LOCKING    \
    {.take = NULL, .try_take = NULL, .give = NULL}

typedef uint32_t (*State_machine_transition_function)(uint32_t current_state,
    uint32_t event, void *data, State_machine_transition **);

typedef uint32_t (*State_machine_transition_cleanup_function)(void *data,
    State_machine_transition *);

typedef struct State_machine_s
{
    /** Upper bound on number of states.
     *
     * State may be value between 0 (including) and <tt>max_state</tt>
     * (excluding).
     */
    uint32_t max_state;

    /** Upper bound on number of events.
     *
     * Event may be value between 0 (including) and <tt>max_state</tt>
     * (excluding).
     */
    uint32_t max_event;

    /** Current state in which state machine is in.
     */
    uint32_t current_state;

    /** Locking primitives.
     */
    State_machine_locking lock;

    /** State machine can either uses transition table or transition function.
     *
     * Transition tables are fast, but may require a lot of memory if either
     * <tt>max_state</tt> or <tt>max_event</tt> or both are big.
     */
    struct
    {
        bool using_table;

        /** Either implementation uses transition table or function, depending
         * on <tt>using_table</tt> flag.
         */
        union 
        {
            /** Table of state transitions parametrised by event.
             *
             * Transition table is two dimensional table of minimal size
             * <tt>max_state * max_event</tt>, i.e.
             * <tt>State_machine_transition table[max_state][max_event]</tt>.
             */
            State_machine_transition *table;

            struct
            {
                /** Transition function that takes state machine as it is and
                 * returns state machine transition that has to be applied yet applied.
                 *
                 * This field may not be NULL.
                 */
                State_machine_transition_function transition;

                /** If <tt>transition</tt> had to e.g. allocate data, or any other
                 * resource, then this function is called to release it.
                 *
                 * This field may be NULL when there is no cleanup necessary.
                 */
                State_machine_transition_cleanup_function cleanup;
            } function;
        } implementation;
    } transition;

    /** Private implementation data.
     */
    void *data;
} State_machine;

/** Initialize state machine using transition table.
 *
 * @param[in] max_state
 *   Upper bound on number of states. It has to be greater then zero.
 *
 * @param[in] max_event
 *   Upper bound on number of events. It has to be greater then zero.
 *
 * @param[in] init_state
 *   Initial state of state machine. It may be between zero (including) and
 *   max_state (excluding).
 *
 * @param[in] transition_table
 *   Two dimensional array of at least <tt>max_state * max_event</tt> size.
 *   Transition table has to be already filled before calling function
 *   <tt>state_machine_init_table()</tt>.
 */
void state_machine_init_table(State_machine *const state_machine,
    const uint32_t max_state,
    const uint32_t max_event,
    const uint32_t init_state,
    State_machine_locking locking,
    State_machine_transition *transition_table,
    void *const data);

/** Initialize state machine using transition function.
 *
 * @param[in] max_state
 *   Upper bound on number of states. It has to be greater then zero.
 *
 * @param[in] max_event
 *   Upper bound on number of events. It has to be greater then zero.
 *
 * @param[in] init_state
 *   Initial state of state machine. It may be between zero (including) and
 *   max_state (excluding).
 *
 * @param[in] transition
 *   Transition function that takes state machine as it is and returns state
 *   machine transition that has to be applied yet applied.
 *
 * @param[in] cleanup
 *   If <tt>transition</tt> had to e.g. allocate data, or any other resource,
 *   then this function is called to release it.
 */
void state_machine_init_function(State_machine *const state_machine,
    const uint32_t max_state,
    const uint32_t max_event,
    const uint32_t init_state,
    State_machine_locking locking,
    State_machine_transition_function transition,
    State_machine_transition_cleanup_function cleanup,
    void *const data);

/** Flag indicates that state machine should operate in nonblocking manner.
 *
 * Using same value as O_NONBLOCK on Linux, but there is no deep reason behind
 * it.
 */
#define STATE_MACHINE_NONBLOCK  2048

/** Get current state of a state machine.
 *
 * @param[in] state_machine
 *   State machine of which current state this function retrieves.
 *
 * @param[out] state
 *   Address to which state of current machine is stored on success.
 *
 * @param[in] flags
 *   Bit field that additionaly parametrises how this function works. Currently
 *   only <tt>STATE_MACHINE_NONBLOCK</tt> is supported. When flag 
 *   <tt>STATE_MACHINE_NONBLOCK</tt> is set, then it uses <tt>try_take()</tt>
 *   instead <tt>take()</tt> operation on state machine lock. Normally if blocking
 *   operation is considered OK, then this argument would be set to 0.
 *
 * @return
 *   On success function returns <tt>STATE_MACHINE_SUCCESS</tt>. If flags have
 *   <tt>STATE_MACHINE_NONBLOCK</tt> bit set and function was unable to acquire
 *   lock, then it returns <tt>STATE_MACHINE_WOULD_BLOCK</tt>.
 */
uint32_t state_machine_current_state(State_machine *const state_machine,
    uint32_t *const state, const uint32_t flags);

/** Send <tt>event</tt> to state machine for it to handle.
 *
 * @param[in] state_machine
 *   State machine to which <tt>event</tt> is sent to.
 *
 * @param[in] event
 *   Send this event to event_machine, try to perform state transition, and
 *   execute on-enter callback.
 *
 * @param[in] flags
 *   Bit field that additionaly parametrises how this function works. Currently
 *   only <tt>STATE_MACHINE_NONBLOCK</tt> is supported. When flag 
 *   <tt>STATE_MACHINE_NONBLOCK</tt> is set, then it uses <tt>try_take()</tt>
 *   instead <tt>take()</tt> operation on state machine lock. Normally if
 *   blocking operation is considered OK, then this argument would be set to 0.
 *
 * @return
 *   On success function returns <tt>STATE_MACHINE_SUCCESS</tt>. If flags have
 *   <tt>STATE_MACHINE_NONBLOCK</tt> bit set and function was unable to acquire
 *   lock, then it returns <tt>STATE_MACHINE_WOULD_BLOCK</tt>. If on-enter
 *   callback function is specified and it fails (returns value not equal to
 *   <tt>STATE_MACHINE_SUCCESS</tt>), then that value is returned by this
 *   function as well.
 */
uint32_t state_machine_event(State_machine *const, const uint32_t event,
    const uint32_t flags);

#define STATE_MACHINE_SUCCESS       0
#define STATE_MACHINE_WOULD_BLOCK   1

#define is_sm_success(r)        ((r) == STATE_MACHINE_SUCCESS)
#define is_sm_failure(r)        ((r) != STATE_MACHINE_SUCCESS)
#define if_sm_success(r)        if (is_sm_success(r))
#define if_sm_failure(r)        if (is_sm_failure(r))

#ifdef __cplusplus
}
#endif

#endif /* STATE_MACHINE_H_102424114459923656945136905231318306313 */
