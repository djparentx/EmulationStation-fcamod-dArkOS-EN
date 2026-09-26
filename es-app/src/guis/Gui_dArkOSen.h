#pragma once
#ifndef ES_APP_GUIS_GUI_DARKOSEN_H
#define ES_APP_GUIS_GUI_DARKOSEN_H

#include "GuiSettings.h"
#include <string>
#include <vector>

class Window;

class Gui_dArkOSen : public GuiSettings
{
public:
	Gui_dArkOSen(Window* window);
	~Gui_dArkOSen();

private:
	// one <system> entry from es_systems.cfg relevant to SD1/SD2 management
	struct SystemEntry
	{
		std::string fullname;
		std::string path;    // current <path> text as it appears in the cfg
		bool onSD1;           // true if path is currently under /roms/, false if /roms2/
		bool isPorts;         // true if path contains "/ports/" - pinned right after TOOLS in list order
	};

	void initializeMenu();

	// the 3 rows
	void openManualSelection();
	void runAutomatedScan();
	void undoAllChanges();

	// shared helpers
	std::vector<SystemEntry> loadManageableSystems(); // only systems whose /roms/<folder> dir exists, mirrors SystemsMenu()'s scan
	bool getToolsOnSD1();
	void setToolsLocation(bool toSD1);
	void reload(); // ViewController::get()->reloadAll(mWindow), replaces the script's killall/restart

	std::string getFolderName(const std::string& path);
	bool hasMatchingRom(const std::string& dir, const std::string& extensions);
	void showSummaryAndReload(const std::vector<std::string>& moved, const std::vector<std::string>& reverted);
};

#endif // ES_APP_GUIS_GUI_DARKOSEN_H
