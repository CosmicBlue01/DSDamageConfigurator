/* dspatch_star - scale facility star thresholds in ds/household/household.core
 *
 * DSHouseholdInfoResource records (type 0x67666b4bd4016a8c) hold:
 *
 *   0x14 InitialFriendshipLevel
 *   0x18 LevelTH1  (likes needed for star 1)
 *   0x1c LevelTH2
 *   0x20 LevelTH3
 *   0x24 LevelTH4
 *   0x28 LevelTH5
 *   0x2c LevelTHDefault
 *
 * Values below 1.0 make stars faster, above 1.0 slower.
 *
 * Usage: dspatch_star <in.core> <out.core> <multiplier>
 *        dspatch_star <in.core> --verify
 * Exit: 0 ok, 2 not vanilla, 3 malformed
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HOUSEHOLD_HASH 0x67666b4bd4016a8cULL
#define MIN_TH 0x2c

static unsigned char* read_file(const char* path, long* out_size) {
    FILE* f = fopen(path, "rb");
    long n;
    unsigned char* buf;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    buf = (unsigned char*)malloc((size_t)n + 16);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); fclose(f); return NULL; }
    fclose(f); *out_size = n; return buf;
}

static int i32(const unsigned char* p) { int v; memcpy(&v, p, 4); return v; }
static void w32(unsigned char* p, int v) { memcpy(p, &v, 4); }

static int valid_th(const unsigned char* d) {
    int k, prev = 0;
    for (k = 0; k < 5; k++) {
        int v = i32(d + 0x18 + 4 * k);
        if (v <= 0 || v > 10000000) return 0;
        if (k && v < prev) return 0;
        prev = v;
    }
    return 1;
}

int main(int argc, char** argv) {
    long size = 0, pos = 0;
    unsigned char* data;
    int count = 0, patched = 0;
    float mul;

    if (argc < 3) {
        printf("usage: dspatch_star <in.core> <out.core> <multiplier>\n");
        printf("       dspatch_star <in.core> --verify\n");
        return 1;
    }
    data = read_file(argv[1], &size);
    if (!data) { printf("cannot read %s\n", argv[1]); return 3; }

    while (pos + 12 <= size) {
        unsigned long long h;
        unsigned int sz;
        memcpy(&h, data + pos, 8);
        memcpy(&sz, data + pos + 8, 4);
        if (sz == 0 || pos + 12 + (long)sz > size) { printf("malformed core\n"); return 3; }
        if (h == HOUSEHOLD_HASH && sz >= MIN_TH) {
            count++;
            if (valid_th(data + pos + 12)) patched++;
        }
        pos += 12 + (long)sz;
    }

    if (patched < 50) {
        printf("only %d valid household records - refusing\nNOT_VANILLA\n", patched);
        return 2;
    }
    if (strcmp(argv[2], "--verify") == 0) {
        printf("VANILLA_OK household=%d thresholds=%d\n", count, patched);
        return 0;
    }
    if (argc < 4) { printf("multiplier missing\n"); return 1; }
    mul = (float)atof(argv[3]);
    if (!(mul > 0.0f)) { printf("multiplier must be > 0\n"); return 4; }

    if (mul != 1.0f) {
        pos = 0;
        while (pos + 12 <= size) {
            unsigned long long h;
            unsigned int sz;
            memcpy(&h, data + pos, 8);
            memcpy(&sz, data + pos + 8, 4);
            if (h == HOUSEHOLD_HASH && sz >= MIN_TH && valid_th(data + pos + 12)) {
                int k;
                for (k = 0; k < 5; k++) {
                    unsigned char* p = data + pos + 12 + 0x18 + 4 * k;
                    int v = i32(p);
                    int nv = (int)((float)v * mul + 0.5f);
                    if (nv < 1) nv = 1;
                    w32(p, nv);
                }
            }
            pos += 12 + (long)sz;
        }
    }

    {
        FILE* f = fopen(argv[2], "wb");
        if (!f) { printf("cannot write %s\n", argv[2]); return 3; }
        if (fwrite(data, 1, (size_t)size, f) != (size_t)size) { fclose(f); return 3; }
        fclose(f);
    }
    printf("PATCHED_STARS household=%d thresholds=%d multiplier=x%.2f\n", count, patched, mul);
    return 0;
}
