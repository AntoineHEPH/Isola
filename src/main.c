#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <poll.h>
#include <errno.h>

// Inclusion des headers du projet
#include "../include/config.h"
#include "../include/protocol.h"
#include "../include/game.h"

// --- VARIABLES GLOBALES ---

// Tableau de tous les joueurs potentiels
Player clients[MAX_CLIENTS];

// Tableau pour poll() : gère les sockets ouverts
struct pollfd fds[MAX_CLIENTS + 1];
int nfds = 0; // Nombre de sockets surveillés

// Tableau des parties en cours
// (Si on a 100 clients max, on peut avoir max 50 parties)
Game games[MAX_CLIENTS / 2];

// Pointeur vers le joueur qui attend actuellement dans le lobby
Player *waiting_player = NULL;


// --- FONCTIONS UTILITAIRES ---

// Trouve la partie associée à un joueur
Game* find_game_of_player(Player *p) {
    for (int i = 0; i < MAX_CLIENTS / 2; i++) {
        if (games[i].p1 == p || games[i].p2 == p) {
            return &games[i];
        }
    }
    return NULL;
}

// Trouve un emplacement mémoire libre pour créer une partie
Game* create_game_slot() {
    for (int i = 0; i < MAX_CLIENTS / 2; i++) {
        // On considère qu'une partie est vide si p1 est NULL
        if (games[i].p1 == NULL) {
            return &games[i];
        }
    }
    return NULL;
}

// Helper pour envoyer un message structuré
void send_msg(int socket, int type, int v1, int v2, int v3, const char *text) {
    GameMessage msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = type;
    msg.val1 = v1;
    msg.val2 = v2;
    msg.val3 = v3;
    if (text) strncpy(msg.text, text, 63);

    if (send(socket, &msg, sizeof(msg), 0) < 0) {
        perror("Erreur send_msg");
    }
}

// Initialise le socket d'écoute du serveur
int setup_server_socket() {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;

    // 1. Socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Echec socket");
        exit(EXIT_FAILURE);
    }

    // 2. Options (Reuse address pour éviter l'erreur "Address already in use")
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("Echec setsockopt");
        exit(EXIT_FAILURE);
    }

    // 3. Bind
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Toutes les interfaces
    address.sin_port = htons(PORT);       // Port 55555

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Echec bind");
        exit(EXIT_FAILURE);
    }

    // 4. Listen
    if (listen(server_fd, 10) < 0) {
        perror("Echec listen");
        exit(EXIT_FAILURE);
    }

    printf("--- SERVEUR ISOLA DÉMARRÉ SUR LE PORT %d ---\n", PORT);
    return server_fd;
}

// --- LOGIQUE DE JEU ET MATCHMAKING ---

void attempt_matchmaking(Player *p) {
    // Protection : Si le joueur est déjà en jeu, on ne fait rien
    if (p->state == STATE_INGAME) return;

    printf("[MATCHMAKING] Demande de %s...\n", p->username);

    // CAS 1 : Personne n'attend -> Ce joueur se met en attente
    if (waiting_player == NULL) {
        waiting_player = p;
        p->state = STATE_LOBBY;

        printf("-> Mis en file d'attente.\n");
        send_msg(p->socket, RES_LOGIN_OK, 0, 0, 0, "En attente d'un adversaire...");
    }
    // CAS 2 : Quelqu'un attend -> On crée la partie !
    else {
        // Éviter de jouer contre soi-même (bug possible si double clic client)
        if (waiting_player == p) return;

        Player *opponent = waiting_player;

        // Trouver un slot
        Game *new_game = create_game_slot();
        if (new_game == NULL) {
            printf("ERREUR : Serveur plein, impossible de créer une partie.\n");
            send_msg(p->socket, RES_LOGIN_FAIL, 0, 0, 0, "Serveur plein.");
            return;
        }

        // Init de la partie (via src/game.c)
        game_init(new_game, opponent, p);

        // Mise à jour des états
        opponent->state = STATE_INGAME;
        p->state = STATE_INGAME;

        // La file d'attente est vidée
        waiting_player = NULL;

        printf("-> PARTIE LANCÉE : %s (P1) vs %s (P2)\n", opponent->username, p->username);

        // Notifier P1 (Opponent)
        // val1 = ID Joueur (1), val2 = Largeur, val3 = Hauteur
        send_msg(opponent->socket, NOTIF_GAME_START, 1, BOARD_WIDTH, BOARD_HEIGHT, p->username);

        // Notifier P2 (Current)
        // val1 = ID Joueur (2)
        send_msg(p->socket, NOTIF_GAME_START, 2, BOARD_WIDTH, BOARD_HEIGHT, opponent->username);
    }
}

void handle_disconnect(int index_in_poll) {
    int socket = fds[index_in_poll].fd;
    Player *p = NULL;

    // Retrouver le joueur
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket == socket) {
            p = &clients[i];
            break;
        }
    }

    if (p) {
        printf("Déconnexion de %s (Socket %d)\n", (p->username[0] ? p->username : "Inconnu"), socket);

        // Si c'était lui qui attendait, on libère la place
        if (waiting_player == p) {
            waiting_player = NULL;
            printf("-> Il était en file d'attente. File vidée.\n");
        }

        // TODO: S'il était en jeu, gérer le forfait / fin de partie pour l'adversaire
        // (On fera ça dans une prochaine étape)

        // Nettoyage structure joueur
        memset(p, 0, sizeof(Player));
    }

    close(socket);

    // Retrait du tableau poll (on remplace par le dernier pour boucher le trou)
    fds[index_in_poll] = fds[nfds - 1];
    nfds--;
}

// --- MAIN ---

int main() {
    // 1. Setup Réseau
    int server_fd = setup_server_socket();

    // 2. Init structures
    memset(clients, 0, sizeof(clients));
    memset(games, 0, sizeof(games));

    // 3. Init Poll (Le slot 0 est pour le serveur)
    memset(fds, 0, sizeof(fds));
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;
    nfds = 1;

    printf("Serveur prêt. En attente de connexions...\n");

    // 4. Boucle principale
    while (1) {
        // Attente d'événements (-1 = infini)
        int poll_count = poll(fds, nfds, -1);

        if (poll_count < 0) {
            perror("Erreur poll");
            break;
        }

        // Parcours des sockets actifs
        for (int i = 0; i < nfds; i++) {

            // Si aucun événement sur ce socket, on passe
            if (fds[i].revents == 0) continue;

            // --- CAS A : Nouvelle Connexion (Sur le socket serveur) ---
            if (fds[i].fd == server_fd) {
                struct sockaddr_in cli_addr;
                socklen_t len = sizeof(cli_addr);
                int new_sock = accept(server_fd, (struct sockaddr *)&cli_addr, &len);

                if (new_sock >= 0) {
                    printf("Nouvelle connexion IP: %s\n", inet_ntoa(cli_addr.sin_addr));

                    // Ajout à poll
                    if (nfds < MAX_CLIENTS) {
                        fds[nfds].fd = new_sock;
                        fds[nfds].events = POLLIN;
                        nfds++;

                        // Trouver une place dans clients[]
                        for (int j = 0; j < MAX_CLIENTS; j++) {
                            if (clients[j].socket == 0) {
                                clients[j].socket = new_sock;
                                clients[j].state = STATE_LOBBY;
                                // Nom vide pour l'instant
                                break;
                            }
                        }
                    } else {
                        printf("Refus : Serveur plein.\n");
                        close(new_sock);
                    }
                }
            }
            // --- CAS B : Message reçu d'un Client ---
            else if (fds[i].revents & POLLIN) {
                GameMessage msg;
                // On essaie de lire la taille exacte d'une structure GameMessage
                int n = recv(fds[i].fd, &msg, sizeof(msg), 0);

                if (n <= 0) {
                    // Erreur ou Déconnexion (0)
                    handle_disconnect(i);
                    i--; // Important car on a modifié le tableau fds !
                } else {
                    // --- TRAITEMENT DU MESSAGE ---

                    // 1. Retrouver le pointeur du joueur
                    Player *p = NULL;
                    for (int j = 0; j < MAX_CLIENTS; j++) {
                        if (clients[j].socket == fds[i].fd) {
                            p = &clients[j];
                            break;
                        }
                    }

                    if (p) {
                        // Logique selon le type de message
                        switch (msg.type) {
                            case REQ_LOGIN:
                                strncpy(p->username, msg.text, 31);
                                p->username[31] = '\0';
                                printf("Client identifié : %s\n", p->username);
                                attempt_matchmaking(p);
                                break;

                            case REQ_MOVE: { // Ajoute des accolades pour les variables locales
                                Game *g = find_game_of_player(p);
                                if (!g) break;

                                // 1. Vérif Tour
                                int player_num = (p == g->p1) ? 1 : 2;
                                if (g->current_turn != player_num) {
                                    send_msg(p->socket, RES_MOVE_ERR, 0, 0, 0, "Pas ton tour !");
                                    break;
                                }

                                // 2. Vérif Phase (C'EST ÇA QUI EMPÊCHE LE MOUVEMENT INFINI)
                                if (g->phase != PHASE_MOVE) {
                                    send_msg(p->socket, RES_MOVE_ERR, 0, 0, 0, "Tu dois détruire une case !");
                                    break;
                                }

                                // 3. Logique Mouvement
                                if (game_check_move(g, p, msg.val1, msg.val2)) {
                                    game_apply_move(g, p, msg.val1, msg.val2);

                                    // Confirmer au joueur + Dire de passer en mode destruction
                                    send_msg(p->socket, RES_MOVE_OK, msg.val1, msg.val2, 0, "Bravo. Détruis une case !");

                                    // Avertir l'adversaire
                                    Player *opp = (p == g->p1) ? g->p2 : g->p1;
                                    send_msg(opp->socket, NOTIF_OPP_MOVE, msg.val1, msg.val2, 0, "L'adversaire a bougé");
                                } else {
                                    send_msg(p->socket, RES_MOVE_ERR, 0, 0, 0, "Mouvement invalide");
                                }
                                break;
                            }

                            case REQ_DESTROY: {
                                Game *g = find_game_of_player(p);
                                if (!g) break;

                                // 1. Vérifs Tour et Phase
                                int player_num = (p == g->p1) ? 1 : 2;
                                if (g->current_turn != player_num) break;

                                if (g->phase != PHASE_DESTROY) {
                                    // On ignore silencieusement ou on envoie une erreur
                                    break;
                                }

                                // 2. Logique Destruction
                                if (game_check_destroy(g, p, msg.val1, msg.val2)) {
                                    game_apply_destroy(g, msg.val1, msg.val2);

                                    // Avertir tout le monde (la case X,Y est morte)
                                    // On peut utiliser un nouveau type de message NOTIF_TILE_DESTROYED
                                    // Pour simplifier, j'utilise NOTIF_OPP_MOVE avec un code spécial ou un autre type
                                    // Créons un type générique ou réutilisons NOTIF pour l'instant.
                                    // Mieux : Ajoutons un NOTIF_UPDATE_BOARD dans le protocole, mais restons simples :

                                    // On envoie au joueur ET à l'adversaire que la case est détruite
                                    // On va tricher un peu et utiliser val3 = -1 pour dire "C'est une destruction"
                                    // Ou mieux : Définir un message NOTIF_DESTROY dans le protocole.

                                    // Disons que msg type 9 (NOTIF_OPP_MOVE) avec val3 = 1 veut dire "Destruction"
                                    // C'est sale. Ajoutons proprement le cas dans le protocole.h plus tard.
                                    // Pour l'instant, supposons un code 12 = NOTIF_DESTROY

                                    send_msg(p->socket, 12, msg.val1, msg.val2, 0, "Case détruite");
                                    Player *opp = (p == g->p1) ? g->p2 : g->p1;
                                    send_msg(opp->socket, 12, msg.val1, msg.val2, 0, "L'adversaire a détruit une case");

                                    // Vérifier si quelqu'un a perdu
                                    int winner = 0;
                                    if (game_check_loss(g, opp)) winner = player_num; // L'adversaire est bloqué -> Je gagne
                                    else if (game_check_loss(g, p)) winner = (player_num == 1 ? 2 : 1); // Je me suis bloqué -> Il gagne

                                    if (winner != 0) {
                                        send_msg(p->socket, NOTIF_GAME_OVER, winner, 0, 0, (winner == player_num ? "VICTOIRE" : "DÉFAITE"));
                                        send_msg(opp->socket, NOTIF_GAME_OVER, winner, 0, 0, (winner != player_num ? "VICTOIRE" : "DÉFAITE"));
                                        // Reset partie...
                                    }

                                }
                                break;
                            }

                            case REQ_LOGOUT:
                                handle_disconnect(i);
                                i--;
                                break;
                        }
                    }
                }
            }
        }
    }

    // Nettoyage final (si on sort du while, ce qui n'arrive pas ici)
    close(server_fd);
    return 0;
}