# 🐍 Snake Game (with SDL2 GUI)

This project is a classic Snake game implemented in C with a graphical interface using **SDL2** and **SDL2_ttf**. The snake collects loot, and the game ends when all loot is collected (win) or the timer runs out (lose). The game uses shared memory and semaphores for multiprocess coordination, simulating a mini operating system concept.

## 📦 Features

- 🐍 Snake movement using `W`, `A`, `S`, `D`
- 🍎 Random loot generation using forked child processes
- 🧠 Inter-process synchronization via POSIX shared memory and semaphores
- ⏱️ Timer shown in GUI using SDL2_ttf
- 🎉 Win or lose message displayed before exit

## 🖥️ Requirements

Make sure you have the following libraries installed:

### Linux (Ubuntu/Debian):

```bash
sudo apt-get install libsdl2-dev libsdl2-ttf-dev
```

### macOS (with Homebrew):

```bash
brew install sdl2 sdl2_ttf
```

## ⚙️ Build Instructions

### Using Terminal:

```bash
gcc Game.c -o snake_game_sdl `sdl2-config --cflags --libs` -lSDL2_ttf -lpthread
```

## 🕹️ Controls

| Key | Action        |
|-----|---------------|
| W   | Move Up       |
| A   | Move Left     |
| S   | Move Down     |
| D   | Move Right    |
| ⌘Q / Alt+F4 | Quit |

## 📁 File Overview

| File           | Description                                      |
|----------------|--------------------------------------------------|
| `Game.c`       | Main game logic and SDL rendering                |
| `README.md`    | This file                                        |

## 🧠 Concepts Used

- SDL2 rendering and event handling
- Font rendering with SDL2_ttf
- POSIX shared memory and `semaphore.h`
- Process management with `fork()`
- Terminal input capture using pipes

## 🎮 Snake in Action

<img width="396" alt="Gameplay_SS" src="https://github.com/user-attachments/assets/f616659f-d236-4ae3-b952-f38f482e15bb" />

## 📺 Watch the Gameplay

Check out the full gameplay demo on YouTube:  
👉 [Speed Snake - COMP 173 Final Project](https://youtu.be/0qFFk3LxZRA)

