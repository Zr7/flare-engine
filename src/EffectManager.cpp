/*
Copyright © 2012 Justin Jacobs

This file is part of FLARE.

FLARE is free software: you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.

FLARE is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
FLARE.  If not, see http://www.gnu.org/licenses/
*/

/**
 * class EffectManager
 */

#include "Animation.h"
#include "AnimationSet.h"
#include "EffectManager.h"
#include "Settings.h"

EffectManager::EffectManager()
	: bonus(std::vector<int>(STAT_COUNT, 0))
	, bonus_resist(std::vector<int>(ELEMENTS.size(), 0))
	, triggered_others(false)
	, triggered_block(false)
	, triggered_hit(false)
	, triggered_halfdeath(false)
	, triggered_joincombat(false)
	, triggered_death(false) {
	clearStatus();
}

EffectManager::~EffectManager() {
	for (unsigned i=0; i<effect_list.size(); i++) {
		removeAnimation(i);
	}
}

EffectManager& EffectManager::operator= (const EffectManager &emSource) {
	effect_list.resize(emSource.effect_list.size());

	for (unsigned i=0; i<effect_list.size(); i++) {
		effect_list[i].name = emSource.effect_list[i].name;
		effect_list[i].icon = emSource.effect_list[i].icon;
		effect_list[i].ticks = emSource.effect_list[i].ticks;
		effect_list[i].duration = emSource.effect_list[i].duration;
		effect_list[i].type = emSource.effect_list[i].type;
		effect_list[i].magnitude = emSource.effect_list[i].magnitude;
		effect_list[i].magnitude_max = emSource.effect_list[i].magnitude_max;
		effect_list[i].item = emSource.effect_list[i].item;
		effect_list[i].trigger = emSource.effect_list[i].trigger;
		effect_list[i].render_above = emSource.effect_list[i].render_above;
		effect_list[i].passive_id = emSource.effect_list[i].passive_id;

		if (emSource.effect_list[i].animation_name != "") {
			effect_list[i].animation_name = emSource.effect_list[i].animation_name;
			anim->increaseCount(effect_list[i].animation_name);
			effect_list[i].animation = loadAnimation(effect_list[i].animation_name);
		}
	}
	damage = emSource.damage;
	hpot = emSource.hpot;
	mpot = emSource.mpot;
	speed = emSource.speed;
	immunity = emSource.immunity;
	stun = emSource.stun;
	revive = emSource.revive;
	convert = emSource.convert;
	death_sentence = emSource.death_sentence;
	fear = emSource.fear;
	bonus_offense = emSource.bonus_offense;
	bonus_defense = emSource.bonus_defense;
	bonus_physical = emSource.bonus_physical;
	bonus_mental = emSource.bonus_mental;
	for (unsigned i=0; i<STAT_COUNT; i++) {
		bonus[i] = emSource.bonus[i];
	}
	triggered_others = emSource.triggered_others;
	triggered_block = emSource.triggered_block;
	triggered_hit = emSource.triggered_hit;
	triggered_halfdeath = emSource.triggered_halfdeath;
	triggered_joincombat = emSource.triggered_joincombat;
	triggered_death = emSource.triggered_death;

	return *this;
}

void EffectManager::clearStatus() {
	damage = 0;
	hpot = 0;
	mpot = 0;
	speed = 100;
	immunity = false;
	stun = false;
	revive = false;
	convert = false;
	death_sentence = false;
	fear = false;

	bonus_offense = 0;
	bonus_defense = 0;
	bonus_physical = 0;
	bonus_mental = 0;

	for (unsigned i=0; i<STAT_COUNT; i++) {
		bonus[i] = 0;
	}

	for (unsigned i=0; i<bonus_resist.size(); i++) {
		bonus_resist[i] = 0;
	}
}

void EffectManager::logic() {
	clearStatus();

	for (unsigned i=0; i<effect_list.size(); i++) {
		// @CLASS EffectManager|Description of "type" in powers/effects.txt
		// expire timed effects and total up magnitudes of active effects
		if (effect_list[i].duration >= 0) {
			// @TYPE damage|Damage per second
			if (effect_list[i].type == "damage" && effect_list[i].ticks % MAX_FRAMES_PER_SEC == 1) damage += effect_list[i].magnitude;
			// @TYPE hpot|HP restored per second
			else if (effect_list[i].type == "hpot" && effect_list[i].ticks % MAX_FRAMES_PER_SEC == 1) hpot += effect_list[i].magnitude;
			// @TYPE mpot|MP restored per second
			else if (effect_list[i].type == "mpot" && effect_list[i].ticks % MAX_FRAMES_PER_SEC == 1) mpot += effect_list[i].magnitude;
			// @TYPE speed|Changes movement speed. A magnitude of 100 is 100% speed (aka normal speed).
			else if (effect_list[i].type == "speed") speed = (effect_list[i].magnitude * speed) / 100;
			// @TYPE immunity|Removes and prevents bleed, slow, stun, and immobilize. Magnitude is ignored.
			else if (effect_list[i].type == "immunity") immunity = true;
			// @TYPE stun|Can't move or attack. Being attacked breaks stun.
			else if (effect_list[i].type == "stun") stun = true;
			// @TYPE revive|Revives the player. Typically attached to a power that triggers when the player dies.
			else if (effect_list[i].type == "revive") revive = true;
			// @TYPE convert|Causes an enemy or an ally to switch allegiance
			else if (effect_list[i].type == "convert") convert = true;
			// @TYPE fear|Causes enemies to run away
			else if (effect_list[i].type == "fear") fear = true;
			// @TYPE offense|Increase Offense stat.
			else if (effect_list[i].type == "offense") bonus_offense += effect_list[i].magnitude;
			// @TYPE defense|Increase Defense stat.
			else if (effect_list[i].type == "defense") bonus_defense += effect_list[i].magnitude;
			// @TYPE physical|Increase Physical stat.
			else if (effect_list[i].type == "physical") bonus_physical += effect_list[i].magnitude;
			// @TYPE mental|Increase Mental stat.
			else if (effect_list[i].type == "mental") bonus_mental += effect_list[i].magnitude;
			else {
				bool found_key = false;

				// @TYPE $STATNAME|Increases $STATNAME, where $STATNAME is any of the base stats. Examples: hp, dmg_melee_min, xp_gain
				for (unsigned j=0; j<STAT_COUNT; j++) {
					if (effect_list[i].type == STAT_NAME[j]) {
						bonus[j] += effect_list[i].magnitude;
						found_key = true;
					}
				}

				if (!found_key) {
					// @TYPE $ELEMENT_resist|Increase Resistance % to $ELEMENT, where $ELEMENT is any found in engine/elements.txt. Example: fire_resist
					for (unsigned j=0; j<bonus_resist.size(); j++) {
						if (effect_list[i].type == ELEMENTS[j].name + "_resist")
							bonus_resist[j] += effect_list[i].magnitude;
					}
				}
			}

			if (effect_list[i].duration > 0) {
				if (effect_list[i].ticks > 0) effect_list[i].ticks--;
				if (effect_list[i].ticks == 0) {
					//death sentence is only applied at the end of the timer
					// @TYPE death_sentence|Causes sudden death at the end of the effect duration.
					if (effect_list[i].type == "death_sentence") death_sentence = true;
					removeEffect(i);
					i--;
					continue;
				}
			}
		}
		// expire shield effects
		if (effect_list[i].magnitude_max > 0 && effect_list[i].magnitude == 0) {
			// @TYPE shield|Create a damage absorbing barrier based on Mental damage stat. Duration is ignored.
			if (effect_list[i].type == "shield") {
				removeEffect(i);
				i--;
				continue;
			}
		}
		// expire effects based on animations
		if ((effect_list[i].animation && effect_list[i].animation->isLastFrame()) || !effect_list[i].animation) {
			// @TYPE heal|Restore HP based on Mental damage stat.
			if (effect_list[i].type == "heal") {
				removeEffect(i);
				i--;
				continue;
			}
		}

		// animate
		if (effect_list[i].animation) {
			if (!effect_list[i].animation->isCompleted())
				effect_list[i].animation->advanceFrame();
		}
	}
}

void EffectManager::addEffect(EffectDef &effect, int duration, int magnitude, bool item, int trigger, int passive_id, int source_type) {
	// if we're already immune, don't add negative effects
	if (immunity) {
		if (effect.type == "damage") return;
		else if (effect.type == "speed" && magnitude < 100) return;
		else if (effect.type == "stun") return;
	}

	for (unsigned i=effect_list.size(); i>0; i--) {
		if (effect_list[i-1].name == effect.name) {
			if (trigger > -1 && effect_list[i-1].trigger == trigger)
				return; // trigger effects can only be cast once per trigger

			if (!effect.can_stack)
				removeEffect(i-1);
		}
		// if we're adding an immunity effect, remove all negative effects
		if (effect.type == "immunity") {
			clearNegativeEffects();
		}
	}

	Effect e;

	e.name = effect.name;
	e.icon = effect.icon;
	e.type = effect.type;
	e.render_above = effect.render_above;

	if (effect.animation != "") {
		anim->increaseCount(effect.animation);
		e.animation = loadAnimation(effect.animation);
		e.animation_name = effect.animation;
	}

	e.ticks = e.duration = duration;
	e.magnitude = e.magnitude_max = magnitude;
	e.item = item;
	e.trigger = trigger;
	e.passive_id = passive_id;
	e.source_type = source_type;

	effect_list.push_back(e);
}

void EffectManager::removeEffect(int id) {
	removeAnimation(id);
	effect_list.erase(effect_list.begin()+id);
}

void EffectManager::removeAnimation(int id) {
	if (effect_list[id].animation && effect_list[id].animation_name != "") {
		anim->decreaseCount(effect_list[id].animation_name);
		delete effect_list[id].animation;
		effect_list[id].animation = NULL;
		effect_list[id].animation_name = "";
	}
}

void EffectManager::removeEffectType(std::string type) {
	for (unsigned i=effect_list.size(); i > 0; i--) {
		if (effect_list[i-1].type == type) removeEffect(i-1);
	}
}

void EffectManager::removeEffectPassive(int id) {
	for (unsigned i=effect_list.size(); i > 0; i--) {
		if (effect_list[i-1].passive_id == id) removeEffect(i-1);
	}
}

void EffectManager::clearEffects() {
	for (unsigned i=effect_list.size(); i > 0; i--) {
		removeEffect(i-1);
	}

	clearStatus();

	// clear triggers
	triggered_others = triggered_block = triggered_hit = triggered_halfdeath = triggered_joincombat = triggered_death = false;
}

void EffectManager::clearNegativeEffects() {
	for (unsigned i=effect_list.size(); i > 0; i--) {
		if (effect_list[i-1].type == "damage") removeEffect(i-1);
		else if (effect_list[i-1].type == "speed" && effect_list[i-1].magnitude_max < 100) removeEffect(i-1);
		else if (effect_list[i-1].type == "stun") removeEffect(i-1);
	}
}

void EffectManager::clearItemEffects() {
	for (unsigned i=effect_list.size(); i > 0; i--) {
		if (effect_list[i-1].item) removeEffect(i-1);
	}
}

void EffectManager::clearTriggerEffects(int trigger) {
	for (unsigned i=effect_list.size(); i > 0; i--) {
		if (effect_list[i-1].trigger > -1 && effect_list[i-1].trigger == trigger) removeEffect(i-1);
	}
}

int EffectManager::damageShields(int dmg) {
	int over_dmg = dmg;

	for (unsigned i=0; i<effect_list.size(); i++) {
		if (effect_list[i].magnitude_max > 0 && effect_list[i].type == "shield") {
			effect_list[i].magnitude -= over_dmg;
			if (effect_list[i].magnitude < 0) {
				over_dmg = abs(effect_list[i].magnitude);
				effect_list[i].magnitude = 0;
			}
			else {
				return 0;
			}
		}
	}

	return over_dmg;
}

Animation* EffectManager::loadAnimation(std::string &s) {
	if (s != "") {
		AnimationSet *animationSet = anim->getAnimationSet(s);
		return animationSet->getAnimation();
	}
	return NULL;
}

