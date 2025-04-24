# 🐍 Speed Snake — COMP 173 Final Project

## 🎮 Game Overview

### Game Title
**Speed Snake**

### Game Summary
Speed Snake is a real-time, arcade-style twist on the classic snake game built entirely in C. The game was developed as a final project for an Operating Systems course, featuring concurrency and synchronization to simulate real-world process behavior. Players navigate the snake to collect loot under time pressure, with a dynamic GUI rendered using SDL2.

### Core Gameplay Loop
Players use the keyboard to move the snake across a grid, collecting randomly appearing loot. A timer constantly ticks down, creating urgency. The game ends when the player either collects all loot (win) or the timer expires (lose). Loot spawns are managed by background processes.

---

## 🎮 Snake in Action

<img width="396" alt="Gameplay_SS" src="https://github.com/user-attachments/assets/f616659f-d236-4ae3-b952-f38f482e15bb" />

---

## 🎥 Demo Video
Watch the full gameplay demo on YouTube:  
👉 [Speed Snake - COMP 173 Final Project](https://youtu.be/btDZ2FZJvQY)

---

## 🖥️ Requirements & 🛠️ Setup

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

---

## 🕹️ Gameplay Mechanics

### Controls
| Key | Action        |
|-----|---------------|
| W   | Move Up       |
| A   | Move Left     |
| S   | Move Down     |
| D   | Move Right    |
| ⌘Q / Alt+F4 | Quit |

### Core Mechanics
- Snake movement across a grid
- Random loot generation
- Timer countdown
- Collision with loot increases score

### Level Progression
There is a single level/grid. The challenge increases with time pressure.

### Win/Loss Conditions
- **Win**: Collect all loot before the timer runs out.
- **Lose**: Time expires before collecting all loot.

---

## 💻 Operating Systems Concepts Used

### 1. Shared Memory
- **Game Mechanic**: Shared game state accessed by multiple processes and persistent lowest score tracking
- **Snippet**: `mmap()` used to create shared memory segments `/snake_game_shm` and `/snake_lowest_score_shm`

### 2. Semaphores
- **Game Mechanic**: Prevent concurrent write conflicts to game state and score records
- **Snippet**: `sem_wait(mutex); ... sem_post(mutex);`

### 3. Forking
- **Game Mechanic**: Background processes spawn loot at intervals
- **Snippet**: `if (fork() == 0) { loot_process(); exit(0); }`

### 4. Pipes
- **Game Mechanic**: Handles keyboard input from a separate child process
- **Snippet**: `pipe(pipefd); read(pipefd[0], &input, 1);`

### 5. Timer
- **Game Mechanic**: Game duration countdown and GUI time tracking
- **Snippet**: `time(NULL) - game->start_time`

---

## 🆕 Extra Features

- 🧑‍🎮 **Player Name Input**: Personalizes gameplay and scorekeeping
- 🏆 **Record Tracker**: Shows and updates lowest score record across sessions
- 📈 **Score + Timer UI**: Shown dynamically during gameplay
- ♻️ **Shared Memory Persistence**: Record survives multiple runs

---

## 🧹 Shared Memory Cleanup

If you want to manually delete the shared memory used by the game:

### Remove game state memory:
```bash
rm /dev/shm/snake_game_shm
```

### Remove score tracking memory:
```bash
rm /dev/shm/snake_lowest_score_shm
```

These commands are useful if you want to reset the game state and lowest score record.
