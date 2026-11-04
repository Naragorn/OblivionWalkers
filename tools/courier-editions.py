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
                f"The guard of {name} has shut and barred that gate. No one goes in or out by it until the siege is "
                "over - travellers must use another gate, and the Courier strongly advises them to.",
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
    return out
