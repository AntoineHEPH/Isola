// protocol.h
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

// Taille du plateau Isola (6x8)
#define BOARD_WIDTH 8
#define BOARD_HEIGHT 6

// --- 1. Les Types de Messages (Commandes) ---
// C'est ce qui permet au serveur de savoir "Qu'est-ce qu'on me veut ?"
// protocol.h

typedef enum {
    // Connexion / Lobby
    REQ_LOGIN = 1,
    RES_LOGIN_OK = 2,
    RES_LOGIN_FAIL = 3,

    // Jeu
    NOTIF_GAME_START = 4,
    REQ_MOVE = 5,
    REQ_DESTROY = 6,
    RES_MOVE_OK = 7,
    RES_MOVE_ERR = 8,
    NOTIF_OPP_MOVE = 9,

    // Fin
    NOTIF_GAME_OVER = 10,
    REQ_LOGOUT = 11,

    NOTIF_DESTROY = 12

} MessageType;

// --- 2. La Structure du Message (Le "Paquet") ---
// On utilise __attribute__((packed)) pour que le C n'ajoute pas d'espaces vides (padding).
// C'est CRUCIAL pour que Python puisse lire les bytes correctement.

typedef struct __attribute__((packed)) {
    int type;           // Un des codes MessageType ci-dessus
    int client_id;      // ID du joueur (0 si pas encore connecté)

    // Données génériques (on réutilise ces champs selon le type)
    int val1;           // Ex: Coordonnée X ou ID partie
    int val2;           // Ex: Coordonnée Y
    int val3;           // Ex: Score ou autre info

    char text[64];      // Ex: Nom d'utilisateur, Message d'erreur, Chat
} GameMessage;

#endif //PROTOCOL_H