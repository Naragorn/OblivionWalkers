"""The walkers' news: the 34 editions of the Black Horse Courier (docs/walkers-spec.md 4.9, 4.12).

Imported by tools/esp-write.py, which writes them as BOOK records 0x8FC..0x90D
and 0x9B7..0x9C6 in this order - NewsLogic.h EditionIndex: the outbreak in
general 0, a horde heading for a city 1-8, a city's gates attacked 9-16,
several cities 17, a city besieged 18-25, a siege lifted 26-33, the cities in
CityGates.h's order. The text is the broadsheets' own form (the
header picture, the headline, then the body), read off Oblivion.esm's
Broadsheet01VampireNest. What the Legion asks matches what the walkers do in
the game: they walk and never run, no dead rise in the Nine's chapels, what
they carry is spoiled, and a city's guard comes out of its gates to meet them.
"""

from __future__ import annotations

# CityGates.h City order.
CITIES = [
    ("Anvil", "Anvil", "the Gold Coast"),
    ("Bravil", "Bravil", "the Nibenay marshes"),
    ("Bruma", "Bruma", "the Jerall Mountains"),
    ("Cheydinhal", "Cheydinhal", "the eastern Nibenay"),
    ("Chorrol", "Chorrol", "the Great Forest"),
    ("Leyawiin", "Leyawiin", "the Blackwood"),
    ("Skingrad", "Skingrad", "the West Weald"),
    ("ImperialCity", "the Imperial City", "Lake Rumare"),
]

HEADER = '<FONT face=1><DIV align="center">\n<IMG src="Book/BroadsheetHeader01.dds" width=512 height=128>\n <BR>\n'

LEGION_ASKS = (
    "The Imperial Legion asks every citizen of Cyrodiil:<br>\n"
    "Travel by daylight, and never alone. The walkers are slow, but they do not tire and they do not sleep.<br>\n"
    "Do not let them close around you. Walk away from them - they will not run after you.<br>\n"
    "Shelter in the chapels of the Nine. No dead have risen within their walls.<br>\n"
    "Touch nothing a walker carried. What the risen take is spoiled.<br>\n"
    "Report every walker to the nearest guard.<br>\n"
)


def _sheet(headline: str, paragraphs: list[str]) -> str:
    body = "<br>\n<br>\n".join(paragraphs)
    return f"{HEADER}{headline}<br>\n<p><DIV align=\"left\">\n{body}<br>\n<br>\n{LEGION_ASKS}"


def _general() -> tuple[str, str, str]:
    return (
        "WalkersCourierGeneral",
        "The Dead Walk!",
        _sheet(
            "The Dead Walk!",
            [
                "From every road in Cyrodiil the Courier hears the same terrible tale: the dead do not stay dead. "
                "Travellers, bandits and beasts alike fall, and some little while later they rise again - pale, "
                "silent and hungry, shuffling after the living.",
                "The common folk have taken to calling them walkers, and with good reason. They never run, but "
                "they come on without rest, and where one walker is seen, more are never far behind. Shepherds "
                "speak of them gathering in great crowds that drift across the hills together.",
                "The Elder Council has not yet spoken on the matter. The Courier will report on the outbreak in "
                "every edition until it is ended.",
            ],
        ),
    )


def _approaching(key: str, name: str, region: str) -> tuple[str, str, str]:
    title = f"Horde Seen Near {name[0].upper() + name[1:]}!"
    return (
        f"WalkersCourierApproaching{key}",
        title,
        _sheet(
            title,
            [
                f"Travellers arriving in {name} report a great crowd of the walking dead moving through {region}, "
                f"and every report agrees on where they are going: towards {name} itself.",
                "No walker has yet been seen at the city gates. The watch has doubled its patrols on the walls, "
                "and merchants are already asking whether their caravans should wait for the horde to pass.",
                f"The Courier urges every citizen of {name} to keep within the walls after dark, and every "
                "traveller on the roads nearby to turn back to the nearest town.",
            ],
        ),
    )


def _attack(key: str, name: str, region: str) -> tuple[str, str, str]:
    city = name[0].upper() + name[1:]
    title = f"Walkers at the Gates of {city}!"
    return (
        f"WalkersCourierAttack{key}",
        title,
        _sheet(
            title,
            [
                f"The dead have reached {name}. A horde of walkers came out of {region} and gathered before the "
                "city gates, pressing against the walls as if they knew the living were inside.",
                f"The guard of {name} did not wait behind the gates. The watch marched out and met the walkers on "
                "the road, and the Courier is proud to report that the dead were cut down where they stood. Where "
                "a guardsman fell, another took his place at once.",
                "Yet the Courier must warn its readers: more walkers are seen on the roads every day, and the guard "
                f"expects the dead to return to {name}'s gates before long.",
            ],
        ),
    )


def _several() -> tuple[str, str, str]:
    return (
        "WalkersCourierSeveral",
        "The Cities Besieged!",
        _sheet(
            "The Cities Besieged!",
            [
                "It is no longer one city that the dead have found. In the last days, hordes of walkers have stood "
                "before the gates of more than one of the great cities of Cyrodiil.",
                "Everywhere the guards have marched out and held the gates, and everywhere the walkers have fallen "
                "before them. But the guards are tiring, and the dead do not. Messengers from the Imperial City "
                "ride day and night between the cities to keep the counts informed.",
                "The Courier asks its readers to pray to the Nine for the guards at the gates, and to stay off the "
                "roads until the hordes are gone.",
            ],
        ),
    )


def _besieged(key: str, name: str, region: str) -> tuple[str, str, str]:
    city = name[0].upper() + name[1:]
    title = f"{city} Besieged!"
    return (
        f"WalkersCourierBesieged{key}",
        title,
        _sheet(
            title,
            [
                f"A horde of the dead has laid siege to {name}. Walkers in their hundreds came out of {region} and "
                "now stand before one of the city's gates, day and night, and they do not go away.",
                f"The guard of {name} holds that gate against them, and the watch asks every able fighter to come "
                "out and lend a sword. Travellers who need not go that way should use another gate.",
                "Every day the watch marches out against the dead, and every day a few more walkers fall. But the "
                "horde is great, and the Courier cannot say when the gate will open again.",
            ],
        ),
    )


def _lifted(key: str, name: str, region: str) -> tuple[str, str, str]:
    city = name[0].upper() + name[1:]
    title = f"The Siege of {city} Is Over!"
    return (
        f"WalkersCourierLifted{key}",
        title,
        _sheet(
            title,
            [
                f"Good news at last from {name}: the siege is broken. The last of the walkers before the gate have "
                "fallen, and the gate stands open again.",
                f"The people of {name} cheered their guard in the streets. Traders who waited out the siege are "
                f"already on the roads to {region} once more.",
                "The Courier reminds its readers that the dead are still out there. Where one horde has fallen, "
                "another may gather.",
            ],
        ),
    )


def editions() -> list[tuple[str, str, str]]:
    """(editor ID, title, text) in NewsLogic.h EditionIndex order."""
    out = [_general()]
    out += [_approaching(key, name, region) for key, name, region in CITIES]
    out += [_attack(key, name, region) for key, name, region in CITIES]
    out.append(_several())
    out += [_besieged(key, name, region) for key, name, region in CITIES]
    out += [_lifted(key, name, region) for key, name, region in CITIES]
    out += [_camp_saved(), _camp_lost()]
    out.append(_refuges())
    return out


def _refuges() -> tuple[str, str, str]:
    """Refugees hold dungeons (4.20.7): the edition while a held dungeon is
    news and nothing else is to report; reading it tells of that place."""
    return (
        "WalkersCourierRefuges",
        "Refugees Take to the Old Forts!",
        _sheet(
            "Refugees Take to the Old Forts!",
            [
                "With the roads no longer safe and the camps overrun one after another, those who fled the walkers "
                "have found themselves new shelter: the old forts, the mines, the caves and even the ruins of the "
                "Ayleids. Behind thick walls and a single door, they say, a handful can hold out against many.",
                "Not every such place is a safe one. Some are besieged by the dead day and night. Before others the "
                "refugees wait in the open, for bandits or beasts hold what they hoped to make their own. And some "
                "have fallen already, their defenders walking the halls with the rest.",
                "The Courier has word of one such place, and prints it here for the bold and the charitable. Readers "
                "who would help are asked to mark it on their maps.",
            ],
        ),
    )


def dungeon_notice() -> tuple[str, str, str]:
    """The refugees' notice (4.20.7): a paper as an edition; reading it tells
    of a held dungeon."""
    return (
        "WalkersDungeonNotice",
        "Notice: Help Wanted",
        _sheet(
            "To Any Who Can Bear Arms",
            [
                "We are honest folk, driven from our homes by the walking dead. We have found shelter in the wilds, "
                "but we cannot hold it alone.",
                "Whoever reads this and has a strong arm and a kind heart: come and stand with us. We have little, "
                "and we will share it gladly.",
                "The place is marked for you. May the Nine guide your steps.",
            ],
        ),
    )


def _camp_saved() -> tuple[str, str, str]:
    """A refugee camp defended, or its people brought to a city (4.20.6)."""
    return (
        "WalkersCourierCampSaved",
        "Refugees Saved on the Road!",
        _sheet(
            "Refugees Saved on the Road!",
            [
                "Not every tale from the roads ends badly. Travellers report that a band of refugees, camped in the "
                "wilds with little more than a fire and two tents, was saved from the walkers in the last days.",
                "A lone traveller is said to have stood with them when the dead came out of the dark, and to have "
                "fought until the last walker fell. Some say the same hero led a whole camp safely to the gates of "
                "a city.",
                "The Courier salutes such courage, and reminds its readers that many still wait by their fires along "
                "the roads of Cyrodiil, hoping for help.",
            ],
        ),
    )


def _camp_lost() -> tuple[str, str, str]:
    """A refugee camp overrun (4.20.6)."""
    return (
        "WalkersCourierCampLost",
        "A Refugee Camp Overrun",
        _sheet(
            "A Refugee Camp Overrun",
            [
                "Sad news reaches us from the wilds. A camp of refugees who had fled the walkers was found overrun, "
                "its fire cold and its tents empty.",
                "Those who passed it tell of the dead walking among the bedrolls - the refugees themselves, risen "
                "with the rest. Travellers are warned to give the place a wide berth.",
                "The Courier asks anyone who can wield a blade to look in on the camps along the roads. They cannot "
                "hold out alone.",
            ],
        ),
    )


def camp_rumor() -> str:
    """The Rumors topic's line about the refugee camps (4.20.6), said at random
    among the others while [Survivors] Enabled is on."""
    return ("There are refugees camped all along the roads now, people who fled the walkers with nothing. "
            "If you see their fire, look in on them - they could use a sword arm.")


def night_rumor() -> str:
    """The Rumors topic's line about the nights (4.20.5), said at random among
    the others while [Night] Enabled is on."""
    return ("They say the walkers are worse after dark. Quicker on their feet, and more of them on the roads. "
            "Once the sun is down I bar my door and wait for the dawn.")


def rumors() -> list[str]:
    """The Rumors topic's line per city while it is besieged (4.12), in
    CityGates.h's order - said by every NPC who has rumors to tell."""
    out = []
    for _key, name, region in CITIES:
        out.append(f"Have you heard? The walkers have laid siege to {name}. A whole horde came out of {region}, "
                   f"and the guard is holding one of the gates against them. I'd stay away from {name} "
                   "for a while - unless you can swing a sword.")
    return out
