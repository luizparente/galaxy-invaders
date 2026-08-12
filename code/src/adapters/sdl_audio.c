#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>

#include "adapters/sdl_audio.h"

#define SAMPLE_RATE 44100
#define NUM_VOICES 20

/* --- Song timing: 140bpm, 16th-note step sequencer, 64-bar arrangement.
 * The boss theme shares the same grid (16th-note steps, 2-bar phrases,
 * 64-bar arrangement) but runs at 180bpm, so both arrangements share
 * STEPS_PER_BAR / MOTIF_BARS / MOTIF_STEPS / SONG_TOTAL_BARS / TOTAL_STEPS. --- */
#define BPM 140.0
#define BOSS_BPM 180.0
#define STEPS_PER_BAR 16
#define MOTIF_BARS 2
#define MOTIF_STEPS (STEPS_PER_BAR * MOTIF_BARS) /* 32: each pattern is a 2-bar phrase that loops within its section. */
#define SONG_TOTAL_BARS 64
#define TOTAL_STEPS (SONG_TOTAL_BARS * STEPS_PER_BAR)

typedef enum VoiceKind {
    VOICE_SINE_SWEEP,
    VOICE_SQUARE_BEEP,
    VOICE_KICK,
    VOICE_SNARE_NOISE,
    VOICE_HIHAT,
    VOICE_CRASH,
} VoiceKind;

typedef struct Voice {
    bool active;
    VoiceKind kind;
    double t;
    double duration;
    double freq0;
    double freq1;
    double phase;
    double amplitude;
    unsigned int rng_state;
} Voice;

/* A 2-bar (32 sixteenth-note step) musical phrase: distorted guitar, bass,
 * a lead/solo line, and a drum kit. 0.0 = rest, 0 = no drum hit. The
 * keyboard pad is not stored per-pattern; it tracks the bass line an
 * octave up with its own soft synth timbre (see kBass -> keys derivation). */
typedef struct Pattern {
    const double *guitar;
    const double *bass;
    const double *lead;
    const unsigned char *kick;
    const unsigned char *snare;
    const unsigned char *hihat;
    const unsigned char *crash;
} Pattern;

typedef struct SongSection {
    const Pattern *pattern;
    int bars;
} SongSection;

typedef struct SdlAudioCtx {
    SDL_AudioDeviceID device;
    SDL_AudioSpec spec;

    /* Shared with the game-update thread; guarded by SDL_Lock/UnlockAudioDevice. */
    bool paused;
    float difficulty01;
    bool boss_active;

    /* Audio-thread-owned sequencer state. */
    double music_time;
    int last_step_index;

    double guitar_root_phase;
    double guitar_fifth_phase;
    double guitar_prev_freq;
    double current_guitar_freq;

    double bass_phase;
    double bass_prev_freq;
    double current_bass_freq;

    double lead_phase;
    double lead_prev_freq;
    double lead_note_time;
    double current_lead_freq;

    double keys_phase_a;
    double keys_phase_b;
    double keys_prev_freq;
    double current_keys_freq;

    unsigned int noise_rng;

    Voice voices[NUM_VOICES];
} SdlAudioCtx;

/* ---- Note frequencies (Hz), equal temperament, E minor. ---- */
#define REST 0.0
/* Bass register. */
#define N_E1 41.20
#define N_F1 43.65
#define N_FS1 46.25
#define N_G1 49.00
#define N_A1 55.00
#define N_AS1 58.27
#define N_B1 61.74
#define N_CS2 69.30
#define N_D2 73.42
/* Guitar register. */
#define N_E2 82.41
#define N_F2 87.31
#define N_FS2 92.50
#define N_G2 98.00
#define N_AS2 116.54
#define N_B2 123.47
#define N_CS3 138.59
#define N_D3 146.83
#define N_E3 164.81
#define N_FS3 185.00
#define N_G3 196.00
#define N_A3 220.00
/* Lead/solo register. */
#define N_E4 329.63
#define N_FS4 369.99
#define N_G4 392.00
#define N_A4 440.00
#define N_B4 493.88
#define N_C5 523.25
#define N_D5 587.33
#define N_E5 659.25
#define N_G5 783.99
#define N_A5 880.00

/* ---- INTRO: sparse stab building into the main gallop. ---- */
static const double guitar_intro[MOTIF_STEPS] = {
    N_E3,0,0,0,      0,0,N_E3,0,      0,0,0,N_E3,      0,0,0,0,
    N_E3,0,N_E3,0,   N_E3,0,N_E3,0,   N_E3,0,N_E3,N_E3, N_E3,N_E3,N_E3,N_E3,
};
static const double bass_intro[MOTIF_STEPS] = {
    N_E1,0,0,0, 0,0,0,0,    0,0,0,0,    0,0,0,0,
    N_E1,0,0,0, N_E1,0,0,0, N_E1,0,0,0, N_E1,0,0,0,
};
static const double lead_intro[MOTIF_STEPS] = {
    0,0,0,0, 0,0,0,0, 0,0,0,0, N_E4,N_G4,N_B4,N_E5,
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
};
static const unsigned char kick_intro[MOTIF_STEPS] = {
    1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,1,1,
};
static const unsigned char snare_intro[MOTIF_STEPS] = {
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0,
};
static const unsigned char hihat_intro[MOTIF_STEPS] = {
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0,
};
static const unsigned char crash_intro[MOTIF_STEPS] = { 1,0,0,0 };

/* ---- RIFF_A: driving verse gallop, Em -> F#m answer phrase. ---- */
static const double guitar_riffA[MOTIF_STEPS] = {
    N_E3,0,N_E3,0,   N_E3,0,N_E3,N_E3,   N_G3,0,N_E3,0,   N_E3,0,N_D3,N_E3,
    N_FS3,0,N_FS3,0, N_FS3,0,N_FS3,N_FS3, N_E3,0,N_D3,0,   N_E3,0,N_B2,0,
};
static const double bass_riffA[MOTIF_STEPS] = {
    N_E1,0,N_E1,0,   N_E1,0,N_E1,N_E1,   N_G1,0,N_E1,0,   N_E1,0,N_D2,N_E1,
    N_FS1,0,N_FS1,0, N_FS1,0,N_FS1,N_FS1, N_E1,0,N_D2,0,   N_E1,0,N_B1,0,
};
static const double lead_riffA[MOTIF_STEPS] = {0};
static const unsigned char kick_riffA[MOTIF_STEPS] = {
    1,0,1,0, 1,0,1,1, 1,0,1,0, 1,0,1,1,
    1,0,1,0, 1,0,1,1, 1,0,1,0, 1,0,1,0,
};
static const unsigned char snare_riffA[MOTIF_STEPS] = {
    0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0,
    0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0,
};
static const unsigned char hihat_riffA[MOTIF_STEPS] = {
    1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0,
    1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0,
};
static const unsigned char crash_riffA[MOTIF_STEPS] = {0};

/* ---- RIFF_B: anthemic chorus, Em - G - Am - B / B - D - E - E. ---- */
static const double guitar_riffB[MOTIF_STEPS] = {
    N_E3,0,N_E3,0, N_G3,0,N_G3,0, N_A3,0,N_A3,0, N_B2,0,N_B2,0,
    N_B2,0,N_B2,0, N_D3,0,N_D3,0, N_E3,0,N_E3,0, N_E3,0,N_E3,N_E3,
};
static const double bass_riffB[MOTIF_STEPS] = {
    N_E1,0,N_E1,0, N_G1,0,N_G1,0, N_A1,0,N_A1,0, N_B1,0,N_B1,0,
    N_B1,0,N_B1,0, N_D2,0,N_D2,0, N_E1,0,N_E1,0, N_E1,0,N_E1,N_E1,
};
static const double lead_riffB[MOTIF_STEPS] = {0};
static const unsigned char kick_riffB[MOTIF_STEPS] = {
    1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0,
    1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,1,
};
static const unsigned char snare_riffB[MOTIF_STEPS] = {
    0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0,
    0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0,
};
static const unsigned char hihat_riffB[MOTIF_STEPS] = {
    1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,
    1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,
};
static const unsigned char crash_riffB[MOTIF_STEPS] = { 1,0,0,0 };

/* ---- BRIDGE: guitar/bass drop to half-time pulses, keys + lead solo on top. ---- */
static const double guitar_bridge[MOTIF_STEPS] = {
    N_E3,0,0,0, 0,0,0,0, N_G3,0,0,0, 0,0,0,0,
    N_A3,0,0,0, 0,0,0,0, N_B2,0,0,0, 0,0,0,0,
};
static const double bass_bridge[MOTIF_STEPS] = {
    N_E1,0,0,0, 0,0,0,0, N_G1,0,0,0, 0,0,0,0,
    N_A1,0,0,0, 0,0,0,0, N_B1,0,0,0, 0,0,0,0,
};
static const double lead_bridge[MOTIF_STEPS] = {
    N_E4,0,N_G4,N_A4, N_B4,0,N_A4,N_G4, N_B4,0,N_D5,0,     N_B4,N_A4,N_G4,0,
    N_A4,0,N_C5,N_D5, N_E5,0,N_D5,N_C5, N_B4,N_A4,N_G4,N_FS4, N_E4,0,0,0,
};
static const unsigned char kick_bridge[MOTIF_STEPS] = {
    1,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0,
    1,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0,
};
static const unsigned char snare_bridge[MOTIF_STEPS] = {
    0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0,
    0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0,
};
static const unsigned char hihat_bridge[MOTIF_STEPS] = {
    1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0,
    1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0,
};
static const unsigned char crash_bridge[MOTIF_STEPS] = { 1,0,0,0 };

/* ---- BREAKDOWN: heavy unison guitar/bass chugs, syncopated double kick. ---- */
static const double guitar_breakdown[MOTIF_STEPS] = {
    N_E2,0,0,N_E2, 0,0,N_E2,0, N_E2,0,0,N_E2, 0,0,N_E2,N_E2,
    N_E2,0,0,0,    0,0,0,0,    N_E2,0,0,N_E2, 0,0,N_E2,N_E2,
};
static const double bass_breakdown[MOTIF_STEPS] = {
    N_E2,0,0,N_E2, 0,0,N_E2,0, N_E2,0,0,N_E2, 0,0,N_E2,N_E2,
    N_E2,0,0,0,    0,0,0,0,    N_E2,0,0,N_E2, 0,0,N_E2,N_E2,
};
static const double lead_breakdown[MOTIF_STEPS] = {
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,N_E5,N_E5,
};
static const unsigned char kick_breakdown[MOTIF_STEPS] = {
    1,0,0,1, 0,0,1,0, 1,0,0,1, 0,0,1,1,
    1,0,0,0, 0,0,0,0, 1,0,0,1, 0,0,1,1,
};
static const unsigned char snare_breakdown[MOTIF_STEPS] = {
    1,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0,
    1,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0,
};
static const unsigned char hihat_breakdown[MOTIF_STEPS] = {0};
static const unsigned char crash_breakdown[MOTIF_STEPS] = { 1,0,0,0 };

/* ---- OUTRO: big reprise, climbing lead line back into the intro. ---- */
static const double guitar_outro[MOTIF_STEPS] = {
    N_E3,0,N_E3,0, N_G3,0,N_E3,0, N_E3,0,N_E3,0, N_G3,0,N_A3,0,
    N_B2,0,N_B2,0, N_D3,0,N_B2,0, N_E3,0,N_E3,0, N_E3,N_E3,N_E3,N_E3,
};
static const double bass_outro[MOTIF_STEPS] = {
    N_E1,0,N_E1,0, N_G1,0,N_E1,0, N_E1,0,N_E1,0, N_G1,0,N_A1,0,
    N_B1,0,N_B1,0, N_D2,0,N_B1,0, N_E1,0,N_E1,0, N_E1,N_E1,N_E1,N_E1,
};
static const double lead_outro[MOTIF_STEPS] = {
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    0,0,0,0, 0,0,0,0, N_B4,0,N_D5,0, N_E5,0,N_G5,0,
};
static const unsigned char kick_outro[MOTIF_STEPS] = {
    1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0,
    1,0,1,0, 1,0,1,0, 1,0,1,0, 1,1,1,1,
};
static const unsigned char snare_outro[MOTIF_STEPS] = {
    0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0,
    0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0,
};
static const unsigned char hihat_outro[MOTIF_STEPS] = {
    1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0,
    1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0,
};
static const unsigned char crash_outro[MOTIF_STEPS] = {
    1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    0,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0,
};

static const Pattern kPatternIntro = {
    guitar_intro, bass_intro, lead_intro, kick_intro, snare_intro, hihat_intro, crash_intro,
};
static const Pattern kPatternRiffA = {
    guitar_riffA, bass_riffA, lead_riffA, kick_riffA, snare_riffA, hihat_riffA, crash_riffA,
};
static const Pattern kPatternRiffB = {
    guitar_riffB, bass_riffB, lead_riffB, kick_riffB, snare_riffB, hihat_riffB, crash_riffB,
};
static const Pattern kPatternBridge = {
    guitar_bridge, bass_bridge, lead_bridge, kick_bridge, snare_bridge, hihat_bridge, crash_bridge,
};
static const Pattern kPatternBreakdown = {
    guitar_breakdown, bass_breakdown, lead_breakdown, kick_breakdown, snare_breakdown, hihat_breakdown, crash_breakdown,
};
static const Pattern kPatternOutro = {
    guitar_outro, bass_outro, lead_outro, kick_outro, snare_outro, hihat_outro, crash_outro,
};

/* Arrangement: intro, verse, chorus, verse, solo bridge, chorus, breakdown,
 * verse, big outro -- 4+8+8+8+8+8+4+8+8 = 64 bars, then loops. */
static const SongSection kSong[] = {
    { &kPatternIntro,     4 },
    { &kPatternRiffA,     8 },
    { &kPatternRiffB,     8 },
    { &kPatternRiffA,     8 },
    { &kPatternBridge,    8 },
    { &kPatternRiffB,     8 },
    { &kPatternBreakdown, 4 },
    { &kPatternRiffA,     8 },
    { &kPatternOutro,     8 },
};
#define SONG_NUM_SECTIONS (int)(sizeof(kSong) / sizeof(kSong[0]))

/* =========================================================================
 * BOSS THEME: 180bpm thrash metal, same 16th-note/2-bar-phrase grid as the
 * main song. 9 sections x (4|8 bars) = 64 bars, then loops for as long as
 * the boss stays on screen. Built from the E blues-scale/tritone vocabulary
 * (E-Bb "devil's interval", chromatic chugs, galloped 8th-16th-16ths) that
 * powers thrash riffing (Metallica/Slayer). ---------------------------- */

/* ---- BOSS_INTRO: dread tritone stabs building into the gallop. ---- */
static const double guitar_boss_intro[MOTIF_STEPS] = {
    N_E2,0,0,0,     N_AS2,0,0,0,   N_E2,0,0,0,   N_AS2,0,0,N_E2,
    N_E2,0,N_E2,0,  N_E2,0,N_E2,N_E2, N_E2,N_E2,N_E2,N_E2, N_E2,N_E2,N_E2,N_E2,
};
static const double bass_boss_intro[MOTIF_STEPS] = {
    N_E1,0,0,0,     N_AS1,0,0,0,   N_E1,0,0,0,   N_AS1,0,0,N_E1,
    N_E1,0,N_E1,0,  N_E1,0,N_E1,N_E1, N_E1,N_E1,N_E1,N_E1, N_E1,N_E1,N_E1,N_E1,
};
static const double lead_boss_intro[MOTIF_STEPS] = {
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,N_E5,N_G5,
};
static const unsigned char kick_boss_intro[MOTIF_STEPS] = {
    1,0,0,0, 1,0,0,0, 1,0,0,0, 1,0,0,1,
    1,0,1,0, 1,0,1,1, 1,1,1,1, 1,1,1,1,
};
static const unsigned char snare_boss_intro[MOTIF_STEPS] = {
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    0,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0,
};
static const unsigned char hihat_boss_intro[MOTIF_STEPS] = {
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0,
};
static const unsigned char crash_boss_intro[MOTIF_STEPS] = { 1,0,0,0 };

/* ---- BOSS_RIFFA: driving thrash gallop, chromatic walk-up tag. ---- */
static const double guitar_boss_riffA[MOTIF_STEPS] = {
    N_E2,0,N_E2,N_E2, 0,N_E2,0,N_E2, N_E2,0,N_E2,N_E2, N_F2,0,N_FS2,N_G2,
    N_E2,0,N_E2,N_E2, 0,N_E2,0,N_E2, N_E2,0,N_E2,N_E2, N_AS2,0,N_E2,0,
};
static const double bass_boss_riffA[MOTIF_STEPS] = {
    N_E1,0,N_E1,N_E1, 0,N_E1,0,N_E1, N_E1,0,N_E1,N_E1, N_F1,0,N_FS1,N_G1,
    N_E1,0,N_E1,N_E1, 0,N_E1,0,N_E1, N_E1,0,N_E1,N_E1, N_AS1,0,N_E1,0,
};
static const double lead_boss_riffA[MOTIF_STEPS] = {0};
static const unsigned char kick_boss_riffA[MOTIF_STEPS] = {
    1,0,1,1, 0,1,0,1, 1,0,1,1, 1,0,1,1,
    1,0,1,1, 0,1,0,1, 1,0,1,1, 1,0,1,0,
};
static const unsigned char snare_boss_riffA[MOTIF_STEPS] = {
    0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0,
    0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0,
};
static const unsigned char hihat_boss_riffA[MOTIF_STEPS] = {
    1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,
    1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,
};
static const unsigned char crash_boss_riffA[MOTIF_STEPS] = { 1,0,0,0 };

/* ---- BOSS_RIFFB: the tritone ("devil's interval") hook, sequenced up a
 * minor third for the second bar. ---- */
static const double guitar_boss_riffB[MOTIF_STEPS] = {
    N_E2,0,0,N_E2,   N_AS2,0,0,N_AS2, N_E2,0,0,N_E2,   N_AS2,0,N_AS2,0,
    N_G2,0,0,N_G2,   N_CS3,0,0,N_CS3, N_G2,0,0,N_G2,   N_CS3,0,N_CS3,N_CS3,
};
static const double bass_boss_riffB[MOTIF_STEPS] = {
    N_E1,0,0,N_E1,   N_AS1,0,0,N_AS1, N_E1,0,0,N_E1,   N_AS1,0,N_AS1,0,
    N_G1,0,0,N_G1,   N_CS2,0,0,N_CS2, N_G1,0,0,N_G1,   N_CS2,0,N_CS2,N_CS2,
};
static const double lead_boss_riffB[MOTIF_STEPS] = {0};
static const unsigned char kick_boss_riffB[MOTIF_STEPS] = {
    1,0,0,1, 1,0,0,1, 1,0,0,1, 1,0,1,0,
    1,0,0,1, 1,0,0,1, 1,0,0,1, 1,0,1,1,
};
static const unsigned char snare_boss_riffB[MOTIF_STEPS] = {
    0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0,
    0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0,
};
static const unsigned char hihat_boss_riffB[MOTIF_STEPS] = {
    1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0,
    1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0,
};
static const unsigned char crash_boss_riffB[MOTIF_STEPS] = {
    1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
};

/* ---- BOSS_SOLO: pedal-tone chug under a shredded scalar lead run. ---- */
static const double guitar_boss_solo[MOTIF_STEPS] = {
    N_E2,0,0,0, N_E2,0,0,0, N_E2,0,0,0, N_E2,0,0,0,
    N_E2,0,0,0, N_E2,0,0,0, N_E2,0,0,0, N_E2,0,0,0,
};
static const double bass_boss_solo[MOTIF_STEPS] = {
    N_E1,0,0,0, N_E1,0,0,0, N_E1,0,0,0, N_E1,0,0,0,
    N_E1,0,0,0, N_E1,0,0,0, N_E1,0,0,0, N_E1,0,0,0,
};
static const double lead_boss_solo[MOTIF_STEPS] = {
    N_E4,N_FS4,N_G4,N_A4, N_B4,N_C5,N_D5,N_E5, N_G5,N_E5,N_D5,N_C5, N_B4,N_A4,N_G4,N_FS4,
    N_G4,N_A4,N_B4,N_C5,  N_D5,N_E5,N_G5,N_A5, N_G5,N_E5,N_D5,N_C5, N_B4,N_A4,N_G4,N_E4,
};
static const unsigned char kick_boss_solo[MOTIF_STEPS] = {
    1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0,
    1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0,
};
static const unsigned char snare_boss_solo[MOTIF_STEPS] = {
    0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0,
    0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0,
};
static const unsigned char hihat_boss_solo[MOTIF_STEPS] = {
    1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,
    1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,
};
static const unsigned char crash_boss_solo[MOTIF_STEPS] = { 1,0,0,0 };

/* ---- BOSS_BREAKDOWN: unison stutter chug, near-constant kick. ---- */
static const double guitar_boss_breakdown[MOTIF_STEPS] = {
    N_E2,0,0,N_E2, 0,0,N_E2,0,   N_E2,0,0,N_E2, 0,N_E2,0,N_E2,
    N_E2,0,0,0,    0,0,0,0,      N_E2,N_E2,0,N_E2, 0,N_E2,N_E2,0,
};
static const double bass_boss_breakdown[MOTIF_STEPS] = {
    N_E1,0,0,N_E1, 0,0,N_E1,0,   N_E1,0,0,N_E1, 0,N_E1,0,N_E1,
    N_E1,0,0,0,    0,0,0,0,      N_E1,N_E1,0,N_E1, 0,N_E1,N_E1,0,
};
static const double lead_boss_breakdown[MOTIF_STEPS] = {
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,N_E5,N_G5,
};
static const unsigned char kick_boss_breakdown[MOTIF_STEPS] = {
    1,0,1,1, 0,1,1,0, 1,0,1,1, 0,1,0,1,
    1,1,0,1, 1,0,1,1, 1,1,1,0, 1,0,1,1,
};
static const unsigned char snare_boss_breakdown[MOTIF_STEPS] = {
    1,0,0,1, 0,0,1,0, 1,0,0,1, 0,1,0,1,
    1,0,0,0, 0,0,0,0, 1,1,0,1, 0,1,1,0,
};
static const unsigned char hihat_boss_breakdown[MOTIF_STEPS] = {0};
static const unsigned char crash_boss_breakdown[MOTIF_STEPS] = {
    1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
};

/* ---- BOSS_OUTRO: full-throttle wall of sound into a final scream. ---- */
static const double guitar_boss_outro[MOTIF_STEPS] = {
    N_E2,0,N_E2,N_E2, N_F2,0,N_FS2,N_G2, N_AS2,0,N_AS2,N_AS2, N_CS3,0,N_CS3,N_CS3,
    N_E2,N_E2,N_E2,N_E2, N_E2,N_E2,N_E2,N_E2, N_E2,N_E2,N_E2,N_E2, N_E2,N_E2,N_E2,N_E2,
};
static const double bass_boss_outro[MOTIF_STEPS] = {
    N_E1,0,N_E1,N_E1, N_F1,0,N_FS1,N_G1, N_AS1,0,N_AS1,N_AS1, N_CS2,0,N_CS2,N_CS2,
    N_E1,N_E1,N_E1,N_E1, N_E1,N_E1,N_E1,N_E1, N_E1,N_E1,N_E1,N_E1, N_E1,N_E1,N_E1,N_E1,
};
static const double lead_boss_outro[MOTIF_STEPS] = {
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    0,0,0,0, 0,0,0,0, 0,0,0,0, N_E5,0,N_G5,N_A5,
};
static const unsigned char kick_boss_outro[MOTIF_STEPS] = {
    1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0,
    1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,
};
static const unsigned char snare_boss_outro[MOTIF_STEPS] = {
    0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0,
    0,0,0,0, 1,0,0,0, 0,0,0,0, 1,1,1,1,
};
static const unsigned char hihat_boss_outro[MOTIF_STEPS] = {
    1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,
    1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,
};
static const unsigned char crash_boss_outro[MOTIF_STEPS] = {
    1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    1,0,0,0, 0,0,0,0, 0,0,0,0, 1,1,1,1,
};

static const Pattern kPatternBossIntro = {
    guitar_boss_intro, bass_boss_intro, lead_boss_intro, kick_boss_intro, snare_boss_intro, hihat_boss_intro, crash_boss_intro,
};
static const Pattern kPatternBossRiffA = {
    guitar_boss_riffA, bass_boss_riffA, lead_boss_riffA, kick_boss_riffA, snare_boss_riffA, hihat_boss_riffA, crash_boss_riffA,
};
static const Pattern kPatternBossRiffB = {
    guitar_boss_riffB, bass_boss_riffB, lead_boss_riffB, kick_boss_riffB, snare_boss_riffB, hihat_boss_riffB, crash_boss_riffB,
};
static const Pattern kPatternBossSolo = {
    guitar_boss_solo, bass_boss_solo, lead_boss_solo, kick_boss_solo, snare_boss_solo, hihat_boss_solo, crash_boss_solo,
};
static const Pattern kPatternBossBreakdown = {
    guitar_boss_breakdown, bass_boss_breakdown, lead_boss_breakdown, kick_boss_breakdown, snare_boss_breakdown, hihat_boss_breakdown, crash_boss_breakdown,
};
static const Pattern kPatternBossOutro = {
    guitar_boss_outro, bass_boss_outro, lead_boss_outro, kick_boss_outro, snare_boss_outro, hihat_boss_outro, crash_boss_outro,
};

/* Arrangement: dread intro, verse, tritone hook, verse, shred solo, tritone
 * hook, breakdown, verse, wall-of-sound outro -- 4+8+8+8+8+8+8+8+4 = 64
 * bars, then loops for as long as the boss stays alive. */
static const SongSection kBossSong[] = {
    { &kPatternBossIntro,     4 },
    { &kPatternBossRiffA,     8 },
    { &kPatternBossRiffB,     8 },
    { &kPatternBossRiffA,     8 },
    { &kPatternBossSolo,      8 },
    { &kPatternBossRiffB,     8 },
    { &kPatternBossBreakdown, 8 },
    { &kPatternBossRiffA,     8 },
    { &kPatternBossOutro,     4 },
};
#define BOSS_SONG_NUM_SECTIONS (int)(sizeof(kBossSong) / sizeof(kBossSong[0]))

static double square_wave(double phase) {
    return phase < 0.5 ? 1.0 : -1.0;
}

static double triangle_wave(double phase) {
    return 4.0 * fabs(phase - floor(phase + 0.5)) - 1.0;
}

static double saw_wave(double phase) {
    return 2.0 * (phase - floor(phase + 0.5));
}

/* Fades a step's amplitude in/out over `edge` seconds at each 16th-note
 * boundary, giving pitched channels a picked/chugged articulation. */
static double step_env(double time_in_step, double step_duration, double edge) {
    if (time_in_step < edge) return time_in_step / edge;
    double remain = step_duration - time_in_step;
    if (remain < edge) return remain / edge;
    return 1.0;
}

static float rng_noise(unsigned int *state) {
    *state ^= *state << 13;
    *state ^= *state >> 17;
    *state ^= *state << 5;
    return ((float)(*state % 20000) / 10000.0f) - 1.0f;
}

static Voice *find_free_voice(SdlAudioCtx *ctx) {
    for (int i = 0; i < NUM_VOICES; i++) {
        if (!ctx->voices[i].active) return &ctx->voices[i];
    }
    return NULL;
}

static void trigger_voice(SdlAudioCtx *ctx, VoiceKind kind, double freq0, double freq1,
                           double duration, double amplitude) {
    Voice *v = find_free_voice(ctx);
    if (!v) return;
    v->active = true;
    v->kind = kind;
    v->t = 0.0;
    v->duration = duration;
    v->freq0 = freq0;
    v->freq1 = freq1;
    v->phase = 0.0;
    v->amplitude = amplitude;
    v->rng_state = ctx->noise_rng++ * 2654435761u + 1u;
}

static float render_voice(Voice *v, double dt) {
    if (!v->active) return 0.0f;

    double t01 = v->t / v->duration;
    double freq = v->freq0 + (v->freq1 - v->freq0) * t01;
    double envelope = 1.0 - t01;
    double fade_in = v->t < 0.003 ? v->t / 0.003 : 1.0;
    double env = envelope * fade_in;

    float sample = 0.0f;
    switch (v->kind) {
        case VOICE_SINE_SWEEP:
            sample = (float)(sin(v->phase * 2.0 * M_PI) * env);
            v->phase += freq * dt;
            if (v->phase >= 1.0) v->phase -= 1.0;
            break;
        case VOICE_SQUARE_BEEP:
            sample = (float)(square_wave(v->phase) * env);
            v->phase += freq * dt;
            if (v->phase >= 1.0) v->phase -= 1.0;
            break;
        case VOICE_KICK:
            sample = (float)(sin(v->phase * 2.0 * M_PI) * env * env);
            v->phase += freq * dt;
            if (v->phase >= 1.0) v->phase -= 1.0;
            break;
        case VOICE_SNARE_NOISE:
            sample = rng_noise(&v->rng_state) * (float)(env * env);
            break;
        case VOICE_HIHAT:
            sample = rng_noise(&v->rng_state) * (float)(env * env * env);
            break;
        case VOICE_CRASH:
            sample = rng_noise(&v->rng_state) * (float)env;
            break;
    }

    v->t += dt;
    if (v->t >= v->duration) v->active = false;
    return sample * (float)v->amplitude;
}

/* Advances the sequencer by one 16th-note step: resolves the current song
 * section/pattern (regular arrangement, or the boss theme while a boss is
 * on screen), latches the new per-channel target notes, and (unless
 * paused) fires any drum hits scheduled on this step. */
static void advance_step(SdlAudioCtx *ctx, int global_step, bool paused, bool boss_active) {
    int bar = global_step / STEPS_PER_BAR;
    int step_in_bar = global_step % STEPS_PER_BAR;

    const SongSection *song = boss_active ? kBossSong : kSong;
    int num_sections = boss_active ? BOSS_SONG_NUM_SECTIONS : SONG_NUM_SECTIONS;

    const Pattern *pat = song[0].pattern;
    int bar_cursor = 0;
    for (int s = 0; s < num_sections; s++) {
        if (bar < bar_cursor + song[s].bars) {
            pat = song[s].pattern;
            break;
        }
        bar_cursor += song[s].bars;
    }
    int bar_in_section = bar - bar_cursor;
    int motif_step = (bar_in_section % MOTIF_BARS) * STEPS_PER_BAR + step_in_bar;

    double guitar_freq = pat->guitar[motif_step];
    double bass_freq = pat->bass[motif_step];
    double lead_freq = pat->lead[motif_step];
    double keys_freq = bass_freq > 0.0 ? bass_freq * 2.0 : 0.0;

    if (guitar_freq != ctx->guitar_prev_freq) {
        ctx->guitar_root_phase = 0.0;
        ctx->guitar_fifth_phase = 0.0;
    }
    ctx->guitar_prev_freq = guitar_freq;
    ctx->current_guitar_freq = guitar_freq;

    if (bass_freq != ctx->bass_prev_freq) ctx->bass_phase = 0.0;
    ctx->bass_prev_freq = bass_freq;
    ctx->current_bass_freq = bass_freq;

    if (lead_freq != ctx->lead_prev_freq) {
        ctx->lead_phase = 0.0;
        ctx->lead_note_time = 0.0;
    }
    ctx->lead_prev_freq = lead_freq;
    ctx->current_lead_freq = lead_freq;

    if (keys_freq != ctx->keys_prev_freq) {
        ctx->keys_phase_a = 0.0;
        ctx->keys_phase_b = 0.0;
    }
    ctx->keys_prev_freq = keys_freq;
    ctx->current_keys_freq = keys_freq;

    if (!paused) {
        if (pat->kick[motif_step]) trigger_voice(ctx, VOICE_KICK, 150.0, 45.0, 0.11, 0.55);
        if (pat->snare[motif_step]) trigger_voice(ctx, VOICE_SNARE_NOISE, 0.0, 0.0, 0.09, 0.4);
        if (pat->hihat[motif_step]) trigger_voice(ctx, VOICE_HIHAT, 0.0, 0.0, 0.045, 0.16);
        if (pat->crash[motif_step]) trigger_voice(ctx, VOICE_CRASH, 0.0, 0.0, 0.9, 0.32);
    }
}

static void audio_callback(void *userdata, Uint8 *stream, int len) {
    SdlAudioCtx *ctx = userdata;
    float *out = (float *)stream;
    int frames = len / (int)sizeof(float);
    double dt = 1.0 / (double)ctx->spec.freq;

    bool paused = ctx->paused;
    float difficulty01 = ctx->difficulty01;
    bool boss_active = ctx->boss_active;
    double step_duration = 60.0 / (boss_active ? BOSS_BPM : BPM) / 4.0; /* sixteenth notes */
    float energy_gain = 1.0f + 0.15f * difficulty01;

    for (int i = 0; i < frames; i++) {
        int global_step = (int)fmod(ctx->music_time / step_duration, (double)TOTAL_STEPS);
        if (global_step != ctx->last_step_index) {
            ctx->last_step_index = global_step;
            advance_step(ctx, global_step, paused, boss_active);
        }

        float music_sample = 0.0f;
        if (!paused) {
            double time_in_step = fmod(ctx->music_time, step_duration);
            double env_tight = step_env(time_in_step, step_duration, 0.004);
            double env_lead = step_env(time_in_step, step_duration, 0.008);
            double env_soft = step_env(time_in_step, step_duration, 0.015);

            if (ctx->current_guitar_freq > 0.0) {
                double root = saw_wave(ctx->guitar_root_phase);
                double fifth = saw_wave(ctx->guitar_fifth_phase);
                double mixed = root * 0.62 + fifth * 0.38;
                double driven = tanh(mixed * 2.6);
                music_sample += (float)(driven * 0.20 * env_tight);
                ctx->guitar_root_phase += ctx->current_guitar_freq * dt;
                if (ctx->guitar_root_phase >= 1.0) ctx->guitar_root_phase -= 1.0;
                ctx->guitar_fifth_phase += ctx->current_guitar_freq * 1.5 * dt;
                if (ctx->guitar_fifth_phase >= 1.0) ctx->guitar_fifth_phase -= 1.0;
            }

            if (ctx->current_bass_freq > 0.0) {
                music_sample += (float)(triangle_wave(ctx->bass_phase) * 0.17 * env_tight);
                ctx->bass_phase += ctx->current_bass_freq * dt;
                if (ctx->bass_phase >= 1.0) ctx->bass_phase -= 1.0;
            }

            if (ctx->current_lead_freq > 0.0) {
                double vibrato = 1.0 + 0.004 * sin(2.0 * M_PI * 6.0 * ctx->lead_note_time);
                double driven = tanh(saw_wave(ctx->lead_phase) * 1.6);
                music_sample += (float)(driven * 0.15 * env_lead);
                ctx->lead_phase += ctx->current_lead_freq * vibrato * dt;
                if (ctx->lead_phase >= 1.0) ctx->lead_phase -= 1.0;
                ctx->lead_note_time += dt;
            }

            if (ctx->current_keys_freq > 0.0) {
                double a = sin(ctx->keys_phase_a * 2.0 * M_PI);
                double b = sin(ctx->keys_phase_b * 2.0 * M_PI);
                music_sample += (float)((a * 0.5 + b * 0.5) * 0.09 * env_soft);
                ctx->keys_phase_a += ctx->current_keys_freq * dt;
                if (ctx->keys_phase_a >= 1.0) ctx->keys_phase_a -= 1.0;
                ctx->keys_phase_b += ctx->current_keys_freq * 1.003 * dt;
                if (ctx->keys_phase_b >= 1.0) ctx->keys_phase_b -= 1.0;
            }

            music_sample *= energy_gain;
        }

        float voice_sample = 0.0f;
        for (int v = 0; v < NUM_VOICES; v++) {
            voice_sample += render_voice(&ctx->voices[v], dt);
        }

        float sample = music_sample + voice_sample * 0.5f;
        if (sample > 1.0f) sample = 1.0f;
        if (sample < -1.0f) sample = -1.0f;
        out[i] = sample;

        ctx->music_time += dt;
    }
}

static void sdl_audio_update(void *self, float dt, bool paused, float difficulty01, bool boss_active) {
    (void)dt;
    SdlAudioCtx *ctx = self;
    SDL_LockAudioDevice(ctx->device);
    ctx->paused = paused;
    ctx->difficulty01 = difficulty01;
    if (boss_active != ctx->boss_active) {
        /* Cut cleanly to bar 1 of the new arrangement instead of carrying
         * over a step index that means something different at the other
         * song's BPM/section layout. */
        ctx->boss_active = boss_active;
        ctx->music_time = 0.0;
        ctx->last_step_index = -1;
    }
    SDL_UnlockAudioDevice(ctx->device);
}

static void sdl_audio_play_sfx(void *self, SfxId sfx) {
    SdlAudioCtx *ctx = self;
    SDL_LockAudioDevice(ctx->device);
    switch (sfx) {
        case SFX_PLAYER_SHOOT:
            trigger_voice(ctx, VOICE_SQUARE_BEEP, 110.0, 90.0, 0.05, 0.5);
            break;
        case SFX_ENEMY_DESTROYED:
            trigger_voice(ctx, VOICE_SINE_SWEEP, 200.0, 400.0, 0.10, 0.6);
            break;
        case SFX_PLAYER_DESTROYED:
            trigger_voice(ctx, VOICE_SINE_SWEEP, 300.0, 50.0, 0.20, 0.7);
            break;
        case SFX_MENU_SELECT:
            trigger_voice(ctx, VOICE_SQUARE_BEEP, 400.0, 400.0, 0.05, 0.4);
            break;
        case SFX_ORB_CAPTURED:
            trigger_voice(ctx, VOICE_SINE_SWEEP, 500.0, 1100.0, 0.25, 0.7);
            break;
        case SFX_ORB_DESTROYED:
            trigger_voice(ctx, VOICE_SINE_SWEEP, 650.0, 140.0, 0.15, 0.55);
            trigger_voice(ctx, VOICE_SNARE_NOISE, 0.0, 0.0, 0.12, 0.45);
            break;
        case SFX_BOSS_ARRIVED:
            trigger_voice(ctx, VOICE_SINE_SWEEP, 800.0, 120.0, 0.45, 0.75);
            trigger_voice(ctx, VOICE_KICK, 140.0, 35.0, 0.20, 0.6);
            break;
        case SFX_BOSS_HIT:
            trigger_voice(ctx, VOICE_SQUARE_BEEP, 260.0, 200.0, 0.04, 0.35);
            break;
        case SFX_BOSS_DEFEATED:
            trigger_voice(ctx, VOICE_SINE_SWEEP, 150.0, 900.0, 0.4, 0.75);
            trigger_voice(ctx, VOICE_SNARE_NOISE, 0.0, 0.0, 0.3, 0.5);
            trigger_voice(ctx, VOICE_KICK, 160.0, 40.0, 0.25, 0.6);
            break;
    }
    SDL_UnlockAudioDevice(ctx->device);
}

static void sdl_audio_destroy(void *self) {
    SdlAudioCtx *ctx = self;
    if (!ctx) return;
    if (ctx->device) SDL_CloseAudioDevice(ctx->device);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    free(ctx);
}

AudioPort *sdl_audio_create(void) {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL audio init failed: %s\n", SDL_GetError());
        return NULL;
    }

    SdlAudioCtx *ctx = calloc(1, sizeof(SdlAudioCtx));
    if (!ctx) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return NULL;
    }
    ctx->noise_rng = 0x9e3779b9u;
    ctx->last_step_index = -1;

    SDL_AudioSpec desired;
    SDL_zero(desired);
    desired.freq = SAMPLE_RATE;
    desired.format = AUDIO_F32SYS;
    desired.channels = 1;
    desired.samples = 1024;
    desired.callback = audio_callback;
    desired.userdata = ctx;

    ctx->device = SDL_OpenAudioDevice(NULL, 0, &desired, &ctx->spec, 0);
    if (ctx->device == 0) {
        fprintf(stderr, "SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        free(ctx);
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return NULL;
    }

    AudioPort *port = calloc(1, sizeof(AudioPort));
    if (!port) {
        SDL_CloseAudioDevice(ctx->device);
        free(ctx);
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return NULL;
    }
    port->self = ctx;
    port->update = sdl_audio_update;
    port->play_sfx = sdl_audio_play_sfx;
    port->destroy = sdl_audio_destroy;

    SDL_PauseAudioDevice(ctx->device, 0);
    return port;
}
