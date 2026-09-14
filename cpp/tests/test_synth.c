/*
 * Synth-level tests for MonkSynthEngine.
 * Uses synth_internal.h to peek at the held-note stack and unison voices.
 */

#include "synth.h"
#include "synth_internal.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

#define SR 44100.0f

static int float_near(float a, float b, float tol) {
    float d = a - b;
    if (d < 0.0f)
        d = -d;
    return d <= tol;
}

static void test_note_stack_lifo(void) {
    MonkSynthEngine *s = monk_synth_new(SR);
    assert(s);

    monk_synth_note_on(s, 60, 1.0f);
    monk_synth_note_on(s, 64, 1.0f);
    monk_synth_note_on(s, 67, 1.0f);

    assert(s->held_count == 3);
    assert(s->held[0].note == 60);
    assert(s->held[1].note == 64);
    assert(s->held[2].note == 67);

    /* Release the newest note: should fall back to 64 */
    monk_synth_note_off(s, 67);
    assert(s->held_count == 2);
    assert(s->held[s->held_count - 1].note == 64);

    /* Release the middle note: stack still has 60; top is 60 */
    monk_synth_note_off(s, 64);
    assert(s->held_count == 1);
    assert(s->held[s->held_count - 1].note == 60);

    monk_synth_free(s);
}

static void test_note_stack_overflow(void) {
    MonkSynthEngine *s = monk_synth_new(SR);
    assert(s);

    /* Push 20 notes into a 16-slot stack */
    for (uint8_t n = 50; n < 70; n++)
        monk_synth_note_on(s, n, 1.0f);

    /* The stack caps at 16; the first 16 notes are kept, later pushes drop. */
    assert(s->held_count == MONK_MAX_NOTES);
    assert(s->held[0].note == 50);
    assert(s->held[MONK_MAX_NOTES - 1].note == 65);

    monk_synth_free(s);
}

static void test_unison_detune_propagates(void) {
    MonkSynthEngine *s = monk_synth_new(SR);
    assert(s);

    monk_synth_set_unison(s, 3);
    monk_synth_set_unison_detune(s, 50.0f); /* ±50 cents */
    monk_synth_note_on(s, 69, 1.0f);        /* A4 */

    assert(s->unison_count == 3);

    /* Voice 0 should be detuned -50 cents, voice 2 +50 cents, voice 1 center.
     * 50 cents = 0.5 semitones in MIDI note space. */
    float center = s->voices[1].target_pitch;
    float v0 = s->voices[0].target_pitch;
    float v2 = s->voices[2].target_pitch;

    assert(float_near(center, 69.0f, 0.01f));
    assert(float_near(v0, 69.0f - 0.5f, 0.01f));
    assert(float_near(v2, 69.0f + 0.5f, 0.01f));

    monk_synth_free(s);
}

static void test_pitch_bend_propagates_to_all_voices(void) {
    MonkSynthEngine *s = monk_synth_new(SR);
    assert(s);

    monk_synth_set_unison(s, 5);
    monk_synth_note_on(s, 60, 1.0f);
    monk_synth_set_pitch_bend(s, 7.0f);

    for (int i = 0; i < 5; i++)
        assert(float_near(s->voices[i].pitch_bend_offset, 7.0f, 1e-6f));

    monk_synth_set_pitch_bend(s, -3.5f);
    for (int i = 0; i < 5; i++)
        assert(float_near(s->voices[i].pitch_bend_offset, -3.5f, 1e-6f));

    monk_synth_free(s);
}

static void test_reset_clears_notes(void) {
    MonkSynthEngine *s = monk_synth_new(SR);
    assert(s);

    monk_synth_note_on(s, 60, 1.0f);
    monk_synth_note_on(s, 64, 1.0f);
    assert(s->held_count == 2);

    monk_synth_reset(s);
    assert(s->held_count == 0);

    monk_synth_free(s);
}

/* Rendering is silent until note-on, and the note starts where the caller
 * splits the render — the property the VST3 shell relies on to place notes
 * at their sample offset (#22). */
static void test_process_starts_at_split_point(void) {
    MonkSynthEngine *s = monk_synth_new(SR);
    assert(s);
    monk_synth_set_attack(s, 0.0f);

    float l[256], r[256];
    for (int i = 0; i < 256; i++)
        l[i] = r[i] = 123.0f; /* sentinel: every sample must be overwritten */

    monk_synth_process(s, l, r, 100);
    for (int i = 0; i < 100; i++)
        assert(l[i] == 0.0f && r[i] == 0.0f);

    monk_synth_note_on(s, 60, 1.0f);
    monk_synth_process(s, l + 100, r + 100, 156);

    int nonzero = 0;
    for (int i = 100; i < 256; i++) {
        assert(l[i] != 123.0f && r[i] != 123.0f);
        if (l[i] != 0.0f)
            nonzero++;
    }
    assert(nonzero > 0);

    monk_synth_free(s);
}

/* Splitting a block into many small calls must produce the same audio as
 * one call: no per-call state may depend on the call length. */
static void test_process_split_equals_whole(void) {
    MonkSynthEngine *a = monk_synth_new(SR);
    MonkSynthEngine *b = monk_synth_new(SR);
    assert(a && b);
    monk_synth_set_unison(a, 3);
    monk_synth_set_unison(b, 3);
    /* No glide: the pitch-compensated output gain is re-targeted per call,
     * so a moving pitch would legitimately differ between chunkings. */
    monk_synth_set_glide(a, 0.0f);
    monk_synth_set_glide(b, 0.0f);
    monk_synth_note_on(a, 57, 0.8f);
    monk_synth_note_on(b, 57, 0.8f);

    enum { N = 4096 };
    static float la[N], ra[N], lb[N], rb[N];
    monk_synth_process(a, la, ra, N);

    /* Irregular chunk sizes, including single samples. */
    uint32_t sizes[] = {1, 7, 64, 1, 500, 3, 1000, 2};
    uint32_t pos = 0, k = 0;
    while (pos < N) {
        uint32_t n = sizes[k++ % 8];
        if (pos + n > N)
            n = N - pos;
        monk_synth_process(b, lb + pos, rb + pos, n);
        pos += n;
    }

    for (int i = 0; i < N; i++) {
        assert(la[i] == lb[i]);
        assert(ra[i] == rb[i]);
    }

    monk_synth_free(a);
    monk_synth_free(b);
}

/* Blocks longer than the internal scratch buffer are rendered in full. */
static void test_process_block_larger_than_scratch(void) {
    MonkSynthEngine *s = monk_synth_new(SR);
    assert(s);
    monk_synth_note_on(s, 60, 1.0f);

    enum { N = MONK_MAX_BUF + 1000 };
    static float l[N], r[N];
    for (int i = 0; i < N; i++)
        l[i] = r[i] = 123.0f;

    monk_synth_process(s, l, r, N);
    for (int i = 0; i < N; i++)
        assert(l[i] != 123.0f && r[i] != 123.0f);

    monk_synth_free(s);
}

/* A unison change ramps the gain over a fixed time, not over one call, so
 * a tiny render right after the change must not jump to the new gain. */
static void test_unison_gain_ramp_is_time_based(void) {
    MonkSynthEngine *s = monk_synth_new(SR);
    assert(s);
    monk_synth_note_on(s, 60, 1.0f);

    float l[64], r[64];
    monk_synth_process(s, l, r, 64);
    assert(float_near(s->current_voice_gain, 1.0f, 1e-6f));

    monk_synth_set_unison(s, 9); /* target 1/3 */
    monk_synth_process(s, l, r, 1);
    /* After one sample of a ~5 ms ramp we should have moved only a little. */
    assert(s->current_voice_gain > 0.95f);
    assert(s->current_voice_gain < 1.0f);

    /* ...and after a good while, we should have arrived. */
    for (int i = 0; i < 100; i++)
        monk_synth_process(s, l, r, 64);
    assert(float_near(s->current_voice_gain, s->target_voice_gain, 1e-3f));

    monk_synth_free(s);
}

int main(void) {
    test_note_stack_lifo();
    test_note_stack_overflow();
    test_unison_detune_propagates();
    test_pitch_bend_propagates_to_all_voices();
    test_reset_clears_notes();
    test_process_starts_at_split_point();
    test_process_split_equals_whole();
    test_process_block_larger_than_scratch();
    test_unison_gain_ramp_is_time_based();

    printf("test_synth: all tests passed\n");
    return 0;
}
