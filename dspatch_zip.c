/* dspatch_zip - patch zipline movement parameters in dsplayerparamcomponent.core
 *
 * The resource in question is a HumanoidZiplineStateResource block found inside
 * the player parameter component. In the vanilla file it appears twice (player
 * and NPC template) and starts with:
 *
 *   MaxZiplineSpeed   10.0
 *   MaxZiplineTurnSpeed 180.0
 *   ZiplineAcceleration 10.0
 *   ZiplineBrakeFriction 10.0
 *
 * Those four floats are used as the anchor; the file is only patched when they
 * match exactly.
 *
 * Usage: dspatch_zip <in.core> <out.core> <speed-multiplier>
 *        dspatch_zip <in.core> --verify
 * Exit: 0 ok, 2 not vanilla, 3 malformed
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ANCHOR_OFF 0x1a8
#define NEEDED 0x1b8

static unsigned char* read_file(const char* path, long* out_size) {
    FILE* f = fopen(path, "rb");
    long n;
    unsigned char* buf;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    buf = (unsigned char*)malloc((size_t)n);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); fclose(f); return NULL; }
    fclose(f); *out_size = n; return buf;
}

static int is_anchor(const unsigned char* d) {
    float a, b, c, e;
    memcpy(&a, d + ANCHOR_OFF + 0, 4);
    memcpy(&b, d + ANCHOR_OFF + 4, 4);
    memcpy(&c, d + ANCHOR_OFF + 8, 4);
    memcpy(&e, d + ANCHOR_OFF + 12, 4);
    return a == 10.0f && b == 180.0f && c == 10.0f && e == 10.0f;
}

/* walk the core: repeated {u64 type, u32 size, data} */
static int find_blocks(unsigned char* data, long size, int* out_offsets, int max) {
    long pos = 0;
    int n = 0;
    while (pos + 12 <= size) {
        unsigned int sz;
        memcpy(&sz, data + pos + 8, 4);
        if (sz == 0 || pos + 12 + (long)sz > size) return -1;
        if (sz >= NEEDED && is_anchor(data + pos + 12)) {
            if (n < max) out_offsets[n] = (int)(pos + 12 + ANCHOR_OFF);
            n++;
        }
        pos += 12 + (long)sz;
    }
    return n;
}

int main(int argc, char** argv) {
    long size = 0;
    unsigned char* data;
    int offs[64];
    int n, i;
    float mul;

    if (argc < 3) {
        printf("usage: dspatch_zip <in.core> <out.core> <speed-multiplier>\n");
        printf("       dspatch_zip <in.core> --verify\n");
        return 1;
    }
    data = read_file(argv[1], &size);
    if (!data) { printf("cannot read %s\n", argv[1]); return 3; }

    n = find_blocks(data, size, offs, 64);
    if (n <= 0) {
        printf("no zipline parameter block found\nNOT_VANILLA\n");
        return 2;
    }

    if (strcmp(argv[2], "--verify") == 0) {
        printf("VANILLA_OK zipline_blocks=%d size=%ld\n", n, size);
        return 0;
    }

    mul = (float)atof(argv[3]);
    if (!(mul > 0.0f)) { printf("speed multiplier must be > 0\n"); return 4; }
    if (mul != 1.0f) {
        for (i = 0; i < n && i < 64; i++) {
            float* p = (float*)(data + offs[i]);
            p[0] *= mul;   /* MaxZiplineSpeed */
            p[1] *= mul;   /* MaxZiplineTurnSpeed */
            p[2] *= mul;   /* ZiplineAcceleration */
        }
    }

    {
        FILE* f = fopen(argv[2], "wb");
        if (!f) { printf("cannot write %s\n", argv[2]); return 3; }
        if (fwrite(data, 1, (size_t)size, f) != (size_t)size) { fclose(f); return 3; }
        fclose(f);
    }
    printf("PATCHED_ZIPLINE blocks=%d multiplier=x%.2f\n", n, mul);
    return 0;
}
