#ifndef GAME_H
#define GAME_H

#include "protocol.h" // Pour avoir BOARD_WIDTH et BOARD_HEIGHT

// --- Constantes du Plateau ---
#define TILE_EMPTY 0
#define TILE_P1 1
#define TILE_P2 2
#define TILE_DESTROYED -1

// --- Phases du tour ---
// Dans Isola, un tour se fait en 2 étapes : Bouger puis Détruire
typedef enum {
    PHASE_MOVE = 0,
    PHASE_DESTROY
} GamePhase;

// --- États du Joueur ---
typedef enum {
    STATE_LOBBY = 0, // En attente
    STATE_INGAME     // En jeu
} PlayerState;

// --- Structure Joueur ---
typedef struct {
    int socket;         // L'ID du socket pour lui parler
    int id_db;          // ID dans la base de données (pour les stats)
    char username[32];  
    PlayerState state;
    
    // Position actuelle (x=colonne, y=ligne)
    int x;
    int y;
} Player;

// --- Structure Partie ---
typedef struct {
    int id;
    Player *p1; // Pointeur vers le joueur 1
    Player *p2; // Pointeur vers le joueur 2
    
    int board[BOARD_WIDTH][BOARD_HEIGHT]; // Le plateau 6x8
    
    int current_turn; // 1 pour Joueur 1, 2 pour Joueur 2
    GamePhase phase;  // Est-ce qu'il doit bouger ou détruire ?
    
    int winner;       // 0=Personne, 1=P1, 2=P2
} Game;

// --- Prototypes (Les fonctions qu'on va coder) ---
void game_init(Game *g, Player *p1, Player *p2);
int game_check_move(Game *g, Player *p, int x, int y);
void game_apply_move(Game *g, Player *p, int x, int y);
int game_check_destroy(Game *g, Player *p, int x, int y);
void game_apply_destroy(Game *g, int x, int y);
int game_check_loss(Game *g, Player *p); // Renvoie 1 si le joueur a perdu (bloqué)

#endif