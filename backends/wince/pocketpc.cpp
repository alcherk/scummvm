/* ScummVM - Scumm Interpreter
 * Copyright (C) 2001/2002 The ScummVM project
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * $Header$
 *
 */

#include "stdafx.h"
#include <assert.h>

//#include "commctrl.h"

#if _WIN32_WCE < 300

#include <Wingdi.h>
#include <Winbase.h>
#include <Wtypes.h>

#endif

#include <Winuser.h>
#include <Winnls.h>
#include <sipapi.h>

#if _WIN32_WCE >= 300

#include <Aygshell.h>

#else

// Put in include file

typedef enum tagSIPSTATE
{
    SIP_UP = 0,
    SIP_DOWN,
	SIP_FORCEDOWN,
    SIP_UNCHANGED,
    SIP_INPUTDIALOG,
} SIPSTATE;

#define SHFS_SHOWTASKBAR            0x0001
#define SHFS_HIDETASKBAR            0x0002
#define SHFS_SHOWSIPBUTTON          0x0004
#define SHFS_HIDESIPBUTTON          0x0008
#define SHFS_SHOWSTARTICON          0x0010
#define SHFS_HIDESTARTICON          0x0020

typedef struct
{
    DWORD cbSize;
    HWND hwndLastFocus;
    UINT fSipUp :1;
    UINT fSipOnDeactivation :1;
    UINT fActive :1;
    UINT fReserved :29;
} SHACTIVATEINFO, *PSHACTIVATEINFO;


#endif

#include <gx.h>
#include "resource.h"

#include "scumm.h"
#include "debug.h"
#include "screen.h"
#include "gui/newgui.h"
#include "sound/mididrv.h"
#include "gameDetector.h"
#include "simon/simon.h"
#include "gapi_keys.h"
#include "config-file.h"


#include "SDL.h"
#include "SDL_audio.h"
#include "SDL_timer.h"
#include "SDL_thread.h"

#include "dynamic_imports.h"

#if defined(MIPS) || defined(SH3)
// Comment this out if you don't want to support GameX
#define GAMEX
#endif

#ifdef GAMEX
#include "GameX.h"
#endif

#define POCKETSCUMM_BUILD "101902"
#define CURRENT_GAMES_VERSION 1
#define CURRENT_KEYS_VERSION 3

#define VERSION "Build " POCKETSCUMM_BUILD " (VM " SCUMMVM_CVS ")"

typedef int (*tTimeCallback)(int);
typedef void SoundProc(void *param, byte *buf, int len);

// Dynamically linked Aygshell
typedef BOOL (*tSHFullScreen)(HWND,DWORD);
//typedef BOOL (WINSHELLAPI *tSHHandleWMSettingChange)(HWND,WPARAM,LPARAM,SHACTIVATEINFO*);
typedef BOOL (*tSHSipPreference)(HWND,SIPSTATE);

/*
// Dynamically linked SDLAudio
typedef void (*tSDL_AudioQuit)(void);
typedef int (*tSDL_Init)(Uint32);
typedef void (*tSDL_PauseAudio)(int);
typedef int (*tSDL_OpenAudio)(SDL_AudioSpec*, SDL_AudioSpec*);
*/

// GAPI "emulation"
typedef struct pseudoGAPI {
	const TCHAR *device;
	void *buffer;
	int xWidth;
	int yHeight;
	int xPitch;
	int yPitch;
	int BPP;
	int format;
} pseudoGAPI;

typedef struct {
	int x, y, w, h;
} dirty_square;

#define MAX_NUMBER_OF_DIRTY_SQUARES 32

#define AddDirtyRect(xi,yi,wi,hi) 				\
  if (num_of_dirty_square < MAX_NUMBER_OF_DIRTY_SQUARES) {	\
    ds[num_of_dirty_square].x = xi; \
    ds[num_of_dirty_square].y = yi;				\
    ds[num_of_dirty_square].w = wi;				\
    ds[num_of_dirty_square].h = hi;				\
    num_of_dirty_square++;					\
  }


/* Hardcode the video buffer for some devices for which there is no GAPI */
/* and no GameX support */

pseudoGAPI availablePseudoGAPI[] = {
	{ TEXT("HP, Jornada 710"),
      (void*)0x82200000,
	  640,
	  240,
	  2,
	  1280,
	  16,
      0xA8
	},
	{ TEXT("HP, Jornada 720"),
      (void*)0x82200000,
	  640,
	  240,
	  2,
	  1280,
	  16,
      0xA8
	},
	{ TEXT("Compaq iPAQ H3600"),   /* this is just a test for my device :) */
	  (void*)0xAC05029E,
	  320,
	  240,
	  640,
	  -2,
	  16,
	  0xA8
	},
	{ 0, 0, 0, 0, 0, 0, 0, 0 }
};

int _pseudoGAPI_device;

/* Default SDLAUDIO */

/*

void defaultSDL_AudioQuit() {
}

int defaultSDL_Init(Uint32 flags) {
	return 0;
}

void defaultSDL_PauseAudio(int pause) {
}

int defaultSDL_OpenAudio(SDL_AudioSpec *desired, SDL_AudioSpec *obtained) {
	return 0;
}

*/

/* Default AYGSHELL */

BOOL defaultSHFullScreen(HWND handle, DWORD action) {
	if ((action & SHFS_HIDETASKBAR) != 0 || (action & SHFS_SHOWTASKBAR) != 0) {
		// Hide taskbar, WinCE 2.x style - from EasyCE
		HKEY hKey=0;
		DWORD dwValue = 0;
		unsigned long lSize = sizeof( DWORD );
		DWORD dwType = REG_DWORD;
		MSG msg;

		
		RegOpenKeyEx( HKEY_LOCAL_MACHINE, TEXT("\\software\\microsoft\\shell"), 0, KEY_ALL_ACCESS, &hKey );
		RegQueryValueEx( hKey, TEXT("TBOpt"), 0, &dwType, (BYTE*)&dwValue, &lSize );
		if ((action & SHFS_SHOWTASKBAR) != 0)
			 dwValue &= 0xFFFFFFFF - 8;// reset bit to show taskbar
	 	else dwValue |= 8;			   // set bit to hide taskbar
		RegSetValueEx( hKey, TEXT("TBOpt"), 0, REG_DWORD, (BYTE*)&dwValue, lSize );
		msg.hwnd = FindWindow( TEXT("HHTaskBar"), NULL );
		SendMessage( msg.hwnd, WM_COMMAND, 0x03EA, 0 );
		if (handle)
			SetForegroundWindow( handle );
	}

	return TRUE;
}

/*
BOOL WINSHELLAPI defaultSHHandleWMSettingChange(HWND handle, WPARAM param1, LPARAM param2, SHACTIVATEINFO *info) {
	return TRUE;
}
*/

BOOL defaultSHSipPreference(HWND handle, SIPSTATE state) {
	return TRUE;
}

/* Default GAPI */

int defaultGXOpenDisplay(HWND hWnd, DWORD dwFlags) {
	return 0;
}

int defaultGXCloseDisplay() {
	return 0;
}


void* defaultGXBeginDraw() {
	return availablePseudoGAPI[_pseudoGAPI_device].buffer;
}

int defaultGXEndDraw() {
	return 0;
}

int defaultGXOpenInput() {
	return 0;
}

int defaultGXCloseInput() {
	return 0;
}

GXDisplayProperties defaultGXGetDisplayProperties() {
	GXDisplayProperties result;

	result.cxWidth = availablePseudoGAPI[_pseudoGAPI_device].xWidth;
	result.cyHeight = availablePseudoGAPI[_pseudoGAPI_device].yHeight;
	result.cbxPitch = availablePseudoGAPI[_pseudoGAPI_device].xPitch;
	result.cbyPitch = availablePseudoGAPI[_pseudoGAPI_device].yPitch;
	result.cBPP = availablePseudoGAPI[_pseudoGAPI_device].BPP;
	result.ffFormat = availablePseudoGAPI[_pseudoGAPI_device].format;

	return result;
}

GXKeyList defaultGXGetDefaultKeys(int options) {
	GXKeyList result;

	memset(&result, 0xff, sizeof(result));

	return result;
}

int defaultGXSuspend() {
	return 0;
}

int defaultGXResume() {
	return 0;
}

/* GAMEX GAPI emulation */

#ifdef GAMEX

GameX *gameX;

int gameXGXOpenDisplay(HWND hWnd, DWORD dwFlags) {
	gameX = new GameX();
	if (!gameX->OpenGraphics()) {
		MessageBox(NULL, TEXT("Couldn't initialize GameX"), TEXT("Error"), MB_OK);
		exit(1);
	}
	return 0;
}

int gameXGXCloseDisplay() {
	gameX->CloseGraphics();
	delete gameX;
	return 0;
}


void* gameXGXBeginDraw() {
	gameX->BeginDraw();
	return (gameX->GetFBAddress());
}

int gameXGXEndDraw() {
	return gameX->EndDraw();
}

int gameXGXOpenInput() {
	return 0;
}

int gameXGXCloseInput() {
	return 0;
}

GXDisplayProperties gameXGXGetDisplayProperties() {
	GXDisplayProperties result;

	result.cBPP = gameX->GetFBBpp();
	if (result.cBPP == 16)
		result.cbxPitch = 2;
	else
		result.cbxPitch = 1;
	result.cbyPitch = gameX->GetFBModulo();

	return result;
}

GXKeyList gameXGXGetDefaultKeys(int options) {
	GXKeyList result;

	memset(&result, 0xff, sizeof(result));

	return result;
}

int gameXGXSuspend() {
	return 0;
}

int gameXGXResume() {
	return 0;
}

#endif

GameDetector detector;
Engine *engine;
bool is_simon;
NewGui *g_gui;
extern Scumm *g_scumm;
//extern SimonState *g_simon;
//OSystem *g_system;
//SoundMixer *g_mixer;
Config *g_config;
tTimeCallback timer_callback;
int timer_interval;

tSHFullScreen dynamicSHFullScreen = NULL;
//tSHHandleWMSettingChange dynamicSHHandleWMSettingChange = NULL;
tSHSipPreference dynamicSHSipPreference = NULL;
tGXOpenInput dynamicGXOpenInput = NULL;
tGXGetDefaultKeys dynamicGXGetDefaultKeys = NULL;
tGXCloseDisplay dynamicGXCloseDisplay = NULL;
tGXCloseInput dynamicGXCloseInput = NULL;
tGXSuspend dynamicGXSuspend = NULL;
tGXResume dynamicGXResume = NULL;
tGXGetDisplayProperties dynamicGXGetDisplayProperties = NULL;
tGXOpenDisplay dynamicGXOpenDisplay = NULL;
tGXEndDraw dynamicGXEndDraw = NULL;
tGXBeginDraw dynamicGXBeginDraw = NULL;

extern void Cls();

extern BOOL isPrescanning();
extern void changeScanPath();
extern void startScan();
extern void endScanPath();
extern void abortScanPath();

void load_key_mapping();
void keypad_init();

extern void Cls();

extern BOOL isPrescanning();
extern void changeScanPath();
extern void startScan();
extern void endScanPath();
extern void abortScanPath();

void keypad_init();

class OSystem_WINCE3 : public OSystem {
public:
	// Set colors of the palette
	void set_palette(const byte *colors, uint start, uint num);

	// Set the size of the video bitmap.
	// Typically, 320x200
	void init_size(uint w, uint h);

	// Draw a bitmap to screen.
	// The screen will not be updated to reflect the new bitmap
	void copy_rect(const byte *buf, int pitch, int x, int y, int w, int h);

	// Update the dirty areas of the screen
	void update_screen();

	// Either show or hide the mouse cursor
	bool show_mouse(bool visible);
	
	// Set the position of the mouse cursor
	void set_mouse_pos(int x, int y);
	
	// Set the bitmap that's used when drawing the cursor.
	void set_mouse_cursor(const byte *buf, uint w, uint h, int hotspot_x, int hotspot_y);
	
	// Shaking is used in SCUMM. Set current shake position.
	void set_shake_pos(int shake_pos);
		
	// Get the number of milliseconds since the program was started.
	uint32 get_msecs();
	
	// Delay for a specified amount of milliseconds
	void delay_msecs(uint msecs);
	
	// Create a thread
	void *create_thread(ThreadProc *proc, void *param);
	
	// Get the next event.
	// Returns true if an event was retrieved.	
	bool poll_event(Event *event);
	
	// Set function that generates samples 
	bool set_sound_proc(void *param, SoundProc *proc, byte sound);
		
	// Poll cdrom status
	// Returns true if cd audio is playing
	bool poll_cdrom();

	// Play cdrom audio track
	void play_cdrom(int track, int num_loops, int start_frame, int end_frame);

	// Stop cdrom audio track
	void stop_cdrom();

	// Update cdrom audio status
	void update_cdrom();

	// Add a new callback timer
	void set_timer(int timer, int (*callback)(int));

	// Quit
	void quit();

	// Set a parameter
	uint32 property(int param, Property *value);

	// Overlay
	void show_overlay();
	void hide_overlay();
	void clear_overlay();
	void grab_overlay(int16 *buf, int pitch);
	void copy_rect_overlay(const int16 *buf, int pitch, int x, int y, int w, int h);

	void move_screen(int dx, int dy, int height);

	static OSystem *create(int gfx_mode, bool full_screen);

	// Added for hardware keys mapping

	void addEventKeyPressed(int ascii_code);

	void addEventRightButtonClicked();

	// Mutex functions

	void* create_mutex();
	void lock_mutex(void*);
	void unlock_mutex(void*);
	void delete_mutex(void*);

	// Windows callbacks & stuff
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
	// Windows callbacks & stuff
	//bool handleMessage();
	

	byte *_gfx_buf;
	byte *_overlay_buf;
	uint _screenHeight;
	uint _screenWidth;
	bool _overlay_visible;
	uint32 _start_time;
	Event _event;
	Event _last_mouse_event;
	HMODULE hInst;
	HWND hWnd;
	bool _display_cursor;	

	enum {
		DF_FORCE_FULL_ON_PALETTE = 1,
		DF_WANT_RECT_OPTIM = 2,
		DF_2xSAI = 4,
		DF_SEPARATE_HWSCREEN = 8,
		DF_UPDATE_EXPAND_1_PIXEL = 16,
	};

	int _mode;
	bool _full_screen;
	bool _mouse_visible;
	bool _mouse_drawn;
	uint32 _mode_flags;

	byte _internal_scaling;

	bool force_full; //Force full redraw on next update_screen
	bool cksum_valid;

	enum {
		NUM_DIRTY_RECT = 100,
		SCREEN_WIDTH = 320,
		SCREEN_HEIGHT = 200,
		CKSUM_NUM = (SCREEN_WIDTH*SCREEN_HEIGHT/(8*8)),

		MAX_MOUSE_W = 40,
		MAX_MOUSE_H = 40,
		MAX_SCALING = 3,

		TMP_SCREEN_OFFS = 320*2 + 8,
	};

	/* CD Audio */
	int cd_track, cd_num_loops, cd_start_frame, cd_end_frame;
	Uint32 cd_end_time, cd_stop_time, cd_next_second;

	struct MousePos {
		int16 x,y,w,h;
	};

	byte *_ms_buf;
	byte *_ms_backup;
	MousePos _ms_cur;
	MousePos _ms_old;
	int16 _ms_hotspot_x;
	int16 _ms_hotspot_y;
	int _current_shake_pos;

	
	static void fill_sound(void *userdata, Uint8 * stream, int len);
	

	void draw_mouse();
	void undraw_mouse();

	void load_gfx_mode();
	void unload_gfx_mode();

	void hotswap_gfx_mode();

	void get_320x200_image(byte *buf);
};

/************* WinCE Specifics *****************/
byte veryFastMode;

bool sound_activated, terminated;
HWND hWnd_MainMenu;
HWND hWnd_Window;

void drawAllToolbar(bool);
void redrawSoundItem();
ToolbarSelected getToolbarSelection(int, int);

extern bool toolbar_drawn;
extern bool draw_keyboard;
bool hide_toolbar;
bool hide_cursor;
bool save_hide_toolbar;
bool keyboard_override;

bool _get_key_mapping;
static char _directory[MAX_PATH];
bool select_game;

bool gfx_mode_switch;

dirty_square ds[MAX_NUMBER_OF_DIRTY_SQUARES];
int num_of_dirty_square;


SoundProc *real_soundproc;

const char KEYBOARD_MAPPING_ALPHA_HIGH[] = {"ABCDEFGHIJKLM"};
const char KEYBOARD_MAPPING_NUMERIC_HIGH[] = {"12345"};
const char KEYBOARD_MAPPING_ALPHA_LOW[] = {"NOPQRSTUVWXYZ"};
const char KEYBOARD_MAPPING_NUMERIC_LOW[] = {"67890"};

extern void startFindGame();
extern void displayGameInfo();
extern bool loadGameSettings(void);
extern void setFindGameDlgHandle(HWND);
extern void getSelectedGame(int, char*, TCHAR*);
extern void runGame(char*);

extern void palette_update();

extern void own_soundProc(void *buffer, byte *samples, int len);

extern int chooseGame(bool);
extern void handleSelectGame(int, int);

//#define SHMenuBar_GetMenu(hWndMB,ID_MENU) (HMENU)SendMessage((hWndMB), SHCMBM_GETSUBMENU, (WPARAM)0, (LPARAM)ID_MENU)

/* Monkey2 keyboard stuff */
bool monkey2_keyboard;

bool closing = false;

void close_GAPI() {
	g_config->setBool("Sound", sound_activated, "wince");
	g_config->setInt("DisplayMode", GetScreenMode(), "wince");
	g_config->flush();
	dynamicSHFullScreen(hWnd_Window, SHFS_SHOWTASKBAR | SHFS_SHOWSIPBUTTON | SHFS_SHOWSTARTICON);
	dynamicGXCloseInput();
	dynamicGXCloseDisplay();
	SDL_AudioQuit();
	UpdateWindow(hWnd_Window);
	closing = true;
}

void do_quit() {
	close_GAPI();
	exit(1);
}

void Error(LPCTSTR msg)
{
	OutputDebugString(msg);
	MessageBox(HWND_DESKTOP, msg, TEXT("Error"), MB_ICONSTOP);
	exit(1);
}

void Warning(LPCTSTR msg)
{
	OutputDebugString(msg);
	MessageBox(HWND_DESKTOP, msg, TEXT("Error"), MB_ICONSTOP);	
}

int mapKey(int key) {
	if (key>=VK_F1 && key<=VK_F9) {
		return key - VK_F1 + 315;
	}
	return key;
}


#define IMPORT(Handle,Variable,Type,Function) \
	Variable = (Type)GetProcAddress(Handle, TEXT(Function)); \
	if (!Variable) { \
		MessageBox(NULL, TEXT(Function), TEXT("Error importing DLL function"), MB_OK); \
		exit(1); \
	}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd)
{
	TCHAR directory[MAX_PATH];
	char game_name[100];
	bool sound;
	int version;
	int result;
	bool need_rescan = false;

	HMODULE aygshell_handle;
	//HMODULE SDLAudio_handle;
	HMODULE GAPI_handle;

	hide_toolbar = false;

	// See if we're running on a Windows CE version supporting aygshell
	aygshell_handle = LoadLibrary(TEXT("aygshell.dll"));
	if (aygshell_handle) {
		IMPORT(aygshell_handle, dynamicSHFullScreen, tSHFullScreen, "SHFullScreen")
		IMPORT(aygshell_handle, dynamicSHSipPreference, tSHSipPreference, "SHSipPreference")
		// This function doesn't seem to be implemented on my 3630 !
		//IMPORT(aygshell_handle, dynamicSHHandleWMSettingChange, tSHHandleWMSettingChange, "SHHandleWMSettingChange")
	} else {
		dynamicSHFullScreen = defaultSHFullScreen;
		dynamicSHSipPreference = defaultSHSipPreference;
		//dynamicSHHandleWMSettingChange = defaultSHHandleWMSettingChange;
	}

	// See if GX.dll is present 
	GAPI_handle = LoadLibrary(TEXT("gx.dll"));
	if (GAPI_handle) {
		IMPORT(GAPI_handle, dynamicGXOpenInput, tGXOpenInput, "?GXOpenInput@@YAHXZ")
		IMPORT(GAPI_handle, dynamicGXGetDefaultKeys, tGXGetDefaultKeys, "?GXGetDefaultKeys@@YA?AUGXKeyList@@H@Z")
		IMPORT(GAPI_handle, dynamicGXCloseDisplay, tGXCloseDisplay, "?GXCloseDisplay@@YAHXZ")
		IMPORT(GAPI_handle, dynamicGXCloseInput, tGXCloseInput, "?GXCloseInput@@YAHXZ")
		IMPORT(GAPI_handle, dynamicGXSuspend, tGXSuspend, "?GXSuspend@@YAHXZ")
		IMPORT(GAPI_handle, dynamicGXResume, tGXResume, "?GXResume@@YAHXZ")
		IMPORT(GAPI_handle, dynamicGXGetDisplayProperties, tGXGetDisplayProperties, "?GXGetDisplayProperties@@YA?AUGXDisplayProperties@@XZ")
		IMPORT(GAPI_handle, dynamicGXOpenDisplay, tGXOpenDisplay, "?GXOpenDisplay@@YAHPAUHWND__@@K@Z")
		IMPORT(GAPI_handle, dynamicGXEndDraw, tGXEndDraw, "?GXEndDraw@@YAHXZ")
		IMPORT(GAPI_handle, dynamicGXBeginDraw, tGXBeginDraw, "?GXBeginDraw@@YAPAXXZ")
		gfx_mode_switch = true;
	} else {

#ifndef GAMEX

		TCHAR oeminfo[MAX_PATH];
		int i = 0;

		SystemParametersInfo(SPI_GETOEMINFO, sizeof(oeminfo), oeminfo, 0);

		while (availablePseudoGAPI[i].device) {
			if (!_tcsncmp(oeminfo, availablePseudoGAPI[i].device, _tcslen(availablePseudoGAPI[i].device))) {
				_pseudoGAPI_device = i;
				break;
			}
			i++;
		}

		if (!availablePseudoGAPI[i].device) {
			MessageBox(NULL, TEXT("Cannot find GX.dll and no workaround for this device ! better luck next time ..."), TEXT("GAPI not found"), MB_OK);
			exit(1);
		}

		dynamicGXOpenInput = defaultGXOpenInput;
		dynamicGXGetDefaultKeys = defaultGXGetDefaultKeys;
		dynamicGXCloseDisplay = defaultGXCloseDisplay;
		dynamicGXCloseInput = defaultGXCloseInput;
		dynamicGXSuspend = defaultGXSuspend;
		dynamicGXResume = defaultGXResume;
		dynamicGXGetDisplayProperties = defaultGXGetDisplayProperties;
		dynamicGXOpenDisplay = defaultGXOpenDisplay;
		dynamicGXEndDraw = defaultGXEndDraw;
		dynamicGXBeginDraw = defaultGXBeginDraw;

#else

		dynamicGXOpenInput = gameXGXOpenInput;
		dynamicGXGetDefaultKeys = gameXGXGetDefaultKeys;
		dynamicGXCloseDisplay = gameXGXCloseDisplay;
		dynamicGXCloseInput = gameXGXCloseInput;
		dynamicGXSuspend = gameXGXSuspend;
		dynamicGXResume = gameXGXResume;
		dynamicGXGetDisplayProperties = gameXGXGetDisplayProperties;
		dynamicGXOpenDisplay = gameXGXOpenDisplay;
		dynamicGXEndDraw = gameXGXEndDraw;
		dynamicGXBeginDraw = gameXGXBeginDraw;

#endif

		gfx_mode_switch = false;
	}

	g_config = new Config("scummvm.ini", "scummvm");
	g_config->set_writing(true);

	sound = g_config->getBool("Sound", true, "wince");
	if (sound) 
		sound_activated = sound;
	else
		sound_activated = true;

	version = g_config->getInt("GamesVersion", 0, "wince");
	if (!version || version != CURRENT_GAMES_VERSION) 
		need_rescan = true;

	select_game = true;

	/* Create the main window */
	WNDCLASS wcex;
	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= (WNDPROC)OSystem_WINCE3::WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= GetModuleHandle(NULL);
	wcex.hIcon			= 0;
	wcex.hCursor		= NULL;
	wcex.hbrBackground	= (HBRUSH)GetStockObject(BLACK_BRUSH);
	wcex.lpszMenuName	= 0;	
	wcex.lpszClassName	= TEXT("ScummVM");
	if (!RegisterClass(&wcex))
		Error(TEXT("Cannot register window class!"));

	hWnd_Window = CreateWindow(TEXT("ScummVM"), TEXT("ScummVM"), WS_VISIBLE,
      0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), NULL, NULL, GetModuleHandle(NULL), NULL);

	ShowWindow(hWnd_Window, SW_SHOW);
	UpdateWindow(hWnd_Window);
	GraphicsOn(hWnd_Window, gfx_mode_switch);  // open GAPI in Portrait mode
	GAPIKeysInit();
	Cls();

	// Hide taskbar
	SetWindowPos(hWnd_Window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE);
	SetForegroundWindow(hWnd_Window);
	dynamicSHFullScreen(hWnd_Window, SHFS_HIDESIPBUTTON | SHFS_HIDETASKBAR | SHFS_HIDESTARTICON);

	result = chooseGame(need_rescan);

	if (need_rescan) {
		g_config->setInt("GamesVersion", CURRENT_GAMES_VERSION, "wince");
		g_config->flush();
	}

	getSelectedGame(result, game_name, directory);
	WideCharToMultiByte(CP_ACP, 0, directory, wcslen(directory) + 1, _directory, sizeof(_directory), NULL, NULL);
	strcat(_directory, "\\");
	
	runGame(game_name);

	return 0;
}

void runGame(char *game_name) {
	int argc = 3;
	char* argv[3];
	char argdir[MAX_PATH];

	select_game = false;

	argv[0] = NULL;	
	sprintf(argdir, "-p%s", _directory);
	argv[1] = argdir;
	argv[2] = game_name;

	if (!argv[2])
		//return 0;
		return;

	// No default toolbar for zak256
	/*
	if (strcmp(game_name, "zak256") == 0)
		hide_toolbar = true;
	*/

	// Keyboard activated for Monkey Island 2
	if (strcmp(game_name, "monkey2") == 0) {
		draw_keyboard = true;
		monkey2_keyboard = true;
	}		

	detector.parseCommandLine(argc, argv);

	if (detector.detectMain())
		//return (-1);
		return;

	OSystem *system = detector.createSystem();

	//g_system = system;
	g_gui = new NewGui(system);

	/* Start the engine */

	engine = Engine::createFromDetector(&detector, system);

	keypad_init();
	load_key_mapping();

	if (detector._gameId == GID_SAMNMAX || detector._gameId == GID_FT || detector._gameId == GID_DIG)
		hide_cursor = FALSE;
	else
		hide_cursor = TRUE;

	is_simon = (detector._gameId >= GID_SIMON_FIRST);

	engine->go();

	//return 0;
}



LRESULT CALLBACK OSystem_WINCE3::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static		 SHACTIVATEINFO sai;

	OSystem_WINCE3 *wm = NULL;
	
	if (!select_game)
		wm = (OSystem_WINCE3*)GetWindowLong(hWnd, GWL_USERDATA);
	
	if (!select_game && monkey2_keyboard && g_scumm->_vars[g_scumm->VAR_ROOM] != 108) {
		monkey2_keyboard = false;
		draw_keyboard = false;
		toolbar_drawn = false;
	}

	switch (message) 
	{
	case WM_CREATE:
		memset(&sai, 0, sizeof(sai));
		dynamicSHSipPreference(hWnd, SIP_FORCEDOWN);
//		SHSipPreference(hWnd, SIP_INPUTDIALOG);

		return 0;

	case WM_DESTROY:
	case WM_CLOSE:
		GraphicsOff();
		PostQuitMessage(0);
		break;

	case WM_ERASEBKGND:
		{
			
			RECT rc;
			HDC hDC;
			if (select_game || closing)
				break;
			if (!GetScreenMode()) {
				GetClientRect(hWnd, &rc);
				rc.top = 200;
				hDC = GetDC(hWnd);
				if(rc.top < rc.bottom)
					FillRect(hDC, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
				ReleaseDC(hWnd, hDC);			
			}			
		}
		return 1;

	case WM_PAINT:
		{
			HDC hDC;
			PAINTSTRUCT ps;
			hDC = BeginPaint (hWnd, &ps);
			EndPaint (hWnd, &ps);
			if (!hide_toolbar)
				toolbar_drawn = false;

			/*
			if(!GetScreenMode()) {
				SHSipPreference(hWnd, SIP_UP);
			} else {
				SHSipPreference(hWnd, SIP_FORCEDOWN);
			} 
			*/
			dynamicSHSipPreference(hWnd, SIP_FORCEDOWN);
		}
//		SHSipPreference(hWnd, SIP_UP); /* Hack! */
		/* It does not happen often but I don't want to see tooltip traces */
		if (!select_game)
			wm->update_screen();
		return 0;

	case WM_ACTIVATE:
	case WM_SETFOCUS:	
		GraphicsResume();
		if (!hide_toolbar)
			toolbar_drawn = false;
//		SHHandleWMActivate(hWnd, wParam, lParam, &sai, SHA_INPUTDIALOG);

		dynamicSHSipPreference(hWnd, SIP_FORCEDOWN);
		dynamicSHFullScreen(hWnd, SHFS_HIDETASKBAR);
		MoveWindow(hWnd, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), TRUE);
		SetCapture(hWnd);
		
		/*
		if (LOWORD(wParam) == WA_ACTIVE) {
			if (GetScreenMode()) {		
				SHSipPreference(hWnd, SIP_FORCEDOWN);
				SHFullScreen(hWnd, SHFS_HIDETASKBAR);
				MoveWindow(hWnd, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), TRUE);
				SetCapture(hWnd);
			}
			else {
				SHFullScreen(hWnd, SHFS_SHOWTASKBAR);
				MoveWindow(hWnd, 0, 0, GetSystemMetrics(SM_CYSCREEN), GetSystemMetrics(SM_CXSCREEN), TRUE);
			}
		}
		*/

		return 0;

	case WM_HIBERNATE:
	case WM_KILLFOCUS:
		GraphicsSuspend();
		if (!hide_toolbar)
			toolbar_drawn = false;
		return 0;

	case WM_SETTINGCHANGE:
		//not implemented ?
		//dynamicSHHandleWMSettingChange(hWnd, wParam, lParam, &sai);
		if (!hide_toolbar)
			toolbar_drawn = false;
		return 0;

	
	case WM_COMMAND:		
		return 0;
	
	case WM_KEYDOWN:

		if (wParam == VK_ESCAPE)   // FIXME
			do_quit();

		if(wParam && wParam != 0x84 && wParam != 0x5B) { // WHAT THE ???			

			if (select_game) {
				GAPIKeysHandleSelect((int)wParam);
				break;
			}

			/*
			unsigned char GAPI_key;

			GAPI_key = getGAPIKeyMapping((short)wParam);
			if (GAPI_key) {
			*/
				if (_get_key_mapping) {
					wm->_event.kbd.flags = 0xff;
					wm->_event.kbd.ascii = GAPIKeysTranslate((unsigned int)(wParam));
					wm->_event.event_code = EVENT_KEYDOWN;
					break;
				}					
				/*
				else
					processAction((short)wParam);
				*/
			/*}*/
			if (!processAction(GAPIKeysTranslate((unsigned int)(wParam))))
			/*else*/ {
				wm->_event.kbd.ascii = mapKey(wParam);
				wm->_event.event_code = EVENT_KEYDOWN;								
			}
		}

		break;

	case WM_KEYUP:
		if (_get_key_mapping) {
			_get_key_mapping = false;
			wm->_event.kbd.flags = 0xff;
			wm->_event.kbd.ascii = GAPIKeysTranslate((int)wParam);
			wm->_event.event_code = EVENT_KEYUP;
			break;
		}
		break;

	case WM_MOUSEMOVE:
		{
			int x = ((int16*)&lParam)[0];
			int y = ((int16*)&lParam)[1];
			/*if (select_game) {
				handleSelectGame(x, y);
				break;
			}*/
			if (select_game)
				break;
			Translate(&x, &y);
			wm->_event.event_code = EVENT_MOUSEMOVE;
			wm->_event.mouse.x = x;
			wm->_event.mouse.y = y;
			wm->_last_mouse_event = wm->_event;
		}
		break;
	case WM_LBUTTONDOWN:
		{
			ToolbarSelected toolbar_selection;
			int x = ((int16*)&lParam)[0];
			int y = ((int16*)&lParam)[1];
			if (select_game) {
				handleSelectGame(x, y);
				break;
			}

			Translate(&x, &y);

			if (draw_keyboard) {
				// Handle keyboard selection
				int offset_y;
				int saved_x = x;
				int saved_y = y;

				/*
				if (!GetScreenMode()) {
					x = ((int16*)&lParam)[0];
					y = ((int16*)&lParam)[1];
				}
				*/

				offset_y = (GetScreenMode() ? 0 : 40 + 22);

				if (x<185 && y>=(200 + offset_y)) {
					//Alpha selection
					wm->_event.event_code = EVENT_KEYDOWN;
					wm->_event.kbd.ascii = 
						(y <= (220 + offset_y)? KEYBOARD_MAPPING_ALPHA_HIGH[((x + 10) / 14) - 1] :
												KEYBOARD_MAPPING_ALPHA_LOW[((x + 10) / 14) - 1]);
					break;
				} 
				else
				if (x>=186 && y>=(200 + offset_y) && x<=255) {
				   // Numeric selection
				   wm->_event.event_code = EVENT_KEYDOWN;
				   wm->_event.kbd.ascii =
					   (y <= (220 + offset_y) ? KEYBOARD_MAPPING_NUMERIC_HIGH[((x - 187 + 10) / 14) - 1] :
												KEYBOARD_MAPPING_NUMERIC_LOW[((x - 187 + 10) / 14) - 1]);
				   break;
				}
				else
				if (x>=302 && x <= 316 && y >= (200 + offset_y) && y <= (220 + offset_y)) {
				  // Backspace
				  wm->_event.event_code = EVENT_KEYDOWN;
				  wm->_event.kbd.ascii = mapKey(VK_BACK);
				  break;
				}
				else
				if (x>=302 && x<= 316 && y >= (220 + offset_y)) { 
				  // Enter
				  wm->_event.event_code = EVENT_KEYDOWN;
				  wm->_event.kbd.ascii = mapKey(VK_RETURN);
				  break;
				}

				x = saved_x;
				y = saved_y;

				wm->_event.event_code = EVENT_LBUTTONDOWN;
				wm->_event.mouse.x = x;
				wm->_event.mouse.y = y;
				wm->_last_mouse_event = wm->_event;
				break;

			}
					

			toolbar_selection = (hide_toolbar || _get_key_mapping ? ToolbarNone : 
									 getToolbarSelection(
										 (GetScreenMode() ? x : ((int16*)&lParam)[0]), 
										 (GetScreenMode() ? y : ((int16*)&lParam)[1])));
			if (toolbar_selection == ToolbarNone) {				
				wm->_event.event_code = EVENT_LBUTTONDOWN;
				wm->_event.mouse.x = x;
				wm->_event.mouse.y = y;
				wm->_last_mouse_event = wm->_event;			
			}
			else {
				switch(toolbar_selection) {
					case ToolbarSaveLoad:
						if (is_simon) 
							break;
						/*if (GetScreenMode()) {*/
						/*
							draw_keyboard = true;
							if (!hide_toolbar)
								toolbar_drawn = false;
						*/
						/*}*/
						wm->_event.event_code = EVENT_KEYDOWN;
						wm->_event.kbd.ascii = mapKey(VK_F5);
						break;
					case ToolbarMode:
						SetScreenMode(!GetScreenMode());
						num_of_dirty_square = MAX_NUMBER_OF_DIRTY_SQUARES;
						if (!hide_toolbar)
							toolbar_drawn = false;
						break;
					case ToolbarSkip:
						if (is_simon) {
							((SimonState*)engine)->_exit_cutscene = true;
							break;
						}
						wm->_event.event_code = EVENT_KEYDOWN;
						if (g_scumm->vm.cutScenePtr[g_scumm->vm.cutSceneStackPointer])
							wm->_event.kbd.ascii = g_scumm->_vars[g_scumm->VAR_CUTSCENEEXIT_KEY];
						else
							wm->_event.kbd.ascii = g_scumm->_vars[g_scumm->VAR_TALKSTOP_KEY];						
						break;
					case ToolbarSound:
						sound_activated = !sound_activated;
						redrawSoundItem();
						break;
					default:
						break;
				}
			}
		}
		break;
	case WM_LBUTTONUP:
		{
			// pinched from the SDL code. Distinguishes between taps and not
			int x = ((int16*)&lParam)[0];
			int y = ((int16*)&lParam)[1];
			/*
			if (select_game) {
				handleSelectGame(x, y);
				break;
			}
			*/
			if (select_game)
				break;
			Translate(&x, &y);
			wm->_event.event_code = EVENT_LBUTTONUP;
			wm->_event.mouse.x = x;
			wm->_event.mouse.y = y;
			wm->_last_mouse_event = wm->_event;
		}
		break;
	case WM_LBUTTONDBLCLK:  // doesn't seem to work right now
		//wm->_scumm->_rightBtnPressed |= msClicked | msDown;
		break;
	case WM_TIMER:
		timer_callback(timer_interval);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
   }
   return 0;
}

/*************** Specific config support ***********/

void load_key_mapping() {
//	 unsigned char actions[TOTAL_ACTIONS];
	 unsigned int actions_keys[TOTAL_ACTIONS];
	 const char		*current;
	 int			version;
	 int			i;
	 
	 memset(actions_keys, 0, sizeof(actions_keys));
	 
	 version = g_config->getInt("KeysVersion", 0, "wince");

	 memset(actions_keys, 0, TOTAL_ACTIONS);

	 current = g_config->get("ActionKeys", "wince");
	 if (current && version == CURRENT_KEYS_VERSION) {
		for (i=0; i<TOTAL_ACTIONS; i++) {
			char x[6];
			int j;

			memset(x, 0, sizeof(x));
			memcpy(x, current + 5 * i, 4);
			sscanf(x, "%x", &j);
			actions_keys[i] = j;
		}
	 }
	 setActionKeys(actions_keys);

	 /*
	 memset(actions, 0, TOTAL_ACTIONS);

	 actions[0] = ACTION_PAUSE;
	 actions[1] = ACTION_SAVE;
	 actions[2] = ACTION_BOSS;
	 actions[3] = ACTION_SKIP;
	 actions[4] = ACTION_HIDE;

	 current = g_config->get("ActionTypes", "wince");
	 if (current && version) {
		for (i=0; i<TOTAL_ACTIONS; i++) {
			char x[6];
			int j;

			memset(x, 0, sizeof(x));
			memcpy(x, current + 3 * i, 2);
			sscanf(x, "%x", &j);
			actions[i] = j;
		}
	 }
	 setActionTypes(actions);
	 */

	 if (!version || version != CURRENT_KEYS_VERSION) {
		 g_config->setInt("KeysVersion", CURRENT_KEYS_VERSION, "wince");
		 g_config->flush();
	 }
}
					
void save_key_mapping() {
	 char tempo[1024];
	 const unsigned int *work_keys;
//	 const unsigned char *work;
	 int i;

	 tempo[0] = '\0';
	 work_keys = getActionKeys();
	 for (i=0; i<TOTAL_ACTIONS; i++) {
		 char x[4];
		 sprintf(x, "%.4x ", work_keys[i]);
		 strcat(tempo, x);
	 }
	 g_config->set("ActionKeys", tempo, "wince");

/*
	 tempo[0] = '\0';

	 work = getActionTypes();
	 for (i=0; i<TOTAL_ACTIONS; i++) {
		 char x[3];
		 sprintf(x, "%.2x ", work[i]);
		 strcat(tempo, x);
	 }
	 g_config->set("ActionTypes", tempo, "wince");
*/

	 g_config->flush();
}

/*************** Hardware keys support ***********/

void OSystem_WINCE3::addEventKeyPressed(int ascii_code) {
	_event.event_code = EVENT_KEYDOWN;
	_event.kbd.ascii = ascii_code;
}

void OSystem_WINCE3::addEventRightButtonClicked() {
	OSystem_WINCE3* system;
	system = (OSystem_WINCE3*)g_scumm->_system;
	
	system->addEventKeyPressed(9);
}

void action_right_click() {
	OSystem_WINCE3* system;
	system = (OSystem_WINCE3*)g_scumm->_system;

	system->addEventRightButtonClicked();	
}

void action_pause() {
	OSystem_WINCE3* system;
	system = (OSystem_WINCE3*)g_scumm->_system;

	system->addEventKeyPressed(mapKey(VK_SPACE));
}

void action_save() {
	OSystem_WINCE3* system;
	system = (OSystem_WINCE3*)g_scumm->_system;

	/*if (GetScreenMode()) {*/
	/*
		draw_keyboard = true;
		if (!hide_toolbar)
			toolbar_drawn = false;
	*/
	/*}*/

	system->addEventKeyPressed(mapKey(VK_F5));
}

void action_quit() {
	do_quit();
}

void action_boss() {
	SHELLEXECUTEINFO se;    

	g_config->setBool("Sound", sound_activated, "wince");
	g_config->setInt("DisplayMode", GetScreenMode(), "wince");
	g_config->flush();
	sound_activated = false;
	toolbar_drawn = false;
	hide_toolbar = true;
	Cls();
	g_scumm->_saveLoadSlot = 0;
	g_scumm->_saveLoadCompatible = false;
	g_scumm->_saveLoadFlag = 1;
	strcpy(g_scumm->_saveLoadName, "BOSS");
	g_scumm->saveState(g_scumm->_saveLoadSlot, g_scumm->_saveLoadCompatible);
	dynamicGXCloseInput();
	dynamicGXCloseDisplay();
	SDL_AudioQuit();
	memset(&se, 0, sizeof(se));
	se.cbSize = sizeof(se);
	se.hwnd = NULL;
	se.lpFile = TEXT("tasks.exe");
	se.lpVerb = TEXT("open");
	se.lpDirectory = TEXT("\\windows");
	ShellExecuteEx(&se);
	exit(1);
}

void action_skip() {
	OSystem_WINCE3* system;
	system = (OSystem_WINCE3*)g_scumm->_system;

	if (g_scumm->vm.cutScenePtr[g_scumm->vm.cutSceneStackPointer])
		system->addEventKeyPressed(g_scumm->_vars[g_scumm->VAR_CUTSCENEEXIT_KEY]);
	else
		system->addEventKeyPressed(g_scumm->_vars[g_scumm->VAR_TALKSTOP_KEY]);						
}

void do_hide(bool hide_state) {
	hide_toolbar = hide_state;
	if (hide_toolbar)
		RestoreScreenGeometry();
	else
		LimitScreenGeometry();
	Cls();
	toolbar_drawn = hide_toolbar;
	g_scumm->_system->update_screen();
}

void action_hide() {
	do_hide(!hide_toolbar);
}

void action_keyboard() {
	/*if (GetScreenMode()) {*/
		draw_keyboard = !draw_keyboard;
		if (!hide_toolbar)
			toolbar_drawn = false;
	/*}*/
}

void action_sound() {
	sound_activated = !sound_activated;
}

void action_cursoronoff() {
	hide_cursor = !hide_cursor;
}

void action_subtitleonoff() {
	g_scumm->_noSubtitles = !g_scumm->_noSubtitles;
}

void keypad_init() {
	static pAction actions[TOTAL_ACTIONS] =
	{ NULL, action_pause, action_save, action_quit, action_skip, action_hide, 
	  action_keyboard, action_sound, action_right_click, action_cursoronoff,
	  action_subtitleonoff, action_boss
	};
	
	GAPIKeysInitActions(actions);
	
}

void keypad_close() {
	dynamicGXCloseInput();	
}

void force_keyboard(bool activate) {

if (activate) {
	save_hide_toolbar = hide_toolbar;
	if (save_hide_toolbar) {
		// Display the keyboard while the dialog is running
		do_hide(false);
	}
	if (!draw_keyboard) {
		keyboard_override = true;
		draw_keyboard = true;
		toolbar_drawn = false;
	}
}
else {
	if (save_hide_toolbar) {
		do_hide(true);
		save_hide_toolbar = false;
	}
	if (keyboard_override) {
		keyboard_override = false;
		draw_keyboard = false;
		toolbar_drawn = false;
	}
}
}

/************* OSystem Main **********************/
OSystem *OSystem_WINCE3::create(int gfx_mode, bool full_screen) {
	const char *display_mode;
	OSystem_WINCE3 *syst = new OSystem_WINCE3();
	syst->_mode = gfx_mode;
	syst->_full_screen = full_screen;
	syst->_event.event_code = -1;
	syst->_start_time = GetTickCount();

	/* Retrieve the handle of this module */
	syst->hInst = GetModuleHandle(NULL);

	syst->hWnd = hWnd_Window;
	SetWindowLong(syst->hWnd, GWL_USERDATA, (long)syst);
	
	// Mini SDL init

	if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER)==-1) {		
	    exit(1);
	}

	reducePortraitGeometry();

	Cls();
	drawWait();

	// Set mode, portrait or landscape
	display_mode = g_config->get("DisplayMode", "wince");
	if (display_mode)
		SetScreenMode(atoi(display_mode));

	return syst;
}

OSystem *OSystem_WINCE3_create() {
	return OSystem_WINCE3::create(0, 0);
}

void OSystem_WINCE3::set_timer(int timer, int (*callback)(int)) {
	SetTimer(hWnd, 1, timer, NULL);
	timer_interval = timer;
	timer_callback = callback;
}

void OSystem_WINCE3::set_palette(const byte *colors, uint start, uint num) {
	const byte *b = colors;
	uint i;
	for(i=0;i!=num;i++) {
		SetPalEntry(i + start, b[0], b[1], b[2]);
		b += 4;
	}

	palette_update();

	num_of_dirty_square = MAX_NUMBER_OF_DIRTY_SQUARES;
}

void OSystem_WINCE3::load_gfx_mode() {
	force_full = true;

	_gfx_buf = (byte*)malloc((320 * 240) * sizeof(byte));	
	_overlay_buf = (byte*)malloc((320 * 240) * sizeof(uint16));
	_ms_backup = (byte*)malloc((40 * 40 * 3) * sizeof(byte));
}

void OSystem_WINCE3::unload_gfx_mode() {
 // FIXME: Free the _gfx_buf here
}

void OSystem_WINCE3::init_size(uint w, uint h) {
	load_gfx_mode();
	SetScreenGeometry(w, h);
	LimitScreenGeometry();
	_screenWidth = w;
	_screenHeight = h;
	num_of_dirty_square = MAX_NUMBER_OF_DIRTY_SQUARES;
}

void OSystem_WINCE3::copy_rect(const byte *buf, int pitch, int x, int y, int w, int h) {
	byte *dst;

	if (!hide_cursor && _mouse_drawn)
		undraw_mouse();

	AddDirtyRect(x, y, w, h);

	dst = _gfx_buf + y * 320 + x;
	do {
		memcpy(dst, buf, w);
		dst += 320;
		buf += pitch;
	} while (--h);

}

void OSystem_WINCE3::update_screen() {

	if (!hide_cursor)
		draw_mouse();

	if (_overlay_visible) {
		Set_565((int16*)_overlay_buf, 320, 0, 0, 320, 200);
		checkToolbar();
	}
	else {
		if (num_of_dirty_square >= MAX_NUMBER_OF_DIRTY_SQUARES) {
			Blt(_gfx_buf);  // global redraw
			num_of_dirty_square = 0;
		}
		else {
			int i;
			for (i=0; i<num_of_dirty_square; i++) {
				Blt_part(_gfx_buf + (320 * ds[i].y) + ds[i].x, (GetScreenMode() ? ds[i].x : ds[i].x * 3/4), ds[i].y, ds[i].w, ds[i].h, 320);
			}
			num_of_dirty_square = 0;
		}
	}
}

bool OSystem_WINCE3::show_mouse(bool visible) {
	if (_mouse_visible == visible)
		return visible;
	
	bool last = _mouse_visible;
	_mouse_visible = visible;

	return last;
}

// From X11 port

void OSystem_WINCE3::draw_mouse() {
	if (_mouse_drawn || !_mouse_visible)
		return;
	_mouse_drawn = true;

	int xdraw = _ms_cur.x - _ms_hotspot_x;
	int ydraw = _ms_cur.y - _ms_hotspot_y;
	int w = _ms_cur.w;
	int h = _ms_cur.h;
	int real_w;
	int real_h;
	int real_h_2;

	byte *dst;
	byte *dst2;
	const byte *buf = _ms_buf;
	byte *bak = _ms_backup;

	assert(w <= 40 && h <= 40);

	if (ydraw < 0) {
		real_h = h + ydraw;
		buf += (-ydraw) * w;
		ydraw = 0;
	} else {
		real_h = (ydraw + h) > 200 ? (200 - ydraw) : h;
	}
	if (xdraw < 0) {
		real_w = w + xdraw;
		buf += (-xdraw);
		xdraw = 0;
	} else {
		real_w = (xdraw + w) > 320 ? (320 - xdraw) : w;
	}

	dst = _gfx_buf + (ydraw * 320) + xdraw;
	dst2 = dst;

	if ((real_h == 0) || (real_w == 0)) {
		_mouse_drawn = false;
		return;
	}

	AddDirtyRect(xdraw, ydraw, real_w, real_h);
	_ms_old.x = xdraw;
	_ms_old.y = ydraw;
	_ms_old.w = real_w;
	_ms_old.h = real_h;

	real_h_2 = real_h;
	while (real_h_2 > 0) {
		memcpy(bak, dst, real_w);
		bak += 40;
		dst += 320;
		real_h_2--;
	}
	while (real_h > 0) {
		int width = real_w;
		while (width > 0) {
			byte color = *buf;
			if (color != 0xFF) {
				*dst2 = color;
			}
			buf++;
			dst2++;
			width--;
		}
		buf += w - real_w;
		dst2 += 320 - real_w;
		real_h--;
	}
}

void OSystem_WINCE3::undraw_mouse() {
	if (!_mouse_drawn)
		return;
	_mouse_drawn = false;

	int old_h = _ms_old.h;

	AddDirtyRect(_ms_old.x, _ms_old.y, _ms_old.w, _ms_old.h);

	byte *dst = _gfx_buf + (_ms_old.y * 320) + _ms_old.x;
	byte *bak = _ms_backup;

	while (old_h > 0) {
		memcpy(dst, bak, _ms_old.w);
		bak += 40;
		dst += 320;
		old_h--;
	}
}

	
void OSystem_WINCE3::set_mouse_pos(int x, int y) {
	if (x != _ms_cur.x || y != _ms_cur.y) {
		_ms_cur.x = x;
		_ms_cur.y = y;
	}
}
	
void OSystem_WINCE3::set_mouse_cursor(const byte *buf, uint w, uint h, int hotspot_x, int hotspot_y) {
	_ms_cur.w = w;
	_ms_cur.h = h;

	_ms_hotspot_x = hotspot_x;
	_ms_hotspot_y = hotspot_y;

	_ms_buf = (byte*)buf;

	// Refresh mouse cursor

	if (!hide_cursor) {
		undraw_mouse();
		draw_mouse();
	}
}
	
void OSystem_WINCE3::set_shake_pos(int shake_pos) {;}
		
uint32 OSystem_WINCE3::get_msecs() {
	return GetTickCount() - _start_time;
}
	
void OSystem_WINCE3::delay_msecs(uint msecs) {
	//handleMessage();
	Sleep(msecs);
}
	
void *OSystem_WINCE3::create_thread(ThreadProc *proc, void *param) {
	// needed for emulated MIDI support (Sam'n'Max)
	HANDLE handle;
	handle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)proc, param, 0, NULL);
	SetThreadPriority(handle, THREAD_PRIORITY_LOWEST);
	return handle;
}

int mapKey(int key, byte mod)
{
	if (key>=VK_F1 && key<=VK_F9) {
		return key - VK_F1 + 315;
	}
	return key;
}
	
bool OSystem_WINCE3::poll_event(Event *event) {

	for (;;) {
		MSG msg;

		_event.event_code = -1;

		if (!PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			return false;

		if (msg.message==WM_QUIT) {
			terminated=true;
			do_quit();
			return false;
		}

		TranslateMessage(&msg);
		DispatchMessage(&msg);

		*event = _event;

		return true;
	}
	
	return false;
}
	
void own_soundProc(void *buffer, byte *samples, int len) {

	(*real_soundproc)(buffer, samples, len);

	if (!sound_activated)
		memset(samples, 0, len);
}

bool OSystem_WINCE3::set_sound_proc(void *param, SoundProc *proc, byte format) {
	SDL_AudioSpec desired;

	/* only one format supported at the moment */

	real_soundproc = proc;
	desired.freq = SAMPLES_PER_SEC;
	desired.format = AUDIO_S16SYS;
	desired.channels = 2;
	desired.samples = 128;
	desired.callback = own_soundProc;
	desired.userdata = param;
	if (SDL_OpenAudio(&desired, NULL) != 0) {
		return false;
	}
	SDL_PauseAudio(0);
	return true;
}

/* Hotswap graphics modes */
void OSystem_WINCE3::get_320x200_image(byte *buf) {;}
void OSystem_WINCE3::hotswap_gfx_mode() {;}
uint32 OSystem_WINCE3::property(int param, Property *value) {
	switch(param) {

	case PROP_TOGGLE_FULLSCREEN:
		return 1;

	case PROP_SET_WINDOW_CAPTION:
		return 1;

	case PROP_OPEN_CD:		
		break;

	case PROP_SET_GFX_MODE:
		return 1;

	case PROP_SHOW_DEFAULT_CURSOR:
		break;

	case PROP_GET_SAMPLE_RATE:
		return SAMPLES_PER_SEC;
	}

	return 0;
}
		
void OSystem_WINCE3::quit() {
	unload_gfx_mode();		
	do_quit();
}

/* CDRom Audio */
void OSystem_WINCE3::stop_cdrom() {;}
void OSystem_WINCE3::play_cdrom(int track, int num_loops, int start_frame, int end_frame) {
	/* Reset sync count */
	g_scumm->_vars[g_scumm->VAR_MI1_TIMER] = 0;
}

bool OSystem_WINCE3::poll_cdrom() {return 0;}
void OSystem_WINCE3::update_cdrom() {;}

void ScummDebugger::attach(Scumm *s) {;}

/* Mutex stuff */
void* OSystem_WINCE3::create_mutex() {
	return (void*)CreateMutex(NULL, FALSE, NULL);
}
void OSystem_WINCE3::lock_mutex(void *handle) {
	WaitForSingleObject((HANDLE)handle, INFINITE);
}

void OSystem_WINCE3::unlock_mutex(void *handle) {
	ReleaseMutex((HANDLE)handle);
}

void OSystem_WINCE3::delete_mutex(void *handle) {
	CloseHandle((HANDLE)handle);
}

/* Overlay stuff */

void OSystem_WINCE3::show_overlay() {
	undraw_mouse();
	_overlay_visible = true;
	clear_overlay();

}

void OSystem_WINCE3::hide_overlay() {
	undraw_mouse();
	_overlay_visible = false;
	toolbar_drawn = false;
	num_of_dirty_square = MAX_NUMBER_OF_DIRTY_SQUARES;
}

void OSystem_WINCE3::clear_overlay() {

	if (!_overlay_visible)
		return;

	Blt(_gfx_buf);
}

void OSystem_WINCE3::grab_overlay(int16 *buf, int pitch) {
	//FIXME : it'd be better with a REAL surface :)
	//Blt(_gfx_buf);
	Get_565(_gfx_buf, buf, pitch, 0, 0, 320, 200);
	memcpy(_overlay_buf, buf, 320 * 200 * sizeof(int16));
}

void OSystem_WINCE3::copy_rect_overlay(const int16 *buf, int pitch, int x, int y, int w, int h) {
	int i;

	UBYTE *dest = _overlay_buf;
	dest += y * 320  * sizeof(int16);
	for (i=0; i<h; i++) {
		memcpy(dest + (x * sizeof(int16)), buf, w * 2);
		dest += 320 * sizeof(int16);
		buf += pitch;
	}	
}

void OSystem_WINCE3::move_screen(int dx, int dy, int height) {
	// FIXME : to be implemented
}

/* NECESSARY operators redefinition */

void *operator new(size_t size) {
	return calloc(size, 1);
}

void operator delete(void *ptr) {
	free(ptr);
}
