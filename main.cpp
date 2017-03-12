/**
 * @file main.cpp
 * @brief Bigclock screensaver for xscreensaver
 * @author Martin Becker <becker@mytum.de>
 * @date 2017-03-11
 * SDL1 version (ugly)
 */
#ifdef __cplusplus
    #include <cstdlib>
#else
    #include <stdlib.h>
#endif
#ifdef __APPLE__
    #include <SDL/SDL.h>
#else
    #include <SDL.h>
#endif
#include <SDL_ttf.h>
#include <SDL_syswm.h>
#include <SDL_gfxPrimitives.h>
#include <string>
#include <sstream>
#include <getopt.h>
#include <sys/time.h>
#include <string.h>
#include <string>
#include <X11/Xlib.h>
#include <signal.h>

using namespace std;

int past_m=0; // the previous minute that we have drawn

/*****************
 * config options
 *****************/
struct config_t {
    int ampm; ///< whether to show AM/PM or 24hour format
    int showdate;
    int showdebug;
    int fullscreen;
    
    const char* FONT_CUSTOM_FILE;
    int width;
    int height;
};
static config_t cfg;

/*************
 * CONSTANTS
 *************/
static const int DEFAULT_WIDTH = 800;
static const int DEFAULT_HEIGHT = 600;
static const unsigned int RATE_FAST_MS = 20; ///< when animations happen
static const unsigned int RATE_SLOW_MS = 500; ///< otherwise

/**************
 * SDL stuff
 **************/
static SDL_Surface *screen;
static TTF_Font *fnt_dbg = NULL;
static TTF_Font *fnt_date = NULL;
static TTF_Font *fnt_time = NULL;
static TTF_Font *fnt_mode = NULL;
static SDL_Rect loc_time; ///< rectangle into which hours are drawn
static SDL_Rect loc_date; ///< rectangle into which hours are drawn
static SDL_Rect loc_ampm;

static string _txtdbg;

static int screenWidth = 0;
static int screenHeight = 0;

static const char* FONT_BOLD = "/usr/share/fonts/truetype/droid/DroidSans-Bold.ttf";
static const char* FONT_NORM = "/usr/share/fonts/truetype/droid/DroidSans.ttf";
static const char* FONT_DEBUG = "/usr/share/fonts/truetype/droid/DroidSans.ttf";

// colors
static SDL_Color COLOR_FONT = {176,176,176};
static const Uint32 COLOR_BACKGROUND = 0x00000000;

/**
 * @brief get current clock time
 * @param ms_to_next_minute written by this func
 * @param ltime written by this func
 */
void check_time(struct tm & ltime, Uint32 &ms_to_next_minute) {
    timeval tv;
    gettimeofday(&tv, NULL);    
    localtime_r(&tv.tv_sec, &ltime);
    
    Uint32 seconds_to_next_minute = 60 - ltime.tm_sec;
    ms_to_next_minute = seconds_to_next_minute*1000 - tv.tv_usec/1000;
}

/**
 * @brief determine in in how much seconds we have to draw on the screen again
 */
static Uint32 check_emit(Uint32 interval, void *param) {
    struct tm time_i;
    Uint32 ms_to_next_minute;

    check_time(time_i, ms_to_next_minute);

    if ( time_i.tm_min != past_m) {
        SDL_Event e;
        e.type = SDL_USEREVENT;
        e.user.code = 0;
        e.user.data1 = NULL;
        e.user.data2 = NULL;
        SDL_PushEvent(&e);
        past_m = time_i.tm_min;
    }

    // Don't wake up until the next minute.
    interval = ms_to_next_minute;
    // Make sure interval is positive.
    // Should only matter for leap seconds.
    if ( interval <= 0 ) {
        interval = 500;
    }
    return (interval);
}

/**
 * @brief load font & calculate once the coordinates of everything we draw later
 */
static int init_resources(void) {
    try {
        screen = SDL_SetVideoMode(0,0,32, SDL_FULLSCREEN);
        screenHeight = screen->h;
        screenWidth = screen->w;
        //printf("Full Screen Resolution : %dx%d\n", screenWidth, screenHeight);
        if (cfg.fullscreen) {
            cfg.height = screenHeight;
            cfg.width = screenWidth;
        }
        if (strcmp("", cfg.FONT_CUSTOM_FILE) == 0) {
            fnt_time = TTF_OpenFont(FONT_BOLD, cfg.height / 2);
            fnt_mode = TTF_OpenFont(FONT_BOLD, cfg.height/ 15);
            fnt_date = TTF_OpenFont(FONT_NORM, cfg.height/ 15);
        } else {
            fnt_time = TTF_OpenFont(cfg.FONT_CUSTOM_FILE, cfg.height / 2);
            fnt_mode = TTF_OpenFont(cfg.FONT_CUSTOM_FILE, cfg.height / 15);
            fnt_date = TTF_OpenFont(cfg.FONT_CUSTOM_FILE, cfg.height / 15);
        }
        
        if (cfg.showdebug) {
            fnt_dbg = TTF_OpenFont(FONT_DEBUG, 12);
            if (!fnt_dbg) printf("Error loading debug font\n");
        }
        stringstream ss; ss << "Resolution: " << screenWidth << "x" << screenHeight << ", size=" << cfg.width << "x" << cfg.height;
        _txtdbg = ss.str();

        if (!screen)
            throw 2;
        if (!fnt_time || !fnt_date || !fnt_mode)
            throw 1;
    } catch (int param) {
        if (param == 1)
            printf("TTF_OpenFont: %s\n", TTF_GetError());
        else if (param == 2)
            printf("Couldn't initialize video mode to check native, fullscreen resolution.\n");

        return param;
    }

    /* CALCULATE BACKGROUND COORDINATES */
    // to find out correct positioning, render dummy text
    const char*dummy="23:23";
    int w, h;
    TTF_SizeText(fnt_time, dummy, &w, &h);
    int dscent = abs(TTF_FontDescent(fnt_time)); // we want to subtract this space, as numbers never go below baseline
    loc_time.w = w;
    loc_time.h = h;
    loc_time.x = 0.5 * cfg.width - 0.5*loc_time.w;
    if (cfg.showdate) {
        loc_time.y = 0.47 * cfg.height - 0.5*loc_time.h;
    } else {
        loc_time.y = 0.5 * cfg.height - 0.5*loc_time.h;
    }
    //printf("loc_time=%d,%d,%d,%d\n", loc_time.x, loc_time.y, loc_time.w, loc_time.h);

    // am/pm mark
    loc_ampm.x = loc_time.x + loc_time.w;
    loc_ampm.y = loc_time.y + 0.85*dscent; // guesswork to align am/pm to upper pixel of time
    const char*dummy2="AM";    
    TTF_SizeText (fnt_mode, dummy2, &w, &h);
    loc_ampm.w = w;
    loc_ampm.h = h;
    //printf("loc_ampm=%d,%d,%d,%d\n", loc_ampm.x, loc_ampm.y, loc_ampm.w, loc_ampm.h);
    
    // date    
    loc_date.h = TTF_FontHeight(fnt_date);
    loc_date.x = loc_time.x;
    loc_date.w = loc_time.w;
    int padding = 0.3*TTF_FontHeight(fnt_date);
    loc_date.y = loc_time.y + loc_time.h - dscent + padding;

    SDL_AddTimer(500, check_emit, NULL); ///< call check_emit regularly
    return 0;
}

static SDL_Rect getCoordinates(SDL_Rect * background, SDL_Surface * foreground) {
    SDL_Rect cord;
    int dx = (background->w - foreground->w) * 0.5;
    cord.x = background->x + dx;
    int dy = (background->h - foreground->h)  * 0.5;
    cord.y = background->y + dy;
    return cord;
}

static void draw_debug(SDL_Surface * surface) {
    if (!fnt_dbg || _txtdbg.empty()) return;
    SDL_Surface *text = TTF_RenderText_Solid(fnt_dbg, _txtdbg.c_str(), COLOR_FONT);
    SDL_BlitSurface(text, 0, screen, NULL);
    SDL_FreeSurface(text);
}

/**
 * @brief draw a line through middle of time to make it look nicer
 */
static void draw_divider(SDL_Surface * surface) {
    SDL_Rect line;
    line.h = cfg.height * 0.0051;
    line.w = cfg.width;
    line.x = 0;
    line.y = loc_time.y + 0.48*loc_time.h; // not centered on purpose (looks better)
    SDL_FillRect(surface, &line, SDL_MapRGBA(surface->format, 0,0,0,0));
}

/**
 * @brief draw the little AM/PM box
 */
static void draw_AMPM(SDL_Surface * surface, const struct tm & _time) {

    char mode[3];
    strftime(mode, sizeof(mode), "%p", &_time);
    SDL_Surface *AMPM = TTF_RenderText_Blended(fnt_mode, (const char *)mode, COLOR_FONT);
    SDL_BlitSurface(AMPM, 0, surface, &loc_ampm);
    SDL_FreeSurface(AMPM);
}

static void draw_time(SDL_Surface *surface, const struct tm & _time) {
    try {
        
        SDL_Rect coordinates;
        char st[10];
        if (cfg.ampm) {
            strftime(st, sizeof(st), "%I:%M", &_time);
        } else {
            strftime(st, sizeof(st), "%H:%M", &_time);
        }
        
        SDL_Surface *text = TTF_RenderText_Blended(fnt_time, (const char *) st, COLOR_FONT);
        coordinates = getCoordinates(&loc_time, text);
        SDL_BlitSurface(text, 0, screen, &coordinates);
        SDL_FreeSurface(text);        
    } catch (...) {
        printf ("Problem drawing time");
    }
}

static void draw_date(SDL_Surface *surface, const struct tm & _time) {
    try {
        char datestr[255];
        strftime(datestr, sizeof(datestr), "%A, %d %B %Y", &_time);
        SDL_Rect coordinates;
        SDL_Surface *text = TTF_RenderText_Blended(fnt_date, datestr, COLOR_FONT);
        coordinates = getCoordinates(&loc_date, text);
        SDL_BlitSurface(text, 0, screen, &coordinates);
        SDL_FreeSurface(text);
    } catch (...) {
        printf ("Problem drawing date");
    }
}


/**
 * @brief draw the entire screen
 */
static void redraw(void) {
    // background
    SDL_FillRect (screen, 0, COLOR_BACKGROUND);//SDL_MapRGB(screen->format, 0, 0, 0));

    // time
    struct tm ltime;
    Uint32 ignored;
    check_time (ltime, ignored);    
    draw_time (screen, ltime);

    // AM/PM stuff
    if (cfg.ampm) draw_AMPM (screen, ltime);    
    if (cfg.showdate) draw_date (screen, ltime);

    
    draw_divider (screen);
    if (cfg.showdebug) draw_debug (screen);
    SDL_Flip (screen);
}

void exit_immediately(int sig) {
    exit(0);
}

static void print_usage(void) {
    printf("Usage: [OPTION...]\n");
    printf("Options:\n");
    printf(" --help\t\t\t\tDisplay this\n");
    printf(" --date\t\t\t\tShow also date not only time\n");
    printf(" -root,--fullscreen,--root\tFullscreen\n");
    printf(" -ampm, --ampm\t\t\tTurn off 24 h system and use 12 h system instead\n");
    printf(" -w\t\t\t\tCustom Width\n");
    printf(" -h\t\t\t\tCustom Height\n");
    printf(" -f, --font\t\t\tPath to custom file font. Has to be Truetype font.\n");
}

static void SDL_cleanup() {
    SDL_FreeSurface(screen);
    TTF_CloseFont(fnt_time);
    TTF_CloseFont(fnt_mode);
    TTF_CloseFont(fnt_dbg);
    TTF_CloseFont(fnt_date);
    TTF_Quit();
    SDL_Quit();
}

static int parse_args(int argc, char** argv) {
    // defaults
    cfg.FONT_CUSTOM_FILE = "";

    const char *const short_options = "radd?w:h:f"; /* A string listing valid short options letters.  */
    /* An array describing valid long options.  */
    const struct option long_options[] = {
        // following options set a flag:
        {"root",       0, &cfg.fullscreen, 1},
        {"ampm",       0, &cfg.ampm, 1},
        {"date",       0, &cfg.showdate, 1},
        {"debug",      0, &cfg.showdebug, 1},
        // remaining do not set a flag
        {"help",       0, NULL, '?'},
        {"width",      1, NULL, 'w'},
        {"height",     1, NULL, 'h'},
        {"font",       1, NULL, 'f'},
        {NULL,         0, NULL, 0}  /* Required at end of array.  */
    };

    int next_option;
    int customwidth = 0, customheight=0;
    while ((next_option = getopt_long_only(argc, argv, short_options, long_options, NULL)) != -1) {
        switch (next_option) {
        case '?':
            print_usage();
            exit(0);
            break;
        case 'w':
            customwidth = atoi(optarg);
            //printf ("Width : %d\n", customwidth);
            break;
        case 'h':
            customheight = atoi(optarg);
            //printf ("Height: %d\n", customheight);
            break;
        case 'f':
            cfg.FONT_CUSTOM_FILE = optarg; // FIXME: is this ptr opaque to argv?
            //printf("Custom font:%s", cfg.FONT_CUSTOM_FILE);
            break;
        default:
            //printf("unknown option: %c ignored\n", next_option);
            break;
        }
    }
            
    if (customwidth > 0) {
        cfg.width = customwidth;
    } else {
        cfg.width = DEFAULT_WIDTH;
    }
    if (customheight > 0) {
        cfg.height = customheight;
    } else {
        cfg.height = DEFAULT_HEIGHT;
    }
    return 0;
}

int main (int argc, char** argv) {
    signal(SIGINT, exit_immediately);
    signal(SIGTERM, exit_immediately);

    char *wid_env; 
    static char sdlwid[100];
    Uint32 wid = 0; 
    Display *display; 
    XWindowAttributes windowAttributes;
    windowAttributes.height = 0;
    windowAttributes.width = 0;

    // Note that Xscreensaver *always* spawns one screenserver per monitor. There is no way around this.
    // we could have date on one and time on the other, but how do we find out which one we are?
    
    /* If no window argument, check environment */
    if (wid == 0) {
        if ((wid_env = getenv("XSCREENSAVER_WINDOW")) != NULL ) {
            wid = strtol(wid_env, (char **) NULL, 0); /* Base 0 autodetects hex/dec */
            printf("window from env");
        }
    }

    /* Get win attrs if we've been given a window, otherwise we'll use our own */
    if (wid != 0 ) {
        if ((display = XOpenDisplay(NULL)) != NULL) { /* Use the default display */
            printf("window from default display");
            XGetWindowAttributes(display, (Window) wid, &windowAttributes);
            XCloseDisplay(display);
            snprintf(sdlwid, 100, "SDL_WINDOWID=0x%X", wid);
            putenv(sdlwid); /* Tell SDL to use this window */
        }
    }

    parse_args(argc, argv);    

    /************
     * START SDL
     ***********/
    if ( SDL_Init( SDL_INIT_VIDEO|SDL_INIT_TIMER ) < 0 ) {
        printf( "Unable to init SDL: %s\n", SDL_GetError() );
        return 2;
    }    
    TTF_Init();
    init_resources();
    SDL_ShowCursor(SDL_DISABLE);
    atexit(SDL_Quit);
    atexit(TTF_Quit);

    try {
        if(cfg.fullscreen) {
            screen = SDL_SetVideoMode(windowAttributes.width, windowAttributes.height,32,SDL_HWSURFACE|SDL_DOUBLEBUF|SDL_FULLSCREEN);
        } else {
            screen = SDL_SetVideoMode(cfg.width, cfg.height, 32,SDL_HWSURFACE|SDL_DOUBLEBUF);
        }
        if (!screen) {
            throw 2;
        }
    } catch (int param) {
        if (param == 2)
            printf("Unable to set video mode: %s\n", SDL_GetError());
        return 2;
    }

    /*******************
     * MAIN LOOP
     *******************/
    redraw(); ///< initial draw
    bool done = false;
    while (!done) {
        SDL_Event event;
        while(SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_USEREVENT:
                redraw(); ///< only draw on our user event (that happens when minute changes)
                break;
            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE) done = true;
                break;
            case SDL_QUIT:
                done = true;
                break;
            }
        }
        if (!done) {
            Uint32 ms_to_next_minute;
            struct tm time_i;
            check_time(time_i, ms_to_next_minute);

            // adaptive frame rate
            if(ms_to_next_minute > RATE_SLOW_MS) {
                SDL_Delay(RATE_SLOW_MS); // still fast enough to react to events
            } else {
                SDL_Delay(RATE_FAST_MS); // faster for smooth animations
            }
        }
    }

    // cleanup & exit
    SDL_cleanup();
    return 0;
}
