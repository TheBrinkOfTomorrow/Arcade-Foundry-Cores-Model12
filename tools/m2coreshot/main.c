/* The game core through its C interface only (include/afcore.h), as a front
 * end uses it: load the ROM set, run FRAMES frames with no input, print the
 * last screen's hash (the same FNV-1a as m2run's "last screen hash") and how
 * much sound came out. C, so the header stays C.
 *
 *   m2coreshot ROMSET.zip FRAMES [--stereo OUT.ppm] [--head OUT.ppm] [--scale M] [--hashes OUT.txt]
 *              [--play] [--save-state-at N --state FILE] [--load-state FILE]
 *
 * --hashes: every frame's screen hash, one a line (to compare two builds
 * frame by frame), with the frame's number and its sound's hash; the run's
 * time is printed too.
 *
 * --play: a coin, start, then the controls moving (accelerator down, the
 * wheel, guns and stick swinging, buttons pressed in turn), the same for
 * every game, from the frame number, so a run from a save state gets the
 * same inputs as the run that saved it.
 *
 * --save-state-at N --state FILE: after frame N (counting from 1), write a
 * save state (afcore save_state) to FILE and carry on. --load-state FILE:
 * load it before the first frame; frames then count on from the state's
 * frame (tests/save_state_check.sh compares the two runs).
 *
 * --stereo: the last screen's two eyes (render_eyes, AFCORE_STEREO_PAIR)
 * side by side, left eye on the left. --head: both eyes of a headset-like
 * view (AFCORE_STEREO_HEAD, 512 x 512 per eye, 90 degrees, the head turned
 * 20 degrees left and the eyes 63 mm apart). --scale: metres per game unit.
 */
#include "afcore.h"

#include <inttypes.h>
#include <stddef.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define M2_ENTRY_(set) afcore_api_##set
#define M2_ENTRY(set) M2_ENTRY_(set)
// The set's entry point, declared here too: afcore.h (Common) lists the sets
// it knew of when it was last updated, and vformula came after
const afcore_api *M2_ENTRY(M2_ROMSET_ID)(void);

/* The inputs of --play at frame f (from 1). */
static void play_input(long f, afcore_input *in) {
    memset(in, 0, sizeof *in);
    in->size = sizeof *in;
    if (f >= 600 && f < 606) in->buttons |= AFCORE_COIN;
    if ((f >= 700 && f < 706) || (f >= 900 && f < 906) || (f >= 1100 && f < 1106)) in->buttons |= AFCORE_START;
    if (f < 1200) return;
    const float t = (float)f / 60.0f;
    in->accel = 1.0f;
    in->steer = 0.6f * sinf(t * 0.9f);
    in->stick_y = 0.5f * sinf(t * 0.7f);
    in->gun_x[0] = 0.5f + 0.4f * sinf(t * 1.3f), in->gun_y[0] = 0.5f + 0.3f * cosf(t * 1.1f);
    in->gun_x[1] = 0.5f + 0.4f * cosf(t * 1.2f), in->gun_y[1] = 0.5f + 0.3f * sinf(t * 0.8f);
    if ((f / 20) % 3 == 0) in->buttons |= AFCORE_TRIGGER1 | AFCORE_PUNCH;
    if ((f / 30) % 4 == 1) in->buttons |= AFCORE_KICK | AFCORE_TRIGGER2;
    if ((f / 45) % 5 == 2) in->buttons |= AFCORE_GUARD | AFCORE_RIGHT;
    in->buttons2 = in->buttons & (AFCORE_PUNCH | AFCORE_KICK);
}

static uint64_t fnv(uint64_t h, const void *data, size_t n) {
    const unsigned char *b = (const unsigned char *)data;
    for (size_t i = 0; i < n; ++i) {
        h ^= b[i];
        h *= 0x100000001b3ULL;
    }
    return h;
}

static unsigned char *read_all(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    size_t cap = 1 << 20, n = 0, got;
    unsigned char *buf = malloc(cap);
    while (buf && (got = fread(buf + n, 1, cap - n, f)) > 0)
        if ((n += got) == cap) buf = realloc(buf, cap *= 2);
    fclose(f);
    *size = n;
    return buf;
}

static uint64_t screen_hash(const afcore_api *api, afcore *core, int *w, int *h) {
    const uint32_t *px = api->screen(core, w, h);
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (long i = 0; i < (long)*w * *h; ++i)
        for (int b = 0; b < 4; ++b) {
            hash ^= (px[i] >> (8 * b)) & 0xff;
            hash *= 0x100000001b3ULL;
        }
    return hash;
}

/* Two images side by side as a binary PPM. */
static int write_pair(const char *path, const uint32_t *l, const uint32_t *r, int w, int h) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    fprintf(f, "P6\n%d %d\n255\n", 2 * w, h);
    for (int y = 0; y < h; ++y)
        for (int e = 0; e < 2; ++e)
            for (int x = 0; x < w; ++x) {
                const uint32_t c = (e ? r : l)[(size_t)y * w + x];
                const unsigned char rgb[3] = {(unsigned char)(c >> 16), (unsigned char)(c >> 8), (unsigned char)c};
                fwrite(rgb, 1, 3, f);
            }
    return fclose(f);
}

static int write_eyes(const afcore_api *api, afcore *core, const afcore_stereo *st, const char *path) {
    if (!api->render_eyes) {
        fprintf(stderr, "m2coreshot: this core has no stereo\n");
        return 1;
    }
    if (!api->render_eyes(core, st)) {
        fprintf(stderr, "m2coreshot: no 3D scene on the last screen\n");
        return 1;
    }
    int w, h, w2, h2;
    const uint32_t *l = api->eye_screen(core, 0, &w, &h), *r = api->eye_screen(core, 1, &w2, &h2);
    if (!l || !r || w != w2 || h != h2 || write_pair(path, l, r, w, h)) {
        fprintf(stderr, "m2coreshot: could not write %s\n", path);
        return 1;
    }
    printf("  %s eyes: %dx%d each, %s\n", st->mode == AFCORE_STEREO_HEAD ? "head" : "pair", w, h, path);
    return 0;
}

int main(int argc, char **argv) {
    const char *stereo = NULL, *head = NULL;
    float scale = 0;
    const char *hashes = NULL;
    const char *state_path = NULL, *load_path = NULL;
    long save_at = -1;
    int play = 0;
    int args = 0;
    char *pos[2];
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--stereo") && i + 1 < argc) stereo = argv[++i];
        else if (!strcmp(argv[i], "--head") && i + 1 < argc) head = argv[++i];
        else if (!strcmp(argv[i], "--scale") && i + 1 < argc) scale = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--hashes") && i + 1 < argc) hashes = argv[++i];
        else if (!strcmp(argv[i], "--play")) play = 1;
        else if (!strcmp(argv[i], "--save-state-at") && i + 1 < argc) save_at = atol(argv[++i]);
        else if (!strcmp(argv[i], "--state") && i + 1 < argc) state_path = argv[++i];
        else if (!strcmp(argv[i], "--load-state") && i + 1 < argc) load_path = argv[++i];
        else if (args < 2) pos[args++] = argv[i];
        else args = 3;
    }
    if (args != 2) {
        fprintf(stderr, "usage: m2coreshot ROMSET.zip FRAMES [--stereo OUT.ppm] [--head OUT.ppm] [--scale M] [--hashes OUT.txt]\n"
                        "                  [--play] [--save-state-at N --state FILE] [--load-state FILE]\n");
        return 2;
    }
    argv[1] = pos[0], argv[2] = pos[1];
    const afcore_api *api = M2_ENTRY(M2_ROMSET_ID)();
    if (AFCORE_VERSION_MAJOR(api->version) != AFCORE_API_MAJOR) {
        fprintf(stderr, "m2coreshot: interface %u.%u.%u, expected %d.x\n", AFCORE_VERSION_MAJOR(api->version),
                AFCORE_VERSION_MINOR(api->version), AFCORE_VERSION_PATCH(api->version), AFCORE_API_MAJOR);
        return 1;
    }
    char err[512];
    afcore *core = api->create(argv[1], NULL, err, sizeof err);
    if (!core) {
        fprintf(stderr, "m2coreshot: %s\n", err);
        return 1;
    }
    const long frames = atol(argv[2]);
    const int states = api->size >= offsetof(afcore_api, load_state) + sizeof(void *) && api->load_state;
    if ((save_at >= 0 || load_path) && !states) {
        fprintf(stderr, "m2coreshot: this core has no save states\n");
        return 1;
    }
    if (save_at >= 0 && !state_path) {
        fprintf(stderr, "m2coreshot: --save-state-at needs --state FILE\n");
        return 2;
    }
    long first = 0; /* frames before this run's first: the loaded state's */
    if (load_path) {
        size_t n = 0;
        unsigned char *st = read_all(load_path, &n);
        if (!st || n < 20) {
            fprintf(stderr, "m2coreshot: can't read %s\n", load_path);
            return 1;
        }
        uint64_t frame = 0;
        for (int b = 7; b >= 0; --b) frame = frame << 8 | st[12 + b]; /* afcommon/state.h: u64 frame at byte 12 */
        if (api->load_state(core, st, n, err, sizeof err) != 0) {
            fprintf(stderr, "m2coreshot: %s\n", err);
            return 1;
        }
        free(st);
        first = (long)frame;
        printf("  loaded %s: frame %ld\n", load_path, first);
    }
    afcore_stereo hst;
    memset(&hst, 0, sizeof hst);
    if (head) {
        afcore_stereo st;
        memset(&st, 0, sizeof st);
        st.size = sizeof st;
        st.mode = AFCORE_STEREO_HEAD;
        st.world_scale = scale;
        const float yaw = 20.0f * 3.14159265f / 180.0f, c = cosf(yaw), s = sinf(yaw);
        for (int e = 0; e < 2; ++e) {
            /* camera to eye: the head turned left by yaw, this eye 31.5 mm to its side */
            const float ex = (e ? 0.0315f : -0.0315f);
            float *m = st.eye[e].view;
            memset(m, 0, 16 * sizeof(float));
            m[0] = c, m[2] = -s, m[5] = 1, m[8] = s, m[10] = c, m[15] = 1;
            /* translation: -(R * eye position), the eye position in camera space (ex along the turned x axis) */
            m[12] = -ex;
            st.eye[e].tan_left = -1, st.eye[e].tan_right = 1, st.eye[e].tan_down = -1, st.eye[e].tan_up = 1;
            st.eye[e].width = 512, st.eye[e].height = 512;
        }
        hst = st;
    }
    afcore_stereo pst;
    memset(&pst, 0, sizeof pst);
    pst.size = sizeof pst;
    pst.mode = AFCORE_STEREO_PAIR;
    pst.world_scale = scale;
    static float buf[2 * 4096];
    size_t sound[3] = {0, 0, 0};
    afcore_input in = {0};
    in.size = sizeof in;
    FILE *hf = hashes ? fopen(hashes, "w") : NULL;
    if (hashes && !hf) {
        fprintf(stderr, "m2coreshot: can't write %s\n", hashes);
        return 1;
    }
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (long f = 0; f < frames; ++f) {
        if (play) play_input(first + f + 1, &in);
        api->run_frame(core, &in);
        uint64_t sound_hash = 0xcbf29ce484222325ULL;
        for (int s = 0; s < 3; ++s) {
            size_t n;
            while ((n = api->audio_read(core, s, buf, 4096)) > 0) {
                sound[s] += n;
                sound_hash = fnv(sound_hash, buf, n * 2 * sizeof(float));
            }
        }
        if (hf) {
            int fw, fh;
            fprintf(hf, "%ld %016" PRIx64 " %016" PRIx64 "\n", first + f + 1, screen_hash(api, core, &fw, &fh), sound_hash);
        }
        if (first + f + 1 == save_at) {
            const size_t need = api->state_size(core);
            unsigned char *st = need ? malloc(need) : NULL;
            const size_t n = st ? api->save_state(core, st, need, err, sizeof err) : 0;
            FILE *sf = n ? fopen(state_path, "wb") : NULL;
            if (!sf || fwrite(st, 1, n, sf) != n || fclose(sf) != 0) {
                fprintf(stderr, "m2coreshot: state not saved: %s\n", n ? state_path : err);
                return 1;
            }
            free(st);
            printf("  saved %s: frame %ld, %zu bytes\n", state_path, save_at, n);
        }
        /* head-tracked, as a headset front end would: the core opens its clip planes */
        if (head && api->render_eyes && f >= frames - 60) api->render_eyes(core, &hst);
        /* as a front end showing 3D would: a core may keep each frame's 3D only while asked */
        if (stereo && api->render_eyes && f >= frames - 60) api->render_eyes(core, &pst);
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    if (hf) fclose(hf);
    int w, h;
    const uint64_t hash = screen_hash(api, core, &w, &h);
    printf("%s (%s): %ld frames, %dx%d, last screen hash %016" PRIx64 "\n", api->title, api->romset, frames, w, h, hash);
    printf("  sound: %zu frames at %.0f Hz, %zu at %.0f Hz", sound[0], api->audio_rate(core, 0), sound[1], api->audio_rate(core, 1));
    if (api->audio_rate(core, 2) > 0) printf(", %zu at %.0f Hz", sound[2], api->audio_rate(core, 2));
    printf("\n");
    printf("  time: %.2f s, %.1f frames a second\n", (double)(t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9,
           frames / ((double)(t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9));
    int status = 0;
    if (stereo) status |= write_eyes(api, core, &pst, stereo);
    if (head) status |= write_eyes(api, core, &hst, head);
    api->destroy(core);
    return status;
}
