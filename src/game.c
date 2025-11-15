//
// Created by PX on 15/11/2025.
//
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../include/game.h"

// Fonction utilitaire interne : Est-ce qu'on est dans la grille ?
static int is_inside(int x, int y) {
    return (x >= 0 && x < BOARD_WIDTH && y >= 0 && y < BOARD_HEIGHT);
}

// Check si la case est une case d'origine
static int is_origin(int x, int y) {
    return ((x == 0 && y == 3) || (x == 7 && y == 2));
}

// Initialise une nouvelle partie
void game_init(Game *g, Player *p1, Player *p2) {
    // 1. Nettoyage de la mémoire (tout à 0)
    memset(g, 0, sizeof(Game));

    // 2. Assignation des joueurs
    g->p1 = p1;
    g->p2 = p2;

    // 3. Placement initial (Règles Isola)
    // P1 à gauche au milieu
    p1->x = 0;     // WIDTH
    p1->y = 3;

    // P2 à droite au milieu
    p2->x = 7;
    p2->y = 2;

    // 4. Mise à jour du plateau
    g->board[p1->x][p1->y] = TILE_P1;
    g->board[p2->x][p2->y] = TILE_P2;

    // 5. C'est à P1 de commencer par bouger
    g->current_turn = 1;
    g->phase = PHASE_MOVE;
}

// Vérifie si un mouvement est légal (sans le jouer)
int game_check_move(Game *g, Player *p, int dest_x, int dest_y) {
    // 1. Est-ce dans le plateau ?
    if (!is_inside(dest_x, dest_y)) return 0;

    // 2. Est-ce que la case est libre ? (Ni joueur, ni trou)
    if (g->board[dest_x][dest_y] != TILE_EMPTY) return 0;

    // 3. Est-ce que c'est une case adjacente (Diagonale comprise) ?
    // La distance absolue en X et en Y doit être <= 1
    int diff_x = abs(p->x - dest_x);
    int diff_y = abs(p->y - dest_y);

    // On ne peut pas rester sur place (0,0) et on ne peut pas sauter (>1)
    if (diff_x > 1 || diff_y > 1) return 0;
    if (diff_x == 0 && diff_y == 0) return 0;

    return 1; // Mouvement valide
}

// Applique le mouvement
void game_apply_move(Game *g, Player *p, int x, int y) {
    // On vide l'ancienne case
    g->board[p->x][p->y] = TILE_EMPTY;

    // On met à jour les coordonnées du joueur
    p->x = x;
    p->y = y;

    // On remplit la nouvelle case (1 ou 2)
    int player_id = (p == g->p1) ? TILE_P1 : TILE_P2;
    g->board[x][y] = player_id;

    // On change la phase : maintenant il doit détruire
    g->phase = PHASE_DESTROY;
}

// Vérifie si on peut détruire une case
int game_check_destroy(Game *g, Player *p, int x, int y) {
    // 1. Dans le plateau ?
    if (!is_inside(x, y)) return 0;

    // 2. La case doit être vide (on ne détruit pas un joueur ni une case déjà détruite)
    if (g->board[x][y] != TILE_EMPTY) return 0;

    // 3. La case ne peut pas être une case d'origine (apparition des joueurs)
    if (is_origin(x, y)) return 0;

    return 1;
}

// Applique la destruction
void game_apply_destroy(Game *g, int x, int y) {
    g->board[x][y] = TILE_DESTROYED;

    // Fin du tour : on passe la main à l'autre joueur
    g->current_turn = (g->current_turn == 1) ? 2 : 1;
    g->phase = PHASE_MOVE;
}

// Vérifie si le joueur est bloqué (Défaite)
int game_check_loss(Game *g, Player *p) {
    // On regarde les 8 cases autour de lui
    // x de -1 à +1, y de -1 à +1
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {

            if (dx == 0 && dy == 0) continue; // On ne teste pas sa propre case

            int tx = p->x + dx;
            int ty = p->y + dy;

            // Si on trouve UNE case valide (dans le plateau et vide), il n'a pas perdu
            if (is_inside(tx, ty) && g->board[tx][ty] == TILE_EMPTY) {
                return 0; // Il peut encore bouger
            }
        }
    }
    // Si on a fait le tour et qu'on a rien trouvé : il est bloqué
    return 1;
}