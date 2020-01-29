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

#include "net.h"

/*
 * This extremely ugly code does some initial quick training of the
 * given neural net file.
 *
 * It sets only the "dragon" and "no cards" inputs and trains the
 * network about those.  Otherwise, the first several tens or hundreds
 * of thousands of training games are mostly wasted while the networks
 * learn about the scoring and victory conditions.
 */
int main(int argc, char *argv[])
{
	net learner;
	int i, n;
	double target[2];

	srand(time(NULL));

	make_learner(&learner, 443, 50, 2);

	learner.alpha = 0.1;

	load_net(&learner, argv[1]);

	for (i = 0; i < 443; i++) learner.input_value[i] = 0;

	/* Set "game over" input */
	learner.input_value[271] = 1;

	/* Do several training iterations */
	for (n = 0; n < 500; n++)
	{
		/* Set player 0 "ran out of cards first" input */
		learner.input_value[354] = 1;

		/* Set target scores */
		target[0] = 0.4;
		target[1] = 0.6;

		/* Train network */
		compute_net(&learner);
		train_net(&learner, 1.0, target);

		/* Clear no cards input */
		learner.input_value[354] = 0;

		/* Train dragon inputs */
		for (i = 0; i < 3; i++)
		{
			/* Set input */
			learner.input_value[355 + i] = 1;

			/* Compute desired outputs */
			target[0] = 0.5 + (i + 2) * 0.1;
			target[1] = 1.0 - target[0];

			/* Train network */
			compute_net(&learner);
			train_net(&learner, 1.0, target);
		}

		/* Set player 0 instant win input */
		learner.input_value[358] = 1;

		/* Compute desired outputs */
		target[0] = 1.0;
		target[1] = 0.0;

		/* Train network */
		compute_net(&learner);
		train_net(&learner, 1.0, target);

		/* Clear player 0 dragon inputs */
		for (i = 0; i < 3; i++) learner.input_value[355 + i] = 0;

		/* Clear player 0 instant win input */
		learner.input_value[358] = 0;

		/* Set player 1 "ran out of cards first" input */
		learner.input_value[438] = 1;

		/* Set target scores */
		target[0] = 0.6;
		target[1] = 0.4;

		/* Train network */
		compute_net(&learner);
		train_net(&learner, 1.0, target);

		/* Clear no cards input */
		learner.input_value[438] = 0;

		/* Train dragon inputs */
		for (i = 0; i < 3; i++)
		{
			/* Set input */
			learner.input_value[439 + i] = 1;

			/* Compute desired outputs */
			target[0] = 0.5 - (i + 2) * 0.1;
			target[1] = 1.0 - target[0];

			/* Train network */
			compute_net(&learner);
			train_net(&learner, 1.0, target);
		}

		/* Set player 1 instant win input */
		learner.input_value[442] = 1;

		/* Compute desired outputs */
		target[0] = 0.0;
		target[1] = 1.0;

		/* Train network */
		compute_net(&learner);
		train_net(&learner, 1.0, target);

		/* Clear player 1 dragon inputs */
		for (i = 0; i < 3; i++) learner.input_value[439 + i] = 0;

		/* Clear player 1 instant win input */
		learner.input_value[442] = 0;
	}

	save_net(&learner, argv[1]);
}
