/* dsconfig - shared ini parser for dspatch / dspatch_mag
 *
 * ini layout (per weapon category):
 *   [General]   ExcludeEnemy=1
 *   [Assault]   Damage=3  Range=2  Clip=1.5  Ammo=1
 *   [Shotgun]   ... Pellet=1.5 ...
 *   [Handgun] [MachineGun] [Rifle] [Grenade] [Melee] [Other]
 */
#pragma once

#define CATEGORY_COUNT 10

int   ds_config_load(const char* path);
int   ds_exclude_enemy(void);
const char* ds_category_name(int cat);
float ds_damage(int cat);
float ds_range(int cat);
float ds_clip(int cat);
float ds_pellet(int cat);
float ds_zipline_speed(void);
float ds_veh_top(int kind);
float ds_veh_torque(int kind);
float ds_veh_boost(int kind);
float ds_star_threshold(void);
