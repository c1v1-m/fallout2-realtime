// Generates fallout2-ce/src/rt/f1compat_gen.h: one inline wrapper per F1 engine function our
// modules call, with the F1 signature, forwarding to the F2 function. A wrong pairing or a changed
// signature fails to compile, which is the point.
const fs = require('fs');
const path = require('path');
const F1 = 'C:/Users/MS/Documents/GameDev/fallout1-ce/src';
const OUT = 'C:/Users/MS/Documents/GameDev/fallout2-ce/src/rt/f1compat_gen.h';

// F1 name -> F2 name (plain forward, same argument order).
const MAP = `
obj_dist objectGetDistanceBetween
gsound_play_sfx_file soundPlayFile
stat_level critterGetStat
get_time getTicks
anim_busy animationIsBusy
config_get_double configGetDouble
config_get_value configGetInt
tile_dist tileDistanceBetween
critter_is_dead critterIsDead
item_w_range weaponGetRange
tile_refresh_rect tileWindowRefreshRect
art_ptr_unlock artUnlock
item_get_type itemGetType
register_end reg_anim_end
tile_num_in_direction tileGetTileInDirection
art_id buildFid
config_set_double configSetDouble
dialog_active _gdialogActive
obj_bound objectGetRect
config_set_value configSetInt
elapsed_time getTicksSince
inven_right_hand critterGetItem2
tile_coord tileToScreenXY
roll_random randomBetween
is_within_perception isWithinPerception
gmouse_3d_get_mode gameMouseGetMode
art_ptr_lock artLock
intface_is_enabled interfaceBarEnabled
item_w_subtype weaponGetAttackTypeForHitMode
item_hit_with critterGetWeaponForHitMode
obj_find_first_at objectFindFirstAtElevation
obj_find_next_at objectFindNextAtElevation
mouse_get_position mouseGetPosition
proto_ptr protoGetProto
item_w_curr_ammo ammoGetQuantity
art_exists artExists
object_name objectGetName
critter_get_hits critterGetHitPoints
item_w_max_ammo ammoGetCapacity
register_object_move_to_tile animationRegisterMoveToTile
tile_dir tileGetRotationTo
db_fwriteInt fileWriteInt32
art_frame_max_frame artGetFrameCount
register_begin reg_anim_begin
obj_create_list objectListCreate
obj_delete_list objectListFree
register_object_run_to_tile animationRegisterRunToTile
gmouse_3d_set_mode gameMouseSetMode
art_lock artLockFrameDataReturningSize
register_object_animate animationRegisterAnimate
win_get_rect windowGetRect
item_add_force itemAdd
intface_is_item_right_hand interfaceGetCurrentHand
intface_get_current_item interfaceGetActiveItem
item_w_called_shot critterCanAim
obj_move_to_tile objectSetLocation
rect_min_bound rectUnion
text_font fontSetCurrent
win_get_buf windowGetBuffer
gmouse_set_cursor gameMouseSetCursor
GNW_add_input_buffer enqueueInputEvent
item_remove_mult itemRemove
register_object_call animationRegisterCallback
display_print displayMonitorAddMessage
inven_left_hand critterGetItem1
item_w_anim_weap weaponGetAnimationForHitMode
obj_change_fid objectSetFid
art_frame_data artGetFrameData
art_frame_width artGetWidth
art_frame_length artGetHeight
obj_set_frame objectSetFrame
critter_body_type critterGetBodyType
obj_set_rotation objectSetRotation
obj_is_locked objectIsLocked
mouse_get_buttons mouseGetEvent
config_init configInit
config_load configRead
config_exit configFree
db_fclose fileClose
draw_box bufferDrawRect
draw_line bufferDrawLine
win_draw windowRefresh
win_add windowCreate
win_delete windowDestroy
intface_update_hit_points interfaceRenderHitPoints
art_frame_action_frame artGetActionFrame
register_clear reg_anim_clear
inven_pid_is_carried_ptr objectGetCarriedObjectByPid
item_w_anim_code weaponGetAnimationCode
item_w_can_reload weaponCanBeReloadedWith
gsnd_build_weapon_sfx_name sfxBuildWeaponName
db_fread fileRead
item_w_anim critterGetAnimationForHitMode
item_w_skill_level weaponGetSkillValue
obj_outline_object objectSetOutline
obj_turn_on_outline objectEnableOutline
critter_kill critterKill
object_under_mouse gameMouseGetObjectUnderCursor
action_get_an_object actionPickUp
db_fopen fileOpen
buf_fill bufferFill
gconfig_save gameConfigSave
inven_set_mouse inventorySetCursor
map_disable_bk_processes isoDisable
map_enable_bk_processes isoEnable
skill_name skillGetName
skill_level skillGetValue
inven_worn critterGetArmor
item_w_damage_min_max weaponGetDamageMinMax
proto_name protoGetName
compute_tpf animationComputeTicksPerFrame
pc_flag_off dudeDisableState
inven_pid_quantity_carried objectGetCarriedQuantityByPid
inven_display_msg inventoryRenderItemDescription
intface_update_ac interfaceRenderArmorClass
object_description objectGetDescription
db_fseek fileSeek
art_frame_hot artGetFrameOffsets
item_w_try_reload weaponAttemptReload
make_path_func pathfinderFindPath
scr_ptr scriptGetScript
intface_get_attack interfaceGetCurrentHitMode
obj_remove_outline objectClearOutline
tile_refresh_display tileWindowRefresh
obj_pid_new objectCreateWithPid
config_get_string configGetString
config_set_string configSetString
get_input inputGetInput
pipboy pipboyOpen
editor_design characterEditorShow
automap automapShow
intface_toggle_item_state interfaceCycleItemAction
handle_inventory inventoryOpen
do_options showOptions
display_scroll_up displayMonitorScrollUpOnMouseDown
display_scroll_down displayMonitorScrollDownOnMouseDown
intface_toggle_items interfaceBarSwapHands
trans_cscale blitBufferToBufferStretchTrans
skill_pic skillGetFrmId
mouse_get_rect mouseGetRect
win_refresh_all windowRefreshAll
mouse_show mouseShowCursor
stat_name statGetName
item_w_ammo_pid weaponGetAmmoTypePid
proto_description protoGetDescription
item_name itemGetName
item_ar_female_fid armorGetFemaleFid
item_ar_male_fid armorGetMaleFid
item_w_can_unload weaponCanBeUnloaded
item_w_unload weaponUnload
item_w_reload weaponReload
art_get_name artBuildFilePath
text_curr fontGetCurrent
win_draw_rect windowRefreshRect
win_width windowGetWidth
win_height windowGetHeight
item_w_mp_cost weaponGetActionPointCost
obj_turn_off_outline objectDisableOutline
skill_result skillRoll
critter_adjust_hits critterAdjustHitPoints
register_object_run_to_object animationRegisterRunToObject
register_object_move_to_object animationRegisterMoveToObject
scr_set_objs scriptSetObjects
scr_set_ext_param scriptSetFixedParam
exec_script_proc scriptExecProc
obj_inc_frame objectSetNextFrame
scr_chk_spatials_in scriptsExecSpatialProc
critter_kill_count_type critterGetKillType
win_get_top_win windowGetAtPoint
trait_level traitIsSelected
scr_exec_map_update_scripts scriptsExecMapUpdateProc
intface_update_move_points interfaceRenderActionPoints
game_ui_enable gameUiEnable
action_talk_to actionTalk
action_use_skill_on actionUseSkill
item_w_set_curr_ammo ammoSetQuantity
stat_set_base critterSetBaseStat
stat_recalc_derived critterUpdateDerivedStats
stat_set_bonus critterSetBonusStat
stat_get_bonus critterGetBonusStat
tile_set_center tileSetCenter
mouse_set_position _mouse_set_position
action_skill_use _action_skill_use
gmouse_3d_enable_modes _gmouse_3d_enable_modes
item_d_take_drug _item_d_take_drug
obj_connect _obj_connect
obj_destroy _obj_destroy
critter_is_prone _critter_is_prone
proto_action_can_use _proto_action_can_use
proto_action_can_use_on _proto_action_can_use_on
inven_unwield _inven_unwield
inven_wield _inven_wield
obj_drop _obj_drop
adjust_ac _adjust_ac
protinst_use_item _protinst_use_item
obj_disconnect _obj_disconnect
inven_find_type _inven_find_type
intface_use_item _intface_use_item
win_buffering _win_buffering
ai_danger_source _ai_danger_source
obj_toggle_flat _obj_toggle_flat
is_hit_from_front _is_hit_from_front
obj_offset _obj_offset
obj_blocking_at _obj_blocking_at
dude_stand _dude_stand
db_freadInt fileReadInt32
combat_is_shot_blocked _combat_is_shot_blocked
combatai_msg _combatai_msg
combat_check_bad_shot _combat_check_bad_shot
tile_num_beyond _tile_num_beyond
combat_attack _combat_attack
determine_to_hit _determine_to_hit
make_path _make_path
make_straight_path _make_straight_path
action_loot_container _action_loot_container
gmouse_3d_refresh _gmouse_3d_refresh
combat_give_exps _combat_give_exps
obj_action_can_talk_to _obj_action_can_talk_to
win_move _win_move
action_use_an_object _action_use_an_object
obj_examine _obj_examine
obj_look_at _obj_look_at
`.trim().split('\n').map(l => l.trim().split(/\s+/));

function walk(d, out = []) {
  for (const e of fs.readdirSync(d, { withFileTypes: true })) {
    const p = path.join(d, e.name);
    if (e.isDirectory()) walk(p, out); else if (e.name.endsWith('.h')) out.push(p);
  }
  return out;
}
const decl = new Map();
for (const f of walk(F1)) {
  for (const raw of fs.readFileSync(f, 'utf8').split(/\r?\n/)) {
    const line = raw.replace(/\/\/.*$/, '').trim();
    const m = line.match(/^((?:const\s+|unsigned\s+|struct\s+)*[A-Za-z_][\w:]*[\s\*&]+)([A-Za-z_]\w*)\s*\(([^)]*)\)\s*;$/);
    if (m && !decl.has(m[2])) decl.set(m[2], { ret: m[1].trim(), params: m[3].trim(), file: path.relative(F1, f) });
  }
}
const out = [];
const missing = [];
out.push('// GENERATED by gencompat.js (session scratchpad) -- do not edit by hand; edit the table there.');
out.push('// One inline wrapper per F1 engine function our modules call: the F1 name and signature,');
out.push('// forwarding to the fallout2-ce function that does the same job. Included by f1compat.h.');
out.push('');
for (const [f1, f2] of MAP) {
  const d = decl.get(f1);
  if (!d) { missing.push(f1); continue; }
  let params = d.params === 'void' ? '' : d.params;
  const names = [];
  let i = 0;
  const named = params ? params.split(',').map(p => {
    p = p.trim();
    if (p === '...') return p;
    // has a name? last token is an identifier and there is more than one token
    const m = p.match(/^(.*?[\s\*&])([A-Za-z_]\w*)(\[[^\]]*\])?$/);
    const typeOnly = !m || /^(int|char|bool|float|double|unsigned|long|short|void|const)$/.test(m[2]) || /^[A-Z]\w*$/.test(p) && !p.includes(' ');
    if (!m || typeOnly) { const n = `a${i++}`; names.push(n); return `${p} ${n}`; }
    names.push(m[2]); i++; return p;
  }).join(', ') : '';
  const isVoid = /^void$/.test(d.ret);
  out.push(`// ${f1} (${d.file.replace(/\\/g, '/')})`);
  out.push(`static inline ${d.ret} ${f1}(${named}) { ${isVoid ? '' : 'return '}${f2}(${names.join(', ')}); }`);
}
fs.writeFileSync(OUT, out.join('\n') + '\n');
console.log(`wrote ${MAP.length - missing.length} wrappers; no F1 declaration found for: ${missing.join(' ')}`);
