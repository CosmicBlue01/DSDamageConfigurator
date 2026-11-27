/* DSTweaks.exe - installer / restorer for the DSTweaks mod.
 *
 *   DSTweaks.exe             install (double-click friendly)
 *   DSTweaks.exe install     same
 *   DSTweaks.exe restore     restore the original archive
 *   optional next argument: game folder
 *
 * The two patchers (dspatch.c / dspatch_mag.c) are linked in; the archive is
 * unpacked and repacked with the bundled bin\DecimaExplorer.exe.
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <io.h>
#include <direct.h>
#include "dsconfig.h"


extern int dd_attack_main(int argc, char** argv);
extern int dd_mag_main(int argc, char** argv);
extern int dd_veh_main(int argc, char** argv);

#define VANILLA_SIZE 7297853249ULL
#define BIN_HINT "7017f9bb9d52fc1c4433599203cc51b1.bin"

static char g_exe_dir[MAX_PATH];

static int fexists(const char* p) { return GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES; }
static int fisdir(const char* p) {
    DWORD a = GetFileAttributesA(p);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}
static unsigned long long fsize(const char* p) {
    WIN32_FILE_ATTRIBUTE_DATA d;
    if (!GetFileAttributesExA(p, GetFileExInfoStandard, &d)) return 0;
    return ((unsigned long long)d.nFileSizeHigh << 32) | d.nFileSizeLow;
}
static void to_fwd(char* s) { for (; *s; s++) if (*s == '\\') *s = '/'; }
static void dir_of(const char* path, char* out) {
    const char* s = strrchr(path, '\\');
    const char* t = strrchr(path, '/');
    if (t > s) s = t;
    if (!s) { strcpy(out, "."); return; }
    memcpy(out, path, (size_t)(s - path));
    out[s - path] = 0;
}
static int mkdir_p(const char* path) {
    char tmp[MAX_PATH];
    size_t i, n = strlen(path);
    if (n >= sizeof(tmp)) return 0;
    strcpy(tmp, path);
    for (i = 3; i <= n; i++) {
        if (tmp[i] == '\\' || tmp[i] == '/' || tmp[i] == 0) {
            char c = tmp[i];
            tmp[i] = 0;
            if (!fisdir(tmp)) CreateDirectoryA(tmp, NULL);
            tmp[i] = c;
        }
    }
    return fisdir(path);
}
static int rm_tree(const char* path) {
    WIN32_FIND_DATAA fd;
    HANDLE h;
    char pat[MAX_PATH], full[MAX_PATH];
    if (!fisdir(path)) return 0;
    snprintf(pat, sizeof(pat), "%s\\*", path);
    h = FindFirstFileA(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
            snprintf(full, sizeof(full), "%s\\%s", path, fd.cFileName);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) rm_tree(full);
            else {
                SetFileAttributesA(full, FILE_ATTRIBUTE_NORMAL);
                DeleteFileA(full);
            }
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryA(path);
    return 1;
}
/* run an exe with an argument string, wait, return exit code */
static int run_exe(const char* exe, const char* args, int hide) {
    char cmd[4096];
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    DWORD code = (DWORD)-1;
    snprintf(cmd, sizeof(cmd), "\"%s\" %s", exe, args ? args : "");
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    if (hide) { si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE; }
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, g_exe_dir, &si, &pi)) return -1;
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)code;
}
static void pause_exit(int code) {
    if (_isatty(_fileno(stdin))) {
        int c;
        printf("\nPress Enter to close...");
        fflush(stdout);
        while ((c = getchar()) != EOF && c != '\n') {}
    }
    exit(code);
}
static int try_game(const char* p, char* out) {
    char probe[MAX_PATH];
    if (!p || !*p || !fisdir(p)) return 0;
    snprintf(probe, sizeof(probe), "%s\\ds.exe", p);
    if (fexists(probe)) { strcpy(out, p); return 1; }
    snprintf(probe, sizeof(probe), "%s\\DeathStranding.exe", p);
    if (fexists(probe)) { strcpy(out, p); return 1; }
    return 0;
}
static int find_game(const char* hint, char* out) {
    static const char* rel[] = {
        "\\Steam\\steamapps\\common\\DEATH STRANDING DIRECTORS CUT",
        "\\SteamLibrary\\steamapps\\common\\DEATH STRANDING DIRECTORS CUT",
        "\\Games\\Steam\\steamapps\\common\\DEATH STRANDING DIRECTORS CUT",
        "\\Program Files (x86)\\Steam\\steamapps\\common\\DEATH STRANDING DIRECTORS CUT",
        "\\Program Files\\Steam\\steamapps\\common\\DEATH STRANDING DIRECTORS CUT",
        "\\Epic Games\\DeathStranding",
        "\\Program Files\\Epic Games\\DeathStranding"
    };
    char path[MAX_PATH], buf[MAX_PATH];
    FILE* f;
    int d, i;

    if (try_game(hint, out)) return 1;
    snprintf(path, sizeof(path), "%s\\gamepath.txt", g_exe_dir);
    f = fopen(path, "r");
    if (f) {
        if (fgets(buf, sizeof(buf), f)) {
            char* nl = strpbrk(buf, "\r\n");
            if (nl) *nl = 0;
            if (try_game(buf, out)) { fclose(f); return 1; }
        }
        fclose(f);
    }
    {
        char cwd[MAX_PATH];
        if (_getcwd(cwd, sizeof(cwd)) && try_game(cwd, out)) return 1;
    }
    for (d = 'C'; d <= 'J'; d++)
        for (i = 0; i < (int)(sizeof(rel) / sizeof(rel[0])); i++) {
            snprintf(path, sizeof(path), "%c:%s", d, rel[i]);
            if (try_game(path, out)) return 1;
        }
    return 0;
}
static int find_archive(const char* data, char* name, size_t n) {
    WIN32_FIND_DATAA fd;
    HANDLE h;
    char pat[MAX_PATH];
    snprintf(pat, sizeof(pat), "%s\\" BIN_HINT, data);
    if (fexists(pat)) { strncpy(name, BIN_HINT, n - 1); name[n - 1] = 0; return 1; }
    snprintf(pat, sizeof(pat), "%s\\*.bin", data);
    h = FindFirstFileA(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            unsigned long long sz = ((unsigned long long)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && sz == VANILLA_SIZE) {
                strncpy(name, fd.cFileName, n - 1); name[n - 1] = 0;
                FindClose(h);
                return 1;
            }
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    snprintf(pat, sizeof(pat), "%s\\*.bin.vanilla", data);
    h = FindFirstFileA(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            unsigned long long sz = ((unsigned long long)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
            if (sz == VANILLA_SIZE) {
                strncpy(name, fd.cFileName, n - 1); name[n - 1] = 0;
                name[strlen(name) - 8] = 0;
                FindClose(h);
                return 1;
            }
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    snprintf(pat, sizeof(pat), "%s\\*.bin", data);
    h = FindFirstFileA(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        strncpy(name, fd.cFileName, n - 1); name[n - 1] = 0;
        FindClose(h);
        return 1;
    }
    return 0;
}
static int cmp_files(const char* a, const char* b) {
    FILE* fa = fopen(a, "rb");
    FILE* fb = fopen(b, "rb");
    int r = 1;
    if (!fa || !fb) { if (fa) fclose(fa); if (fb) fclose(fb); return 0; }
    for (;;) {
        int ca = fgetc(fa), cb = fgetc(fb);
        if (ca != cb) { r = 0; break; }
        if (ca == EOF) break;
    }
    fclose(fa); fclose(fb);
    return r;
}

int main(int argc, char** argv) {
    char game[MAX_PATH], data[MAX_PATH], bin[MAX_PATH], arch[MAX_PATH], archv[MAX_PATH];
    char work[MAX_PATH], tool[MAX_PATH], ini[MAX_PATH];
    char core_a[MAX_PATH], core_m[MAX_PATH];
    char out_a[MAX_PATH], out_m[MAX_PATH];
    char vcore[5][MAX_PATH], vout[5][MAX_PATH];
    char a_fwd[MAX_PATH], af_fwd[MAX_PATH], w_fwd[MAX_PATH];
    const char* mode = "install";
    const char* hint = NULL;
    int first_run = 0;
    unsigned long long sz;

    GetModuleFileNameA(NULL, g_exe_dir, sizeof(g_exe_dir));
    dir_of(g_exe_dir, g_exe_dir);

    if (argc > 1) {
        if (strcmp(argv[1], "restore") == 0 || strcmp(argv[1], "install") == 0) {
            mode = argv[1];
            if (argc > 2) hint = argv[2];
        } else if (fisdir(argv[1])) {
            hint = argv[1];
        } else {
            printf("Usage: DSTweaks.exe [install|restore] [game folder]\n");
            pause_exit(1);
        }
    }
    if (!(argc > 1) || fisdir(argv[1])) {
        char line[64];
        printf("\n  1. Install / apply DSTweaks.ini\n");
        printf("  2. Restore the original archive\n");
        printf("  0. Exit\n\n");
        printf("Choose [1]: ");
        fflush(stdout);
        if (fgets(line, sizeof(line), stdin) && line[0] == '2') mode = "restore";
        else if (line[0] == '0') return 0;
    }

    printf("============================================================\n");
    printf("  DSTweaks 1.0  -  %s\n", strcmp(mode, "restore") == 0 ? "restore" : "install");
    printf("============================================================\n");

    if (!find_game(hint, game)) { printf("[X] Game folder not found.\n"); pause_exit(1); }
    printf("[OK] Game: %s\n", game);

    snprintf(data, sizeof(data), "%s\\data", game);
    if (!find_archive(data, bin, sizeof(bin))) {
        printf("[X] Could not find the game archive in %s\n", data);
        printf("    Expected a 7,297,853,249 byte .bin file.\n");
        pause_exit(1);
    }
    snprintf(arch, sizeof(arch), "%s\\%s", data, bin);
    snprintf(archv, sizeof(archv), "%s.vanilla", arch);
    printf("[OK] Archive: %s\n", bin);

    if (strcmp(mode, "restore") == 0) {
        if (!fexists(archv)) {
            printf("[X] Backup not found: %s\n", archv);
            printf("    Use Steam/Epic \"verify integrity\" instead.\n");
            pause_exit(1);
        }
        printf("Restoring original archive (7.3 GB)...\n");
        if (!CopyFileA(archv, arch, FALSE)) { printf("[X] Restore failed (%lu).\n", GetLastError()); pause_exit(1); }
        printf("[OK] Restored the original archive.\n");
        pause_exit(0);
    }

    snprintf(tool, sizeof(tool), "%s\\bin\\DecimaExplorer.exe", g_exe_dir);
    snprintf(ini, sizeof(ini), "%s\\DSTweaks.ini", g_exe_dir);
    if (!fexists(tool)) { printf("[X] bin\\DecimaExplorer.exe missing.\n"); pause_exit(1); }
    if (!fexists(ini)) { printf("[X] DSTweaks.ini missing.\n"); pause_exit(1); }

    {
        char oo2[MAX_PATH], gameoo[MAX_PATH];
        snprintf(oo2, sizeof(oo2), "%s\\bin\\oo2core_7_win64.dll", g_exe_dir);
        if (!fexists(oo2)) {
            snprintf(gameoo, sizeof(gameoo), "%s\\oo2core_7_win64.dll", game);
            if (!fexists(gameoo)) { printf("[X] oo2core_7_win64.dll not found in the game folder.\n"); pause_exit(1); }
            CopyFileA(gameoo, oo2, FALSE);
        }
    }

    snprintf(work, sizeof(work), "%s\\_dstweaks_tmp", game);
    if (fisdir(work)) rm_tree(work);
    {
        char pk[MAX_PATH];
        snprintf(pk, sizeof(pk), "%s\\pack\\ds\\entities\\weapons", work);
        if (!mkdir_p(pk)) {
            char tmp[MAX_PATH];
            GetTempPathA(sizeof(tmp), tmp);
            snprintf(work, sizeof(work), "%sdsdamage_wk", tmp);
            snprintf(pk, sizeof(pk), "%s\\pack\\ds\\entities\\weapons", work);
            if (!mkdir_p(pk)) { printf("[X] Cannot create a temp folder.\n"); pause_exit(1); }
        }
    }
    printf("     temp: %s\n", work);

    snprintf(core_a, sizeof(core_a), "%s\\src_a.core", work);
    snprintf(core_m, sizeof(core_m), "%s\\src_m.core", work);
    snprintf(out_a, sizeof(out_a), "%s\\pack\\ds\\entities\\weapons\\attack_parameter_collection.core", work);
    snprintf(out_m, sizeof(out_m), "%s\\pack\\ds\\entities\\weapons\\magazine_parameter_collection.core", work);
    {
        static const char* vnames[5] = {
            "pe_ds_vehicle_bkyk", "pe_ds_vehicle_ca02", "pe_ds_vehicle_ca03",
            "pe_ds_vehicle_casp", "pe_ds_vehicle_celd"
        };
        int vi;
        for (vi = 0; vi < 5; vi++) {
            snprintf(vcore[vi], sizeof(vcore[vi]), "%s\\src_v%d.core", work, vi);
            snprintf(vout[vi], sizeof(vout[vi]), "%s\\pack\\ds\\entities\\vehicle\\%s.core", work, vnames[vi]);
        }
    }

    if (!fexists(archv)) {
        if (fsize(arch) != VANILLA_SIZE) {
            printf("[X] Archive size is %llu bytes, expected %llu.\n", fsize(arch), VANILLA_SIZE);
            printf("    The game version is not supported, or the files are modified.\n");
            pause_exit(1);
        }
        first_run = 1;
    } else if (fsize(archv) != VANILLA_SIZE) {
        printf("[X] The backup is %llu bytes, expected %llu.\n", fsize(archv), VANILLA_SIZE);
        printf("    Delete \"%s\" and verify the game files, then retry.\n", archv);
        pause_exit(1);
    }

    printf("[1/7] Verifying the source archive\n");
    snprintf(af_fwd, sizeof(af_fwd), "%s", first_run ? arch : archv); to_fwd(af_fwd);
    snprintf(w_fwd, sizeof(w_fwd), "%s", work); to_fwd(w_fwd);
    {
        char args[2048];
        snprintf(args, sizeof(args), "-e \"%s\" \"ds/entities/weapons/attack_parameter_collection\" \"%s/src_a.core\"", af_fwd, w_fwd);
        run_exe(tool, args, 1);
        snprintf(args, sizeof(args), "-e \"%s\" \"ds/entities/weapons/magazine_parameter_collection\" \"%s/src_m.core\"", af_fwd, w_fwd);
        run_exe(tool, args, 1);
        {
            static const char* vp[5] = {
                "pe_ds_vehicle_bkyk", "pe_ds_vehicle_ca02", "pe_ds_vehicle_ca03",
                "pe_ds_vehicle_casp", "pe_ds_vehicle_celd"
            };
            int vi;
            for (vi = 0; vi < 5; vi++) {
                snprintf(args, sizeof(args), "-e \"%s\" \"ds/entities/vehicle/%s\" \"%s/src_v%d.core\"",
                         af_fwd, vp[vi], w_fwd, vi);
                run_exe(tool, args, 1);
            }
        }
    }
    if (!fexists(core_a)) { printf("[X] Extraction failed (attack).\n"); pause_exit(1); }
    if (!fexists(core_m)) { printf("[X] Extraction failed (magazine).\n"); pause_exit(1); }

    {
        char* v1[3]; char* v2[3];
        int bad = 0;
        v1[0] = "dspatch"; v1[1] = core_a; v1[2] = "--verify";
        v2[0] = "dspatch_mag"; v2[1] = core_m; v2[2] = "--verify";
        if (dd_attack_main(3, v1) != 0 || dd_mag_main(3, v2) != 0) bad = 1;
        {
            int vi;
            char* vv[3];
            vv[0] = "dspatch_veh"; vv[2] = "--verify";
            for (vi = 0; vi < 5 && !bad; vi++) {
                if (!fexists(vcore[vi])) continue;
                vv[1] = vcore[vi];
                if (dd_veh_main(3, vv) != 0) bad = 1;
            }
        }
        if (bad) {
            printf("[X] The weapon data in the archive is not the vanilla table.\n");
            printf("    Another mod (or an incomplete restore) is present.\n");
            printf("    Delete \"%s\" and verify the game files.\n", archv);
            pause_exit(1);
        }
        printf("     all cores match the vanilla tables.\n");

        if (first_run) {
            printf("[2/7] Backing up the original archive (7.3 GB)\n");
            if (!CopyFileA(arch, archv, FALSE)) { printf("[X] Backup failed.\n"); pause_exit(1); }
        } else printf("[2/7] Backup present and verified\n");

        printf("[3/7] Restoring clean state from backup\n");
        if (!CopyFileA(archv, arch, FALSE)) { printf("[X] Restore failed.\n"); pause_exit(1); }

        printf("[4/7] Patching attack parameters\n");
        {
            char* p1[4];
            p1[0] = "dspatch"; p1[1] = core_a; p1[2] = out_a; p1[3] = ini;
            if (dd_attack_main(4, p1) != 0) { printf("[X] Attack patch failed.\n"); pause_exit(1); }
        }
        printf("[5/7] Patching magazines\n");
        {
            char* p2[4];
            p2[0] = "dspatch_mag"; p2[1] = core_m; p2[2] = out_m; p2[3] = ini;
            if (dd_mag_main(4, p2) != 0) { printf("[X] Magazine patch failed.\n"); pause_exit(1); }
        }


        printf("[6/7] Patching vehicles\n");
        {
            int vi;
            char top[32], tor[32], bst[32];
            char* pv[6];
            char vdir[MAX_PATH];
            snprintf(vdir, sizeof(vdir), "%s\\pack\\ds\\entities\\vehicle", work);
            mkdir_p(vdir);
            for (vi = 0; vi < 5; vi++) {
                int truck;
                if (!fexists(vcore[vi])) continue;
                if (vi == 0) truck = 0;              /* bike */
                else if (vi == 1 || vi == 2) truck = 1; /* trucks */
                else continue;                        /* casp / celd: vanilla */
                snprintf(top, sizeof(top), "%.6f", ds_veh_top(truck));
                snprintf(tor, sizeof(tor), "%.6f", ds_veh_torque(truck));
                snprintf(bst, sizeof(bst), "%.6f", ds_veh_boost(truck));
                pv[0] = "dspatch_veh"; pv[1] = vcore[vi]; pv[2] = vout[vi];
                pv[3] = top; pv[4] = tor; pv[5] = bst;
                if (dd_veh_main(6, pv) != 0) { printf("[X] Vehicle patch failed.\n"); pause_exit(1); }
            }
        }

        {
            int same = cmp_files(core_a, out_a) && cmp_files(core_m, out_m);
            if (fexists(vcore[0])) same = same && cmp_files(vcore[0], vout[0]);
            if (fexists(vcore[1])) same = same && cmp_files(vcore[1], vout[1]);
            if (fexists(vcore[2])) same = same && cmp_files(vcore[2], vout[2]);
            if (same) {
                rm_tree(work);
                printf("\n[OK] Every multiplier is 1 - the archive was left untouched (vanilla).\n");
                pause_exit(0);
            }
        }
    }

    printf("[7/7] Repacking the patched files (7.3 GB, takes a few minutes)\n");
    snprintf(a_fwd, sizeof(a_fwd), "%s", arch); to_fwd(a_fwd);
    {
        char args[2048];
        snprintf(args, sizeof(args), "-r \"%s\" \"%s/pack\"", a_fwd, w_fwd);
        run_exe(tool, args, 1);
    }
    if (fsize(arch) == VANILLA_SIZE) {
        char prev[MAX_PATH], pk[MAX_PATH];
        printf("     retrying with relative paths...\n");
        GetCurrentDirectoryA(sizeof(prev), prev);
        _chdir(game);
        snprintf(pk, sizeof(pk), "_dstweaks_tmp/pack");
        {
            char args[2048], rel[MAX_PATH];
            snprintf(rel, sizeof(rel), "data/%s", bin);
            snprintf(args, sizeof(args), "-r \"%s\" \"%s\"", rel, pk);
            run_exe(tool, args, 1);
        }
        _chdir(prev);
    }
    if (fsize(arch) == VANILLA_SIZE) {
        printf("[X] Repacking did not change the archive - restoring the original.\n");
        CopyFileA(archv, arch, FALSE);
        rm_tree(work);
        pause_exit(1);
    }

    rm_tree(work);
    printf("\n[OK] Done. Values were taken from DSTweaks.ini\n");
    printf("     To undo, run: DSTweaks.exe restore\n");
    pause_exit(0);
    return 0;
}
