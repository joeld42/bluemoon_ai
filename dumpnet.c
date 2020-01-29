#include "bluemoon.h"
#include "net.h"

people peep[2];
int who;

void message_add(char *msg)
{
}

static void input_name(char *ptr, int i)
{
	int index, num;
	char *type;

	if (i < 240)
	{
		strcpy(ptr, peep[i / 120].name);

		strcat(ptr, " ");

		strcat(ptr, peep[i / 120].deck[i % 30 + 1].name);

		if (i % 120 < 30) strcat(ptr, " active");
		else if (i % 120 < 60) strcat(ptr, " in hand");
		else if (i % 120 < 90) strcat(ptr, " discarded");
		else strcat(ptr, " loaded");
	}
	else if (i < 270)
	{
		strcpy(ptr, peep[who].name);

		strcat(ptr, " ");

		strcat(ptr, peep[who].deck[i - 240 + 1].name);

		strcat(ptr, " special");
	}
	else if (i == 270) strcpy(ptr, "Bad bluff");
	else if (i == 271) strcpy(ptr, "Game over");
	else if (i == 272) strcpy(ptr, "Fight started");
	else if (i == 273) strcpy(ptr, "Fight in Earth");
	else if (i == 274) strcpy(ptr, "Fight in Fire");
	else
	{
		index = (i - 275) % 84;

		if (index == 0)
		{
			sprintf(ptr, "%s turn", peep[(i - 275) / 84].name);
			return;
		}
		index--;

		if (index == 78)
		{
			sprintf(ptr, "%s no cards", peep[(i - 275) / 84].name);
			return;
		}

		if (index == 82)
		{
			sprintf(ptr, "%s instant win", peep[(i - 275) / 84].name);
			return;
		}

		if (index < 15) type = "power";
		else if (index < 23) type = "combat";
		else if (index < 27) type = "bluff";
		else if (index < 37) type = "handsize";
		else if (index < 67) type = "decksize";
		else if (index < 72) type = "chars";
		else if (index < 78) type = "disclosed";
		else type = "dragons";

		if (index < 15) num = index;
		else if (index < 23) num = index - 15;
		else if (index < 27) num = index - 23;
		else if (index < 37) num = index - 27;
		else if (index < 67) num = index - 37;
		else if (index < 72) num = index - 67;
		else if (index < 78) num = index - 72;
		else num = index - 79;

		sprintf(ptr, "%s %s %d", peep[(i - 275) / 84].name, type, num);
	}
}

int main(int argc, char *argv[])
{
	net learner;
	int i;
	double start;
	char buf[1024], *ptr;
	int n = 0;

	read_cards();

	make_learner(&learner, 443, 50, 2);

	load_net(&learner, argv[1]);

	for (i = 0; i < 9; i++) if (strstr(argv[1], peoples[i].name)) peep[n++] = peoples[i];

	for (i = 0; i < 443; i++) learner.input_value[i] = 0;

	compute_net(&learner);

	who = atoi(argv[2]);

	start = learner.win_prob[who];

	for (i = 0; i < 443; i++)
	{
		learner.input_value[i] = 1;

		compute_net(&learner);

		input_name(buf, i);

		for (ptr = buf; *ptr; ptr++) if (*ptr == ' ') *ptr = '_';

		printf("%s: %f\n", buf, learner.win_prob[who] - start);

		learner.input_value[i] = 0;
	}
}
