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
#include <stdio.h>
#include <stdlib.h>

enum
{
    STATE_0 = 0,
    STATE_1,
    STATE_2,
    MAX_STATE
} State;

static const char *state_names[] =
    {"STATE_0", "STATE_1", "STATE_2", "unknown"};

enum
{
    EVENT_INC = 0,
    EVENT_DEC,
    MAX_EVENT
} Event;

static const char *event_names[] = {"EVENT_INC", "EVENT_DEC", "unknown"};

#define x_to_str(arr, maxidx, x)    (x < maxidx ? arr[x] : arr[maxidx])
#define state_to_str(s)             x_to_str(state_names, MAX_STATE, s)
#define event_to_str(e)             x_to_str(event_names, MAX_EVENT, e)

void on_enter(uint32_t cause, uint32_t current_state, uint32_t previous_state,
    void *data)
{
    printf("on_enter(cause=%s, current_state=%s, previous_state=%s, data=%p);"
        "\n", event_to_str(cause), state_to_str(current_state),
        state_to_str(previous_state), data);
}

void on_undefined(uint32_t cause, uint32_t current_state, void *data)
{
    printf("on_enter(cause=%s, current_state=%s, data=%p);\n",
        event_to_str(cause), state_to_str(current_state), data);
}

static const State_machine_transition transition_table[MAX_STATE][MAX_EVENT] =
{
    /* 0: STATE_0 */
    {
        /* 0: EVENT_INC */
        STATE_MACHINE_TRANSITION(STATE_0, EVENT_INC, STATE_1, on_enter),

        /* 1: EVENT_DEC */
        STATE_MACHINE_NO_TRANSITION(STATE_0, EVENT_DEC, on_undefined)
    },

    /* 1: STATE_1 */
    {
        /* 0: EVENT_INC */
        STATE_MACHINE_TRANSITION(STATE_1, EVENT_INC, STATE_2, on_enter),

        /* 1: EVENT_DEC */
        STATE_MACHINE_TRANSITION(STATE_1, EVENT_DEC, STATE_0, on_enter)
    },

    /* 2: STATE_2 */
    {
        /* 0: EVENT_INC */
        STATE_MACHINE_NO_TRANSITION(STATE_2, EVENT_INC, on_undefined),

        /* 1: EVENT_DEC */
        STATE_MACHINE_TRANSITION(STATE_2, EVENT_DEC, STATE_1, on_enter)
    }
};
int main()
{
    State_machine sm;
    State_machine_locking locking = STATE_MACHINE_NO_LOCKING;

    state_machine_init_table(&sm,
        MAX_STATE, MAX_EVENT, STATE_0,
        locking, (State_machine_transition *)transition_table, NULL);

    for (uint32_t event = EVENT_INC; event < MAX_EVENT; event++)
    {
        for (uint32_t run = 0; run < MAX_STATE; run++)
        {
            uint32_t state;

            if_sm_failure (state_machine_current_state(&sm, &state, 0))
            {
                exit(EXIT_FAILURE);
            }

            printf("State machine is in %s and we send it %s\n",
                state_to_str(state), event_to_str(event));

            if_sm_failure (state_machine_event(&sm, event, 0))
            {
                exit(EXIT_FAILURE);
            }

            if_sm_failure (state_machine_current_state(&sm, &state, 0))
            {
                exit(EXIT_FAILURE);
            }

            printf("Now state machine is in %s\n\n", state_to_str(state));
        }
    }

    exit(EXIT_SUCCESS);
}
