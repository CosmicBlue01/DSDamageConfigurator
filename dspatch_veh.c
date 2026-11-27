/* dspatch_veh - patch vehicle performance in pe_ds_vehicle_*.core
 *
 * The parameters live in a block of consecutive floats:
 *
 *   [MaxTorque][MinRPM][OptRPM][MaxRPM][TopSpeed][BurstAccelScale]
 *   vanilla truck:  500   1000    5500    7500     50         3.0
 *
 * They are not 4-byte aligned relative to the record, so the block is located
 * by content: 1000.0, 5500.0 and 3.0 at fixed relative positions.
 *
 * Usage: dspatch_veh <in.core> <out.core> <top> <torque> <boost>
 *        dspatch_veh <in.core> --verify
 * Exit: 0 ok, 2 not vanilla, 3 malformed
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static float f32(const unsigned char* p) { float v; memcpy(&v, p, 4); return v; }
static void w32(unsigned char* p, float v) { memcpy(p, &v, 4); }

/* returns file offset of the block (MaxTorque) or -1 */
static long find_block(const unsigned char* d, long size) {
    long o;
    for (o = 16; o + 24 <= size; o++) {
        if (f32(d + o + 4) == 1000.0f && f32(d + o + 8) == 5500.0f &&
            f32(d + o + 20) == 3.0f) {
            float top = f32(d + o + 16);
            if (top > 5.0f && top < 300.0f)
                return o;
        }
    }
    return -1;
}

int main(int argc, char** argv) {
    long size = 0, off;
    unsigned char* data;
    float top, torque, boost;

    if (argc < 3) {
        printf("usage: dspatch_veh <in.core> <out.core> <top> <torque> <boost>\n");
        printf("       dspatch_veh <in.core> --verify\n");
        return 1;
    }
    data = read_file(argv[1], &size);
    if (!data) { printf("cannot read %s\n", argv[1]); return 3; }

    off = find_block(data, size);
    if (off < 0) { printf("vehicle parameter block not found\nNOT_VANILLA\n"); return 2; }

    if (strcmp(argv[2], "--verify") == 0) {
        printf("VANILLA_OK vehicle_block=%#lx torque=%.0f rpm=%.0f top=%.0f boost=%.2f\n",
               off, f32(data + off), f32(data + off + 12), f32(data + off + 16),
               f32(data + off + 20));
        return 0;
    }
    if (argc < 6) { printf("need three multipliers\n"); return 1; }
    top = (float)atof(argv[3]);
    torque = (float)atof(argv[4]);
    boost = (float)atof(argv[5]);
    if (!(top > 0 && torque > 0 && boost > 0)) { printf("multipliers must be > 0\n"); return 4; }

    if (top > 0.0f) w32(data + off + 16, top);
    if (torque > 0.0f) w32(data + off, torque);
    if (boost > 0.0f) w32(data + off + 20, boost);

    {
        FILE* f = fopen(argv[2], "wb");
        if (!f) { printf("cannot write %s\n", argv[2]); return 3; }
        if (fwrite(data, 1, (size_t)size, f) != (size_t)size) { fclose(f); return 3; }
        fclose(f);
    }
    printf("PATCHED_VEHICLE torque=%.0f top=%.0f boost=%.2f\n",
           f32(data + off), f32(data + off + 16), f32(data + off + 20));
    return 0;
}
