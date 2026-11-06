# AoK Thor

This fork of [OpenAoK](https://github.com/Zaydme/openaok) plays Age of Empires
II: The Age of Kings (1999) on the AYN Thor, with both of its screens:

- The top screen shows the map only, drawn at exactly twice its size, so every
  pixel of the game's art stays sharp.
- The bottom touch screen holds the interface, drawn in the game's own art:
  resources, the selected unit's card, its commands, the minimap and the
  buttons for the technology tree, chat, diplomacy, objectives and the menu.
- The Thor's controller plays as a mouse and sends the game's own hotkeys.
- An optional CRT look (crt-lottes) covers both screens.

Everything else is OpenAoK: a new engine that reads the game's data from your
own copy of the CD. Neither this repository nor the APK contains any game
file.

<p align="center">
  <img src="docs/thor/top-screen.png" width="640" alt="The map on the Thor's top screen">
  <br>
  <img src="docs/thor/bottom-screen.png" width="413" alt="The interface on the Thor's bottom screen">
</p>

<p align="center">The top and bottom screens during a game, captured on the Thor.</p>

The engine's own documentation is on the
[OpenAoK site](https://zaydme.github.io/openaok/).

## What you need

- An AYN Thor. It was tested on the Thor's Android 13 only.
- Your own copy of Age of Empires II: The Age of Kings, the 1999 CD. It was
  tested with the English CD, version 1.0. The Conquerors and the HD and
  Definitive editions are not supported.
- A computer with Python 3, CMake, cabextract and a C++ compiler, to prepare
  the game's data once.

## Install

### 1. Install the APK

Download the APK from this repository's
[releases](https://github.com/simonbrtrnd/openaok-thor/releases) and open it
on the Thor. Android asks once to allow installs from the file manager.

The app is called AoK Thor. It installs beside the official OpenAoK app
and does not replace it.

### 2. Prepare the game's data

On the computer, clone this repository and copy the whole CD into its
`original_game_1999/` folder.

```sh
git clone -b thor https://github.com/simonbrtrnd/openaok-thor.git
cd openaok-thor
```

Install the tools the script needs:

```sh
# macOS, with the Xcode command-line tools
brew install cmake cabextract

# Debian or Ubuntu
sudo apt install build-essential cmake python3 zlib1g-dev cabextract
```

Then prepare the data:

```sh
python3 android/package.py
```

The script writes `android/build/OpenAoK-data/`, about 350 MB. It holds every
file the game reads and a `manifest.txt` with each file's checksum.

On the 1999 CD, the in-game music is recorded as audio tracks, and an ISO
image keeps only the data part. If your copy has no in-game music, put it as
MP3 files named `MUSIC1.MP3`, `MUSIC2.MP3` and so on in
`original_game_1999/GAME/SOUND/STREAM/` before you run the script. The voices,
sounds and campaign narration are in the data part and need nothing.

### 3. Copy the data to the Thor

Copy the `OpenAoK-data` folder anywhere on the Thor, for example to
`Games/AgeOfKings` on the SD card, over USB or with:

```sh
adb push android/build/OpenAoK-data /sdcard/Download/
```

### 4. Choose the folder

Open AoK Thor. It asks for the game's data: tap Choose folder, open the
`OpenAoK-data` folder itself, tap Use this folder, then Allow. The app checks
every file against the manifest, copies the data into its own storage, and
starts the game. You can then delete the copy you made.

## The controller

The letters are the ones printed on the Thor: B at the bottom, A on the right,
Y on the left and X at the top.

| Button | In the game |
| --- | --- |
| Left stick | Moves the pointer |
| Right stick | Scrolls the map |
| B | Left click; hold it to drag a box or a wall |
| A | Right click: gives an order |
| Y | Pause |
| X | Escape: cancels what is half done |
| Back key | Escape: closes a window, cancels an action |
| L1, held | Shift: adds to the selection, queues orders, trains five units |
| R1 | Goes to the Town Center |
| L2, R2 | Zoom out and in, from 35% to 300%, with a stop at 100, 200 and 300% |
| D-pad up, down | Game speed faster, slower |
| D-pad left | Next idle villager |
| D-pad right | Next idle military unit |
| Left stick click | Centres the view on the selected unit |
| Right stick click | Goes to the last alert |
| Start | Opens or closes the game menu |
| Select | Moves the interface to the top screen, or back to the bottom one |
| Select held, d-pad left or right | CRT mode: Off, Soft, Arcade, TV |
| Select held, d-pad up or down | CRT intensity, in steps of 10% |
| Select held, A, B, X or Y | Selects control group 1, 2, 3 or 4; press twice to go to it |
| Select and L1 held, A, B, X or Y | Saves the selection as control group 1, 2, 3 or 4 |

## Touch on the top screen

| Gesture | In the game |
| --- | --- |
| Tap | Left click |
| Hold a finger still for a moment | Right click; the Thor vibrates |
| Drag one finger | Draws a selection box, or lays a wall |
| Drag two fingers | Scrolls the map |
| Pinch | Zooms in and out, with a stop at 100, 200 and 300% |

While a building waits to be placed, the finger slides it, and lifting the
finger places the building.

## The bottom screen

- A tap on a button or a command uses it. Hold a finger on it to read its help
  on the card instead.
- A tap on the minimap moves the view there.
- A tap on a bar button opens its window, and a second tap closes it.
- With several units selected, a tap on one of their cards keeps only that
  unit. Hold the card to keep every unit of that type, or hold L1 and tap to
  drop that unit from the selection.
- On a building's card, a tap on a slot of its queue cancels one unit of that
  batch and gives its cost back.
- The age, at the top right, is a button: it selects the Town Center and
  starts the next age. This button and Delete ask for a second tap within 3
  seconds.
- The keyboard comes up on the screen you touched last. After the Chat button,
  it comes up on the bottom screen, and the chat stays in view on the top one.
  When it comes up on the top screen, as in the menus, touch the bottom screen
  to bring it there.

## Options

Some options in the game's Options screens matter on the Thor:

- Text Size sets the size of the text written over the map: campaign
  instructions, chat, the game's messages, scores and the help.
- One-Click Garrisoning lets a right click garrison units in a building. The
  original asks you to hold Alt, which the Thor does not have.
- Edge Scrolling scrolls the map when the controller's pointer reaches the
  edge of the top screen. A finger never scrolls the map that way.
- Production Queue shows what your buildings are making at the map's top left.
- The two CRT buttons choose the CRT look's mode and intensity. It starts
  off, because it makes the text harder to read. Arcade is the best balance.
- Top Bar Size and Panel Size count only while the interface is on the top
  screen.

## Network games

The app asks for two permissions only: the network and the vibration motor.
The network opens only in the Multiplayer menu. Do not join games hosted by
strangers: the host sends a saved game that the engine loads without enough
checks, and a bad one can crash the app.

## Build the APK yourself

See [android/README.md](android/README.md). This fork builds with JDK 17, NDK
28.1.13356709 and CMake 3.22.1 from the Android SDK manager:

```sh
cd android
./fetch_deps.sh
./gradlew assembleRelease
```

## Credits and license

- OpenAoK is by Zaydme8 and the OpenAoK contributors, under the MIT license.
  The changes in this fork are under the same license.
- The CRT look is crt-lottes, by Timothy Lottes, in the public domain.
- Age of Empires II is a trademark of Microsoft. This project is not made or
  endorsed by Microsoft or by AYN. You need your own copy of the game.
