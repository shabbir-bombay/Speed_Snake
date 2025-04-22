// SDL version of the snake game with GUI using SDL2
// Updated: Timer moved to bottom margin of screen

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <semaphore.h>
#include <termios.h>
#include <signal.h>
#include <time.h>

#define GRID_SIZE 10
#define SHM_NAME "/snake_game_shm"
#define LOOT_SYMBOL '*'
#define SNAKE_HEAD 'S'
#define LOOT_PROCESSES 2
#define GAME_DURATION 300
#define CELL_SIZE 40
#define TEXT_HEIGHT 40
#define WINDOW_WIDTH (GRID_SIZE * CELL_SIZE)
#define WINDOW_HEIGHT (GRID_SIZE * CELL_SIZE + TEXT_HEIGHT)

typedef struct {
    char grid[GRID_SIZE][GRID_SIZE];
    int snake_x;
    int snake_y;
    int score;
    int direction;
    time_t start_time;
    int game_over;
} GameState;

GameState *game;
sem_t *mutex;
int pipefd[2];

void place_loot(int count) {
    int placed = 0, attempts = 0;
    while (placed < count && attempts++ < 500) {
        int x = rand() % GRID_SIZE;
        int y = rand() % GRID_SIZE;
        if (game->grid[x][y] == ' ') {
            game->grid[x][y] = LOOT_SYMBOL;
            placed++;
        }
    }
}

void loot_process() {
    while (1) {
        sleep(5);
        sem_wait(mutex);
        place_loot(3);
        sem_post(mutex);
    }
}

void draw_text(SDL_Renderer *renderer, TTF_Font *font, const char *text, int x, int y) {
    SDL_Color color = {0, 0, 0};
    SDL_Surface *surface = TTF_RenderText_Solid(font, text, color);
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Rect dstrect = {x, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, NULL, &dstrect);
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

void draw_grid_sdl(SDL_Renderer *renderer, TTF_Font *font) {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);

    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            SDL_Rect cell = { j * CELL_SIZE, i * CELL_SIZE, CELL_SIZE, CELL_SIZE };
            if (game->grid[i][j] == SNAKE_HEAD)
                SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
            else if (game->grid[i][j] == LOOT_SYMBOL)
                SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            else
                SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
            SDL_RenderFillRect(renderer, &cell);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderDrawRect(renderer, &cell);
        }
    }

    int time_left = GAME_DURATION - (int)(time(NULL) - game->start_time);
    if (time_left < 0) time_left = 0;
    char timer_text[32];
    snprintf(timer_text, sizeof(timer_text), "Time Left: %02d:%02d", time_left / 60, time_left % 60);
    draw_text(renderer, font, timer_text, 10, GRID_SIZE * CELL_SIZE + 10);

    SDL_RenderPresent(renderer);
}

void move_snake() {
    int new_x = game->snake_x;
    int new_y = game->snake_y;
    switch (game->direction) {
        case 0: new_x--; break;
        case 1: new_x++; break;
        case 2: new_y--; break;
        case 3: new_y++; break;
    }
    if (new_x < 0 || new_x >= GRID_SIZE || new_y < 0 || new_y >= GRID_SIZE) return;
    if (game->grid[new_x][new_y] == LOOT_SYMBOL) game->score++;
    game->grid[game->snake_x][game->snake_y] = ' ';
    game->snake_x = new_x;
    game->snake_y = new_y;
    game->grid[new_x][new_y] = SNAKE_HEAD;
}

int check_loot_remaining() {
    for (int i = 0; i < GRID_SIZE; i++)
        for (int j = 0; j < GRID_SIZE; j++)
            if (game->grid[i][j] == LOOT_SYMBOL)
                return 1;
    return 0;
}

int main() {
    srand(time(NULL));

    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, sizeof(GameState));
    game = mmap(0, sizeof(GameState), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    memset(game, 0, sizeof(GameState));

    sem_unlink("/snake_mutex");
    mutex = sem_open("/snake_mutex", O_CREAT, 0644, 1);

    for (int i = 0; i < GRID_SIZE; i++)
        for (int j = 0; j < GRID_SIZE; j++)
            game->grid[i][j] = ' ';

    game->snake_x = 5;
    game->snake_y = 5;
    game->score = 1;
    game->grid[5][5] = SNAKE_HEAD;
    game->start_time = time(NULL);

    place_loot(10);

    for (int i = 0; i < LOOT_PROCESSES; i++) {
        if (fork() == 0) {
            loot_process();
            exit(0);
        }
    }

    pipe(pipefd);
    if (fork() == 0) {
        close(pipefd[0]);
        char input;
        while (1) {
            read(STDIN_FILENO, &input, 1);
            write(pipefd[1], &input, 1);
        }
    }
    close(pipefd[1]);

    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    SDL_Window *window = SDL_CreateWindow("Snake Game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    TTF_Font *font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 18);

    char dir;
    int running = 1;
    int won = 0;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
            else if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_w: game->direction = 0; break;
                    case SDLK_s: game->direction = 1; break;
                    case SDLK_a: game->direction = 2; break;
                    case SDLK_d: game->direction = 3; break;
                }
            }
        }

        sem_wait(mutex);
        if (!check_loot_remaining()) {
            game->game_over = 1;
            running = 0;
            won = 1;
        }

        if (time(NULL) - game->start_time >= GAME_DURATION) {
            game->game_over = 1;
            running = 0;
            won = 0;
        }

        move_snake();
        draw_grid_sdl(renderer, font);
        sem_post(mutex);

        usleep(150000);
    }

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);
    const char *msg = won ? "You Win!" : "Time's Up! You Lose!";
    draw_text(renderer, font, msg, WINDOW_WIDTH / 2 - 70, WINDOW_HEIGHT / 2);
    SDL_RenderPresent(renderer);
    sleep(3);

    TTF_CloseFont(font);
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    sem_close(mutex);
    sem_unlink("/snake_mutex");
    munmap(game, sizeof(GameState));
    shm_unlink(SHM_NAME);

    return 0;
}
