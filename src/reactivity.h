/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <obs.h>
#include <obs-audio-controls.h>
#include <stdatomic.h>
struct reactivity {
    obs_volmeter_t *meter;
    obs_weak_source_t *weak;
    char *uuid;
    atomic_int peak_mdb, magnitude_mdb;
    atomic_uint_fast64_t sample_time;
    bool enabled;
    float depth, gain, threshold, attack, release, envelope, linear_envelope;
    uint64_t retry_time;
};
void reactivity_init(struct reactivity *a);
void reactivity_update(struct reactivity *a, obs_data_t *settings);
float reactivity_tick(struct reactivity *a, float seconds);
void reactivity_destroy(struct reactivity *a);
void reactivity_defaults(obs_data_t *settings);
void reactivity_properties(obs_properties_t *properties, obs_data_t *settings, obs_source_t *owner);
