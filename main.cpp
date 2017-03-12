/**
 * @file main.cpp
 * @brief Bigclock screensaver for xscreensaver
 * @author Martin Becker <becker@mytum.de>
 * @date 2017-03-11
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
#include <SDL2_gfxPrimitives.h>
#include <string>
#include <sstream>
#include <getopt.h>
#include <sys/time.h>
#include <string.h>
#include <string>
#include <X11/Xlib.h>
#include <signal.h>

using namespace std;

static int past_m=0; // the previous minute that we have drawn

#define USE_COLOR_FG  SDL_SetRenderDrawColor(renderer, COLOR_FONT.r, COLOR_FONT.g, COLOR_FONT.b, 255)
#define USE_COLOR_BG  SDL_SetRenderDrawColor(renderer, COLOR_BACKGROUND.r, COLOR_BACKGROUND.g, COLOR_BACKGROUND.b, 255)

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
static const int DEFAULT_WIDTH = 1920; // will be scaled to actual screen size
static const int DEFAULT_HEIGHT = 1080; // will be scaled to actual screen size
static const unsigned int RATE_FAST_MS = 20; ///< when animations happen
static const unsigned int RATE_SLOW_MS = 500; ///< otherwise

/**************
 * SDL stuff
 **************/
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
//SDL_Surface *screen = NULL;;
static TTF_Font *fnt_dbg = NULL;
static TTF_Font *fnt_date = NULL;
static TTF_Font *fnt_time = NULL;
static TTF_Font *fnt_ampm = NULL;
static SDL_Rect loc_time; ///< rectangle into which time is drawn
static SDL_Rect loc_date; ///< rectangle into which date is drawn
static SDL_Rect loc_ampm;
static SDL_Rect loc_dbg;

static string _txtdbg;

static const char* FONT_BOLD = "/usr/share/fonts/truetype/droid/DroidSans-Bold.ttf";
static const char* FONT_NORM = "/usr/share/fonts/truetype/droid/DroidSans.ttf";

// colors
static SDL_Color COLOR_FONT = {176,176,176};
static SDL_Color COLOR_BACKGROUND = {0,0,0};

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
        // fonts
        if (strcmp("", cfg.FONT_CUSTOM_FILE) == 0) {
            fnt_time = TTF_OpenFont(FONT_BOLD, cfg.height / 2);
            fnt_ampm = TTF_OpenFont(FONT_BOLD, cfg.height/ 15);
            fnt_date = TTF_OpenFont(FONT_NORM, cfg.height/ 15);
        } else {
            fnt_time = TTF_OpenFont(cfg.FONT_CUSTOM_FILE, cfg.height / 2);
            fnt_ampm = TTF_OpenFont(cfg.FONT_CUSTOM_FILE, cfg.height / 15);
            fnt_date = TTF_OpenFont(cfg.FONT_CUSTOM_FILE, cfg.height / 15);
        }
        if (!fnt_time || !fnt_date || !fnt_ampm) throw 1;
        
        // debug output
        if (cfg.showdebug) {
            fnt_dbg = TTF_OpenFont(FONT_NORM, 12);
            if (!fnt_dbg) throw 1;
        }
        stringstream ss; ss << "Resolution: " << cfg.width << "x" << cfg.height;
        _txtdbg = ss.str();

    } catch (int param) {
        if (param == 1)
            printf("TTF_OpenFont: %s\n", TTF_GetError());
        else if (param == 2)
            printf("Couldn't initialize video mode to check native, fullscreen resolution.\n");
        return param;
    }

    /******************************
     * LOCATIONS OF CLOCK ELEMENTS
     ******************************/
    const char*dummy="23:23";
    TTF_SizeText (fnt_time, dummy, &loc_time.w, &loc_time.h);
    int dscent = abs(TTF_FontDescent(fnt_time)); // we want to subtract this space, as numbers never go below baseline

    // time offset
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
    TTF_SizeText (fnt_ampm, dummy2, &loc_ampm.w, &loc_ampm.h);
    //printf("loc_ampm=%d,%d,%d,%d\n", loc_ampm.x, loc_ampm.y, loc_ampm.w, loc_ampm.h);
    
    // date
    loc_date.h = TTF_FontHeight(fnt_date);
    loc_date.x = loc_time.x;
    loc_date.w = loc_time.w;
    int padding = 0.3*TTF_FontHeight(fnt_date);
    loc_date.y = loc_time.y + loc_time.h - dscent + padding;

    // debug
    if (cfg.showdebug) {
        loc_dbg.w = cfg.width;
        loc_dbg.h = TTF_FontHeight(fnt_dbg);
        loc_dbg.x = 0;
        loc_dbg.y = 0;
    }
    
    SDL_AddTimer(500, check_emit, NULL); ///< call check_emit regularly
    return 0;
}

/**
 * @return rect for SDL_RenderCopy, such that fg is centered in bg.
 */
static SDL_Rect align_center (const SDL_Rect & bg, const SDL_Surface * const fg) {
    SDL_Rect ret;

    // squeeze if necessary
    int objw = (fg->w < bg.w) ? fg->w : bg.w;
    int objh = (fg->h < bg.h) ? fg->h : bg.h;
    
    int dx = (bg.w - objw) * 0.5;
    ret.x = bg.x + dx;
    int dy = (bg.h - objh)  * 0.5;
    ret.y = bg.y + dy;
    ret.w = objw;
    ret.h = objh;
    return ret;    
}

/**
 * @return rect for SDL_RenderCopy, such that fg is flush left in bg.
 */
static SDL_Rect align_left (const SDL_Rect & bg, const SDL_Surface * const fg) {
    SDL_Rect ret;

    // squeeze if necessary
    int objw = (fg->w < bg.w) ? fg->w : bg.w;
    int objh = (fg->h < bg.h) ? fg->h : bg.h;    
    ret.x = bg.x;
    ret.y = bg.y;
    ret.w = objw;
    ret.h = objh;
    return ret;    
}

static void draw_debug() {
    if (!fnt_dbg || _txtdbg.empty()) return;
    SDL_Surface *surf = TTF_RenderText_Solid(fnt_dbg, _txtdbg.c_str(), COLOR_FONT);
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_Rect coord = align_left(loc_dbg, surf);
    SDL_RenderCopy(renderer, texture, NULL, &coord);
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surf);
}

/**
 * @brief draw a line through middle of time to make it look nicer
 */
static void draw_divider(void) {
    SDL_Rect line;
    line.h = cfg.height * 0.005;
    line.w = cfg.width;
    line.x = 0;
    line.y = loc_time.y + 0.475*loc_time.h; // not centered on purpose (looks better)

    USE_COLOR_BG;
    SDL_RenderFillRect(renderer, &line);
}

/**
 * @brief draw the little AM/PM box
 */
static void draw_AMPM(const struct tm & _time) {
    char mode[3];
    strftime(mode, sizeof(mode), "%p", &_time);
    SDL_Surface *surf = TTF_RenderText_Blended(fnt_ampm, (const char *)mode, COLOR_FONT);
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surf);
    if (cfg.showdebug) SDL_RenderDrawRect(renderer, &loc_ampm);
    SDL_RenderCopy(renderer, texture, NULL, &loc_ampm);
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surf);
}

static void draw_time(const struct tm & _time) {
    try {        
        char st[10];
        if (cfg.ampm) {
            strftime(st, sizeof(st), "%I:%M", &_time);
        } else {
            strftime(st, sizeof(st), "%H:%M", &_time);
        }
        
        SDL_Surface *surf = TTF_RenderText_Blended(fnt_time, (const char *) st, COLOR_FONT);
        SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surf);
        if (cfg.showdebug) {
            SDL_SetRenderDrawColor(renderer, 255,255,0,255);
            SDL_RenderDrawRect(renderer, &loc_time);
        }
        if (!texture) return;
        SDL_RenderCopy(renderer, texture, NULL, &loc_time);
        SDL_FreeSurface(surf);
        SDL_DestroyTexture(texture);
    } catch (...) {
        printf ("Problem drawing time");
    }
}

static void draw_date(const struct tm & _time) {
    try {
        char datestr[255];
        strftime(datestr, sizeof(datestr), "%A, %d %B %Y", &_time);
        SDL_Surface *surf = TTF_RenderText_Blended(fnt_date, datestr, COLOR_FONT);
        SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surf);               
        if (cfg.showdebug) {
            SDL_SetRenderDrawColor(renderer, 255,255,0,255);
            SDL_RenderDrawRect(renderer, &loc_date);
        }
        if (!texture) return;
        SDL_Rect coordinates;
        coordinates = align_center(loc_date, surf);
        SDL_RenderCopy(renderer, texture, NULL, &coordinates);
        SDL_FreeSurface(surf);
        SDL_DestroyTexture(texture);
    } catch (...) {
        printf ("Problem drawing date");
    }
}


/**
 * @brief draw the entire screen
 */
static void redraw(void) {
    // background
    USE_COLOR_BG;
    SDL_RenderClear(renderer);

    // time
    struct tm ltime;
    Uint32 ignored;
    check_time (ltime, ignored);    
    draw_time (ltime);

    // AM/PM stuff
    if (cfg.ampm) draw_AMPM (ltime);    
    if (cfg.showdate) draw_date (ltime);

    
    draw_divider ();
    if (cfg.showdebug) draw_debug ();
    SDL_RenderPresent(renderer);
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

static int initialize_SDL (void) {
    /****************
     * CHECK XWINDOW
     ****************/
    char *wid_env; 
    Uint32 wid = 0; 
    Display *display;
    Window nativewin;
    XWindowAttributes windowAttributes;
    windowAttributes.height = 0;
    windowAttributes.width = 0;

    // Note that Xscreensaver *always* spawns one screenserver per monitor. There is no way around this.
    // we have to use the window that XScreensaver gives to us.
    bool window_predefined = false;
    if ((wid_env = getenv("XSCREENSAVER_WINDOW")) != NULL ) {
        wid = strtol(wid_env, (char **) NULL, 0); /* Base 0 autodetects hex/dec */
        /* Get win attrs if we've been given a window, otherwise we'll use our own */
        if ((display = XOpenDisplay(NULL)) != NULL) { /* Use the default display */
            nativewin = (Window) wid;
            XGetWindowAttributes(display, nativewin, &windowAttributes);
            XCloseDisplay(display);
            #ifdef SDL1_2
            static char sdlwid[100];
            snprintf(sdlwid, 100, "SDL_WINDOWID=0x%X", wid);
            putenv(sdlwid); /* Tell SDL to use this window */
            #endif
            window_predefined = false;
        }
    }

    /*************
     * SETUP SDL
     ************/
    if ( SDL_Init( SDL_INIT_VIDEO|SDL_INIT_TIMER ) < 0 ) {
        printf( "Unable to init SDL: %s\n", SDL_GetError() );
        return 2;
    }
    
    TTF_Init();
    if (!TTF_WasInit()) {
        printf("TTF could not be initialized!");
        exit (44);
    }
    SDL_ShowCursor(SDL_DISABLE);
    atexit(SDL_Quit);
    atexit(TTF_Quit);

    // create window and renderer
    try {
        if (window_predefined) {
            // when running as screensaver, we are here
            window = SDL_CreateWindowFrom((void*)nativewin);
            if (!window) exit(99);;
            cfg.height = windowAttributes.height;
            cfg.width = windowAttributes.width;
            
        } else {
            if(!cfg.fullscreen) {
                window = SDL_CreateWindow("BigClock", 0, 0, cfg.width, cfg.height, SDL_WINDOW_SHOWN);
            
            } else {
                window = SDL_CreateWindow("BigClock", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, cfg.width, cfg.height, SDL_WINDOW_OPENGL|SDL_WINDOW_FULLSCREEN_DESKTOP);
            }
        }
        if (window == NULL) {
            printf("SDL_CreateWindow Error: %s", SDL_GetError());
            return 1;
        }
        // write-back actual size of window
        //cfg.height = screenHeight;
        //cfg.width = screenWidth;

        // renderer
        renderer = SDL_CreateRenderer (window, -1, SDL_RENDERER_ACCELERATED);
        if (!renderer) {
            throw 3;
        }
        USE_COLOR_FG;
        SDL_RenderClear(renderer);

        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");  // make the scaled rendering look smoother.
        SDL_RenderSetLogicalSize(renderer, cfg.width, cfg.height);       
        
    } catch (int param) {
        if (param == 3) printf("Unable to init renderer: %s\n", SDL_GetError());
        return 2;
    }
    return 0;
}

static void cleanup_SDL(void) {
    TTF_CloseFont(fnt_time);
    TTF_CloseFont(fnt_ampm);
    TTF_CloseFont(fnt_dbg);
    TTF_CloseFont(fnt_date);
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();;
}

int main (int argc, char** argv) {
    parse_args(argc, argv);
    
    signal(SIGINT, exit_immediately);
    signal(SIGTERM, exit_immediately);    
        
    if (initialize_SDL()) exit(1);
    if (init_resources()) exit(2);
    
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

    cleanup_SDL();
    return 0;
}
