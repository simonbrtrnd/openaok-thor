# AoK Thor

Age of Empires II: The Age of Kings (1999) on the AYN Thor, with both screens.
It is a fork of [OpenAoK](https://github.com/Zaydme/openaok), a new engine that
plays the game from your own CD.

- The top screen shows the map, at exactly twice its size, so the art stays
  sharp.
- The bottom touch screen holds the interface, in the game's own art.
- The Thor's controller plays as a mouse and sends the game's hotkeys.
- An optional CRT look covers both screens.

It also brings smaller improvements:

- Zoom from 35% to 300%, with a stop at 100, 200 and 300%.
- Control groups on the controller.
- All windows drawn at one size, as large as the screen allows. The
  technology tree scrolls with a finger or the right stick.
- Campaigns open on your own units.
- Edge scrolling with the controller's pointer, as an option.

<p align="center">
  <img src="docs/thor/top-screen.png" width="640" alt="The map on the Thor's top screen">
  <br>
  <img src="docs/thor/bottom-screen.png" width="413" alt="The interface on the Thor's bottom screen">
</p>

## What you need

- An AYN Thor. Only its Android 13 was tested.
- The 1999 Age of Kings CD. The English version 1.0 was tested. The
  Conquerors, HD and Definitive editions do not work.
- A computer with Python 3, CMake, cabextract and a C++ compiler, once.

This repository and the APK contain no game files.

## Install

1. Download the APK from the
   [releases](https://github.com/simonbrtrnd/openaok-thor/releases) and open it
   on the Thor. The app is called AoK Thor and installs beside the official
   OpenAoK app.

2. On the computer, prepare the game's data from your CD:

   ```sh
   git clone -b thor https://github.com/simonbrtrnd/openaok-thor.git
   cd openaok-thor
   # copy the whole CD into original_game_1999/
   brew install cmake cabextract    # or, on Debian/Ubuntu: sudo apt install build-essential cmake python3 zlib1g-dev cabextract
   python3 android/package.py
   ```

   The data goes to `android/build/OpenAoK-data/`, about 350 MB.

3. Copy the `OpenAoK-data` folder anywhere on the Thor, such as the SD card.

4. Open AoK Thor, tap Choose folder, open `OpenAoK-data`, then tap Use this
   folder and Allow. The app copies the data and starts.

The CD holds the in-game music as audio tracks, which an ISO image does not
keep. To have it, put MP3 files named `MUSIC1.MP3`, `MUSIC2.MP3` and so on in
`original_game_1999/GAME/SOUND/STREAM/` before step 2.

## The controller

The letters are the ones printed on the Thor: B at the bottom, A on the right,
Y on the left, X at the top.

| Button | Does |
| --- | --- |
| Left stick | Pointer |
| Right stick | Scrolls the map |
| B | Left click; hold to drag a box or a wall |
| A | Right click |
| Y | Pause |
| X, Back key | Escape |
| L1, held | Shift |
| R1 | Town Center |
| L2, R2 | Zoom out, in (35% to 300%, stops at 100, 200, 300%) |
| D-pad up, down | Game speed |
| D-pad left, right | Next idle villager, next idle soldier |
| Left stick click | Selected unit |
| Right stick click | Last alert |
| Start | Game menu |
| Select | Moves the interface to the top screen and back |
| Select held + d-pad | CRT mode (left, right) and intensity (up, down) |
| Select held + A, B, X, Y | Control group 1 to 4; twice to go to it |
| Select and L1 held + A, B, X, Y | Saves control group 1 to 4 |

## Touch

On the top screen:

| Gesture | Does |
| --- | --- |
| Tap | Left click |
| Hold | Right click |
| Drag one finger | Selection box, or a wall |
| Drag two fingers | Scrolls the map |
| Pinch | Zoom |

On the bottom screen:

| Gesture | Does |
| --- | --- |
| Tap a button | Uses it; a bar button tapped again closes its window |
| Hold a button | Shows its help |
| Tap the minimap | Moves the view |
| Tap a unit in a group | Keeps only that unit; L1 + tap drops it |
| Hold a unit in a group | Keeps every unit of its type |
| Tap a queued unit | Cancels it |
| Tap the age | Starts the next age; tap twice, as for Delete |

The keyboard opens on the bottom screen whenever it can. Android shows it only
on the screen touched last, so if it opens on the top screen, tap the bottom
screen to bring it down.

## Options

- Text Size: the text over the map.
- One-Click Garrisoning: garrison with a right click, as the Thor has no Alt.
- Edge Scrolling: the pointer at the screen's edge scrolls the map.
- CRT: its mode and intensity. Off at first, as it makes text harder to read.

## Permissions

The app asks for the network, for multiplayer only, and for vibration. Do not
join games hosted by strangers: a bad saved game from the host can crash the
app.

## Build the APK

With JDK 17, NDK 28.1.13356709 and CMake 3.22.1 from the Android SDK manager:

```sh
cd android
./fetch_deps.sh
./gradlew assembleRelease
```

See [android/README.md](android/README.md) for details.

## Feedback

Feedback is welcome: a bug, an idea or a question. Open an
[issue](https://github.com/simonbrtrnd/openaok-thor/issues).

## Credits and license

- OpenAoK is by Zaydme8 and the OpenAoK contributors, under the MIT license,
  as are this fork's changes. Its documentation is on the
  [OpenAoK site](https://zaydme.github.io/openaok/).
- The CRT look is crt-lottes, by Timothy Lottes, in the public domain.
- Age of Empires II is a trademark of Microsoft. This project is not made or
  endorsed by Microsoft or AYN.
