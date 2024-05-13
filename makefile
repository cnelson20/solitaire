all: SOLITAIRE.PRG

CC = cl65
FLAGS = -t cx16

SOLITAIRE.PRG: game.c helper.s
	$(CC) $(FLAGS) -o $@ game.c helper.s