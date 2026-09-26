#include "guis/Gui_dArkOSen.h"

#include "components/OptionListComponent.h"
#include "guis/GuiMsgBox.h"
#include "views/ViewController.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "Window.h"

#include <pugixml/src/pugixml.hpp>

#include <algorithm>
#include <fstream>
#include <sstream>

static const std::string CFG = "/etc/emulationstation/es_systems.cfg";
static const std::string FSTAB = "/etc/fstab";
static const std::string TOOLS_MOUNT = "/opt/system/Tools";

Gui_dArkOSen::Gui_dArkOSen(Window* window)
	: GuiSettings(window, _("REASSIGN TO SD1").c_str())
{
	initializeMenu();
}

Gui_dArkOSen::~Gui_dArkOSen()
{
}

void Gui_dArkOSen::initializeMenu()
{
	addEntry(_("MANUAL SELECTION"), true, [this] { openManualSelection(); });

	addEntry(_("AUTOMATED SCAN"), true, [this]
	{
		mWindow->pushGui(new GuiMsgBox(mWindow, _("SCAN /roms/ FOR CHANGES?"), _("YES"),
			[this] { runAutomatedScan(); }, _("NO"), nullptr));
	});

	addEntry(_("UNDO ALL CHANGES"), true, [this]
	{
		mWindow->pushGui(new GuiMsgBox(mWindow, _("REVERT ALL SYSTEMS TO SD2?"), _("YES"),
			[this] { undoAllChanges(); }, _("NO"), nullptr));
	});
}

std::string Gui_dArkOSen::getFolderName(const std::string& path)
{
	std::string p = path;
	while (!p.empty() && p.back() == '/')
		p.pop_back();

	auto pos = p.find_last_of('/');
	return (pos == std::string::npos) ? p : p.substr(pos + 1);
}

bool Gui_dArkOSen::hasMatchingRom(const std::string& dir, const std::string& extensions)
{
	if (!Utils::FileSystem::isDirectory(dir))
		return false;

	std::vector<std::string> exts;
	std::istringstream iss(extensions);
	std::string tok;
	while (iss >> tok)
	{
		if (!tok.empty() && tok[0] == '.')
			tok.erase(0, 1);
		exts.push_back(tok);
	}

	for (auto& file : Utils::FileSystem::getDirContent(dir, false))
	{
		std::string fext = Utils::FileSystem::getExtension(file, false);
		if (std::find(exts.cbegin(), exts.cend(), fext) != exts.cend())
			return true;
	}
	return false;
}

bool Gui_dArkOSen::getToolsOnSD1()
{
	std::ifstream f(FSTAB);
	std::string line;
	while (std::getline(f, line))
	{
		std::istringstream iss(line);
		std::string src, dst;
		if (!(iss >> src >> dst))
			continue;

		if (dst != TOOLS_MOUNT)
			continue;

		if (src == "/roms/tools")
			return true;
		if (src == "/roms2/tools")
			return false;
	}
	return false;
}

void Gui_dArkOSen::setToolsLocation(bool toSD1)
{
	mWindow->renderLoadingScreen(_("PLEASE WAIT..."));

	system(("sudo umount " + TOOLS_MOUNT + " 2>/dev/null").c_str());

	if (toSD1)
	{
		system(("sudo mount -B /roms/tools " + TOOLS_MOUNT).c_str());
		system("sudo sed -i '/roms2\\/tools/s//roms\\/tools/' /etc/fstab");
	}
	else
	{
		system(("sudo mount -B /roms2/tools " + TOOLS_MOUNT).c_str());
		system("sudo sed -i '/roms\\/tools/s//roms2\\/tools/' /etc/fstab");
	}

	system("sudo systemctl daemon-reload");
}

void Gui_dArkOSen::reload()
{
	ViewController::get()->reloadAll(mWindow);
}

std::vector<Gui_dArkOSen::SystemEntry> Gui_dArkOSen::loadManageableSystems()
{
	std::vector<SystemEntry> result;

	pugi::xml_document doc;
	if (!doc.load_file(CFG.c_str()))
		return result;

	pugi::xml_node systemList = doc.child("systemList");
	if (!systemList)
		return result;

	std::vector<std::string> seenPaths;

	for (pugi::xml_node system = systemList.child("system"); system; system = system.next_sibling("system"))
	{
		std::string fullname = system.child("fullname").text().get();
		std::string path = system.child("path").text().get();

		if (fullname.empty() || path.empty())
			continue;

		if (std::find(seenPaths.cbegin(), seenPaths.cend(), path) != seenPaths.cend())
			continue;

		SystemEntry entry;
		entry.fullname = fullname;
		entry.path = path;
		entry.isPorts = (path.find("/ports/") != std::string::npos);

		std::string folder = getFolderName(path);

		if (path.find("/roms2/") != std::string::npos)
		{
			if (!Utils::FileSystem::isDirectory("/roms/" + folder))
				continue;
			entry.onSD1 = false;
		}
		else if (path.find("/roms/") != std::string::npos)
		{
			if (!Utils::FileSystem::isDirectory("/roms/" + folder))
				continue;
			entry.onSD1 = true;
		}
		else
		{
			continue;
		}

		seenPaths.push_back(path);
		result.push_back(entry);
	}

	return result;
}

void Gui_dArkOSen::openManualSelection()
{
	bool toolsOnSD1 = getToolsOnSD1();
	std::vector<SystemEntry> systems = loadManageableSystems();

	// ports pinned right after Tools, rest alphabetical by fullname
	std::stable_sort(systems.begin(), systems.end(), [](const SystemEntry& a, const SystemEntry& b)
	{
		if (a.isPorts != b.isPorts)
			return a.isPorts;
		return Utils::String::toLower(a.fullname) < Utils::String::toLower(b.fullname);
	});

	auto s = new GuiSettings(mWindow, _("MANUAL SELECTION"));

	auto list = std::make_shared<OptionListComponent<std::string>>(mWindow, _("MANUAL SELECTION"), true);
	list->add(_("Tools"), "TOOLS", toolsOnSD1);
	for (auto& entry : systems)
		list->add(entry.fullname, entry.path, entry.onSD1);

	s->addWithLabel(_("SYSTEMS"), list);

	auto moved = std::make_shared<std::vector<std::string>>();
	auto reverted = std::make_shared<std::vector<std::string>>();

	s->addSaveFunc([this, list, systems, toolsOnSD1, moved, reverted]
	{
		std::vector<std::string> sel = list->getSelectedObjects();

		bool toolsSelected = std::find(sel.cbegin(), sel.cend(), "TOOLS") != sel.cend();
		if (toolsSelected && !toolsOnSD1)
		{
			setToolsLocation(true);
			moved->push_back("Tools");
		}
		else if (!toolsSelected && toolsOnSD1)
		{
			setToolsLocation(false);
			reverted->push_back("Tools");
		}

		pugi::xml_document doc;
		bool loaded = doc.load_file(CFG.c_str());
		pugi::xml_node systemList = loaded ? doc.child("systemList") : pugi::xml_node();
		if (!systemList)
			return;

		bool changed = false;

		for (auto& entry : systems)
		{
			bool selected = std::find(sel.cbegin(), sel.cend(), entry.path) != sel.cend();

			std::string newPath;
			if (selected && !entry.onSD1)
				newPath = Utils::String::replace(entry.path, "/roms2/", "/roms/");
			else if (!selected && entry.onSD1)
				newPath = Utils::String::replace(entry.path, "/roms/", "/roms2/");
			else
				continue;

			for (pugi::xml_node system = systemList.child("system"); system; system = system.next_sibling("system"))
			{
				pugi::xml_node pathNode = system.child("path");
				if (pathNode && std::string(pathNode.text().get()) == entry.path)
					pathNode.text().set(newPath.c_str());
			}

			if (selected)
				moved->push_back(entry.fullname);
			else
				reverted->push_back(entry.fullname);

			changed = true;
		}

		if (changed)
		{
			Utils::FileSystem::copyFile(CFG, CFG + ".bak");
			doc.save_file(CFG.c_str());
		}
	});

	s->onFinalize([this, moved, reverted]
	{
		showSummaryAndReload(*moved, *reverted);
	});

	mWindow->pushGui(s);
}

void Gui_dArkOSen::runAutomatedScan()
{
	pugi::xml_document doc;
	if (!doc.load_file(CFG.c_str()))
		return;

	pugi::xml_node systemList = doc.child("systemList");
	if (!systemList)
		return;

	std::vector<std::string> moved, reverted;
	std::vector<std::string> seenMoved, seenReverted;

	// --- /roms2/ systems with ROMs now under /roms/ and none left under /roms2/ -> migrate ---
	for (pugi::xml_node system = systemList.child("system"); system; system = system.next_sibling("system"))
	{
		std::string fullname = system.child("fullname").text().get();
		std::string extensions = system.child("extension").text().get();
		std::string path = system.child("path").text().get();

		if (fullname.empty() || extensions.empty() || path.empty())
			continue;
		if (path.find("/roms2/") == std::string::npos)
			continue;
		if (std::find(seenMoved.cbegin(), seenMoved.cend(), path) != seenMoved.cend())
			continue;

		std::string folder = getFolderName(path);
		std::string primaryDir = "/roms/" + folder;
		std::string oldDir = "/roms2/" + folder;

		if (!Utils::FileSystem::isDirectory(primaryDir))
			continue;

		bool foundPrimary = hasMatchingRom(primaryDir, extensions);
		bool foundOld = hasMatchingRom(oldDir, extensions);

		if (foundPrimary && !foundOld)
		{
			std::string newPath = Utils::String::replace(path, "/roms2/", "/roms/");
			system.child("path").text().set(newPath.c_str());
			moved.push_back(fullname);
			seenMoved.push_back(path);
		}
	}

	// --- /roms/ systems with no ROMs left -> revert to /roms2/ ---
	for (pugi::xml_node system = systemList.child("system"); system; system = system.next_sibling("system"))
	{
		std::string fullname = system.child("fullname").text().get();
		std::string extensions = system.child("extension").text().get();
		std::string path = system.child("path").text().get();

		if (fullname.empty() || extensions.empty() || path.empty())
			continue;
		if (path.find("/roms/") == std::string::npos)
			continue;
		if (std::find(seenReverted.cbegin(), seenReverted.cend(), path) != seenReverted.cend())
			continue;

		if (!hasMatchingRom(path, extensions))
		{
			std::string newPath = Utils::String::replace(path, "/roms/", "/roms2/");
			system.child("path").text().set(newPath.c_str());
			reverted.push_back(fullname);
			seenReverted.push_back(path);
		}
	}

	if (moved.empty() && reverted.empty())
	{
		mWindow->pushGui(new GuiMsgBox(mWindow, _("NOTHING WAS MODIFIED."), _("OK")));
		return;
	}

	Utils::FileSystem::copyFile(CFG, CFG + ".bak");
	doc.save_file(CFG.c_str());

	showSummaryAndReload(moved, reverted);
}

void Gui_dArkOSen::undoAllChanges()
{
	mWindow->renderLoadingScreen(_("RESTORING ALL SYSTEMS TO /roms2/."));

	pugi::xml_document doc;
	if (!doc.load_file(CFG.c_str()))
		return;

	pugi::xml_node systemList = doc.child("systemList");
	if (!systemList)
		return;

	Utils::FileSystem::copyFile(CFG, CFG + ".bak");

	for (pugi::xml_node system = systemList.child("system"); system; system = system.next_sibling("system"))
	{
		std::string path = system.child("path").text().get();
		if (path.empty() || path.find("/roms/") == std::string::npos)
			continue;

		std::string newPath = Utils::String::replace(path, "/roms/", "/roms2/");
		system.child("path").text().set(newPath.c_str());
	}

	doc.save_file(CFG.c_str());

	reload();
}

void Gui_dArkOSen::showSummaryAndReload(const std::vector<std::string>& moved, const std::vector<std::string>& reverted)
{
	std::string msg;

	if (!moved.empty())
	{
		msg += _("MOVED TO SD1:");
		msg += "\n";
		for (auto& name : moved)
			msg += "  - " + name + "\n";
	}

	if (!reverted.empty())
	{
		if (!msg.empty())
			msg += "\n";
		msg += _("REVERTED TO SD2:");
		msg += "\n";
		for (auto& name : reverted)
			msg += "  - " + name + "\n";
	}

	if (msg.empty())
	{
		mWindow->pushGui(new GuiMsgBox(mWindow, _("NOTHING WAS MODIFIED."), _("OK")));
		return;
	}

	mWindow->pushGui(new GuiMsgBox(mWindow, msg, _("OK"), [this] { reload(); }));
}
