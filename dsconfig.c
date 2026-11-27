#include "dsconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static const char* kNames[CATEGORY_COUNT] = {
    "Assault", "Shotgun", "Handgun", "MachineGun", "Rifle", "Grenade", "Melee", "Other", "Launcher", "Rocket"
};

static int   gExcludeEnemy = 1;
static float gDamage[CATEGORY_COUNT];
static float gRange[CATEGORY_COUNT];
static float gClip[CATEGORY_COUNT];
static float gPellet[CATEGORY_COUNT];
static int   gLoaded;
static float gZipline = 1.0f;
static float gVehTop[4] = {60.0f, 50.0f, 60.0f, 80.0f};
static float gVehTorque[4] = {500.0f, 500.0f, 500.0f, 500.0f};
static float gVehBoost[4] = {3.0f, 3.0f, 3.0f, 3.0f};

static int ieq(const char* a, const char* b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == 0 && *b == 0;
}

static void trim(char* s) {
    char* b = s;
    char* e;
    while (*b == ' ' || *b == '\t') b++;
    if (b != s) memmove(s, b, strlen(b) + 1);
    e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0;
}

static int category_of(const char* name) {
    for (int i = 0; i < CATEGORY_COUNT; i++) {
        if (ieq(name, kNames[i])) return i;
    }
    return -1;
}

int ds_config_load(const char* path) {
    FILE* f;
    char line[512];
    int section = -1;   /* -1 = none, -2 = general */

    for (int i = 0; i < CATEGORY_COUNT; i++) {
        gDamage[i] = gRange[i] = gClip[i] = gPellet[i] = 1.0f;
    }
    gLoaded = 1;
    if (!path) return 1;
    f = fopen(path, "r");
    if (!f) return 0;

    while (fgets(line, sizeof(line), f)) {
        char* p = line;
        char* eq;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == ';' || *p == '#' || *p == '\0' || *p == '\r' || *p == '\n') continue;
        if (*p == '[') {
            char* end = strchr(p, ']');
            if (end) *end = 0;
            p++;
            trim(p);
            if (ieq(p, "General") || ieq(p, "Filter")) section = -2;
            else if (ieq(p, "Zipline")) section = -3;
            else if (ieq(p, "Bike")) section = -4;
            else if (ieq(p, "TruckCA02")) section = -5;
            else if (ieq(p, "TruckCA03")) section = -6;
            else if (ieq(p, "SpeedCar")) section = -7;
            else section = category_of(p);
            continue;
        }
        eq = strchr(p, '=');
        if (!eq) continue;
        *eq = 0;
        trim(p);
        trim(eq + 1);
        if (section == -2) {
            if (ieq(p, "ExcludeEnemy")) gExcludeEnemy = atoi(eq + 1) ? 1 : 0;
            continue;
        }
        if (section == -3) {
            if (ieq(p, "Speed")) { float v = (float)atof(eq + 1); if (v > 0.0f) gZipline = v; }
            continue;
        }
        if (section <= -4 && section >= -7) {
            int k = -4 - section;
            float v = (float)atof(eq + 1);
            if (v > 0.0f) {
                if (ieq(p, "TopSpeed")) gVehTop[k] = v;
                else if (ieq(p, "Torque")) gVehTorque[k] = v;
                else if (ieq(p, "Boost")) gVehBoost[k] = v;
            }
            continue;
        }
        if (section < 0) continue;
        {
            float v = (float)atof(eq + 1);
            if (!(v > 0.0f)) v = 1.0f;
            if (ieq(p, "Damage")) gDamage[section] = v;
            else if (ieq(p, "Range")) gRange[section] = v;
            else if (ieq(p, "Clip") || ieq(p, "ClipCount")) gClip[section] = v;
            else if (ieq(p, "Pellet") || ieq(p, "PelletCount")) gPellet[section] = v;
        }
    }
    fclose(f);
    return 1;
}

int ds_exclude_enemy(void) { return gExcludeEnemy; }
const char* ds_category_name(int cat) { return kNames[cat]; }
float ds_damage(int cat) { return gDamage[cat]; }
float ds_range(int cat) { return gRange[cat]; }
float ds_clip(int cat) { return gClip[cat]; }
float ds_pellet(int cat) { return gPellet[cat]; }
float ds_zipline_speed(void) { return gZipline; }
float ds_veh_top(int kind) { return gVehTop[kind]; }
float ds_veh_torque(int kind) { return gVehTorque[kind]; }
float ds_veh_boost(int kind) { return gVehBoost[kind]; }
