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

#include <gtk/gtk.h>
#include "bluemoon.h"

/*
 * AI verbosity.
 */
int verbose = 0;

/*
 * Base filenames for people's card images.
 */
static char *image_base[] =
{
	"hoax",
	"vulca",
	"mimix",
	"flit",
	"khind",
	"terrah",
	"pillar",
	"aqua",
	"buka",
	"mutant"
};

/*
 * Current (real) game state.
 */
static game real_game;

/*
 * Back-up game to restore from when undo-ing.
 */
static game backup;
static int backup_set;

/*
 * Player we're playing as.
 */
static int player_us;

/*
 * People indices for both players.
 */
static int human_people;
static int ai_people;

/*
 * Cached card images.
 */
static GdkPixbuf *image_cache[10][128];

/*
 * Card back image.
 */
static GdkPixbuf *card_back;

/*
 * Widgets used in multiple functions.
 */
static GtkWidget *window;
static GtkWidget *full_image;
static GtkWidget *hand_area, *opp_hand;
static GtkWidget *opp_area, *our_area;
static GtkWidget *retreat_button, *fire_button, *earth_button, *undo_button;
static GtkWidget *opp_status, *our_status;
static GtkWidget *choose_dialog, *choose_ok;
static GtkWidget *choice_area;
static GtkWidget *our_frame, *opp_frame;
static GtkWidget *popup_menu;

/*
 * Card design we have a popup menu for.
 */
static design *popup_menu_d_ptr;

/*
 * Information about card choices.
 */
static design *card_choices[DECK_SIZE];
static int card_num_choices, choice_who, current_choices;
static int choice_min, choice_max;
static int choice_width, choice_height;
static void *choice_data;
static choose_result choice_callback;
static game *choice_game;

/*
 * Text buffer for message area.
 */
static GtkWidget *message_view;

/*
 * Mark at end of message area text buffer.
 */
static GtkTextMark *message_end;

/*
 * Forward function definitions.
 */
static void redraw_hand(void);
static void redraw_table(void);
static void redraw_status(void);
static void redraw_choice(void);

/*
 * Add text to the message buffer.
 */
void message_add(char *msg)
{
	GtkTextIter end_iter;
	GtkTextBuffer *message_buffer;
	GtkWidget *dialog;

	/* Check for uninitialized GUI */
	if (!message_view)
	{
		/* Create message dialog instead */
		dialog = gtk_message_dialog_new(NULL, 0,
						GTK_MESSAGE_INFO,
		                                GTK_BUTTONS_CLOSE, msg);

		/* Run dialog */
		gtk_dialog_run(GTK_DIALOG(dialog));

		/* Destroy dialog */
		gtk_widget_destroy(dialog);

		/* Done */
		return;
	}

	/* Get message buffer */
	message_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(message_view));

	/* Get end mark */
	gtk_text_buffer_get_iter_at_mark(message_buffer, &end_iter,
	                                 message_end);

	/* Add message */
	gtk_text_buffer_insert(message_buffer, &end_iter, msg, -1);

	/* Scroll to end mark */
	gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(message_view),
	                                   message_end);
}

/*
 * Clear message log.
 */
static void clear_log(void)
{
	GtkTextBuffer *message_buffer;

	/* Get message buffer */
	message_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(message_view));

	/* Clear text buffer */
	gtk_text_buffer_set_text(message_buffer, "", 0);
}

/*
 * Load pixbufs with card images.
 */
static void load_images(void)
{
	int i, j;
	int x, y;
	char fn[1024];
	design *d_ptr;

	/* Check for card back image already loaded */
	if (!card_back)
	{
		/* Load card back image */
		card_back = gdk_pixbuf_new_from_file(
		                      DATADIR "/image/cardback.jpg", NULL);
	}

	/* Loop over peoples */
	for (i = 0; i < 2; i++)
	{
		/* Loop over cards in deck */
		for (j = 0; j < DECK_SIZE; j++)
		{
			/* Get design pointer */
			d_ptr = &real_game.p[i].p_ptr->deck[j];

			/* Get design's base people and index */
			x = d_ptr->people;
			y = d_ptr->index;

			/* Check for already loaded image */
			if (image_cache[x][y]) continue;

			/* Construct image filename */
			sprintf(fn, DATADIR "/image/%s%02d.jpg",
			            image_base[x], y);

			/* Load image */
			image_cache[x][y] = gdk_pixbuf_new_from_file(fn, NULL);

			/* Check for error */
			if (!image_cache[x][y])
			{
				/* Print error */
				printf(_("Cannot open image %s!\n"), fn);
			}
		}
	}
}

/*
 * Refresh the full-size card image.
 *
 * Called when the pointer moves over a small card image.
 */
static gboolean redraw_full(GtkWidget *widget, GdkEventCrossing *event,
                            gpointer data)
{
	design *d_ptr = (design *)data;

	/* Check for no design */
	if (!d_ptr)
	{
		/* Set image to card back */
		gtk_image_set_from_pixbuf(GTK_IMAGE(full_image), card_back);
	}
	else
	{
		/* Reset image */
		gtk_image_set_from_pixbuf(GTK_IMAGE(full_image),
	                              image_cache[d_ptr->people][d_ptr->index]);
	}

	/* Event handled */
	return TRUE;
}

/*
 * Store the current game state as an "undo" state.
 */
static void save_state(void)
{
	/* Check for state already stored */
	if (backup_set) return;

	/* Store game state */
	backup = real_game;

	/* Backup is available */
	backup_set = 1;

	/* Activate undo button */
	gtk_widget_set_sensitive(undo_button, TRUE);
}

/*
 * Retrieve the given card.
 */
static gboolean card_retrieved(GtkWidget *widget, GdkEventButton *event,
                               gpointer data)
{
	design *d_ptr = (design *)data;

	/* Save state */
	save_state();

	/* Retrieve card */
	retrieve_card(&real_game, d_ptr);

	/* Redraw stuff */
	redraw_table();
	redraw_hand();
	redraw_status();

	/* Event handled */
	return TRUE;
}

/*
 * Reveal the given bluff card.
 */
static gboolean card_revealed(GtkWidget *widget, GdkEventButton *event,
                              gpointer data)
{
	design *d_ptr = (design *)data;

	/* Save state */
	save_state();

	/* Reveal bluff card */
	reveal_bluff(&real_game, player_us, d_ptr);

	/* Redraw stuff */
	redraw_table();
	redraw_hand();
	redraw_status();

	/* Event handled */
	return TRUE;
}

/*
 * Set the sensitivity of the retreat and announce buttons.
 */
static void set_buttons(void)
{
	game sim;
	player *p = &real_game.p[player_us];
	card *c;
	int retreat, fire, earth;
	int i;

	/* Check for retreat legal */
	retreat = p->phase <= PHASE_RETREAT;

	/* Check for incomplete turn */
	if (!p->char_played)
	{
		/* Cannot announce yet */
		fire = earth = 0;
	}

	/* Check for no fight started */
	else if (!real_game.fight_started)
	{
		/* Both are legal */
		fire = earth = 1;
	}

	/* Check for enough power */
	else if (compute_power(&real_game, player_us) >=
		 compute_power(&real_game, !player_us))
	{
		/* Check for fire element */
		fire = !real_game.fight_element;
		earth = real_game.fight_element;
	}

	/* Insufficent power, check for shields */
	else
	{
		/* Assume neither are legal */
		fire = earth = 0;

		/* Look for active shield */
		for (i = 0; i < DECK_SIZE; i++)
		{
			/* Get card pointer */
			c = &p->deck[i];

			/* Skip inactive cards */
			if (!c->active) continue;

			/* Check for shield */
			if (c->icons & (1 << real_game.fight_element))
			{
				/* Shields make announcement ok */
				fire = !real_game.fight_element;
				earth = real_game.fight_element;
			}
		}
	}

	/* Check for unsatisfied opponent cards */
	if (fire || earth)
	{
		/* Simulate game */
		sim = real_game;

		/* Set simulation flag */
		sim.simulation = 1;

		/* Set control interface to AI */
		sim.p[player_us].control = &ai_func;

		/* Check for illegal end turn */
		if (!check_end_support(&sim)) fire = earth = 0;
	}

	/* Activate/deactivate buttons */
	gtk_widget_set_sensitive(retreat_button, retreat);
	gtk_widget_set_sensitive(fire_button, fire);
	gtk_widget_set_sensitive(earth_button, earth);
}

/*
 * Play the given card.
 */
static gboolean card_played(GtkWidget *widget, GdkEventButton *event,
                            gpointer data)
{
	design *d_ptr = (design *)data;
	player *p = &real_game.p[player_us];

	/* Save state */
	save_state();

	/* Play card */
	play_card(&real_game, d_ptr, 0, 1);

	/* Check for leadership phase card */
	if (d_ptr->type == TYPE_LEADERSHIP || d_ptr->type == TYPE_INFLUENCE)
	{
		/* Set phase to retreat */
		p->phase = PHASE_RETREAT;
	}
	else if (d_ptr->type == TYPE_CHARACTER)
	{
		/* Set phase to character */
		p->phase = PHASE_CHAR;
	}
	else
	{
		/* Set phase to booster/support */
		p->phase = PHASE_SUPPORT;
	}

	/* Set button sensitivity */
	set_buttons();

	/* Redraw stuff */
	redraw_table();
	redraw_hand();
	redraw_status();

	/* Event handled */
	return TRUE;
}

/*
 * Use a card's special power.
 */
static gboolean card_used(GtkWidget *widget, GdkEventButton *event,
                          gpointer data)
{
	design *d_ptr = (design *)data;

	/* Save state */
	save_state();

	/* Use card's power */
	use_special(&real_game, d_ptr);

	/* Set button sensitivity */
	set_buttons();

	/* Redraw stuff */
	redraw_table();
	redraw_hand();
	redraw_status();

	/* Event handled */
	return TRUE;
}

/*
 * Satisfy an opponent's "discard or..." card.
 */
static gboolean card_satisfied(GtkWidget *widget, GdkEventButton *event,
                               gpointer data)
{
	design *d_ptr = (design *)data;

	/* Save state */
	save_state();

	/* Satisfy card */
	satisfy_discard(&real_game, d_ptr);

	/* Set button sensitivity */
	set_buttons();

	/* Redraw stuff */
	redraw_table();
	redraw_hand();
	redraw_status();

	/* Event handled */
	return TRUE;
}

/*
 * Land a ship.
 */
static gboolean card_landed(GtkWidget *widget, GdkEventButton *event,
                            gpointer data)
{
	design *d_ptr = (design *)data;

	/* Save state */
	save_state();

	/* Land ship */
	land_ship(&real_game, d_ptr);

	/* Redraw stuff */
	redraw_table();
	redraw_hand();
	redraw_status();

	/* Event handled */
	return TRUE;
}

/*
 * Return true if card can be played.
 */
static int card_legal(design *d_ptr)
{
	player *p = &real_game.p[player_us];

	/* Never legal to play cards after game is over */
	if (real_game.game_over) return 0;

	/* Not legal to play when not our turn */
	if (real_game.turn != player_us) return 0;

	/* Check for late leadership */
	if (d_ptr->type == TYPE_LEADERSHIP && p->phase > PHASE_LEADER)
	{
		/* Illegal */
		return 0;
	}

	/* Check for late character card */
	if (d_ptr->type == TYPE_CHARACTER && p->phase > PHASE_CHAR)
	{
		/* Illegal */
		return 0;
	}

	/* Check for early support/booster card */
	if ((d_ptr->type == TYPE_SUPPORT || d_ptr->type == TYPE_BOOSTER) &&
	    p->char_played == 0)
	{
		/* Illegal */
		return 0;
	}

	/* Check for other restrictions */
	return card_allowed(&real_game, d_ptr);
}

/*
 * Play a card (from popup menu).
 */
static int play_item(GtkMenuItem *menu_item, gpointer data)
{
	player *p = &real_game.p[player_us];
	int no_effect = GPOINTER_TO_INT(data);

	/* Save state */
	save_state();

	/* Play card */
	play_card(&real_game, popup_menu_d_ptr, no_effect, 1);

	/* Check for leadership phase card */
	if (popup_menu_d_ptr->type == TYPE_LEADERSHIP ||
	    popup_menu_d_ptr->type == TYPE_INFLUENCE)
	{
		/* Set phase to retreat */
		p->phase = PHASE_RETREAT;
	}
	else if (popup_menu_d_ptr->type == TYPE_CHARACTER)
	{
		/* Set phase to character */
		p->phase = PHASE_CHAR;
	}
	else
	{
		/* Set phase to booster/support */
		p->phase = PHASE_SUPPORT;
	}

	/* Set button sensitivity */
	set_buttons();

	/* Redraw stuff */
	redraw_table();
	redraw_hand();
	redraw_status();

	/* Event handled */
	return TRUE;
}

/*
 * Play a card as a bluff.
 */
static int bluff_item(GtkMenuItem *menu_item, gpointer data)
{
	player *p = &real_game.p[player_us];

	/* Save state */
	save_state();

	/* Play card as bluff */
	play_bluff(&real_game, popup_menu_d_ptr);

	/* Set phase to booster/support */
	p->phase = PHASE_SUPPORT;

	/* Set button sensitivity */
	set_buttons();

	/* Redraw stuff */
	redraw_table();
	redraw_hand();
	redraw_status();

	/* Event handled */
	return TRUE;
}

/*
 * Load a card onto a ship.
 */
static int load_item(GtkMenuItem *menu_item, gpointer data)
{
	player *p = &real_game.p[player_us];
	design *ship = (design *)data;

	/* Save state */
	save_state();

	/* Load card onto ship */
	load_card(&real_game, popup_menu_d_ptr, ship);

	/* Set phase to booster/support */
	p->phase = PHASE_SUPPORT;

	/* Set button sensitivity */
	set_buttons();

	/* Redraw stuff */
	redraw_table();
	redraw_hand();
	redraw_status();

	/* Event handled */
	return TRUE;
}

/*
 * Popup context menu.
 */
static gboolean popup_context(GtkWidget *widget, GdkEventButton *event,
                              gpointer data)
{
	design *d_ptr = (design *)data;
	design *ships[DECK_SIZE];
	card *c;
	player *p = &real_game.p[player_us];
	GtkWidget *play, *play_no;
	GtkWidget *load, *bluff;
	int i, num_ships = 0;
	char buf[1024];

	/* Check for left button */
	if (event->button == 1) return FALSE;

	/* Store card this menu is for */
	popup_menu_d_ptr = d_ptr;

	/* Create menu */
	popup_menu = gtk_menu_new();

	/* Create play menu item */
	play = gtk_menu_item_new_with_label(_("Play"));

	/* Set play item callback */
	g_signal_connect(G_OBJECT(play), "activate",
	                 G_CALLBACK(play_item), GINT_TO_POINTER(0));

	/* Create play with no effect menu item */
	play_no = gtk_menu_item_new_with_label(_("Play with no effect"));

	/* Set play item callback */
	g_signal_connect(G_OBJECT(play_no), "activate",
	                 G_CALLBACK(play_item), GINT_TO_POINTER(1));

	/* Check for illegal play */
	if (!card_legal(d_ptr))
	{
		/* Deactivate play items */
		gtk_widget_set_sensitive(play, FALSE);
		gtk_widget_set_sensitive(play_no, FALSE);
	}

	/* Add items to menu */
	gtk_menu_shell_append(GTK_MENU_SHELL(popup_menu), play);
	gtk_menu_shell_append(GTK_MENU_SHELL(popup_menu), play_no);

	/* Check for bluff icons */
	if (d_ptr->icons & ICON_BLUFF_MASK)
	{
		/* Add seperator */
		gtk_menu_shell_append(GTK_MENU_SHELL(popup_menu),
		                      gtk_menu_item_new());

		/* Create bluff item */
		bluff = gtk_menu_item_new_with_label(_("Play as bluff"));

		/* Set bluff item callback */
		g_signal_connect(G_OBJECT(bluff), "activate",
				 G_CALLBACK(bluff_item), NULL);

		/* Get card pointer */
		c = find_card(&real_game, player_us, d_ptr);

		/* Check for unable to play */
		if (!support_allowed(&real_game) ||
		    !(c->icons & ICON_BLUFF_MASK) ||
		    !bluff_legal(&real_game, player_us))
		{
			/* Deactivate menu item */
			gtk_widget_set_sensitive(bluff, FALSE);
		}

		/* Add item to menu */
		gtk_menu_shell_append(GTK_MENU_SHELL(popup_menu), bluff);
	}

	/* Look for active ships */
	for (i = 1; i < DECK_SIZE; i++)
	{
		/* Get card pointer */
		c = &p->deck[i];

		/* Skip inactive cards */
		if (!c->active) continue;

		/* Skip cards that are not ships */
		if (!c->d_ptr->capacity) continue;

		/* Add ship to list */
		ships[num_ships++] = c->d_ptr;
	}

	/* Check if card can never be loaded */
	if (d_ptr->type != TYPE_CHARACTER && d_ptr->type != TYPE_BOOSTER &&
	    d_ptr->type != TYPE_SUPPORT)
	{
		/* Clear ship count */
		num_ships = 0;
	}

	/* Check for ships to load */
	if (num_ships)
	{
		/* Add seperator */
		gtk_menu_shell_append(GTK_MENU_SHELL(popup_menu),
		                      gtk_menu_item_new());
	}

	/* Loop over ships */
	for (i = 0; i < num_ships; i++)
	{
		/* Create string */
		sprintf(buf, _("Load onto %s"), _(ships[i]->name));

		/* Create menu item */
		load = gtk_menu_item_new_with_label(buf);

		/* Set load item callback */
		g_signal_connect(G_OBJECT(load), "activate",
				 G_CALLBACK(load_item), ships[i]);

		/* Check for illegal load */
		if (!load_allowed(&real_game, ships[i]))
		{
			/* Deactivate menu item */
			gtk_widget_set_sensitive(load, FALSE);
		}

		/* Add item to menu */
		gtk_menu_shell_append(GTK_MENU_SHELL(popup_menu), load);
	}

	/* Show menu and items */
	gtk_widget_show_all(popup_menu);

	/* Popup menu */
	gtk_menu_popup(GTK_MENU(popup_menu), NULL, NULL, NULL, NULL,
	               event->button, event->time);

	/* Event handled */
	return TRUE;
}


/*
 * Callback to destroy child widgets so that new ones can take their place.
 */
static void destroy_widget(GtkWidget *widget, gpointer data)
{
	/* Simply destroy widget */
	gtk_widget_destroy(widget);
}

/*
 * Create an event box containing the given card's image.
 */
static GtkWidget *new_image_box(design *d_ptr, int w, int h, int color,
                                int hide)
{
	GdkPixbuf *buf;
	GtkWidget *image, *box;
	int x, y;

	/* Check for no image or hidden image */
	if (!d_ptr || hide)
	{
		/* Scale card back image */
		buf = gdk_pixbuf_scale_simple(card_back, w, h,
		                              GDK_INTERP_BILINEAR);
	}
	else
	{
		/* Get design people and index */
		x = d_ptr->people;
		y = d_ptr->index;

		/* Scale image */
		buf = gdk_pixbuf_scale_simple(image_cache[x][y], w, h,
					      GDK_INTERP_BILINEAR);
	}

	/* Check for grayscale */
	if (!color)
	{
		/* Desaturate */
		gdk_pixbuf_saturate_and_pixelate(buf, buf, 0, FALSE);
	}

	/* Make image widget */
	image = gtk_image_new_from_pixbuf(buf);

	/* Destroy our copy of the pixbuf */
	g_object_unref(G_OBJECT(buf));

	/* Make event box for image */
	box = gtk_event_box_new();

	/* Add image to event box */
	gtk_container_add(GTK_CONTAINER(box), image);

	/* Connect "pointer enter" signal */
	g_signal_connect(G_OBJECT(box), "enter-notify-event",
			 G_CALLBACK(redraw_full), d_ptr);

	/* Return event box widget */
	return box;
}

/*
 * Refresh hand area.
 */
static void redraw_hand(void)
{
	GtkWidget *box;
	player *p = &real_game.p[player_us];
	card *c;
	int i, n = 0;
	int count = 0;
	int width, height;
	int card_w, card_h;
	int legal;

	/* First destroy all child widgets */
	gtk_container_foreach(GTK_CONTAINER(hand_area), destroy_widget, NULL);

	/* Check for not our turn */
	if (real_game.turn != player_us)
	{
		/* Get number of cards in hand */
		n = p->stack[LOC_HAND];
	}
	else
	{
		/* Loop over cards */
		for (i = 1; i < DECK_SIZE; i++)
		{
			/* Get card pointer */
			c = &p->deck[i];

			/* Count eligible cards */
			if (card_eligible(&real_game, c->d_ptr)) n++;
		}
	}

	/* Get hand area width and height */
	width = hand_area->allocation.width;
	height = hand_area->allocation.height;

	/* Get width of individual card */
	card_w = width / 6;

	/* Compute height of card */
	card_h = card_w * 409 / 230;

	/* Compute pixels per card */
	if (n) width = width / n;

	/* Maximum width */
	if (width > card_w) width = card_w;

	/* Loop over cards */
	for (i = 0; i < DECK_SIZE; i++)
	{
		/* Get card pointer */
		c = &p->deck[i];

		/* Check for our turn */
		if (real_game.turn == player_us)
		{
			/* Skip ineligible cards */
			if (!card_eligible(&real_game, c->d_ptr)) continue;
		}
		else
		{
			/* Skip cards not in hand */
			if (c->where != LOC_HAND) continue;
		}

		/* Check whether card can be played */
		legal = card_legal(c->d_ptr);

		/* Get event box with image */
		box = new_image_box(c->d_ptr, card_w, card_h, legal, 0);

		/* Check for legal to play */
		if (legal)
		{
			/* Connect "button released" signal */
			g_signal_connect(G_OBJECT(box), "button-release-event",
					 G_CALLBACK(card_played), c->d_ptr);
		}

		/* Show context menu when buttons are pressed */
		g_signal_connect(G_OBJECT(box), "button-press-event",
		                 G_CALLBACK(popup_context), c->d_ptr);

		/* Place event box with image */
		gtk_fixed_put(GTK_FIXED(hand_area), box, count * width, 0);

		/* Show image */
		gtk_widget_show_all(box);

		/* Check for disclosed */
		if (c->disclosed)
		{
			/* Add tooltip to card */
			gtk_widget_set_tooltip_text(box, _("Disclosed"));
		}

		/* Count images shown */
		count++;
	}

	/* Switch to opponent pointer */
	p = &real_game.p[!player_us];

	/* First destroy all child widgets */
	gtk_container_foreach(GTK_CONTAINER(opp_hand), destroy_widget, NULL);

	/* Count cards in hand */
	n = p->stack[LOC_HAND];

	/* Get hand area width and height */
	width = opp_hand->allocation.width;
	height = opp_hand->allocation.height;

	/* Get width of individual card */
	card_w = width / 6;

	/* Compute height of card */
	card_h = card_w * 409 / 230;

	/* Compute pixels per card */
	if (n) width = width / n;

	/* Maximum width */
	if (width > card_w) width = card_w;

	/* Clear count */
	count = 0;

	/* Loop over cards */
	for (i = 0; i < DECK_SIZE; i++)
	{
		/* Get card pointer */
		c = &p->deck[i];

		/* Skip cards not in hand */
		if (c->where != LOC_HAND) continue;

		/* Get event box with image */
		box = new_image_box(c->disclosed ? c->d_ptr : NULL,
		                    card_w, card_h, 1, 0);

		/* Place event box with image */
		gtk_fixed_put(GTK_FIXED(opp_hand), box, count * width, 0);

		/* Show image */
		gtk_widget_show_all(box);

		/* Count images shown */
		count++;
	}
}

/*
 * Redraw area where cards are played.
 */
static void redraw_table_area(int who, GtkWidget *area)
{
	GtkWidget *box;
	player *p = &real_game.p[who];
	player *opp = &real_game.p[!who];
	card *c, *d, *list[DECK_SIZE];
	int i, j, n, extra = 0;
	int x, y;
	int width, height, dy;
	int card_w, card_h;
	int num_infl, num_support, num_combat;
	char buf[80];

	/* First destroy all child widgets */
	gtk_container_foreach(GTK_CONTAINER(area), destroy_widget, NULL);

	/* Assume no influence slots */
	num_infl = 0;

	/* Loop over our cards */
	for (i = 1; i < DECK_SIZE; i++)
	{
		/* Get card pointer */
		c = &p->deck[i];

		/* Skip inactive cards */
		if (!c->active) continue;

		/* Count active influence cards */
		if (c->type == TYPE_INFLUENCE) num_infl++;
	}

	/* Loop over opponent cards */
	for (i = 1; i < DECK_SIZE; i++)
	{
		/* Get card pointer */
		c = &opp->deck[i];

		/* Skip inactive cards */
		if (!c->active) continue;

		/* Count active influence cards */
		if (c->type == TYPE_INFLUENCE) extra++;
	}

	/* Check for more influence cards on our side */
	if (num_infl > extra) extra = num_infl;

	/* Get table area width */
	width = area->allocation.width;

	/* Get table area height */
	height = area->allocation.height;

	/* Get width of individual card */
	card_w = width / (6 + extra);

	/* Compute height of card */
	card_h = card_w * 409 / 230;

	/* Check for active influence cards */
	if (num_infl)
	{
		/* Start at left side */
		x = 0;

		/* Loop over deck */
		for (i = 1; i < DECK_SIZE; i++)
		{
			/* Get card pointer */
			c = &p->deck[i];

			/* Skip inactive cards */
			if (!c->active) continue;

			/* Skip non-influence cards */
			if (c->type != TYPE_INFLUENCE) continue;

			/* Make image box */
			box = new_image_box(c->d_ptr, card_w, card_h, 1, 0);

			/* Place event box */
			gtk_fixed_put(GTK_FIXED(area), box, x * card_w, 0);

			/* Check for landable ship */
			if (real_game.turn == player_us &&
			    who == player_us &&
			    p->phase == PHASE_BEGIN &&
			    c->d_ptr->capacity && !c->landed)
			{
				/* Connect "button released" signal */
				g_signal_connect(G_OBJECT(box),
				                 "button-release-event",
						 G_CALLBACK(card_landed),
				                 c->d_ptr);
			}

			/* Maximum height */
			dy = height / 5;

			/* One card displayed so far */
			y = 1;

			/* Loop over deck */
			for (j = 1; j < DECK_SIZE; j++)
			{
				/* Get card pointer */
				d = &p->deck[j];

				/* Skip cards not loaded on influence card */
				if (d->ship != c->d_ptr) continue;

				/* Make image box */
				box = new_image_box(d->d_ptr, card_w, card_h,
				                    0, 0);

				/* Place event box */
				gtk_fixed_put(GTK_FIXED(area), box, x * card_w,
				              y * dy);

				/* One more card displayed */
				y++;
			}

			/* Move to next slot */
			x++;
		}
	}

	/* Count support cards */
	num_support = p->stack[LOC_SUPPORT];

	/* Check for support cards played */
	if (num_support)
	{
		/* No cards drawn yet */
		n = 0;

		/* Compute width of each card */
		width = card_w * 2 / num_support;

		/* Maximum width */
		if (width > card_w) width = card_w;

		/* Loop over cards */
		for (i = 1; i < DECK_SIZE; i++)
		{
			/* Get card pointer */
			c = &p->deck[i];

			/* Skip cards not in support pile */
			if (c->where != LOC_SUPPORT) continue;

			/* Check for opponent bluff card */
			if (who != player_us && c->bluff)
			{
				/* Make hidden image box */
				box = new_image_box(NULL, card_w, card_h, 1, 0);
			}
			else
			{
				/* Make image box */
				box = new_image_box(c->d_ptr, card_w, card_h, 1,
						    c->bluff);
			}

			/* Place event box */
			gtk_fixed_put(GTK_FIXED(area), box,
			              n * width + extra * card_w, 0);

			/* One more support card drawn */
			n++;

			/* Check for retrievable */
			if (real_game.turn == player_us &&
			    who == player_us &&
			    p->phase == PHASE_BEGIN &&
			    retrieve_legal(&real_game, c))
			{
				/* Connect "button released" signal */
				g_signal_connect(G_OBJECT(box),
				                 "button-release-event",
						 G_CALLBACK(card_retrieved),
				                 c->d_ptr);
			}

			/* Check for bluff card */
			if (real_game.turn == player_us &&
			    who == player_us &&
			    p->phase == PHASE_BEGIN &&
			    c->bluff)
			{
				/* Connect "button released" signal */
				g_signal_connect(G_OBJECT(box),
				                 "button-release-event",
						 G_CALLBACK(card_revealed),
				                 c->d_ptr);
			}

			/* Check for "on my turn" special power */
			if (real_game.turn == player_us &&
			    who == player_us &&
			    c->d_ptr->special_time == TIME_MYTURN &&
			    !c->used && !c->text_ignored)
			{
				/* Connect "button released" signal */
				g_signal_connect(G_OBJECT(box),
				                 "button-release-event",
				                 G_CALLBACK(card_used),
				                 c->d_ptr);
			}

			/* Check for card to satisfy */
			if (real_game.turn == player_us &&
			    who != player_us &&
			    c->d_ptr->special_cat == 7 &&
			    (c->d_ptr->special_effect & S7_DISCARD_MASK) &&
			    !c->text_ignored && !c->used &&
			    satisfy_possible(&real_game, c->d_ptr))
			{
				/* Connect "button released" signal */
				g_signal_connect(G_OBJECT(box),
				                 "button-release-event",
				                 G_CALLBACK(card_satisfied),
				                 c->d_ptr);
			}
		}
	}

	/* Assume no active combat cards */
	num_combat = 0;

	/* Loop over our cards */
	for (i = 1; i < DECK_SIZE; i++)
	{
		/* Get card pointer */
		c = &p->deck[i];

		/* Skip cards not in combat area */
		if (c->where != LOC_COMBAT) continue;

		/* Skip non-character cards */
		if (c->d_ptr->type != TYPE_CHARACTER) continue;

		/* Skip inactive cards */
		if (!c->active) continue;

		/* Add card to list */
		list[num_combat++] = c;
	}

	/* Loop over our cards */
	for (i = 1; i < DECK_SIZE; i++)
	{
		/* Get card pointer */
		c = &p->deck[i];

		/* Skip cards not in combat area */
		if (c->where != LOC_COMBAT) continue;

		/* Skip non-booster cards */
		if (c->d_ptr->type != TYPE_BOOSTER) continue;

		/* Skip inactive cards */
		if (!c->active) continue;

		/* Add card to list */
		list[num_combat++] = c;
	}

	/* Check for combat cards to draw */
	if (num_combat)
	{
		/* No cards drawn yet */
		n = 0;

		/* Determine area per card */
		dy = height / num_combat;

		/* Maximum height */
		if (dy > card_h / 5) dy = card_h / 5;

		/* Loop over cards in list */
		for (i = 0; i < num_combat; i++)
		{
			/* Get card pointer */
			c = list[i];

			/* Make image box */
			box = new_image_box(c->d_ptr, card_w, card_h, 1, 0);

			/* Place event box */
			gtk_fixed_put(GTK_FIXED(area), box,
			              card_w * (2 + extra), n * dy);

			/* One more combat card drawn */
			n++;

			/* Check for retrievable */
			if (real_game.turn == player_us &&
			    who == player_us &&
			    p->phase == PHASE_BEGIN &&
			    retrieve_legal(&real_game, c))
			{
				/* Connect "button released" signal */
				g_signal_connect(G_OBJECT(box),
				                 "button-release-event",
						 G_CALLBACK(card_retrieved),
				                 c->d_ptr);
			}

			/* Check for "on my turn" special power */
			if (real_game.turn == player_us &&
			    who == player_us &&
			    c->d_ptr->special_time == TIME_MYTURN &&
			    !c->used && !c->text_ignored)
			{
				/* Connect "button released" signal */
				g_signal_connect(G_OBJECT(box),
				                 "button-release-event",
				                 G_CALLBACK(card_used),
				                 c->d_ptr);
			}

			/* Check for card to satisfy */
			if (real_game.turn == player_us &&
			    who != player_us &&
			    c->d_ptr->special_cat == 7 &&
			    (c->d_ptr->special_effect & S7_DISCARD_MASK) &&
			    !c->text_ignored && !c->used &&
			    satisfy_possible(&real_game, c->d_ptr))
			{
				/* Connect "button released" signal */
				g_signal_connect(G_OBJECT(box),
				                 "button-release-event",
				                 G_CALLBACK(card_satisfied),
				                 c->d_ptr);
			}
		}
	}

	/* Check for cards in draw pile */
	if (p->stack[LOC_DRAW])
	{
		/* Get event box */
		box = new_image_box(NULL, card_w, card_h, 1, 0);

		/* Make tooltip text */
		sprintf(buf,
		        ngettext("%d card", "%d cards", p->stack[LOC_DRAW]),
		        p->stack[LOC_DRAW]);

		/* Add tooltip for number of cards */
		gtk_widget_set_tooltip_text(box, buf);

		/* Place event box */
		gtk_fixed_put(GTK_FIXED(area), box, card_w * (3 + extra), 0);
	}

	/* Check for discard */
	if (p->last_discard)
	{
		/* Get event box */
		box = new_image_box(p->last_discard, card_w, card_h, 0, 0);

		/* Make tooltip text */
		sprintf(buf,
		        ngettext("%d card", "%d cards", p->stack[LOC_DISCARD]),
		        p->stack[LOC_DISCARD]);

		/* Add tooltip for number of cards */
		gtk_widget_set_tooltip_text(box, buf);

		/* Place event box */
		gtk_fixed_put(GTK_FIXED(area), box, card_w * (4 + extra), 0);
	}

	/* Get event box for leadership card */
	box = new_image_box(p->last_leader, card_w, card_h, 1, 0);

	/* Place event box */
	gtk_fixed_put(GTK_FIXED(area), box, card_w * (5 + extra), 0);

	/* Show all widgets */
	gtk_widget_show_all(area);
}

/*
 * Redraw both our and opponent's table area.
 */
static void redraw_table(void)
{
	/* Redraw our area */
	redraw_table_area(player_us, our_area);

	/* Redraw opponent's area */
	redraw_table_area(!player_us, opp_area);
}

/*
 * Request new height, computed from area width.
 */
static void card_area_resized(GtkWidget *widget, GtkRequisition *requisition,
                              gpointer data)
{
	int req_height;
	int full = GPOINTER_TO_INT(data);

	/* Determine height to request */
	req_height = (widget->allocation.width / 6) * 409 / 230;

	/* Shrink if not fullsize */
	if (!full) req_height /= 2;

	/* Request height to match width */
	gtk_widget_set_size_request(widget, 0, req_height);
}

/*
 * Redraw table and hand.
 */
static void redraw_everything(GtkWidget *widget, GtkAllocation *allocation,
                              gpointer data)
{
	static int old_width, old_height;

	/* Check for no difference from before */
	if (allocation->width == old_width && allocation->height == old_height)
	{
		/* Do nothing */
		return;
	}

	/* Remember current size */
	old_width = allocation->width;
	old_height = allocation->height;

	/* Redraw table and hand */
	redraw_table();
	redraw_hand();
}

/*
 * Redraw status labels.
 */
static void redraw_status(void)
{
	player *p;
	char buf[1024];
	int power;

	/* Get our player pointer */
	p = &real_game.p[player_us];

	/* Check for fight started */
	if (real_game.fight_started)
	{
		/* Compute power */
		power = compute_power(&real_game, player_us);
	}
	else
	{
		/* No power */
		power = 0;
	}

	/* Make label */
	sprintf(buf, _("Dragons: %d  Cards: %d  Power: %d"), p->dragons,
	             p->stack[LOC_COMBAT] + p->stack[LOC_SUPPORT], power);

	/* Set label */
	gtk_label_set_text(GTK_LABEL(our_status), buf);

	/* Get opponent player pointer */
	p = &real_game.p[!player_us];

	/* Check for fight started */
	if (real_game.fight_started)
	{
		/* Compute power */
		power = compute_power(&real_game, !player_us);
	}
	else
	{
		/* No power */
		power = 0;
	}

	/* Make label */
	sprintf(buf, _("Dragons: %d  Cards: %d  Power: %d"), p->dragons,
	             p->stack[LOC_COMBAT] + p->stack[LOC_SUPPORT], power);

	/* Set label */
	gtk_label_set_text(GTK_LABEL(opp_status), buf);
}

/*
 * After our turn, let the AI take actions until it is our turn again,
 * then refresh the drawing areas so that we can take our next turn.
 */
static void handle_end_turn(void)
{
	/* Deactivate "retreat" button */
	gtk_widget_set_sensitive(retreat_button, FALSE);

	/* Deactivate "undo" button */
	gtk_widget_set_sensitive(undo_button, FALSE);

	/* Deactivate "announce" buttons */
	gtk_widget_set_sensitive(fire_button, FALSE);
	gtk_widget_set_sensitive(earth_button, FALSE);

	/* Have AI take actions until it is our turn */
	while (real_game.turn != player_us && !real_game.game_over)
	{
		/* Request action from AI */
		real_game.p[real_game.turn].control->take_action(&real_game);

		/* Redraw everything */
		redraw_table();
		redraw_hand();
		redraw_status();

		/* Loop until all GTK events are handled */
		while (g_main_context_iteration(NULL, FALSE));
	}

	/* No undo available */
	backup_set = 0;

	/* Set "retreat" button */
	gtk_widget_set_sensitive(retreat_button, !real_game.game_over);

	/* Start turn */
	start_turn(&real_game);

	/* Advance to beginning of turn phase */
	real_game.p[player_us].phase = PHASE_BEGIN;

	/* Draw stuff */
	redraw_table();
	redraw_hand();
	redraw_status();
}

/*
 * Handle press of the "retreat" button.
 */
static void retreat_clicked(GtkButton *button, gpointer data)
{
	/* Retreat */
	retreat(&real_game);

	/* Let AI take turn */
	handle_end_turn();
}

/*
 * Handle press of the "undo" button.
 */
static void undo_clicked(GtkButton *button, gpointer data)
{
	/* Restore game */
	real_game = backup;

	/* No backup available */
	backup_set = 0;

	/* Deactivate undo button */
	gtk_widget_set_sensitive(undo_button, FALSE);

	/* Retreat is always legal after undo */
	gtk_widget_set_sensitive(retreat_button, TRUE);

	/* Announce power is always illegal */
	gtk_widget_set_sensitive(fire_button, FALSE);
	gtk_widget_set_sensitive(earth_button, FALSE);

	/* Draw stuff */
	redraw_table();
	redraw_hand();
	redraw_status();

	/* Message */
	message_add(_("Current turn undone.\n"));
}

/*
 * Handle press of "announce" button.
 */
static void announce_clicked(GtkButton *button, gpointer data)
{
	int element = GPOINTER_TO_INT(data);
	int old_turn = real_game.turn;

	/* Handle end of support phase */
	end_support(&real_game);

	/* Announce power */
	announce_power(&real_game, element);

	/* Check for end of fight due to forced retreat */
	if (!real_game.fight_started)
	{
		/* Redraw everything */
		redraw_hand();
		redraw_table();
		redraw_status();

		/* Done */
		return;
	}

	/* Refresh hand */
	refresh_phase(&real_game);

	/* End turn */
	end_turn(&real_game);

	/* Check for change of turn due to forced retreat */
	if (real_game.turn != old_turn)
	{
		/* Let AI take turn */
		handle_end_turn();

		/* Done */
		return;
	}

	/* Turn over */
	real_game.p[player_us].phase = PHASE_NONE;

	/* Go to next player */
	real_game.turn = !real_game.turn;

	/* Start next player's turn */
	real_game.p[real_game.turn].phase = PHASE_START;

	/* Let AI take turn */
	handle_end_turn();
}

/*
 * Add or remove the given card to the set of chosen cards.
 */
static gboolean choice_made(GtkWidget *widget, GdkEventButton *event,
                            gpointer data)
{
	int i = GPOINTER_TO_INT(data);

	/* Add/remove choice */
	current_choices ^= 1 << i;

	/* Redraw choice area */
	redraw_choice();

	/* Event handled */
	return TRUE;
}

/*
 * Redraw the choose dialog card area.
 */
static void redraw_choice(void)
{
	game sim;
	design *chosen[DECK_SIZE];
	GtkWidget *image, *box;
	GdkPixbuf *buf;
	int i, width, height;
	int card_w, card_h;
	int x, y, n = 0;

	/* First destroy all child widgets */
	gtk_container_foreach(GTK_CONTAINER(choice_area), destroy_widget, NULL);

	/* Get choice area width and height */
	width = choice_area->allocation.width;
	height = choice_area->allocation.height;

	/* Get width of individual card */
	card_w = width / 6;

	/* Minimum card width */
	if (card_w <= 0) return;

	/* Check for no choices */
	if (!card_num_choices)
	{
		/* Error */
		printf(_("gui_choose: No choices\n"));
		return;
	}

	/* Compute height of card */
	card_h = card_w * 409 / 230;

	/* Compute pixels per card */
	width = width / card_num_choices;

	/* Maximum width */
	if (width > card_w) width = card_w;

	/* Loop over choices */
	for (i = 0; i < card_num_choices; i++)
	{
		/* Check for empty design */
		if (!card_choices[i])
		{
			/* Scale card back image */
			buf = gdk_pixbuf_scale_simple(card_back, card_w, card_h,
			                              GDK_INTERP_BILINEAR);
		}
		else
		{
			/* Get card design's people and index */
			x = card_choices[i]->people;
			y = card_choices[i]->index;

			/* Scale card image */
			buf = gdk_pixbuf_scale_simple(image_cache[x][y], card_w,
			                              card_h,
						      GDK_INTERP_BILINEAR);
		}

		/* Check for card chosen */
		if (current_choices & (1 << i))
		{
			/* Save choices made */
			chosen[n++] = card_choices[i];
		}
		else
		{
			/* Desaturate */
			gdk_pixbuf_saturate_and_pixelate(buf, buf, 0, FALSE);
		}

		/* Make image widget */
		image = gtk_image_new_from_pixbuf(buf);

		/* Destroy our copy of the pixbuf */
		g_object_unref(G_OBJECT(buf));

		/* Make event box for image */
		box = gtk_event_box_new();

		/* Add image to event box */
		gtk_container_add(GTK_CONTAINER(box), image);

		/* Connect "pointer enter" signal */
		g_signal_connect(G_OBJECT(box), "enter-notify-event",
		                 G_CALLBACK(redraw_full), card_choices[i]);

		/* Connect "button released" signal */
		g_signal_connect(G_OBJECT(box), "button-release-event",
		                 G_CALLBACK(choice_made), GINT_TO_POINTER(i));

		/* Place event box with image */
		gtk_fixed_put(GTK_FIXED(choice_area), box, i * width, 0);

		/* Show image */
		gtk_widget_show_all(box);
	}

	/* Deactivate dialog OK button */
	gtk_widget_set_sensitive(choose_ok, FALSE);

	/* Check for too few or too many */
	if (n < choice_min || n > choice_max)
	{
		/* Done */
		return;
	}

	/* Copy game state */
	sim = *choice_game;

	/* Set simulation flag */
	sim.simulation = 1;

	/* Attempt callback */
	if (choice_callback(&sim, choice_who, chosen, n, choice_data))
	{
		/* Activate dialog OK button */
		gtk_widget_set_sensitive(choose_ok, TRUE);
	}
}

/*
 * Redraw choice dialog when resized.
 */
static void redraw_choice_dialog(GtkWidget *widget, GtkAllocation *allocation,
                                 gpointer data)
{
	/* Check for no difference from before */
	if (allocation->width == choice_width &&
	    allocation->height == choice_height)
	{
		/* Do nothing */
		return;
	}

	/* Remember current size */
	choice_width = allocation->width;
	choice_height = allocation->height;

	/* Redraw choice area */
	redraw_choice();
}

/*
 * Handle a request to choose a set of cards.
 */
static void gui_choose(game *g, int chooser, int who, design **choices,
                       int num_choices, int min, int max,
                       choose_result callback, void *data, char *prompt)
{
	GtkWidget *prompt_label, *minmax_label;
	char minmax[1024];
	design *chosen[DECK_SIZE];
	int i, n = 0;

	/* Check for no real choice */
	if (min == num_choices)
	{
		/* Call callback */
		callback(g, who, choices, num_choices, data);

		/* Done */
		return;
	}

	/* Check for simulated choice */
	if (g->simulation) return;
	
	/* Redraw everything */
	redraw_table();
	redraw_hand();
	redraw_status();

	/* Create dialog */
	choose_dialog = gtk_dialog_new();

	/* Hide the close button */
	/* gtk_window_set_deletable(GTK_WINDOW(choose_dialog), FALSE); */

	/* Default size */
	gtk_window_set_default_size(GTK_WINDOW(choose_dialog), 700, 300);

	/* Make dialog modal */
	gtk_window_set_modal(GTK_WINDOW(choose_dialog), TRUE);

	/* Put dialog on top of parent */
	gtk_window_set_transient_for(GTK_WINDOW(choose_dialog),
	                             GTK_WINDOW(window));

	/* Destroy dialog with parent */
	gtk_window_set_destroy_with_parent(GTK_WINDOW(choose_dialog), TRUE);

	/* Add 'OK' button */
	choose_ok = gtk_dialog_add_button(GTK_DIALOG(choose_dialog),
	                                  GTK_STOCK_OK, 1);

	/* Make label */
	prompt_label = gtk_label_new(prompt);

	/* Add label to dialog */
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(choose_dialog)->vbox),
	                   prompt_label, FALSE, FALSE, 0);

	/* Make prompt with minimum and maximum */
	sprintf(minmax, _("Min: %d, Max %d"), min, max);

	/* Make minmax label */
	minmax_label = gtk_label_new(minmax);

	/* Add label to dialog */
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(choose_dialog)->vbox),
	                   minmax_label, FALSE, FALSE, 0);

	/* Create area to display choices */
	choice_area = gtk_fixed_new();

	/* Put choice area in dialog box */
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(choose_dialog)->vbox),
	                   choice_area, TRUE, TRUE, 0);

	/* Have choice area negotiate new size if resized */
	g_signal_connect(G_OBJECT(choice_area), "size-request",
	                 G_CALLBACK(card_area_resized), GINT_TO_POINTER(1));

	/* Redraw card area if we are resized */
	g_signal_connect(G_OBJECT(choice_area), "size-allocate",
	                 G_CALLBACK(redraw_choice_dialog), NULL);

	/* Assume no choices made yet */
	current_choices = 0;

	/* Copy choices */
	for (i = 0; i < num_choices; i++) card_choices[i] = choices[i];

	/* Save number of choices */
	card_num_choices = num_choices;

	/* Save minimum and maximum number to choose */
	choice_min = min;
	choice_max = max;

	/* Save player */
	choice_who = who;

	/* Save data */
	choice_data = data;

	/* Save callback */
	choice_callback = callback;

	/* Save pointer to game state */
	choice_game = g;

	/* Clear saved choice area size */
	choice_width = choice_height = 0;

	/* Draw choice area */
	redraw_choice();

	/* Show dialog */
	gtk_widget_show_all(choose_dialog);

	/* Run dialog */
	gtk_dialog_run(GTK_DIALOG(choose_dialog));

	/* Destroy dialog */
	gtk_widget_destroy(choose_dialog);

	/* Loop over choices */
	for (i = 0; i < num_choices; i++)
	{
		/* Check for chosen */
		if (current_choices & (1 << i))
		{
			/* Add to list */
			chosen[n++] = choices[i];
		}
	}

	/* Call callback */
	choice_callback(g, who, chosen, n, data);

	/* Consider a user choice to be a random event */
	g->random_event = 1;
}

/*
 * Ask the user to choose whether to call the opponent's bluff or not.
 */
static int gui_call_bluff(struct game *g)
{
	GtkWidget *bluff_dialog, *label;
	int response;

	/* Redraw stuff */
	redraw_table();
	redraw_hand();
	redraw_status();

	/* Create dialog box */
	bluff_dialog = gtk_dialog_new_with_buttons(_("Call bluff"),
	                    GTK_WINDOW(window),
	                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
	                    GTK_STOCK_YES, GTK_RESPONSE_YES,
	                    GTK_STOCK_NO, GTK_RESPONSE_NO, NULL);

	/* Create label */
	label = gtk_label_new(_("Call bluff?"));

	/* Add label to dialog box */
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(bluff_dialog)->vbox), label);

	/* Show contents of dialog */
	gtk_widget_show_all(bluff_dialog);

	/* Run dialog */
	response = gtk_dialog_run(GTK_DIALOG(bluff_dialog));

	/* Destroy dialog */
	gtk_widget_destroy(bluff_dialog);

	/* Check for yes */
	if (response == GTK_RESPONSE_YES) return 1;

	/* Assume no */
	return 0;
}

static interface gui_func =
{
	NULL,
	NULL,
	gui_choose,
	gui_call_bluff,
	NULL,
};

/*
 * New game.
 */
static void new_game(GtkMenuItem *menu_item, gpointer data)
{
	/* Clear text log */
	clear_log();

	/* Reinitialize game */
	init_game(&real_game, 1);

	/* Let the AI player take their turn and setup for our first turn */
	handle_end_turn();
}

/*
 * Quit.
 */
static void quit_game(GtkMenuItem *menu_item, gpointer data)
{
	/* Quit */
	gtk_main_quit();
}

/*
 * Set people pointers in player structures.
 *
 * If necessary, reverse them so that lower index people is at player 0.
 */
static void set_people(void)
{
	/* Check for human player having later people */
	if (human_people > ai_people)
	{
		/* Set player peoples */
		real_game.p[0].p_ptr = &peoples[ai_people];
		real_game.p[1].p_ptr = &peoples[human_people];

		/* Human is second player */
		player_us = 1;
	}
	else
	{
		/* Set player peoples */
		real_game.p[0].p_ptr = &peoples[human_people];
		real_game.p[1].p_ptr = &peoples[ai_people];

		/* Human is first player */
		player_us = 0;
	}

	/* Load card images */
	load_images();

	/* Initialize game */
	init_game(&real_game, 1);

	/* Set opponent interface */
	real_game.p[!player_us].control = &ai_func;

	/* Call opponent initialization */
	real_game.p[!player_us].control->init(&real_game, !player_us);

	/* Set our interface to AI functions (briefly) */
	real_game.p[player_us].control = &ai_func;

	/* Initialize AI for our side */
	real_game.p[player_us].control->init(&real_game, player_us);

	/* Set our interface to GUI */
	real_game.p[player_us].control = &gui_func;
}

/*
 * React to a people selection button being toggled.
 */
static void human_toggle(GtkToggleButton *button, gpointer data)
{
	int i = GPOINTER_TO_INT(data);

	/* Check for button set */
	if (gtk_toggle_button_get_active(button)) human_people = i;
}

/*
 * React to a people selection button being toggled.
 */
static void ai_toggle(GtkToggleButton *button, gpointer data)
{
	int i = GPOINTER_TO_INT(data);

	/* Check for button set */
	if (gtk_toggle_button_get_active(button)) ai_people = i;
}

/*
 * Pop up a dialog box to let the user choose a People for each player.
 */
static void select_matchup(GtkMenuItem *menu_item, gpointer data)
{
	GtkWidget *select_dialog;
	GtkWidget *radio = NULL;
	GtkWidget *hbox, *box, *frame;
	int i;

	/* Create dialog window */
	select_dialog = gtk_dialog_new_with_buttons(_("Select Matchup"),
	                                            GTK_WINDOW(window),
	                                            GTK_DIALOG_MODAL,
	                                            GTK_STOCK_OK,
	                                            GTK_RESPONSE_ACCEPT,
	                                            GTK_STOCK_CANCEL,
	                                            GTK_RESPONSE_REJECT,
	                                            NULL);

	/* Create horizontal box for button groups */
	hbox = gtk_hbox_new(FALSE, 0);

	/* Create vertical button box for human radio buttons */
	box = gtk_vbutton_box_new();

	/* Create decorative frame */
	frame = gtk_frame_new(_("Human"));

	/* Add button box to frame */
	gtk_container_add(GTK_CONTAINER(frame), box);

	/* Add frame to hbox */
	gtk_container_add(GTK_CONTAINER(hbox), frame);

	/* Loop over peoples */
	for (i = 0; i < MAX_PEOPLE; i++)
	{
		/* Make radio button for this people */
		radio = gtk_radio_button_new_with_label_from_widget(
		                                  GTK_RADIO_BUTTON(radio),
		                                  peoples[i].name);

		/* Check for current player */
		if (real_game.p[player_us].p_ptr == &peoples[i])
		{
			/* Set button active */
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio),
			                             TRUE);

			/* Set desired people */
			human_people = i;
		}

		/* Add handler */
		g_signal_connect(G_OBJECT(radio), "toggled",
		                 G_CALLBACK(human_toggle), GINT_TO_POINTER(i));

		/* Add button to box */
		gtk_container_add(GTK_CONTAINER(box), radio);
	}

	/* Make radio button for random selection */
	radio = gtk_radio_button_new_with_label_from_widget(
	                                                GTK_RADIO_BUTTON(radio),
	                                                _("Random"));

	/* Add handler */
	g_signal_connect(G_OBJECT(radio), "toggled",
	                 G_CALLBACK(human_toggle), GINT_TO_POINTER(MAX_PEOPLE));

	/* Add button to box */
	gtk_container_add(GTK_CONTAINER(box), radio);

	/* Clear radio button widget */
	radio = NULL;

	/* Create vertical button box for AI radio buttons */
	box = gtk_vbutton_box_new();

	/* Create decorative frame */
	frame = gtk_frame_new(_("Computer"));

	/* Add button box to frame */
	gtk_container_add(GTK_CONTAINER(frame), box);

	/* Add frame to hbox */
	gtk_container_add(GTK_CONTAINER(hbox), frame);

	/* Loop over peoples */
	for (i = 0; i < MAX_PEOPLE; i++)
	{
		/* Make radio button for this people */
		radio = gtk_radio_button_new_with_label_from_widget(
		                                  GTK_RADIO_BUTTON(radio),
		                                  peoples[i].name);

		/* Check for current player */
		if (real_game.p[!player_us].p_ptr == &peoples[i])
		{
			/* Set button active */
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio),
			                             TRUE);

			/* Set desired people */
			ai_people = i;
		}

		/* Add handler */
		g_signal_connect(G_OBJECT(radio), "toggled",
				 G_CALLBACK(ai_toggle), GINT_TO_POINTER(i));

		/* Add button to box */
		gtk_container_add(GTK_CONTAINER(box), radio);
	}

	/* Make radio button for this people */
	radio = gtk_radio_button_new_with_label_from_widget(
	                                                GTK_RADIO_BUTTON(radio),
	                                                _("Random"));

	/* Add handler */
	g_signal_connect(G_OBJECT(radio), "toggled",
			 G_CALLBACK(ai_toggle), GINT_TO_POINTER(MAX_PEOPLE));

	/* Add button to box */
	gtk_container_add(GTK_CONTAINER(box), radio);

	/* Add hbox to dialog */
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(select_dialog)->vbox), hbox);

	/* Show everything */
	gtk_widget_show_all(select_dialog);

	/* Show dialog and run */
	if (gtk_dialog_run(GTK_DIALOG(select_dialog)) == GTK_RESPONSE_ACCEPT)
	{
		/* Check for random human */
		if (human_people == MAX_PEOPLE)
		{
			/* Randomize */
			do
			{
				/* Pick random people */
				human_people = myrand(&real_game.random_seed) %
				               MAX_PEOPLE;

			} while (human_people == ai_people);
		}

		/* Check for random AI */
		if (ai_people == MAX_PEOPLE)
		{
			/* Randomize */
			do
			{
				/* Pick random people */
				ai_people = myrand(&real_game.random_seed) %
				            MAX_PEOPLE;
			
			} while (ai_people == human_people);
		}

		/* Ensure user did not pick same peoples */
		if (human_people != ai_people)
		{
			/* Clear text log */
			clear_log();

			/* Set up player pointers to people */
			set_people();

			/* Change frame labels */
			gtk_frame_set_label(GTK_FRAME(opp_frame),
					    peoples[ai_people].name);
			gtk_frame_set_label(GTK_FRAME(our_frame),
					    peoples[human_people].name);

			/* Clear full-size image */
			redraw_full(NULL, NULL, NULL);

			/* Let the AI player take their turn */
			handle_end_turn();
		}
	}

	/* Destroy dialog */
	gtk_widget_destroy(select_dialog);
}

/*
 * Dump debugging information.
 */
static void dump_debug(GtkMenuItem *menu_item, gpointer data)
{
	GtkWidget *dialog, *disclose;
	GtkWidget *label;
	player *p;
	card *c;
	int i;
	char buf[1024];

	/* Create dialog box */
	dialog = gtk_dialog_new_with_buttons(_("Debug"), GTK_WINDOW(window), 0,
	                                     GTK_STOCK_OK,
	                                     GTK_RESPONSE_ACCEPT, NULL);

	/* Create label text */
	sprintf(buf, _("Game random seed: %u"), real_game.start_seed);

	/* Create label with game start seed */
	label = gtk_label_new(buf);

	/* Create checkbox for disclosing opponent's cards */
	disclose = gtk_check_button_new_with_label(_("Disclose opponent hand"));

	/* Pack widgets into dialog box */
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), disclose);

	/* Show everything */
	gtk_widget_show_all(dialog);

	/* Run dialog */
	gtk_dialog_run(GTK_DIALOG(dialog));

	/* Check disclose button */
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(disclose)))
	{
		/* Get opponent player pointer */
		p = &real_game.p[!player_us];

		/* Loop over cards */
		for (i = 1; i < DECK_SIZE; i++)
		{
			/* Get card pointer */
			c = &p->deck[i];

			/* Skip cards not in hand */
			if (c->where != LOC_HAND) continue;

			/* Disclose card */
			c->disclosed = 1;
		}

		/* Redraw hand */
		redraw_hand();
	}

	/* Destroy dialog box */
	gtk_widget_destroy(dialog);
}

/*
 * Get help from the AI.
 */
static void assist_action(GtkMenuItem *menu_item, gpointer data)
{
	GtkWidget *dialog;
	char buffer[1024];

	/* Ask AI about best action */
	ai_assist(&real_game, buffer);

	/* Create dialog */
	dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
	                                GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE,
	                                buffer);

	/* Run dialog */
	gtk_dialog_run(GTK_DIALOG(dialog));

	/* Destroy dialog */
	gtk_widget_destroy(dialog);
}

/*
 * Show an "about" dialog.
 */
static void about_dialog(GtkMenuItem *menu_item, gpointer data)
{
	GtkWidget *dialog;
	GtkWidget *image;

	/* Create dialog */
	dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
	                                GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE,
					"Blue Moon " VERSION);

	/* Set secondary text */
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
_("This program is written by Keldon Jones, and the source code is licensed \
under the GNU General Public License.\n\n\
The original Blue Moon card game is authored by Reiner Knizia.\n\n\
Blue Moon is published by Kosmos.  All copyrights are owned by Kosmos.\n\n\
Windows porting and other improvements made by Dean Svendsen.\n\n\
Send bug reports to keldon@keldon.net"));

	/* Create image from card back */
	image = gtk_image_new_from_pixbuf(card_back);

	/* Show image */
	gtk_widget_show(image);

	/* Set dialog's image */
	gtk_message_dialog_set_image(GTK_MESSAGE_DIALOG(dialog), image);

	/* Run dialog */
	gtk_dialog_run(GTK_DIALOG(dialog));

	/* Destroy dialog */
	gtk_widget_destroy(dialog);
}

/*
 * Main window was destroyed.
 */
static void destroy(GtkWidget *widget, gpointer data)
{
	/* Quit */
	gtk_main_quit();
}

/*
 * Setup windows, callbacks, etc, then let GTK take over.
 */
int main(int argc, char *argv[])
{
	GtkWidget *main_vbox, *main_hbox;
	GtkWidget *left_vbox;
	GtkWidget *right_vbox;
	GtkWidget *v_sep, *h_sep;
	GtkWidget *b_box1, *b_box2;
	GtkWidget *msg_scroll;
	GtkWidget *opp_view, *our_view, *opp_hand_view;
	GtkWidget *game_item, *game_menu;
	GtkWidget *help_item, *help_menu;
	GtkWidget *new_item, *select_item, *debug_item;
	GtkWidget *assist_item, *quit_item;
	GtkWidget *about_item;
	GtkWidget *menu_bar;
	GtkTextIter end_iter;
	GtkTextBuffer *message_buffer;
	int i, j;

	/* Set random seed */
	real_game.random_seed = time(NULL);

	/* Parse GTK options */
	gtk_init(&argc, &argv);

	/* Change numeric format to widely portable mode */
	setlocale(LC_NUMERIC, "C");

	/* Bind and set text domain */
	bindtextdomain("bluemoon", LOCALEDIR);
	textdomain("bluemoon");

	/* Always provide translated text in UTF-8 format */
	bind_textdomain_codeset("bluemoon", "UTF-8");

	/* Load card designs */
	read_cards();

	/* Set people */
	human_people = 0;
	ai_people = 1;

	/* Loop over remaining options */
	for (i = 1; i < argc; i++)
	{
		/* Check for people argument */
		if (!strcmp(argv[i], "-1") || !strcmp(argv[i], "-2"))
		{
			/* Loop over people */
			for (j = 0 ; j < MAX_PEOPLE; j++)
			{
				/* Check for match */
				if (!strcasecmp(argv[i + 1], peoples[j].name))
				{
					/* Set people */
					if (argv[i][1] == '1')
					{
						/* Set human people */
						human_people = j;
					}
					else
					{
						/* Set AI people */
						ai_people = j;
					}
				}
			}

			/* Advance argument count */
			i++;
		}

		/* Check for random seed argument */
		if (!strcmp(argv[i], "-r"))
		{
			/* Set random seed */
			real_game.random_seed = atoi(argv[i + 1]);

			/* Advance argument count */
			i++;
		}
	}

	/* Set people pointers */
	set_people();

	/* Create toplevel window */
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	/* Window default size */
	gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);

	/* Set window title */
	gtk_window_set_title(GTK_WINDOW(window), "Blue Moon " VERSION);

	/* Handle main window destruction */
	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(destroy),
	                 NULL);

	/* Create main vbox to hold menu bar, then rest of game area */
	main_vbox = gtk_vbox_new(FALSE, 0);

	/* Create menu bar */
	menu_bar = gtk_menu_bar_new();

	/* Create menu item for 'game' menu */
	game_item = gtk_menu_item_new_with_label(_("Game"));

	/* Add game item to menu bar */
	gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), game_item);

	/* Create menu item for 'help' menu */
	help_item = gtk_menu_item_new_with_label(_("Help"));

	/* Right-justify help menu item */
	gtk_menu_item_set_right_justified(GTK_MENU_ITEM(help_item), TRUE);

	/* Add help item to menu bar */
	gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), help_item);

	/* Create game menu */
	game_menu = gtk_menu_new();

	/* Create game menu items */
	new_item = gtk_menu_item_new_with_label(_("New"));
	select_item = gtk_menu_item_new_with_label(_("Select Matchup..."));
	debug_item = gtk_menu_item_new_with_label(_("Debug..."));
	assist_item = gtk_menu_item_new_with_label(_("Assist..."));
	quit_item = gtk_menu_item_new_with_label(_("Quit"));

	/* Add items to game menu */
	gtk_menu_shell_append(GTK_MENU_SHELL(game_menu), new_item);
	gtk_menu_shell_append(GTK_MENU_SHELL(game_menu), select_item);
	gtk_menu_shell_append(GTK_MENU_SHELL(game_menu), debug_item);
	gtk_menu_shell_append(GTK_MENU_SHELL(game_menu), assist_item);
	gtk_menu_shell_append(GTK_MENU_SHELL(game_menu), quit_item);

	/* Create help menu */
	help_menu = gtk_menu_new();

	/* Create help menu item */
	about_item = gtk_menu_item_new_with_label(_("About..."));

	/* Add item to help menu */
	gtk_menu_shell_append(GTK_MENU_SHELL(help_menu), about_item);
	
	/* Attach events to menu items */
	g_signal_connect(G_OBJECT(new_item), "activate",
	                 G_CALLBACK(new_game), NULL);
	g_signal_connect(G_OBJECT(select_item), "activate",
	                 G_CALLBACK(select_matchup), NULL);
	g_signal_connect(G_OBJECT(debug_item), "activate",
	                 G_CALLBACK(dump_debug), NULL);
	g_signal_connect(G_OBJECT(assist_item), "activate",
	                 G_CALLBACK(assist_action), NULL);
	g_signal_connect(G_OBJECT(quit_item), "activate",
	                 G_CALLBACK(quit_game), NULL);
	g_signal_connect(G_OBJECT(about_item), "activate",
	                 G_CALLBACK(about_dialog), NULL);

	/* Set submenus */
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(game_item), game_menu);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(help_item), help_menu);

	/* Create main hbox to contain status box and game area box */
	main_hbox = gtk_hbox_new(FALSE, 0);

	/* Create left vbox for status information */
	left_vbox = gtk_vbox_new(FALSE, 0);

	/* Create "card view" image */
	full_image = gtk_image_new_from_pixbuf(card_back);

	/* Create separator for status info */
	h_sep = gtk_hseparator_new();

	/* Create opponent status label */
	opp_status = gtk_label_new(_("Dragons: 0  Cards: 0  Power: 0"));

	/* Create opponent status frame */
	opp_frame = gtk_frame_new(real_game.p[!player_us].p_ptr->name);

	/* Add opponent status to opponent frame */
	gtk_container_add(GTK_CONTAINER(opp_frame), opp_status);

	/* Create our status label */
	our_status = gtk_label_new(_("Dragons: 0  Cards: 0  Power: 0"));

	/* Create our status frame */
	our_frame = gtk_frame_new(real_game.p[player_us].p_ptr->name);

	/* Add our status to our frame */
	gtk_container_add(GTK_CONTAINER(our_frame), our_status);

	/* Create buttons */
	retreat_button = gtk_button_new_with_label(_("Retreat"));
	fire_button = gtk_button_new_with_label(_("Announce Fire"));
	earth_button = gtk_button_new_with_label(_("Announce Earth"));
	undo_button = gtk_button_new_with_label(_("Undo Turn"));

	/* Attach events */
	g_signal_connect(G_OBJECT(retreat_button), "clicked",
	                 G_CALLBACK(retreat_clicked), NULL);
	g_signal_connect(G_OBJECT(fire_button), "clicked",
	                 G_CALLBACK(announce_clicked), GINT_TO_POINTER(0));
	g_signal_connect(G_OBJECT(earth_button), "clicked",
	                 G_CALLBACK(announce_clicked), GINT_TO_POINTER(1));
	g_signal_connect(G_OBJECT(undo_button), "clicked",
	                 G_CALLBACK(undo_clicked), NULL);

	/* Create boxes for buttons */
	b_box1 = gtk_hbox_new(FALSE, 0);
	b_box2 = gtk_hbox_new(FALSE, 0);

	/* Pack buttons into box */
	gtk_box_pack_start(GTK_BOX(b_box1), retreat_button, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(b_box1), undo_button, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(b_box2), fire_button, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(b_box2), earth_button, TRUE, TRUE, 0);

	/* Create text view for message area */
	message_view = gtk_text_view_new();

	/* Set text wrapping mode */
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(message_view), GTK_WRAP_WORD);

	/* Make text uneditable */
	gtk_text_view_set_editable(GTK_TEXT_VIEW(message_view), FALSE);

	/* Hide cursor */
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(message_view), FALSE);

	/* Get message buffer */
	message_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(message_view));

	/* Get iterator for end of buffer */
	gtk_text_buffer_get_end_iter(message_buffer, &end_iter);

	/* Get mark at end of buffer */
	message_end = gtk_text_buffer_create_mark(message_buffer, NULL,
	                                          &end_iter, FALSE);

	/* Make scrolled window for message buffer */
	msg_scroll = gtk_scrolled_window_new(NULL, NULL);

	/* Add message buffer to scrolled window */
	gtk_container_add(GTK_CONTAINER(msg_scroll), message_view);

	/* Never scroll horizontally; always vertically */
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(msg_scroll),
	                               GTK_POLICY_NEVER,
	                               GTK_POLICY_ALWAYS);

	/* Pack status info in left vbox */
	gtk_box_pack_start(GTK_BOX(left_vbox), full_image, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(left_vbox), opp_frame, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(left_vbox), b_box1, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(left_vbox), b_box2, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(left_vbox), our_frame, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(left_vbox), msg_scroll, TRUE, TRUE, 0);

	/* Create right vbox for game area */
	right_vbox = gtk_vbox_new(FALSE, 0);

	/* Create area for opponent's played cards */
	opp_area = gtk_fixed_new();

	/* Create horizontal separator for opponent/our areas */
	h_sep = gtk_hseparator_new();

	/* Create area for our played cards */
	our_area = gtk_fixed_new();

	/* Create area for our hand of cards */
	hand_area = gtk_fixed_new();

	/* Create area for opponent's hand of cards */
	opp_hand = gtk_fixed_new();

	/* Have hand area negotiate new size if resized */
	g_signal_connect(G_OBJECT(hand_area), "size-request",
	                 G_CALLBACK(card_area_resized), GINT_TO_POINTER(1));

	/* Have opponent's hand area request new size */
	g_signal_connect(G_OBJECT(opp_hand), "size-request",
	                 G_CALLBACK(card_area_resized), GINT_TO_POINTER(0));

	/* Redraw card area if we are resized */
	g_signal_connect(G_OBJECT(hand_area), "size-allocate",
	                 G_CALLBACK(redraw_everything), NULL);

	/* Request minimal size */
	gtk_widget_set_size_request(our_area, 50, 0);
	gtk_widget_set_size_request(opp_area, 50, 0);

	/* Create viewports for table areas */
	opp_view = gtk_viewport_new(NULL, NULL);
	our_view = gtk_viewport_new(NULL, NULL);

	/* Create viewport for opponent hand */
	opp_hand_view = gtk_viewport_new(NULL, NULL);

	/* Add table areas to viewports */
	gtk_container_add(GTK_CONTAINER(opp_view), opp_area);
	gtk_container_add(GTK_CONTAINER(our_view), our_area);

	/* Add opponent hand to viewport */
	gtk_container_add(GTK_CONTAINER(opp_hand_view), opp_hand);

	/* Pack game area */
	gtk_box_pack_start(GTK_BOX(right_vbox), opp_hand_view, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(right_vbox), opp_view, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(right_vbox), h_sep, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(right_vbox), our_view, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(right_vbox), hand_area, FALSE, TRUE, 0);

	/* Create vertical separator between areas */
	v_sep = gtk_vseparator_new();

	/* Pack vbox's into main hbox */
	gtk_box_pack_start(GTK_BOX(main_hbox), left_vbox, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(main_hbox), v_sep, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(main_hbox), right_vbox, TRUE, TRUE, 0);

	/* Pack menu and main hbox into main vbox */
	gtk_box_pack_start(GTK_BOX(main_vbox), menu_bar, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(main_vbox), main_hbox, TRUE, TRUE, 0);

	/* Add main hbox to main window */
	gtk_container_add(GTK_CONTAINER(window), main_vbox);

	/* Show all widgets */
	gtk_widget_show_all(window);

	/* Let the AI player take their turn and setup for our first turn */
	handle_end_turn();

	/* Begin GTK main loop */
	gtk_main();

	/* Exit */
	return 0;
}
