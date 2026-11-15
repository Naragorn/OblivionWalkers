#pragma once

// Shots and spells that end a walker as a lost head does (4.17b; Nadi,
// 2026-11-14: "kopfschüsse zählen auch als beheaded", magic "B C D"):
// - an arrow stuck in its head or neck (ArrowProjectile's CollisionData
//   names the node it struck - xOBSE GameObjects.h: +0x5C the data, in it
//   +0x24 what it hit and +0x28 the NiNode, "seen Bip01 Spine"),
// - a spell bolt that lands at its head (the bolt's position against the
//   head bone's - no node is known for a spell),
// - a spell hit that takes its whole health at once (overkill),
// - and fire: a walker that dies within FireSeconds of a fire damage hit
//   burns and stays down.
// Every one counts for any shooter or caster, as the others' kills of 4.17a.

#include "Types.h"

namespace walkers {

// [Headshots] in OblivionWalkers.ini.
struct HeadshotSettings {
	bool arrows = true;          // Arrows: an arrow in the head or neck ends a walker
	bool bolts = true;           // Bolts: a spell bolt at the head ends it
	float headRadius = 30.0f;    // HeadRadius: units from the head bone a bolt counts within
	bool overkill = true;        // Overkill: a spell hit of OverkillPercent of its health ends it
	float overkillPercent = 100.0f;  // OverkillPercent: of its base health, in one hit (100: the whole)
	bool fire = true;            // Fire: a walker dying burning ends
	float fireSeconds = 6.0f;    // FireSeconds: how long after a fire hit it still burns
};

// The magic effect code of Fire Damage ('FIDG', as the engine keeps it).
inline constexpr UInt32 kEffectFireDamage = 'F' | ('I' << 8) | ('D' << 16) | ('G' << 24);

// Whether a node's name is a head's or a neck's ("Bip01 Head", "Bip01 Neck",
// "Bip01 Neck1", MOO's skeletons alike), case aside.
inline bool IsHeadBone(const char* name) {
	if (name == nullptr) {
		return false;
	}
	for (const char* p = name; *p != '\0'; ++p) {
		const char* words[2] = {"head", "neck"};
		for (const char* w : words) {
			const char* a = p;
			const char* b = w;
			while (*a != '\0' && *b != '\0' && (*a == *b || *a - 'A' + 'a' == *b)) {
				++a;
				++b;
			}
			if (*b == '\0') {
				return true;
			}
		}
	}
	return false;
}

// The projectiles already judged: an arrow stays stuck in a body and is
// found again at every later hit - judged once, at the hit it made.
inline constexpr UInt32 kSeenShotsMost = 64;
struct SeenShots {
	UInt32 ids[kSeenShotsMost] = {};
	UInt32 count = 0;
	UInt32 next = 0;
};

// True the first time `id` is asked (and notes it), false after and for 0.
inline bool FreshShot(SeenShots& s, UInt32 id) {
	if (id == 0) {
		return false;
	}
	for (UInt32 i = 0; i < s.count; ++i) {
		if (s.ids[i] == id) {
			return false;
		}
	}
	s.ids[s.next] = id;
	s.next = (s.next + 1) % kSeenShotsMost;
	if (s.count < kSeenShotsMost) {
		++s.count;
	}
	return true;
}

// A bolt `dx dy dz` from the head bone counts within `radius`.
inline bool BoltAtHead(float dx, float dy, float dz, float radius) {
	return radius > 0.0f && dx * dx + dy * dy + dz * dz <= radius * radius;
}

// A hit that takes `percent` of the base health at once - above 100 more
// than the whole; 0 or below never (not every hit); no base health known:
// none. 2026-11-15: a walker's base health is small - the records' 80 at
// HealthPercent 15 is 12 (4.5) - so 100 already ends one at a 12 point
// spell; above 100 asks a bigger spell.
inline bool Overkill(float damage, float baseHealth, float percent = 100.0f) {
	const float share = percent / 100.0f;
	return baseHealth > 0.0f && share > 0.0f && damage >= baseHealth * share;
}

// Recent hits of one kind (fire, magic) by reference, with their time.
inline constexpr UInt32 kHitNotesMost = 32;
struct HitNotes {
	UInt32 ids[kHitNotesMost] = {};
	UInt32 ms[kHitNotesMost] = {};
	UInt32 count = 0;
	UInt32 next = 0;
};

inline void NoteHit(HitNotes& n, UInt32 id, UInt32 nowMs) {
	if (id == 0) {
		return;
	}
	for (UInt32 i = 0; i < n.count; ++i) {
		if (n.ids[i] == id) {
			n.ms[i] = nowMs;
			return;
		}
	}
	n.ids[n.next] = id;
	n.ms[n.next] = nowMs;
	n.next = (n.next + 1) % kHitNotesMost;
	if (n.count < kHitNotesMost) {
		++n.count;
	}
}

// Whether `id` was hit within `windowMs` before `nowMs` (the clock may wrap).
inline bool HitWithin(const HitNotes& n, UInt32 id, UInt32 nowMs, UInt32 windowMs) {
	for (UInt32 i = 0; id != 0 && i < n.count; ++i) {
		if (n.ids[i] == id) {
			return nowMs - n.ms[i] <= windowMs;
		}
	}
	return false;
}

// A walker's death while burning ends it.
inline bool Burned(const HeadshotSettings& s, const HitNotes& fire, UInt32 id, UInt32 nowMs) {
	return s.fire && s.fireSeconds > 0.0f && HitWithin(fire, id, nowMs, static_cast<UInt32>(s.fireSeconds * 1000.0f));
}

// A spell's damage: the hit the engine reports came within this of a magic
// effect landing on the same actor (the effect's damage is applied at its
// first update, a frame or two after it lands).
inline constexpr UInt32 kSpellDamageWindowMs = 500;

inline bool SpellOverkill(const HeadshotSettings& s, const HitNotes& magic, UInt32 id, UInt32 nowMs, float damage,
                          float baseHealth) {
	return s.overkill && HitWithin(magic, id, nowMs, kSpellDamageWindowMs) &&
	       Overkill(damage, baseHealth, s.overkillPercent);
}

}  // namespace walkers
