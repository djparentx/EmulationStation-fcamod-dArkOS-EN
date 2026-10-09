#include "guis/Gui_dArkOSen.h"

#include "components/OptionListComponent.h"
#include "guis/GuiMsgBox.h"
#include "views/ViewController.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "Window.h"
#include "Log.h"
#include "Scripting.h"
#include "platform.h"

#include <pugixml/src/pugixml.hpp>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <regex>
#include <cstdio>
#include <ctime>
#include <unistd.h>
#include <sys/wait.h>

static const std::string CFG = "/etc/emulationstation/es_systems.cfg";
static const std::string FSTAB = "/etc/fstab";
static const std::string TOOLS_MOUNT = "/opt/system/Tools";
static const std::string SCAN_LOG = "/home/ark/sd_scan.log";

Gui_dArkOSen::Gui_dArkOSen(Window* window)
	: GuiSettings(window, _("REASSIGN SYSTEMS TO SD1").c_str())
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

// =======================================================
// Scan & Repair helpers
// =======================================================
static int RunCaptured(const std::string& cmd, std::string& output)
{
	output.clear();
	FILE* pipe = popen(cmd.c_str(), "r");
	if (!pipe)
		return -1;

	char buf[512];
	while (fgets(buf, sizeof(buf), pipe) != nullptr)
		output += buf;

	return WEXITSTATUS(pclose(pipe));
}

static void AppendScanLog(const std::string& text)
{
	std::ofstream log(SCAN_LOG, std::ios::app);
	if (!log.is_open())
		return;

	time_t now = time(nullptr);
	char ts[32];
	strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&now));
	log << ts << " " << text << "\n";
}

static std::string BuildScanSummary(const std::string& result, const std::vector<std::string>& notes)
{
	std::string header;
	if (result == "failed")
		header = _("Scan failed. Repairs were not successful.");
	else if (result == "repaired")
		header = _("Scan failed. Repairs were successful.");
	else
		return _("Scan passed. No repairs were needed.");

	std::string noteList;
	int count = 0;
	for (auto& n : notes)
	{
		if (n.empty() || count >= 3)
			continue;
		noteList += "- " + n + "\n";
		count++;
	}

	return noteList.empty() ? header : header + "\n\n" + noteList;
}

static void CheckWifiThenRun(Window* window, const std::function<void()>& runFn)
{
	std::string wifiState;
	RunCaptured("nmcli radio wifi", wifiState);

	if (wifiState.find("enabled") != std::string::npos)
	{
		AppendScanLog("Turning off wifi");
		system("/usr/local/bin/wifi_disable.sh");
		window->pushGui(new GuiMsgBox(window,
			_("Wi-fi has been turned off to ensure the disk is not busy."),
			_("OK"), runFn));
	}
	else
	{
		runFn();
	}
}

void ScanRepairBoot(Window* window)
{
	std::remove(SCAN_LOG.c_str());

	CheckWifiThenRun(window, [window]
	{
		window->renderLoadingScreen(_("PLEASE WAIT..."));
		AppendScanLog("boot scan started");

		std::string result;
		std::vector<std::string> notes;

		int umountRc = system("sudo -n umount /boot 2>>/home/ark/sd_scan.log");
		if (umountRc != 0)
		{
			result = "failed";
			notes.push_back(_("Could not unmount /boot"));
		}
		else
		{
			std::string output;
			int rc = RunCaptured("sudo -n fsck.fat -a /dev/mmcblk0p1 2>&1", output);
			AppendScanLog(output);

			std::string lower = Utils::String::toLower(output);
			if (rc == 0)
				result = "clean";
			else if (rc == 1 || rc == 2)
			{
				result = "repaired";
				if (lower.find("dirty bit") != std::string::npos)
					notes.push_back(_("Dirty bit cleared"));
				if (lower.find("differences between boot sector") != std::string::npos)
					notes.push_back(_("Boot sector mismatch noted"));
				if (lower.find("bad file") != std::string::npos)
					notes.push_back(_("Bad file entries fixed"));
			}
			else
				result = "failed";
		}

		// always remount, success or failure
		system("sudo -n mount /dev/mmcblk0p1 /boot 2>>/home/ark/sd_scan.log");

		AppendScanLog("boot scan result: " + result);
		window->pushGui(new GuiMsgBox(window, BuildScanSummary(result, notes), _("OK")));
	});
}

void ScanRepairRootfs(Window* window)
{
	std::remove(SCAN_LOG.c_str());

	CheckWifiThenRun(window, [window]
	{
		window->renderLoadingScreen(_("PLEASE WAIT..."));
		AppendScanLog("rootfs scan started");

		std::string output;
		int rc = RunCaptured("sudo -n btrfs scrub start -B /dev/mmcblk0p2 2>&1", output);
		AppendScanLog(output);

		std::string result;
		std::vector<std::string> notes;
		std::string lower = Utils::String::toLower(output);
		bool p2Eject = false;

		if (rc != 0)
			result = "failed";
		else if (std::regex_search(lower, std::regex("error summary:.*no errors found")))
			result = "clean";
		else if (lower.find("uncorrectable") != std::string::npos)
		{
			result = "failed";
			notes.push_back(_("Uncorrectable errors found on system partition"));
		}
		else
		{
			result = "repaired";
			std::smatch m;
			if (std::regex_search(output, m, std::regex(R"(\d+\s+errors corrected)", std::regex::icase)))
				notes.push_back(m.str());
		}

		if (result == "failed")
			p2Eject = true;

		AppendScanLog("rootfs scan result: " + result);

		std::string msg = BuildScanSummary(result, notes);
		if (p2Eject)
			msg += "\n" + std::string(_("Partition 2 (system) could not be fully repaired while running. Eject the card and run 'sudo btrfs check --repair /dev/mmcblk0p2' from a Linux PC."));

		window->pushGui(new GuiMsgBox(window, msg, _("OK")));
	});
}

static void TriggerRecoveryReboot(Window* window, const std::string& helperScript, const std::string& confirmMsg)
{
	if (!Utils::FileSystem::exists(helperScript))
	{
		window->pushGui(new GuiMsgBox(window, _("HELPER SCRIPT NOT FOUND") + "\n" + helperScript, _("OK")));
		return;
	}

	window->pushGui(new GuiMsgBox(window, confirmMsg, _("YES"),
		[window, helperScript]
		{
			bool copied = false;
			for (int i = 0; i < 5; i++)
			{
				system(("sudo -n cp -f \"" + helperScript + "\" /boot/recovery.sh").c_str());
				system("sync");
				if (Utils::FileSystem::exists("/boot/recovery.sh"))
				{
					copied = true;
					break;
				}
				usleep(200000);
			}

			if (!copied)
			{
				window->pushGui(new GuiMsgBox(window, _("FAILED TO PREPARE RECOVERY SCRIPT"), _("OK")));
				return;
			}

			Scripting::fireEvent("quit", "reboot");
			Scripting::fireEvent("reboot");
			if (quitES(QuitMode::REBOOT) != 0)
				LOG(LogWarning) << "Restart terminated with non-zero result!";
		},
		_("NO"), nullptr));
}

void ScanRepairSD1Games(Window* window)
{
	TriggerRecoveryReboot(window, "/usr/local/bin/Scan_SD1p3.sh",
		_("SCAN SD1 GAMES?\nTHE CONSOLE WILL REBOOT TO COMPLETE THE SCAN."));
}

void ScanRepairSD2(Window* window)
{
	TriggerRecoveryReboot(window, "/usr/local/bin/Scan_SD2.sh",
		_("SCAN SD2?\nTHE CONSOLE WILL REBOOT TO COMPLETE THE SCAN."));
}