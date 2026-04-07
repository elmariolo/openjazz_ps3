
/**
 *
 * @file video.cpp
 *
 * Part of the OpenJazz project
 *
 * @par History:
 * - 23rd August 2005: Created main.c
 * - 22nd July 2008: Created util.c from parts of main.c
 * - 3rd February 2009: Renamed util.c to util.cpp
 * - 13th July 2009: Created graphics.cpp from parts of util.cpp
 * - 26th July 2009: Renamed graphics.cpp to video.cpp
 *
 * @par Licence:
 * Copyright (c) 2005-2017 AJ Thomson
 * Copyright (c) 2015-2026 Carsten Teibes
 *
 * OpenJazz is distributed under the terms of
 * the GNU General Public License, version 2.0
 *
 * @par Description:
 * Contains graphics utility functions.
 *
 */


#include "paletteeffects.h"
#include "video.h"

#ifdef SCALE
	#include <scalebit.h>
#endif

#include "setup.h"
#include "util.h"
#include "io/log.h"

#include <string.h>

#include <sysutil/video.h>
#include <unistd.h>

/**
 * Creates a surface.
 *
 * @param pixels Pixel data to copy into the surface. Can be nullptr.
 * @param width Width of the pixel data and of the surface to be created
 * @param height Height of the pixel data and of the surface to be created
 *
 * @return The completed surface
 */
SDL_Surface* Video::createSurface (const unsigned char * pixels, int width, int height) {

	// Create the surface
	SDL_Surface *ret = nullptr;
	ret = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, 8, 0, 0, 0, 0);

	if(!ret) {
		LOG_FATAL("Could not create surface: %s", SDL_GetError());
		return nullptr;
	}

	// Set the surface's palette
	video.restoreSurfacePalette(ret);

	if (pixels) {

		// Upload pixel data to the surface
		if (SDL_MUSTLOCK(ret)) SDL_LockSurface(ret);

		for (int y = 0; y < height; y++)
			memcpy(static_cast<unsigned char*>(ret->pixels) + (ret->pitch * y),
				pixels + (width * y), width);

		if (SDL_MUSTLOCK(ret)) SDL_UnlockSurface(ret);

	}

	return ret;

}

void Video::destroySurface (SDL_Surface *surface) {
	if (!surface)
		return;

	SDL_FreeSurface(surface);
	surface = nullptr;
}

void Video::setClipRect (SDL_Surface *surface, const SDL_Rect *rect) {
	SDL_SetClipRect(surface, rect);
}

static void showCursor(bool enable) {
	SDL_ShowCursor(enable? SDL_ENABLE: SDL_DISABLE);
}


Video::Video () :

	screen(nullptr), scaleFactor(MIN_SCALE), scaleMethod(scalerType::None),
	fullscreen(false), isPlayingMovie(false) {

	minW = maxW = screenW = DEFAULT_SCREEN_WIDTH;
	minH = maxH = screenH = DEFAULT_SCREEN_HEIGHT;

	// Generate the logical palette
	for (int i = 0; i < MAX_PALETTE_COLORS; i++) {
		logicalPalette[i].r = logicalPalette[i].g = logicalPalette[i].b = i;
	}

	currentPalette = logicalPalette;
}


/**
 * Find the minimum and maximum horizontal and vertical resolutions.
 */
void Video::findResolutions () {

#ifdef NO_RESIZE
	minW = maxW = DEFAULT_SCREEN_WIDTH;
	minH = maxH = DEFAULT_SCREEN_HEIGHT;

	LOG_DEBUG("Using fixed resolution %dx%d.",
		DEFAULT_SCREEN_WIDTH, DEFAULT_SCREEN_HEIGHT);

	// no need to sanitize
	return;
#else
	SDL_Rect **resolutions = SDL_ListModes(nullptr, fullscreen? FULLSCREEN_FLAGS: WINDOWED_FLAGS);

	// All resolutions available, set to arbitrary limit
	if (resolutions == reinterpret_cast<SDL_Rect**>(-1)) {
		LOG_DEBUG("No display mode limit found.");
		minW = SW;
		minH = SH;
		maxW = MAX_SCREEN_WIDTH;
		maxH = MAX_SCREEN_HEIGHT;
	} else {

		for (int i = 0; resolutions[i] != nullptr; i++) {

			// Save largest resolution
			if (i == 0) {
				maxW = resolutions[i]->w;
				maxH = resolutions[i]->h;
			}

			// Save smallest resolution
			if (resolutions[i + 1] == nullptr) {
				minW = resolutions[i]->w;
				minH = resolutions[i]->h;
			}

		}

	}
#endif

	// Sanitize
	if (minW < SW) minW = SW;
	if (minH < SH) minH = SH;
	if (maxW > MAX_SCREEN_WIDTH) maxW = MAX_SCREEN_WIDTH;
	if (maxH > MAX_SCREEN_HEIGHT) maxH = MAX_SCREEN_HEIGHT;

	LOG_TRACE("Allowing resolutions between %dx%d and %dx%d.", minW, minH, maxW, maxH);

}

/**
 * Initialise video output.
 *
 * @param cfg Video Options
 *
 * @return Success
 */
bool Video::init (SetupOptions cfg) {

	fullscreen = cfg.fullScreen;

	// The window stays for whole runtime, as it is needed for
	// e.g. determining desktop resolution

	if (fullscreen) showCursor(false);

	//findResolutions();

#ifdef SCALE

	scaleFactor = cfg.videoScale;
	
	scaleMethod = cfg.scaleMethod;
#endif

	if (!reset(cfg.videoWidth, cfg.videoHeight)) {
		LOG_FATAL("Could not set video mode: %s", SDL_GetError());
		return false;
	}

	setTitle(nullptr);

	return true;
}

/**
 * Shared Deinitialisation code for reset() and deinit()
 *
 */
void Video::commonDeinit () {
	// canvas is used when scaling or built with SDL2
	if (canvas && canvas != screen)
		destroySurface(canvas);
}

/**
 * Deinitialise video output.
 *
 */
void Video::deinit () {
	commonDeinit();
}

/**
 * Sets the size of the video window or the resolution of the screen.
 *
 * @param width New width of the window or screen
 * @param height New height of the window or screen
 *
 * @return Success
 */
bool Video::reset (int width, int height) {
	
	LOG_DEBUG("Video::reset %d x %d", width, height);
	
    int tvW = 720;
    int tvH = 480;

    // --- BLOQUE DE INICIALIZACIÓN ÚNICA ---
    if (!screen) { 
        #ifdef __PS3__
        videoState vState;
        if (videoGetState(0, 0, &vState) == 0) {
            videoConfiguration vConfig;
            memset(&vConfig, 0, sizeof(videoConfiguration));
            vConfig.resolution = VIDEO_RESOLUTION_480; 
            vConfig.format = VIDEO_BUFFER_FORMAT_XRGB;
            vConfig.aspect = VIDEO_ASPECT_16_9; 
            videoConfigure(0, &vConfig, NULL, 0);
            usleep(200000); 
        }
        #endif

        screenW = tvW;
        screenH = tvH;
        screen = SDL_SetVideoMode(screenW, screenH, 32, SDL_HWSURFACE | SDL_DOUBLEBUF | SDL_FULLSCREEN);
        
        if (!screen)
		{
			LOG_DEBUG("Video::reset no screen!");
			return false;
		}
        
        // Limpieza inicial para evitar rayas
        SDL_FillRect(screen, nullptr, 0);
        SDL_Flip(screen);
    }

    // --- BLOQUE DE CAMBIO DINÁMICO (Solo afecta al Canvas) ---
    // Si ya había un canvas, lo borramos para crear el nuevo con la nueva escala
    if (canvas && canvas != screen) {
        SDL_FreeSurface(canvas);
        canvas = nullptr;
    }

    canvasW = width;
    canvasH = height;
    
    // El canvas siempre es SWSURFACE porque es donde la CPU de la PS3 dibuja el juego
    canvas = SDL_CreateRGBSurface(SDL_SWSURFACE, canvasW, canvasH, 8, 0, 0, 0, 0);
    
    if (!canvas){
		LOG_DEBUG("Video::reset no canvas!");
		return false;
	}

    video.restoreSurfacePalette(canvas);
    expose();

	LOG_DEBUG("Video::reset return True");
    return true;
}

/**
 * Sets the display palette.
 *
 * @param palette The new palette
 */
void Video::setPalette (SDL_Color *palette) {

	// Make palette changes invisible until the next draw. Hopefully.
	clearScreen(SDL_MapRGB(screen->format, 0, 0, 0));
	flip(0);

	changePalette(palette, 0, MAX_PALETTE_COLORS);
	currentPalette = palette;
}

/**
 * Sets some colours of the display palette.
 *
 * @param palette The palette containing the new colours
 * @param first The index of the first colour in both the display palette and the specified palette
 * @param amount The number of colours
 */
void Video::changePalette (SDL_Color *palette, unsigned char first, unsigned int amount) {
	SDL_SetPalette(screen, SDL_PHYSPAL, palette, first, amount);
}

/**
 * Restores a surface's palette.
 *
 * @param surface Surface with a modified palette
 */
void Video::restoreSurfacePalette (SDL_Surface* surface) {
	setSurfacePalette(surface, logicalPalette, 0, MAX_PALETTE_COLORS);
}

/**
 * Sets the window title.
 *
 * @param title the title or nullptr, to use default
 */
void Video::setTitle (const char *title) {

	constexpr const char *titleBase = "OpenJazz";
	int titleLen = strlen(titleBase) + 1;

	if (title != nullptr) {
		titleLen = strlen(titleBase) + 3 + strlen(title) + 1;
	}

	char *windowTitle = new char[titleLen];
	strcpy(windowTitle, titleBase);

	if (title != nullptr) {
		strcat(windowTitle, " - ");
		strcat(windowTitle, title);
	}

	SDL_WM_SetCaption(windowTitle, nullptr);

	delete[] windowTitle;
}

/**
 * Sets the scaling factor and method.
 *
 * @param newScaleFactor The new scaling factor
 * @param newScaleMethod The new scaling method
 */
void Video::setScaling (int newScaleFactor, scalerType newScaleMethod) {
	bool reset_needed = true;
	scaleFactor = newScaleFactor;
	scaleMethod = newScaleMethod;
	if (screen)	reset(canvasW, canvasH);
}

/**
 * Refresh display palette.
 */
void Video::expose () {
	setSurfacePalette(screen, logicalPalette, 0, MAX_PALETTE_COLORS);
	changePalette(currentPalette, 0, MAX_PALETTE_COLORS);
}

/**
 * Update video based on a system event.
 *
 * @param event The system event. Events not affecting video will be ignored
 */
void Video::update (SDL_Event *event) {
	(void)event;
}

/**
 * Draw graphics to screen.
 *
 * @param mspf Ticks per frame
 * @param paletteEffects Palette effects to use
 * @param effectsStopped Whether the effects should be applied without advancing
 */
void Video::flip (int mspf, PaletteEffect* paletteEffects, bool effectsStopped) {
    if (!screen || !canvas) return;

    SDL_Color shownPalette[MAX_PALETTE_COLORS];

    // 1. Preparar la paleta de este frame
    SDL_Color* activePal = currentPalette;
    if (paletteEffects) {
        memcpy(shownPalette, currentPalette, sizeof(SDL_Color) * MAX_PALETTE_COLORS);
        paletteEffects->apply(shownPalette, false, mspf, effectsStopped);
        activePal = shownPalette;
    }

    // 2. BLOQUEAR SUPERFICIES (Crucial en Modo Hardware)
    if (SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
    if (SDL_MUSTLOCK(canvas)) SDL_LockSurface(canvas);

    // 3. CONVERSIÓN MANUAL 8-BIT -> 32-BIT (Estilo Tyrian/PS3)
    // Esto puentea el fallo de SDL_SoftStretch en hardware
    Uint32 *dst = (Uint32*)screen->pixels;
    Uint8 *src = (Uint8*)canvas->pixels;

    // Calculamos el escalado simple (ej. 2x) para llenar los 480p
    // Si canvas es 320x200 y screen es 720x480
    for (int y = 0; y < screenH; y++) {
        int srcY = (y * canvasH) / screenH;
        Uint8 *srcRow = src + (srcY * canvas->pitch);
        Uint32 *dstRow = dst + (y * screen->pitch / 4);

        for (int x = 0; x < screenW; x++) {
            int srcX = (x * canvasW) / screenW;
            Uint8 colorIndex = srcRow[srcX];
            
            // Convertimos el índice de paleta a color real 32-bit (XRGB)
            SDL_Color &c = activePal[colorIndex];
            dstRow[x] = (c.r << 16) | (c.g << 8) | c.b;
        }
    }

    // 4. DESBLOQUEAR Y VOLCAR
    if (SDL_MUSTLOCK(canvas)) SDL_UnlockSurface(canvas);
    if (SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);

    SDL_Flip(screen);
}

/**
 * Fill the screen with a colour.
 *
 * @param index Index of the colour to use
 */
void Video::clearScreen (int index) {
	SDL_FillRect(canvas, nullptr, index);

}

/**
 * Sets up scaling for movie mode.
 *
 * @param status Whether or not movie mode will be enabled
 */
void Video::moviePlayback (bool status) {
	(void)status;
}

/**
 * Fill a specified rectangle of the screen with a colour.
 *
 * @param x X-coordinate of the left side of the rectangle
 * @param y Y-coordinate of the top of the rectangle
 * @param width Width of the rectangle
 * @param height Height of the rectangle
 * @param index Index of the colour to use
 * @param fill Whether to fill or only color the borders
 */
void Video::drawRect (int x, int y, int width, int height, int index, bool fill) {
	SDL_Rect dst[4];

	for(int i = 0; i < 4; i++) {
		dst[i].x = x;
		dst[i].y = y;
		dst[i].w = width;
		dst[i].h = height;
	}

	if (fill) {
		SDL_FillRect(canvas, &dst[0], index);
	} else {
		// left
		dst[0].w = 1;
		// right
		dst[1].x = x + width - 1;
		dst[1].w = 1;
		// top
		dst[2].h = 1;
		// bottom
		dst[3].y = y + height - 1;
		dst[3].h = 1;
		// draw each border

		SDL_FillRect(canvas, &dst[0], index);
		SDL_FillRect(canvas, &dst[1], index);
		SDL_FillRect(canvas, &dst[2], index);
		SDL_FillRect(canvas, &dst[3], index);
	}
}

/**
 * Sets the Color key of provided surface.
 *
 * @param surface Surface to change
 * @param index Color index
 */
void Video::enableColorKey (SDL_Surface* surface, unsigned int index) {

	SDL_SetColorKey(surface, SDL_SRCCOLORKEY, index);

}

/**
 * Returns the Color key of provided surface.
 *
 * @param surface Surface to query
 *
 * @return color index
 */
unsigned int Video::getColorKey (SDL_Surface* surface) {

	Uint32 key;
	SDL_GetColorKey(surface, &key);
	return key;
	
}

/**
 * Sets the palette colors of provided surface.
 *
 * @param surface Surface to change
 * @param palette Palette to copy colors from
 * @param start index of first color copy
 * @param length number of colors to copy
 */
void Video::setSurfacePalette (SDL_Surface* surface, SDL_Color *palette, int start, int length) {

	SDL_SetPalette(surface, SDL_LOGPAL, palette, start, length);

}

int Video::getCanvasWidth()
{
	return canvasW;
}

int Video::getCanvasHeight()
{
	return canvasH;
}