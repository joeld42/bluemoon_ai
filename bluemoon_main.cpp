#include "raylib.h"
#include "raymath.h"

extern "C" 
{
#include "bluemoon.h"
}

#include <time.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

/* NOTES
message_add -- Adds a message to the console log
clear_log -- clear the console log
redraw_full -- redraw the full-size card image when hover (mouse pos) changes
save_state -- save game state into backup
card_retreived -- ???
card_revealed -- reveal bluff card
set_buttons -- calculate if fire/earth/retreat buttons valid and set their active states
play_card -- play a card and advance the turn phase
card_used -- use a card's special power
card_satisfied -- ??
card_legal -- test if card is legal to play
play_item -- ??
bluff_item -- play a card as a bluff ?
load_item -- load a card onto a ship ?
popup_context -- make context menu for card with differnt ways to play it (bluff, ship, etc)
new_image_box -- make card image, might be card back or greyscale filtered

*/

Color Background = { 21, 32, 47 };

/*
 * Base filenames for people's card images.
 */
static const char *image_base[] =
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

Texture2D image_cache[10][128]; // [people][index]

typedef struct BlueMoonStruct {
    game real_game; //Current (real) game state.
    
    game backup; // Back-up game to restore from when undo-ing.
    int backup_set;

    int player_us; //Player we're playing as.
    
    int human_people; // People indices for both players.
    int ai_people;

} BlueMoon; 

/*
 * AI verbosity.
 * (refered to in ai.c)
 */
int verbose = 0;

Texture2D card_back;

extern "C" {
void message_add(char *msg)
{
    printf("TODO: Message_add '%s'\n", msg );
}

} // extern C


void process_args( BlueMoon *bmgame, int argc, char **argv)
{
    /* Set people */
    bmgame->human_people = 0;
    bmgame->ai_people = 1;

    /* Loop over remaining options */
    for (int i = 1; i < argc; i++)
    {
        /* Check for people argument */
        if (!strcmp(argv[i], "-1") || !strcmp(argv[i], "-2"))
        {
            /* Loop over people */
            for (int j = 0 ; j < MAX_PEOPLE; j++)
            {
                /* Check for match */
                if (!strcasecmp(argv[i + 1], peoples[j].name))
                {
                    /* Set people */
                    if (argv[i][1] == '1')
                    {
                        /* Set human people */
                        bmgame->human_people = j;
                    }
                    else
                    {
                        /* Set AI people */
                        bmgame->ai_people = j;
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
            bmgame->real_game.random_seed = atoi(argv[i + 1]);

            /* Advance argument count */
            i++;
        }
    }
}

static void gui_choose(game *g, int chooser, int who, design **choices,
                       int num_choices, int min, int max,
                       choose_result callback, void *data, char *prompt)
{
    printf("NOT YET IMPLEMENTED: GUI_CHOOSE\n");
}

static int gui_call_bluff(struct game *g)
{
    printf("NOT YET IMPLEMENTED: GUI_CALL_BLUFF\n");
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

bool image_loaded( Texture2D &tex ) 
{
    return (tex.id != 0);
}

void load_images(BlueMoon *bmgame )
{
    int i, j;
    int x, y;
    char fn[1024];
    design *d_ptr;

    /* Check for card back image already loaded */
    if (card_back.id==0)
    {
        /* Load card back image */
        card_back = LoadTexture( DATADIR "/image/cardback.png" );
        printf("Loaded card back %dx%d\n", card_back.width, card_back.height );
    }

//    return;

    /* Loop over peoples */
    for (i = 0; i < 2; i++)
    {
        /* Loop over cards in deck */
        for (j = 0; j < DECK_SIZE; j++)
        {
            /* Get design pointer */
            d_ptr = &bmgame->real_game.p[i].p_ptr->deck[j];

            /* Get design's base people and index */
            x = d_ptr->people;
            y = d_ptr->index;
#if 1
            /* Check for already loaded image */
            if ( image_loaded( image_cache[x][y]) )  continue;

            /* Construct image filename */
            sprintf(fn, DATADIR "/image/%s%02d.png",
                        image_base[x], y);

            /* Load image */
            //image_cache[x][y] = gdk_pixbuf_new_from_file(fn, NULL);
            image_cache[x][y] = LoadTexture( fn );
            printf("Loaded card image %s %dx%d\n", fn,
                image_cache[x][y].width, image_cache[x][y].height );

            /* Check for error */
            if (!image_loaded(image_cache[x][y]))
            {
                /* Print error */
                printf(_("Cannot open image %s!\n"), fn);
            }
#endif
        }
    }
}


/*
 * Set people pointers in player structures.
 *
 * If necessary, reverse them so that lower index people is at player 0.
 */
static void set_people( BlueMoon *bmgame )
{
    /* Check for human player having later people */
    if (bmgame->human_people > bmgame->ai_people)
    {
        /* Set player peoples */
        bmgame->real_game.p[0].p_ptr = &peoples[bmgame->ai_people];
        bmgame->real_game.p[1].p_ptr = &peoples[bmgame->human_people];

        /* Human is second player */
        bmgame->player_us = 1;
    }
    else
    {
        /* Set player peoples */
        bmgame->real_game.p[0].p_ptr = &peoples[bmgame->human_people];
        bmgame->real_game.p[1].p_ptr = &peoples[bmgame->ai_people];

        /* Human is first player */
        bmgame->player_us = 0;
    }

    /* Load card images */
    load_images( bmgame );

    /* Initialize game */
    init_game(&bmgame->real_game, 1);

    /* Set opponent interface */
    bmgame->real_game.p[!bmgame->player_us].control = &ai_func;

    /* Call opponent initialization */
    bmgame->real_game.p[!bmgame->player_us].control->init(&bmgame->real_game, !bmgame->player_us);

    /* Set our interface to AI functions (briefly) */
    bmgame->real_game.p[bmgame->player_us].control = &ai_func;

    /* Initialize AI for our side */
    bmgame->real_game.p[bmgame->player_us].control->init(&bmgame->real_game, bmgame->player_us);

    /* Set our interface to GUI */
    bmgame->real_game.p[bmgame->player_us].control = &gui_func;
}

/*
 * After our turn, let the AI take actions until it is our turn again,
 * then refresh the drawing areas so that we can take our next turn.
 */
static void handle_end_turn( BlueMoon *bmgame )
{
    /* Have AI take actions until it is our turn */
    while (bmgame->real_game.turn != bmgame->player_us && !bmgame->real_game.game_over)
    {
        /* Request action from AI */
        bmgame->real_game.p[bmgame->real_game.turn].control->take_action(&bmgame->real_game);

        printf("TODO: feedback for action...\n");
    }

    /* No undo available */
    bmgame->backup_set = 0;

    // /* Set "retreat" button */
    // gtk_widget_set_sensitive(retreat_button, !real_game.game_over);

    /* Start turn */
    start_turn(&bmgame->real_game);

    /* Advance to beginning of turn phase */
    bmgame->real_game.p[bmgame->player_us].phase = PHASE_BEGIN;    
    printf("Your turn...\n");
}

// ======================================================================
// ======================================================================

void DrawCard( Texture2D &tex, int posx, int posy ) 
{    
    //DrawTexture( card_back, posx, posy, (Color)WHITE );

    // DrawTexturePro(Texture2D texture, Rectangle sourceRec, 
    //                 Rectangle destRec, Vector2 origin,
    //                     float rotation, Color tint);
    //230x409
    float cardAspect = 230.f / 409.f;
    int cardSz[2] = { (int)(200 * cardAspect), 200 };
    DrawTexturePro( tex, (Rectangle){ 0, 0, 230, 409 },
                        (Rectangle){ posx, posy, cardSz[0], cardSz[1] }, 
                        (Vector2){ cardSz[0]/2, cardSz[1]/2}, 0.0, (Color)WHITE );
}

int main( int argc, char **argv )
{
	
	//--------------------------------------------------------------------------------------
    int screenWidth = 1280;
    int screenHeight = 720;

    InitWindow(screenWidth, screenHeight, "Blue Moon");    
    SetTargetFPS(30);

    // Game setup -------
    BlueMoon _bmgame = {0};
    BlueMoon *bmgame;

    /* Set random seed */       
    bmgame->real_game.random_seed = time(NULL);

    /* Change numeric format to widely portable mode */
    setlocale(LC_NUMERIC, "C");

    /* Bind and set text domain */
    //bindtextdomain("bluemoon", LOCALEDIR);
    //textdomain("bluemoon");

    /* Always provide translated text in UTF-8 format */
    //bind_textdomain_codeset("bluemoon", "UTF-8");

    /* Load card designs */
    read_cards();

    /* set up args */
    process_args( bmgame, argc, argv );

    /* Set people pointers */
    set_people( bmgame );

    Camera camera;
    camera.position = (Vector3){ 0.0f, 10.0f, 10.0f };  // Camera position
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };      // Camera looking at point
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };          // Camera up vector (rotation towards target)
    camera.fovy = 45.0f;                                // Camera field-of-view Y


    handle_end_turn( bmgame );

    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        float frameDt = 1.0f / 30.0f;

 		BeginDrawing();
 		ClearBackground( Background );

        // Dunno if will use 3d mode or not
        BeginMode3D(camera);
        DrawGrid(10, 1.0f);
        EndMode3D();

        //DrawTexture( card_back, 20, 20, (Color)WHITE );
        DrawCard( card_back, 20, 20 );

        // Vector2 cardPos = { 350, 20 };
        // DrawTextureEx( card_back, cardPos, 15, 0.8f, (Color)WHITE );

        for (int person=0; person < 2; person++)
        {
            int xx = 50;
            for (int c=0; c < DECK_SIZE; c++) {
                if (image_loaded(image_cache[person][c])) {
                    // DrawTexture( image_cache[0][c], xx, 30, (Color)WHITE );
                    DrawCard( image_cache[person][c], xx, 100 + (person*300) );

                    xx += image_cache[person][c].width;
                }
            }
        }

		EndDrawing();
    }

     CloseWindow();    
}