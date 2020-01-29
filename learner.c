/*
 * Bluemoon AI
 * 
 * Copyright (C) 2007-2008 Keldon Jones
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "bluemoon.h"

#include <signal.h>

/*
 * Be noisy?
 */
int verbose;

/*
 * No need for messages.
 */
void message_add(char *msg)
{
	/* Print messages if asked */
	if (verbose) printf("%s", msg);
}

/*
 * Initialize game and have AIs take all actions.
 */
int main(int argc, char *argv[])
{
	game my_game;
	player *p;
	int i, j, n = 100;

	/* Initialize random seed */
	my_game.random_seed = time(NULL);

	/* Read card designs */
	read_cards();

	/* Set default people */
	my_game.p[0].p_ptr = &peoples[0];
	my_game.p[1].p_ptr = &peoples[1];

	/* Parse arguments */
	for (i = 1; i < argc; i++)
	{
		/* Check for verbosity */
		if (!strcmp(argv[i], "-v"))
		{
			/* Set verbose flag */
			verbose += 1;
		}

		/* Check for number of games */
		else if (!strcmp(argv[i], "-n"))
		{
			/* Set number of games */
			n = atoi(argv[++i]);
		}

		/* Check for people setting */
		else if (!strcmp(argv[i], "-1") || !strcmp(argv[i], "-2"))
		{
			/* Loop over people */
			for (j = 0; j < MAX_PEOPLE; j++)
			{
				/* Check for match */
				if (!strcasecmp(argv[i + 1], peoples[j].name))
				{
					/* Set people */
					my_game.p[argv[i][1] - '1'].p_ptr =
					                           &peoples[j];
				}
			}

			/* Advance argument count */
			i++;
		}

		/* Check for random seed setting */
		else if (!strcmp(argv[i], "-r"))
		{
			/* Set random seed */
			my_game.random_seed = atoi(argv[i + 1]);

			/* Advance argument count */
			i++;
		}
	}

	/* Initialize game */
	init_game(&my_game, 1);

	/* Set player interface functions */
	my_game.p[0].control = &ai_func;
	my_game.p[1].control = &ai_func;

	/* Call interface initialization */
	for (i = 0; i < 2; i++)
	{
		/* Call init function */
		my_game.p[i].control->init(&my_game, i);
	}

	/* Play a number of games */
	for (i = 0; i < n; i++)
	{
		/* Take actions until game is over */
		while (1)
		{
			/* Get current player */
			p = &my_game.p[my_game.turn];

			/* Have AI take action */
			p->control->take_action(&my_game);

			/* Check for end of game */
			if (my_game.game_over)
			{
				/* Quit playing */
				break;
			}
		}

		/* Call game over functions */
		my_game.p[0].control->game_over(&my_game, 0);
		my_game.p[1].control->game_over(&my_game, 1);

		/* Message */
		printf("Crystals: %d %d\n", my_game.p[0].crystals,
		                            my_game.p[1].crystals);

		/* Restart game */
		init_game(&my_game, 1);
	}

	/* Call interface shutdown */
	for (i = 0; i < 2; i++)
	{
		/* Call shutdown function */
		my_game.p[i].control->shutdown(&my_game, i);
	}

	/* Done */
	return 0;
}
