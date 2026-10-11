Crafti Survival Edition
=======================

A survival-mode fork of [crafti](https://github.com/Vogtinator/crafti) by Fabian Vogt: 3D Minecraft for TI-Nspire CX calculators running Ndless.
The goal is 1.8.8-style survival play (items, mining, mobs, and difficulty levels) on a calculator.

> **Status:** survival basics are playable: hold-to-mine with tool tiers, block drops, a day/night cycle,
> crafting, chests, beds and the smithing table. Furnaces, health and hunger, and mobs are next.
> Every screenshot below is from the current build.

Screenshots
-----------

| | |
|---|---|
| ![Survival HUD](docs/screenshots/world-survival.png) | ![Creative hotbar](docs/screenshots/world-creative.png) |
| Survival: hearts, hunger and the 9-slot hotbar | Creative: the same hotbar, without hearts and hunger |
| ![Game menu](docs/screenshots/menu.png) | ![Options](docs/screenshots/options.png) |
| Game menu | Options, with Auto-jump and Game Mode |
| ![Inventory](docs/screenshots/inventory.png) | ![Crafting table](docs/screenshots/crafting.png) |
| Survival inventory with the simple recipes | Crafting table: every recipe, with what each one takes |
| ![Block list](docs/screenshots/block-list.png) | ![Help](docs/screenshots/help.png) |
| Creative block list | Help & Controls |

All UI art is original: the pixel font, hearts, hunger, item icons and the title wordmark are drawn by scripts in `tools/` (previews in `tools/preview/`). Nothing is copied from Minecraft or Eaglercraft.

What's new so far
-----------------

- **Eaglercraft-style UI:**
  - a 9-slot hotbar with item counts and wear bars
  - hearts, hunger and air bubbles
  - an inventory screen
  - button menus and a pixel font with drop shadows
- **Items:** 53 items, each with its own icon: tools in six materials, food, ores and materials.
- **Game mode:** switch between Survival and Creative in Options (locked in Hardcore).
- **Game clock:** fixed 20 ticks per second, independent of frame rate.
- **Entities:** up to 32 entities with physics and collision.
- **Saves:** new save format (v7) with an automatic backup. An unreadable save is never overwritten, and old v6 worlds still load (in creative mode).
- **Lighting:** 8 brightness levels for the coming day/night cycle.
- **Auto-jump:** walks you up one-block steps.
- **Mining:** hold 9 to break blocks at 1.8.8 speeds. Tools have tiers and wear out, and blocks drop items you walk over to collect.
- **World:** a sun and moon with a day/night cycle, more common emeralds, Ancient Debris near bedrock, and saplings that grow into trees.
- **Crafting:** a recipe list instead of a 3x3 grid. Your inventory makes planks, sticks, torches and the crafting table; press 7 on a crafting table for tools, furnaces, chests, beds, storage blocks and more.
- **Chests, beds and the smithing table:** chests hold 27 stacks, beds skip the night and set your spawn, and the smithing table turns diamond tools into netherite.
- **Stress test:** `crafti-stress.tns` shows FPS and ticks per second on the calculator.

Planned
-------

- Furnaces and smelting
- Health, hunger and damage
- Zombies, creepers, skeletons, passive animals and a wandering trader
- Difficulties: Peaceful, Easy, Normal, Hard and Torment, plus a Hardcore switch

Building
--------

See [docs/building.md](docs/building.md). GitHub Actions builds `crafti.tns` and `crafti-stress.tns` on every push (see the **Actions** tab).

Controls
--------

| Key | In the world | In screens |
|---|---|---|
| 8 4 6 2 | Walk (auto-jump climbs steps) | Move the cursor / pick a recipe |
| 5 | Jump | Take or put down a stack / craft one (hold to repeat) |
| 7 | Place block, or use a chest, crafting table, bed or smithing table | Take half, or put down one |
| 9 | Break block (hold in survival) | |
| 1 / 3 | Previous / next hotbar slot | Pick the hotbar slot (block list) |
| . | Inventory (creative: block list) | Close |
| enter | | Send a stack to the other section / craft as many as you can |
| tab | | Switch between the slots and the recipe list |
| menu | Game menu | Back |
| esc | Save and quit | Close |
| + / - | View distance | |
| ctrl + . | Screenshot | |

Limitations
-----------

crafti doesn't use floats, so there will be some graphical inaccuracies.

Credits
-------

Original crafti by [Fabian Vogt (Vogtinator)](https://github.com/Vogtinator/crafti); ticalc.org Program of the Year 2014.
Block textures from PureBDcraft by https://bdcraft.net.
