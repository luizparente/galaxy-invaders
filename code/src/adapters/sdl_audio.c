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
/* The GAME OVER theme (see the big comment block above kGameOverSong below)
 * shares the same STEPS_PER_BAR/MOTIF_BARS/MOTIF_STEPS grid but is its own
 * much slower tempo and a much shorter, non-looping-within-a-fight
 * arrangement length (32 bars, per spec, instead of the other two songs'
 * 64) - so unlike BOSS_BPM piggybacking on the shared TOTAL_STEPS, this one
 * needs its own step-count constant too. */
#define GAME_OVER_BPM 76.0
#define GAME_OVER_SONG_TOTAL_BARS 72
#define GAME_OVER_TOTAL_STEPS (GAME_OVER_SONG_TOTAL_BARS * STEPS_PER_BAR)
/* Empirically tuned (see the RMS comparison this was measured against) so
 * GAME OVER's own overall loudness lands in the same range as the main/
 * boss themes despite its guitar being boosted and constantly present. */
static const double kGameOverMixTrim = 0.58;

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
    /* False (the default every existing Pattern literal below gets for
     * free, since C zero-fills any trailing struct field a positional
     * initializer doesn't mention) means the driven saw+tanh distortion
     * every pattern above this one already uses, unchanged. True switches
     * the guitar voice to a clean, chorused triangle-wave tone instead -
     * see the guitar_clean branch in audio_callback. Only the GAME OVER
     * theme's own quiet sections ever set this. */
    bool guitar_clean;
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
    bool game_over_active;

    /* Audio-thread-owned sequencer state. */
    double music_time;
    int last_step_index;

    double guitar_root_phase;
    double guitar_fifth_phase;
    double guitar_prev_freq;
    double current_guitar_freq;
    bool guitar_clean;
    /* GAME OVER's own clean-section doubling layer - the written note at
     * its own plain pitch (one octave *below* guitar_root_phase's own
     * clean_freq, see the guitar_clean branch in audio_callback), through
     * a slightly-driven synth voice. Unused whenever guitar_clean is
     * false. */
    double synth_double_phase;
    /* Time since the doubled synth's own current note last changed pitch -
     * drives a one-shot attack at the start of each note (see
     * kSynthDoubleAttack in audio_callback) instead of env_soft's own
     * every-16th-step re-trigger, so a note held across a whole beat (the
     * normal case for GAME OVER's clean sections) sings as one continuous
     * tone rather than 4 stroked micro-attacks. */
    double synth_double_note_time;
    /* A second copy of the clean/chorus voice (guitar_root_phase/
     * guitar_fifth_phase), one octave above THAT voice's own clean_freq -
     * so across the 3 voices (this one, the clean chorus voice, and
     * synth_double_phase above) every octave from the written note up to
     * 2 octaves above it is covered, one voice per octave. */
    double clean_high_root_phase;
    double clean_high_fifth_phase;
    /* Time since the 2 chorus voices' own current note last changed pitch
     * - same one-shot-attack-then-flat-sustain envelope reasoning as
     * synth_double_note_time above, shared by both since they always
     * change note at the exact same instant (the same guitar_freq
     * change). */
    double clean_note_time;

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
/* The GAME OVER theme below is in E minor too (see its own big comment),
 * so it reuses almost every note above as-is (E1/A1/B1/E2/B2/G2/E3/A3/E4/
 * F#4/G4/A4/B4/C5/D5/E5/G5/A5) - it only needs its own VI chord's root (C,
 * scale degree 6 of E natural minor), which nothing above ever did. */
#define N_C2 65.41
#define N_C3 130.81

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
    guitar_intro, bass_intro, lead_intro, kick_intro, snare_intro, hihat_intro, crash_intro, false,
};
static const Pattern kPatternRiffA = {
    guitar_riffA, bass_riffA, lead_riffA, kick_riffA, snare_riffA, hihat_riffA, crash_riffA, false,
};
static const Pattern kPatternRiffB = {
    guitar_riffB, bass_riffB, lead_riffB, kick_riffB, snare_riffB, hihat_riffB, crash_riffB, false,
};
static const Pattern kPatternBridge = {
    guitar_bridge, bass_bridge, lead_bridge, kick_bridge, snare_bridge, hihat_bridge, crash_bridge, false,
};
static const Pattern kPatternBreakdown = {
    guitar_breakdown, bass_breakdown, lead_breakdown, kick_breakdown, snare_breakdown, hihat_breakdown, crash_breakdown, false,
};
static const Pattern kPatternOutro = {
    guitar_outro, bass_outro, lead_outro, kick_outro, snare_outro, hihat_outro, crash_outro, false,
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
    guitar_boss_intro, bass_boss_intro, lead_boss_intro, kick_boss_intro, snare_boss_intro, hihat_boss_intro, crash_boss_intro, false,
};
static const Pattern kPatternBossRiffA = {
    guitar_boss_riffA, bass_boss_riffA, lead_boss_riffA, kick_boss_riffA, snare_boss_riffA, hihat_boss_riffA, crash_boss_riffA, false,
};
static const Pattern kPatternBossRiffB = {
    guitar_boss_riffB, bass_boss_riffB, lead_boss_riffB, kick_boss_riffB, snare_boss_riffB, hihat_boss_riffB, crash_boss_riffB, false,
};
static const Pattern kPatternBossSolo = {
    guitar_boss_solo, bass_boss_solo, lead_boss_solo, kick_boss_solo, snare_boss_solo, hihat_boss_solo, crash_boss_solo, false,
};
static const Pattern kPatternBossBreakdown = {
    guitar_boss_breakdown, bass_boss_breakdown, lead_boss_breakdown, kick_boss_breakdown, snare_boss_breakdown, hihat_boss_breakdown, crash_boss_breakdown, false,
};
static const Pattern kPatternBossOutro = {
    guitar_boss_outro, bass_boss_outro, lead_boss_outro, kick_boss_outro, snare_boss_outro, hihat_boss_outro, crash_boss_outro, false,
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

/* =========================================================================
 * GAME OVER THEME: 76bpm, E minor, 72 bars, rewritten again after two
 * rounds of feedback on top of the original Pantera "Hollow"-inspired
 * production brief:
 *   - Strict 4/4, "4 by 4" lock: every instrument's own attacks land only
 *     on the 4 quarter-note beats of a bar (steps 0/4/8/12 of each 16-step
 *     bar) - guitar and bass play one chord tone per beat, held for the
 *     whole beat; kick always fires on a subset of those same 4 beat
 *     positions, never in between, so kick is always literally hitting
 *     the same instant the guitar/bass attack - true lock, not two
 *     independently-timed parts that happen to overlap. Snare's own
 *     classic 2-and-4 backbeat is itself just beats 2 and 4, so it's
 *     already on-grid for free. Only hihat ever subdivides between beats
 *     (a plain, unsyncopated 8th-note pulse - still a clean subdivision of
 *     the same 4/4 grid, not an independent rhythm).
 *   - Clean-with-chorus guitar (see the guitar_clean branch in
 *     audio_callback) for every section before the riff turns heavy -
 *     triangle wave, no distortion at all, a slow detuned second voice for
 *     chorus width - instead of the driven saw+tanh tone the heavy
 *     sections still use.
 *   - A melancholic synth pad: the existing keys voice (already shadowing
 *     the bass an octave up) gets a slow amplitude swell/breathing motion,
 *     scoped to this song only (see the mode == SONG_MODE_GAME_OVER branch
 *     in the keys block of audio_callback).
 *   - The lead's own bend-up-into-every-note (added for the previous
 *     revision, see its own comment in audio_callback) is unchanged and
 *     still scoped to this song.
 * Arrangement unchanged in shape from the last revision - quiet intro,
 * 16-bar ballad (Em-C, Am-Em), 8-bar tension build, 16-bar heavy riff
 * (Em-C, Am-Em), 16-bar lead break over the same riff, 8-bar final peak,
 * then loops - but every pattern's own note/drum content below is
 * rewritten to the strict beat-lock above. Plays only while
 * gs.state == STATE_GAME_OVER (see sdl_audio_update's own game_over_active
 * parameter), nowhere else. -------------------------- */

/* ---- Shared rhythm-section arrays reused verbatim across multiple
 * sections below (same underlying pattern, so one definition covers all of
 * them) - snare's plain 2-and-4 backbeat, a soft on-the-beat hihat for the
 * sparsest sections, an 8th-note hihat for the fuller ones, and drum-
 * silence placeholders for lead/hihat where a given section has none. ---- */
static const unsigned char snare_go_backbeat[MOTIF_STEPS] = {
    0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0,
    0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0,
};
static const unsigned char hihat_go_quarter[MOTIF_STEPS] = {
    1,0,0,0, 1,0,0,0, 1,0,0,0, 1,0,0,0,
    1,0,0,0, 1,0,0,0, 1,0,0,0, 1,0,0,0,
};
static const unsigned char hihat_go_8ths[MOTIF_STEPS] = {
    1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0,
    1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0,
};
static const unsigned char hihat_go_silent[MOTIF_STEPS] = {0};
static const unsigned char crash_go_silent[MOTIF_STEPS] = {0};
static const double lead_go_silent[MOTIF_STEPS] = {0};

/* ---- INTRO: one clean chord tone per beat (root-3rd-root-5th over 4
 * beats), each held the full beat, over a true one-note-per-bar bass
 * pedal. Kick on beat 1 only, snare/hihat keep steady time regardless -
 * the rhythm section never actually stops, so this reads as a song
 * already playing, not silence. ---- */
static const double guitar_go_intro[MOTIF_STEPS] = {
    N_E3,N_E3,N_E3,N_E3, N_G3,N_G3,N_G3,N_G3, N_E3,N_E3,N_E3,N_E3, N_B2,N_B2,N_B2,N_B2,
    N_C3,N_C3,N_C3,N_C3, N_E3,N_E3,N_E3,N_E3, N_C3,N_C3,N_C3,N_C3, N_G3,N_G3,N_G3,N_G3,
};
static const double bass_go_intro[MOTIF_STEPS] = {
    N_E1,N_E1,N_E1,N_E1, N_E1,N_E1,N_E1,N_E1, N_E1,N_E1,N_E1,N_E1, N_E1,N_E1,N_E1,N_E1,
    N_C2,N_C2,N_C2,N_C2, N_C2,N_C2,N_C2,N_C2, N_C2,N_C2,N_C2,N_C2, N_C2,N_C2,N_C2,N_C2,
};
static const unsigned char kick_go_intro[MOTIF_STEPS] = {
    1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
};
static const unsigned char crash_go_intro[MOTIF_STEPS] = { 1,0,0,0 };
static const Pattern kPatternGameOverIntro = {
    guitar_go_intro, bass_go_intro, lead_go_silent, kick_go_intro, snare_go_backbeat, hihat_go_quarter,
    crash_go_intro, true,
};

/* ---- BALLAD_A/B: the same one-note-per-beat idea, fuller (kick on beats
 * 1 and 3, hihat opens to steady 8ths). Em-C for 8 bars, then Am-Em for 8;
 * BALLAD_B adds two single held notes - the brief's own "occasional short,
 * plaintive lead-guitar replies" - each entering on beat 4 and ringing to
 * the end of the bar. ---- */
static const unsigned char kick_go_ballad[MOTIF_STEPS] = {
    1,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0,
    1,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0,
};

static const double guitar_go_balladA[MOTIF_STEPS] = {
    N_E3,N_E3,N_E3,N_E3, N_G3,N_G3,N_G3,N_G3, N_E3,N_E3,N_E3,N_E3, N_B2,N_B2,N_B2,N_B2,
    N_C3,N_C3,N_C3,N_C3, N_E3,N_E3,N_E3,N_E3, N_C3,N_C3,N_C3,N_C3, N_G3,N_G3,N_G3,N_G3,
};
static const double bass_go_balladA[MOTIF_STEPS] = {
    N_E1,N_E1,N_E1,N_E1, N_E1,N_E1,N_E1,N_E1, N_E1,N_E1,N_E1,N_E1, N_E1,N_E1,N_E1,N_E1,
    N_C2,N_C2,N_C2,N_C2, N_C2,N_C2,N_C2,N_C2, N_C2,N_C2,N_C2,N_C2, N_C2,N_C2,N_C2,N_C2,
};
static const Pattern kPatternGameOverBalladA = {
    guitar_go_balladA, bass_go_balladA, lead_go_silent, kick_go_ballad, snare_go_backbeat, hihat_go_8ths,
    crash_go_silent, true,
};

static const double guitar_go_balladB[MOTIF_STEPS] = {
    N_A3,N_A3,N_A3,N_A3, N_C3,N_C3,N_C3,N_C3, N_A3,N_A3,N_A3,N_A3, N_E3,N_E3,N_E3,N_E3,
    N_E3,N_E3,N_E3,N_E3, N_G3,N_G3,N_G3,N_G3, N_E3,N_E3,N_E3,N_E3, N_B2,N_B2,N_B2,N_B2,
};
static const double bass_go_balladB[MOTIF_STEPS] = {
    N_A1,N_A1,N_A1,N_A1, N_A1,N_A1,N_A1,N_A1, N_A1,N_A1,N_A1,N_A1, N_A1,N_A1,N_A1,N_A1,
    N_E1,N_E1,N_E1,N_E1, N_E1,N_E1,N_E1,N_E1, N_E1,N_E1,N_E1,N_E1, N_E1,N_E1,N_E1,N_E1,
};
static const double lead_go_balladB[MOTIF_STEPS] = {
    0,0,0,0, 0,0,0,0, 0,0,0,0, N_E4,N_E4,N_E4,N_E4,
    0,0,0,0, 0,0,0,0, 0,0,0,0, N_G4,N_G4,N_G4,N_G4,
};
static const Pattern kPatternGameOverBalladB = {
    guitar_go_balladB, bass_go_balladB, lead_go_balladB, kick_go_ballad, snare_go_backbeat, hihat_go_8ths,
    crash_go_silent, true,
};

/* ---- TENSION_BUILD: a true bass pedal - one sustained E1 across the
 * entire 2-bar phrase, no beat changes at all - under a guitar that
 * darkens with the tritone (A#2, the "devil's interval"), one chord tone
 * per beat as everywhere else, more of the bar given to the tritone in the
 * second half - "darker harmony... increasingly assertive". Kick now
 * fires on every beat (still exactly on-grid, just fuller), building
 * toward the heavy section; a crash on the last beat launches it. ---- */
static const double guitar_go_tension[MOTIF_STEPS] = {
    N_E2,N_E2,N_E2,N_E2, N_E2,N_E2,N_E2,N_E2, N_AS2,N_AS2,N_AS2,N_AS2, N_E2,N_E2,N_E2,N_E2,
    N_AS2,N_AS2,N_AS2,N_AS2, N_E2,N_E2,N_E2,N_E2, N_AS2,N_AS2,N_AS2,N_AS2, N_E2,N_E2,N_E2,N_E2,
};
static const double bass_go_tension[MOTIF_STEPS] = {
    N_E1,N_E1,N_E1,N_E1, N_E1,N_E1,N_E1,N_E1, N_E1,N_E1,N_E1,N_E1, N_E1,N_E1,N_E1,N_E1,
    N_E1,N_E1,N_E1,N_E1, N_E1,N_E1,N_E1,N_E1, N_E1,N_E1,N_E1,N_E1, N_E1,N_E1,N_E1,N_E1,
};
static const unsigned char kick_go_tension[MOTIF_STEPS] = {
    1,0,0,0, 1,0,0,0, 1,0,0,0, 1,0,0,0,
    1,0,0,0, 1,0,0,0, 1,0,0,0, 1,0,0,0,
};
static const unsigned char crash_go_tension[MOTIF_STEPS] = {
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    0,0,0,0, 0,0,0,0, 0,0,0,0, 1,0,0,0,
};
static const Pattern kPatternGameOverTension = {
    guitar_go_tension, bass_go_tension, lead_go_silent, kick_go_tension, snare_go_backbeat, hihat_go_8ths,
    crash_go_tension, true,
};

/* ---- HEAVY_A/B: the octave-dropped, doubled (guitar+bass play the exact
 * same notes on the exact same beats - true unison, not just "close")
 * riff - one chord tone per beat, stepping down to a neighboring note on
 * beat 4 ("a short stepwise movement downward" per the brief), never
 * syncopated. Kick fires on all 4 beats, locked exactly to where
 * guitar/bass attack; snare keeps its plain backbeat (coinciding with kick
 * on beats 2/4, a deliberate unison hit, not a clash); hihat drops out so
 * the riff alone carries the rhythm; crash opens on beat 1 of each bar. Am
 * doubles all the way down to the shared low A1 for the heaviest, lowest
 * moment in the piece. ---- */
static const double guitar_go_heavyA[MOTIF_STEPS] = {
    N_E2,N_E2,N_E2,N_E2, N_E2,N_E2,N_E2,N_E2, N_E2,N_E2,N_E2,N_E2, N_D2,N_D2,N_D2,N_D2,
    N_C2,N_C2,N_C2,N_C2, N_C2,N_C2,N_C2,N_C2, N_C2,N_C2,N_C2,N_C2, N_B1,N_B1,N_B1,N_B1,
};
static const double bass_go_heavyA[MOTIF_STEPS] = {
    N_E1,N_E1,N_E1,N_E1, N_E1,N_E1,N_E1,N_E1, N_E1,N_E1,N_E1,N_E1, N_D2,N_D2,N_D2,N_D2,
    N_C2,N_C2,N_C2,N_C2, N_C2,N_C2,N_C2,N_C2, N_C2,N_C2,N_C2,N_C2, N_B1,N_B1,N_B1,N_B1,
};
static const double guitar_go_heavyB[MOTIF_STEPS] = {
    N_A1,N_A1,N_A1,N_A1, N_A1,N_A1,N_A1,N_A1, N_A1,N_A1,N_A1,N_A1, N_G1,N_G1,N_G1,N_G1,
    N_E2,N_E2,N_E2,N_E2, N_E2,N_E2,N_E2,N_E2, N_E2,N_E2,N_E2,N_E2, N_D2,N_D2,N_D2,N_D2,
};
static const double bass_go_heavyB[MOTIF_STEPS] = {
    N_A1,N_A1,N_A1,N_A1, N_A1,N_A1,N_A1,N_A1, N_A1,N_A1,N_A1,N_A1, N_G1,N_G1,N_G1,N_G1,
    N_E1,N_E1,N_E1,N_E1, N_E1,N_E1,N_E1,N_E1, N_E1,N_E1,N_E1,N_E1, N_D2,N_D2,N_D2,N_D2,
};
static const unsigned char kick_go_heavy[MOTIF_STEPS] = {
    1,0,0,0, 1,0,0,0, 1,0,0,0, 1,0,0,0,
    1,0,0,0, 1,0,0,0, 1,0,0,0, 1,0,0,0,
};
static const unsigned char crash_go_heavy[MOTIF_STEPS] = {
    1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
};
static const Pattern kPatternGameOverHeavyA = {
    guitar_go_heavyA, bass_go_heavyA, lead_go_silent, kick_go_heavy, snare_go_backbeat, hihat_go_silent,
    crash_go_heavy, false,
};
static const Pattern kPatternGameOverHeavyB = {
    guitar_go_heavyB, bass_go_heavyB, lead_go_silent, kick_go_heavy, snare_go_backbeat, hihat_go_silent,
    crash_go_heavy, false,
};

/* ---- LEAD_BREAK_A/B: byte-for-byte the same HEAVY_A/B riff bed - only
 * the lead differs, entering on beat 3 and ringing through beat 4, still
 * exactly on-grid, climbing register into LEAD_BREAK_B, each note bending
 * up into pitch (see the SONG_MODE_GAME_OVER-gated portamento in
 * audio_callback). ---- */
static const double lead_go_leadbreakA[MOTIF_STEPS] = {
    0,0,0,0, 0,0,0,0, N_G4,N_G4,N_G4,N_G4, N_G4,N_G4,N_G4,N_G4,
    0,0,0,0, 0,0,0,0, N_E4,N_E4,N_E4,N_E4, N_E4,N_E4,N_E4,N_E4,
};
static const double lead_go_leadbreakB[MOTIF_STEPS] = {
    0,0,0,0, 0,0,0,0, N_C5,N_C5,N_C5,N_C5, N_C5,N_C5,N_C5,N_C5,
    0,0,0,0, 0,0,0,0, N_B4,N_B4,N_B4,N_B4, N_B4,N_B4,N_B4,N_B4,
};
static const Pattern kPatternGameOverLeadBreakA = {
    guitar_go_heavyA, bass_go_heavyA, lead_go_leadbreakA, kick_go_heavy, snare_go_backbeat, hihat_go_silent,
    crash_go_heavy, false,
};
static const Pattern kPatternGameOverLeadBreakB = {
    guitar_go_heavyB, bass_go_heavyB, lead_go_leadbreakB, kick_go_heavy, snare_go_backbeat, hihat_go_silent,
    crash_go_heavy, false,
};

/* ---- FINAL_PEAK: the riff stops changing chords entirely and just pounds
 * one sustained low E on every beat under the lead's highest, longest
 * notes (climbing from E5 to G5) - "combine the central heavy riff with
 * longer cymbal crashes and higher lead sustains... end on a held,
 * unresolved minor-power-chord sonority" - before looping back to the
 * quiet intro. ---- */
static const double guitar_go_finalPeak[MOTIF_STEPS] = {
    N_E2,N_E2,N_E2,N_E2, N_E2,N_E2,N_E2,N_E2, N_E2,N_E2,N_E2,N_E2, N_E2,N_E2,N_E2,N_E2,
    N_E2,N_E2,N_E2,N_E2, N_E2,N_E2,N_E2,N_E2, N_E2,N_E2,N_E2,N_E2, N_E2,N_E2,N_E2,N_E2,
};
static const double bass_go_finalPeak[MOTIF_STEPS] = {
    N_E1,N_E1,N_E1,N_E1, N_E1,N_E1,N_E1,N_E1, N_E1,N_E1,N_E1,N_E1, N_E1,N_E1,N_E1,N_E1,
    N_E1,N_E1,N_E1,N_E1, N_E1,N_E1,N_E1,N_E1, N_E1,N_E1,N_E1,N_E1, N_E1,N_E1,N_E1,N_E1,
};
static const double lead_go_finalPeak[MOTIF_STEPS] = {
    0,0,0,0, N_E5,N_E5,N_E5,N_E5, N_E5,N_E5,N_E5,N_E5, N_E5,N_E5,N_E5,N_E5,
    N_E5,N_E5,N_E5,N_E5, N_G5,N_G5,N_G5,N_G5, N_G5,N_G5,N_G5,N_G5, N_G5,N_G5,N_G5,N_G5,
};
static const unsigned char kick_go_finalPeak[MOTIF_STEPS] = {
    1,0,0,0, 1,0,0,0, 1,0,0,0, 1,0,0,0,
    1,0,0,0, 1,0,0,0, 1,0,0,0, 1,0,0,0,
};
static const unsigned char crash_go_finalPeak[MOTIF_STEPS] = {
    1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
};
static const Pattern kPatternGameOverFinalPeak = {
    guitar_go_finalPeak, bass_go_finalPeak, lead_go_finalPeak, kick_go_finalPeak, snare_go_backbeat,
    hihat_go_silent, crash_go_finalPeak, false,
};

/* Arrangement, 9 sections x 8 bars = 72 bars, then loops for as long as the
 * game-over screen stays up: quiet intro -> 16-bar ballad (Em-C, Am-Em) ->
 * 8-bar tension build -> 16-bar heavy riff (Em-C, Am-Em) -> 16-bar lead
 * break over the same riff -> 8-bar final peak. */
static const SongSection kGameOverSong[] = {
    { &kPatternGameOverIntro,        8 },
    { &kPatternGameOverBalladA,      8 },
    { &kPatternGameOverBalladB,      8 },
    { &kPatternGameOverTension,      8 },
    { &kPatternGameOverHeavyA,       8 },
    { &kPatternGameOverHeavyB,       8 },
    { &kPatternGameOverLeadBreakA,   8 },
    { &kPatternGameOverLeadBreakB,   8 },
    { &kPatternGameOverFinalPeak,    8 },
};
#define GAME_OVER_SONG_NUM_SECTIONS (int)(sizeof(kGameOverSong) / sizeof(kGameOverSong[0]))

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

/* Which of the 3 looping arrangements is currently playing - resolved once
 * per callback buffer from the ctx flags (see current_song_mode) rather
 * than threading two separate bools through every call site below, since
 * exactly one of these is ever active at a time (game_over_active wins if
 * somehow both it and boss_active were ever true at once - see
 * AudioPort.update's own doc comment). */
typedef enum SongMode {
    SONG_MODE_NORMAL,
    SONG_MODE_BOSS,
    SONG_MODE_GAME_OVER,
} SongMode;

static SongMode current_song_mode(bool boss_active, bool game_over_active) {
    if (game_over_active) return SONG_MODE_GAME_OVER;
    if (boss_active) return SONG_MODE_BOSS;
    return SONG_MODE_NORMAL;
}

/* Advances the sequencer by one 16th-note step: resolves the current song
 * section/pattern for whichever of the 3 arrangements mode selects, latches
 * the new per-channel target notes, and (unless paused) fires any drum hits
 * scheduled on this step. */
static void advance_step(SdlAudioCtx *ctx, int global_step, bool paused, SongMode mode) {
    int bar = global_step / STEPS_PER_BAR;
    int step_in_bar = global_step % STEPS_PER_BAR;

    const SongSection *song = kSong;
    int num_sections = SONG_NUM_SECTIONS;
    if (mode == SONG_MODE_BOSS) {
        song = kBossSong;
        num_sections = BOSS_SONG_NUM_SECTIONS;
    } else if (mode == SONG_MODE_GAME_OVER) {
        song = kGameOverSong;
        num_sections = GAME_OVER_SONG_NUM_SECTIONS;
    }

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
        ctx->synth_double_phase = 0.0;
        ctx->synth_double_note_time = 0.0;
        ctx->clean_high_root_phase = 0.0;
        ctx->clean_high_fifth_phase = 0.0;
        ctx->clean_note_time = 0.0;
    }
    ctx->guitar_prev_freq = guitar_freq;
    ctx->current_guitar_freq = guitar_freq;
    ctx->guitar_clean = pat->guitar_clean;

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
        /* GAME OVER's own drums sit a bit further back than the main/boss
         * themes' own unchanged levels below, so its constantly-present
         * guitar (see guitar_gain in audio_callback) reads as clearly the
         * loudest thing in the mix rather than getting buried under kick
         * hits - main/boss keep these exact amplitudes since their own
         * mix was already balanced/approved. */
        double kick_amp = 0.55, snare_amp = 0.4, hihat_amp = 0.16, crash_amp = 0.32;
        if (mode == SONG_MODE_GAME_OVER) {
            kick_amp = 0.38;
            snare_amp = 0.28;
            hihat_amp = 0.11;
            crash_amp = 0.22;
        }
        if (pat->kick[motif_step]) trigger_voice(ctx, VOICE_KICK, 150.0, 45.0, 0.11, kick_amp);
        if (pat->snare[motif_step]) trigger_voice(ctx, VOICE_SNARE_NOISE, 0.0, 0.0, 0.09, snare_amp);
        if (pat->hihat[motif_step]) trigger_voice(ctx, VOICE_HIHAT, 0.0, 0.0, 0.045, hihat_amp);
        if (pat->crash[motif_step]) trigger_voice(ctx, VOICE_CRASH, 0.0, 0.0, 0.9, crash_amp);
    }
}

static void audio_callback(void *userdata, Uint8 *stream, int len) {
    SdlAudioCtx *ctx = userdata;
    float *out = (float *)stream;
    int frames = len / (int)sizeof(float);
    double dt = 1.0 / (double)ctx->spec.freq;

    bool paused = ctx->paused;
    float difficulty01 = ctx->difficulty01;
    SongMode mode = current_song_mode(ctx->boss_active, ctx->game_over_active);
    double bpm = mode == SONG_MODE_BOSS ? BOSS_BPM : mode == SONG_MODE_GAME_OVER ? GAME_OVER_BPM : BPM;
    int total_steps = mode == SONG_MODE_GAME_OVER ? GAME_OVER_TOTAL_STEPS : TOTAL_STEPS;
    double step_duration = 60.0 / bpm / 4.0; /* sixteenth notes */
    float energy_gain = 1.0f + 0.15f * difficulty01;

    for (int i = 0; i < frames; i++) {
        int global_step = (int)fmod(ctx->music_time / step_duration, (double)total_steps);
        if (global_step != ctx->last_step_index) {
            ctx->last_step_index = global_step;
            advance_step(ctx, global_step, paused, mode);
        }

        float music_sample = 0.0f;
        if (!paused) {
            double time_in_step = fmod(ctx->music_time, step_duration);
            double env_tight = step_env(time_in_step, step_duration, 0.004);
            double env_lead = step_env(time_in_step, step_duration, 0.008);
            double env_soft = step_env(time_in_step, step_duration, 0.015);

            if (ctx->current_guitar_freq > 0.0) {
                /* The guitar is the main instrument throughout GAME OVER -
                 * it's the only voice active in literally every section,
                 * clean or distorted - so it gets its own louder mix level
                 * there instead of the 0.20 the main/boss themes still use
                 * unchanged (their own guitar sits under a busier full-band
                 * mix that was already balanced/approved). */
                double guitar_gain = mode == SONG_MODE_GAME_OVER ? 0.32 : 0.20;
                if (ctx->guitar_clean) {
                    /* Clean, chorused electric tone (GAME OVER's own quiet
                     * sections only) - triangle instead of saw (no bite/
                     * buzz to distort), no tanh saturation at all, and
                     * guitar_fifth_phase repurposed from "a musical fifth
                     * a la the distorted voice below" into a true chorus
                     * voice: the exact same pitch, detuned by a slow
                     * +-0.6% sine LFO (kChorusRate) instead of a fixed
                     * ratio, for the classic chorus-pedal shimmer/width.
                     * Pitched one octave above the written note - inverted
                     * from the doubling layer below, which now sits an
                     * octave under it instead of above. Its own note-level
                     * envelope (one attack per actual note change, then
                     * flat sustain) rather than env_soft, same
                     * "GAME OVER writes one note per beat, so don't
                     * re-trigger 4 times a beat" reasoning as
                     * synth_double_note_time's own doc comment - shared
                     * with the second copy below since both change note at
                     * the same instant. */
                    const double kChorusRate = 0.6;
                    const double kChorusDepth = 0.006;
                    const double kCleanAttack = 0.03;
                    double detune = 1.0 + kChorusDepth * sin(2.0 * M_PI * kChorusRate * ctx->music_time);
                    double clean_note_env =
                        ctx->clean_note_time < kCleanAttack ? ctx->clean_note_time / kCleanAttack : 1.0;
                    double clean_freq = ctx->current_guitar_freq * 2.0;
                    double voice1 = triangle_wave(ctx->guitar_root_phase);
                    double voice2 = triangle_wave(ctx->guitar_fifth_phase);
                    /* TEMPORARY PREVIEW MUTE (user asked to hear the mix
                     * without the middle octave) - phases still advance
                     * below so nothing loses sync if this gets restored. */
                    music_sample += (float)((voice1 * 0.5 + voice2 * 0.5) * guitar_gain * clean_note_env * 0.0);
                    ctx->guitar_root_phase += clean_freq * dt;
                    if (ctx->guitar_root_phase >= 1.0) ctx->guitar_root_phase -= 1.0;
                    ctx->guitar_fifth_phase += clean_freq * detune * dt;
                    if (ctx->guitar_fifth_phase >= 1.0) ctx->guitar_fifth_phase -= 1.0;

                    /* A second copy of the clean/chorus voice above, one
                     * octave higher still (2 octaves above the written
                     * note) - same triangle-plus-detuned-chorus timbre and
                     * mix level, just transposed, so the 3 voices together
                     * cover 3 consecutive octaves, one voice each: this one
                     * (highest), the clean voice above (middle), and the
                     * synth double below (lowest, at the written pitch). */
                    double clean_high_freq = clean_freq * 2.0;
                    double voice3 = triangle_wave(ctx->clean_high_root_phase);
                    double voice4 = triangle_wave(ctx->clean_high_fifth_phase);
                    music_sample += (float)((voice3 * 0.5 + voice4 * 0.5) * guitar_gain * clean_note_env);
                    ctx->clean_high_root_phase += clean_high_freq * dt;
                    if (ctx->clean_high_root_phase >= 1.0) ctx->clean_high_root_phase -= 1.0;
                    ctx->clean_high_fifth_phase += clean_high_freq * detune * dt;
                    if (ctx->clean_high_fifth_phase >= 1.0) ctx->clean_high_fifth_phase -= 1.0;
                    ctx->clean_note_time += dt;

                    /* Doubling layer: the written note at its own plain
                     * pitch (one octave *below* the clean voice above,
                     * inverted from the original higher-double layout),
                     * through a slightly-driven synth voice (a saw core -
                     * brighter/more "synth" than the clean tone's triangle -
                     * through a much gentler tanh than the heavy guitar's
                     * own 2.6 drive, so it reads as "a little edge," not
                     * distorted metal) - sits under the main clean tone
                     * rather than competing with it. Deliberately its own
                     * note-level envelope (one attack per actual note
                     * change, then flat sustain) instead of env_soft, which
                     * re-triggers every 16th-note step regardless of
                     * whether the pitch changed - with GAME OVER's own
                     * one-note-per-beat writing, that would stroke 4 times
                     * a beat instead of singing as one continuous tone. */
                    const double kSynthDoubleAttack = 0.03;
                    double synth_note_env =
                        ctx->synth_double_note_time < kSynthDoubleAttack
                            ? ctx->synth_double_note_time / kSynthDoubleAttack
                            : 1.0;
                    double synth_freq = ctx->current_guitar_freq;
                    double synth_raw = saw_wave(ctx->synth_double_phase);
                    double synth_driven = tanh(synth_raw * 1.3);
                    music_sample += (float)(synth_driven * (guitar_gain * 0.6) * synth_note_env);
                    ctx->synth_double_phase += synth_freq * dt;
                    if (ctx->synth_double_phase >= 1.0) ctx->synth_double_phase -= 1.0;
                    ctx->synth_double_note_time += dt;
                } else {
                    double root = saw_wave(ctx->guitar_root_phase);
                    double fifth = saw_wave(ctx->guitar_fifth_phase);
                    double mixed = root * 0.62 + fifth * 0.38;
                    double driven = tanh(mixed * 2.6);
                    music_sample += (float)(driven * guitar_gain * env_tight);
                    ctx->guitar_root_phase += ctx->current_guitar_freq * dt;
                    if (ctx->guitar_root_phase >= 1.0) ctx->guitar_root_phase -= 1.0;
                    ctx->guitar_fifth_phase += ctx->current_guitar_freq * 1.5 * dt;
                    if (ctx->guitar_fifth_phase >= 1.0) ctx->guitar_fifth_phase -= 1.0;
                }
            }

            if (ctx->current_bass_freq > 0.0) {
                music_sample += (float)(triangle_wave(ctx->bass_phase) * 0.17 * env_tight);
                ctx->bass_phase += ctx->current_bass_freq * dt;
                if (ctx->bass_phase >= 1.0) ctx->bass_phase -= 1.0;
            }

            if (ctx->current_lead_freq > 0.0) {
                /* GAME OVER's own "wounded, vocal" lead per its own brief -
                 * bends up into every new note from a whole step below over
                 * LEAD_BEND_TIME (smoothstep-eased, so it settles rather
                 * than snapping), then the existing subtle vibrato below
                 * takes over once the bend has resolved. Scoped to
                 * SONG_MODE_GAME_OVER only so the main/boss themes' own
                 * lead voice (fast pentatonic runs that don't want a slow
                 * bend smearing their attack) stay byte-for-byte
                 * unchanged. */
                double played_freq = ctx->current_lead_freq;
                if (mode == SONG_MODE_GAME_OVER) {
                    const double kLeadBendTime = 0.15;
                    double bend_t = ctx->lead_note_time / kLeadBendTime;
                    if (bend_t < 1.0) {
                        double start_freq = ctx->current_lead_freq * pow(2.0, -2.0 / 12.0);
                        double eased = bend_t * bend_t * (3.0 - 2.0 * bend_t);
                        played_freq = start_freq + (ctx->current_lead_freq - start_freq) * eased;
                    }
                }
                double vibrato = 1.0 + 0.004 * sin(2.0 * M_PI * 6.0 * ctx->lead_note_time);
                double driven = tanh(saw_wave(ctx->lead_phase) * 1.6);
                music_sample += (float)(driven * 0.15 * env_lead);
                ctx->lead_phase += played_freq * vibrato * dt;
                if (ctx->lead_phase >= 1.0) ctx->lead_phase -= 1.0;
                ctx->lead_note_time += dt;
            }

            if (ctx->current_keys_freq > 0.0) {
                double a = sin(ctx->keys_phase_a * 2.0 * M_PI);
                double b = sin(ctx->keys_phase_b * 2.0 * M_PI);
                double keys_gain = 0.09;
                /* GAME OVER's own "melancholic synth" touch: a slow
                 * amplitude swell (tremolo) on top of the existing 2-
                 * oscillator chorus shimmer, so the pad breathes in and
                 * out under the guitar rather than sitting at one static
                 * level - scoped to this song only, same reasoning as the
                 * lead's own bend above. */
                if (mode == SONG_MODE_GAME_OVER) {
                    const double kSwellRate = 0.18;
                    keys_gain *= 0.8 + 0.35 * (0.5 + 0.5 * sin(2.0 * M_PI * kSwellRate * ctx->music_time));
                }
                music_sample += (float)((a * 0.5 + b * 0.5) * keys_gain * env_soft);
                ctx->keys_phase_a += ctx->current_keys_freq * dt;
                if (ctx->keys_phase_a >= 1.0) ctx->keys_phase_a -= 1.0;
                ctx->keys_phase_b += ctx->current_keys_freq * 1.003 * dt;
                if (ctx->keys_phase_b >= 1.0) ctx->keys_phase_b -= 1.0;
            }

            music_sample *= energy_gain;
            /* GAME OVER's guitar is on in literally every section (unlike
             * main/boss, where it often rests), so simply boosting its own
             * gain above would make the whole track read louder overall
             * than the other two even after the drum trim above - this
             * mode-scoped trim brings its overall level back down to
             * match them, verified by comparing RMS across all 3 songs
             * (see the mix-check capture this was tuned against). */
            if (mode == SONG_MODE_GAME_OVER) music_sample *= (float)kGameOverMixTrim;
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

static void sdl_audio_update(void *self, float dt, bool paused, float difficulty01, bool boss_active,
                              bool game_over_active) {
    (void)dt;
    SdlAudioCtx *ctx = self;
    SDL_LockAudioDevice(ctx->device);
    ctx->paused = paused;
    ctx->difficulty01 = difficulty01;
    SongMode old_mode = current_song_mode(ctx->boss_active, ctx->game_over_active);
    SongMode new_mode = current_song_mode(boss_active, game_over_active);
    ctx->boss_active = boss_active;
    ctx->game_over_active = game_over_active;
    if (new_mode != old_mode) {
        /* Cut cleanly to bar 1 of the new arrangement instead of carrying
         * over a step index that means something different at the other
         * song's BPM/section layout. */
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
