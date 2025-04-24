// SDL version of the snake game with GUI using SDL2
// rm /dev/shm/snake_lowest_score_shm
// gcc Game.c -o snake_game_sdl `sdl2-config --cflags --libs` -lSDL2_ttf -lpthread

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
#define SHM_SCORE_NAME "/snake_lowest_score_shm"
#define LOOT_SYMBOL '*'
#define SNAKE_HEAD 'S'
#define LOOT_PROCESSES 2
#define GAME_DURATION 300
#define CELL_SIZE 40
#define TEXT_HEIGHT 40
#define HEADER_HEIGHT 40
#define WINDOW_WIDTH (GRID_SIZE * CELL_SIZE)
#define WINDOW_HEIGHT (GRID_SIZE * CELL_SIZE + TEXT_HEIGHT + HEADER_HEIGHT)
#define MAX_NAME_LENGTH 50

typedef struct {
    char grid[GRID_SIZE][GRID_SIZE];
    int snake_x;
    int snake_y;
    int score;
    int direction;
    time_t start_time;
    int game_over;
    char player_name[MAX_NAME_LENGTH];
} GameState;

typedef struct {
    int lowest_score;
    int score_exists;  // Flag to indicate if a previous score exists
    char player_name[MAX_NAME_LENGTH];  // Name of the player with the lowest score
} ScoreState;

GameState *game;
ScoreState *score_state;
sem_t *mutex;
sem_t *score_mutex;
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

    // Display current player name at the top
    char player_text[MAX_NAME_LENGTH + 20];
    snprintf(player_text, sizeof(player_text), "Player: %s", game->player_name);
    draw_text(renderer, font, player_text, 10, 10);

    // Display previous record holder info at the top
    char record_text[MAX_NAME_LENGTH + 50];
    if (score_state->score_exists) {
        snprintf(record_text, sizeof(record_text), "Record: %s - %d", 
                 score_state->player_name, score_state->lowest_score);
    } else {
        snprintf(record_text, sizeof(record_text), "Record: None yet");
    }
    draw_text(renderer, font, record_text, WINDOW_WIDTH - 200, 10);

    // Display grid with offset for the header
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            SDL_Rect cell = { j * CELL_SIZE, i * CELL_SIZE + HEADER_HEIGHT, CELL_SIZE, CELL_SIZE };
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
    snprintf(timer_text, sizeof(timer_text), "Time: %02d:%02d", time_left / 60, time_left % 60);

    char score_text[32];
    snprintf(score_text, sizeof(score_text), "Score: %d", game->score);

    // Draw bottom status bar
    draw_text(renderer, font, timer_text, 10, GRID_SIZE * CELL_SIZE + HEADER_HEIGHT + 10);
    draw_text(renderer, font, score_text, WINDOW_WIDTH - 120, GRID_SIZE * CELL_SIZE + HEADER_HEIGHT + 10);

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

// Initialize the shared memory for the lowest score
int initialize_shared_score() {
    int is_new_score = 0;
    
    // Try to open existing shared memory first
    int shm_score_fd = shm_open(SHM_SCORE_NAME, O_RDWR, 0666);
    
    if (shm_score_fd == -1) {
        // Shared memory doesn't exist, create it
        shm_score_fd = shm_open(SHM_SCORE_NAME, O_CREAT | O_RDWR, 0666);
        ftruncate(shm_score_fd, sizeof(ScoreState));
        is_new_score = 1;
    }
    
    // Map the shared memory segment into our address space
    score_state = mmap(0, sizeof(ScoreState), PROT_READ | PROT_WRITE, MAP_SHARED, shm_score_fd, 0);
    
    // Initialize the score state if it's new
    if (is_new_score) {
        score_state->score_exists = 0;  // No previous score exists
        score_state->lowest_score = 0;  // Initialize to 0 (not used until score_exists = 1)
        strcpy(score_state->player_name, "None");  // Initialize player name
    }
    
    // Create a semaphore for synchronizing access to the score
    sem_unlink("/snake_score_mutex");
    score_mutex = sem_open("/snake_score_mutex", O_CREAT, 0644, 1);
    
    return is_new_score;
}

// Get player name input with basic validation
void get_player_name() {
    printf("Enter your name (max %d characters): ", MAX_NAME_LENGTH - 1);
    
    // Read player name, limiting to MAX_NAME_LENGTH-1 characters to leave room for null terminator
    if (fgets(game->player_name, MAX_NAME_LENGTH, stdin) == NULL) {
        // Error or EOF occurred
        strcpy(game->player_name, "Unknown");
        return;
    }
    
    // Remove newline if present
    size_t len = strlen(game->player_name);
    if (len > 0 && game->player_name[len-1] == '\n') {
        game->player_name[len-1] = '\0';
    }
    
    // If empty name, use default
    if (strlen(game->player_name) == 0) {
        strcpy(game->player_name, "Player");
    }
    
    printf("Welcome, %s! Get ready to play Snake!\n", game->player_name);
}

// Draw the end screen with left-aligned text
void draw_end_screen(SDL_Renderer *renderer, TTF_Font *font, int won, int new_record) {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);
    
    int left_margin = 30;
    int y_pos = WINDOW_HEIGHT / 3;
    int line_spacing = 30;
    
    // Game result message
    char result_msg[64];
    snprintf(result_msg, sizeof(result_msg), "%s", won ? "You Win!" : "Time's Up! You Lose!");
    draw_text(renderer, font, result_msg, left_margin, y_pos);
    y_pos += line_spacing;
    
    // Player score
    char score_msg[128];
    snprintf(score_msg, sizeof(score_msg), "%s's Score: %d", game->player_name, game->score);
    draw_text(renderer, font, score_msg, left_margin, y_pos);
    y_pos += line_spacing;
    
    // Record information - always show the most current information
    char record_msg[128];
    if (score_state->score_exists) {
        snprintf(record_msg, sizeof(record_msg), "Current Record: %s - %d", 
                 score_state->player_name, score_state->lowest_score);
    } else {
        snprintf(record_msg, sizeof(record_msg), "No Record Yet");
    }
    draw_text(renderer, font, record_msg, left_margin, y_pos);
    y_pos += line_spacing;
    
    // New record message if applicable
    if (new_record) {
        char congrats_msg[128];
        snprintf(congrats_msg, sizeof(congrats_msg), 
                 "NEW RECORD! Congratulations %s!", game->player_name);
        draw_text(renderer, font, congrats_msg, left_margin, y_pos);
    }
    
    SDL_RenderPresent(renderer);
}

int main() {
    srand(time(NULL));

    // Initialize the game state shared memory
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, sizeof(GameState));
    game = mmap(0, sizeof(GameState), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    memset(game, 0, sizeof(GameState));

    // Initialize the score shared memory
    int is_first_run = initialize_shared_score();
    
    // If this is the first run, display a message
    if (is_first_run) {
        printf("No previous lowest score found. Starting fresh!\n");
    } else {
        if (score_state->score_exists) {
            printf("Previous record: %s with score %d\n", 
                   score_state->player_name, score_state->lowest_score);
        } else {
            printf("Previous lowest score status: Not available\n");
        }
    }

    // Get player name before starting the game
    get_player_name();

    // Create and initialize mutex for synchronization
    sem_unlink("/snake_mutex");
    mutex = sem_open("/snake_mutex", O_CREAT, 0644, 1);

    // Initialize grid
    for (int i = 0; i < GRID_SIZE; i++)
        for (int j = 0; j < GRID_SIZE; j++)
            game->grid[i][j] = ' ';

    // Initialize snake and game parameters
    game->snake_x = 5;
    game->snake_y = 5;
    game->score = 0;
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
    SDL_Window *window = SDL_CreateWindow("Snake Game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                                         WINDOW_WIDTH, WINDOW_HEIGHT, 0);
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

    // Variable to track if a new record was set
    int new_record = 0;
    
    // Update lowest score ONLY if the game is won
    if (won) {
        sem_wait(score_mutex);
        if (!score_state->score_exists || game->score < score_state->lowest_score) {
            score_state->lowest_score = game->score;
            score_state->score_exists = 1;  // Now we have a score
            
            // Update the record holder's name
            strncpy(score_state->player_name, game->player_name, MAX_NAME_LENGTH - 1);
            score_state->player_name[MAX_NAME_LENGTH - 1] = '\0'; // Ensure null termination
            
            printf("New record! %s achieved lowest score of %d\n", 
                   game->player_name, game->score);
            
            new_record = 1;
        }
        sem_post(score_mutex);
    } else {
        printf("Game lost or timed out. Record not updated.\n");
    }
    
    // Draw the end screen with the updated score information
    draw_end_screen(renderer, font, won, new_record);
    
    sleep(3);

    TTF_CloseFont(font);
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    // Clean up resources
    sem_close(mutex);
    sem_unlink("/snake_mutex");
    sem_close(score_mutex);
    sem_unlink("/snake_score_mutex");
    
    munmap(game, sizeof(GameState));
    shm_unlink(SHM_NAME);
    
    munmap(score_state, sizeof(ScoreState));
    // Note: We don't unlink the score shared memory so it persists
    // between game sessions
    
    return 0;
}
