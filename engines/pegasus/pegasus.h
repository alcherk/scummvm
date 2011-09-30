/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * Additional copyright for this file:
 * Copyright (C) 1995-1997 Presto Studios, Inc.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef PEGASUS_H
#define PEGASUS_H

#include "common/list.h"
#include "common/macresman.h"
#include "common/scummsys.h"
#include "common/system.h"
#include "common/rect.h"
#include "common/util.h"

#include "engines/engine.h"

#include "pegasus/graphics.h"
#include "pegasus/hotspot.h"
#include "pegasus/input.h"
#include "pegasus/notification.h"
#include "pegasus/items/autodragger.h"
#include "pegasus/items/inventory.h"
#include "pegasus/items/itemdragger.h"
#include "pegasus/neighborhood/neighborhood.h"

namespace Video {
	class SeekableVideoDecoder;
}

namespace Pegasus {

class PegasusConsole;
struct PegasusGameDescription;
class SoundManager;
class GraphicsManager;
class Idler;
class Cursor;
class TimeBase;
class GameMenu;
class InventoryItem;
class BiochipItem;
class Neighborhood;

class PegasusEngine : public ::Engine, public InputHandler, public NotificationManager {
friend class InputHandler;

public:
	PegasusEngine(OSystem *syst, const PegasusGameDescription *gamedesc);
	virtual ~PegasusEngine();

	// Engine stuff
	const PegasusGameDescription *_gameDescription;
	bool hasFeature(EngineFeature f) const;
	GUI::Debugger *getDebugger();
	bool canLoadGameStateCurrently() { return _loadAllowed && !isDemo(); }
	bool canSaveGameStateCurrently() { return _saveAllowed && !isDemo(); }
	Common::Error loadGameState(int slot);
	Common::Error saveGameState(int slot, const Common::String &desc);

	// Base classes
	GraphicsManager *_gfx;
	Common::MacResManager *_resFork;
	Cursor *_cursor;

	// Menu
	void useMenu(GameMenu *menu);
	bool checkGameMenu();

	// Misc.
	bool isDemo() const;
	void addIdler(Idler *idler);
	void removeIdler(Idler *idler);
	void addTimeBase(TimeBase *timeBase);
	void removeTimeBase(TimeBase *timeBase);
	void delayShell(TimeValue time, TimeScale scale);
	void resetIntroTimer();
	void refreshDisplay();
	bool playerAlive();
	void processShell();
	void checkCallBacks();
	void createInterface();
	void setGameMode(const tGameMode);
	tGameMode getGameMode() const { return _gameMode; }

	// Energy
	void setLastEnergyValue(const int32 value) { _savedEnergyValue = value; }
	int32 getSavedEnergyValue() { return _savedEnergyValue; }

	// Death
	void setEnergyDeathReason(const tDeathReason reason) { _deathReason = reason; } 
	tDeathReason getEnergyDeathReason() { return _deathReason; }
	void resetEnergyDeathReason();
	void die(const tDeathReason);

	// Volume
	uint16 getSoundFXLevel() { return _FXLevel; }
	void setSoundFXLevel(uint16);
	uint16 getAmbienceLevel() { return _ambientLevel; }
	void setAmbienceLevel(uint16);

	// Items
	bool playerHasItem(const Item *);
	bool playerHasItemID(const tItemID);
	void checkFlashlight();
	bool itemInLocation(const tItemID, const tNeighborhoodID, const tRoomID, const tDirectionConstant);

	// Inventory Items
	InventoryItem *getCurrentInventoryItem();
	bool itemInInventory(InventoryItem *);
	bool itemInInventory(tItemID);
	Inventory *getItemsInventory() { return &_items; }
	tInventoryResult addItemToInventory(InventoryItem *);
	void removeAllItemsFromInventory();
	tInventoryResult removeItemFromInventory(InventoryItem *);
	uint32 countInventoryItems() { return _items.getNumItems(); }

	// Biochips
	BiochipItem *getCurrentBiochip();
	bool itemInBiochips(BiochipItem *);
	bool itemInBiochips(tItemID);
	Inventory *getBiochipsInventory() { return &_biochips; }
	void removeAllItemsFromBiochips();
	tInventoryResult addItemToBiochips(BiochipItem *);

	// AI
	Common::String getBriefingMovie();
	Common::String getEnvScanMovie();
	uint getNumHints();
	Common::String getHintMovie(uint);
	bool canSolve();
	void prepareForAIHint(const Common::String &);
	void cleanUpAfterAIHint(const Common::String &);

	// Neighborhood
	void jumpToNewEnvironment(const tNeighborhoodID, const tRoomID, const tDirectionConstant);
	tNeighborhoodID getCurrentNeighborhoodID() const;

	// Dragging
	void dragItem(const Input &, Item *, tDragType);
	bool isDragging() const { return _dragType != kDragNoDrag; }
	tDragType getDragType() const { return _dragType; }
	Item *getDraggingItem() const { return _draggingItem; }
	void dragTerminated(const Input &);
	void autoDragItemIntoRoom(Item *, Sprite *);
	void autoDragItemIntoInventory(Item *, Sprite*);

	// Save/Load
	void makeContinuePoint();
	bool swapSaveAllowed(bool allow) {
		bool old = _saveAllowed;
		_saveAllowed = allow;
		return old;
	}
	bool swapLoadAllowed(bool allow) {
		bool old = _loadAllowed;
		_loadAllowed = allow;
		return old;
	}

protected:
	Common::Error run();
	void pauseEngineIntern(bool pause);

	Notification _shellNotification;
	virtual void receiveNotification(Notification *notification, const tNotificationFlags flags);

	void handleInput(const Input &input, const Hotspot *cursorSpot);
	virtual bool isClickInput(const Input &, const Hotspot *);
	virtual tInputBits getClickFilter();

	void clickInHotspot(const Input &, const Hotspot *);
	void activateHotspots(void);

	void updateCursor(const Common::Point, const Hotspot *);
	bool wantsCursor();

private:
	// Console
	PegasusConsole *_console;

	// Intro
	void runIntro();
	bool detectOpeningClosingDirectory();
	Common::String _introDirectory;

	// Idlers
	Common::List<Idler *> _idlers;
	void giveIdleTime();

	// Items
	void createItems();
	void createItem(tItemID itemID, tNeighborhoodID neighborhoodID, tRoomID roomID, tDirectionConstant direction);
	Inventory _items;
	Inventory _biochips;
	tItemID _currentItemID;
	tItemID _currentBiochipID;

	// TimeBases
	Common::List<TimeBase *> _timeBases;

	// Save/Load
	bool loadFromStream(Common::ReadStream *stream);
	bool writeToStream(Common::WriteStream *stream, int saveType);
	void loadFromContinuePoint();
	Common::ReadStream *_continuePoint;
	bool _saveAllowed, _loadAllowed; // It's so nice that this was in the original code already :P

	// Misc.
	Hotspot _returnHotspot;
	InputHandler *_savedHandler;
	void showLoadDialog();
	void showTempScreen(const Common::String &fileName);
	bool playMovieScaled(Video::SeekableVideoDecoder *video, uint16 x, uint16 y);
	void throwAwayEverything();
	void shellGameInput(const Input &input, const Hotspot *cursorSpot);

	// Menu
	GameMenu *_gameMenu;
	void doGameMenuCommand(const tGameMenuCommand);
	void doInterfaceOverview();
	ScreenDimmer _screenDimmer;
	void pauseMenu(bool menuUp);

	// Energy
	int32 _savedEnergyValue;

	// Death
	tDeathReason _deathReason;
	void doDeath();

	// Neighborhood
	Neighborhood *_neighborhood;
	void useNeighborhood(Neighborhood *neighborhood);
	void performJump(const tNeighborhoodID start);
	void startNewGame();
	void startNeighborhood();
	void makeNeighborhood(tNeighborhoodID, Neighborhood *&);

	// Sound
	uint16 _ambientLevel;
	uint16 _FXLevel;

	// Game Mode
	tGameMode _gameMode;
	bool _switchModesSync;
	void switchGameMode(const tGameMode, const tGameMode);
	bool canSwitchGameMode(const tGameMode, const tGameMode);

	// Dragging
	ItemDragger _itemDragger;
	Item *_draggingItem;
	Sprite *_draggingSprite;
	tDragType _dragType;
	AutoDragger _autoDragger;

	// Interface
	void toggleInventoryDisplay();
	void toggleBiochipDisplay();
	void raiseInventoryDrawer();
	void raiseBiochipDrawer();
	void lowerInventoryDrawer();
	void lowerBiochipDrawer();
	void raiseInventoryDrawerSync();
	void raiseBiochipDrawerSync();
	void lowerInventoryDrawerSync();
	void lowerBiochipDrawerSync();
	void showInfoScreen();
	void hideInfoScreen();
	void toggleInfo();
	Movie _bigInfoMovie, _smallInfoMovie;
};

} // End of namespace Pegasus

#endif
