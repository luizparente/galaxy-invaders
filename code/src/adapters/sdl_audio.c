#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>

#include "adapters/sdl_audio.h"

#define SAMPLE_RATE 44100
#define NUM_VOICES 12
#define NUM_STEPS 16

typedef enum VoiceKind {
    VOICE_SINE_SWEEP,
    VOICE_SQUARE_BEEP,
    VOICE_KICK,
    VOICE_SNARE_NOISE,
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

typedef struct SdlAudioCtx {
    SDL_AudioDeviceID device;
    SDL_AudioSpec spec;

    /* Shared with the game-update thread; guarded by SDL_Lock/UnlockAudioDevice. */
    bool paused;
    float difficulty01;

    /* Audio-thread-owned sequencer state. */
    double music_time;
    int last_step_index;
    double lead_phase;
    double bass_phase;
    unsigned int noise_rng;

    Voice voices[NUM_VOICES];
} SdlAudioCtx;

/* A minor pentatonic-flavored 16-step (two-bar) riff. 0 = rest. */
static const double kLeadNotes[NUM_STEPS] = {
    440.00, 0.0, 523.25, 587.33,
    659.25, 587.33, 523.25, 0.0,
    440.00, 0.0, 392.00, 440.00,
    523.25, 0.0, 440.00, 0.0,
};
static const double kBassNotes[2] = {220.00, 196.00};

static double square_wave(double phase) {
    return phase < 0.5 ? 1.0 : -1.0;
}

static double triangle_wave(double phase) {
    return 4.0 * fabs(phase - floor(phase + 0.5)) - 1.0;
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
    }

    v->t += dt;
    if (v->t >= v->duration) v->active = false;
    return sample * (float)v->amplitude;
}

static void audio_callback(void *userdata, Uint8 *stream, int len) {
    SdlAudioCtx *ctx = userdata;
    float *out = (float *)stream;
    int frames = len / (int)sizeof(float);
    double dt = 1.0 / (double)ctx->spec.freq;

    bool paused = ctx->paused;
    float difficulty01 = ctx->difficulty01;
    double bpm = 100.0 + (double)difficulty01 * 40.0;
    double step_duration = 60.0 / bpm / 2.0; /* eighth notes */

    for (int i = 0; i < frames; i++) {
        int step_index = (int)fmod(ctx->music_time / step_duration, (double)NUM_STEPS);
        if (step_index != ctx->last_step_index) {
            ctx->last_step_index = step_index;
            ctx->lead_phase = 0.0;
            if (step_index % 8 == 0) ctx->bass_phase = 0.0;
            if (!paused) {
                if (step_index == 0 || step_index == 4) {
                    trigger_voice(ctx, VOICE_KICK, 150.0, 40.0, 0.09, 0.5);
                }
                if (step_index == 2 || step_index == 6) {
                    trigger_voice(ctx, VOICE_SNARE_NOISE, 0.0, 0.0, 0.07, 0.35);
                }
            }
        }

        float music_sample = 0.0f;
        if (!paused) {
            double lead_freq = kLeadNotes[step_index];
            double time_in_step = fmod(ctx->music_time, step_duration);
            double step_env = 1.0;
            double edge = 0.005;
            if (time_in_step < edge) step_env = time_in_step / edge;
            else if (time_in_step > step_duration - edge) step_env = (step_duration - time_in_step) / edge;

            if (lead_freq > 0.0) {
                music_sample += (float)(square_wave(ctx->lead_phase) * 0.16 * step_env);
                ctx->lead_phase += lead_freq * dt;
                if (ctx->lead_phase >= 1.0) ctx->lead_phase -= 1.0;
            }

            double bass_freq = kBassNotes[(step_index / 8) % 2];
            music_sample += (float)(triangle_wave(ctx->bass_phase) * 0.14);
            ctx->bass_phase += bass_freq * dt;
            if (ctx->bass_phase >= 1.0) ctx->bass_phase -= 1.0;
        }

        float voice_sample = 0.0f;
        for (int v = 0; v < NUM_VOICES; v++) {
            voice_sample += render_voice(&ctx->voices[v], dt);
        }

        float sample = music_sample + voice_sample * 0.45f;
        if (sample > 1.0f) sample = 1.0f;
        if (sample < -1.0f) sample = -1.0f;
        out[i] = sample;

        ctx->music_time += dt;
    }
}

static void sdl_audio_update(void *self, float dt, bool paused, float difficulty01) {
    (void)dt;
    SdlAudioCtx *ctx = self;
    SDL_LockAudioDevice(ctx->device);
    ctx->paused = paused;
    ctx->difficulty01 = difficulty01;
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
