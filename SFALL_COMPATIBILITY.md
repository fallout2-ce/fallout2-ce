# Sfall Compatibility

This document tracks Fallout 2 CE compatibility with sfall.  This is for modders who need to know which Sfall features work in CE.

For now, this covers opcodes/metarules, and hooks.  In the future, it will include other ways of modifying the engine (like ini files), and other Sfall-specific behaviour.

## Opcodes / Metarules

See [`https://sfall-team.github.io/sfall/`](https://sfall-team.github.io/sfall/) for documentation on specific functions.

| Group | Opcodes In Group | Compatibility | Notes |
| --- | --- | --- | --- |
| Direct memory access| read_byte,short,int,string<br>write_byte,short,int,string<br>call_offset_vX | 🚫 | Not possible.  Open an issue if you need functionality not covered by other opcodes. |
| Stats | set_pc_base_stat<br>set_pc_extra_stat<br>get_pc_base_stat<br>get_pc_extra_stat<br>set_critter_base_stat<br>set_critter_extra_stat<br>get_critter_base_stat<br>get_critter_extra_stat<br>get_stat_min<br>get_stat_max | implemented: all except get_stat_min, get_stat_max | CE uses engine stat helpers here instead of sfall's direct proto-field behavior, so derived-stat update behavior can differ. |
| Stats / Alter min/max | set_stat_max<br>set_stat_min<br>set_pc_stat_max<br>set_pc_stat_min<br>set_npc_stat_max<br>set_npc_stat_min | not implemented | - |
| Skills | set_critter_skill_points<br>get_critter_skill_points<br>set_available_skill_points<br>get_available_skill_points<br>set_skill_max<br>set_critter_skill_mod<br>set_base_skill_mod<br>mod_skill_points_per_level | not implemented | - |
| Graphics | graphics_funcs_available<br>force_graphics_refresh<br>get_screen_width<br>get_screen_height<br>set_palette | implemented: only get_screen_width, get_screen_height | - |
| Shaders | load_shader<br>free_shader<br>activate_shader<br>deactivate_shader<br>set/get_shader_* | 🚫 | likely will not implement direct compatibility
| Perks and traits | set_perk_image<br>set_perk_*<br>set_pyromaniac_mod<br>apply_heaveho_fix<br>set_swiftlearner_mod<br>set_fake_perk<br>set_fake_trait<br>set_selectable_perk<br>set_perkbox_title<br>hide_real_perks<br>show_real_perks<br>perk_add_mode<br>clear_selectable_perks<br>has_fake_perk<br>has_fake_trait<br>add_trait<br>remove_trait<br>seq_perk_freq | not implemented | - |
| Virtual file system | fs_create<br>fs_copy<br>fs_find<br>fs_read/write_*<br>fs_delete<br>fs_size<br>fs_pos<br>fs_seek<br>fs_resize | 🚫 | Open an issue if you have a use case for these |
| Combat / Knockback | set_weapon_knockback<br>set_target_knockback<br>set_attacker_knockback<br>remove_weapon_knockback<br>remove_target_knockback<br>remove_attacker_knockback | not implemented | - |
| Maps and encounters | in_world_map<br>force_encounter<br>force_encounter_with_flags<br>set_map_time_multi<br>set_map_enter_position<br>get_map_enter_position<br>exec_map_update_scripts<br>set_terrain_name<br>get_terrain_name<br>set_town_title<br>set_can_rest_on_map<br>get_can_rest_on_map<br>set_rest_heal_time<br>set_rest_mode<br>set_worldmap_heal_time | implemented: in_world_map, force_encounter, force_encounter_with_flags, set_map_time_multi | - |
| Maps and encounters / Worldmap | get_world_map_x_pos<br>get_world_map_y_pos<br>set_world_map_pos | ✅ | - |
| Audio | eax_available<br>set_eax_environment<br>play_sfall_sound<br>stop_sfall_sound | not implemented | *eax* opcodes will not be implemented |
| Combat / Weapons and ammo | get_weapon_ammo_pid<br>set_weapon_ammo_pid<br>get_weapon_ammo_count<br>set_weapon_ammo_count | ✅ | - |
| Sfall / Version | sfall_ver_major<br>sfall_ver_minor<br>sfall_ver_build | ✅ | CE currently reports `4.3.4` |
| Utility / Math | log, exponent, round, sqrt, abs, sin, cos, tan, arctan, ceil, ^, floor2, div | ✅ | - |
| Keyboard and mouse | key_pressed<br>tap_key<br>get_mouse_x/y<br>get_mouse_buttons | ✅ | - |
| Lists | list_begin<br>list_next<br>list_end<br>list_as_array<br>party_member_list | ✅ | - |
| Explosions | metarule2_explosions<br>set_attack_explosion_pattern<br>set_attack_explosion_art<br>set_attack_explosion_radius<br>set_attack_is_explosion_fire<br>set_explosion_radius<br>set_dynamite_damage<br>set_plastic_damage<br>get_explosion_damage<br>set_explosion_max_targets<br>item_make_explosive | ✅  except item_make_explosive | - |
| Animations | reg_anim_combat_check<br>reg_anim_destroy<br>reg_anim_animate_and_hide<br>reg_anim_light<br>reg_anim_change_fid<br>reg_anim_take_out<br>reg_anim_turn_towards<br>reg_anim_callback<br>reg_anim_animate_and_move | not implemented | - |
| Art and appearance | art_exists<br>refresh_pc_art<br>art_cache_clear<br>set_hero_race<br>set_hero_style | implemented: only art_exists, refresh_pc_art | - |
| Tiles and paths | get_tile_fid<br>tile_under_cursor<br>tile_light<br>tile_get_objs<br>tile_refresh_display<br>obj_blocking_tile<br>tile_by_position<br>get_tile_ground_fid<br>get_tile_roof_fid<br>obj_blocking_line<br>path_find_to<br>objects_in_radius | implemented: all except objects_in_radius | - |
| Utility | sprintf<br>typeof<br>atoi<br>atof | ✅ | - |
| Utility / Strings | string_split<br>substr<br>strlen<br>charcode<br>get_string_pointer<br>string_find<br>string_find_from<br>string_format<br>string_format_array<br>string_replace<br>string_to_case<br>string_compare | ✅ except get_string_pointer | `get_string_pointer` is deprecated and intentionally omitted. |
| Interface / Tags | show_iface_tag<br>hide_iface_tag<br>is_iface_tag_active<br>set_iface_tag_text<br>add_iface_tag | implemented: all except set_iface_tag_text, add_iface_tag | CE only handles built-in interface tags here; custom tag creation/text is not supported yet. |
| Global variables | set_sfall_global<br>get_sfall_global_int<br>get_sfall_global_float | implemented: all except get_sfall_global_float | Current CE storage is int-backed; `set_sfall_global` stores integer values and there is no float getter yet. |
| Hooks / Hook functions | init_hook<br>get_sfall_arg<br>get_sfall_args<br>get_sfall_arg_at<br>set_sfall_return<br>set_sfall_arg<br>register_hook<br>register_hook_proc<br>register_hook_proc_spec | ✅ except init_hook, register_hook_proc_spec | See below for implemented hooks. `init_hook` is deprecated and will not be implemented |
| Arrays / Array functions | create_array<br>temp_array<br>fix_array<br>set_array<br>get_array<br>resize_array<br>free_array<br>scan_array<br>len_array<br>save_array<br>load_array<br>array_key<br>arrayexpr | ✅ except save_array, load_array | - |
| Perks and traits / NPC perks | set_fake_perk_npc<br>set_fake_trait_npc<br>set_selectable_perk_npc<br>has_fake_perk_npc<br>has_fake_trait_npc | not implemented | - |
| Global scripts / Global script functions | set_global_script_repeat<br>set_global_script_type<br>available_global_script_types | implemented: all except available_global_script_types | - |
| Combat | attack_is_aimed<br>block_combat<br>force_aimed_shots<br>disable_aimed_shots<br>get_attack_type<br>get_bodypart_hit_modifier<br>combat_data<br>set_bodypart_hit_modifier<br>set_critical_table<br>get_critical_table<br>reset_critical_table<br>get_last_target<br>get_last_attacker<br>set_critter_burst_disable<br>get_critter_current_ap<br>set_critter_current_ap<br>set_spray_settings<br>get_combat_free_move<br>set_combat_free_move | implemented: only get_attack_type, get_bodypart_hit_modifier, combat_data, set_bodypart_hit_modifier | - |
| Car | set_car_current_town<br>car_gas_amount<br>set_car_intface_art | implemented: all except set_car_intface_art | - |
| Interface / Windows and images | art_frame_data<br>interface_art_draw<br>interface_print<br>draw_image<br>draw_image_scaled<br>get_window_under_mouse<br>create_win<br>get_window_attribute<br>message_box<br>set_window_flag<br>win_fill_color<br>interface_overlay<br>dialog_message<br>get_text_width<br>hide_window<br>show_window | implemented: only message_box, get_text_width, show_window | - |
| Interface / Outline | outlined_object<br>get_outline<br>set_outline | implemented: all except get_outline | - |
| Interface / Main interface | intface_is_hidden<br>intface_redraw<br>intface_hide<br>intface_show<br>set_quest_failure_value | implemented: only intface_redraw | `intface_redraw` only supports the zero-argument form; the optional-argument path is explicitly unimplemented. |
| Interface / Inventory | display_stats<br>inventory_redraw<br>critter_inven_obj2<br>get_current_inven_size<br>item_weight | implemented: only critter_inven_obj2 | `critter_inven_obj2` is present, but CE source marks it as a likely parameter-order mismatch. |
| Interface / Cursor | get_cursor_mode<br>set_cursor_mode | ✅ | - |
| Locks | lock_is_jammed<br>unjam_lock<br>set_unjam_locks_time | not implemented | - |
| INI settings | modified_ini<br>get_ini_setting<br>get_ini_string<br>get_ini_section<br>get_ini_sections<br>get_ini_config<br>get_ini_config_db<br>set_ini_setting | implemented: all except modified_ini, get_ini_config, get_ini_config_db | `modified_ini` is intentionally omitted as deprecated. |
| Objects and scripts | set_self<br>set_dude_obj<br>real_dude_obj<br>remove_script<br>set_script<br>get_script<br>obj_is_carrying_obj<br>loot_obj<br>dialog_obj<br>obj_under_cursor<br>get_object_data<br>set_object_data<br>get_flags<br>set_flags<br>set_unique_id<br>set_scr_name<br>obj_is_openable | implemented: set_self, get_script, loot_obj, dialog_obj, obj_under_cursor, get_object_data, get_flags, set_flags | - |
| Other | input_funcs_available<br>get_year<br>set_dm_model<br>set_df_model<br>set_movie_path<br>set_pipboy_available<br>get_kill_counter<br>mod_kill_counter<br>active_hand<br>toggle_active_hand<br>set_pickpocket_max<br>set_hit_chance_max<br>set_xp_mod<br>set_critter_hit_chance_mod<br>set_base_hit_chance_mod<br>inc_npc_level<br>get_npc_level<br>get_viewport_x<br>get_viewport_y<br>set_viewport_x<br>set_viewport_y<br>set_hp_per_level_mod<br>set_unspent_ap_bonus<br>get_unspent_ap_bonus<br>set_unspent_ap_perk_bonus<br>get_unspent_ap_perk_bonus<br>nb_create_char<br>get_proto_data<br>set_proto_data<br>hero_select_win<br>stop_game<br>resume_game<br>create_message_window<br>get_light_level<br>mark_movie_played<br>gdialog_get_barter_mod<br>set_inven_ap_cost<br>game_loaded<br>get_game_mode<br>get_uptime<br>set_base_pickpocket_mod<br>set_critter_pickpocket_mod<br>message_str_game<br>sneak_success<br>create_spatial<br>unwield_slot<br>get_inven_ap_cost<br>add_g_timer_event<br>signal_close_game<br>add_extra_msg_file<br>get_metarule_table<br>get_object_ai_data<br>metarule_exist<br>npc_engine_level_up<br>remove_timer_event<br>set_drugs_data<br>spatial_radius | implemented: get_year, active_hand, toggle_active_hand, get_proto_data, set_proto_data, create_message_window, game_loaded, get_game_mode, get_uptime, message_str_game, add_extra_msg_file, metarule_exist | `input_funcs_available`, `nb_create_char` are deprecated in sfall and intentionally absent in CE. `add_extra_msg_file` does not support the explicit `fileNumber` form in CE. |

## Hooks

| Hook | ID | Compatibility | Notes |
| --- | --- | --- | --- |
| ToHit | `HOOK_TOHIT` | ✅ | - |
| AfterHitRoll | `HOOK_AFTERHITROLL` | 🚫 | - |
| CalcAPCost | `HOOK_CALCAPCOST` | 🚫 | - |
| DeathAnim1 | `HOOK_DEATHANIM1` | 🚫 | (maybe) |
| DeathAnim2 | `HOOK_DEATHANIM2` | 🚫 | - |
| CombatDamage | `HOOK_COMBATDAMAGE` | ✅ | CE passes the raw `Attack*` as the final mixed argument, matching sfall's shape; this inherits the current `get_object_data`/`set_object_data` caveats. |
| OnDeath | `HOOK_ONDEATH` | 🚫 | - |
| FindTarget | `HOOK_FINDTARGET` | 🚫 | (maybe) |
| UseObjOn | `HOOK_USEOBJON` | ✅ | - |
| UseObj | `HOOK_USEOBJ` | ✅ | CE notes an sfall-matching inconsistency around return code `2` behavior between interface contexts. |
| RemoveInvenObj | `HOOK_REMOVEINVENOBJ` | 🚫 | - |
| BarterPrice | `HOOK_BARTERPRICE` | ✅ | - |
| MoveCost | `HOOK_MOVECOST` | 🚫 | - |
| ItemDamage | `HOOK_ITEMDAMAGE` | 🚫 | - |
| AmmoCost | `HOOK_AMMOCOST` | 🚫 | - |
| KeyPress | `HOOK_KEYPRESS` | ✅ | Third hook arg is currently `0`; CE notes that sfall used VK codes there. |
| MouseClick | `HOOK_MOUSECLICK` | 🚫 | - |
| UseSkill | `HOOK_USESKILL` | 🚫 | - |
| Steal | `HOOK_STEAL` | 🚫 | - |
| WithinPerception | `HOOK_WITHINPERCEPTION` | 🚫 | - |
| InventoryMove | `HOOK_INVENTORYMOVE` | 🚫 | - |
| InvenWield | `HOOK_INVENWIELD` | 🚫 | - |
| AdjustFID | `HOOK_ADJUSTFID` | 🚫 | - |
| CombatTurn | `HOOK_COMBATTURN` | 🚫 | - |
| StdProcedure | `HOOK_STDPROCEDURE` | 🚫 | - |
| StdProcedureEnd | `HOOK_STDPROCEDURE_END` | 🚫 | - |
| CarTravel | `HOOK_CARTRAVEL` | 🚫 | - |
| SetGlobalVar | `HOOK_SETGLOBALVAR` | 🚫 | - |
| RestTimer | `HOOK_RESTTIMER` | 🚫 | - |
| GameModeChange | `HOOK_GAMEMODECHANGE` | ✅ | - |
| UseAnimObj | `HOOK_USEANIMOBJ` | 🚫 | (maybe) |
| ExplosiveTimer | `HOOK_EXPLOSIVETIMER` | 🚫 | - |
| DescriptionObj | `HOOK_DESCRIPTIONOBJ` | 🚫 | - |
| UseSkillOn | `HOOK_USESKILLON` | 🚫 | - |
| OnExplosion | `HOOK_ONEXPLOSION` | 🚫 | (maybe) |
| SubCombatDamage | `HOOK_SUBCOMBATDAMAGE` | 🚫 | (maybe) |
| SetLighting | `HOOK_SETLIGHTING` | 🚫 | (maybe) |
| Sneak | `HOOK_SNEAK` | 🚫 | - |
| TargetObject | `HOOK_TARGETOBJECT` | 🚫 | (maybe) |
| Encounter | `HOOK_ENCOUNTER` | 🚫 | - |
| AdjustPoison | `HOOK_ADJUSTPOISON` | 🚫 | (maybe) |
| AdjustRads | `HOOK_ADJUSTRADS` | 🚫 | (maybe) |
| RollCheck | `HOOK_ROLLCHECK` | 🚫 | - |
| BestWeapon | `HOOK_BESTWEAPON` | 🚫 | - |
| CanUseWeapon | `HOOK_CANUSEWEAPON` | 🚫 | - |
| BuildSfxWeapon | `HOOK_BUILDSFXWEAPON` | 🚫 | - |
