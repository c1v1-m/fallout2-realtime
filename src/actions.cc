#include "actions.h"

#include <limits.h>
#include <string.h>

#include "animation.h"
#include "art.h"
#include "color.h"
#include "combat.h"
#include "combat_ai.h"
#include "config.h"
#include "critter.h"
#include "debug.h"
#include "display_monitor.h"
#include "game.h"
#include "game_sound.h"
#include "geometry.h"
#include "interface.h"
#include "item.h"
#include "map.h"
#include "memory.h"
#include "object.h"
#include "party_member.h"
#include "perk.h"
#include "proto.h"
#include "proto_instance.h"
#include "proto_types.h"
#include "random.h"
#include "scripts.h"
#include "settings.h"
#include "sfall_config.h"
#include "skill.h"
#include "stat.h"
#include "text_object.h"
#include "tile.h"
#include "trait.h"

// Realtime (fallout1-ce's actions.cc changes, ported).
#include "rt/game/combat_rt.h"
#include "rt/game/game_trace.h"
#include "rt/rt_hooks.h"

namespace fallout {

#define MAX_KNOCKDOWN_DISTANCE 20

typedef enum ScienceRepairTargetType {
    SCIENCE_REPAIR_TARGET_TYPE_DEFAULT,
    SCIENCE_REPAIR_TARGET_TYPE_DUDE,
    SCIENCE_REPAIR_TARGET_TYPE_ANYONE,
} ScienceRepairTargetType;

// 0x5106D0
static bool _action_in_explode = false;

// 0x5106D4
int rotation;

// 0x5106E0
static const int gNormalDeathAnimations[DAMAGE_TYPE_COUNT] = {
    ANIM_DANCING_AUTOFIRE,
    ANIM_SLICED_IN_HALF,
    ANIM_CHARRED_BODY,
    ANIM_CHARRED_BODY,
    ANIM_ELECTRIFY,
    ANIM_FALL_BACK,
    ANIM_BIG_HOLE,
};

// 0x5106FC
static const int gMaximumBloodDeathAnimations[DAMAGE_TYPE_COUNT] = {
    ANIM_CHUNKS_OF_FLESH,
    ANIM_SLICED_IN_HALF,
    ANIM_FIRE_DANCE,
    ANIM_MELTED_TO_NOTHING,
    ANIM_ELECTRIFIED_TO_NOTHING,
    ANIM_FALL_BACK,
    ANIM_EXPLODED_TO_NOTHING,
};

static int actionKnockdown(Object* obj, int* anim, int maxDistance, int rotation, int delay);
static int _action_blood(Object* obj, int anim, int delay);
static int _pick_death(Object* attacker, Object* defender, Object* weapon, int damage, int attackerAnimation, bool hitFromFront);
static int _check_death(Object* obj, int anim, int minViolenceLevel, bool hitFromFront);
static int _internal_destroy(Object* a1, Object* a2);
static void _show_damage_to_object(Object* defender, int damage, int flags, Object* weapon, bool hitFromFront, int knockbackDistance, int knockbackRotation, int attackerAnimation, Object* attacker, int delay);
static int _show_death(Object* obj, int anim);
static int _show_damage_extras(Attack* attack);
static void _show_damage(Attack* attack, int attackerAnimation, int delay, bool splitDefender = false);
static int _action_melee(Attack* attack, int a2);
static int _action_ranged(Attack* attack, int a2);
static int _is_next_to(Object* a1, Object* a2);
static int _action_climb_ladder(Object* a1, Object* a2);
static int _action_use_skill_in_combat_error(Object* critter);
static int _pick_fall(Object* obj, int anim);
static int _report_explosion(Attack* attack, Object* sourceObj);
static int _finished_explosion(Object* a1, Object* a2);
static int _compute_explosion_damage(int min, int max, Object* defender, int* knockbackDistancePtr);
static int _can_talk_to(Object* a1, Object* a2);
static int _talk_to(Object* a1, Object* a2);
static int _report_dmg(Attack* attack, Object* a2);
static int _compute_dmg_damage(int min, int max, Object* obj, int* knockbackDistancePtr, int damageType);

static int hideProjectile(void* a1, void* a2);

// 0x410468
int actionKnockdown(Object* obj, int* anim, int maxDistance, int rotation, int delay)
{
    if (_critter_flag_check(obj->pid, CRITTER_NO_KNOCKBACK)) {
        return -1;
    }

    if (*anim == ANIM_FALL_FRONT) {
        int fid = buildFid(OBJ_TYPE_CRITTER, obj->fid & 0xFFF, *anim, (obj->fid & 0xF000) >> 12, obj->rotation + 1);
        if (!artExists(fid)) {
            *anim = ANIM_FALL_BACK;
        }
    }

    // SFALL: Fix to limit the maximum distance for the knockback animation.
    if (maxDistance > MAX_KNOCKDOWN_DISTANCE) {
        maxDistance = MAX_KNOCKDOWN_DISTANCE;
    }

    int distance;
    int tile;
    for (distance = 1; distance <= maxDistance; distance++) {
        tile = tileGetTileInDirection(obj->tile, rotation, distance);
        if (_obj_blocking_at(obj, tile, obj->elevation) != nullptr) {
            distance--;
            break;
        }

        // CE: Fix to prevent critters (including player) cross an exit grid as
        // a result of knockback. Sfall has similar fix done differently and it
        // affects the player only. This approach is better since it also
        // prevents unreachable (=unlootable) corpses on exit grids.
        if (isExitGridAt(tile, obj->elevation)) {
            distance--;
            break;
        }
    }

    const char* soundEffectName = sfxBuildCharName(obj, *anim, CHARACTER_SOUND_EFFECT_KNOCKDOWN);
    animationRegisterPlaySoundEffect(obj, soundEffectName, delay);

    // TODO: Check, probably step back because we've started with 1?
    distance--;

    if (distance <= 0) {
        tile = obj->tile;
        animationRegisterAnimate(obj, *anim, 0);
    } else {
        tile = tileGetTileInDirection(obj->tile, rotation, distance);
        animationRegisterMoveToTileStraightAndWaitForComplete(obj, tile, obj->elevation, *anim, 0);
    }

    return tile;
}

// 0x410568
int _action_blood(Object* obj, int anim, int delay)
{
    if (settings.preferences.violence_level == VIOLENCE_LEVEL_NONE) {
        return anim;
    }

    int bloodyAnim;
    if (anim == ANIM_FALL_BACK) {
        bloodyAnim = ANIM_FALL_BACK_BLOOD;
    } else if (anim == ANIM_FALL_FRONT) {
        bloodyAnim = ANIM_FALL_FRONT_BLOOD;
    } else {
        return anim;
    }

    int fid = buildFid(OBJ_TYPE_CRITTER, obj->fid & 0xFFF, bloodyAnim, (obj->fid & 0xF000) >> 12, obj->rotation + 1);
    if (artExists(fid)) {
        animationRegisterAnimate(obj, bloodyAnim, delay);
    } else {
        bloodyAnim = anim;
    }

    return bloodyAnim;
}

// 0x41060C
int _pick_death(Object* attacker, Object* defender, Object* weapon, int damage, int attackerAnimation, bool hitFromFront)
{
    int normalViolenceLevelDamageThreshold = 15;
    int maximumBloodViolenceLevelDamageThreshold = 45;

    int damageType = weaponGetDamageType(attacker, weapon);

    if (weapon != nullptr && weapon->pid == PROTO_ID_MOLOTOV_COCKTAIL) {
        normalViolenceLevelDamageThreshold = 5;
        maximumBloodViolenceLevelDamageThreshold = 15;
        damageType = DAMAGE_TYPE_FIRE;
        attackerAnimation = ANIM_FIRE_SINGLE;
    }

    if (attacker == gDude && perkHasRank(attacker, PERK_PYROMANIAC) && damageType == DAMAGE_TYPE_FIRE) {
        normalViolenceLevelDamageThreshold = 1;
        maximumBloodViolenceLevelDamageThreshold = 1;
    }

    if (weapon != nullptr && weaponGetPerk(weapon) == PERK_WEAPON_FLAMEBOY) {
        normalViolenceLevelDamageThreshold /= 3;
        maximumBloodViolenceLevelDamageThreshold /= 3;
    }

    int violenceLevel = settings.preferences.violence_level;

    if (_critter_flag_check(defender->pid, CRITTER_SPECIAL_DEATH)) {
        // In realtime, force the explode regardless of the violence level (fallout1-ce): a low-gore
        // setting would fail the VIOLENCE_LEVEL_NORMAL gate and fall back to a normal death frame these
        // protos have no art for -- an invisible blank corpse. Turn-based keeps vanilla.
        return _check_death(defender, ANIM_EXPLODED_TO_NOTHING,
            combat_rt_enabled() ? VIOLENCE_LEVEL_NONE : VIOLENCE_LEVEL_NORMAL, hitFromFront);
    }

    bool hasBloodyMess = false;
    if (attacker == gDude && traitIsSelected(TRAIT_BLOODY_MESS)) {
        hasBloodyMess = true;
    }

    // NOTE: Original code is slightly different. There are lots of jumps and
    // conditions. It's easier to set the default in advance, rather than catch
    // it with bunch of "else" statements.
    int deathAnim = ANIM_FALL_BACK;

    if ((attackerAnimation == ANIM_THROW_PUNCH && damageType == DAMAGE_TYPE_NORMAL)
        || attackerAnimation == ANIM_KICK_LEG
        || attackerAnimation == ANIM_THRUST_ANIM
        || attackerAnimation == ANIM_SWING_ANIM
        || (attackerAnimation == ANIM_THROW_ANIM && damageType != DAMAGE_TYPE_EXPLOSION)) {
        if (violenceLevel == VIOLENCE_LEVEL_MAXIMUM_BLOOD && hasBloodyMess) {
            deathAnim = ANIM_BIG_HOLE;
        }
    } else {
        if (attackerAnimation == ANIM_FIRE_SINGLE && damageType == DAMAGE_TYPE_NORMAL) {
            if (violenceLevel == VIOLENCE_LEVEL_MAXIMUM_BLOOD) {
                if (hasBloodyMess || maximumBloodViolenceLevelDamageThreshold <= damage) {
                    deathAnim = ANIM_BIG_HOLE;
                }
            }
        } else {
            if (violenceLevel > VIOLENCE_LEVEL_MINIMAL && (hasBloodyMess || normalViolenceLevelDamageThreshold <= damage)) {
                if (violenceLevel > VIOLENCE_LEVEL_NORMAL && (hasBloodyMess || maximumBloodViolenceLevelDamageThreshold <= damage)) {
                    deathAnim = gMaximumBloodDeathAnimations[damageType];
                    if (_check_death(defender, deathAnim, VIOLENCE_LEVEL_MAXIMUM_BLOOD, hitFromFront) != deathAnim) {
                        deathAnim = gNormalDeathAnimations[damageType];
                    }
                } else {
                    deathAnim = gNormalDeathAnimations[damageType];
                }
            }
        }
    }

    if (!hitFromFront && deathAnim == ANIM_FALL_BACK) {
        deathAnim = ANIM_FALL_FRONT;
    }

    return _check_death(defender, deathAnim, VIOLENCE_LEVEL_NONE, hitFromFront);
}

// 0x410814
int _check_death(Object* obj, int anim, int minViolenceLevel, bool hitFromFront)
{
    int fid;

    if (settings.preferences.violence_level >= minViolenceLevel) {
        fid = buildFid(OBJ_TYPE_CRITTER, obj->fid & 0xFFF, anim, (obj->fid & 0xF000) >> 12, obj->rotation + 1);
        if (artExists(fid)) {
            return anim;
        }
    }

    if (hitFromFront) {
        return ANIM_FALL_BACK;
    }

    fid = buildFid(OBJ_TYPE_CRITTER, obj->fid & 0xFFF, ANIM_FALL_FRONT, (obj->fid & 0xF000) >> 12, obj->rotation + 1);
    if (artExists(fid)) {
        return ANIM_FALL_BACK;
    }

    return ANIM_FALL_FRONT;
}

// 0x4108C8
int _internal_destroy(Object* a1, Object* a2)
{
    return _obj_destroy(a2);
}

// TODO: Check very carefully, lots of conditions and jumps.
//
// 0x4108D0
void _show_damage_to_object(Object* defender, int damage, int flags, Object* weapon, bool hitFromFront, int knockbackDistance, int knockbackRotation, int attackerAnimation, Object* attacker, int delay)
{
    int anim;
    int fid;
    const char* sfx_name;

    if (_critter_flag_check(defender->pid, CRITTER_NO_KNOCKBACK)) {
        knockbackDistance = 0;
    }

    anim = FID_ANIM_TYPE(defender->fid);
    if (!_critter_is_prone(defender)) {
        if ((flags & DAM_DEAD) != 0) {
            fid = buildFid(OBJ_TYPE_MISC, 10, 0, 0, 0);
            if (fid == attacker->fid) {
                anim = _check_death(defender, ANIM_EXPLODED_TO_NOTHING, VIOLENCE_LEVEL_MAXIMUM_BLOOD, hitFromFront);
            } else if (attacker->pid == PROTO_ID_0x20001EB) {
                anim = _check_death(defender, ANIM_ELECTRIFIED_TO_NOTHING, VIOLENCE_LEVEL_MAXIMUM_BLOOD, hitFromFront);
            } else if (attacker->fid == FID_0x20001F5) {
                anim = _check_death(defender, attackerAnimation, VIOLENCE_LEVEL_MAXIMUM_BLOOD, hitFromFront);
            } else {
                anim = _pick_death(attacker, defender, weapon, damage, attackerAnimation, hitFromFront);
            }

            if (anim != ANIM_FIRE_DANCE) {
                if (knockbackDistance != 0 && (anim == ANIM_FALL_FRONT || anim == ANIM_FALL_BACK)) {
                    actionKnockdown(defender, &anim, knockbackDistance, knockbackRotation, delay);
                    anim = _action_blood(defender, anim, -1);
                } else {
                    sfx_name = sfxBuildCharName(defender, anim, CHARACTER_SOUND_EFFECT_DIE);
                    animationRegisterPlaySoundEffect(defender, sfx_name, delay);

                    anim = _pick_fall(defender, anim);
                    animationRegisterAnimate(defender, anim, 0);

                    if (anim == ANIM_FALL_FRONT || anim == ANIM_FALL_BACK) {
                        anim = _action_blood(defender, anim, -1);
                    }
                }
            } else {
                fid = buildFid(OBJ_TYPE_CRITTER, defender->fid & 0xFFF, ANIM_FIRE_DANCE, (defender->fid & 0xF000) >> 12, defender->rotation + 1);
                if (artExists(fid)) {
                    sfx_name = sfxBuildCharName(defender, anim, CHARACTER_SOUND_EFFECT_UNUSED);
                    animationRegisterPlaySoundEffect(defender, sfx_name, delay);

                    // SFALL
                    if (explosionEmitsLight()) {
                        // 0xFFFF0002:
                        // - distance: 2
                        // - intensity: 65535
                        //
                        // NOTE: Change intensity to 65536 (which is on par with
                        // `anim_set_check_light_fix` Sfall's hack).
                        animationRegisterSetLightIntensity(defender, 2, 65536, 0);
                    }

                    animationRegisterAnimate(defender, anim, 0);

                    // SFALL
                    if (explosionEmitsLight()) {
                        // 0x00010000:
                        // - distance: 0
                        // - intensity: 1
                        //
                        // NOTE: Change intensity to 0. I guess using 1 was a
                        // workaround for `anim_set_check_light_fix` hack which
                        // requires two upper bytes to be non-zero to override
                        // default behaviour.
                        animationRegisterSetLightIntensity(defender, 0, 0, -1);
                    }

                    int randomDistance = randomBetween(2, 5);
                    int randomRotation = randomBetween(0, 5);

                    // CE: Fix to prevent critters (including player) to cross
                    // an exit grid as a result of fire dance animation. See
                    // `actionKnockdown` for notes.
                    int rotation = randomRotation;
                    int distance = randomDistance;
                    while (true) {
                        int tile = tileGetTileInDirection(defender->tile, (rotation + randomRotation) % ROTATION_COUNT, distance);
                        if (!isExitGridAt(tile, defender->elevation)) {
                            Object* obstacle = nullptr;
                            _make_straight_path(defender, defender->tile, tile, nullptr, &obstacle, 4);
                            if (obstacle == nullptr) {
                                animationRegisterRotateToTile(defender, tile);
                                animationRegisterMoveToTileStraight(defender, tile, defender->elevation, anim, 0);
                                break;
                            }
                        }

                        if (distance > 0) {
                            distance--;
                        } else if (rotation < ROTATION_COUNT) {
                            rotation++;
                            distance = randomDistance;
                        } else {
                            break;
                        }
                    }
                }

                anim = ANIM_BURNED_TO_NOTHING;
                sfx_name = sfxBuildCharName(defender, anim, CHARACTER_SOUND_EFFECT_UNUSED);
                animationRegisterPlaySoundEffect(defender, sfx_name, -1);
                animationRegisterAnimate(defender, anim, 0);
            }
        } else {
            if ((flags & (DAM_KNOCKED_OUT | DAM_KNOCKED_DOWN)) != 0) {
                anim = hitFromFront ? ANIM_FALL_BACK : ANIM_FALL_FRONT;
                sfx_name = sfxBuildCharName(defender, anim, CHARACTER_SOUND_EFFECT_UNUSED);
                animationRegisterPlaySoundEffect(defender, sfx_name, delay);
                if (knockbackDistance != 0) {
                    actionKnockdown(defender, &anim, knockbackDistance, knockbackRotation, 0);
                } else {
                    anim = _pick_fall(defender, anim);
                    animationRegisterAnimate(defender, anim, 0);
                }
            } else if ((flags & DAM_ON_FIRE) != 0 && artExists(buildFid(OBJ_TYPE_CRITTER, defender->fid & 0xFFF, ANIM_FIRE_DANCE, (defender->fid & 0xF000) >> 12, defender->rotation + 1))) {
                animationRegisterAnimate(defender, ANIM_FIRE_DANCE, delay);

                fid = buildFid(OBJ_TYPE_CRITTER, defender->fid & 0xFFF, ANIM_STAND, (defender->fid & 0xF000) >> 12, defender->rotation + 1);
                animationRegisterSetFid(defender, fid, -1);
            } else {
                if (knockbackDistance != 0) {
                    anim = hitFromFront ? ANIM_FALL_BACK : ANIM_FALL_FRONT;
                    actionKnockdown(defender, &anim, knockbackDistance, knockbackRotation, delay);
                    if (anim == ANIM_FALL_BACK) {
                        animationRegisterAnimate(defender, ANIM_BACK_TO_STANDING, -1);
                    } else {
                        animationRegisterAnimate(defender, ANIM_PRONE_TO_STANDING, -1);
                    }
                } else {
                    if (hitFromFront || !artExists(buildFid(OBJ_TYPE_CRITTER, defender->fid & 0xFFF, ANIM_HIT_FROM_BACK, (defender->fid & 0xF000) >> 12, defender->rotation + 1))) {
                        anim = ANIM_HIT_FROM_FRONT;
                    } else {
                        anim = ANIM_HIT_FROM_BACK;
                    }

                    sfx_name = sfxBuildCharName(defender, anim, CHARACTER_SOUND_EFFECT_UNUSED);
                    animationRegisterPlaySoundEffect(defender, sfx_name, delay);

                    // Realtime drives the light hit-react as the defender's OWN animation sequence
                    // (combat_rt, at commit), decoupled from this attacker's swing, so several
                    // attackers can stagger one target without a flinch cutting anyone's swing.
                    // Knockdown/death/fire above still play here. Turn-based unchanged.
                    if (!combat_rt_enabled()) {
                        animationRegisterAnimate(defender, anim, 0);
                    }
                }
            }
        }
    } else {
        if ((flags & DAM_DEAD) != 0 && (defender->data.critter.combat.results & DAM_DEAD) == 0) {
            if (combat_rt_enabled() && _critter_flag_check(defender->pid, CRITTER_SPECIAL_DEATH)) {
                // Only explode-to-nothing death art: a prone (settle-in-place) death would leave a
                // blank invisible body, so register the valid explode death here.
                anim = _check_death(defender, ANIM_EXPLODED_TO_NOTHING, VIOLENCE_LEVEL_NONE, hitFromFront);
                sfx_name = sfxBuildCharName(defender, anim, CHARACTER_SOUND_EFFECT_DIE);
                animationRegisterPlaySoundEffect(defender, sfx_name, delay);
                animationRegisterAnimate(defender, anim, 0);
            } else {
                // Killed where it lies (knocked down by one hit, killed by the next before it stood up --
                // focus-fire in realtime does it all the time): vanilla's blood. ⚠️ NOT A SECOND FALL, as
                // realtime used to believe and skipped it, so these corpses just went still with no blood
                // ("nepřehrává se death animace", 2026-10-29). Every FALL_*_BLOOD art in critter.dat is the
                // body LYING and a pool spreading under it: 3-7 frames, at most a few px of shuffle, its
                // first frame the size the plain fall ends on (scan of all 76, 2026-10-29). His pick
                // "Kaluž krve". A body already turned to its single-frame pose bleeds the same way.
                if (anim == ANIM_FALL_BACK_SF) {
                    anim = ANIM_FALL_BACK;
                } else if (anim == ANIM_FALL_FRONT_SF) {
                    anim = ANIM_FALL_FRONT;
                }
                // Never at the price of the transaction it joins (review 2026-10-29): a body some other live
                // sequence still holds makes the registration fail, and that failure throws away everything
                // else in a shared transaction -- a blast's, a script's damage. That one kill goes without.
                if (!combat_rt_enabled() || !anim_owned_elsewhere(defender)) {
                    anim = _action_blood(defender, anim, delay);
                }
            }
        } else {
            return;
        }
    }

    if (weapon != nullptr) {
        if ((flags & DAM_EXPLODE) != 0) {
            animationRegisterCallbackForced(defender, weapon, (AnimationCallback*)_obj_drop, -1);
            fid = buildFid(OBJ_TYPE_MISC, 10, 0, 0, 0);
            animationRegisterSetFid(weapon, fid, 0);
            animationRegisterAnimateAndHide(weapon, ANIM_STAND, 0);

            sfx_name = sfxBuildWeaponName(WEAPON_SOUND_EFFECT_HIT, weapon, HIT_MODE_RIGHT_WEAPON_PRIMARY, defender);
            animationRegisterPlaySoundEffect(weapon, sfx_name, 0);

            animationRegisterHideObjectForced(weapon);
        } else if ((flags & DAM_DESTROY) != 0) {
            animationRegisterCallbackForced(defender, weapon, (AnimationCallback*)_internal_destroy, -1);
        } else if ((flags & DAM_DROP) != 0) {
            animationRegisterCallbackForced(defender, weapon, (AnimationCallback*)_obj_drop, -1);
        }
    }

    if ((flags & DAM_DEAD) != 0) {
        // TODO: Get rid of casts.
        animationRegisterCallbackForced(defender, (void*)anim, (AnimationCallback*)_show_death, -1);
    }
}

// 0x410E24
int _show_death(Object* obj, int anim)
{
    Rect tempRect;
    Rect dirtyRect;
    int fid;

    objectGetRect(obj, &dirtyRect);
    if (anim < 48 && anim > 63) {
        fid = buildFid(OBJ_TYPE_CRITTER, obj->fid & 0xFFF, anim + 28, (obj->fid & 0xF000) >> 12, obj->rotation + 1);
        if (objectSetFid(obj, fid, &tempRect) == 0) {
            rectUnion(&dirtyRect, &tempRect, &dirtyRect);
        }

        if (objectSetFrame(obj, 0, &tempRect) == 0) {
            rectUnion(&dirtyRect, &tempRect, &dirtyRect);
        }
    }

    if (!_critter_flag_check(obj->pid, CRITTER_FLAT)) {
        obj->flags |= OBJECT_NO_BLOCK;
        if (_obj_toggle_flat(obj, &tempRect) == 0) {
            rectUnion(&dirtyRect, &tempRect, &dirtyRect);
        }
    }

    if (objectDisableOutline(obj, &tempRect) == 0) {
        rectUnion(&dirtyRect, &tempRect, &dirtyRect);
    }

    if (anim >= 30 && anim <= 31 && !_critter_flag_check(obj->pid, CRITTER_SPECIAL_DEATH) && !_critter_flag_check(obj->pid, CRITTER_NO_DROP)) {
        itemDropAll(obj, obj->tile);
    }

    tileWindowRefreshRect(&dirtyRect, obj->elevation);

    return 0;
}

// 0x410FEC
int _show_damage_extras(Attack* attack)
{
    for (int index = 0; index < attack->extrasLength; index++) {
        Object* obj = attack->extras[index];
        if (FID_TYPE(obj->fid) == OBJ_TYPE_CRITTER) {
            // NOTE: Uninline.
            bool hitFromFront = _is_hit_from_front(attack->attacker, obj);
            reg_anim_begin(ANIMATION_REQUEST_RESERVED);
            _register_priority(1);
            int attackerAnimation = critterGetAnimationForHitMode(attack->attacker, attack->hitMode);
            int knockbackRotation = tileGetRotationTo(attack->attacker->tile, obj->tile);
            _show_damage_to_object(obj, attack->extrasDamage[index], attack->extrasFlags[index], attack->weapon, hitFromFront, attack->extrasKnockback[index], knockbackRotation, attackerAnimation, attack->attacker, 0);
            reg_anim_end();
        }
    }

    return 0;
}

// --- REALTIME: the defender's reaction runs as its OWN animation sequence (fallout1-ce). ---
//
// animationIsBusy(obj) reports "busy" for any live sequence that holds a description OWNED by obj --
// including descriptions that already finished -- so vanilla's single attack sequence (the attacker's
// swing, then the defender's hit/death reaction, then the attacker's ANIM_UNPOINT) pins the shooter in
// place for the whole death fall of whoever he just killed. In realtime the defender's reaction is
// registered from combat_rt's COMMIT point instead (action_defender_reaction), which is already
// action-frame synchronised: it lands with the blow, and the attacker's sequence ends with his swing.
static bool action_split_defender_anim()
{
    return combat_rt_enabled();
}

// The defender half of _show_damage, split out so it can be registered either inline (vanilla, inside
// the attacker's sequence) or standalone (realtime, see show_damage_defender_split).
static void show_damage_to_defender(Attack* attack, int attackerAnimation, int delay)
{
    // NOTE: Uninline. (Fallout 2 passes defender first here, unlike _show_damage_extras.)
    bool hitFromFront = _is_hit_from_front(attack->defender, attack->attacker);

    // FID_0x20001F5 keeps the caller's animation code instead of deriving one from the weapon.
    int weaponAnimation = attack->attacker->fid == FID_0x20001F5
        ? attackerAnimation
        : critterGetAnimationForHitMode(attack->attacker, attack->hitMode);
    int knockbackRotation = tileGetRotationTo(attack->attacker->tile, attack->defender->tile);
    _show_damage_to_object(attack->defender, attack->defenderDamage, attack->defenderFlags, attack->weapon, hitFromFront, attack->defenderKnockback, knockbackRotation, weaponAnimation, attack->attacker, delay);
}

// Realtime counterpart of the inline defender block: the reaction as a standalone sequence, the way
// _show_damage_extras already does it. MUST be called outside any open transaction.
static bool show_damage_defender_split(Attack* attack, int attackerAnimation, int delay)
{
    if ((attack->attackerFlags & DAM_HIT) == 0
        || attack->defender == nullptr
        || FID_TYPE(attack->defender->fid) != OBJ_TYPE_CRITTER) {
        return false;
    }

    // _show_damage zeroes the delay once it has spent it on the extras ping; mirror that.
    for (int index = 0; index < attack->extrasLength; index++) {
        if (FID_TYPE(attack->extras[index]->fid) == OBJ_TYPE_CRITTER) {
            delay = 0;
            break;
        }
    }

    reg_anim_begin(ANIMATION_REQUEST_RESERVED);
    _register_priority(1);
    show_damage_to_defender(attack, attackerAnimation, delay);
    // It does NOT take when the defender is still held by a live sequence of its own -- the caller
    // must know: a death whose fall never registered falls back to settling a static corpse.
    return reg_anim_end() == 0;
}

// CE: the explosive projectile (rocket, grenade) the LAST _action_ranged put in the air, and the art
// it turns into when it lands. combat_rt takes it straight after the attack is issued and commits the
// blast when the projectile wears that art. A thrown knife or rock rides here too, with its own item
// art as the "explosion"; a bolt with no blast hands over ACTION_RT_LANDS_ON_ERASE.
static Object* action_rt_explosive_projectile = nullptr;
static int action_rt_explosion_fid = -1;

Object* action_take_explosive_projectile(int* explosionFid)
{
    Object* projectile = action_rt_explosive_projectile;
    if (explosionFid != nullptr) {
        *explosionFid = action_rt_explosion_fid;
    }
    action_rt_explosive_projectile = nullptr;
    action_rt_explosion_fid = -1;
    return projectile;
}

// Public entry for combat_rt: the reactions of every OTHER critter the attack caught (a burst's cone,
// a blast's ring), at the commit. A victim the blow kills or knocks down gets whatever it was doing
// cut first: a fall that cannot register on a busy body snapped splash victims flat with no animation.
void action_extras_reaction(Attack* attack)
{
    if (attack == nullptr || !combat_rt_enabled() || attack->extrasLength <= 0) {
        return;
    }

    for (int index = 0; index < attack->extrasLength; index++) {
        Object* extra = attack->extras[index];
        if (extra == nullptr || FID_TYPE(extra->fid) != OBJ_TYPE_CRITTER) {
            continue;
        }
        if ((attack->extrasFlags[index] & (DAM_DEAD | DAM_KNOCKED_DOWN | DAM_KNOCKED_OUT)) != 0
            && !_critter_is_prone(extra)
            && (extra->data.critter.combat.results & DAM_DEAD) == 0) {
            anim_force_clear(extra);
        }
    }

    _show_damage_extras(attack);
}

// Public entry for combat_rt: the defender's damage reaction (its fall / knockdown / death, the
// weapon-drop callbacks and the _show_death corpse settle) as the DEFENDER's OWN sequence, at the
// moment the attack COMMITS. Returns true if it registered.
bool action_defender_reaction(Attack* attack)
{
    if (attack == nullptr || !combat_rt_enabled()) {
        return false;
    }

    Object* defender = attack->defender;
    if (defender == nullptr || FID_TYPE(defender->fid) != OBJ_TYPE_CRITTER) {
        return false;
    }

    if ((attack->attackerFlags & DAM_HIT) == 0) {
        // MISS -> the cosmetic dodge, on its own sequence and timed from the commit. Skipped for a
        // prone or already-busy defender: pure decoration, never worth cutting something real.
        if (_critter_is_prone(defender) || animationIsBusy(defender) != 0) {
            return false;
        }

        reg_anim_begin(ANIMATION_REQUEST_RESERVED);
        _register_priority(1);
        const char* sfx = sfxBuildCharName(defender, ANIM_DODGE_ANIM, CHARACTER_SOUND_EFFECT_UNUSED);
        animationRegisterPlaySoundEffect(defender, sfx, 0);
        animationRegisterAnimate(defender, ANIM_DODGE_ANIM, 0);
        return reg_anim_end() == 0;
    }

    return show_damage_defender_split(attack, 0, 0);
}

// 0x4110AC
//
// splitDefender: realtime attack paths pass true to SKIP the defender's reaction here (combat_rt
// registers it at the commit). ⚠️ NO PINGS under it either (fallout1-ce 2026-10-16): a ping reserves a
// slot for reactions registered right after the attack, which realtime registers at the commit -- the
// reserved slot was left for whoever registered next, and a ping reaching an unfilled slot ends the
// attacker's WHOLE sequence.
void _show_damage(Attack* attack, int attackerAnimation, int delay, bool splitDefender)
{
    for (int index = 0; index < attack->extrasLength && !splitDefender; index++) {
        Object* object = attack->extras[index];
        if (FID_TYPE(object->fid) == OBJ_TYPE_CRITTER) {
            animationRegisterPing(ANIMATION_REQUEST_RESERVED, delay);
            delay = 0;
        }
    }

    if ((attack->attackerFlags & DAM_HIT) == 0) {
        if ((attack->attackerFlags & DAM_CRITICAL) != 0) {
            _show_damage_to_object(attack->attacker, attack->attackerDamage, attack->attackerFlags, attack->weapon, 1, 0, 0, attackerAnimation, attack->attacker, -1);
        } else if ((attack->attackerFlags & DAM_BACKWASH) != 0) {
            _show_damage_to_object(attack->attacker, attack->attackerDamage, attack->attackerFlags, attack->weapon, 1, 0, 0, attackerAnimation, attack->attacker, -1);
        }
    } else {
        if (attack->defender != nullptr && !splitDefender) {
            if (FID_TYPE(attack->defender->fid) == OBJ_TYPE_CRITTER) {
                show_damage_to_defender(attack, attackerAnimation, delay);
            } else {
                tileGetRotationTo(attack->attacker->tile, attack->defender->tile);
                critterGetAnimationForHitMode(attack->attacker, attack->hitMode);
            }
        }

        if ((attack->attackerFlags & DAM_DUD) != 0) {
            _show_damage_to_object(attack->attacker, attack->attackerDamage, attack->attackerFlags, attack->weapon, 1, 0, 0, attackerAnimation, attack->attacker, -1);
        }
    }
}

// 0x411224
int _action_attack(Attack* attack)
{
    // ⚠️ reg_anim_clear ends ONE sequence and returns, but _check_registry vetoes on EVERY live
    // sequence that lists the object as an owner. An attacker holding TWO kept the second, which
    // refused the very first registration inside _action_melee/_action_ranged and the whole attack
    // disappeared silently (fallout1-ce trace_2026-08-23: 285 x attack-fail). So DRAIN them.
    for (int cleared = 0;; cleared++) {
        int rc = reg_anim_clear(attack->attacker);
        if (rc == -1) {
            break; // no sequence left mentions the attacker
        }
        if (rc == -2) {
            trace_event(TRACE_WARN, "reg-clear-refused", "pid=%d busy=%d blk=%d cleared=%d",
                attack->attacker != nullptr ? attack->attacker->pid : -1,
                animationIsBusy(attack->attacker), anim_busy_blocking(attack->attacker), cleared);
            return -1;
        }
        // Bounded by the sequence table (64 slots); belt-and-braces.
        if (cleared >= 64) {
            trace_event(TRACE_WARN, "reg-clear-loop", "pid=%d",
                attack->attacker != nullptr ? attack->attacker->pid : -1);
            break;
        }
    }

    // ⚠️ In realtime nothing of an attack needs the defender's slot at its START -- the dodge and the
    // hit reaction are registered at the commit, sized to what actually landed. So a reloading or
    // injecting defender keeps at it, a knocked-down one keeps getting up, and a SHOT or a THROW leaves
    // its target walking until the bullet arrives (fallout1-ce 2026-10-06 / 10-17 / 10-19). A melee
    // swing still stops its target: it needs it in reach.
    int targetAnim = critterGetAnimationForHitMode(attack->attacker, attack->hitMode);
    bool shotLeavesTarget = combat_rt_enabled() && (targetAnim >= ANIM_FIRE_SINGLE || targetAnim == ANIM_THROW_ANIM);
    bool keepDefenderReload = combat_rt_enabled()
        && (shotLeavesTarget
            || combat_rt_is_reloading(attack->defender) || combat_rt_is_injecting(attack->defender)
            || combat_rt_in_heavy_reaction(attack->defender));
    if (!keepDefenderReload && reg_anim_clear(attack->defender) == -2) {
        // The defender is in a PRIORITIZED sequence -- almost always its OWN attack swing. Turn-based
        // can't reach here; realtime can, and failing the attack meant "enemies can't hit me while I'm
        // attacking". Leave a committed-to swing running (its damage lands regardless, combat_rt
        // snapshots it); only a HEAVY reaction must own the slot, so for that end the swing -- but
        // never a defender already dying or prone (its sequence carries _show_death; cutting it runs
        // that early and strands the corpse mid-fall).
        if (combat_rt_enabled()) {
            if ((attack->defenderFlags & (DAM_DEAD | DAM_KNOCKED_DOWN | DAM_KNOCKED_OUT | DAM_ON_FIRE)) != 0
                && !_critter_is_prone(attack->defender)
                && (attack->defender->data.critter.combat.results & DAM_DEAD) == 0) {
                anim_force_clear(attack->defender);
            }
        } else {
            return -1;
        }
    }

    for (int index = 0; index < attack->extrasLength; index++) {
        // Same rule for a body caught in the burst: its reload is only a hit's to end, at the commit.
        if (combat_rt_enabled()
            && (shotLeavesTarget
                || combat_rt_is_reloading(attack->extras[index]) || combat_rt_is_injecting(attack->extras[index])
                || combat_rt_in_heavy_reaction(attack->extras[index]))) {
            continue;
        }
        if (reg_anim_clear(attack->extras[index]) == -2) {
            // ⚠️ SAME TRAP ONE LOOP LOWER: "somebody near your target is mid-animation, therefore your
            // burst does not happen at all" (fallout1-ce trace_2026-08-15, 18 dropped bursts in 18 s).
            // Skipping is safe: an extra that keeps its sequence just plays no reaction, and its damage
            // still lands from combat_rt's snapshot.
            if (!combat_rt_enabled()) {
                return -1;
            }
        }
    }

    int anim = critterGetAnimationForHitMode(attack->attacker, attack->hitMode);

    // Scoped so _check_registry names whoever refuses a registration in here.
    anim_debug_attack_scope(true);
    int rc = (anim < ANIM_FIRE_SINGLE && anim != ANIM_THROW_ANIM)
        ? _action_melee(attack, anim)
        : _action_ranged(attack, anim);
    anim_debug_attack_scope(false);
    return rc;
}

// ALWAYS true in realtime: the inline dodge (registered INSIDE THE ATTACKER'S sequence) needs the
// defender's slot -- a refusal aborts the WHOLE attack (53 of the player's attempts in one fight) --
// and it puts the defender into the attacker's prioritized sequence, so the defender's own next
// attack is refused (87 of 87). The dodge is registered at the commit instead (action_defender_reaction).
static bool action_suppress_miss_dodge(Object* defender)
{
    (void)defender;
    return combat_rt_enabled();
}

// 0x4112B4
int _action_melee(Attack* attack, int anim)
{
    int fid;
    Art* art;
    CacheEntry* cache_entry;
    int delay;
    const char* sfx_name;
    char sfx_name_temp[16];

    bool splitDefender = action_split_defender_anim();

    // The attack's WHOLE failure mode funnels through here; name why the slot was not acquired.
    if (reg_anim_begin(ANIMATION_REQUEST_RESERVED) == -1) {
        int openTx = -1;
        int inStop = 0;
        anim_debug_gate(&openTx, &inStop);
        trace_event(TRACE_WARN, "reg-begin-fail", "melee pid=%d openTx=%d inStop=%d",
            attack->attacker != nullptr ? attack->attacker->pid : -1, openTx, inStop);
    }
    _register_priority(1);

    fid = buildFid(OBJ_TYPE_CRITTER, attack->attacker->fid & 0xFFF, anim, (attack->attacker->fid & 0xF000) >> 12, attack->attacker->rotation + 1);
    art = artLock(fid, &cache_entry);
    if (art != nullptr) {
        delay = artGetActionFrame(art);
    } else {
        delay = 0;
    }
    artUnlock(cache_entry);

    tileGetTileInDirection(attack->attacker->tile, attack->attacker->rotation, 1);
    animationRegisterRotateToTile(attack->attacker, attack->defender->tile);

    if (anim != ANIM_THROW_PUNCH && anim != ANIM_KICK_LEG) {
        sfx_name = sfxBuildWeaponName(WEAPON_SOUND_EFFECT_ATTACK, attack->weapon, attack->hitMode, attack->defender);
    } else {
        sfx_name = sfxBuildCharName(attack->attacker, anim, CHARACTER_SOUND_EFFECT_UNUSED);
    }

    strcpy(sfx_name_temp, sfx_name);

    _combatai_msg(attack->attacker, attack, AI_MESSAGE_TYPE_ATTACK, 0);

    if (attack->attackerFlags & 0x0300) {
        animationRegisterPlaySoundEffect(attack->attacker, sfx_name_temp, 0);
        if (anim != ANIM_THROW_PUNCH && anim != ANIM_KICK_LEG) {
            sfx_name = sfxBuildWeaponName(WEAPON_SOUND_EFFECT_HIT, attack->weapon, attack->hitMode, attack->defender);
        } else {
            sfx_name = sfxBuildCharName(attack->attacker, anim, CHARACTER_SOUND_EFFECT_CONTACT);
        }

        strcpy(sfx_name_temp, sfx_name);

        animationRegisterAnimate(attack->attacker, anim, 0);
        animationRegisterPlaySoundEffect(attack->attacker, sfx_name_temp, delay);
        _show_damage(attack, anim, 0, splitDefender);
    } else {
        // Only the attacker's swing when the defender is prone -- or, in realtime, always: the miss's
        // dodge is registered at the commit on the defender's own sequence (action_suppress_miss_dodge).
        if ((attack->defender->data.critter.combat.results & 0x03)
            || action_suppress_miss_dodge(attack->defender)) {
            animationRegisterPlaySoundEffect(attack->attacker, sfx_name_temp, -1);
            animationRegisterAnimate(attack->attacker, anim, 0);
        } else {
            fid = buildFid(OBJ_TYPE_CRITTER, attack->defender->fid & 0xFFF, ANIM_DODGE_ANIM, (attack->defender->fid & 0xF000) >> 12, attack->defender->rotation + 1);
            art = artLock(fid, &cache_entry);
            if (art != nullptr) {
                int dodgeDelay = artGetActionFrame(art);
                artUnlock(cache_entry);

                if (dodgeDelay <= delay) {
                    animationRegisterPlaySoundEffect(attack->attacker, sfx_name_temp, -1);
                    animationRegisterAnimate(attack->attacker, anim, 0);

                    sfx_name = sfxBuildCharName(attack->defender, ANIM_DODGE_ANIM, CHARACTER_SOUND_EFFECT_UNUSED);
                    animationRegisterPlaySoundEffect(attack->defender, sfx_name, delay - dodgeDelay);
                    animationRegisterAnimate(attack->defender, ANIM_DODGE_ANIM, 0);
                } else {
                    sfx_name = sfxBuildCharName(attack->defender, ANIM_DODGE_ANIM, CHARACTER_SOUND_EFFECT_UNUSED);
                    animationRegisterPlaySoundEffect(attack->defender, sfx_name, -1);
                    animationRegisterAnimate(attack->defender, ANIM_DODGE_ANIM, 0);
                    animationRegisterPlaySoundEffect(attack->attacker, sfx_name_temp, dodgeDelay - delay);
                    animationRegisterAnimate(attack->attacker, anim, 0);
                }
            }
        }
    }

    if ((attack->attackerFlags & DAM_HIT) != 0) {
        if ((attack->defenderFlags & DAM_DEAD) == 0) {
            _combatai_msg(attack->attacker, attack, AI_MESSAGE_TYPE_HIT, -1);
        }
    } else {
        _combatai_msg(attack->attacker, attack, AI_MESSAGE_TYPE_MISS, -1);
    }

    if (reg_anim_end() == -1) {
        return -1;
    }

    // NOT registered here in realtime: combat_rt calls action_defender_reaction() and
    // action_extras_reaction() at its commit point, so the reaction lands with the blow.
    if (!splitDefender) {
        _show_damage_extras(attack);
    }

    return 0;
}

// --- REALTIME: a projectile's flight runs as its OWN animation sequence (fallout1-ce 2026-10-16). ---
//
// Vanilla registers everything a projectile does -- appear at the release frame, fly, wear its blast,
// the blast ring, the hide -- inline in the ATTACKER's sequence, ahead of his own closing pose, so he
// stood until his grenade had flown AND gone off. In realtime his sequence holds a must-call at the
// release frame instead, and the callback registers the flight as a sequence of its own
// (action_rt_launch_flight). MUST-call: a sequence cut short still runs it, so the record is always
// released and the projectile always either flies or is put away (action_flight_put_away).
// Unchanged: the commit. combat_rt hurts the target when the projectile wears its landing art.
typedef struct ActionFlight {
    bool used; // a pool slot (realtime) in use
    Object* projectile;
    // Owns the hit sound of a projectile that does not blow up: the gun, or the thrown item itself.
    // Nulled by action_notify_object_freed if it is freed before the launch.
    Object* weapon;
    int origin;
    int tile;
    int elevation;
    bool hit;
    bool thrown;
    bool isGrenade;
    // A grenade's or a rocket's: the tail hides it. It blows up unless the attack was a DAM_DROP.
    bool eraseAtEnd;
    bool explodes;
    int damageType;
    int weaponFid; // a thrown item's own art, worn again when it lands
    int explosionFid;
    char flySfx[32];
    char hitSfx[32];
    Object* neighbors[ROTATION_COUNT]; // the blast ring, created by action_register_flight
    // The target's bark rides the flight too: in the attacker's sequence it would follow his pose.
    Object* barker;
    int barkType;
    int barkHitLocation;
    // The defender. A HIT flies to where it stands at the LAUNCH ("kde jsi při výstřelu").
    Object* target;
} ActionFlight;

#define ACTION_RT_FLIGHT_MAX 16
static ActionFlight action_rt_flights[ACTION_RT_FLIGHT_MAX];

// The projectiles launched and not yet freed (action_rt_launch_flight adds, action_notify_object_freed
// drops); the flight record itself is let go at the launch.
static Object* action_rt_flying[ACTION_RT_FLIGHT_MAX];

// How many thrown or fired projectiles (a rocket, a plasma ball, a grenade, a spear) are visible in the
// air right now -- the self-played runs photograph them, as they do the bullet tracers.
int action_rt_flights_in_air()
{
    int n = 0;
    for (int index = 0; index < ACTION_RT_FLIGHT_MAX; index++) {
        Object* projectile = action_rt_flying[index];
        if (projectile != nullptr && (projectile->flags & OBJECT_HIDDEN) == 0) {
            n++;
        }
    }
    return n;
}

// Null when all are in the air -- _action_ranged then flies it inline, the vanilla way.
static ActionFlight* action_rt_flight_alloc()
{
    for (int index = 0; index < ACTION_RT_FLIGHT_MAX; index++) {
        ActionFlight* fl = &(action_rt_flights[index]);
        if (!fl->used) {
            memset(fl, 0, sizeof(*fl));
            fl->used = true;
            return fl;
        }
    }
    return nullptr;
}

// The flight's descriptions, into the OPEN transaction, exactly as vanilla's _action_ranged
// registered them (Fallout 2: with sfall's explosion light and blast pattern). releaseDelay = frames
// of the attacker's pose before the projectile appears (his action frame inline, 0 for a flight of its
// own). Creates the blast ring. True when the blast took the hit sound (vanilla's l56).
static bool action_register_flight(ActionFlight* fl, int releaseDelay)
{
    animationRegisterUnsetFlag(fl->projectile, OBJECT_HIDDEN, releaseDelay);
    animationRegisterPlaySoundEffect(fl->projectile, fl->flySfx, 0);

    animationRegisterMoveToTileStraight(fl->projectile, fl->tile, fl->elevation, ANIM_WALK, 0);
    int hitSfxDelay = fl->hit ? _make_straight_path(fl->projectile, fl->origin, fl->tile, nullptr, nullptr, 32) - 1 : 0;

    if (fl->explodes) {
        if (fl->isGrenade) {
            animationRegisterSetFid(fl->projectile, fl->weaponFid, -1);
        }

        animationRegisterSetFid(fl->projectile, fl->explosionFid, -1);
        animationRegisterPlaySoundEffect(fl->projectile, fl->hitSfx, 0);

        // SFALL
        if (explosionEmitsLight()) {
            animationRegisterAnimate(fl->projectile, ANIM_STAND, 0);
            // 0xFFFF0008
            // - distance: 8
            // - intensity: 65535
            animationRegisterSetLightIntensity(fl->projectile, 8, 65536, 0);
        } else {
            animationRegisterAnimateAndHide(fl->projectile, ANIM_STAND, 0);
        }

        // SFALL
        int startRotation;
        int endRotation;
        explosionGetPattern(&startRotation, &endRotation);

        for (int rotation = startRotation; rotation < endRotation; rotation++) {
            if (objectCreateWithFidPid(&(fl->neighbors[rotation]), fl->explosionFid, -1) != -1) {
                objectHide(fl->neighbors[rotation], nullptr);

                int adjacentTile = tileGetTileInDirection(fl->tile, rotation, 1);
                objectSetLocation(fl->neighbors[rotation], adjacentTile, fl->projectile->elevation, nullptr);

                int delay;
                if (rotation != ROTATION_NE) {
                    delay = 0;
                } else {
                    if (fl->damageType == DAMAGE_TYPE_PLASMA) {
                        delay = 4;
                    } else {
                        delay = 2;
                    }
                }

                animationRegisterUnsetFlag(fl->neighbors[rotation], OBJECT_HIDDEN, delay);
                animationRegisterAnimateAndHide(fl->neighbors[rotation], ANIM_STAND, 0);
            }
        }

        return true;
    }

    if (!fl->eraseAtEnd && !fl->thrown) {
        animationRegisterHideObjectForced(fl->projectile);
    }

    animationRegisterPlaySoundEffect(fl->weapon != nullptr ? fl->weapon : fl->projectile, fl->hitSfx, hitSfxDelay);

    return false;
}

// The flight's closing descriptions -- vanilla registers them after _show_damage.
static void action_register_flight_tail(ActionFlight* fl)
{
    if (fl->eraseAtEnd) {
        // CE: hide rather than free (see hideProjectile) -- a thrown grenade IS the attack's weapon,
        // and damage_p_proc may still interrogate it.
        animationRegisterCallbackForced(fl->projectile, fl->projectile, hideProjectile, -1);
    } else if (fl->thrown) {
        animationRegisterSetFid(fl->projectile, fl->weaponFid, -1);
    }

    for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
        if (fl->neighbors[rotation] != nullptr) {
            animationRegisterHideObjectForced(fl->neighbors[rotation]);
        }
    }
}

// A flight that cannot register (animationStop under way, the table full): put away the way a cut
// vanilla sequence put it away -- except that a thrown knife or rock drops at the thrower's feet as
// itself, where vanilla left it lying hidden in its projectile art.
static void action_flight_put_away(ActionFlight* fl)
{
    for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
        if (fl->neighbors[rotation] != nullptr) {
            objectDestroy(fl->neighbors[rotation], nullptr);
            fl->neighbors[rotation] = nullptr;
        }
    }

    Object* projectile = fl->projectile;
    if (projectile == nullptr) {
        return;
    }

    if (fl->eraseAtEnd) {
        hideProjectile(projectile, projectile);
    } else if (!fl->thrown) {
        objectDestroy(projectile, nullptr);
    } else {
        Rect rect;
        objectSetFid(projectile, fl->weaponFid, nullptr);
        if (objectShow(projectile, &rect) == 0) {
            tileWindowRefreshRect(&rect, projectile->elevation);
        }
    }
}

// The must-call at the attacker's release frame (or at his sequence's end, if it was cut first).
static int action_rt_launch_flight(void* a1, void* a2)
{
    (void)a2;
    ActionFlight* fl = (ActionFlight*)a1;
    // The projectile, before the flight's registration takes it over (see action_rt_flying).
    Object* launched = fl->projectile;

    // Aimed where the target is NOW: the raise before the release is not a free sidestep any more.
    if (fl->hit && fl->target != nullptr && fl->target->elevation == fl->elevation && fl->target->tile >= 0
        && FID_TYPE(fl->target->fid) == OBJ_TYPE_CRITTER) {
        fl->tile = fl->target->tile;
    }

    bool flying = false;
    if (fl->projectile != nullptr && reg_anim_begin(ANIMATION_REQUEST_RESERVED) == 0) {
        action_register_flight(fl, 0);
        // Where _action_ranged registers it inline: after the landing, before the tail.
        if (fl->barker != nullptr) {
            combatai_msg_at(fl->barker, fl->barkHitLocation, fl->barkType, -1);
        }
        action_register_flight_tail(fl);
        flying = reg_anim_end() == 0;
    }

    trace_event(TRACE_COMBAT, "flight", "pid=%d to=%d from=%d hit=%d explodes=%d thrown=%d %s",
        fl->projectile != nullptr ? fl->projectile->pid : -1, fl->tile, fl->origin,
        fl->hit ? 1 : 0, fl->explodes ? 1 : 0, fl->thrown ? 1 : 0, flying ? "flying" : "put-away");

    if (!flying) {
        action_flight_put_away(fl);
    } else {
        for (int index = 0; index < ACTION_RT_FLIGHT_MAX; index++) {
            if (action_rt_flying[index] == nullptr || action_rt_flying[index] == launched) {
                action_rt_flying[index] = launched;
                break;
            }
        }
    }

    fl->used = false;
    return 0;
}

// objectDestroy (object.cc), just before `obj` is freed: no flight waiting for its launch may keep a
// pointer to it. The gun goes with its owner's inventory, hence the owner test.
void action_notify_object_freed(Object* obj)
{
    if (obj == nullptr) {
        return;
    }

    for (int index = 0; index < ACTION_RT_FLIGHT_MAX; index++) {
        if (action_rt_flying[index] == obj) {
            action_rt_flying[index] = nullptr;
        }
    }

    for (int index = 0; index < ACTION_RT_FLIGHT_MAX; index++) {
        ActionFlight* fl = &(action_rt_flights[index]);
        if (!fl->used) {
            continue;
        }
        if (fl->projectile == obj) {
            fl->projectile = nullptr;
        }
        if (fl->barker == obj) {
            fl->barker = nullptr;
        }
        if (fl->target == obj) {
            fl->target = nullptr;
        }
        if (fl->weapon == obj || (fl->weapon != nullptr && objectGetOwner(fl->weapon) == obj)) {
            fl->weapon = nullptr;
        }
    }
}

// 0x411600
int _action_ranged(Attack* attack, int anim)
{
    // The projectile's flight: inline (vanilla, or a full pool) or, in realtime, a pool record that
    // flies on its own (rtFlight) -- see ActionFlight.
    ActionFlight inlineFlight;
    memset(&inlineFlight, 0, sizeof(inlineFlight));
    ActionFlight* flight = nullptr;
    ActionFlight* rtFlight = nullptr;

    bool splitDefender = action_split_defender_anim();

    // The attack's WHOLE failure mode funnels through here; name why the slot was not acquired.
    if (reg_anim_begin(ANIMATION_REQUEST_RESERVED) == -1) {
        int openTx = -1;
        int inStop = 0;
        anim_debug_gate(&openTx, &inStop);
        trace_event(TRACE_WARN, "reg-begin-fail", "ranged pid=%d openTx=%d inStop=%d",
            attack->attacker != nullptr ? attack->attacker->pid : -1, openTx, inStop);
    }
    _register_priority(1);

    Object* projectile = nullptr;
    Object* replacedWeapon = nullptr;
    int weaponFid = -1;

    // Nothing explosive in the air yet from THIS attack (see action_take_explosive_projectile).
    action_rt_explosive_projectile = nullptr;
    action_rt_explosion_fid = -1;

    Proto* weaponProto;
    Object* weapon = attack->weapon;
    protoGetProto(weapon->pid, &weaponProto);

    int fid = buildFid(OBJ_TYPE_CRITTER, attack->attacker->fid & 0xFFF, anim, (attack->attacker->fid & 0xF000) >> 12, attack->attacker->rotation + 1);
    CacheEntry* artHandle;
    Art* art = artLock(fid, &artHandle);
    int delay = (art != nullptr) ? artGetActionFrame(art) : 0;
    artUnlock(artHandle);

    weaponGetRange(attack->attacker, attack->hitMode);

    int damageType = weaponGetDamageType(attack->attacker, attack->weapon);

    tileGetTileInDirection(attack->attacker->tile, attack->attacker->rotation, 1);

    animationRegisterRotateToTile(attack->attacker, attack->defender->tile);

    bool isGrenade = false;
    if (anim == ANIM_THROW_ANIM) {
        // SFALL
        if (damageType == explosionGetDamageType() || damageType == DAMAGE_TYPE_PLASMA || damageType == DAMAGE_TYPE_EMP) {
            isGrenade = true;
        }
    } else if (!combat_rt_enabled() || !combat_rt_weapon_is_up(attack->attacker)) {
        // RAISING the weapon -- a turn-based ceremony that becomes a per-shot tax when firing
        // continuously (fallout1-ce: 0.45-0.56 s before the swing pose even starts). In realtime it
        // plays on the opening shot of an engagement and is skipped while the weapon is still up.
        animationRegisterAnimate(attack->attacker, ANIM_POINT, -1);
    }

    _combatai_msg(attack->attacker, attack, AI_MESSAGE_TYPE_ATTACK, 0);

    const char* sfx;
    if (((attack->attacker->fid & 0xF000) >> 12) != 0) {
        sfx = sfxBuildWeaponName(WEAPON_SOUND_EFFECT_ATTACK, weapon, attack->hitMode, attack->defender);
    } else {
        sfx = sfxBuildCharName(attack->attacker, anim, CHARACTER_SOUND_EFFECT_UNUSED);
    }
    animationRegisterPlaySoundEffect(attack->attacker, sfx, -1);

    animationRegisterAnimate(attack->attacker, anim, 0);

    if (anim != ANIM_FIRE_CONTINUOUS) {
        if ((attack->attackerFlags & DAM_HIT) != 0 || (attack->attackerFlags & DAM_CRITICAL) == 0) {
            bool l56 = false;

            int projectilePid = weaponGetProjectilePid(weapon);
            Proto* projectileProto;
            if (protoGetProto(projectilePid, &projectileProto) != -1 && projectileProto->fid != -1) {
                if (anim == ANIM_THROW_ANIM) {
                    projectile = weapon;
                    weaponFid = weapon->fid;
                    int weaponFlags = weapon->flags;

                    int leftItemAction;
                    int rightItemAction;
                    interfaceGetItemActions(&leftItemAction, &rightItemAction);

                    itemRemove(attack->attacker, weapon, 1);
                    replacedWeapon = itemReplace(attack->attacker, weapon, weaponFlags & OBJECT_IN_ANY_HAND);
                    objectSetFid(projectile, projectileProto->fid, nullptr);
                    _cAIPrepWeaponItem(attack->attacker, weapon);

                    if (attack->attacker == gDude) {
                        if (replacedWeapon == nullptr) {
                            if ((weaponFlags & OBJECT_IN_LEFT_HAND) != 0) {
                                leftItemAction = INTERFACE_ITEM_ACTION_DEFAULT;
                            } else if ((weaponFlags & OBJECT_IN_RIGHT_HAND) != 0) {
                                rightItemAction = INTERFACE_ITEM_ACTION_DEFAULT;
                            }
                        }
                        interfaceUpdateItems(false, leftItemAction, rightItemAction);
                    }

                    _obj_connect(weapon, attack->attacker->tile, attack->attacker->elevation, nullptr);
                } else {
                    objectCreateWithFidPid(&projectile, projectileProto->fid, -1);
                }

                objectHide(projectile, nullptr);

                // SFALL
                if (explosionEmitsLight() && projectile->lightIntensity == 0) {
                    objectSetLight(projectile, projectileProto->item.lightDistance, projectileProto->item.lightIntensity, nullptr);
                } else {
                    objectSetLight(projectile, 9, projectile->lightIntensity, nullptr);
                }

                int projectileOrigin = _combat_bullet_start(attack->attacker, attack->defender);
                objectSetLocation(projectile, projectileOrigin, attack->attacker->elevation, nullptr);

                int projectileRotation = tileGetRotationTo(attack->attacker->tile, attack->defender->tile);
                objectSetRotation(projectile, projectileRotation, nullptr);

                // In realtime the flight flies on its own (ActionFlight); a full pool falls back to
                // vanilla's inline flight, which only costs the attacker his wait.
                ActionFlight* fl = combat_rt_enabled() ? action_rt_flight_alloc() : nullptr;
                if (fl == nullptr) {
                    fl = &inlineFlight;
                }

                fl->projectile = projectile;
                fl->weapon = weapon;
                fl->origin = projectileOrigin;
                fl->hit = (attack->attackerFlags & DAM_HIT) != 0;
                fl->tile = fl->hit ? attack->defender->tile : attack->tile;
                fl->target = combat_rt_enabled() ? attack->defender : nullptr;
                fl->elevation = attack->defender->elevation;
                fl->thrown = anim == ANIM_THROW_ANIM;
                fl->isGrenade = isGrenade;
                fl->damageType = damageType;
                // SFALL
                fl->eraseAtEnd = isGrenade || damageType == explosionGetDamageType();
                fl->explodes = fl->eraseAtEnd && (attack->attackerFlags & DAM_DROP) == 0;
                fl->weaponFid = weaponFid;
                fl->explosionFid = -1;
                strncpy(fl->flySfx, sfxBuildWeaponName(WEAPON_SOUND_EFFECT_AMMO_FLYING, weapon, attack->hitMode, attack->defender), sizeof(fl->flySfx) - 1);
                strncpy(fl->hitSfx, sfxBuildWeaponName(WEAPON_SOUND_EFFECT_HIT, weapon, attack->hitMode, attack->defender), sizeof(fl->hitSfx) - 1);

                if (fl->explodes) {
                    int explosionFrmId;
                    if (isGrenade) {
                        switch (damageType) {
                        case DAMAGE_TYPE_EMP:
                            explosionFrmId = 2;
                            break;
                        case DAMAGE_TYPE_PLASMA:
                            explosionFrmId = 31;
                            break;
                        default:
                            explosionFrmId = 29;
                            break;
                        }
                    } else {
                        explosionFrmId = 10;
                    }

                    // SFALL
                    int explosionFrmIdOverride = explosionGetFrm();
                    if (explosionFrmIdOverride != -1) {
                        explosionFrmId = explosionFrmIdOverride;
                    }

                    fl->explosionFid = buildFid(OBJ_TYPE_MISC, explosionFrmId, 0, 0, 0);

                    // For combat_rt: the blast goes off the moment this projectile wears that art.
                    action_rt_explosive_projectile = projectile;
                    action_rt_explosion_fid = fl->explosionFid;
                } else if (fl->thrown && !fl->eraseAtEnd && combat_rt_enabled()) {
                    // ⚠️ A THROWN KNIFE OR ROCK HITS WHEN IT ARRIVES TOO: it lands wearing its own item
                    // art again (the flight's tail), so that fid is its "explosion".
                    action_rt_explosive_projectile = projectile;
                    action_rt_explosion_fid = weaponFid;
                } else if (!fl->thrown && !fl->eraseAtEnd && combat_rt_enabled()) {
                    // ⚠️ A LASER BOLT OR A PLASMA BALL HITS WHEN IT ARRIVES TOO (fallout1-ce
                    // 2026-10-18, "mít prostě projektily"): it is erased at the end of its flight, and
                    // that erase is its landing.
                    action_rt_explosive_projectile = projectile;
                    action_rt_explosion_fid = ACTION_RT_LANDS_ON_ERASE;
                }

                if (fl == &inlineFlight) {
                    l56 = action_register_flight(fl, delay);
                } else {
                    // At the release frame, exactly where the inline flight's first description waited.
                    animationRegisterCallbackForced(fl, nullptr, action_rt_launch_flight, delay);
                    rtFlight = fl;
                }
                flight = fl;
                (void)l56;

                delay = 0;
            } else {
                if ((attack->attackerFlags & DAM_HIT) == 0) {
                    Object* defender = attack->defender;
                    if ((defender->data.critter.combat.results & (DAM_KNOCKED_OUT | DAM_KNOCKED_DOWN)) == 0
                        && !action_suppress_miss_dodge(defender)) {
                        animationRegisterAnimate(defender, ANIM_DODGE_ANIM, delay);
                        l56 = true;
                    }
                }
            }
        }
    }

    _show_damage(attack, anim, delay, splitDefender);

    int barkType = -1;
    if ((attack->attackerFlags & DAM_HIT) == 0) {
        barkType = AI_MESSAGE_TYPE_MISS;
    } else {
        if ((attack->defenderFlags & DAM_DEAD) == 0) {
            barkType = AI_MESSAGE_TYPE_HIT;
        }
    }

    if (barkType != -1) {
        if (rtFlight != nullptr) {
            // Said when the projectile lands -- see ActionFlight::barker.
            rtFlight->barker = attack->defender;
            rtFlight->barkType = barkType;
            rtFlight->barkHitLocation = attack->defenderHitLocation;
        } else {
            _combatai_msg(attack->defender, attack, barkType, -1);
        }
    }

    // A flight of its own registers its tail itself, at the launch.
    if (flight == &inlineFlight) {
        action_register_flight_tail(flight);
    }

    if ((attack->attackerFlags & (DAM_KNOCKED_OUT | DAM_KNOCKED_DOWN | DAM_DEAD)) == 0) {
        if (anim == ANIM_THROW_ANIM) {
            bool takeOutAnimationRegistered = false;
            if (replacedWeapon != nullptr) {
                int weaponAnimationCode = weaponGetAnimationCode(replacedWeapon);
                if (weaponAnimationCode != 0) {
                    animationRegisterTakeOutWeapon(attack->attacker, weaponAnimationCode, -1);
                    takeOutAnimationRegistered = true;
                }
            }

            if (!takeOutAnimationRegistered) {
                int fid = buildFid(OBJ_TYPE_CRITTER, attack->attacker->fid & 0xFFF, ANIM_STAND, 0, attack->attacker->rotation + 1);
                animationRegisterSetFid(attack->attacker, fid, -1);
            }
        } else {
            // The LOWER stays in realtime: it is the only transition from the fire pose back to idle,
            // and since the defender's reaction left this sequence it is a short tail, not a death fall.
            animationRegisterAnimate(attack->attacker, ANIM_UNPOINT, -1);
        }
    }

    if (reg_anim_end() == -1) {
        debugPrint("Something went wrong with a ranged attack sequence!\n");
        // No attack, so no blast for combat_rt to wait for -- and the projectile is about to go.
        action_rt_explosive_projectile = nullptr;
        action_rt_explosion_fid = -1;
        if (projectile != nullptr && (isGrenade || damageType == DAMAGE_TYPE_EXPLOSION || anim != ANIM_THROW_ANIM)) {
            objectDestroy(projectile, nullptr);
        }

        if (flight != nullptr) {
            for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
                if (flight->neighbors[rotation] != nullptr) {
                    objectDestroy(flight->neighbors[rotation], nullptr);
                }
            }
        }

        // The aborted transaction dropped its must-call with it, so nothing else frees the record.
        if (rtFlight != nullptr) {
            rtFlight->used = false;
        }

        return -1;
    }

    // In realtime the OTHER critters the attack caught react at the commit (action_extras_reaction).
    if (!splitDefender) {
        _show_damage_extras(attack);
    }

    return 0;
}

// 0x411D68
int _is_next_to(Object* a1, Object* a2)
{
    if (objectGetDistanceBetween(a1, a2) > 1) {
        if (a2 == gDude) {
            MessageListItem messageListItem;
            // You cannot get there.
            messageListItem.num = 2000;
            if (messageListGetItem(&gMiscMessageList, &messageListItem)) {
                displayMonitorAddMessage(messageListItem.text);
            }
        }
        return -1;
    }

    return 0;
}

// 0x411DB4
int _action_climb_ladder(Object* a1, Object* a2)
{
    if (a1 == gDude) {
        int anim = FID_ANIM_TYPE(gDude->fid);
        if (anim == ANIM_WALK || anim == ANIM_RUNNING) {
            reg_anim_clear(gDude);
        }
    }

    int animationRequestOptions;
    int actionPoints;
    if (isInCombat()) {
        animationRequestOptions = ANIMATION_REQUEST_RESERVED;
        actionPoints = a1->data.critter.combat.ap;
    } else {
        animationRequestOptions = ANIMATION_REQUEST_UNRESERVED;
        actionPoints = -1;
    }

    if (a1 == gDude) {
        animationRequestOptions = ANIMATION_REQUEST_RESERVED;
    }

    animationRequestOptions |= ANIMATION_REQUEST_NO_STAND;
    reg_anim_begin(animationRequestOptions);

    int tile = tileGetTileInDirection(a2->tile, ROTATION_SE, 1);
    if (actionPoints != -1 || objectGetDistanceBetween(a1, a2) < 5) {
        animationRegisterMoveToTile(a1, tile, a2->elevation, actionPoints, 0);
    } else {
        animationRegisterRunToTile(a1, tile, a2->elevation, actionPoints, 0);
    }

    animationRegisterCallbackForced(a1, a2, (AnimationCallback*)_is_next_to, -1);
    animationRegisterRotateToTile(a1, a2->tile);
    animationRegisterCallbackForced(a1, a2, (AnimationCallback*)_check_scenery_ap_cost, -1);

    int weaponAnimationCode = (a1->fid & 0xF000) >> 12;
    if (weaponAnimationCode != 0) {
        const char* puttingAwaySfx = sfxBuildCharName(a1, ANIM_PUT_AWAY, CHARACTER_SOUND_EFFECT_UNUSED);
        animationRegisterPlaySoundEffect(a1, puttingAwaySfx, -1);
        animationRegisterAnimate(a1, ANIM_PUT_AWAY, 0);
    }

    const char* climbingSfx = sfxBuildCharName(a1, ANIM_CLIMB_LADDER, CHARACTER_SOUND_EFFECT_UNUSED);
    animationRegisterPlaySoundEffect(a1, climbingSfx, -1);
    animationRegisterAnimate(a1, ANIM_CLIMB_LADDER, 0);
    animationRegisterCallback(a1, a2, (AnimationCallback*)_obj_use, -1);

    if (weaponAnimationCode != 0) {
        animationRegisterTakeOutWeapon(a1, weaponAnimationCode, -1);
    }

    return reg_anim_end();
}

// 0x411F2C
int _action_use_an_item_on_object(Object* user, Object* targetObj, Object* item)
{
    Proto* proto = nullptr;
    int type = FID_TYPE(targetObj->fid);
    int sceneryType = -1;
    if (type == OBJ_TYPE_SCENERY) {
        if (protoGetProto(targetObj->pid, &proto) == -1) {
            return -1;
        }

        sceneryType = proto->scenery.type;
    }

    if (sceneryType != SCENERY_TYPE_LADDER_UP || item != nullptr) {
        if (user == gDude) {
            int anim = FID_ANIM_TYPE(gDude->fid);
            if (anim == ANIM_WALK || anim == ANIM_RUNNING) {
                reg_anim_clear(gDude);
            }
        }

        int animationRequestOptions;
        int actionPoints;
        if (isInCombat()) {
            animationRequestOptions = ANIMATION_REQUEST_RESERVED;
            actionPoints = user->data.critter.combat.ap;
        } else {
            animationRequestOptions = ANIMATION_REQUEST_UNRESERVED;
            actionPoints = -1;
        }

        if (user == gDude) {
            animationRequestOptions = ANIMATION_REQUEST_RESERVED;
        }

        reg_anim_begin(animationRequestOptions);

        if (actionPoints != -1 || objectGetDistanceBetween(user, targetObj) < 5) {
            animationRegisterMoveToObject(user, targetObj, actionPoints, 0);
        } else {
            animationRegisterRunToObject(user, targetObj, -1, 0);
        }

        animationRegisterCallbackForced(user, targetObj, (AnimationCallback*)_is_next_to, -1);

        if (item == nullptr) {
            animationRegisterCallback(user, targetObj, (AnimationCallback*)_check_scenery_ap_cost, -1);
        }

        int weaponAnimCode = (user->fid & 0xF000) >> 12;
        if (weaponAnimCode != 0) {
            const char* sfx = sfxBuildCharName(user, ANIM_PUT_AWAY, CHARACTER_SOUND_EFFECT_UNUSED);
            animationRegisterPlaySoundEffect(user, sfx, -1);
            animationRegisterAnimate(user, ANIM_PUT_AWAY, 0);
        }

        int anim;
        int objectType = FID_TYPE(targetObj->fid);
        if (objectType == OBJ_TYPE_CRITTER && _critter_is_prone(targetObj)) {
            anim = ANIM_MAGIC_HANDS_GROUND;
        } else if (objectType == OBJ_TYPE_SCENERY && (proto->scenery.extendedFlags & 0x01) != 0) {
            anim = ANIM_MAGIC_HANDS_GROUND;
        } else {
            anim = ANIM_MAGIC_HANDS_MIDDLE;
        }

        if (sceneryType != SCENERY_TYPE_STAIRS && item == nullptr) {
            animationRegisterAnimate(user, anim, -1);
        }

        if (item != nullptr) {
            // TODO: Get rid of cast.
            animationRegisterCallback3(user, targetObj, item, (AnimationCallback3*)_obj_use_item_on, -1);
        } else {
            animationRegisterCallback(user, targetObj, (AnimationCallback*)_obj_use, -1);
        }

        if (weaponAnimCode != 0) {
            animationRegisterTakeOutWeapon(user, weaponAnimCode, -1);
        }

        return reg_anim_end();
    }

    return _action_climb_ladder(user, targetObj);
}

// 0x412114
int _action_use_an_object(Object* user, Object* targetObj)
{
    return _action_use_an_item_on_object(user, targetObj, nullptr);
}

// 0x412134
int actionPickUp(Object* critter, Object* item)
{
    if (FID_TYPE(item->fid) != OBJ_TYPE_ITEM) {
        return -1;
    }

    if (critter == gDude) {
        int animationCode = FID_ANIM_TYPE(gDude->fid);
        if (animationCode == ANIM_WALK || animationCode == ANIM_RUNNING) {
            reg_anim_clear(gDude);
        }
    }

    if (isInCombat()) {
        reg_anim_begin(ANIMATION_REQUEST_RESERVED);
        animationRegisterMoveToObject(critter, item, critter->data.critter.combat.ap, 0);
    } else {
        reg_anim_begin(critter == gDude ? ANIMATION_REQUEST_RESERVED : ANIMATION_REQUEST_UNRESERVED);
        if (objectGetDistanceBetween(critter, item) >= 5) {
            animationRegisterRunToObject(critter, item, -1, 0);
        } else {
            animationRegisterMoveToObject(critter, item, -1, 0);
        }
    }

    animationRegisterCallbackForced(critter, item, (AnimationCallback*)_is_next_to, -1);
    animationRegisterCallback(critter, item, (AnimationCallback*)_check_scenery_ap_cost, -1);

    Proto* itemProto;
    protoGetProto(item->pid, &itemProto);

    if (itemProto->item.type != ITEM_TYPE_CONTAINER || _proto_action_can_pickup(item->pid)) {
        animationRegisterAnimate(critter, ANIM_MAGIC_HANDS_GROUND, 0);

        int fid = buildFid(OBJ_TYPE_CRITTER, critter->fid & 0xFFF, ANIM_MAGIC_HANDS_GROUND, (critter->fid & 0xF000) >> 12, critter->rotation + 1);

        int actionFrame;
        CacheEntry* cacheEntry;
        Art* art = artLock(fid, &cacheEntry);
        if (art != nullptr) {
            actionFrame = artGetActionFrame(art);
        } else {
            actionFrame = -1;
        }

        char sfx[16];
        if (artCopyFileName(FID_TYPE(item->fid), item->fid & 0xFFF, sfx) == 0) {
            // NOTE: looks like they copy sfx one more time, what for?
            animationRegisterPlaySoundEffect(item, sfx, actionFrame);
        }

        animationRegisterCallback(critter, item, (AnimationCallback*)_obj_pickup, actionFrame);
    } else {
        int weaponAnimationCode = (critter->fid & 0xF000) >> 12;
        if (weaponAnimationCode != 0) {
            const char* sfx = sfxBuildCharName(critter, ANIM_PUT_AWAY, CHARACTER_SOUND_EFFECT_UNUSED);
            animationRegisterPlaySoundEffect(critter, sfx, -1);
            animationRegisterAnimate(critter, ANIM_PUT_AWAY, -1);
        }

        // ground vs middle animation
        int anim = (itemProto->item.data.container.openFlags & 0x01) == 0
            ? ANIM_MAGIC_HANDS_MIDDLE
            : ANIM_MAGIC_HANDS_GROUND;
        animationRegisterAnimate(critter, anim, 0);

        int fid = buildFid(OBJ_TYPE_CRITTER, critter->fid & 0xFFF, anim, 0, critter->rotation + 1);

        int actionFrame;
        CacheEntry* cacheEntry;
        Art* art = artLock(fid, &cacheEntry);
        if (art == nullptr) {
            actionFrame = artGetActionFrame(art);
            artUnlock(cacheEntry);
        } else {
            actionFrame = -1;
        }

        if (item->frame != 1) {
            animationRegisterCallback(critter, item, (AnimationCallback*)_obj_use_container, actionFrame);
        }

        if (weaponAnimationCode != 0) {
            animationRegisterTakeOutWeapon(critter, weaponAnimationCode, -1);
        }

        if (item->frame == 0 || item->frame == 1) {
            animationRegisterCallback(critter, item, (AnimationCallback*)scriptsRequestLooting, -1);
        }
    }

    return reg_anim_end();
}

// TODO: Looks like the name is a little misleading, container can only be a
// critter, which is enforced by this function as well as at the call sites.
// Used to loot corpses, so probably should be something like actionLootCorpse.
// Check if it can be called with a living critter.
//
// 0x4123E8
int _action_loot_container(Object* critter, Object* container)
{
    if (FID_TYPE(container->fid) != OBJ_TYPE_CRITTER) {
        return -1;
    }

    // SFALL: Fix for trying to loot corpses with the "NoSteal" flag.
    if (_critter_flag_check(container->pid, CRITTER_NO_STEAL)) {
        return -1;
    }

    if (critter == gDude) {
        int anim = FID_ANIM_TYPE(gDude->fid);
        if (anim == ANIM_WALK || anim == ANIM_RUNNING) {
            reg_anim_clear(gDude);
        }
    }

    if (isInCombat()) {
        reg_anim_begin(ANIMATION_REQUEST_RESERVED);
        animationRegisterMoveToObject(critter, container, critter->data.critter.combat.ap, 0);
    } else {
        reg_anim_begin(critter == gDude ? ANIMATION_REQUEST_RESERVED : ANIMATION_REQUEST_UNRESERVED);

        if (objectGetDistanceBetween(critter, container) < 5) {
            animationRegisterMoveToObject(critter, container, -1, 0);
        } else {
            animationRegisterRunToObject(critter, container, -1, 0);
        }
    }

    animationRegisterCallbackForced(critter, container, (AnimationCallback*)_is_next_to, -1);
    animationRegisterCallback(critter, container, (AnimationCallback*)_check_scenery_ap_cost, -1);
    animationRegisterCallback(critter, container, (AnimationCallback*)scriptsRequestLooting, -1);
    return reg_anim_end();
}

// 0x4124E0
int _action_skill_use(int skill)
{
    if (skill == SKILL_SNEAK) {
        reg_anim_clear(gDude);
        dudeToggleState(DUDE_STATE_SNEAKING);
        return 0;
    }

    return -1;
}

// NOTE: Inlined.
//
// 0x412500
static int _action_use_skill_in_combat_error(Object* critter)
{
    MessageListItem messageListItem;

    if (critter == gDude) {
        messageListItem.num = 902;
        if (messageListGetItem(&gProtoMessageList, &messageListItem) == 1) {
            displayMonitorAddMessage(messageListItem.text);
        }
    }

    return -1;
}

// skill_use
// 0x41255C
int actionUseSkill(Object* user, Object* target, int skill)
{
    switch (skill) {
    case SKILL_FIRST_AID:
    case SKILL_DOCTOR:
        if (isInCombat()) {
            // NOTE: Uninline.
            return _action_use_skill_in_combat_error(user);
        }

        if (PID_TYPE(target->pid) != OBJ_TYPE_CRITTER) {
            return -1;
        }
        break;
    case SKILL_LOCKPICK:
        if (isInCombat()) {
            // NOTE: Uninline.
            return _action_use_skill_in_combat_error(user);
        }

        if (PID_TYPE(target->pid) != OBJ_TYPE_ITEM && PID_TYPE(target->pid) != OBJ_TYPE_SCENERY) {
            return -1;
        }

        break;
    case SKILL_STEAL:
        if (isInCombat()) {
            // NOTE: Uninline.
            return _action_use_skill_in_combat_error(user);
        }

        if (PID_TYPE(target->pid) != OBJ_TYPE_ITEM && PID_TYPE(target->pid) != OBJ_TYPE_CRITTER) {
            return -1;
        }

        if (target == user) {
            return -1;
        }

        break;
    case SKILL_TRAPS:
        if (isInCombat()) {
            // NOTE: Uninline.
            return _action_use_skill_in_combat_error(user);
        }

        if (PID_TYPE(target->pid) == OBJ_TYPE_CRITTER) {
            return -1;
        }

        break;
    case SKILL_SCIENCE:
    case SKILL_REPAIR:
        if (isInCombat()) {
            // NOTE: Uninline.
            return _action_use_skill_in_combat_error(user);
        }

        if (PID_TYPE(target->pid) != OBJ_TYPE_CRITTER) {
            break;
        }

        if (critterGetKillType(target) == KILL_TYPE_ROBOT) {
            break;
        }

        if (critterGetKillType(target) == KILL_TYPE_BRAHMIN
            && skill == SKILL_SCIENCE) {
            break;
        }

        // SFALL: Science on critters patch.
        if (1) {
            int targetType = SCIENCE_REPAIR_TARGET_TYPE_DEFAULT;
            configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_SCIENCE_REPAIR_TARGET_TYPE_KEY, &targetType);
            if (targetType == SCIENCE_REPAIR_TARGET_TYPE_DUDE) {
                if (target == gDude) {
                    break;
                }
            } else if (targetType == SCIENCE_REPAIR_TARGET_TYPE_ANYONE) {
                break;
            }
        }

        return -1;
    case SKILL_SNEAK:
        dudeToggleState(DUDE_STATE_SNEAKING);
        return 0;
    default:
        debugPrint("\nskill_use: invalid skill used.");
    }

    // Performer is either dude, or party member who's best at the specified
    // skill in entire party, and this skill is his/her own best.
    Object* performer = gDude;

    if (user == gDude) {
        Object* partyMember = partyMemberGetBestInSkill(skill);

        if (partyMember == gDude) {
            partyMember = nullptr;
        }

        // Only dude can perform stealing.
        if (skill == SKILL_STEAL) {
            partyMember = nullptr;
        }

        if (partyMember != nullptr) {
            if (partyMemberGetBestSkill(partyMember) != skill) {
                partyMember = nullptr;
            }
        }

        if (partyMember != nullptr) {
            performer = partyMember;
            int anim = FID_ANIM_TYPE(partyMember->fid);
            if (anim != ANIM_WALK && anim != ANIM_RUNNING) {
                if (anim != ANIM_STAND) {
                    performer = gDude;
                    partyMember = nullptr;
                }
            } else {
                reg_anim_clear(partyMember);
            }
        }

        if (partyMember != nullptr) {
            bool isDude = false;
            if (objectGetDistanceBetween(gDude, target) <= 1) {
                isDude = true;
            }

            char* msg = skillsGetGenericResponse(partyMember, isDude);

            Rect rect;
            if (textObjectAdd(partyMember, msg, 101, _colorTable[32747], _colorTable[0], &rect) == 0) {
                tileWindowRefreshRect(&rect, gElevation);
            }

            if (isDude) {
                performer = gDude;
                partyMember = nullptr;
            }
        }

        if (partyMember == nullptr) {
            int anim = FID_ANIM_TYPE(performer->fid);
            if (anim == ANIM_WALK || anim == ANIM_RUNNING) {
                reg_anim_clear(performer);
            }
        }
    }

    if (isInCombat()) {
        reg_anim_begin(ANIMATION_REQUEST_RESERVED);
        animationRegisterMoveToObject(performer, target, performer->data.critter.combat.ap, 0);
    } else {
        reg_anim_begin(user == gDude ? ANIMATION_REQUEST_RESERVED : ANIMATION_REQUEST_UNRESERVED);
        if (target != gDude) {
            if (objectGetDistanceBetween(performer, target) >= 5) {
                animationRegisterRunToObject(performer, target, -1, 0);
            } else {
                animationRegisterMoveToObject(performer, target, -1, 0);
            }
        }
    }

    animationRegisterCallbackForced(performer, target, (AnimationCallback*)_is_next_to, -1);

    int anim = (FID_TYPE(target->fid) == OBJ_TYPE_CRITTER && _critter_is_prone(target)) ? ANIM_MAGIC_HANDS_GROUND : ANIM_MAGIC_HANDS_MIDDLE;
    int fid = buildFid(OBJ_TYPE_CRITTER, performer->fid & 0xFFF, anim, 0, performer->rotation + 1);

    CacheEntry* artHandle;
    Art* art = artLock(fid, &artHandle);
    if (art != nullptr) {
        artGetActionFrame(art);
        artUnlock(artHandle);
    }

    animationRegisterAnimate(performer, anim, -1);
    // TODO: Get rid of casts.
    animationRegisterCallback3(performer, target, (void*)skill, (AnimationCallback3*)_obj_use_skill_on, -1);
    return reg_anim_end();
}

// 0x412BC4
bool _is_hit_from_front(Object* a1, Object* a2)
{
    int diff = a1->rotation - a2->rotation;
    if (diff < 0) {
        diff = -diff;
    }

    return diff != 0 && diff != 1 && diff != 5;
}

// 0x412BEC
bool _can_see(Object* a1, Object* a2)
{
    int diff;

    diff = a1->rotation - tileGetRotationTo(a1->tile, a2->tile);
    if (diff < 0) {
        diff = -diff;
    }

    return diff == 0 || diff == 1 || diff == 5;
}

// looks like it tries to change fall animation depending on object's current rotation
// 0x412C1C
int _pick_fall(Object* obj, int anim)
{
    int i;
    int rotation;
    int tile_num;
    int fid;

    if (anim == ANIM_FALL_FRONT) {
        rotation = obj->rotation;
        for (i = 1; i < 3; i++) {
            tile_num = tileGetTileInDirection(obj->tile, rotation, i);
            if (_obj_blocking_at(obj, tile_num, obj->elevation) != nullptr) {
                anim = ANIM_FALL_BACK;
                break;
            }
        }
    } else if (anim == ANIM_FALL_BACK) {
        rotation = (obj->rotation + 3) % ROTATION_COUNT;
        for (i = 1; i < 3; i++) {
            tile_num = tileGetTileInDirection(obj->tile, rotation, i);
            if (_obj_blocking_at(obj, tile_num, obj->elevation) != nullptr) {
                anim = ANIM_FALL_FRONT;
                break;
            }
        }
    }

    if (anim == ANIM_FALL_FRONT) {
        fid = buildFid(OBJ_TYPE_CRITTER, obj->fid & 0xFFF, ANIM_FALL_FRONT, (obj->fid & 0xF000) >> 12, obj->rotation + 1);
        if (!artExists(fid)) {
            anim = ANIM_FALL_BACK;
        }
    }

    return anim;
}

// 0x412CE4
bool _action_explode_running()
{
    return _action_in_explode;
}

// action_explode
// 0x412CF4
int actionExplode(int tile, int elevation, int minDamage, int maxDamage, Object* sourceObj, bool animate)
{
    if (animate && _action_in_explode) {
        return -2;
    }

    Attack* attack = (Attack*)internal_malloc(sizeof(*attack));
    if (attack == nullptr) {
        return -1;
    }

    Object* explosion;
    int fid = buildFid(OBJ_TYPE_MISC, 10, 0, 0, 0);
    if (objectCreateWithFidPid(&explosion, fid, -1) == -1) {
        internal_free(attack);
        return -1;
    }

    objectHide(explosion, nullptr);
    explosion->flags |= OBJECT_NO_SAVE;

    objectSetLocation(explosion, tile, elevation, nullptr);

    Object* adjacentExplosions[ROTATION_COUNT];
    for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
        int fid = buildFid(OBJ_TYPE_MISC, 10, 0, 0, 0);
        if (objectCreateWithFidPid(&(adjacentExplosions[rotation]), fid, -1) == -1) {
            while (--rotation >= 0) {
                objectDestroy(adjacentExplosions[rotation], nullptr);
            }

            objectDestroy(explosion, nullptr);
            internal_free(attack);
            return -1;
        }

        objectHide(adjacentExplosions[rotation], nullptr);
        adjacentExplosions[rotation]->flags |= OBJECT_NO_SAVE;

        int adjacentTile = tileGetTileInDirection(tile, rotation, 1);
        objectSetLocation(adjacentExplosions[rotation], adjacentTile, elevation, nullptr);
    }

    Object* critter = _obj_blocking_at(nullptr, tile, elevation);
    if (critter != nullptr) {
        if (FID_TYPE(critter->fid) != OBJ_TYPE_CRITTER || (critter->data.critter.combat.results & DAM_DEAD) != 0) {
            critter = nullptr;
        }
    }

    attackInit(attack, explosion, critter, HIT_MODE_PUNCH, HIT_LOCATION_TORSO);

    attack->tile = tile;
    attack->attackerFlags = DAM_HIT;

    gameUiDisable(1);

    if (critter != nullptr) {
        if (reg_anim_clear(critter) == -2) {
            debugPrint("Cannot clear target's animation for action_explode!\n");
        }
        attack->defenderDamage = _compute_explosion_damage(minDamage, maxDamage, critter, &(attack->defenderKnockback));
    }

    _compute_explosion_on_extras(attack, 0, 0, 1);

    for (int index = 0; index < attack->extrasLength; index++) {
        Object* critter = attack->extras[index];
        if (reg_anim_clear(critter) == -2) {
            debugPrint("Cannot clear extra's animation for action_explode!\n");
        }

        attack->extrasDamage[index] = _compute_explosion_damage(minDamage, maxDamage, critter, &(attack->extrasKnockback[index]));
    }

    attackComputeDeathFlags(attack);

    if (animate) {
        _action_in_explode = true;

        reg_anim_begin(ANIMATION_REQUEST_RESERVED);
        _register_priority(1);
        animationRegisterPlaySoundEffect(explosion, "whn1xxx1", 0);
        animationRegisterUnsetFlag(explosion, OBJECT_HIDDEN, 0);
        animationRegisterAnimateAndHide(explosion, ANIM_STAND, 0);
        _show_damage(attack, 0, 1);

        for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
            animationRegisterUnsetFlag(adjacentExplosions[rotation], OBJECT_HIDDEN, 0);
            animationRegisterAnimateAndHide(adjacentExplosions[rotation], ANIM_STAND, 0);
        }

        animationRegisterCallbackForced(explosion, nullptr, (AnimationCallback*)_combat_explode_scenery, -1);
        animationRegisterHideObjectForced(explosion);

        for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
            animationRegisterHideObjectForced(adjacentExplosions[rotation]);
        }

        animationRegisterCallbackForced(attack, sourceObj, (AnimationCallback*)_report_explosion, -1);
        animationRegisterCallbackForced(nullptr, nullptr, (AnimationCallback*)_finished_explosion, -1);
        if (reg_anim_end() == -1) {
            _action_in_explode = false;

            objectDestroy(explosion, nullptr);

            for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
                objectDestroy(adjacentExplosions[rotation], nullptr);
            }

            internal_free(attack);

            gameUiEnable();
            return -1;
        }

        _show_damage_extras(attack);
    } else {
        if (critter != nullptr) {
            if ((attack->defenderFlags & DAM_DEAD) != 0) {
                critterKill(critter, -1, false);
            }
        }

        for (int index = 0; index < attack->extrasLength; index++) {
            if ((attack->extrasFlags[index] & DAM_DEAD) != 0) {
                critterKill(attack->extras[index], -1, false);
            }
        }

        _report_explosion(attack, sourceObj);

        _combat_explode_scenery(explosion, nullptr);

        objectDestroy(explosion, nullptr);

        for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
            objectDestroy(adjacentExplosions[rotation], nullptr);
        }
    }

    return 0;
}

// 0x413144
int _report_explosion(Attack* attack, Object* sourceObj)
{
    bool mainTargetWasDead;
    if (attack->defender != nullptr) {
        mainTargetWasDead = (attack->defender->data.critter.combat.results & DAM_DEAD) != 0;
    } else {
        mainTargetWasDead = false;
    }

    bool extrasWasDead[6];
    for (int index = 0; index < attack->extrasLength; index++) {
        extrasWasDead[index] = (attack->extras[index]->data.critter.combat.results & DAM_DEAD) != 0;
    }

    attackComputeDeathFlags(attack);
    _combat_display(attack);
    _apply_damage(attack, false);

    Object* anyDefender = nullptr;
    int xp = 0;
    if (sourceObj != nullptr) {
        if (attack->defender != nullptr && attack->defender != sourceObj) {
            if ((attack->defender->data.critter.combat.results & DAM_DEAD) != 0) {
                if (sourceObj == gDude && !mainTargetWasDead) {
                    xp += critterGetExp(attack->defender);
                }
            } else {
                _critter_set_who_hit_me(attack->defender, sourceObj);
                anyDefender = attack->defender;
            }
        }

        for (int index = 0; index < attack->extrasLength; index++) {
            Object* critter = attack->extras[index];
            if (critter != sourceObj) {
                if ((critter->data.critter.combat.results & DAM_DEAD) != 0) {
                    if (sourceObj == gDude && !extrasWasDead[index]) {
                        xp += critterGetExp(critter);
                    }
                } else {
                    _critter_set_who_hit_me(critter, sourceObj);

                    if (anyDefender == nullptr) {
                        anyDefender = critter;
                    }
                }
            }
        }

        if (anyDefender != nullptr) {
            if (!isInCombat()) {
                CombatStartData combat;
                combat.attacker = anyDefender;
                combat.defender = sourceObj;
                combat.actionPointsBonus = 0;
                combat.accuracyBonus = 0;
                combat.damageBonus = 0;
                combat.minDamage = 0;
                combat.maxDamage = INT_MAX;
                combat.overrideAttackResults = 0;
                scriptsRequestCombat(&combat);
            }
        }
    }

    internal_free(attack);
    gameUiEnable();

    if (sourceObj == gDude) {
        _combat_give_exps(xp);
    }

    return 0;
}

// 0x4132C0
int _finished_explosion(Object* a1, Object* a2)
{
    _action_in_explode = false;
    return 0;
}

// calculate explosion damage applying threshold and resistances
// 0x4132CC
int _compute_explosion_damage(int min, int max, Object* defender, int* knockbackDistancePtr)
{
    int damage = randomBetween(min, max) - critterGetStat(defender, STAT_DAMAGE_THRESHOLD_EXPLOSION);
    if (damage > 0) {
        damage -= critterGetStat(defender, STAT_DAMAGE_RESISTANCE_EXPLOSION) * damage / 100;
    }

    if (damage < 0) {
        damage = 0;
    }

    if (knockbackDistancePtr != nullptr) {
        if ((defender->flags & OBJECT_MULTIHEX) == 0) {
            *knockbackDistancePtr = damage / 10;
        }
    }

    return damage;
}

// 0x413330
int actionTalk(Object* a1, Object* a2)
{
    if (a1 != gDude) {
        return -1;
    }

    if (FID_TYPE(a2->fid) != OBJ_TYPE_CRITTER) {
        return -1;
    }

    int anim = FID_ANIM_TYPE(gDude->fid);
    if (anim == ANIM_WALK || anim == ANIM_RUNNING) {
        reg_anim_clear(gDude);
    }

    if (isInCombat()) {
        reg_anim_begin(ANIMATION_REQUEST_RESERVED);
        animationRegisterMoveToObject(a1, a2, a1->data.critter.combat.ap, 0);
    } else {
        reg_anim_begin(a1 == gDude ? ANIMATION_REQUEST_RESERVED : ANIMATION_REQUEST_UNRESERVED);

        if (objectGetDistanceBetween(a1, a2) >= 9 || _combat_is_shot_blocked(a1, a1->tile, a2->tile, a2, nullptr)) {
            animationRegisterRunToObject(a1, a2, -1, 0);
        }
    }

    animationRegisterCallbackForced(a1, a2, (AnimationCallback*)_can_talk_to, -1);
    animationRegisterCallback(a1, a2, (AnimationCallback*)_talk_to, -1);
    return reg_anim_end();
}

// 0x413420
int _can_talk_to(Object* a1, Object* a2)
{
    if (_combat_is_shot_blocked(a1, a1->tile, a2->tile, a2, nullptr) || objectGetDistanceBetween(a1, a2) >= 9) {
        if (a1 == gDude) {
            // You cannot get there. (used in actions.c)
            MessageListItem messageListItem;
            messageListItem.num = 2000;
            if (messageListGetItem(&gMiscMessageList, &messageListItem)) {
                displayMonitorAddMessage(messageListItem.text);
            }
        }

        return -1;
    }

    return 0;
}

// 0x413488
int _talk_to(Object* a1, Object* a2)
{
    scriptsRequestDialog(a2);
    return 0;
}

// 0x413494
void actionDamage(int tile, int elevation, int minDamage, int maxDamage, int damageType, bool animated, bool bypassArmor)
{
    Attack* attack = (Attack*)internal_malloc(sizeof(*attack));
    if (attack == nullptr) {
        return;
    }

    Object* attacker;
    if (objectCreateWithFidPid(&attacker, FID_0x20001F5, -1) == -1) {
        internal_free(attack);
        return;
    }

    objectHide(attacker, nullptr);

    attacker->flags |= OBJECT_NO_SAVE;

    objectSetLocation(attacker, tile, elevation, nullptr);

    Object* defender = _obj_blocking_at(nullptr, tile, elevation);
    attackInit(attack, attacker, defender, HIT_MODE_PUNCH, HIT_LOCATION_TORSO);
    attack->tile = tile;
    attack->attackerFlags = DAM_HIT;
    gameUiDisable(1);

    if (defender != nullptr) {
        reg_anim_clear(defender);

        int damage;
        if (bypassArmor) {
            damage = maxDamage;
        } else {
            damage = _compute_dmg_damage(minDamage, maxDamage, defender, &(attack->defenderKnockback), damageType);
        }

        attack->defenderDamage = damage;
    }

    attackComputeDeathFlags(attack);

    if (animated) {
        reg_anim_begin(ANIMATION_REQUEST_RESERVED);
        animationRegisterPlaySoundEffect(attacker, "whc1xxx1", 0);
        _show_damage(attack, gMaximumBloodDeathAnimations[damageType], 0);
        animationRegisterCallbackForced(attack, nullptr, (AnimationCallback*)_report_dmg, 0);
        animationRegisterHideObjectForced(attacker);

        if (reg_anim_end() == -1) {
            objectDestroy(attacker, nullptr);
            internal_free(attack);
            return;
        }
    } else {
        if (defender != nullptr) {
            if ((attack->defenderFlags & DAM_DEAD) != 0) {
                critterKill(defender, -1, 1);
            }
        }

        // NOTE: Uninline.
        _report_dmg(attack, nullptr);

        objectDestroy(attacker, nullptr);
    }

    gameUiEnable();
}

// 0x41363C
int _report_dmg(Attack* attack, Object* a2)
{
    _combat_display(attack);
    _apply_damage(attack, false);
    internal_free(attack);
    gameUiEnable();
    return 0;
}

// Calculate damage by applying threshold and resistances.
//
// 0x413660
int _compute_dmg_damage(int min, int max, Object* obj, int* knockbackDistancePtr, int damageType)
{
    if (!_critter_flag_check(obj->pid, CRITTER_NO_KNOCKBACK)) {
        knockbackDistancePtr = nullptr;
    }

    int damage = randomBetween(min, max) - critterGetStat(obj, STAT_DAMAGE_THRESHOLD + damageType);
    if (damage > 0) {
        damage -= critterGetStat(obj, STAT_DAMAGE_RESISTANCE + damageType) * damage / 100;
    }

    if (damage < 0) {
        damage = 0;
    }

    if (knockbackDistancePtr != nullptr) {
        if ((obj->flags & OBJECT_MULTIHEX) == 0 && damageType != DAMAGE_TYPE_ELECTRICAL) {
            *knockbackDistancePtr = damage / 10;
        }
    }

    return damage;
}

// 0x4136EC
bool actionCheckPush(Object* a1, Object* a2)
{
    // Cannot push anything but critters.
    if (FID_TYPE(a2->fid) != OBJ_TYPE_CRITTER) {
        return false;
    }

    // Cannot push itself.
    if (a1 == a2) {
        return false;
    }

    // Cannot push inactive critters.
    if (!critterIsActive(a2)) {
        return false;
    }

    if (_action_can_talk_to(a1, a2) != 0) {
        return false;
    }

    // Can only push critters that have push handler.
    if (!scriptHasProc(a2->sid, SCRIPT_PROC_PUSH)) {
        return false;
    }

    if (isInCombat()) {
        if (a2->data.critter.combat.team == a1->data.critter.combat.team
            && a2 == a1->data.critter.combat.whoHitMe) {
            return false;
        }

        // TODO: Check.
        Object* whoHitMe = a2->data.critter.combat.whoHitMe;
        if (whoHitMe != nullptr
            && whoHitMe->data.critter.combat.team == a1->data.critter.combat.team) {
            return false;
        }
    }

    return true;
}

// 0x413790
int actionPush(Object* a1, Object* a2)
{
    if (!actionCheckPush(a1, a2)) {
        return -1;
    }

    int sid;
    if (_obj_sid(a2, &sid) == 0) {
        scriptSetObjects(sid, a1, a2);
        scriptExecProc(sid, SCRIPT_PROC_PUSH);

        bool scriptOverrides = false;

        Script* scr;
        if (scriptGetScript(sid, &scr) != -1) {
            scriptOverrides = scr->scriptOverrides;
        }

        if (scriptOverrides) {
            return -1;
        }
    }

    int rotation = tileGetRotationTo(a1->tile, a2->tile);
    int tile;
    do {
        tile = tileGetTileInDirection(a2->tile, rotation, 1);
        if (_obj_blocking_at(a2, tile, a2->elevation) == nullptr) {
            break;
        }

        tile = tileGetTileInDirection(a2->tile, (rotation + 1) % ROTATION_COUNT, 1);
        if (_obj_blocking_at(a2, tile, a2->elevation) == nullptr) {
            break;
        }

        tile = tileGetTileInDirection(a2->tile, (rotation + 5) % ROTATION_COUNT, 1);
        if (_obj_blocking_at(a2, tile, a2->elevation) == nullptr) {
            break;
        }

        tile = tileGetTileInDirection(a2->tile, (rotation + 2) % ROTATION_COUNT, 1);
        if (_obj_blocking_at(a2, tile, a2->elevation) == nullptr) {
            break;
        }

        tile = tileGetTileInDirection(a2->tile, (rotation + 4) % ROTATION_COUNT, 1);
        if (_obj_blocking_at(a2, tile, a2->elevation) == nullptr) {
            break;
        }

        tile = tileGetTileInDirection(a2->tile, (rotation + 3) % ROTATION_COUNT, 1);
        if (_obj_blocking_at(a2, tile, a2->elevation) == nullptr) {
            break;
        }

        return -1;
    } while (0);

    int actionPoints;
    if (isInCombat()) {
        actionPoints = a2->data.critter.combat.ap;
    } else {
        actionPoints = -1;
    }

    reg_anim_begin(ANIMATION_REQUEST_RESERVED);
    animationRegisterRotateToTile(a2, tile);
    animationRegisterMoveToTile(a2, tile, a2->elevation, actionPoints, 0);
    return reg_anim_end();
}

// Returns -1 if can't see there (can't find a path there)
// Returns -2 if it's too far (> 12 tiles).
//
// 0x413970
int _action_can_talk_to(Object* a1, Object* a2)
{
    if (pathfinderFindPath(a1, a1->tile, a2->tile, nullptr, 0, _obj_sight_blocking_at) == 0) {
        return -1;
    }

    if (tileDistanceBetween(a1->tile, a2->tile) > 12) {
        return -2;
    }

    return 0;
}

static int hideProjectile(void* a1, void* a2)
{
    Object* projectile = reinterpret_cast<Object*>(a2);

    Rect rect;
    if (objectHide(projectile, &rect) == 0) {
        tileWindowRefreshRect(&rect, projectile->elevation);
    }

    projectile->flags |= OBJECT_NO_SAVE;

    return 0;
}

} // namespace fallout
