#include "server.h"

/** Shared array that contains all the games. */
tGame games[MAX_GAMES];

void initGame(tGame *game)
{
	pthread_mutex_init(&game->statusMutex, NULL);
	pthread_mutex_init(&game->player1NameMutex, NULL);
	pthread_mutex_init(&game->player2NameMutex, NULL);
	pthread_mutex_init(&game->turnoMutex, NULL);
	pthread_mutex_init(&game->resetGameMutex, NULL);

	pthread_cond_init(&game->resetGameCond, NULL);
	pthread_cond_init(&game->getStatusCond, NULL);
	// Init players' name
	memset(game->player1Name, 0, STRING_LENGTH);
	memset(game->player2Name, 0, STRING_LENGTH);

	// Alloc memory for the decks
	clearDeck(&(game->player1Deck));
	clearDeck(&(game->player2Deck));
	initDeck(&(game->gameDeck));

	// Bet and stack
	game->player1Bet = 0;
	game->player2Bet = 0;
	game->player1Stack = INITIAL_STACK;
	game->player2Stack = INITIAL_STACK;

	// Game status variables
	game->turnoFinalizado = 0;
	game->endOfGame = FALSE;
	game->status = gameEmpty;
	game->currentPlayer = calculateRandomPlayer();
	game->player_reset = 0;
}

void initServerStructures(struct soap *soap)
{

	if (DEBUG_SERVER)
		printf("Initializing structures...\n");

	// Init seed
	srand(time(NULL));

	// Init each game (alloc memory and init)
	for (int i = 0; i < MAX_GAMES; i++)
	{
		games[i].player1Name = (xsd__string)soap_malloc(soap, STRING_LENGTH);
		games[i].player2Name = (xsd__string)soap_malloc(soap, STRING_LENGTH);
		allocDeck(soap, &(games[i].player1Deck));
		allocDeck(soap, &(games[i].player2Deck));
		allocDeck(soap, &(games[i].gameDeck));
		initGame(&(games[i]));
	}
}

void initDeck(blackJackns__tDeck *deck)
{
	deck->__size = DECK_SIZE;

	for (int i = 0; i < DECK_SIZE; i++)
		deck->cards[i] = i;
}

void clearDeck(blackJackns__tDeck *deck)
{
	deck->__size = 0;

	for (int i = 0; i < DECK_SIZE; i++)
		deck->cards[i] = UNSET_CARD;
}

tPlayer calculateNextPlayer(tPlayer currentPlayer)
{
	return ((currentPlayer == player1) ? player2 : player1);
}

tPlayer calculateRandomPlayer()
{
	srand(time(0));
	return (rand() % 2 == 0 ? player1 : player2);
}

unsigned int getRandomCard(blackJackns__tDeck *deck)
{
	if (deck->__size == 0)
		return UNSET_CARD;

	unsigned int cardIndex = rand() % deck->__size;

	unsigned int card = deck->cards[cardIndex];

	for (unsigned int i = cardIndex; i < deck->__size - 1; i++)
		deck->cards[i] = deck->cards[i + 1];

	deck->__size--;
	deck->cards[deck->__size] = UNSET_CARD;

	return card;
}

unsigned int calculatePoints(blackJackns__tDeck *deck)
{

	unsigned int points = 0;

	for (int i = 0; i < deck->__size; i++)
	{

		if (deck->cards[i] % SUIT_SIZE < 9)
			points += (deck->cards[i] % SUIT_SIZE) + 1;
		else
			points += FIGURE_VALUE;
	}

	return points;
}

void copyGameStatusStructure(blackJackns__tBlock *status, char *message, blackJackns__tDeck *newDeck, int newCode)
{
	// Copy the message
	memset((status->msgStruct).msg, 0, STRING_LENGTH);
	strcpy((status->msgStruct).msg, message);
	(status->msgStruct).__size = strlen((status->msgStruct).msg);

	// Copy the deck, only if it is not NULL
	if (newDeck->__size > 0)
		memcpy(status->deck.cards, newDeck->cards, DECK_SIZE * sizeof(unsigned int));
	else
		(status->deck).cards = NULL;

	(status->deck).__size = newDeck->__size;

	// Set the new code
	status->code = newCode;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void jugadorHaPerdidoMano(int id_partida, blackJackns__tMessage playerName)
{
	int puntos = 0, jugador_actual = checkPlayer(id_partida, playerName);

	if (jugador_actual == 1)
		puntos = calculatePoints(&games[id_partida].player1Deck);
	else if (jugador_actual == 2)
		puntos = calculatePoints(&games[id_partida].player2Deck);

	if (puntos > 21)
		games[id_partida].currentPlayer = calculateNextPlayer(games[id_partida].currentPlayer);

	signalStatusCond(id_partida);
}

int checkPlayer(int id_partida, blackJackns__tMessage playerName)
{
	if (games[id_partida].currentPlayer == player1 && strcmp(games[id_partida].player1Name, playerName.msg) == 0)
	{
		return 1;
	}
	else if (games[id_partida].currentPlayer == player2 && strcmp(games[id_partida].player2Name, playerName.msg) == 0)
	{
		return 2;
	}
	return 0;
}

int cualJugador(int id_partida, blackJackns__tMessage playerName)
{
	if (strcmp(games[id_partida].player1Name, playerName.msg) == 0)
	{
		return 1;
	}
	else if (strcmp(games[id_partida].player2Name, playerName.msg) == 0)
	{
		return 2;
	}
	return 0;
}

void genera_deck(blackJackns__tDeck *deck_juego, blackJackns__tDeck *deck_jugador)
{
	for (int i = 0; i < 2; i++)
	{
		deck_jugador->cards[i] = getRandomCard(deck_juego);
		deck_jugador->__size++;
	}
}

void signalStatusCond(int id_partida)
{
	pthread_mutex_lock(&games[id_partida].turnoMutex);
	pthread_cond_signal(&games[id_partida].getStatusCond);
	pthread_mutex_unlock(&games[id_partida].turnoMutex);
}

void controlaTurno(struct soap *soap, blackJackns__tMessage playerName, int id_partida)
{
	pthread_mutex_lock(&games[id_partida].turnoMutex);
	if (!esTurnoJugador(playerName.msg, id_partida) && !games[id_partida].endOfGame)
	{
		pthread_cond_wait(&games[id_partida].getStatusCond, &games[id_partida].turnoMutex);
	}
	pthread_mutex_unlock(&games[id_partida].turnoMutex);
}

int esTurnoJugador(const char *playerName, int id_partida)
{
	const char *currentPlayerName = games[id_partida].currentPlayer == player1 ? games[id_partida].player1Name : games[id_partida].player2Name;

	return (strcmp(playerName, currentPlayerName) == 0) ? 1 : 0;
}

int buscaJugador(const char *playerName, int id_partida)
{

	for (int i = 0; i < MAX_GAMES; i++)
	{
		pthread_mutex_lock(&games[i].player1NameMutex);
		pthread_mutex_lock(&games[i].player2NameMutex);

		if ((games[i].player1Name != NULL && strcmp(games[i].player1Name, playerName) == 0) ||
			(games[i].player2Name != NULL && strcmp(games[i].player2Name, playerName) == 0))
		{
			pthread_mutex_unlock(&games[i].player2NameMutex);
			pthread_mutex_unlock(&games[i].player1NameMutex);

			if (i == id_partida)
				return i;
		}
		else
		{
			pthread_mutex_unlock(&games[i].player2NameMutex);
			pthread_mutex_unlock(&games[i].player1NameMutex);
		}
	}

	return ERROR_PLAYER_NOT_FOUND; // Jugador no encontrado en ningún juego o no coincide con id_partida
}

void gestionaReset(int id_partida, blackJackns__tMessage playerName)
{

	if (games[id_partida].endOfGame)
	{
		pthread_mutex_lock(&games[id_partida].resetGameMutex);

		if (cualJugador(id_partida, playerName) == 1)
		{
			pthread_mutex_lock(&games[id_partida].player1NameMutex);
			strcpy(games[id_partida].player1Name, "");
			pthread_mutex_unlock(&games[id_partida].player1NameMutex);
			games[id_partida].player_reset = 1;
			pthread_cond_signal(&games[id_partida].resetGameCond);
		}
		else if (cualJugador(id_partida, playerName) == 2)
		{
			while (!games[id_partida].player_reset)
			{
				pthread_cond_wait(&games[id_partida].resetGameCond, &games[id_partida].resetGameMutex);
			}
			pthread_mutex_lock(&games[id_partida].player2NameMutex);
			strcpy(games[id_partida].player2Name, "");
			pthread_mutex_unlock(&games[id_partida].player2NameMutex);

			initGame(&(games[id_partida]));
		}

		pthread_mutex_unlock(&games[id_partida].resetGameMutex);
	}
}

void finalizaMano(int id_partida)
{
	if (games[id_partida].turnoFinalizado != 2) // Si no han acabado ambos jugadores esa ronda
		return;

	int points1 = calculatePoints(&games[id_partida].player1Deck);
	int points2 = calculatePoints(&games[id_partida].player2Deck);

	if (points1 <= 21 && (points2 > 21 || points1 > points2))
	{
		games[id_partida].player1Stack++;
		games[id_partida].player2Stack--;
	}
	else if (points2 <= 21 && (points1 > 21 || points2 > points1))
	{
		games[id_partida].player2Stack++;
		games[id_partida].player1Stack--;
	}
	// Se copia la baraja antes de resetearla por si acaso es la ultima jugada y asi poder mostrarla
	games[id_partida].player1LastDeck = games[id_partida].player1Deck;
	games[id_partida].player2LastDeck = games[id_partida].player2Deck;

	clearDeck(&(games[id_partida].player1Deck));
	clearDeck(&(games[id_partida].player2Deck));
	initDeck(&(games[id_partida].gameDeck));
	genera_deck(&(games[id_partida].gameDeck), &(games[id_partida].player1Deck));
	genera_deck(&(games[id_partida].gameDeck), &(games[id_partida].player2Deck));
	games[id_partida].currentPlayer = calculateNextPlayer(games[id_partida].currentPlayer);
	games[id_partida].turnoFinalizado = 0;
}

int blackJackns__playerMove(struct soap *soap, blackJackns__tMessage playerName, int id_partida, unsigned int playerMove, blackJackns__tBlock *status)
{
	playerName.msg[playerName.__size] = 0;

	if (buscaJugador(playerName.msg, id_partida) != ERROR_PLAYER_NOT_FOUND)
	{
		int jugador_actual = checkPlayer(id_partida, playerName); // Se mira que jugador de los dos le toca

		if (playerMove) // Se le reparte carta
		{
			blackJackns__tDeck *playerDeck;
			if (jugador_actual == 1)
				playerDeck = &games[id_partida].player1Deck;
			else if (jugador_actual == 2)
				playerDeck = &games[id_partida].player2Deck;

			playerDeck->cards[playerDeck->__size++] = getRandomCard(&games[id_partida].gameDeck);

			if (calculatePoints(playerDeck) > 21)
			{
				games[id_partida].turnoFinalizado++;
				finalizaMano(id_partida);
			}

			signalStatusCond(id_partida); // Se avisa al otro player que vea la jugada
		}
		else // Jugador se planta
		{
			games[id_partida].turnoFinalizado++;
			finalizaMano(id_partida);
			games[id_partida].currentPlayer = calculateNextPlayer(games[id_partida].currentPlayer);
			signalStatusCond(id_partida);
		}
	}

	return SOAP_OK;
}

void getCheckEnd(int id_partida, blackJackns__tMessage playerName, blackJackns__tBlock *status)
{
	char informacion[STRING_LENGTH];
	memset(informacion, 0, STRING_LENGTH);

	int player = cualJugador(id_partida, playerName);
	int playerStack = (player == 1) ? games[id_partida].player1Stack : games[id_partida].player2Stack;
	int opponentStack = (player == 1) ? games[id_partida].player2Stack : games[id_partida].player1Stack;
	blackJackns__tDeck *playerDeck = (player == 1) ? &(games[id_partida].player1LastDeck) : &(games[id_partida].player2LastDeck);
	blackJackns__tDeck *otherDeck = (player == 1) ? &(games[id_partida].player2LastDeck) : &(games[id_partida].player1LastDeck);

	if (playerStack <= 0 && opponentStack > 0)
	{
		sprintf(informacion, "Has perdido la partida\nLa baraja del rival era: ");
		copyGameStatusStructure(status, informacion, otherDeck, GAME_LOSE);
		games[id_partida].endOfGame = 1;
	}
	else if (playerStack > 0 && opponentStack <= 0)
	{
		sprintf(informacion, "Has ganado la partida\nLa baraja del rival era: ");
		copyGameStatusStructure(status, informacion, otherDeck, GAME_WIN);
		games[id_partida].endOfGame = 1;
	}
}

void getPlayerStatus(int id_partida, int jugador_actual, blackJackns__tBlock *status)
{
	char informacion[STRING_LENGTH];
	memset(informacion, 0, STRING_LENGTH);
	int playerStack, playerPoints, otherPoints;
	blackJackns__tDeck *playerDeck, *otherDeck;

	if (games[id_partida].endOfGame)
		return;

	if (jugador_actual == 1 || jugador_actual == 2)
	{
		playerStack = (jugador_actual == 1) ? games[id_partida].player1Stack : games[id_partida].player2Stack;
		playerDeck = (jugador_actual == 1) ? &(games[id_partida].player1Deck) : &(games[id_partida].player2Deck);
		playerPoints = calculatePoints(playerDeck);
		otherDeck = (jugador_actual == 1) ? &(games[id_partida].player2Deck) : &(games[id_partida].player1Deck);
		otherPoints = calculatePoints(otherDeck);
	}
	else
	{
		sprintf(informacion, "Es el turno del rival.\nLa baraja del otro jugador es: ");
		copyGameStatusStructure(status, informacion, games[id_partida].currentPlayer == player1 ? &(games[id_partida].player1Deck) : &(games[id_partida].player2Deck), TURN_WAIT);
		status->msgStruct.msg[status->msgStruct.__size] = '\0';
		return;
	}

	if (playerPoints > 21)
	{
		sprintf(informacion, "Has perdido el turno con %u puntos\n", playerPoints);
		copyGameStatusStructure(status, informacion, playerDeck, TURN_WAIT);
	}
	else
	{
		sprintf(informacion, "Es tu turno, tienes %u fichas\nTienes %u puntos\nTu rival tiene %u puntos\nTu baraja es: ", playerStack, playerPoints, otherPoints);
		copyGameStatusStructure(status, informacion, playerDeck, TURN_PLAY);
	}
	status->msgStruct.msg[status->msgStruct.__size] = '\0';
}

int blackJackns__getStatus(struct soap *soap, blackJackns__tMessage playerName, int id_partida, blackJackns__tBlock *status)
{
	playerName.msg[playerName.__size] = 0;
	allocClearBlock(soap, status);

	if (buscaJugador(playerName.msg, id_partida) != ERROR_PLAYER_NOT_FOUND)
	{

		controlaTurno(soap, playerName, id_partida); // Se bloquea hasta que se le avise si no es su turno y no es fin de partida

		getPlayerStatus(id_partida, checkPlayer(id_partida, playerName), status); // Le devuelve a al jugador que toca la informacion

		getCheckEnd(id_partida, playerName, status); // Se revisa si alguno ha perdido la partida y se activa flag endOfGame

		jugadorHaPerdidoMano(id_partida, playerName); // Se mira si ha perdido esa mano y se pasa el turno

		gestionaReset(id_partida, playerName); // Se gestiona el reset de la partida si flag endOfGame = 1
	}

	return SOAP_OK;
}

int blackJackns__register(struct soap *soap, blackJackns__tMessage playerName, int *id_partida)
{
	playerName.msg[playerName.__size] = 0;
	int nombreRepetido = 0;

	if (DEBUG_SERVER)
		printf("[Register] Registrando nuevo jugador -> [%s]\n", playerName.msg);

	for (int i = 0; i < MAX_GAMES && !nombreRepetido; i++) // Se buscan en todas las partidas que existen
	{
		pthread_mutex_lock(&games[i].player1NameMutex);
		pthread_mutex_lock(&games[i].player2NameMutex);
		// Si se encuentra un jugador con el nombre que se intenta registrar ya registrado se devuelve ERROR_NAME_REPEATED
		if ((games[i].player1Name != NULL && strcmp(games[i].player1Name, playerName.msg) == 0) || (games[i].player2Name != NULL && strcmp(games[i].player2Name, playerName.msg) == 0))
		{
			nombreRepetido = 1;
		}
		pthread_mutex_unlock(&games[i].player1NameMutex);
		pthread_mutex_unlock(&games[i].player2NameMutex);
	}

	if (nombreRepetido) // Si se encontró un nombre repetido
	{
		*id_partida = ERROR_NAME_REPEATED;
		return SOAP_OK;
	}

	for (int i = 0; i < MAX_GAMES; i++) // Se buscan en todas las partidas que existen
	{

		char *playerNameCopy = strdup(playerName.msg);
		pthread_mutex_lock(&games[i].statusMutex);
		tGameState nuevoEstado = games[i].status == gameEmpty ? gameWaitingPlayer : gameReady;

		if (games[i].status == gameEmpty) // Si ese game esta vacio
		{
			games[i].status = nuevoEstado;
			pthread_mutex_unlock(&games[i].statusMutex);

			pthread_mutex_lock(&games[i].player1NameMutex);
			games[i].player1Name = playerNameCopy;
			pthread_mutex_unlock(&games[i].player1NameMutex);

			genera_deck(&games[i].gameDeck, &games[i].player1Deck);

			pthread_mutex_lock(&games[i].statusMutex);
			pthread_cond_wait(&games[i].getStatusCond, &games[i].statusMutex);
			pthread_mutex_unlock(&games[i].statusMutex);

			*id_partida = i;
			return SOAP_OK;
		}
		else if (games[i].status == gameWaitingPlayer)
		{
			games[i].status = nuevoEstado;
			pthread_mutex_unlock(&games[i].statusMutex);

			pthread_mutex_lock(&games[i].player2NameMutex);
			games[i].player2Name = playerNameCopy;
			pthread_mutex_unlock(&games[i].player2NameMutex);

			genera_deck(&games[i].gameDeck, &games[i].player2Deck);

			pthread_mutex_lock(&games[i].statusMutex);
			pthread_cond_signal(&games[i].getStatusCond);
			pthread_mutex_unlock(&games[i].statusMutex);

			*id_partida = i;
			return SOAP_OK;
		}
		pthread_mutex_unlock(&games[i].statusMutex);
	}

	*id_partida = ERROR_SERVER_FULL;
	return SOAP_OK;
}

void *processRequest(void *soap)
{

	pthread_detach(pthread_self());

	printf("Procesando una nueva peticion\n");

	soap_serve((struct soap *)soap);
	soap_destroy((struct soap *)soap);
	// soap_end((struct soap *)soap); COMENTADO POR QUE SI NO AL HACER INITGAME EN UN RESET SALTA EXCEPCION
	// soap_done((struct soap *)soap); COMENTADO POR QUE SI NO AL HACER INITGAME EN UN RESET SALTA EXCEPCION
	free(soap);

	return NULL;
}

int main(int argc, char **argv)
{

	struct soap soap;
	struct soap *tsoap;
	pthread_t tid;
	int port;
	SOAP_SOCKET m, s;

	// Init soap environment
	soap_init(&soap);

	// Configure timeouts
	soap.send_timeout = 60;		// 60 seconds
	soap.recv_timeout = 60;		// 60 seconds
	soap.accept_timeout = 3600; // server stops after 1 hour of inactivity
	soap.max_keep_alive = 100;	// max keep-alive sequence

	// Get listening port
	port = atoi(argv[1]);

	// Bind
	m = soap_bind(&soap, NULL, port, 100);

	if (!soap_valid_socket(m))
	{
		exit(1);
	}

	initServerStructures(&soap);
	printf("Server is ON!\n");

	while (TRUE)
	{
		s = soap_accept(&soap);
		if (!soap_valid_socket(s))
		{
			if (soap.errnum)
			{
				soap_print_fault(&soap, stderr);
				exit(1);
			}
			fprintf(stderr, "Time out!\n");
			break;
		}

		tsoap = soap_copy(&soap);
		if (!tsoap)
		{
			printf("SOAP copy error!\n");
			break;
		}

		pthread_create(&tid, NULL, (void *(*)(void *))processRequest, (void *)tsoap);
	}
	soap_done(&soap);
	return 0;
}