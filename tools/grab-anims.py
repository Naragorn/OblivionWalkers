#!/usr/bin/env python3
"""Writes the grab's three animations (docs/walkers-spec.md 4.19, option B:
made by a script from the game's own zombie animations, no animator).

Read off Oblivion - Meshes.bsa on 2026-10-28 (pyffi): the zombie's
Creatures\\Zombie\\SpecialAnims\\Walk01ArmsUp.kf walks with both arms raised
forward (sequence "forward", a loop of 1.97 s, priority 40, the arms on
B-spline interpolators), HandToHandAttackForwardPower.kf lunges (a clamp of
1.67 s, priority 60), Stagger.kf reels back (a clamp of 1.17 s). MOO's
zombie skeleton (MOO\\zombie\\skeleton.nif) has the same 69 bones by name,
so the same files serve both.

  WalkersGrabHold.kf   SpecialIdle_WalkersGrabHold - Walk01ArmsUp's upper
                       body only (spine to head and both arms), no Bip01, no
                       pelvis, no legs: the walker holds on, arms forward,
                       where it stands; a loop, priority 70 over its combat
                       stance.
  WalkersGrabBite.kf   SpecialIdle_WalkersGrabBite - the lunge's spine, neck,
                       head and face, the hold's arms: it bites without
                       letting go; a clamp of the lunge's length.
  WalkersGrabLetGo.kf  SpecialIdle_WalkersGrabLetGo - Stagger whole: it reels
                       back as it lets go.

They are written under the CS wiki's rule (Idle Animations: "Only KF files
in the IdleAnims directory beneath the directory in which the NPC or
Creature's skeleton exists"): Meshes\\Creatures\\Zombie\\IdleAnims and
Meshes\\MOO\\zombie\\IdleAnims. Bethesda's data reworked, so nothing of it is
committed: the output goes to build-linux/meshes (tools/build-linux.sh
--deploy copies it into the games), as the ESP is written from the ESM.

    ~/obvr-venv/bin/python tools/grab-anims.py \\
        ~/.local/share/Steam/steamapps/common/Oblivion/Data/"Oblivion - Meshes.bsa"
"""

from __future__ import annotations

import argparse
import io
import math
import struct
import sys
import zlib
from pathlib import Path

from pyffi.formats.nif import NifFormat

HERE = Path(__file__).resolve().parent
SOURCE = "meshes\\creatures\\zombie\\"
FOLDERS = ["Creatures/Zombie/IdleAnims", "MOO/zombie/IdleAnims"]
HOLD_PRIORITY = 70
CYCLE_LOOP = 0
CYCLE_CLAMP = 2
# The bones the walker holds with, and the ones it bites with.
ARM_PREFIXES = ("Bip01 L Clavicle", "Bip01 L UpperArm", "Bip01 L Forearm", "Bip01 L Hand", "Bip01 L Finger",
                "Bip01 R Clavicle", "Bip01 R UpperArm", "Bip01 R Forearm", "Bip01 R Hand", "Bip01 R Finger")
BITE_BONES = ("Bip01 Spine", "Bip01 Spine1", "Bip01 Spine2", "Bip01 Neck", "Bip01 Head", "ChinBone01",
              "ChinBone02", "BrowBone01", "TongueBone01", "TongueBone02", "TongueBone03")
OUTPUTS = {"hold": "WalkersGrabHold.kf", "bite": "WalkersGrabBite.kf", "letgo": "WalkersGrabLetGo.kf"}


def bsa_files(path: Path) -> dict[str, bytes]:
    """A TES4 BSA (version 103): every file under SOURCE, by its lower-case
    path. Header, folder records, file records, names - UESP's BSA format."""
    data = path.read_bytes()
    magic, version, offset, flags, folders, files, _folder_names, file_names, _ = struct.unpack_from("<4sIIIIIIII", data)
    if magic != b"BSA\0" or version != 103:
        raise SystemExit(f"{path}: not an Oblivion BSA")
    compressed = bool(flags & 4)
    records = []
    p = offset
    for _ in range(folders):
        _hash, count, folder_offset = struct.unpack_from("<QII", data, p)
        p += 16
        records.append((count, folder_offset))
    entries = []
    q = 0
    for count, folder_offset in records:
        q = folder_offset - file_names
        length = data[q]
        folder = data[q + 1:q + length].rstrip(b"\0").decode("latin-1")
        q += 1 + length
        for _ in range(count):
            _hash, size, file_offset = struct.unpack_from("<QII", data, q)
            q += 16
            entries.append((folder, size, file_offset))
    names = data[q:q + file_names].split(b"\0")
    out = {}
    for (folder, size, file_offset), name in zip(entries, names):
        full = f"{folder}\\{name.decode('latin-1')}".lower()
        if not full.startswith(SOURCE):
            continue
        flip = bool(size & 0x40000000)
        raw = data[file_offset:file_offset + (size & 0x3FFFFFFF)]
        out[full] = zlib.decompress(raw[4:]) if compressed ^ flip else raw
    return out


def read_kf(raw: bytes):
    kf = NifFormat.Data()
    kf.read(io.BytesIO(raw))
    seq = kf.roots[0]
    if not isinstance(seq, NifFormat.NiControllerSequence):
        raise SystemExit("not a KF")
    return kf, seq


def name_of(block) -> str:
    palette = block.string_palette.palette.palette
    end = palette.index(b"\0", block.node_name_offset)
    return palette[block.node_name_offset:end].decode("latin-1")


def is_arm(name: str) -> bool:
    return name.startswith(ARM_PREFIXES) or name.endswith("ArmTwist")


def add_to_palette(palette, name: str) -> int:
    text = palette.palette.palette
    at = len(text)
    palette.palette.palette = text + name.encode("latin-1") + b"\0"
    palette.palette.length = len(palette.palette.palette)
    return at


def adopt(block, seq):
    """A controlled block of another file moved into `seq`: its names put in
    `seq`'s string palette (the offsets point into the block's own)."""
    own = block.string_palette.palette.palette

    def text_at(offset: int) -> str | None:
        if offset < 0:
            return None
        return own[offset:own.index(b"\0", offset)].decode("latin-1")

    names = {field: text_at(getattr(block, field)) for field in
             ("node_name_offset", "property_type_offset", "controller_type_offset", "variable_1_offset",
              "variable_2_offset")}
    block.string_palette = seq.string_palette
    for field, text in names.items():
        block.set(field, -1 if text is None else add_to_palette(seq.string_palette, text))
    return block


class Snap:
    """A controlled block's fields, detached: pyffi's arrays take no items by
    assignment and its deepcopy follows the links into other blocks, so the
    blocks kept are read out field by field before the array is resized,
    and written back after (the links - interpolator, controller, palette -
    stay links to the same blocks)."""

    def __init__(self, block):
        self.fields = {name: getattr(block, name) for name in block._names}

    @property
    def string_palette(self):
        return self.fields["string_palette"]

    @string_palette.setter
    def string_palette(self, value):
        self.fields["string_palette"] = value

    def __getattr__(self, name):
        if name == "fields":
            raise AttributeError(name)
        return self.fields[name]

    def set(self, name, value):
        self.fields[name] = value


def snap(block):
    return Snap(block)


def finish(seq, name: str, cycle: int, priority: int | None, blocks: list):
    seq.name = name.encode("latin-1")
    seq.cycle_type = cycle
    seq.num_controlled_blocks = len(blocks)
    seq.controlled_blocks.update_size()
    for i, block in enumerate(blocks):
        target = seq.controlled_blocks[i]
        for field, value in block.fields.items():
            setattr(target, field, value)
        if priority is not None:
            target.priority = priority
    keys = seq.text_keys
    keys.num_text_keys = 2
    keys.text_keys.update_size()
    keys.text_keys[0].time = 0.0
    keys.text_keys[0].value = b"start"
    keys.text_keys[1].time = seq.stop_time
    keys.text_keys[1].value = b"end"


def write_kf(kf) -> bytes:
    stream = io.BytesIO()
    kf.write(stream)
    return stream.getvalue()


# The grip (Nadi, 2026-10-28, option 1 after the pictures of 07:36: the hands
# reached wide past the body): Walk01ArmsUp's hands, read off its control
# points on the zombie skeleton, lie some 55 units apart - the left about
# (-25, 48, 95), the right (28, 25, 85) in the walker's frame (+y ahead, +z
# up, its left at -x). Each upper arm is turned, by one turn for the whole
# loop, so the shoulder-to-hand line points at these: the hands 24 units
# apart, 50 ahead, at chest height - closing on the arm before it. The
# sway of the loop is kept (the turn is the same at every key).
GRIP_TARGETS = {"Bip01 L UpperArm": (-12.0, 50.0, 88.0), "Bip01 R UpperArm": (12.0, 50.0, 88.0)}
HANDS = {"Bip01 L UpperArm": "Bip01 L Hand", "Bip01 R UpperArm": "Bip01 R Hand"}


def mat_mul(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(3)) for j in range(3)] for i in range(3)]


def mat_vec(a, v):
    return [sum(a[i][k] * v[k] for k in range(3)) for i in range(3)]


def mat_t(a):
    return [[a[j][i] for j in range(3)] for i in range(3)]


def quat_mat(w, x, y, z):
    """A unit quaternion as the matrix that turns a point (point' = R point)."""
    return [[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
            [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
            [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]]


def mat_quat(m):
    trace = m[0][0] + m[1][1] + m[2][2]
    if trace > 0:
        s = math.sqrt(trace + 1.0) * 2
        return (0.25 * s, (m[2][1] - m[1][2]) / s, (m[0][2] - m[2][0]) / s, (m[1][0] - m[0][1]) / s)
    i = max(range(3), key=lambda k: m[k][k])
    j, k = (i + 1) % 3, (i + 2) % 3
    s = math.sqrt(1.0 + m[i][i] - m[j][j] - m[k][k]) * 2
    q = [0.0, 0.0, 0.0]
    q[i] = 0.25 * s
    q[j] = (m[j][i] + m[i][j]) / s
    q[k] = (m[k][i] + m[i][k]) / s
    return ((m[k][j] - m[j][k]) / s, q[0], q[1], q[2])


def turn_between(u, v):
    """The turn that brings direction u onto direction v (Rodrigues)."""
    nu = math.sqrt(sum(a * a for a in u))
    nv = math.sqrt(sum(a * a for a in v))
    u = [a / nu for a in u]
    v = [a / nv for a in v]
    axis = [u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]]
    s = math.sqrt(sum(a * a for a in axis))
    c = sum(a * b for a, b in zip(u, v))
    if s < 1e-9:
        return [[1, 0, 0], [0, 1, 0], [0, 0, 1]]
    x, y, z = (a / s for a in axis)
    t = 1 - c
    return [[t * x * x + c, t * x * y - s * z, t * x * z + s * y],
            [t * x * y + s * z, t * y * y + c, t * y * z - s * x],
            [t * x * z - s * y, t * y * z + s * x, t * z * z + c]]


class Skeleton:
    """The zombie skeleton's bones at rest: each one's parent and local
    turn, place and scale. pyffi hands a NIF rotation as stored, the
    transpose of the turn of a point (the feet came out at the floor only
    so, read 2026-10-28)."""

    def __init__(self, raw: bytes):
        nif = NifFormat.Data()
        nif.read(io.BytesIO(raw))
        self.parent = {}
        self.rest = {}

        def walk(node, parent):
            name = node.name.decode("latin-1")
            r = node.rotation
            self.parent[name] = parent
            self.rest[name] = ([[r.m_11, r.m_21, r.m_31], [r.m_12, r.m_22, r.m_32], [r.m_13, r.m_23, r.m_33]],
                               [node.translation.x, node.translation.y, node.translation.z], node.scale)
            for child in node.children:
                if isinstance(child, NifFormat.NiNode):
                    walk(child, name)

        walk(nif.roots[0], None)

    def world(self, local: dict) -> dict:
        out = {}

        def at(name):
            if name not in out:
                turn, place, scale = local.get(name, self.rest[name])
                parent = self.parent[name]
                if parent is None:
                    out[name] = (turn, place)
                else:
                    pturn, pplace = at(parent)
                    moved = mat_vec(pturn, [scale * a for a in place])
                    out[name] = (mat_mul(pturn, turn), [a + b for a, b in zip(pplace, moved)])
            return out[name]

        for name in self.rest:
            at(name)
        return out


def keyed(block):
    """A bone's keys as (time, quaternion w x y z, place or None), from a
    B-spline or a keyed interpolator; [] for none."""
    interp = block.interpolator
    if isinstance(interp, NifFormat.NiBSplineCompTransformInterpolator):
        times = list(interp.get_times())
        turns = list(interp.get_rotations())
        places = list(interp.get_translations())
        return [(t, turns[i] if i < len(turns) else None, places[i] if i < len(places) else None)
                for i, t in enumerate(times)]
    if isinstance(interp, NifFormat.NiTransformInterpolator):
        data = interp.data
        if data is not None and data.num_rotation_keys and data.quaternion_keys:
            return [(k.time, (k.value.w, k.value.x, k.value.y, k.value.z), None) for k in data.quaternion_keys]
        q = interp.rotation
        if q.w > -1e30:
            return [(0.0, (q.w, q.x, q.y, q.z), None)]
    return []


def pose_at(skeleton: Skeleton, keys: dict, time: float) -> dict:
    """The local turns and places at the key nearest `time` of each bone."""
    local = {}
    for name, ks in keys.items():
        if not ks or name not in skeleton.rest:
            continue
        _, quat, place = min(ks, key=lambda k: abs(k[0] - time))
        turn, rest_place, scale = skeleton.rest[name]
        local[name] = (quat_mat(*quat) if quat is not None else turn,
                       list(place) if place is not None else rest_place, scale)
    return local


def grip_arms(blocks: list, skeleton: Skeleton) -> dict:
    """The upper arms turned toward GRIP_TARGETS: a keyed interpolator per
    arm, at the B-spline's own key times, each key the old local turn under
    the same world turn of the whole loop. Returns the new interpolators by
    bone and the hands' mean places before and after, for the log."""
    keys = {name_of(b): keyed(b) for b in blocks}
    result = {}
    for arm, target in GRIP_TARGETS.items():
        arm_keys = keys[arm]
        poses = [pose_at(skeleton, keys, t) for t, _, _ in arm_keys]
        worlds = [skeleton.world(p) for p in poses]
        shoulder = [sum(w[arm][1][i] for w in worlds) / len(worlds) for i in range(3)]
        hand = [sum(w[HANDS[arm]][1][i] for w in worlds) / len(worlds) for i in range(3)]
        turn = turn_between([h - s for h, s in zip(hand, shoulder)], [t - s for t, s in zip(target, shoulder)])
        new_keys = []
        for (time, _, _), pose, world in zip(arm_keys, poses, worlds):
            parent_turn = world[skeleton.parent[arm]][0]
            local_turn = pose[arm][0]
            new_local = mat_mul(mat_t(parent_turn), mat_mul(turn, mat_mul(parent_turn, local_turn)))
            new_keys.append((time, mat_quat(new_local)))
            pose[arm] = (new_local, pose[arm][1], pose[arm][2])
        after = [skeleton.world(p)[HANDS[arm]][1] for p in poses]
        moved = [sum(a[i] for a in after) / len(after) for i in range(3)]
        result[arm] = (new_keys, skeleton.rest[arm][1], hand, moved)
    return result


def keyed_interpolator(keys: list, place: list):
    """A NiTransformInterpolator with linear quaternion keys and the bone's
    rest place."""
    data = NifFormat.NiTransformData()
    data.rotation_type = 1  # linear
    data.num_rotation_keys = len(keys)
    data.quaternion_keys.update_size()
    for k, (time, (w, x, y, z)) in zip(data.quaternion_keys, keys):
        k.time = time
        k.value.w, k.value.x, k.value.y, k.value.z = w, x, y, z
    interp = NifFormat.NiTransformInterpolator()
    interp.translation.x, interp.translation.y, interp.translation.z = place
    w, x, y, z = keys[0][1]
    interp.rotation.w, interp.rotation.x, interp.rotation.y, interp.rotation.z = w, x, y, z
    interp.scale = 1.0
    interp.data = data
    return interp


GRIP_LINES: list[str] = []


def gripped(snaps: list, grips: dict) -> list:
    for s in snaps:
        name = name_of(s)
        if name in grips:
            keys, place, _, _ = grips[name]
            s.set("interpolator", keyed_interpolator(keys, place))
    return snaps


def build(files: dict[str, bytes]) -> dict[str, bytes]:
    walk_raw = files[SOURCE + "specialanims\\walk01armsup.kf"]
    skeleton = Skeleton(files[SOURCE + "skeleton.nif"])
    hold_kf, hold = read_kf(walk_raw)
    grips = grip_arms(list(hold.controlled_blocks), skeleton)
    for arm, (_, _, before, after) in grips.items():
        GRIP_LINES.append(f"{HANDS[arm]}: from ({before[0]:.0f}, {before[1]:.0f}, {before[2]:.0f}) to "
                          f"({after[0]:.0f}, {after[1]:.0f}, {after[2]:.0f}), aimed at {GRIP_TARGETS[arm]}")
    upper = gripped([snap(b) for b in hold.controlled_blocks if is_arm(name_of(b)) or name_of(b) in BITE_BONES], grips)
    finish(hold, "SpecialIdle_WalkersGrabHold", CYCLE_LOOP, HOLD_PRIORITY, upper)

    bite_kf, bite = read_kf(files[SOURCE + "handtohandattackforwardpower.kf"])
    _, arms_source = read_kf(walk_raw)
    lunge = [snap(b) for b in bite.controlled_blocks if name_of(b) in BITE_BONES]
    arms = gripped([adopt(snap(b), bite) for b in arms_source.controlled_blocks if is_arm(name_of(b))], grips)
    finish(bite, "SpecialIdle_WalkersGrabBite", CYCLE_CLAMP, HOLD_PRIORITY, lunge + arms)

    letgo_kf, letgo = read_kf(files[SOURCE + "stagger.kf"])
    finish(letgo, "SpecialIdle_WalkersGrabLetGo", CYCLE_CLAMP, None, [snap(b) for b in letgo.controlled_blocks])
    return {"hold": write_kf(hold_kf), "bite": write_kf(bite_kf), "letgo": write_kf(letgo_kf)}


def check(written: dict[str, bytes]) -> list[str]:
    """Each file read back: its sequence's name, cycle, blocks and keys."""
    lines = []
    want = {"hold": ("SpecialIdle_WalkersGrabHold", CYCLE_LOOP), "bite": ("SpecialIdle_WalkersGrabBite", CYCLE_CLAMP),
            "letgo": ("SpecialIdle_WalkersGrabLetGo", CYCLE_CLAMP)}
    for key, raw in written.items():
        _, seq = read_kf(raw)
        names = [name_of(b) for b in seq.controlled_blocks]
        name, cycle = want[key]
        if seq.name.decode() != name or seq.cycle_type != cycle:
            raise SystemExit(f"{key}: the sequence reads back as {seq.name!r}, cycle {seq.cycle_type}")
        if [k.value for k in seq.text_keys.text_keys] != [b"start", b"end"]:
            raise SystemExit(f"{key}: the text keys read back wrong")
        if key != "letgo" and any(n in ("Bip01", "Bip01 Pelvis") or "Thigh" in n or "Calf" in n for n in names):
            raise SystemExit(f"{key}: a lower-body bone is left: the walker would step")
        if key == "bite" and not ({"Bip01 Head", "Bip01 R Hand", "Bip01 L Hand"} <= set(names)):
            raise SystemExit("bite: the head or a hand is missing")
        lines.append(f"{OUTPUTS[key]}: {name}, {len(names)} bones, {seq.stop_time:.2f} s, "
                     f"{'loop' if cycle == CYCLE_LOOP else 'clamp'}")
    return lines


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("bsa", help="Oblivion - Meshes.bsa")
    parser.add_argument("--out", default=str(HERE.parent / "build-linux" / "meshes"))
    args = parser.parse_args()
    written = build(bsa_files(Path(args.bsa)))
    lines = check(written)
    for folder in FOLDERS:
        target = Path(args.out) / folder
        target.mkdir(parents=True, exist_ok=True)
        for key, raw in written.items():
            (target / OUTPUTS[key]).write_bytes(raw)
    print("\n".join(GRIP_LINES + lines))
    print(f"written into {', '.join(str(Path(args.out) / f) for f in FOLDERS)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
