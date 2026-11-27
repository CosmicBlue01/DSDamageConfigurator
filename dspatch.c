/* dspatch - patch attack_parameter_collection.core (damage / range).
 *
 * Usage:
 *   dspatch <in.core> <out.core> <config.ini>
 *   dspatch <in.core> --verify
 *
 * ini layout:
 *   [General]   ExcludeEnemy=1
 *   [Assault]   Damage=3  Range=2
 *   [Shotgun]   ...
 *
 * Exit codes: 0 ok, 2 not vanilla, 3 malformed, 4 bad config
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "vanilla_table.h"
#include "attack_categories.h"
#include "enemy_table.h"
#include "dsconfig.h"

#define REC_HASH 0x6a5a8bdc06d87009ULL
#define REC_SIZE 127
#define REC_STRIDE 139
#define REC_DATA 12

static const int kDmgFields[] = { 18, 22, 26, 30, 34, 38, 42, 46, 50, 54, 58, 62, 66, 70 };
static const int kRngFields[] = { 74, 78, 86, 90, 98, 106 };
#define DMG_N (int)(sizeof(kDmgFields) / sizeof(kDmgFields[0]))
#define RNG_N (int)(sizeof(kRngFields) / sizeof(kRngFields[0]))

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
    int count = 0; long i = 0;
    while (i + REC_STRIDE <= n) {
        if (*(unsigned long long*)(d + i) == REC_HASH && *(unsigned int*)(d + i + 8) == REC_SIZE) {
            if (offsets) offsets[count] = (int)i;
            count++; i += REC_STRIDE;
        } else i++;
    }
    return count;
}

static int verify_vanilla(unsigned char* d, long n) {
    int offs[VANILLA_COUNT + 8];
    int count = scan_records(d, n, offs);
    if (count != VANILLA_COUNT) {
        printf("record count mismatch: found %d, expected %d\n", count, VANILLA_COUNT);
        return 0;
    }
    for (int k = 0; k < count; k++) {
        unsigned char* rec = d + offs[k] + REC_DATA;
        unsigned short id = *(unsigned short*)(rec + 16);
        unsigned int bits = *(unsigned int*)(rec + 18);
        if (id != kVanilla[k].id) { printf("record %d: id %u != %u\n", k, id, kVanilla[k].id); return 0; }
        if (bits != kVanilla[k].dmg_bits) {
            float a, b; memcpy(&a, &bits, 4); memcpy(&b, &kVanilla[k].dmg_bits, 4);
            printf("record %d (id %u): damage %.3f != vanilla %.3f\n", k, id, a, b);
            return 0;
        }
    }
    return 1;
}

int main(int argc, char** argv) {
    long size = 0;
    unsigned char* data;
    int offs[VANILLA_COUNT + 8];
    int count, changed = 0;
    int kept[CATEGORY_COUNT], skipped[CATEGORY_COUNT];

    if (argc < 3) {
        printf("usage: dspatch <in.core> <out.core> <config.ini>\n");
        printf("       dspatch <in.core> --verify\n");
        return 1;
    }
    if (!ds_config_load(argc >= 4 ? argv[3] : NULL)) { printf("cannot read config\n"); return 4; }

    data = read_file(argv[1], &size);
    if (!data) { printf("cannot read %s\n", argv[1]); return 3; }
    if (!verify_vanilla(data, size)) { printf("NOT_VANILLA\n"); return 2; }
    if (strcmp(argv[2], "--verify") == 0) {
        printf("VANILLA_OK records=%d size=%ld\n", VANILLA_COUNT, size);
        return 0;
    }

    memset(kept, 0, sizeof(kept));
    memset(skipped, 0, sizeof(skipped));
    count = scan_records(data, size, offs);

    for (int k = 0; k < count; k++) {
        unsigned char* rec = data + offs[k] + REC_DATA;
        unsigned short id = *(unsigned short*)(rec + 16);
        int cat = (id < ATTACK_ID_MAX) ? kAttackCategory[id] : 7;
        int enemy = (id < ATTACK_ID_MAX_ENEMY) ? kEnemyAttack[id] : 0;
        if (ds_exclude_enemy() && enemy) { skipped[cat]++; continue; }
        if (ds_damage(cat) != 1.0f) {
            for (int j = 0; j < DMG_N; j++) {
                float* v = (float*)(rec + kDmgFields[j]);
                if (*v > 0.0f) { *v *= ds_damage(cat); changed++; }
            }
        }
        if (ds_range(cat) != 1.0f) {
            for (int j = 0; j < RNG_N; j++) {
                float* v = (float*)(rec + kRngFields[j]);
                if (*v > 0.0f) { *v *= ds_range(cat); changed++; }
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

    printf("PATCHED records=%d fields=%d exclude_enemy=%d\n", count, changed, ds_exclude_enemy());
    for (int i = 0; i < CATEGORY_COUNT; i++) {
        printf("  %-10s dmg=x%.2f rng=x%.2f kept=%d enemy_skipped=%d\n",
               ds_category_name(i), ds_damage(i), ds_range(i), kept[i], skipped[i]);
    }
    return 0;
}
