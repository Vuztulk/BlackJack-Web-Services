#include "client.h"

void clearConsole()
{
	printf("\033[H\033[J");
}

void printGameStatus(blackJackns__tBlock *gameStatus) {
    gameStatus->msgStruct.msg[gameStatus->msgStruct.__size] = '\0';
    printf("%s", gameStatus->msgStruct.msg);
    printDeck(&(gameStatus->deck));
    printf("\n");
}

unsigned int readBet()
{

	int isValid, bet = 0;
	xsd__string enteredMove;

	// While player does not enter a correct bet...
	do
	{

		// Init...
		enteredMove = (xsd__string)malloc(STRING_LENGTH);
		bzero(enteredMove, STRING_LENGTH);
		isValid = TRUE;

		printf("Introduce un valor: ");
		fgets(enteredMove, STRING_LENGTH - 1, stdin);
		enteredMove[strlen(enteredMove) - 1] = 0;

		// Check if each character is a digit
		for (int i = 0; i < strlen(enteredMove) && isValid; i++)
			if (!isdigit(enteredMove[i]))
				isValid = FALSE;

		// Entered move is not a number
		if (!isValid)
			printf("El valor no es correcto ha de ser 1 o 0\n");
		else
			bet = atoi(enteredMove);

	} while (!isValid);

	printf("\n");
	free(enteredMove);

	return ((unsigned int)bet);
}

unsigned int readOption()
{

	unsigned int bet;

	do
	{
		printf("Introduce tu movimiento: %d -> pedir carta o %d -> plantarse\n", PLAYER_HIT_CARD, PLAYER_STAND);
		bet = readBet();
		if ((bet != PLAYER_HIT_CARD) && (bet != PLAYER_STAND))
			printf("Wrong option!\n");
	} while ((bet != PLAYER_HIT_CARD) && (bet != PLAYER_STAND));

	return bet;
}

int main(int argc, char **argv)
{

	struct soap soap;				  /** Soap struct */
	char *serverURL;				  /** Server URL */
	blackJackns__tMessage playerName; /** Player name */
	blackJackns__tBlock gameStatus;	  /** Game status */
	unsigned int playerMove;		  /** Player's move */
	int id_partida;					  /** Result and gameId */

	soap_init(&soap);

	// Check arguments
	if (argc != 2)
	{
		printf("Usage: %s http://server:port\n", argv[0]);
		exit(0);
	}

	// Obtain server address
	serverURL = argv[1];

	// Allocate memory
	allocClearMessage(&soap, &(playerName));
	allocClearBlock(&soap, &gameStatus);

	do
	{
		printf("Introduce tu nombre: ");
		fgets(playerName.msg, STRING_LENGTH - 1, stdin);
		playerName.msg[strlen(playerName.msg) - 1] = 0;
		playerName.__size = strlen(playerName.msg);
		soap_call_blackJackns__register(&soap, serverURL, "", playerName, &id_partida);
		switch (id_partida)
		{
		case ERROR_NAME_REPEATED:
			printf("Ya existe un jugador con ese nombre jugando\n");
			break;
		case ERROR_SERVER_FULL:
			printf("No hay partidas disponibles, el servidor esta lleno\n");
			break;
		}
	} while (id_partida < 0);

	while (gameStatus.code != GAME_WIN && gameStatus.code != GAME_LOSE)
	{
		clearConsole();
		soap_call_blackJackns__getStatus(&soap, serverURL, "", playerName, id_partida, &gameStatus);
		printGameStatus(&gameStatus);

		while (gameStatus.code == TURN_PLAY)
		{
			soap_call_blackJackns__getStatus(&soap, serverURL, "", playerName, id_partida, &gameStatus);
			playerMove = readOption();
			clearConsole();
			soap_call_blackJackns__playerMove(&soap, serverURL, "", playerName, id_partida, playerMove, &gameStatus);
			soap_call_blackJackns__getStatus(&soap, serverURL, "", playerName, id_partida, &gameStatus);
			printGameStatus(&gameStatus);
		}
	}

	soap_destroy(&soap);
	soap_end(&soap);
	soap_done(&soap);

	return 0;
}
