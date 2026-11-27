/* dspatch_mag - patch magazine_parameter_collection.core (clip size / pellets).
 *
 * Usage:
 *   dspatch_mag <in.core> <out.core> <config.ini>
 *   dspatch_mag <in.core> --verify
 *
 * Exit codes: 0 ok, 2 not vanilla, 3 malformed
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "mag_table.h"
#include "dsconfig.h"

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

static int scan_records(unsigned char* d, long n, int* offsets) {
    int count = 0;
    long i = 0;
    while (i + 12 <= n) {
        unsigned int sz = *(unsigned int*)(d + i + 8);
        if (sz > 0 && i + 12 + sz <= n) {
            if (offsets) offsets[count] = (int)i;
            count++;
            i += 12 + sz;
        } else return -1;
    }
    return count;
}

int main(int argc, char** argv) {
    long size = 0;
    unsigned char* data;
    int offs[MAG_COUNT + 8];
    int count, changed = 0;
    int kept[CATEGORY_COUNT], skipped[CATEGORY_COUNT];

    if (argc < 3) {
        printf("usage: dspatch_mag <in.core> <out.core> <config.ini>\n");
        printf("       dspatch_mag <in.core> --verify\n");
        return 1;
    }
    if (!ds_config_load(argc >= 4 ? argv[3] : NULL)) { printf("cannot read config\n"); return 4; }

    data = read_file(argv[1], &size);
    if (!data) { printf("cannot read %s\n", argv[1]); return 3; }
    count = scan_records(data, size, offs);
    if (count != MAG_COUNT) {
        printf("record count mismatch: found %d, expected %d\n", count, MAG_COUNT);
        printf("NOT_VANILLA\n");
        return 2;
    }
    for (int k = 0; k < count; k++) {
        unsigned char id = data[offs[k] + 12 + 16];
        if (id != kMagRecs[k].id) {
            printf("record %d: id %u != %u\n", k, id, kMagRecs[k].id);
            printf("NOT_VANILLA\n");
            return 2;
        }
        if (kMagRecs[k].off >= 0) {
            unsigned int clip = *(unsigned int*)(data + offs[k] + 12 + kMagRecs[k].off + 4);
            unsigned int pel = *(unsigned int*)(data + offs[k] + 12 + kMagRecs[k].off + 8);
            if (clip != kMagRecs[k].clip) {
                printf("record %d (id %u): clip %u != vanilla %u\n", k, id, clip, kMagRecs[k].clip);
                printf("NOT_VANILLA\n");
                return 2;
            }
            if (pel != kMagRecs[k].pellet) {
                printf("record %d (id %u): pellet %u != vanilla %u\n", k, id, pel, kMagRecs[k].pellet);
                printf("NOT_VANILLA\n");
                return 2;
            }
            {
                unsigned int bits = *(unsigned int*)(data + offs[k] + 12 + kMagRecs[k].off);
                if (bits != kMagRecs[k].scale_bits) {
                    printf("record %d (id %u): ammo scale bits %08x != vanilla %08x\n", k, id, bits, kMagRecs[k].scale_bits);
                    printf("NOT_VANILLA\n");
                    return 2;
                }
            }
        }
    }
    if (strcmp(argv[2], "--verify") == 0) {
        printf("VANILLA_OK records=%d size=%ld\n", MAG_COUNT, size);
        return 0;
    }

    memset(kept, 0, sizeof(kept));
    memset(skipped, 0, sizeof(skipped));
    for (int k = 0; k < count; k++) {
        int cat = kMagRecs[k].cat;
        unsigned char* rec = data + offs[k] + 12;
        if (kMagRecs[k].off < 0) continue;
        if (ds_exclude_enemy() && kMagRecs[k].enemy) { skipped[cat]++; continue; }
        {
            unsigned int* clip = (unsigned int*)(rec + kMagRecs[k].off + 4);
            unsigned int* pel = (unsigned int*)(rec + kMagRecs[k].off + 8);
            if (ds_clip(cat) != 1.0f && *clip > 0) {
                unsigned int v = (unsigned int)((float)*clip * ds_clip(cat) + 0.5f);
                if (v < 1) v = 1;
                if (v != *clip) { *clip = v; changed++; }
            }
            if (ds_pellet(cat) != 1.0f && *pel > 0) {
                unsigned int v = (unsigned int)((float)*pel * ds_pellet(cat) + 0.5f);
                if (v < 1) v = 1;
                if (v != *pel) { *pel = v; changed++; }
            }
        }
        kept[cat]++;
    }

    {
        FILE* f = fopen(argv[2], "wb");
        if (!f) { printf("cannot write %s\n", argv[2]); return 3; }
        if (fwrite(data, 1, (size_t)size, f) != (size_t)size) { fclose(f); return 3; }
        fclose(f);
    }

    printf("PATCHED_MAG records=%d fields=%d exclude_enemy=%d\n", count, changed, ds_exclude_enemy());
    for (int i = 0; i < CATEGORY_COUNT; i++) {
        printf("  %-10s clip=x%.2f pellet=x%.2f kept=%d enemy_skipped=%d\n",
               ds_category_name(i), ds_clip(i), ds_pellet(i), kept[i], skipped[i]);
    }
    return 0;
}
