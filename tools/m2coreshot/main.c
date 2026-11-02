/* The game core through its C interface only (include/afcore.h), as a front
 * end uses it: load the ROM set, run FRAMES frames with no input, print the
 * last screen's hash (the same FNV-1a as m2run's "last screen hash") and how
 * much sound came out. C, so the header stays C.
 *
 *   m2coreshot ROMSET.zip FRAMES [--stereo OUT.ppm] [--head OUT.ppm] [--scale M] [--hashes OUT.txt]
 *
 * --hashes: every frame's screen hash, one a line (to compare two builds
 * frame by frame); the run's time is printed too.
 *
 * --stereo: the last screen's two eyes (render_eyes, AFCORE_STEREO_PAIR)
 * side by side, left eye on the left. --head: both eyes of a headset-like
 * view (AFCORE_STEREO_HEAD, 512 x 512 per eye, 90 degrees, the head turned
 * 20 degrees left and the eyes 63 mm apart). --scale: metres per game unit.
 */
#include "afcore.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define M2_ENTRY_(set) afcore_api_##set
#define M2_ENTRY(set) M2_ENTRY_(set)

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
    int args = 0;
    char *pos[2];
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--stereo") && i + 1 < argc) stereo = argv[++i];
        else if (!strcmp(argv[i], "--head") && i + 1 < argc) head = argv[++i];
        else if (!strcmp(argv[i], "--scale") && i + 1 < argc) scale = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--hashes") && i + 1 < argc) hashes = argv[++i];
        else if (args < 2) pos[args++] = argv[i];
        else args = 3;
    }
    if (args != 2) {
        fprintf(stderr, "usage: m2coreshot ROMSET.zip FRAMES [--stereo OUT.ppm] [--head OUT.ppm] [--scale M] [--hashes OUT.txt]\n");
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
        api->run_frame(core, &in);
        if (hf) {
            int fw, fh;
            fprintf(hf, "%016" PRIx64 "\n", screen_hash(api, core, &fw, &fh));
        }
        /* head-tracked, as a headset front end would: the core opens its clip planes */
        if (head && api->render_eyes && f >= frames - 60) api->render_eyes(core, &hst);
        /* as a front end showing 3D would: a core may keep each frame's 3D only while asked */
        if (stereo && api->render_eyes && f >= frames - 60) api->render_eyes(core, &pst);
        for (int s = 0; s < 3; ++s) {
            size_t n;
            while ((n = api->audio_read(core, s, buf, 4096)) > 0) sound[s] += n;
        }
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
