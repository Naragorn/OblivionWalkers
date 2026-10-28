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


def build(files: dict[str, bytes]) -> dict[str, bytes]:
    walk_raw = files[SOURCE + "specialanims\\walk01armsup.kf"]
    hold_kf, hold = read_kf(walk_raw)
    upper = [snap(b) for b in hold.controlled_blocks if is_arm(name_of(b)) or name_of(b) in BITE_BONES]
    finish(hold, "SpecialIdle_WalkersGrabHold", CYCLE_LOOP, HOLD_PRIORITY, upper)

    bite_kf, bite = read_kf(files[SOURCE + "handtohandattackforwardpower.kf"])
    _, arms_source = read_kf(walk_raw)
    lunge = [snap(b) for b in bite.controlled_blocks if name_of(b) in BITE_BONES]
    arms = [adopt(snap(b), bite) for b in arms_source.controlled_blocks if is_arm(name_of(b))]
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
    print("\n".join(lines))
    print(f"written into {', '.join(str(Path(args.out) / f) for f in FOLDERS)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
