/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is _globals.FRee software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the _globals.FRee Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the _globals.FRee Software
 * Foundation, Inc., 51 _globals.FRanklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "common/scummsys.h"
#include "common/events.h"
#include "hopkins/menu.h"
#include "hopkins/files.h"
#include "hopkins/hopkins.h"
#include "hopkins/globals.h"
#include "hopkins/events.h"
#include "hopkins/graphics.h"
#include "hopkins/sound.h"

namespace Hopkins {

void MenuManager::setParent(HopkinsEngine *vm) {
	_vm = vm;
}

int MenuManager::MENU() {
	byte *spriteData = NULL; 
	signed int menuIndex;
	int v3; 
	int v4; 
	signed int v6;
	signed __int16 v7;
	signed __int16 v8;
	signed __int16 v9;
	signed __int16 v10;
	__int16 v11;
	signed int v12;

	v6 = 0;
	while (!g_system->getEventManager()->shouldQuit()) {
		_vm->_globals.FORET = 0;
		_vm->_eventsManager.CASSE = 0;
		_vm->_globals.DESACTIVE_INVENT = 1;
		_vm->_globals.FLAG_VISIBLE = 0;
		_vm->_globals.SORTIE = 0;

		for (int idx = 0; idx < 31; ++idx)
			_vm->_globals.INVENTAIRE[idx] = 0;
    
		memset(_vm->_globals.SAUVEGARDE, 0, 2000);
		_vm->_objectsManager.AJOUTE_OBJET(14);
		v7 = 0;
		v8 = 0;
		v9 = 0;
		v10 = 0;
		v11 = 0;


		if (_vm->_globals.FR == 0)
			_vm->_graphicsManager.LOAD_IMAGE("MENUAN");
		else if (_vm->_globals.FR == 1)
			_vm->_graphicsManager.LOAD_IMAGE("MENUFR");
		else if (_vm->_globals.FR == 2)
			_vm->_graphicsManager.LOAD_IMAGE("MENUES");
    
		_vm->_graphicsManager.FADE_INW();
		if (_vm->_globals.FR == 0)
			FileManager::CONSTRUIT_SYSTEM("MENUAN.SPR");
		if (_vm->_globals.FR == 1)
			FileManager::CONSTRUIT_SYSTEM("MENUFR.SPR");
		if (_vm->_globals.FR == 2)
			FileManager::CONSTRUIT_SYSTEM("MENUES.SPR");
    
		spriteData = _vm->_objectsManager.CHARGE_SPRITE(_vm->_globals.NFICHIER);
		_vm->_eventsManager.MOUSE_ON();
		_vm->_eventsManager.CHANGE_MOUSE(0);
		_vm->_eventsManager.btsouris = 0;
		_vm->_eventsManager.souris_n = 0;
    
		for (;;) {
			for (;;) {
				_vm->_soundManager.WSOUND(28);
				v12 = 0;

				do {
					if (g_system->getEventManager()->shouldQuit())
						return -1;

					menuIndex = 0;
					v3 = _vm->_eventsManager.XMOUSE();
					v4 = _vm->_eventsManager.YMOUSE();
          
					if ((unsigned int)(v3 - 232) <= 176) {
						if ((unsigned int)(v4 - 261) <= 23)
							menuIndex = 1;
						if ((unsigned int)(v4 - 293) <= 23)
							menuIndex = 2;
						if ((unsigned int)(v4 - 325) <= 22)
							menuIndex = 3;
						if ((unsigned int)(v4 - 356) <= 23)
							menuIndex = 4;
            
						if ((unsigned int)(v4 - 388) <= 23)
							menuIndex = 5;
					}
          
					switch (menuIndex) {
					case 0:
						v11 = 0;
						v10 = 0;
						v9 = 0;
						v8 = 0;
						v7 = 0;
						break;
					case 1:
						v11 = 1;
						v10 = 0;
						v9 = 0;
						v8 = 0;
						v7 = 0;
						break;
					case 2:
						v11 = 0;
						v10 = 1;
						v9 = 0;
						v8 = 0;
						v7 = 0;
						break;
					case 3:
						v11 = 0;
						v10 = 0;
						v9 = 1;
						v8 = 0;
						v7 = 0;
						break;
					case 4:
						v11 = 0;
						v10 = 0;
						v9 = 0;
						v8 = 1;
						v7 = 0;
						break;
					case 5:
						v11 = 0;
						v10 = 0;
						v9 = 0;
						v8 = 0;
						v7 = 1;
					default:
						break;
					}
          
					_vm->_graphicsManager.AFFICHE_SPEED(spriteData, 230, 259, v11);
					_vm->_graphicsManager.AFFICHE_SPEED(spriteData, 230, 291, v10 + 2);
					_vm->_graphicsManager.AFFICHE_SPEED(spriteData, 230, 322, v9 + 4);
					_vm->_graphicsManager.AFFICHE_SPEED(spriteData, 230, 354, v8 + 6);
					_vm->_graphicsManager.AFFICHE_SPEED(spriteData, 230, 386, v7 + 8);
					_vm->_graphicsManager.VBL();
          
					if (_vm->_eventsManager.BMOUSE() == 1 && menuIndex > 0)
						v12 = 1;
				} while (v12 != 1);
        
				if (menuIndex == 1) {
					_vm->_graphicsManager.AFFICHE_SPEED(spriteData, 230, 259, 10);
					_vm->_graphicsManager.VBL();
					_vm->_eventsManager.delay(200);
					v6 = 1;
				}
				if (menuIndex != 2)
					break;

				_vm->_graphicsManager.AFFICHE_SPEED(spriteData, 230, 291, 11);
				_vm->_graphicsManager.VBL();
				_vm->_eventsManager.delay(200);
        
				_vm->_globals.SORTIE = -1;
				CHARGE_PARTIE();
        
				if (_vm->_globals.SORTIE != -1) {
					v6 = _vm->_globals.SORTIE;
					break;
				}
				_vm->_globals.SORTIE = 0;
			}
      
			if (menuIndex != 3)
				break;
      
			_vm->_graphicsManager.AFFICHE_SPEED(spriteData, 230, 322, 12);
			_vm->_graphicsManager.VBL();
			_vm->_eventsManager.delay(200);
      
			CHOICE_OPTION();
		}
		if (menuIndex == 4) {
			_vm->_graphicsManager.AFFICHE_SPEED(spriteData, 230, 354, 13);
			_vm->_graphicsManager.VBL();
			_vm->_eventsManager.delay(200);
			_vm->INTRORUN();
			continue;
		}

		if ( menuIndex == 5 ) {
			_vm->_graphicsManager.AFFICHE_SPEED(spriteData, 230, 386, 14);
			_vm->_graphicsManager.VBL();
			_vm->_eventsManager.delay(200);
			v6 = -1;
		}
		break;
	}
  
	_vm->_globals.LIBERE_FICHIER(spriteData);
	_vm->_globals.DESACTIVE_INVENT = 0;
	_vm->_globals.FLAG_VISIBLE = 0;
	_vm->_graphicsManager.FADE_OUTW();
	return v6;
}

void MenuManager::CHOICE_OPTION() {
	warning("CHOICE_OPTION");
}

void MenuManager::CHARGE_PARTIE() {
	warning("CHARGE_PARTIE");
}

} // End of namespace Hopkins
