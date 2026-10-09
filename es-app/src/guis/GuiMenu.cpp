#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include "guis/GuiMenu.h"
#include "guis/GuiTools.h"
#include "components/OptionListComponent.h"
#include "components/SliderComponent.h"
#include "components/SwitchComponent.h"
#include "guis/GuiCollectionSystemsOptions.h"
#include "guis/GuiDetectDevice.h"
#include "guis/GuiGeneralScreensaverOptions.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiScraperStart.h"
#include "guis/GuiSettings.h"
#include "views/UIModeController.h"
#include "views/ViewController.h"
#include "CollectionSystemManager.h"
#include "EmulationStation.h"
#include "Scripting.h"
#include "SystemData.h"
#include "VolumeControl.h"
#include <SDL_events.h>
#include <algorithm>
#include <sstream>
#include <cctype>
#include <vector>
#include <thread>
#include <mutex>
#include <cstdlib>
#include <ctime>
#include <cstdio>
#include "utils/StringUtil.h"
#include "AudioManager.h"
#include "resources/TextureData.h"
#include "animations/LambdaAnimation.h"
#include "guis/GuiThemeInstall.h"
#include "GuiGamelistOptions.h" // grid sizes
#include "platform.h"
#include "renderers/Renderer.h" // setSwapInterval()
#include "guis/GuiTextEditPopupKeyboard.h"
#include "scrapers/ThreadedScraper.h"
#include "ApiSystem.h"
#include "views/gamelist/IGameListView.h"
#include "components/BatteryIndicatorComponent.h"

//#include <go2/display.h>
#include "SystemConf.h"

GuiMenu::GuiMenu(Window* window, bool animate) : GuiComponent(window), mMenu(window, _("MAIN MENU")), mVersion(window)
{

	addEntry(_("DISPLAY SETTINGS AND INFO"), true, [this] { openDisplaySettings(); }, "iconSystem");

	auto theme = ThemeData::getMenuTheme();

	bool isFullUI = UIModeController::getInstance()->isUIModeFull();	
	
	if (isFullUI)
	{
		addEntry(_("UI SETTINGS"), true, [this] { openUISettings(); }, "iconUI");
		// addEntry(_("CONFIGURE INPUT"), true, [this] { openConfigInput(); }, "iconControllers");
	}

	addEntry(_("NETWORK SETTINGS"), true, [this] { openNetworkSettings(); }, "iconNetwork");

	addEntry(_("SOUND SETTINGS"), true, [this] { openSoundSettings(); }, "iconSound");

	addEntry(_("PERFORMANCE SETTINGS"), true, [this] { openPerformanceSettings(); }, "iconGames");

	if (isFullUI)
	{
		addEntry(_("GAME COLLECTION SETTINGS"), true, [this] { openCollectionSystemSettings(); }, "iconGames");

		// Emulator settings 
		for (auto system : SystemData::sSystemVector)
		{
			if (system->isCollection() || system->getSystemEnvData()->mEmulators.size() == 0 || (system->getSystemEnvData()->mEmulators.size() == 1 && system->getSystemEnvData()->mEmulators[0].mCores.size() <= 1))
				continue;

			addEntry(_("EMULATOR SETTINGS"), true, [this] { openEmulatorSettings(); }, "iconSystem");
			break;
		}
		
		addEntry(_("SCRAPER"), true, [this] { openScraperSettings(); }, "iconScraper");

#if WIN32
		addEntry(_("DOWNLOADS AND UPDATES"), true, [this] { openUpdateSettings(); }, "iconUpdates");
#endif

    // Tools Menu
    mMenu.addEntry(_("OPTIONS"), true, [this, window] {
        window->pushGui(new GuiTools(window));
    }, "iconAdvanced");

		addEntry(_("ADVANCED SETTINGS"), true, [this] { openOtherSettings(); }, "iconAdvanced");
	}
	
	addEntry(_("QUIT"), !Settings::getInstance()->getBool("ShowOnlyExit"), [this] {openQuitMenu(); }, "iconQuit");

	addEntry(_("BAT") + ": " + std::string(getShOutput(R"(cat /sys/class/power_supply/battery/capacity)")) + "%" + " | " + _("SND") + ": " + std::string(getShOutput(R"(current_volume)")) + " | " + _("BRT") + ": " + std::to_string(ApiSystem::getInstance()->getBrightnessLevel()) + "% | " + _("WIFI") + ": " + std::string(getShOutput(R"(if [ -z $(cat /sys/class/net/wlan0/operstate) ]; then echo "Off"; else cat /sys/class/net/wlan0/operstate; fi)")), true, [this] {  });

	addEntry(_("Distro Version") + ": " + std::string(getShOutput(R"(cat /usr/share/plymouth/themes/text.plymouth | grep title | cut -c 7-50)")), false, [this] {
		if (access("/usr/local/bin/Update.sh", F_OK) == 0)
		{
			AudioManager::getInstance()->deinit();
			VolumeControl::getInstance()->deinit();
			mWindow->deinit(true);
			system("/bin/bash \"/usr/local/bin/Update.sh\" 2>&1 > /dev/tty1");
			mWindow->init(true);
			VolumeControl::getInstance()->init();
			AudioManager::getInstance()->init();
		}
		else
			mWindow->pushGui(new GuiMsgBox(mWindow, _("UPDATE SCRIPT NOT FOUND\n/usr/local/bin/Update.sh"), _("OK")));
	});
	
	addChild(&mMenu);
	addVersionInfo();

	setSize(mMenu.getSize());

	if (animate)	
		animateTo(
			Vector2f((Renderer::getScreenWidth() - mSize.x()) / 2, Renderer::getScreenHeight() * 0.9),
			Vector2f((Renderer::getScreenWidth() - mSize.x()) / 2, (Renderer::getScreenHeight() - mSize.y()) / 2));
	else
		setPosition((Renderer::getScreenWidth() - mSize.x()) / 2, (Renderer::getScreenHeight() - mSize.y()) / 2);
}

// ============================================================================
// Helper Functions (borrowed from ArkOS4Clone)
// ============================================================================

static std::mutex g_execCommandMutex;
static std::string executeCommand(const std::string& cmd)
{
	std::lock_guard<std::mutex> lock(g_execCommandMutex);

	FILE* pipe = popen(cmd.c_str(), "r");
	if (!pipe) return "";

	char buffer[256];
	std::string result;
	while (fgets(buffer, sizeof(buffer), pipe)) {
		result += buffer;
	}
	pclose(pipe);
	return Utils::String::trim(result);
}

static const std::string DEADZONE_STATE_FILE = "/home/ark/.deadzone_adc_value";

// Read current joystick deadzone (decimal ADC value). Uses the state file if
// present; otherwise falls back to the dts and writes the state file so the
// next read doesn't need the fallback.
static std::string getDeadzoneDecimal()
{
	std::string val = executeCommand("cat " + DEADZONE_STATE_FILE + " 2>/dev/null");
	while (!val.empty() && (val.back() == '\n' || val.back() == '\r'))
		val.pop_back();
	if (!val.empty())
		return val;

	executeCommand(
		"for f in /boot/*linux.dtb; do "
		"sudo dtc -I dtb -O dts -o \"${f%.dtb}.dts\" \"$f\"; "
		"done");

	std::string hex = executeCommand(
		"grep -m1 -E '^[[:space:]]*button-adc-deadzone[[:space:]]*=' "
		"$(find /boot -maxdepth 1 -name '*.dts') 2>/dev/null | "
		"sed -E 's/.*<([0-9A-Fa-fx]+)>.*/\\1/'");

	executeCommand("sudo rm -f /boot/*linux.dts");

	int dec = 0;
	if (!hex.empty())
		dec = (int)strtol(hex.c_str(), nullptr, 16);

	val = std::to_string(dec);
	executeCommand("echo " + val + " > " + DEADZONE_STATE_FILE);
	return val;
}

// Apply a new deadzone across every *.dts in /boot, recompile each to .dtb,
// and persist the decimal value to the state file.
static void setDeadzoneValue(const std::string& hexVal, const std::string& decVal)
{
	executeCommand(
		"for f in /boot/*linux.dtb; do "
		"sudo dtc -I dtb -O dts -o \"${f%.dtb}.dts\" \"$f\"; "
		"done");

	executeCommand(
		"for dts in $(find /boot -type f -name '*.dts'); do "
		"grep -q 'button-adc-deadzone' \"$dts\" && "
		"sudo sed -i -E \"s/^([[:space:]]*button-adc-deadzone[[:space:]]*=[[:space:]]*<)[^>]+(>;)$/\\1" + hexVal + "\\2/\" \"$dts\" && "
		"sudo dtc -I dts -O dtb -o \"${dts%.dts}.dtb\" \"$dts\"; "
		"done");

	executeCommand("sudo rm -f /boot/*linux.dts");

	executeCommand("echo " + decVal + " > " + DEADZONE_STATE_FILE);
}

static const std::string FILEMGR_SCRIPT = "/opt/system/File Manager.sh";
static const std::string FILES351_SCRIPT = "/opt/system/351Files.sh";

// Root file access state is read from File Manager.sh: "on" if its exec line
// still has sudo, "off" if sudo has been stripped.
static bool isRootFileAccessEnabled()
{
	std::string result = executeCommand(
		"grep -m1 -E '^exec sudo ' \"" + FILEMGR_SCRIPT + "\" 2>/dev/null");
	return !Utils::String::trim(result).empty();
}

// Add or strip sudo from both file explorers' launch lines.
static void toggleRootFileAccess(bool enable)
{
	if (enable)
	{
		executeCommand(
			"sudo sed -i -E 's|^exec (sudo )?/opt/dingux/DinguxCommander|exec sudo /opt/dingux/DinguxCommander|' \""
			+ FILEMGR_SCRIPT + "\"");
		executeCommand(
			"sudo sed -i -E 's|^(sudo )?\\./351Files-sd2|sudo ./351Files-sd2|' \""
			+ FILES351_SCRIPT + "\"");
	}
	else
	{
		executeCommand(
			"sudo sed -i -E 's|^exec (sudo )?/opt/dingux/DinguxCommander|exec /opt/dingux/DinguxCommander|' \""
			+ FILEMGR_SCRIPT + "\"");
		executeCommand(
			"sudo sed -i -E 's|^(sudo )?\\./351Files-sd2|./351Files-sd2|' \""
			+ FILES351_SCRIPT + "\"");
	}
}

// Check current WiFi state: no interface = disabled, otherwise check rfkill
static bool isWifiRfkillBlocked()
{
    std::string result = executeCommand("cat /var/cache/wifi_manager_state 2>/dev/null");
    return result.find("OFF") != std::string::npos;
}

// Check current LED color: gpio77 value 1 = red, 0 = blue/green
static bool isLedRed()
{
    std::string result = executeCommand("cat /sys/class/gpio/gpio77/value 2>/dev/null");
    return result.find("1") != std::string::npos;
}

// Get active WiFi interface (wlan0, p2p0, etc.)
static std::string getActiveWifiInterface()
{
    // Check for connected wifi devices via nmcli
    std::string result = executeCommand("nmcli -t -f DEVICE,TYPE,STATE dev 2>/dev/null");
    if (!result.empty()) {
        std::istringstream stream(result);
        std::string line;
        while (std::getline(stream, line)) {
            // Format: device:type:state
            if (line.find(":wifi:") != std::string::npos && line.find(":connected") != std::string::npos) {
                size_t colonPos = line.find(':');
                if (colonPos != std::string::npos) {
                    std::string iface = line.substr(0, colonPos);
                    if (!iface.empty()) return iface;
                }
            }
        }
    }
    
    // Fallback: check operstate of common wifi interfaces
    std::vector<std::string> wifiInterfaces = {"p2p0", "wlan0", "wlan1"};
    for (const auto& iface : wifiInterfaces) {
        std::string operstate = executeCommand("cat /sys/class/net/" + iface + "/operstate 2>/dev/null");
        if (operstate == "up") return iface;
    }
    
    return "wlan0"; // Default fallback
}



static std::string getCurrentWifiSSID()
{
    // Method 1: nmcli active connection - check all wifi interfaces
    std::string result = executeCommand("nmcli -t -f NAME,DEVICE connection show --active 2>/dev/null");
    if (!result.empty()) {
        std::istringstream stream(result);
        std::string line;
        while (std::getline(stream, line)) {
            // Check for wlan, p2p, or any wifi interface
            if (line.find(":wlan") != std::string::npos || 
                line.find(":p2p") != std::string::npos ||
                line.find("wlan") != std::string::npos ||
                line.find("p2p") != std::string::npos) {
                size_t colonPos = line.find(':');
                if (colonPos != std::string::npos) {
                    std::string connName = line.substr(0, colonPos);
                    if (!connName.empty() && connName != "lo") {
                        return connName;
                    }
                }
            }
        }
    }
    
    // Method 2: iw dev - check active interface
    std::string iface = getActiveWifiInterface();
    std::string ssid = executeCommand("iw dev " + iface + " info 2>/dev/null | grep ssid");
    if (!ssid.empty()) {
        size_t pos = ssid.find("ssid ");
        if (pos != std::string::npos) {
            ssid = ssid.substr(pos + 5);
            ssid.erase(std::remove_if(ssid.begin(), ssid.end(), ::isspace), ssid.end());
            if (!ssid.empty() && ssid != "off/any") {
                return ssid;
            }
        }
    }
    
    return "";
}

void GuiMenu::openDisplaySettings()
{
	// Brightness
	auto s = new GuiSettings(mWindow, _("DISPLAY"));

    int brighness;
    ApiSystem::getInstance()->getBrighness(brighness);
   	auto brightnessComponent = std::make_shared<SliderComponent>(mWindow, 1.0f, 100.f, 1.0f, "%");
    brightnessComponent->setValue((float) ApiSystem::getInstance()->getBrightnessLevel());
   	brightnessComponent->setOnValueChanged([](const float &newVal)
    {
    	ApiSystem::getInstance()->setBrighness((int)Math::round(newVal));
   	});
    s->addSaveFunc([this, brightnessComponent] {
         SystemConf::getInstance()->set("brightness.level", std::to_string((int)Math::round(brightnessComponent->getValue())));
    });

	s->addWithLabel(_("BRIGHTNESS"), brightnessComponent);
	
	auto gammaComponent = std::make_shared<SliderComponent>(mWindow, 0.4f, 1.8f, 0.1f, "", 1);
	gammaComponent->setValue(ApiSystem::getInstance()->getGamma());
	gammaComponent->setOnValueChanged([](const float &newVal)
	{
		ApiSystem::getInstance()->setGamma(newVal);
	});
	s->addWithLabel(_("GAMMA"), gammaComponent);
	
		auto brightnessPopup = std::make_shared<SwitchComponent>(mWindow);
		brightnessPopup->setState(Settings::getInstance()->getBool("BrightnessPopup"));
		s->addWithLabel(_("SHOW OVERLAY WHEN BRIGHTNESS CHANGES"), brightnessPopup);
		s->addSaveFunc([brightnessPopup]
			{
				bool old_value = Settings::getInstance()->getBool("BrightnessPopup");
				if (old_value != brightnessPopup->getState())
					Settings::getInstance()->setBool("BrightnessPopup", brightnessPopup->getState());
			}
		);

	mWindow->pushGui(s);
}

void GuiMenu::openDateTimeSettings()
{
	auto s = new GuiSettings(mWindow, _("DATE & TIME"));

	// --- CURRENT TIME (display only, no clicks, right-aligned) ---
	auto timeText = std::make_shared<TextComponent>(mWindow, "", ThemeData::getMenuTheme()->Text.font, ThemeData::getMenuTheme()->Text.color, ALIGN_RIGHT);
	auto refreshTimeText = [timeText] {
		time_t now = time(nullptr);
		struct tm* t = localtime(&now);
		bool clock12 = Settings::getInstance()->getBool("ClockMode12");
		char buf[32] = {0};
		strftime(buf, sizeof(buf), clock12 ? "%I:%M %p, %m-%d-%Y" : "%H:%M, %m-%d-%Y", t);
		timeText->setText(std::string(buf));
	};
	refreshTimeText();
	s->addWithLabel(_("CURRENT TIME"), timeText);

	// --- NETWORK SYNC ---
	s->addEntry(_("NETWORK SYNC"), true, [this, refreshTimeText] {
		std::string gateway = executeCommand("ip route | awk '/default/ { print $3; exit }' 2>/dev/null");
		if (Utils::String::trim(gateway).empty())
		{
			mWindow->pushGui(new GuiMsgBox(mWindow, _("NO NETWORK CONNECTION"), _("OK")));
			return;
		}
		executeCommand("sudo timedatectl set-ntp 1 2>/dev/null");
		refreshTimeText();
		mWindow->pushGui(new GuiMsgBox(mWindow, _("TIME SYNCED"), _("OK")));
	}, "");

	// --- SET MANUALLY ---
	s->addEntry(_("SET MANUALLY"), true, [this, refreshTimeText] {
		openManualDateTimeSettings(refreshTimeText);
	}, "");

	mWindow->pushGui(s);
}

void GuiMenu::openManualDateTimeSettings(std::function<void()> onApplied)
{
	auto s = new GuiSettings(mWindow, _("SET MANUALLY"));

	time_t now = time(nullptr);
	struct tm t = *localtime(&now);

	int initYear  = t.tm_year + 1900;
	int initMonth = t.tm_mon + 1;
	int initDay   = t.tm_mday;
	int initHour  = (t.tm_hour == 0) ? 24 : t.tm_hour;
	int initMin   = (t.tm_min == 0) ? 60 : t.tm_min;

	// --- SET YEAR (2026 +/- 50) ---
	auto yearList = std::make_shared<OptionListComponent<int>>(mWindow, _("SET YEAR"), false);
	for (int y = 2026 - 50; y <= 2026 + 50; y++)
		yearList->add(std::to_string(y), y, y == initYear);
	s->addWithLabel(_("SET YEAR"), yearList);

	// --- SET MONTH ---
	auto monthList = std::make_shared<OptionListComponent<int>>(mWindow, _("SET MONTH"), false);
	for (int m = 1; m <= 12; m++)
		monthList->add(std::to_string(m), m, m == initMonth);
	s->addWithLabel(_("SET MONTH"), monthList);

	// --- SET DATE (1-31, validated on close) ---
	auto dayList = std::make_shared<OptionListComponent<int>>(mWindow, _("SET DATE"), false);
	for (int d = 1; d <= 31; d++)
		dayList->add(std::to_string(d), d, d == initDay);
	s->addWithLabel(_("SET DATE"), dayList);

	// --- SET HOURS (1-24) ---
	auto hourList = std::make_shared<OptionListComponent<int>>(mWindow, _("SET HOURS"), false);
	for (int h = 1; h <= 24; h++)
		hourList->add(std::to_string(h), h, h == initHour);
	s->addWithLabel(_("SET HOURS"), hourList);

	// --- SET MINUTES (1-60) ---
	auto minList = std::make_shared<OptionListComponent<int>>(mWindow, _("SET MINUTES"), false);
	for (int mi = 1; mi <= 60; mi++)
		minList->add(std::to_string(mi), mi, mi == initMin);
	s->addWithLabel(_("SET MINUTES"), minList);

	// --- Apply only on close, only if changed ---
	s->addSaveFunc([this, yearList, monthList, dayList, hourList, minList,
		initYear, initMonth, initDay, initHour, initMin, onApplied]
	{
		int year  = yearList->getSelected();
		int month = monthList->getSelected();
		int day   = dayList->getSelected();
		int hour  = hourList->getSelected();
		int min   = minList->getSelected();

		if (year == initYear && month == initMonth && day == initDay &&
			hour == initHour && min == initMin)
			return; // nothing changed, do nothing

		// Static table (Feb always allows 29, no leap-year math per spec)
		static const int daysInMonth[12] = {31,29,31,30,31,30,31,31,30,31,30,31};
		if (day > daysInMonth[month - 1])
		{
			mWindow->pushGui(new GuiMsgBox(mWindow, _("INVALID DATE SET"), _("OK")));
			return; // reverts by simply not applying; system clock untouched
		}

		int hour24 = (hour == 24) ? 0 : hour;
		int min60  = (min == 60) ? 0 : min;

		char cmd[160];
		snprintf(cmd, sizeof(cmd),
			"sudo timedatectl set-ntp 0 2>/dev/null && sudo date -s \"%04d-%02d-%02d %02d:%02d:00\" 2>/dev/null",
			year, month, day, hour24, min60);
		executeCommand(cmd);

		if (onApplied)
			onApplied();

		mWindow->pushGui(new GuiMsgBox(mWindow, _("TIME SET"), _("OK")));
	});

	mWindow->pushGui(s);
}

void GuiMenu::openScraperSettings()
{
	auto s = new GuiSettings(mWindow, _("SCRAPER"));

	std::string scraper = Settings::getInstance()->getString("Scraper");

	// scrape from
	auto scraper_list = std::make_shared< OptionListComponent< std::string > >(mWindow, _("SCRAPE FROM"), false);
	std::vector<std::string> scrapers = getScraperList();

	// Select either the first entry of the one read from the settings, just in case the scraper from settings has vanished.
	for (auto it = scrapers.cbegin(); it != scrapers.cend(); it++)
		scraper_list->add(*it, *it, *it == scraper);

	s->addWithLabel(_("SCRAPE FROM"), scraper_list);
	s->addSaveFunc([scraper_list] { Settings::getInstance()->setString("Scraper", scraper_list->getSelected()); });

	if (!scraper_list->hasSelection())
	{
		scraper_list->selectFirstItem();
		scraper = scraper_list->getSelected();
	}

	if (scraper == "ScreenScraper")
	{
		// Image source : <image> tag
		std::string imageSourceName = Settings::getInstance()->getString("ScrapperImageSrc");
		auto imageSource = std::make_shared< OptionListComponent<std::string> >(mWindow, _("IMAGE SOURCE"), false);
		//imageSource->add(_("NONE"), "", imageSourceName.empty());
		imageSource->add(_("SCREENSHOT"), "ss", imageSourceName == "ss");
		imageSource->add(_("TITLE SCREENSHOT"), "sstitle", imageSourceName == "sstitle");
		imageSource->add(_("MIX V1"), "mixrbv1", imageSourceName == "mixrbv1");
		imageSource->add(_("MIX V2"), "mixrbv2", imageSourceName == "mixrbv2");
		imageSource->add(_("BOX 2D"), "box-2D", imageSourceName == "box-2D");
		imageSource->add(_("BOX 3D"), "box-3D", imageSourceName == "box-3D");

		if (!imageSource->hasSelection())
			imageSource->selectFirstItem();

		s->addWithLabel(_("IMAGE SOURCE"), imageSource);
		s->addSaveFunc([imageSource] { Settings::getInstance()->setString("ScrapperImageSrc", imageSource->getSelected()); });

		// Box source : <thumbnail> tag
		std::string thumbSourceName = Settings::getInstance()->getString("ScrapperThumbSrc");
		auto thumbSource = std::make_shared< OptionListComponent<std::string> >(mWindow, _("BOX SOURCE"), false);
		thumbSource->add(_("NONE"), "", thumbSourceName.empty());
		thumbSource->add(_("BOX 2D"), "box-2D", thumbSourceName == "box-2D");
		thumbSource->add(_("BOX 3D"), "box-3D", thumbSourceName == "box-3D");

		if (!thumbSource->hasSelection())
			thumbSource->selectFirstItem();

		s->addWithLabel(_("BOX SOURCE"), thumbSource);
		s->addSaveFunc([thumbSource] { Settings::getInstance()->setString("ScrapperThumbSrc", thumbSource->getSelected()); });

		imageSource->setSelectedChangedCallback([this, thumbSource](std::string value)
		{
			if (value == "box-2D")
				thumbSource->remove(_("BOX 2D"));
			else
				thumbSource->add(_("BOX 2D"), "box-2D", false);

			if (value == "box-3D")
				thumbSource->remove(_("BOX 3D"));
			else
				thumbSource->add(_("BOX 3D"), "box-3D", false);
		});

		// Logo source : <marquee> tag
		std::string logoSourceName = Settings::getInstance()->getString("ScrapperLogoSrc");
		auto logoSource = std::make_shared< OptionListComponent<std::string> >(mWindow, _("LOGO SOURCE"), false);
		logoSource->add(_("NONE"), "", logoSourceName.empty());
		logoSource->add(_("WHEEL"), "wheel", logoSourceName == "wheel");
		logoSource->add(_("MARQUEE"), "marquee", logoSourceName == "marquee");

		if (!logoSource->hasSelection())
			logoSource->selectFirstItem();

		s->addWithLabel(_("LOGO SOURCE"), logoSource);
		s->addSaveFunc([logoSource] { Settings::getInstance()->setString("ScrapperLogoSrc", logoSource->getSelected()); });

		// Region source
		std::string regionName = Settings::getInstance()->getString("ScrapperRegionSrc");
		auto regionSource = std::make_shared< OptionListComponent<std::string> >(mWindow, _("REGION SOURCE"), false);
		regionSource->add(_("US"), "US", regionName == "US");
		regionSource->add(_("EU"), "EU", regionName == "EU");
		regionSource->add(_("FR"), "FR", regionName == "FR");
		regionSource->add(_("JP"), "JP", regionName == "JP");

		if (!regionSource->hasSelection())
			regionSource->selectFirstItem();

		s->addWithLabel(_("REGION SOURCE"), regionSource);
		s->addSaveFunc([regionSource] { Settings::getInstance()->setString("ScrapperRegionSrc", regionSource->getSelected()); });

		// scrape ratings
		auto scrape_ratings = std::make_shared<SwitchComponent>(mWindow);
		scrape_ratings->setState(Settings::getInstance()->getBool("ScrapeRatings"));
		s->addWithLabel(_("SCRAPE RATINGS"), scrape_ratings);
		s->addSaveFunc([scrape_ratings] { Settings::getInstance()->setBool("ScrapeRatings", scrape_ratings->getState()); });

		// scrape video
		auto scrape_video = std::make_shared<SwitchComponent>(mWindow);
		scrape_video->setState(Settings::getInstance()->getBool("ScrapeVideos"));
		s->addWithLabel(_("SCRAPE VIDEOS"), scrape_video);
		s->addSaveFunc([scrape_video] { Settings::getInstance()->setBool("ScrapeVideos", scrape_video->getState()); });

		// Account
		createInputTextRow(s, _("USERNAME"), "ScreenScraperUser", false);
		createInputTextRow(s, _("PASSWORD"), "ScreenScraperPass", true);
	}
	else
	{
		// scrape ratings
		auto scrape_ratings = std::make_shared<SwitchComponent>(mWindow);
		scrape_ratings->setState(Settings::getInstance()->getBool("ScrapeRatings"));
		s->addWithLabel(_("SCRAPE RATINGS"), scrape_ratings); // batocera
		s->addSaveFunc([scrape_ratings] { Settings::getInstance()->setBool("ScrapeRatings", scrape_ratings->getState()); });
		createInputTextRow(s, _("API KEY"), "GamesDBApiKey", false);
	}

	// scrape now
	ComponentListRow row;
	auto openScrapeNow = [this] 
	{ 
		if (ThreadedScraper::isRunning())
		{
			Window* window = mWindow;

			mWindow->pushGui(new GuiMsgBox(mWindow, _("SCRAPING IS RUNNING. DO YOU WANT TO STOP IT ?"), _("YES"), [this, window]
			{
				ThreadedScraper::stop();
			}, _("NO"), nullptr));

			return;
		}

		mWindow->pushGui(new GuiScraperStart(mWindow)); 
	};
	std::function<void()> openAndSave = openScrapeNow;
	openAndSave = [s, openAndSave] { s->save(); openAndSave(); };
	s->addEntry(_("SCRAPE NOW"), true, openAndSave, "iconScraper");

	s->updatePosition();

	scraper_list->setSelectedChangedCallback([this, s, scraper, scraper_list](std::string value)
	{
		if (value != scraper && (scraper == "ScreenScraper" || value == "ScreenScraper"))
		{
			Settings::getInstance()->setString("Scraper", value);
			delete s;
			openScraperSettings();
		}
	});

	mWindow->pushGui(s);
}

// Remote services helpers
static bool isRemoteServicesEnabled()
{
	std::string smbActive = executeCommand("timeout 3 systemctl is-active --quiet smbd 2>/dev/null && echo 1 || echo 0");
	std::string sshActive = executeCommand("timeout 3 systemctl is-active --quiet ssh.service 2>/dev/null && echo 1 || echo 0");
	std::string fbActive = executeCommand("pgrep -x filebrowser >/dev/null 2>/dev/null && echo 1 || echo 0");

	auto isOne = [](std::string s) {
		while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
		return s == "1";
	};

	return (isOne(smbActive) || isOne(sshActive) || isOne(fbActive));
}

static bool isSambaRootAccessEnabled()
{
	return access("/home/ark/.smb_root_access", F_OK) == 0;
}

static void toggleRemoteServices(bool enable)
{
	if (enable) {
		std::string gateway = executeCommand("ip route | awk '/default/ { print $3; exit }' 2>/dev/null");
		if (Utils::String::trim(gateway).empty()) {
			return;
		}
		executeCommand("sudo systemctl enable NetworkManager-wait-online 2>/dev/null");
		executeCommand("sudo systemctl start NetworkManager-wait-online 2>/dev/null");
		executeCommand("sudo timedatectl set-ntp 1 2>/dev/null");

		// --- roms2 share toggle in smb.conf ---
		if (access("/roms2", F_OK) != 0) {
			executeCommand("sudo sed -i '/^\\[roms2\\]/,/^$/{s/^/#/}' /etc/samba/smb.conf");
		} else {
			executeCommand("sudo sed -i '/^#\\[roms2\\]/,/^#$/{s/^#//}' /etc/samba/smb.conf");
		}

		executeCommand("sudo systemctl start smbd 2>/dev/null");
		executeCommand("sudo systemctl start nmbd 2>/dev/null");
		executeCommand("sudo systemctl start ssh.service 2>/dev/null");
		executeCommand("sudo pkill -x filebrowser 2>/dev/null || true");
		executeCommand("sudo filebrowser -a 0.0.0.0 -p 80 -d /home/ark/.config/filebrowser.db -r / >/dev/null 2>&1 &");
	} else {
		executeCommand("sudo systemctl disable NetworkManager-wait-online 2>/dev/null");
		executeCommand("sudo systemctl stop NetworkManager-wait-online 2>/dev/null");
		executeCommand("sudo timedatectl set-ntp 0 2>/dev/null");
		executeCommand("sudo systemctl stop smbd 2>/dev/null");
		executeCommand("sudo systemctl stop nmbd 2>/dev/null");
		executeCommand("sudo systemctl stop ssh.service 2>/dev/null");
		executeCommand("sudo pkill -x filebrowser 2>/dev/null || true");
	}
}

static void toggleSambaRootAccess(bool enable)
{
	const std::string smb = "/etc/samba/smb.conf";
	const std::string flag = "/home/ark/.smb_root_access";

	bool remoteActive = isRemoteServicesEnabled();

	if (enable) {
		executeCommand("sudo cp " + smb + ".root " + smb);
		executeCommand("touch " + flag);
	} else {
		executeCommand("sudo cp " + smb + ".default " + smb);
		executeCommand("rm -f " + flag);
	}

	if (remoteActive) {
		toggleRemoteServices(false);
		sleep(2);
		toggleRemoteServices(true);
	}
}

static bool isRemoteServicesAutoStart()
{
	std::string result = executeCommand("systemctl is-enabled remote-autostart.service 2>/dev/null");
	result.erase(std::remove_if(result.begin(), result.end(), ::isspace), result.end());
	return result.find("enabled") != std::string::npos;
}

static void toggleRemoteServicesAutoStart(bool enable)
{
	const std::string unitPath = "/etc/systemd/system/remote-autostart.service";

	// Create the unit file if it doesn't exist yet (matches WM's Toggle_Autostart)
	std::string exists = executeCommand("[ -f " + unitPath + " ] && echo yes || echo no");
	if (exists != "yes") {
		std::string createCmd =
			"sudo bash -c 'cat > " + unitPath + " << \"EOF\"\n"
			"[Unit]\n"
			"Description=Remote Services Autostart\n"
			"After=network.target emulationstation.service\n"
			"[Service]\n"
			"Type=oneshot\n"
			"RemainAfterExit=yes\n"
			"ExecStart=/bin/bash -c \"timedatectl set-ntp 1 & systemctl start smbd & systemctl start nmbd & systemctl start ssh.service & filebrowser -a 0.0.0.0 -p 80 -d /home/ark/.config/filebrowser.db -r / &\"\n"
			"[Install]\n"
			"WantedBy=multi-user.target\n"
			"EOF'";
		executeCommand(createCmd);
		executeCommand("sudo systemctl daemon-reload");
	}

	if (enable) {
		executeCommand("sudo systemctl enable remote-autostart.service 2>/dev/null || true");
	} else {
		executeCommand("sudo systemctl disable remote-autostart.service 2>/dev/null || true");
	}
}

void GuiMenu::scanWifi()
{
	auto busy = new GuiComponent(mWindow);
	auto busyComp = new BusyComponent(mWindow);
	busy->addChild(busyComp);
	busyComp->setText(_("SCANNING WIFI NETWORKS"));
	busy->setSize((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
	mWindow->pushGui(busy);

	mWifiNetworks.clear();

	system("sudo nmcli device wifi rescan 2>/dev/null");
	std::this_thread::sleep_for(std::chrono::seconds(2));

	std::string clist = executeCommand("sudo nmcli -t -f IN-USE,SSID,SIGNAL dev wifi 2>/dev/null");

	std::istringstream stream(clist);
	std::string line;
	while (std::getline(stream, line)) {
		if (line.empty()) continue;

		size_t pos1 = line.find(':');
		if (pos1 == std::string::npos) continue;

		size_t pos2 = line.find(':', pos1 + 1);

		std::string ssid;
		int signal = 0;

		if (pos2 != std::string::npos) {
			ssid = line.substr(pos1 + 1, pos2 - pos1 - 1);
			signal = atoi(line.substr(pos2 + 1).c_str());
		} else {
			ssid = line.substr(pos1 + 1);
		}

		if (ssid.empty() || ssid == "--" || ssid == "\\x00") continue;

		mWifiNetworks.push_back(std::make_pair(ssid, signal));
	}

	mWindow->removeGui(busy);
	delete busy;

	if (mWifiNetworks.empty()) {
		mWindow->pushGui(new GuiMsgBox(mWindow, _("NO WIFI NETWORKS FOUND"), _("OK")));
		return;
	}

	auto s = new GuiSettings(mWindow, _("SELECT WIFI NETWORK"));

	std::sort(mWifiNetworks.begin(), mWifiNetworks.end(),
		[](const std::pair<std::string, int>& a, const std::pair<std::string, int>& b) {
			return a.second > b.second;
		});

	std::map<std::string, int> uniqueNetworks;
	for (auto& net : mWifiNetworks) {
		if (uniqueNetworks.find(net.first) == uniqueNetworks.end() || uniqueNetworks[net.first] < net.second) {
			uniqueNetworks[net.first] = net.second;
		}
	}

	for (auto& net : uniqueNetworks) {
		if (net.first.empty()) continue;

		std::string entryName = net.first + " (" + std::to_string(net.second) + "%)";
		std::string ssid = net.first;
		s->addEntry(entryName, true, [this, ssid] {
			showWifiPasswordInput(ssid);
		}, "");
	}

	mWindow->pushGui(s);
}

void GuiMenu::showWifiPasswordInput(const std::string& ssid)
{
	mWindow->pushGui(new GuiTextEditPopupKeyboard(mWindow,
		_("PASSWORD FOR") + " " + ssid,
		"",
		[this, ssid](const std::string& password) {
			connectWifi(ssid, password);
		},
		false, _("CONNECT")));
}

void GuiMenu::connectWifi(const std::string& ssid, const std::string& password)
{
	auto busy = new GuiComponent(mWindow);
	auto busyComp = new BusyComponent(mWindow);
	busy->addChild(busyComp);
	busyComp->setText(_("CONNECTING TO") + " " + ssid + "...");
	busy->setSize((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
	mWindow->pushGui(busy);

	executeCommand("systemctl disable --now wifi_monitor.service 2>/dev/null || true");
	executeCommand("nmcli con delete \"" + ssid + "\" 2>/dev/null");

	std::string result;
	if (password.empty())
		result = executeCommand("nmcli device wifi connect \"" + ssid + "\" 2>&1");
	else
		result = executeCommand("nmcli device wifi connect \"" + ssid + "\" password \"" + password + "\" 2>&1");

	std::this_thread::sleep_for(std::chrono::seconds(3));

	mWindow->removeGui(busy);
	delete busy;

	std::string connectedSSID = getCurrentWifiSSID();
	bool connected = (connectedSSID == ssid);

	if (connected) {
		if (mWifiStatusText) mWifiStatusText->setText(connectedSSID);
		executeCommand("nmcli con modify \"" + ssid + "\" wifi-sec.psk-flags 0 2>/dev/null || true");
		executeCommand("nmcli con modify \"" + ssid + "\" 802-11-wireless.bgscan \"\" 2>/dev/null || true");
		executeCommand("systemctl enable --now wifi_monitor.service 2>/dev/null || true");
		mWindow->pushGui(new GuiMsgBox(mWindow, _("CONNECTED TO") + "\n" + ssid, _("OK")));
	} else {
		executeCommand("sudo rm -f \"/etc/NetworkManager/system-connections/" + ssid + ".nmconnection\" 2>/dev/null");
		if (mWifiStatusText) mWifiStatusText->setText(connectedSSID.empty() ? _("NOT CONNECTED") : connectedSSID);

		std::string errorMsg = _("CONNECTION FAILED");
		if (result.find("Secrets were required") != std::string::npos)
			errorMsg += "\n" + _("INVALID PASSWORD");
		else if (result.find("not found") != std::string::npos || result.find("No network") != std::string::npos)
			errorMsg += "\n" + _("NETWORK NOT FOUND");
		else if (!result.empty())
			errorMsg += "\n" + result;
		mWindow->pushGui(new GuiMsgBox(mWindow, errorMsg, _("OK")));
	}
}

void GuiMenu::showHostnameInput(std::shared_ptr<TextComponent> hostnameText)
{
	mWindow->pushGui(new GuiTextEditPopupKeyboard(mWindow,
		_("HOSTNAME"),
		hostnameText->getValue(),
		[this, hostnameText](const std::string& newHostname) {
			applyHostname(newHostname, hostnameText);
		},
		false, _("SAVE")));
}

void GuiMenu::applyHostname(const std::string& newHostname, std::shared_ptr<TextComponent> hostnameText)
{
	if (newHostname.empty())
		return;

	executeCommand("echo \"" + newHostname + "\" | sudo tee /etc/hostname > /dev/null");
	executeCommand("sudo hostname \"" + newHostname + "\"");
	executeCommand("sudo sed -i 's/^127\\.0\\.1\\.1[[:space:]].*/127.0.1.1\\t" + newHostname + "/' /etc/hosts");

	hostnameText->setValue(newHostname);

	mWindow->pushGui(new GuiMsgBox(mWindow, _("HOSTNAME CHANGED") + "\n" + _("REBOOT REQUIRED FOR FULL EFFECT"), _("OK")));
}

void GuiMenu::activateExistingConnection()
{
	std::string conns = executeCommand("ls -1 /etc/NetworkManager/system-connections/ 2>/dev/null | sed 's/\\.nmconnection$//'");

	if (conns.empty()) {
		mWindow->pushGui(new GuiMsgBox(mWindow, _("NO SAVED CONNECTIONS"), _("OK")));
		return;
	}

	std::string curSsid = getCurrentWifiSSID();

	auto s = new GuiSettings(mWindow, _("SELECT CONNECTION"));

	std::istringstream stream(conns);
	std::string conn;
	while (std::getline(stream, conn)) {
		if (conn.empty()) continue;

		std::string connName = conn;
		std::string displayName = connName;
		if (connName == curSsid)
			displayName = connName + " [" + _("CONNECTED") + "]";

		s->addEntry(displayName, true, [this, connName] {
			activateConnection(connName);
		}, "");
	}

	mWindow->pushGui(s);
}

void GuiMenu::activateConnection(const std::string& connName)
{
	auto busy = new GuiComponent(mWindow);
	auto busyComp = new BusyComponent(mWindow);
	busy->addChild(busyComp);
	busyComp->setText(_("CONNECTING..."));
	busy->setSize((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
	mWindow->pushGui(busy);

	std::string curSsid = getCurrentWifiSSID();
	executeCommand("systemctl disable --now wifi_monitor.service 2>/dev/null || true");
	if (!curSsid.empty() && curSsid != connName) {
		executeCommand("nmcli con down \"" + curSsid + "\" 2>/dev/null");
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}

	std::string result = executeCommand("nmcli con up \"" + connName + "\" 2>&1");
	std::this_thread::sleep_for(std::chrono::seconds(2));

	mWindow->removeGui(busy);
	delete busy;

	std::string newSsid = getCurrentWifiSSID();
	if (newSsid == connName) {
		if (mWifiStatusText) mWifiStatusText->setText(newSsid);
		executeCommand("nmcli con modify \"" + connName + "\" wifi-sec.psk-flags 0 2>/dev/null || true");
		executeCommand("systemctl enable --now wifi_monitor.service 2>/dev/null || true");
		mWindow->pushGui(new GuiMsgBox(mWindow, _("CONNECTED TO") + "\n" + connName, _("OK")));
	} else {
		if (mWifiStatusText) mWifiStatusText->setText(newSsid.empty() ? _("NOT CONNECTED") : newSsid);
		mWindow->pushGui(new GuiMsgBox(mWindow, _("CONNECTION FAILED") + "\n" + result, _("OK")));
	}
}

void GuiMenu::deleteConnections()
{
	std::string conns = executeCommand("ls -1 /etc/NetworkManager/system-connections/ 2>/dev/null | sed 's/\\.nmconnection$//'");

	if (conns.empty()) {
		mWindow->pushGui(new GuiMsgBox(mWindow, _("NO SAVED CONNECTIONS"), _("OK")));
		return;
	}

	std::string curSsid = getCurrentWifiSSID();

	auto s = new GuiSettings(mWindow, _("DELETE CONNECTION"));

	std::istringstream stream(conns);
	std::string conn;
	while (std::getline(stream, conn)) {
		if (conn.empty()) continue;

		std::string connName = conn;
		std::string displayName = connName;
		if (connName == curSsid)
			displayName = connName + " [" + _("CONNECTED") + "]";

		s->addEntry(displayName, true, [this, connName] {
			mWindow->pushGui(new GuiMsgBox(mWindow,
				_("DELETE CONNECTION") + "?\n" + connName,
				_("YES"), [this, connName] {
					std::string curSsid = getCurrentWifiSSID();

					// if deleting the currently connected network, disconnect first
					if (connName == curSsid) {
						executeCommand("systemctl disable --now wifi_monitor.service 2>/dev/null || true");
						executeCommand("nmcli con down \"" + connName + "\" >/dev/null 2>&1 || true");
						toggleRemoteServices(false);
					}

					executeCommand("nmcli connection delete \"" + connName + "\" >/dev/null 2>&1 || true");
					executeCommand("rm -f \"/etc/NetworkManager/system-connections/" + connName + ".nmconnection\"");

					std::string newSsid = getCurrentWifiSSID();
					if (mWifiStatusText) mWifiStatusText->setText(newSsid.empty() ? _("NOT CONNECTED") : newSsid);

					mWindow->pushGui(new GuiMsgBox(mWindow, _("DELETED"), _("OK")));
				},
				_("NO"), nullptr));
		}, "");
	}

	mWindow->pushGui(s);
}

void GuiMenu::openNetworkSettings()
{
	auto s = new GuiSettings(mWindow, _("NETWORK SETTINGS"));

	// --- Current Network ---
	std::string wifiStatus = getCurrentWifiSSID();
    if (wifiStatus.empty()) {
        wifiStatus = _("NOT CONNECTED");
    }
    mWifiStatusText = std::make_shared<TextComponent>(mWindow, wifiStatus, ThemeData::getMenuTheme()->Text.font, ThemeData::getMenuTheme()->Text.color);
    mWifiStatusText->setLineSpacing(1.0f);
    s->addWithLabel(_("CURRENT NETWORK"), mWifiStatusText);

	// --- Hostname (affiché seulement si SSH ou Samba actif) ---
	{
		bool sshActive  = Settings::getInstance()->getBool("SshEnabled");
		bool sambaActive = Settings::getInstance()->getBool("SambaEnabled");
		if (sshActive || sambaActive)
		{
			char buf[64] = {0};
			FILE* f = popen("hostname 2>/dev/null", "r");
			if (f) { fgets(buf, sizeof(buf), f); pclose(f); }
			std::string hn(buf);
			if (!hn.empty() && hn.back() == '\n') hn.pop_back();
			if (!hn.empty())
			{
				auto hostnameText = std::make_shared<TextComponent>(mWindow, hn, ThemeData::getMenuTheme()->Text.font, ThemeData::getMenuTheme()->Text.color, ALIGN_RIGHT);

				ComponentListRow hostnameRow;
				auto hostnameLbl = std::make_shared<TextComponent>(mWindow, _("HOSTNAME"), ThemeData::getMenuTheme()->Text.font, ThemeData::getMenuTheme()->Text.color);
				hostnameRow.addElement(hostnameLbl, true);
				hostnameRow.addElement(hostnameText, true);

				hostnameRow.makeAcceptInputHandler([this, hostnameText] {
					showHostnameInput(hostnameText);
				});

				s->addRow(hostnameRow);
			}
		}
	}

	// --- IP Address (affichée seulement si WiFi connecté) ---
	{
		char buf[64] = {0};
		FILE* f = popen("ip -4 addr show wlan0 2>/dev/null | grep -oP '(?<=inet )[\.0-9]+'", "r");
		if (f) { fgets(buf, sizeof(buf), f); pclose(f); }
		std::string ip(buf);
		if (!ip.empty() && ip.back() == '\n') ip.pop_back();
		if (!ip.empty())
		{
			auto ipText = std::make_shared<TextComponent>(mWindow, ip, ThemeData::getMenuTheme()->Text.font, ThemeData::getMenuTheme()->Text.color);
			s->addWithLabel(_("IP ADDRESS"), ipText);
		}
	}

	// --- Gateway (affichée seulement si disponible) ---
	{
		char buf[64] = {0};
		FILE* f = popen("ip r 2>/dev/null | grep default | awk '{print $3}'", "r");
		if (f) { fgets(buf, sizeof(buf), f); pclose(f); }
		std::string gateway(buf);
		if (!gateway.empty() && gateway.back() == '\n') gateway.pop_back();
		if (!gateway.empty())
		{
			auto gatewayText = std::make_shared<TextComponent>(mWindow, gateway, ThemeData::getMenuTheme()->Text.font, ThemeData::getMenuTheme()->Text.color);
			s->addWithLabel(_("GATEWAY"), gatewayText);
		}
	}

	// --- DNS (affichée seulement si disponible) ---
	{
		char buf[64] = {0};
		FILE* f = popen("nmcli dev show wlan0 2>/dev/null | grep DNS | awk '{print $2}' | head -1", "r");
		if (f) { fgets(buf, sizeof(buf), f); pclose(f); }
		std::string dns(buf);
		if (!dns.empty() && dns.back() == '\n') dns.pop_back();
		if (!dns.empty())
		{
			auto dnsText = std::make_shared<TextComponent>(mWindow, dns, ThemeData::getMenuTheme()->Text.font, ThemeData::getMenuTheme()->Text.color);
			s->addWithLabel(_("DNS"), dnsText);
		}
	}

	// WiFi enable/disable toggle
	bool wifiInitialState = !isWifiRfkillBlocked();
	auto wifiSwitch = std::make_shared<SwitchComponent>(mWindow);
	wifiSwitch->setState(wifiInitialState);
	wifiSwitch->setOnChangedCallback([wifiSwitch] {
		std::string cmd = wifiSwitch->getState()
			? "/usr/local/bin/wifi_enable.sh"
			: "/usr/local/bin/wifi_disable.sh";
		system(cmd.c_str());
		Settings::getInstance()->setBool("networkIcon", wifiSwitch->getState());
	});
	s->addWithLabel(_("WIFI ENABLED"), wifiSwitch);
	s->addSaveFunc([s, wifiSwitch, wifiInitialState]
	{
		if (wifiSwitch->getState() != wifiInitialState)
			s->setVariable("reloadAll", true);
	});

	// --- WiFi Network Actions ---
	s->addEntry(_("SCAN WIFI NETWORKS"), true, [this] {
		scanWifi();
	}, "");

	s->addEntry(_("ACTIVATE EXISTING CONNECTION"), true, [this] {
		activateExistingConnection();
	}, "");

	s->addEntry(_("DELETE EXISTING CONNECTIONS"), true, [this] {
		deleteConnections();
	}, "");

    // --- Remote Services toggle (SSH, Samba, FileBrowser, NTP) ---
    bool remoteEnabled = isRemoteServicesEnabled();
        auto remoteSwitch = std::make_shared<SwitchComponent>(mWindow);
    remoteSwitch->setState(remoteEnabled);
    remoteSwitch->setOnChangedCallback([remoteSwitch] {
        bool enable = remoteSwitch->getState();
        std::thread([enable] {
            toggleRemoteServices(enable);
        }).detach();
    });
    s->addWithLabel(_("REMOTE SERVICES"), remoteSwitch);

    // --- Remote Services Auto-Start toggle ---
    bool autoStartEnabled = isRemoteServicesAutoStart();
        auto autoStartSwitch = std::make_shared<SwitchComponent>(mWindow);
    autoStartSwitch->setState(autoStartEnabled);
    autoStartSwitch->setOnChangedCallback([autoStartSwitch] {
        bool enable = autoStartSwitch->getState();
        std::thread([enable] {
            toggleRemoteServicesAutoStart(enable);
        }).detach();
    });
    s->addWithLabel(_("REMOTE SERVICES AUTO-START"), autoStartSwitch);

	// --- Samba Root Access toggle ---
	auto sambaRootSwitch = std::make_shared<SwitchComponent>(mWindow);
	sambaRootSwitch->setState(isSambaRootAccessEnabled());
	sambaRootSwitch->setOnChangedCallback([sambaRootSwitch] {
		bool enable = sambaRootSwitch->getState();
		std::thread([enable] {
			toggleSambaRootAccess(enable);
		}).detach();
	});
	s->addWithLabel(_("ROOT SAMBA ACCESS"), sambaRootSwitch);

	// --- WiFi Monitor Service toggle ---
	std::string wifiMonitorState = executeCommand("systemctl is-enabled wifi_monitor.service 2>/dev/null");
	bool wifiMonitorEnabled = wifiMonitorState.find("masked") == std::string::npos;

	auto wifiMonitorSwitch = std::make_shared<SwitchComponent>(mWindow);
	wifiMonitorSwitch->setState(wifiMonitorEnabled);

	wifiMonitorSwitch->setOnChangedCallback([wifiMonitorSwitch] {
		if (wifiMonitorSwitch->getState())
			executeCommand("sudo systemctl unmask wifi_monitor.service && sudo systemctl start wifi_monitor.service");
		else
			executeCommand("sudo systemctl stop wifi_monitor.service && sudo systemctl mask wifi_monitor.service");
	});

	s->addWithLabel(_("WIFI MONITOR SERVICE"), wifiMonitorSwitch);

	// --- Bluetooth Manager ---
	s->addEntry(_("BLUETOOTH MANAGER"), false, [this] {
		if (access("/usr/local/bin/BT Manager.sh", F_OK) == 0)
		{
			AudioManager::getInstance()->deinit();
			VolumeControl::getInstance()->deinit();
			mWindow->deinit(true);
			system("/bin/bash \"/usr/local/bin/BT Manager.sh\" 2>&1 > /dev/tty1");
			mWindow->init(true);
			VolumeControl::getInstance()->init();
			AudioManager::getInstance()->init();
		}
		else
			mWindow->pushGui(new GuiMsgBox(mWindow, _("BLUETOOTH MANAGER NOT FOUND\n/usr/local/bin/BT Manager.sh"), _("OK")));
	}, "iconBluetooth");

	s->onFinalize([s, this] {
		if (s->getVariable("reloadAll"))
			ViewController::get()->reloadAll(mWindow);
	});

	mWindow->pushGui(s);
}

void GuiMenu::openBatterySettings()
{
	auto s = new GuiSettings(mWindow, _("BATTERYPLUS SETTINGS"));

	// --- Statut BatteryPlus ---
	{
		char buf[32] = {0};
		FILE* f = popen("systemctl is-active batteryplus 2>/dev/null", "r");
		if (f) { fgets(buf, sizeof(buf), f); pclose(f); }
		std::string status(buf);
		if (!status.empty() && status.back() == '\n') status.pop_back();
		std::string statusLabel = (status == "active") ? _("ACTIVE") : _("INACTIVE");
		s->addEntry(_("BATTERYPLUS STATUS") + ": " + statusLabel, false, nullptr);
	}

	// --- Statut calibration ---
	{
		bool calibrated = (access("/home/ark/.config/batteryplus/batteryplus-calibrated", F_OK) == 0);
		std::string calLabel = calibrated ? _("CALIBRATED") : _("NOT CALIBRATED");
		s->addEntry(_("CALIBRATION") + ": " + calLabel, false, nullptr);
	}

	// --- Pourcentage actuel ---
	{
		char buf[16] = {0};
		FILE* f = popen("cat /tmp/battery.percent 2>/dev/null", "r");
		if (f) { fgets(buf, sizeof(buf), f); pclose(f); }
		std::string pct(buf);
		if (!pct.empty() && pct.back() == '\n') pct.pop_back();
		if (pct.empty()) pct = "N/A";
		s->addEntry(_("BATTERY LEVEL") + ": " + pct + "%", false, nullptr);
	}

	// --- Toggle BatteryPlus ---
	auto batteryPlusEnabled = std::make_shared<SwitchComponent>(mWindow);
	batteryPlusEnabled->setState(Settings::getInstance()->getBool("BatteryPlusEnabled"));
	s->addWithLabel(_("BATTERYPLUS ENABLED"), batteryPlusEnabled);
	batteryPlusEnabled->setOnChangedCallback([batteryPlusEnabled] {
		Settings::getInstance()->setBool("BatteryPlusEnabled", batteryPlusEnabled->getState());
		Settings::getInstance()->saveFile();
		if (batteryPlusEnabled->getState()) {
			runSystemCommand("sudo -n systemctl start batteryplus 2>/dev/null", "", nullptr);
			runSystemCommand("sudo -n systemctl enable batteryplus 2>/dev/null", "", nullptr);
		} else {
			runSystemCommand("sudo -n systemctl stop batteryplus 2>/dev/null", "", nullptr);
			runSystemCommand("sudo -n systemctl disable batteryplus 2>/dev/null", "", nullptr);
		}
	});

	// --- Mode voltage/pmic ---
	auto batteryMode = std::make_shared<OptionListComponent<std::string>>(mWindow, _("BATTERYPLUS MODE"), false);
	batteryMode->add(_("VOLTAGE"), "voltage", Settings::getInstance()->getString("BatteryPlusMode") == "voltage");
	batteryMode->add(_("PMIC"), "pmic", Settings::getInstance()->getString("BatteryPlusMode") == "pmic");
	s->addWithLabel(_("BATTERYPLUS MODE"), batteryMode);
	s->addSaveFunc([batteryMode] {
		if (Settings::getInstance()->setString("BatteryPlusMode", batteryMode->getSelected())) {
			std::string cmd = "sudo -n sed -i 's/^mode=.*/mode=" + batteryMode->getSelected() + "/' /etc/batteryplus/batteryplus.conf 2>/dev/null";
			runSystemCommand(cmd, "", nullptr);
			runSystemCommand("sudo -n systemctl restart batteryplus 2>/dev/null", "", nullptr);
		}
	});

	// --- Reset calibration ---
	s->addEntry(_("RESET CALIBRATION"), false, [this] {
		mWindow->pushGui(new GuiMsgBox(mWindow,
			_("RESET BATTERYPLUS CALIBRATION?\nThis will delete learned voltage anchors."),
			_("YES"), [] {
				runSystemCommand("sudo -n rm -f /home/ark/.config/batteryplus/batteryplus-calibrated /home/ark/.config/batteryplus/batteryplus-voltage.map 2>/dev/null", "", nullptr);
				runSystemCommand("sudo -n systemctl restart batteryplus 2>/dev/null", "", nullptr);
			},
			_("NO"), nullptr));
	});

	mWindow->pushGui(s);
}

static void setBootVolume(const std::string& percent)
{
	if (percent == "Default")
	{
		executeCommand("sudo systemctl disable boot_volume.service 2>/dev/null || true");
		executeCommand("sudo rm -f /usr/local/bin/boot_volume.sh");
		return;
	}

	std::string card = executeCommand("aplay -l 2>/dev/null | grep -m1 -oP 'card \\K[0-9]+(?=.*rk817)'");
	card.erase(std::remove_if(card.begin(), card.end(), ::isspace), card.end());
	if (card.empty())
		card = "0";

	std::string createCmd =
		"sudo bash -c 'cat > /usr/local/bin/boot_volume.sh << \"EOF\"\n"
		"#!/bin/bash\n"
		"sleep 2\n"
		"amixer -c " + card + " -q sset Playback " + percent + "%\n"
		"EOF'";
	executeCommand(createCmd);
	executeCommand("sudo chmod +x /usr/local/bin/boot_volume.sh");
	executeCommand("sudo systemctl enable boot_volume.service 2>/dev/null || true");
	executeCommand("sudo systemctl daemon-reload");
}

void GuiMenu::openSoundSettings()
{
	auto s = new GuiSettings(mWindow, _("SOUND SETTINGS"));
	
	// volume
	auto volume = std::make_shared<SliderComponent>(mWindow, 0.f, 100.f, 1.f, "%");
	volume->setValue((float)VolumeControl::getInstance()->getVolume());
	volume->setOnValueChanged([](const float &newVal) { VolumeControl::getInstance()->setVolume((int)Math::round(newVal)); });
	s->addWithLabel(_("SYSTEM VOLUME"), volume);
	//s->addSaveFunc([volume] { VolumeControl::getInstance()->setVolume((int)Math::round(volume->getValue())); });

	if (UIModeController::getInstance()->isUIModeFull())
	{
#if defined(__linux__)
		// audio card
		auto audio_card = std::make_shared< OptionListComponent<std::string> >(mWindow, _("AUDIO CARD"), false);
		std::vector<std::string> audio_cards;
	#ifdef _RPI_
		// RPi Specific  Audio Cards
		audio_cards.push_back("local");
		audio_cards.push_back("hdmi");
		audio_cards.push_back("both");
	#endif
		audio_cards.push_back("default");
		audio_cards.push_back("sysdefault");
		audio_cards.push_back("dmix");
		audio_cards.push_back("hw");
		audio_cards.push_back("plughw");
		audio_cards.push_back("null");
		if (Settings::getInstance()->getString("AudioCard") != "") {
			if(std::find(audio_cards.begin(), audio_cards.end(), Settings::getInstance()->getString("AudioCard")) == audio_cards.end()) {
				audio_cards.push_back(Settings::getInstance()->getString("AudioCard"));
			}
		}
		for(auto ac = audio_cards.cbegin(); ac != audio_cards.cend(); ac++)
			audio_card->add(*ac, *ac, Settings::getInstance()->getString("AudioCard") == *ac);
		s->addWithLabel(_("AUDIO CARD"), audio_card);
		s->addSaveFunc([audio_card] {
			Settings::getInstance()->setString("AudioCard", audio_card->getSelected());
			VolumeControl::getInstance()->deinit();
			VolumeControl::getInstance()->init();
		});

		// volume control device
		auto vol_dev = std::make_shared< OptionListComponent<std::string> >(mWindow, _("AUDIO DEVICE"), false);
		std::vector<std::string> transitions;
		transitions.push_back("PCM");
		transitions.push_back("Speaker");
		transitions.push_back("Master");
		transitions.push_back("Digital");
		transitions.push_back("Analogue");
		if (Settings::getInstance()->getString("AudioDevice") != "") {
			if(std::find(transitions.begin(), transitions.end(), Settings::getInstance()->getString("AudioDevice")) == transitions.end()) {
				transitions.push_back(Settings::getInstance()->getString("AudioDevice"));
			}
		}
		for(auto it = transitions.cbegin(); it != transitions.cend(); it++)
			vol_dev->add(*it, *it, Settings::getInstance()->getString("AudioDevice") == *it);
		s->addWithLabel(_("AUDIO DEVICE"), vol_dev);
		s->addSaveFunc([vol_dev] {
			Settings::getInstance()->setString("AudioDevice", vol_dev->getSelected());
			VolumeControl::getInstance()->deinit();
			VolumeControl::getInstance()->init();
		});
#endif
		auto volumePopup = std::make_shared<SwitchComponent>(mWindow);
		volumePopup->setState(Settings::getInstance()->getBool("VolumePopup"));
		s->addWithLabel(_("SHOW OVERLAY WHEN VOLUME CHANGES"), volumePopup);
		s->addSaveFunc([volumePopup]
			{
				bool old_value = Settings::getInstance()->getBool("VolumePopup");
				if (old_value != volumePopup->getState())
					Settings::getInstance()->setBool("VolumePopup", volumePopup->getState());
			}
		);

        //Flip volume button function primarily for RGB10X unit
        std::string isitpowkiddy=(getShOutput(R"(if [ -f "/boot/rk3326-rg351mp-linux.dtb" ]; then echo "YES"; else echo "NO"; fi)"));
        if (isitpowkiddy.compare("YES")==0) {
          auto volBtns = std::make_shared<SwitchComponent>(mWindow);
          volBtns->setState(Settings::getInstance()->getBool("InvertVolBtns"));
          s->addWithLabel(_("FLIP VOLUME BUTTONS"), volBtns);
          s->addSaveFunc([this, s, volBtns]
          {
            if (Settings::getInstance()->setBool("InvertVolBtns", volBtns->getState()))
              {
                if (Settings::getInstance()->getBool("InvertVolBtns") == 1)
                  runSystemCommand("[ -z $(find /home/ark/.config/.SWAPVOLUMEBUTTONS) ] && touch /home/ark/.config/.SWAPVOLUMEBUTTONS", "", nullptr);
                else
                  runSystemCommand("[ ! -z $(find /home/ark/.config/.SWAPVOLUMEBUTTONS) ] && rm /home/ark/.config/.SWAPVOLUMEBUTTONS", "", nullptr);
              }
          });
        }

		// disable sounds
		auto music_enabled = std::make_shared<SwitchComponent>(mWindow);
		music_enabled->setState(Settings::getInstance()->getBool("audio.bgmusic"));
		s->addWithLabel(_("FRONTEND MUSIC"), music_enabled);
		s->addSaveFunc([music_enabled] {
			Settings::getInstance()->setBool("audio.bgmusic", music_enabled->getState());
			if (music_enabled->getState())
				AudioManager::getInstance()->playRandomMusic();
			else
				AudioManager::getInstance()->stopMusic();
		});

		//display music titles
		auto display_titles = std::make_shared<SwitchComponent>(mWindow);
		display_titles->setState(Settings::getInstance()->getBool("MusicTitles"));
		s->addWithLabel(_("DISPLAY SONG TITLES"), display_titles);
		s->addSaveFunc([display_titles] {
			Settings::getInstance()->setBool("MusicTitles", display_titles->getState());
		});

		// music per system
		auto music_per_system = std::make_shared<SwitchComponent>(mWindow);
		music_per_system->setState(Settings::getInstance()->getBool("audio.persystem"));
		s->addWithLabel(_("ONLY PLAY SYSTEM-SPECIFIC MUSIC FOLDER"), music_per_system);
		s->addSaveFunc([music_per_system] {
			Settings::getInstance()->setBool("audio.persystem", music_per_system->getState());
		});
		
		// batocera - music per system
		auto enableThemeMusics = std::make_shared<SwitchComponent>(mWindow);
		enableThemeMusics->setState(Settings::getInstance()->getBool("audio.thememusics"));
		s->addWithLabel(_("PLAY THEME MUSICS"), enableThemeMusics);
		s->addSaveFunc([enableThemeMusics] {
			if (Settings::getInstance()->setBool("audio.thememusics", enableThemeMusics->getState()))
				AudioManager::getInstance()->themeChanged(ViewController::get()->getState().getSystem()->getTheme(), true);
		});

		// disable sounds
		auto sounds_enabled = std::make_shared<SwitchComponent>(mWindow);
		sounds_enabled->setState(Settings::getInstance()->getBool("EnableSounds"));
		s->addWithLabel(_("ENABLE NAVIGATION SOUNDS"), sounds_enabled);
		s->addSaveFunc([sounds_enabled] {
			if (sounds_enabled->getState()
				&& !Settings::getInstance()->getBool("EnableSounds")
				&& PowerSaver::getMode() == PowerSaver::INSTANT)
			{
				Settings::getInstance()->setString("PowerSaverMode", "default");
				PowerSaver::init();
			}
			Settings::getInstance()->setBool("EnableSounds", sounds_enabled->getState());
		});

		auto video_audio = std::make_shared<SwitchComponent>(mWindow);
		video_audio->setState(Settings::getInstance()->getBool("VideoAudio"));
		s->addWithLabel(_("ENABLE VIDEO AUDIO"), video_audio);
		s->addSaveFunc([video_audio] { Settings::getInstance()->setBool("VideoAudio", video_audio->getState()); });

		auto videolowermusic = std::make_shared<SwitchComponent>(mWindow);
		videolowermusic->setState(Settings::getInstance()->getBool("VideoLowersMusic"));
		s->addWithLabel(_("LOWER MUSIC WHEN PLAYING VIDEO"), videolowermusic);
		s->addSaveFunc([videolowermusic] { Settings::getInstance()->setBool("VideoLowersMusic", videolowermusic->getState()); });

	// Verbal Battery Voices
	auto VerbalbatteryVoice = std::make_shared<OptionListComponent<std::string> >(mWindow, _("Verbal BATTERY Voice"), false);
	VerbalbatteryVoice->addRange({ { _("MALE 1"), "male1" },{ _("MALE 2"), "male2" },{ _("FEMALE"), "female" } }, Settings::getInstance()->getString("VerbalBatteryVoice"));
	s->addWithLabel(_("VERBAL BATTERY VOICE"), VerbalbatteryVoice);
	s->addSaveFunc([s, VerbalbatteryVoice]
	{
		std::string old_value = Settings::getInstance()->getString("VerbalBatteryVoice");
		if (old_value != VerbalbatteryVoice->getSelected())
           {
            if (strstr(VerbalbatteryVoice->getSelected().c_str(),"male1")) {
              runSystemCommand("[ ! -z $(find /home/ark/.config/.MBROLA_VOICE*) ] && rm /home/ark/.config/.MBROLA_VOICE*", "", nullptr);
            }
            else if (strstr(VerbalbatteryVoice->getSelected().c_str(),"male2")) {
              runSystemCommand("[ ! -z $(find /home/ark/.config/.MBROLA_VOICE*) ] && rm /home/ark/.config/.MBROLA_VOICE*", "", nullptr);
              runSystemCommand("touch /home/ark/.config/.MBROLA_VOICE_MALE3", "", nullptr);
            }
            else {
              runSystemCommand("[ ! -z $(find /home/ark/.config/.MBROLA_VOICE*) ] && rm /home/ark/.config/.MBROLA_VOICE*", "", nullptr);
              runSystemCommand("touch /home/ark/.config/.MBROLA_VOICE_FEMALE", "", nullptr);
            }
			Settings::getInstance()->setString("VerbalBatteryVoice", VerbalbatteryVoice->getSelected());
		   }
	});

	// Verbal Battery Warning indicator
	auto VerbalbatteryStatus = std::make_shared<OptionListComponent<std::string> >(mWindow, _("Verbal BATTERY Warning"), false);
	VerbalbatteryStatus->addRange({ { _("OFF"), "no" },{ _("ON"), "yes" } }, Settings::getInstance()->getString("VerbalBatteryWarning"));
	s->addWithLabel(_("VERBAL BATTERY WARNING"), VerbalbatteryStatus);
	s->addSaveFunc([s, VerbalbatteryStatus]
	{
		std::string old_value = Settings::getInstance()->getString("VerbalBatteryWarning");
		if (old_value != VerbalbatteryStatus->getSelected())
           {
            if (strstr(VerbalbatteryStatus->getSelected().c_str(),"no")) {
              runSystemCommand("[ -z $(find /home/ark/.config/.NOVERBALBATTERYWARNING) ] && touch /home/ark/.config/.NOVERBALBATTERYWARNING", "", nullptr);
            }
            else {
              runSystemCommand("[ ! -z $(find /home/ark/.config/.NOVERBALBATTERYWARNING) ] && rm /home/ark/.config/.NOVERBALBATTERYWARNING", "", nullptr);
            }
			Settings::getInstance()->setString("VerbalBatteryWarning", VerbalbatteryStatus->getSelected());
		   }
	});

	// Verbal Battery Warning threshold
	auto Thold = std::make_shared< OptionListComponent<std::string> >(mWindow, _("Verbal BATTERY Warning Threshold (%)"), false);
	std::vector<std::string> athreshold;
	athreshold.push_back("Default");
	athreshold.push_back("5");
	athreshold.push_back("10");
	athreshold.push_back("15");
	athreshold.push_back("20");
	athreshold.push_back("25");
	athreshold.push_back("30");
	athreshold.push_back("35");
	athreshold.push_back("40");
	athreshold.push_back("45");
	athreshold.push_back("50");

	auto threshold = Settings::getInstance()->getString("VerbalBatteryThreshold");
	if (threshold.empty())
		threshold = "Default";

	for (auto it = athreshold.cbegin(); it != athreshold.cend(); it++)
		Thold->add(_(it->c_str()), *it, threshold == *it);

	s->addWithLabel(_("Verbal BATTERY Warning Threshold (%)"), Thold);
	s->addSaveFunc([this, Thold] { Settings::getInstance()->setString("VerbalBatteryThreshold", Thold->getSelected());
		if (Thold->changed()) {
		    if (strstr(Thold->getSelected().c_str(),"Default")) {
		      runSystemCommand("[ ! -z $(find /home/ark/.config/.CUSTOM_BATT_LIFE_WARNING) ] && rm -f /home/ark/.config/.CUSTOM_BATT_LIFE_WARNING", "", nullptr);
		    }
		    else {
		      runSystemCommand("echo " + Settings::getInstance()->getString("VerbalBatteryThreshold") + " > /home/ark/.config/.CUSTOM_BATT_LIFE_WARNING", "", nullptr);
		    }
		}
	});

	// Boot Volume
	auto BootVol = std::make_shared< OptionListComponent<std::string> >(mWindow, _("BOOT VOLUME (%)"), false);
	std::vector<std::string> abootvol;
	abootvol.push_back("Default");
	for (int v = 5; v <= 100; v += 5)
		abootvol.push_back(std::to_string(v));

	std::string bootvol = "Default";
	std::string bootvolSvc = executeCommand("systemctl is-enabled boot_volume.service 2>/dev/null");
	bootvolSvc.erase(std::remove_if(bootvolSvc.begin(), bootvolSvc.end(), ::isspace), bootvolSvc.end());
	if (bootvolSvc == "enabled")
	{
		std::string current = executeCommand("grep -oE '[0-9]+%' /usr/local/bin/boot_volume.sh 2>/dev/null | tail -1");
		current.erase(std::remove_if(current.begin(), current.end(), ::isspace), current.end());
		if (!current.empty())
			bootvol = current.substr(0, current.size() - 1);
	}

	for (auto it = abootvol.cbegin(); it != abootvol.cend(); it++)
		BootVol->add(_(it->c_str()), *it, bootvol == *it);

	s->addWithLabel(_("BOOT VOLUME (%)"), BootVol);
	s->addSaveFunc([BootVol] {
		if (BootVol->changed())
			setBootVolume(BootVol->getSelected());
	});

#ifdef _RPI_
		// OMX player Audio Device
		auto omx_audio_dev = std::make_shared< OptionListComponent<std::string> >(mWindow, "OMX PLAYER AUDIO DEVICE", false);
		std::vector<std::string> omx_cards;
		// RPi Specific  Audio Cards
		omx_cards.push_back("local");
		omx_cards.push_back("hdmi");
		omx_cards.push_back("both");
		omx_cards.push_back("alsa:hw:0,0");
		omx_cards.push_back("alsa:hw:1,0");
		if (Settings::getInstance()->getString("OMXAudioDev") != "") {
			if (std::find(omx_cards.begin(), omx_cards.end(), Settings::getInstance()->getString("OMXAudioDev")) == omx_cards.end()) {
				omx_cards.push_back(Settings::getInstance()->getString("OMXAudioDev"));
			}
		}
		for (auto it = omx_cards.cbegin(); it != omx_cards.cend(); it++)
			omx_audio_dev->add(*it, *it, Settings::getInstance()->getString("OMXAudioDev") == *it);
		s->addWithLabel("OMX PLAYER AUDIO DEVICE", omx_audio_dev);
		s->addSaveFunc([omx_audio_dev] {
			if (Settings::getInstance()->getString("OMXAudioDev") != omx_audio_dev->getSelected())
				Settings::getInstance()->setString("OMXAudioDev", omx_audio_dev->getSelected());
		});
#endif
	}

	s->updatePosition();
	mWindow->pushGui(s);

}

std::string GuiMenu::getCpuBinning()
{
    // Try to get CPU binning info from dmesg (added by rockchip-cpufreq.c)
    // Format: es_info: cpu_bin=X process=X scale=X volt_sel=X
    // volt_sel is the actual quality grade based on CPU leakage:
    // - lower volt_sel = higher leakage = worse quality = needs higher voltage
    // - higher volt_sel = lower leakage = better quality = can run at lower voltage
    std::string dmesgBin = executeCommand("dmesg | grep 'es_info: cpu_bin=' | tail -1");
    
    if (!dmesgBin.empty()) {
        // Parse the volt_sel value (actual quality indicator)
        std::string voltSel = executeCommand("echo '" + dmesgBin + "' | sed 's/.*volt_sel=\\(-*[0-9]*\\).*/\\1/'");
        voltSel.erase(std::remove_if(voltSel.begin(), voltSel.end(), ::isspace), voltSel.end());
        
        int voltVal = atoi(voltSel.c_str());
        
        // Rockchip CPU quality grades based on volt_sel:
        // volt_sel=0: L0 - higher leakage, needs more voltage
        // volt_sel=1: L1 - slightly better than L0
        // volt_sel=2: L2 - standard quality
        // volt_sel=3: L3 - lowest leakage, can run at lowest voltage
        // negative value: N/A - not detected
        
        if (voltVal < 0) return "N/A";
        if (voltVal == 0) return "L0 (" + std::string(_("AVERAGE")) + ")";
        if (voltVal == 1) return "L1 (" + std::string(_("POOR")) + ")";
        if (voltVal == 2) return "L2 (" + std::string(_("STANDARD")) + ")";
        if (voltVal == 3) return "L3 (" + std::string(_("BEST")) + ")";
        
        return "L" + std::to_string(voltVal) + " (" + std::string(_("AVERAGE")) + ")";
    }
    
    return "N/A";
}

std::string GuiMenu::getCpuTemp()
{
    std::string temp = executeCommand("cat /sys/class/thermal/thermal_zone0/temp 2>/dev/null");
    temp.erase(std::remove_if(temp.begin(), temp.end(), ::isspace), temp.end());
    
    if (!temp.empty()) {
        // Convert millidegree to degree
        int tempVal = atoi(temp.c_str());
        if (tempVal > 1000) {
            tempVal = tempVal / 1000;
        }
        return std::to_string(tempVal) + "°C";
    }
    
    return _("N/A");
}

int GuiMenu::getCpuCoreCount()
{
    std::string result = executeCommand("ls -d /sys/devices/system/cpu/cpu[0-9]* 2>/dev/null | wc -l");
    return atoi(result.c_str());
}

int GuiMenu::getOnlineCpuCount()
{
    int count = 0;
    int total = getCpuCoreCount();
    for (int i = 0; i < total; i++) {
        std::string result = executeCommand("cat /sys/devices/system/cpu/cpu" + std::to_string(i) + "/online 2>/dev/null");
        // Remove all whitespace including newlines
        result.erase(std::remove_if(result.begin(), result.end(), ::isspace), result.end());
        if (result == "1" || result.empty()) {  // cpu0 has no online file but is always on
            count++;
        }
    }
    return count;
}

std::string GuiMenu::getCpuGovernor()
{
    std::string result = executeCommand("cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null");
    // Remove all whitespace including newlines
    result.erase(std::remove_if(result.begin(), result.end(), ::isspace), result.end());
    return result;
}

void GuiMenu::setCpuGovernor(const std::string& governor)
{
    int cores = getCpuCoreCount();
    for (int i = 0; i < cores; i++) {
        executeCommand("echo " + governor + " | sudo tee /sys/devices/system/cpu/cpu" + std::to_string(i) + "/cpufreq/scaling_governor >/dev/null 2>&1");
    }
}

std::string GuiMenu::getCpuMaxFreq()
{
    std::string result = executeCommand("cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq 2>/dev/null");
    result.erase(std::remove_if(result.begin(), result.end(), ::isspace), result.end());
    return result;
}

void GuiMenu::setCpuMaxFreq(const std::string& freq)
{
    int cores = getCpuCoreCount();
    for (int i = 0; i < cores; i++) {
        executeCommand("echo " + freq + " | sudo tee /sys/devices/system/cpu/cpu" + std::to_string(i) + "/cpufreq/scaling_max_freq >/dev/null 2>&1");
    }
}

std::vector<std::string> GuiMenu::getCpuAvailableFreqs()
{
    std::string result = executeCommand("cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies 2>/dev/null");
    std::vector<std::string> freqs;
    std::istringstream stream(result);
    std::string freq;
    while (stream >> freq) {
        freq.erase(std::remove_if(freq.begin(), freq.end(), ::isspace), freq.end());
        if (!freq.empty()) freqs.push_back(freq);
    }
    return freqs;
}

std::vector<std::string> GuiMenu::getAvailableGovernors()
{
    std::string result = executeCommand("cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_available_governors 2>/dev/null");
    std::vector<std::string> governors;
    std::istringstream stream(result);
    std::string gov;
    while (stream >> gov) {
        gov.erase(std::remove_if(gov.begin(), gov.end(), ::isspace), gov.end());
        if (!gov.empty()) governors.push_back(gov);
    }
    return governors;
}

void GuiMenu::setCpuCores(int count)
{
    int totalCores = getCpuCoreCount();
    // Enable all cores first
    for (int i = 0; i < totalCores; i++) {
        executeCommand("echo 1 | sudo tee /sys/devices/system/cpu/cpu" + std::to_string(i) + "/online >/dev/null 2>&1");
    }
    // Disable cores beyond count (keep cpu0 always on)
    for (int i = count; i < totalCores; i++) {
        executeCommand("echo 0 | sudo tee /sys/devices/system/cpu/cpu" + std::to_string(i) + "/online >/dev/null 2>&1");
    }
}

bool GuiMenu::isCpuBootApplyEnabled()
{
    std::string result = executeCommand("systemctl is-enabled cpu-governor.service 2>/dev/null");
    result.erase(std::remove_if(result.begin(), result.end(), ::isspace), result.end());
    return result == "enabled";
}

void GuiMenu::toggleCpuBootApply(bool enable)
{
    if (enable) {
        executeCommand("sudo systemctl enable cpu-governor.service 2>/dev/null || true");
    } else {
        executeCommand("sudo systemctl disable cpu-governor.service 2>/dev/null || true");
    }
}

bool GuiMenu::hasGpuFreqControl()
{
    std::string result = executeCommand("ls -d /sys/class/devfreq/ff400000.gpu 2>/dev/null");
    return !Utils::String::trim(result).empty();
}

std::string GuiMenu::getGpuDevPath()
{
    return "/sys/class/devfreq/ff400000.gpu/";
}

std::string GuiMenu::getGpuMaxFreq()
{
    std::string gpuPath = getGpuDevPath();
    if (gpuPath.empty()) return "";
    std::string result = executeCommand("cat " + gpuPath + "max_freq 2>/dev/null");
    result.erase(std::remove_if(result.begin(), result.end(), ::isspace), result.end());
    return result;
}

void GuiMenu::setGpuMaxFreq(const std::string& freq)
{
    std::string gpuPath = getGpuDevPath();
    if (gpuPath.empty()) return;
    executeCommand("echo " + freq + " | sudo tee " + gpuPath + "max_freq >/dev/null 2>&1");
}

std::vector<std::string> GuiMenu::getGpuAvailableFreqs()
{
    std::string gpuPath = getGpuDevPath();
    if (gpuPath.empty()) return {};
    std::string result = executeCommand("cat " + gpuPath + "available_frequencies 2>/dev/null");
    std::vector<std::string> freqs;
    std::istringstream stream(result);
    std::string freq;
    while (stream >> freq) {
        freq.erase(std::remove_if(freq.begin(), freq.end(), ::isspace), freq.end());
        if (!freq.empty()) freqs.push_back(freq);
    }
    return freqs;
}

bool GuiMenu::isGpuBootApplyEnabled()
{
    std::string result = executeCommand("systemctl is-enabled gpu-freq.service 2>/dev/null");
    result.erase(std::remove_if(result.begin(), result.end(), ::isspace), result.end());
    return result == "enabled";
}

void GuiMenu::toggleGpuBootApply(bool enable)
{
    if (enable) {
        executeCommand("sudo systemctl enable gpu-freq.service 2>/dev/null || true");
    } else {
        executeCommand("sudo systemctl disable gpu-freq.service 2>/dev/null || true");
    }
}

void GuiMenu::writeCpuBootConfig()
{
    std::string gov = getCpuGovernor();
    std::string freq = getCpuMaxFreq();
    std::string content = "GOV=" + gov + "\nFREQ=" + freq + "\n";
    executeCommand("echo '" + content + "' | sudo tee /etc/cpu-settings.conf >/dev/null 2>&1");
}

void GuiMenu::writeGpuBootConfig()
{
    std::string freq = getGpuMaxFreq();
    std::string content = "FREQ=" + freq + "\n";
    executeCommand("echo '" + content + "' | sudo tee /etc/gpu-settings.conf >/dev/null 2>&1");
}

void GuiMenu::writeDmcBootConfig()
{
    std::string freq = getDmcMaxFreq();
    std::string content = "FREQ=" + freq + "\n";
    executeCommand("echo '" + content + "' | sudo tee /etc/dmc-settings.conf >/dev/null 2>&1");
}

bool GuiMenu::isDmcBootApplyEnabled()
{
    std::string result = executeCommand("systemctl is-enabled dmc-governor.service 2>/dev/null");
    result.erase(std::remove_if(result.begin(), result.end(), ::isspace), result.end());
    return result == "enabled";
}

void GuiMenu::toggleDmcBootApply(bool enable)
{
    if (enable) {
        executeCommand("sudo systemctl enable dmc-governor.service 2>/dev/null || true");
    } else {
        executeCommand("sudo systemctl disable dmc-governor.service 2>/dev/null || true");
    }
}

bool GuiMenu::hasDmcFreqControl()
{
    std::string result = executeCommand("ls /sys/class/devfreq/dmc/available_frequencies 2>/dev/null");
    return !Utils::String::trim(result).empty();
}

std::string GuiMenu::getDmcMaxFreq()
{
    std::string result = executeCommand("cat /sys/class/devfreq/dmc/max_freq 2>/dev/null");
    result.erase(std::remove_if(result.begin(), result.end(), ::isspace), result.end());
    return result;
}

void GuiMenu::setDmcMaxFreq(const std::string& freq)
{
    executeCommand("echo " + freq + " | sudo tee /sys/class/devfreq/dmc/max_freq >/dev/null 2>&1");
}

std::vector<std::string> GuiMenu::getDmcAvailableFreqs()
{
    std::string result = executeCommand("cat /sys/class/devfreq/dmc/available_frequencies 2>/dev/null");
    std::vector<std::string> freqs;
    std::istringstream stream(result);
    std::string freq;
    while (stream >> freq) {
        freq.erase(std::remove_if(freq.begin(), freq.end(), ::isspace), freq.end());
        if (!freq.empty()) freqs.push_back(freq);
    }
    return freqs;
}

std::string GuiMenu::getZramSize()
{
    std::string result = executeCommand("cat /sys/block/zram0/disksize 2>/dev/null");
    result.erase(std::remove_if(result.begin(), result.end(), ::isspace), result.end());
    if (result.empty()) return "0";
    long size = atol(result.c_str());
    // Convert to MB
    long mb = size / (1024 * 1024);
    return std::to_string(mb) + "M";
}

bool GuiMenu::isZramEnabled()
{
    std::string result = executeCommand("grep -q '^/dev/zram0' /proc/swaps 2>/dev/null && echo yes || echo no");
    result.erase(std::remove_if(result.begin(), result.end(), ::isspace), result.end());
    return result == "yes";
}

std::string GuiMenu::getZramCompAlgorithm()
{
    std::string result = executeCommand("cat /sys/block/zram0/comp_algorithm 2>/dev/null");
    // Parse current algorithm from format like "[lzo] lz4 lz4hc zstd"
    size_t start = result.find('[');
    size_t end = result.find(']');
    if (start != std::string::npos && end != std::string::npos && end > start) {
        return result.substr(start + 1, end - start - 1);
    }
    return "lz4";
}

std::vector<std::string> GuiMenu::getAvailableZramAlgorithms()
{
    std::vector<std::string> algos;
    std::string result = executeCommand("cat /sys/block/zram0/comp_algorithm 2>/dev/null");
    // Parse format like "[lzo] lz4 lz4hc zstd"
    std::string current;
    for (size_t i = 0; i < result.size(); i++) {
        char c = result[i];
        if (c == '[' || c == ']') continue;
        if (c == ' ' || c == '\n' || c == '\t') {
            if (!current.empty()) {
                algos.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) {
        algos.push_back(current);
    }
    return algos;
}

void GuiMenu::toggleZram(bool enable, const std::string& size,
                                         const std::string& compAlgo)
{
    if (enable) {
        // Disable first if already enabled
        executeCommand("sudo swapoff /dev/zram0 2>/dev/null || true");
        // Reset zram
        executeCommand("echo 1 | sudo tee /sys/block/zram0/reset >/dev/null 2>&1");

        // Set compression algorithm (must be set after reset, before disksize)
        executeCommand("echo " + compAlgo + " | sudo tee /sys/block/zram0/comp_algorithm >/dev/null 2>&1");

        // Convert size string (e.g., "512M") to bytes
        long bytes = 536870912; // default 512M
        if (size == "128M") bytes = 134217728;
        else if (size == "256M") bytes = 268435456;
        else if (size == "512M") bytes = 536870912;
        else if (size == "1024M") bytes = 1073741824;

        // Set size in bytes
        executeCommand("echo " + std::to_string(bytes) + " | sudo tee /sys/block/zram0/disksize >/dev/null 2>&1");
        // Create swap and enable
        executeCommand("sudo mkswap /dev/zram0 >/dev/null 2>&1");
        executeCommand("sudo swapon -p 5 /dev/zram0 >/dev/null 2>&1");
    } else {
        executeCommand("sudo swapoff /dev/zram0 2>/dev/null || true");
        executeCommand("echo 1 | sudo tee /sys/block/zram0/reset >/dev/null 2>&1 || true");
    }
}

void GuiMenu::saveZramConfig(const std::string& size, const std::string& compAlgo)
{
    long bytes = 536870912;
    if (size == "128M") bytes = 134217728;
    else if (size == "256M") bytes = 268435456;
    else if (size == "512M") bytes = 536870912;
    else if (size == "1024M") bytes = 1073741824;

    std::string content = "ENABLED=1\nALGORITHM=" + compAlgo + "\nSIZE=" + std::to_string(bytes) + "\n";
    executeCommand("echo '" + content + "' | sudo tee /etc/zram.conf >/dev/null 2>&1");
}

bool GuiMenu::isZramAutoStart()
{
    std::string result = executeCommand("systemctl is-enabled zram-swap.service 2>/dev/null");
    result.erase(std::remove_if(result.begin(), result.end(), ::isspace), result.end());
    return result == "enabled";
}

void GuiMenu::toggleZramAutoStart(bool enable, const std::string& size,
                                                  const std::string& compAlgo)
{
    if (enable) {
        saveZramConfig(size, compAlgo);
        executeCommand("sudo systemctl enable zram-swap.service 2>/dev/null || true");
    } else {
        executeCommand("sudo systemctl disable zram-swap.service 2>/dev/null || true");
    }
}

void GuiMenu::openPerformanceSettings()
{
	auto s = new GuiSettings(mWindow, _("PERFORMANCE SETTINGS"));
	
	// --- CPU Grade ---
    /* std::string cpuBinning = getCpuBinning();
    auto cpuText = std::make_shared<TextComponent>(mWindow,
        cpuBinning,
        ThemeData::getMenuTheme()->Text.font, ThemeData::getMenuTheme()->Text.color);
    s->addWithLabel(_("CPU GRADE"), cpuText);
	*/
	
	// --- CPU Temp ---
    auto cpuTempText = std::make_shared<TextComponent>(mWindow,
        getCpuTemp(),
        ThemeData::getMenuTheme()->Text.font, ThemeData::getMenuTheme()->Text.color);
    s->addWithLabel(_("CPU TEMP"), cpuTempText);

	// --- CPU Cores ---
    int coreCount = getCpuCoreCount();
    int onlineCount = getOnlineCpuCount();
    LOG(LogDebug) << "CPU totalCores: " << coreCount << " onlineCores: " << onlineCount;
    if (coreCount > 1) {
        auto coreList = std::make_shared<OptionListComponent<std::string>>(mWindow, _("CPU CORES"), false);
        for (int i = 1; i <= coreCount; i++) {
            coreList->add(std::to_string(i), std::to_string(i), i == onlineCount);
        }
        s->addWithLabel(_("CPU CORES"), coreList);
        
        coreList->setSelectedChangedCallback([this](const std::string& val) {
            setCpuCores(atoi(val.c_str()));
        });
    }
	
	// --- CPU Governor ---
    auto governors = getAvailableGovernors();
    if (!governors.empty()) {
        auto govList = std::make_shared<OptionListComponent<std::string>>(mWindow, _("GOVERNOR"), false);
        std::string currentGov = getCpuGovernor();
        LOG(LogDebug) << "CPU currentGov: '" << currentGov << "'";
        bool found = false;
        for (const auto& gov : governors) {
            bool isSelected = (gov == currentGov);
            LOG(LogDebug) << "CPU gov option: '" << gov << "' selected: " << isSelected;
            if (isSelected) found = true;
            govList->add(gov, gov, isSelected);
        }
        if (!found && !governors.empty()) {
            govList->selectFirstItem();
        }
        s->addWithLabel(_("CPU GOVERNOR"), govList);
        
        govList->setSelectedChangedCallback([this](const std::string& val) {
            setCpuGovernor(val);
        });
    }
	
	// --- CPU Frequency ---
    auto cpuFreqs = getCpuAvailableFreqs();
    if (!cpuFreqs.empty()) {
        auto freqList = std::make_shared<OptionListComponent<std::string>>(mWindow, _("MAX FREQ"), false);
        std::string currentFreq = getCpuMaxFreq();
        LOG(LogDebug) << "CPU currentFreq: '" << currentFreq << "'";
        bool found = false;
        for (const auto& freq : cpuFreqs) {
            // Convert kHz to MHz for display
            int mhz = atoi(freq.c_str()) / 1000;
            bool isSelected = (freq == currentFreq);
            LOG(LogDebug) << "CPU freq option: '" << freq << "' selected: " << isSelected;
            if (isSelected) found = true;
            freqList->add(std::to_string(mhz) + " MHz", freq, isSelected);
        }
        if (!found && !cpuFreqs.empty()) {
            freqList->selectFirstItem();
        }
        s->addWithLabel(_("CPU MAX FREQ"), freqList);
        
        freqList->setSelectedChangedCallback([this](const std::string& val) {
            setCpuMaxFreq(val);
        });
    }
	
	// --- CPU Persistence ---
    auto cpuBootApplySwitch = std::make_shared<SwitchComponent>(mWindow);
    cpuBootApplySwitch->setState(isCpuBootApplyEnabled());
    s->addWithLabel(_("CPU APPLY AT BOOT"), cpuBootApplySwitch);
    cpuBootApplySwitch->setOnChangedCallback([this, cpuBootApplySwitch] {
        toggleCpuBootApply(cpuBootApplySwitch->getState());
    });	
	
	// --- GPU Frequency ---
    auto gpuFreqs = getGpuAvailableFreqs();
    if (!gpuFreqs.empty()) {
        auto freqList = std::make_shared<OptionListComponent<std::string>>(mWindow, _("MAX FREQ"), false);
        std::string currentFreq = getGpuMaxFreq();
        LOG(LogDebug) << "GPU currentFreq: '" << currentFreq << "'";
        bool found = false;
        for (const auto& freq : gpuFreqs) {
            // Convert Hz to MHz for display
            int mhz = atoi(freq.c_str()) / 1000000;
            bool isSelected = (freq == currentFreq);
            LOG(LogDebug) << "GPU freq option: '" << freq << "' selected: " << isSelected;
            if (isSelected) found = true;
            freqList->add(std::to_string(mhz) + " MHz", freq, isSelected);
        }
        if (!found && !gpuFreqs.empty()) {
            freqList->selectFirstItem();
        }
        s->addWithLabel(_("GPU MAX FREQ"), freqList);
        
        freqList->setSelectedChangedCallback([this](const std::string& val) {
            setGpuMaxFreq(val);
        });
    }
		
	// --- GPU Persistence ---
    auto gpuBootApplySwitch = std::make_shared<SwitchComponent>(mWindow);
    gpuBootApplySwitch->setState(isGpuBootApplyEnabled());
    s->addWithLabel(_("GPU APPLY AT BOOT"), gpuBootApplySwitch);
    gpuBootApplySwitch->setOnChangedCallback([this, gpuBootApplySwitch] {
        toggleGpuBootApply(gpuBootApplySwitch->getState());
    });
	
	// --- RAM Frequency ---
    auto dmcFreqs = getDmcAvailableFreqs();
    if (!dmcFreqs.empty()) {
        auto freqList = std::make_shared<OptionListComponent<std::string>>(mWindow, _("MAX FREQ"), false);
        std::string currentFreq = getDmcMaxFreq();
        LOG(LogDebug) << "DMC currentFreq: '" << currentFreq << "'";
        bool found = false;
        for (const auto& freq : dmcFreqs) {
            // Convert Hz to MHz for display
            int mhz = atoi(freq.c_str()) / 1000000;
            bool isSelected = (freq == currentFreq);
            LOG(LogDebug) << "DMC freq option: '" << freq << "' selected: " << isSelected;
            if (isSelected) found = true;
            freqList->add(std::to_string(mhz) + " MHz", freq, isSelected);
        }
        if (!found && !dmcFreqs.empty()) {
            freqList->selectFirstItem();
        }
        s->addWithLabel(_("RAM MAX FREQ"), freqList);
        
        freqList->setSelectedChangedCallback([this](const std::string& val) {
            setDmcMaxFreq(val);
        });
    }	

	// --- RAM Persistence ---
    auto dmcBootApplySwitch = std::make_shared<SwitchComponent>(mWindow);
    dmcBootApplySwitch->setState(isDmcBootApplyEnabled());
    s->addWithLabel(_("RAM APPLY ON BOOT"), dmcBootApplySwitch);
    dmcBootApplySwitch->setOnChangedCallback([this, dmcBootApplySwitch] {
        toggleDmcBootApply(dmcBootApplySwitch->getState());
    });

    // ZRAM Enable/Disable
    bool zramEnabled = isZramEnabled();
    auto zramSwitch = std::make_shared<SwitchComponent>(mWindow);
    zramSwitch->setState(zramEnabled);
    s->addWithLabel(_("ZRAM ENABLE"), zramSwitch);
	
	// --- ZRAM Size ---
    auto sizeList = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SIZE"), false);
    std::vector<std::string> sizes = {"256M", "512M", "768M"};
    std::string currentSize = getZramSize();
    bool found = false;
    for (const auto& size : sizes) {
        if (size == currentSize) found = true;
    }
    if (!found) currentSize = "512M";
    for (const auto& size : sizes) {
        sizeList->add(size, size, size == currentSize);
    }
    s->addWithLabel(_("ZRAM SIZE"), sizeList);

    // ZRAM Compression Algorithm
    auto algoList = std::make_shared<OptionListComponent<std::string>>(mWindow, _("COMP ALGO"), false);
    std::vector<std::string> algos = getAvailableZramAlgorithms();
    std::string currentAlgo = getZramCompAlgorithm();
    if (algos.empty()) {
        algos.push_back("lz4");
    }
    bool algoFound = false;
    for (const auto& a : algos) {
        if (a == currentAlgo) algoFound = true;
    }
    if (!algoFound) currentAlgo = "lz4";
    for (const auto& a : algos) {
        algoList->add(a, a, a == currentAlgo);
    }
    s->addWithLabel(_("ZRAM COMPRESSION"), algoList);

    // Enable/Disable callback
    zramSwitch->setOnChangedCallback([this, zramSwitch, sizeList, algoList] {
        std::string selectedSize = sizeList->getSelected();
        if (selectedSize.empty()) selectedSize = "512M";
        std::string selectedAlgo = algoList->getSelected();
        if (selectedAlgo.empty()) selectedAlgo = "lz4";
        toggleZram(zramSwitch->getState(), selectedSize, selectedAlgo);
        toggleZramAutoStart(zramSwitch->getState(), selectedSize, selectedAlgo);
    });

    // Compression algorithm change callback
    algoList->setSelectedChangedCallback([this, zramSwitch, sizeList](const std::string& val) {
        std::string selectedSize = sizeList->getSelected();
        if (selectedSize.empty()) selectedSize = "512M";
        if (zramSwitch->getState()) {
            toggleZram(false);
            toggleZram(true, selectedSize, val);
            toggleZramAutoStart(true, selectedSize, val);
        }
    });

    // Size change callback
    sizeList->setSelectedChangedCallback([this, zramSwitch, algoList](const std::string& val) {
        std::string selectedAlgo = algoList->getSelected();
        if (selectedAlgo.empty()) selectedAlgo = "lz4";
        if (zramSwitch->getState()) {
            toggleZram(false);
            toggleZram(true, val, selectedAlgo);
            toggleZramAutoStart(true, val, selectedAlgo);
        }
    });

	s->addSaveFunc([this] {
		writeCpuBootConfig();
		writeGpuBootConfig();
		writeDmcBootConfig();
	});

	mWindow->pushGui(s);
}

struct ThemeConfigOption
{
	std::string defaultSettingName;
	std::string subset;
	std::shared_ptr<OptionListComponent<std::string>> component;
};

void GuiMenu::openThemeConfiguration(Window* mWindow, GuiComponent* s, std::shared_ptr<OptionListComponent<std::string>> theme_set,const std::string systemTheme)
{
	if (theme_set != nullptr && Settings::getInstance()->getString("ThemeSet") != theme_set->getSelected())
	{
		mWindow->pushGui(new GuiMsgBox(mWindow, _("YOU MUST APPLY THE THEME BEFORE EDIT CONFIGURATION"), _("OK")));
		return;
	}

	Window* window = mWindow;

	auto system = ViewController::get()->getState().getSystem();
	auto theme = system->getTheme();

	auto themeconfig = new GuiSettings(mWindow, (systemTheme.empty() ? _("THEME CONFIGURATION") : _("VIEW CUSTOMIZATION")).c_str());

	auto themeSubSets = theme->getSubSets();

	std::string viewName;
	bool showGridFeatures = true;
	if (!systemTheme.empty())
	{
		auto glv = ViewController::get()->getGameListView(system);
		viewName = glv->getName();
		std::string baseType = theme->getCustomViewBaseType(viewName);

		showGridFeatures = (viewName == "grid" || baseType == "grid");
	}


	// gamelist_style
	std::shared_ptr<OptionListComponent<std::string>> gamelist_style = nullptr;

	if (systemTheme.empty())
	{
		gamelist_style = std::make_shared< OptionListComponent<std::string> >(mWindow, _("GAMELIST VIEW STYLE"), false);

		std::vector<std::pair<std::string, std::string>> styles;
		styles.push_back(std::pair<std::string, std::string>("automatic", _("automatic")));

		if (system != NULL)
		{
			auto mViews = theme->getViewsOfTheme();
			for (auto it = mViews.cbegin(); it != mViews.cend(); ++it)
			{
				if (it->first == "basic" || it->first == "detailed" || it->first == "grid" || it->first == "video")
					styles.push_back(std::pair<std::string, std::string>(it->first, _(it->first.c_str())));
				else
					styles.push_back(*it);
			}
		}
		else
		{
			styles.push_back(std::pair<std::string, std::string>("basic", _("basic")));
			styles.push_back(std::pair<std::string, std::string>("detailed", _("detailed")));
		}

		auto viewPreference = systemTheme.empty() ? Settings::getInstance()->getString("GamelistViewStyle") : system->getSystemViewMode();
		if (!theme->hasView(viewPreference))
			viewPreference = "automatic";

		for (auto it = styles.cbegin(); it != styles.cend(); it++)
			gamelist_style->add(it->second, it->first, viewPreference == it->first);

		if (!gamelist_style->hasSelection())
			gamelist_style->selectFirstItem();

		themeconfig->addWithLabel(_("GAMELIST VIEW STYLE"), gamelist_style);
	}

	// Default grid size
	std::shared_ptr<OptionListComponent<std::string>> mGridSize = nullptr;
	if (showGridFeatures && system != NULL && theme->hasView("grid"))
	{
		Vector2f gridOverride =
			systemTheme.empty() ? Vector2f::parseString(Settings::getInstance()->getString("DefaultGridSize")) :
			system->getGridSizeOverride();

		auto ovv = std::to_string((int)gridOverride.x()) + "x" + std::to_string((int)gridOverride.y());

		mGridSize = std::make_shared<OptionListComponent<std::string>>(mWindow, _("DEFAULT GRID SIZE"), false);

		bool found = false;
		for (auto it = GuiGamelistOptions::gridSizes.cbegin(); it != GuiGamelistOptions::gridSizes.cend(); it++)
		{
			bool sel = (gridOverride == Vector2f(0, 0) && *it == "automatic") || ovv == *it;
			if (sel)
				found = true;

			mGridSize->add(_(it->c_str()), *it, sel);
		}

		if (!found)
			mGridSize->selectFirstItem();

		themeconfig->addWithLabel(_("DEFAULT GRID SIZE"), mGridSize);
	}

	std::map<std::string, ThemeConfigOption> options;

	for (std::string subset : theme->getSubSetNames(viewName))
	{
		std::string settingName = "subset." + subset;
		std::string perSystemSettingName = systemTheme.empty() ? "" : "subset." + systemTheme + "." + subset;

		if (subset == "colorset") settingName = "ThemeColorSet";
		else if (subset == "iconset") settingName = "ThemeIconSet";
		else if (subset == "menu") settingName = "ThemeMenu";
		else if (subset == "systemview") settingName = "ThemeSystemView";
		else if (subset == "gamelistview") settingName = "ThemeGamelistView";
		else if (subset == "region") settingName = "ThemeRegionName";

		auto themeColorSets = ThemeData::getSubSet(themeSubSets, subset);
			
		if (themeColorSets.size() > 0)
		{
			auto selectedColorSet = themeColorSets.end();
			auto selectedName = !perSystemSettingName.empty() ? Settings::getInstance()->getString(perSystemSettingName) : Settings::getInstance()->getString(settingName);

			if (!perSystemSettingName.empty() && selectedName.empty())
				selectedName = Settings::getInstance()->getString(settingName);

			for (auto it = themeColorSets.begin(); it != themeColorSets.end() && selectedColorSet == themeColorSets.end(); it++)
				if (it->name == selectedName)
					selectedColorSet = it;

			std::shared_ptr<OptionListComponent<std::string>> item = std::make_shared<OptionListComponent<std::string> >(mWindow, _(("THEME " + Utils::String::toUpper(subset)).c_str()), false);
			item->setTag(!perSystemSettingName.empty()? perSystemSettingName : settingName);

			for (auto it = themeColorSets.begin(); it != themeColorSets.end(); it++)
			{
				std::string displayName = it->displayName;

				if (!systemTheme.empty())
				{
					std::string defaultValue = Settings::getInstance()->getString(settingName);
					if (defaultValue.empty())
						defaultValue = system->getTheme()->getDefaultSubSetValue(subset);

					if (it->name == defaultValue)
						displayName = displayName + " (" + _("DEFAULT") +")";
				}

				item->add(displayName, it->name, it == selectedColorSet);
			}

			if (selectedColorSet == themeColorSets.end())
				item->selectFirstItem();

			if (!themeColorSets.empty())
			{						
				std::string displayName = themeColorSets.cbegin()->subSetDisplayName;
				if (!displayName.empty())
				{
					std::string prefix;

					if (systemTheme.empty())
					{
						for (auto subsetName : themeColorSets.cbegin()->appliesTo)
						{
							std::string pfx = theme->getViewDisplayName(subsetName);
							if (!pfx.empty())
							{
								if (prefix.empty())
									prefix = pfx;
								else
									prefix = prefix + ", " + pfx;								
							}
						}

						if (!prefix.empty())
							prefix = " ("+ prefix+")";

					}					

					themeconfig->addWithLabel(displayName + prefix, item);
				}
				else
					themeconfig->addWithLabel(_(("THEME " + Utils::String::toUpper(subset)).c_str()), item);
			}

			ThemeConfigOption opt;
			opt.component = item;
			opt.subset = subset;			
			opt.defaultSettingName = settingName;
			options[!perSystemSettingName.empty() ? perSystemSettingName : settingName] = opt;
		}
		else
		{
			ThemeConfigOption opt;
			opt.component = nullptr;
			options[!perSystemSettingName.empty() ? perSystemSettingName : settingName] = opt;
		}
	}


	if (systemTheme.empty())
	{
		themeconfig->addEntry(_("RESET GAMELIST CUSTOMIZATIONS"), false, [s, themeconfig, window]
		{
			Settings::getInstance()->setString("GamelistViewStyle", "");
			Settings::getInstance()->setString("DefaultGridSize", "");

			for (auto sysIt = SystemData::sSystemVector.cbegin(); sysIt != SystemData::sSystemVector.cend(); sysIt++)
				(*sysIt)->setSystemViewMode("automatic", Vector2f(0, 0));

			themeconfig->setVariable("reloadAll", true);
			themeconfig->close();
		});


		// File extensions
		if (!system->isCollection() && !system->isGroupSystem())
		{
			auto hiddenExts = Utils::String::split(Settings::getInstance()->getString(system->getName() + ".HiddenExt"), ';');

			auto hiddenCtrl = std::make_shared<OptionListComponent<std::string>>(mWindow, _("FILE EXTENSIONS"), true);

			for (auto ext : system->getExtensions())
			{
				std::string extid = Utils::String::toLower(Utils::String::replace(ext, ".", ""));
				hiddenCtrl->add(ext, extid, std::find(hiddenExts.cbegin(), hiddenExts.cend(), extid) == hiddenExts.cend());
			}

			themeconfig->addWithLabel(_("FILE EXTENSIONS"), hiddenCtrl);
			themeconfig->addSaveFunc([themeconfig, system, hiddenCtrl]
			{
				std::string hiddenSystems;

				std::vector<std::string> sel = hiddenCtrl->getSelectedObjects();

				for (auto ext : system->getExtensions())
				{
					std::string extid = Utils::String::toLower(Utils::String::replace(ext, ".", ""));
					if (std::find(sel.cbegin(), sel.cend(), extid) == sel.cend())
					{
						if (hiddenSystems.empty())
							hiddenSystems = extid;
						else
							hiddenSystems = hiddenSystems + ";" + extid;
					}
				}

				if ((Settings::getInstance()->getString(system->getName() + ".HiddenExt") != hiddenSystems))
				{
					if (Settings::getInstance()->setString(system->getName() + ".HiddenExt", hiddenSystems))
					{
						Settings::getInstance()->saveFile();
						themeconfig->setVariable("reloadAll", true);
						//themeconfig->setVariable("forceReloadGames", true);
						themeconfig->setVariable("reloadGuiMenu", true);
					}
				}
			});
		}
	}

	//  theme_colorset, theme_iconset, theme_menu, theme_systemview, theme_gamelistview, theme_region,
	themeconfig->addSaveFunc([systemTheme, system, themeconfig, theme_set, options, gamelist_style, mGridSize, window]
	{
		bool reloadAll = systemTheme.empty() ? Settings::getInstance()->setString("ThemeSet", theme_set == nullptr ? "" : theme_set->getSelected()) : false;

		for (auto option : options)
		{
			ThemeConfigOption& opt = option.second;

			std::string value;

			if (opt.component != nullptr)
			{
				value = opt.component->getSelected();

				if (!systemTheme.empty() && !value.empty())
				{
					std::string defaultValue = Settings::getInstance()->getString(opt.defaultSettingName);
					if (defaultValue.empty())
						defaultValue = system->getTheme()->getDefaultSubSetValue(opt.subset);

					if (value == defaultValue)
						value = "";
				}
				else if (systemTheme.empty() && value == system->getTheme()->getDefaultSubSetValue(opt.subset))
					value = "";
			}

			if (value != Settings::getInstance()->getString(option.first))
				reloadAll |= Settings::getInstance()->setString(option.first, value);
		}
	
		Vector2f gridSizeOverride(0, 0);

		if (mGridSize != nullptr)
		{
			std::string str = mGridSize->getSelected();
			std::string value = "";

			size_t divider = str.find('x');
			if (divider != std::string::npos)
			{
				std::string first = str.substr(0, divider);
				std::string second = str.substr(divider + 1, std::string::npos);

				gridSizeOverride = Vector2f((float)atof(first.c_str()), (float)atof(second.c_str()));
				value = Utils::String::replace(Utils::String::replace(gridSizeOverride.toString(), ".000000", ""), "0 0", "");
			}

			if (systemTheme.empty())
				reloadAll |= Settings::getInstance()->setString("DefaultGridSize", value);
		}
		else if (systemTheme.empty())
			reloadAll |= Settings::getInstance()->setString("DefaultGridSize", "");
		
		if (systemTheme.empty())
			reloadAll |= Settings::getInstance()->setString("GamelistViewStyle", gamelist_style == nullptr ? "" : gamelist_style->getSelected());
		else
		{
			std::string viewMode = gamelist_style == nullptr ? system->getSystemViewMode() : gamelist_style->getSelected();
			reloadAll |= system->setSystemViewMode(viewMode, gridSizeOverride);
		}

		if (reloadAll || themeconfig->getVariable("reloadAll"))
		{	
			if (themeconfig->getVariable("forceReloadGames"))
			{
				reloadAllGames(window, false);
			}
			else if (systemTheme.empty())
			{				
				CollectionSystemManager::get()->updateSystemsList();
				ViewController::get()->reloadAll(window);
				window->endRenderLoadingScreen();

				if (theme_set != nullptr)
				{
					std::string oldTheme = Settings::getInstance()->getString("ThemeSet");
					Scripting::fireEvent("theme-changed", theme_set->getSelected(), oldTheme);
				}
			}
			else
			{
				system->loadTheme();
				system->resetFilters();
				ViewController::get()->reloadGameListView(system);
			}
		}
	});

	mWindow->pushGui(themeconfig);
}

void GuiMenu::reloadAllGames(Window* window, bool deleteCurrentGui)
{
	window->renderLoadingScreen(_("Loading..."));

	if (!deleteCurrentGui)
	{
		GuiComponent* topGui = window->peekGui();
		window->removeGui(topGui);
	}

	GuiComponent *gui;
	while ((gui = window->peekGui()) != NULL)
	{
		window->removeGui(gui);
		delete gui;
	}

	ViewController::init(window);
	CollectionSystemManager::deinit();
	CollectionSystemManager::init(window);
	SystemData::loadConfig(window);

	ViewController::get()->reloadAll(window);
	//ViewController::get()->reloadAll(nullptr, false); // Avoid reloading themes a second time

	window->pushGui(ViewController::get());
}

void GuiMenu::openUISettings()
{	
	auto pthis = this;
	Window* window = mWindow;

	auto s = new GuiSettings(mWindow, _("UI SETTINGS"));
	
	// theme set
	auto theme = ThemeData::getMenuTheme();
	auto themeSets = ThemeData::getThemeSets();
	auto system = ViewController::get()->getState().getSystem();

	if (!themeSets.empty())
	{
		std::map<std::string, ThemeSet>::const_iterator selectedSet = themeSets.find(Settings::getInstance()->getString("ThemeSet"));
		if (selectedSet == themeSets.cend())
			selectedSet = themeSets.cbegin();

		auto theme_set = std::make_shared< OptionListComponent<std::string> >(mWindow, _("THEME"), false);
		for (auto it = themeSets.cbegin(); it != themeSets.cend(); it++)
			theme_set->add(it->first, it->first, it == selectedSet);

		s->addWithLabel(_("THEME"), theme_set);
		s->addSaveFunc([s, theme_set, window]
		{
			std::string oldTheme = Settings::getInstance()->getString("ThemeSet");
			if (oldTheme != theme_set->getSelected())
			{
				Settings::getInstance()->setString("ThemeSet", theme_set->getSelected());

				// theme changed without setting options, forcing options to avoid crash/blank theme
				Settings::getInstance()->setString("ThemeRegionName", "");
				Settings::getInstance()->setString("ThemeColorSet", "");
				Settings::getInstance()->setString("ThemeIconSet", "");
				Settings::getInstance()->setString("ThemeMenu", "");
				Settings::getInstance()->setString("ThemeSystemView", "");
				Settings::getInstance()->setString("ThemeGamelistView", "");
				Settings::getInstance()->setString("GamelistViewStyle", "");
				Settings::getInstance()->setString("DefaultGridSize", "");

				for(auto sm : Settings::getInstance()->getStringMap())
					if (Utils::String::startsWith(sm.first, "subset."))
						Settings::getInstance()->setString(sm.first, "");

				for (auto sysIt = SystemData::sSystemVector.cbegin(); sysIt != SystemData::sSystemVector.cend(); sysIt++)
					(*sysIt)->setSystemViewMode("automatic", Vector2f(0, 0));

				s->setVariable("reloadCollections", true);
				s->setVariable("reloadAll", true);
				s->setVariable("reloadGuiMenu", true);

				Scripting::fireEvent("theme-changed", theme_set->getSelected(), oldTheme);
			}
		});
	
		bool showThemeConfiguration = system->getTheme()->hasSubsets() || system->getTheme()->hasView("grid");
		if (showThemeConfiguration)
		{
			s->addSubMenu(_("THEME CONFIGURATION"), [this, s, theme_set]() { openThemeConfiguration(mWindow, s, theme_set); });
		}
		else // GameList view style only, acts like Retropie for simple themes
		{
			auto gamelist_style = std::make_shared< OptionListComponent<std::string> >(mWindow, _("GAMELIST VIEW STYLE"), false);
			std::vector<std::pair<std::string, std::string>> styles;
			styles.push_back(std::pair<std::string, std::string>("automatic", _("automatic")));

			auto system = ViewController::get()->getState().getSystem();
			if (system != NULL)
			{
				auto mViews = system->getTheme()->getViewsOfTheme();
				for (auto it = mViews.cbegin(); it != mViews.cend(); ++it)
					styles.push_back(*it);
			}
			else
			{
				styles.push_back(std::pair<std::string, std::string>("basic", _("basic")));
				styles.push_back(std::pair<std::string, std::string>("detailed", _("detailed")));
				styles.push_back(std::pair<std::string, std::string>("video", _("video")));
				styles.push_back(std::pair<std::string, std::string>("grid", _("grid")));
			}

			auto viewPreference = Settings::getInstance()->getString("GamelistViewStyle");
			if (!system->getTheme()->hasView(viewPreference))
				viewPreference = "automatic";

			for (auto it = styles.cbegin(); it != styles.cend(); it++)
				gamelist_style->add(it->second, it->first, viewPreference == it->first);

			s->addWithLabel(_("GAMELIST VIEW STYLE"), gamelist_style);
			s->addSaveFunc([s, gamelist_style, window] 
			{
				if (Settings::getInstance()->setString("GamelistViewStyle", gamelist_style->getSelected()))
				{
					s->setVariable("reloadAll", true);
					s->setVariable("reloadGuiMenu", true);
				}
			});
		}
	}

	// screensaver
	ComponentListRow screensaver_row;
	screensaver_row.elements.clear();
	screensaver_row.addElement(std::make_shared<TextComponent>(mWindow, _("SCREENSAVER SETTINGS"), theme->Text.font, theme->Text.color), true);
	screensaver_row.addElement(makeArrow(mWindow), false);
	screensaver_row.makeAcceptInputHandler(std::bind(&GuiMenu::openScreensaverOptions, this));
	s->addRow(screensaver_row);


	//#ifndef WIN32
		//UI mode
	auto UImodeSelection = std::make_shared< OptionListComponent<std::string> >(mWindow, _("UI MODE"), false);
	std::vector<std::string> UImodes = UIModeController::getInstance()->getUIModes();
	for (auto it = UImodes.cbegin(); it != UImodes.cend(); it++)
		UImodeSelection->add(_(*it), *it, Settings::getInstance()->getString("UIMode") == *it);
	s->addWithLabel(_("UI MODE"), UImodeSelection);

	s->addSaveFunc([UImodeSelection, window]
	{
		std::string selectedMode = UImodeSelection->getSelected();
		if (selectedMode != "Full")
		{
			std::string msg = _("You are changing the UI to a restricted mode:") + "\n" + selectedMode + "\n";
			msg += _("This will hide most menu-options to prevent changes to the system.") + "\n";
			msg += _("To unlock and return to the full UI, enter this code:") + "\n";
			msg += "\"" + UIModeController::getInstance()->getFormattedPassKeyStr() + "\"\n\n";
			msg += _("Do you want to proceed?");
			window->pushGui(new GuiMsgBox(window, msg,
				_("YES"), [selectedMode] {
				LOG(LogDebug) << "Setting UI mode to " << selectedMode;
				Settings::getInstance()->setString("UIMode", selectedMode);
				Settings::getInstance()->saveFile();
			}, _("NO"), nullptr));
		}
	});
	//#endif

	// LANGUAGE
	/*
	std::vector<std::string> langues;
	langues.push_back("en");

	std::string xmlpath = ResourceManager::getInstance()->getResourcePath(":/splash.svg");
	if (xmlpath.length() > 0)
	{
		xmlpath = Utils::FileSystem::getParent(xmlpath) + "/locale/";

		Utils::FileSystem::stringList dirContent = Utils::FileSystem::getDirContent(xmlpath, true);
		for (Utils::FileSystem::stringList::const_iterator it = dirContent.cbegin(); it != dirContent.cend(); ++it)
		{
			if (Utils::FileSystem::isDirectory(*it))
				continue;

			std::string name = *it;

			if (name.rfind("emulationstation2.po") == std::string::npos)
				continue;

			name = Utils::FileSystem::getParent(name);
			name = Utils::FileSystem::getFileName(name);

			if (name != "en")
				langues.push_back(name);
		}

		if (langues.size() > 1)
		{
			auto language = std::make_shared< OptionListComponent<std::string> >(mWindow, _("LANGUAGE"), false);

			for (auto it = langues.cbegin(); it != langues.cend(); it++)
				language->add(*it, *it, Settings::getInstance()->getString("Language") == *it);

			s->addWithLabel(_("LANGUAGE"), language);
			s->addSaveFunc([language, window, pthis, s] {
				
				if (language->getSelected() != Settings::getInstance()->getString("Language"))
				{
					if (Settings::getInstance()->setString("Language", language->getSelected()))
						s->setVariable("reloadGuiMenu", true);
				}
			});
		}
	}
	*/
	// transition style
	auto transition_style = std::make_shared< OptionListComponent<std::string> >(mWindow, _("TRANSITION STYLE"), false);
	std::vector<std::string> transitions;
	transitions.push_back("auto");
	transitions.push_back("fade");
	transitions.push_back("slide");
	transitions.push_back("instant");
	
	for (auto it = transitions.cbegin(); it != transitions.cend(); it++)
		transition_style->add(_(*it), *it, Settings::getInstance()->getString("TransitionStyle") == *it);

	if (!transition_style->hasSelection())
		transition_style->selectFirstItem();

	s->addWithLabel(_("TRANSITION STYLE"), transition_style);
	s->addSaveFunc([transition_style] {
		if (Settings::getInstance()->getString("TransitionStyle") == "instant"
			&& transition_style->getSelected() != "instant"
			&& PowerSaver::getMode() == PowerSaver::INSTANT)
		{
			Settings::getInstance()->setString("PowerSaverMode", "default");
			PowerSaver::init();
		}
		Settings::getInstance()->setString("TransitionStyle", transition_style->getSelected());
		GuiComponent::ALLOWANIMATIONS = Settings::getInstance()->getString("TransitionStyle") != "instant";
	});
	
	auto transitionOfGames_style = std::make_shared< OptionListComponent<std::string> >(mWindow, _("GAME LAUNCH TRANSITION"), false);
	std::vector<std::string> gameTransitions;
	gameTransitions.push_back("fade");
	gameTransitions.push_back("slide");
	gameTransitions.push_back("instant");
	for (auto it = gameTransitions.cbegin(); it != gameTransitions.cend(); it++)
		transitionOfGames_style->add(_(*it), *it, Settings::getInstance()->getString("GameTransitionStyle") == *it);

	s->addWithLabel(_("GAME LAUNCH TRANSITION"), transitionOfGames_style);
	s->addSaveFunc([transitionOfGames_style] {
		if (Settings::getInstance()->getString("GameTransitionStyle") == "instant"
			&& transitionOfGames_style->getSelected() != "instant"
			&& PowerSaver::getMode() == PowerSaver::INSTANT)
		{
			Settings::getInstance()->setString("PowerSaverMode", "default");
			PowerSaver::init();
		}
		Settings::getInstance()->setString("GameTransitionStyle", transitionOfGames_style->getSelected());
	});


	// Optionally start in selected system
	auto systemfocus_list = std::make_shared< OptionListComponent<std::string> >(mWindow, _("START ON SYSTEM"), false);
	systemfocus_list->add(_("NONE"), "", Settings::getInstance()->getString("StartupSystem") == "");

	for (auto it = SystemData::sSystemVector.cbegin(); it != SystemData::sSystemVector.cend(); it++)
		if ("retropie" != (*it)->getName() && (*it)->isVisible())
			systemfocus_list->add((*it)->getName(), (*it)->getName(), Settings::getInstance()->getString("StartupSystem") == (*it)->getName());

	if (!systemfocus_list->hasSelection())
		systemfocus_list->selectFirstItem();

	s->addWithLabel(_("START ON SYSTEM"), systemfocus_list);
	s->addSaveFunc([systemfocus_list] {
		Settings::getInstance()->setString("StartupSystem", systemfocus_list->getSelected());
	});



	// Select systems to hide
	auto hiddenSystems = Utils::String::split(Settings::getInstance()->getString("HiddenSystems"), ';');

	auto displayedSystems = std::make_shared<OptionListComponent<SystemData*>>(mWindow, _("VISIBLE SYSTEMS"), true);

	for (auto system : SystemData::sSystemVector)
		if(!system->isCollection() && !system->isGroupChildSystem())
			displayedSystems->add(system->getFullName(), system, std::find(hiddenSystems.cbegin(), hiddenSystems.cend(), system->getName()) == hiddenSystems.cend());

	s->addWithLabel(_("VISIBLE SYSTEMS"), displayedSystems);
	s->addSaveFunc([s, displayedSystems]
	{
		std::string hiddenSystems;

		std::vector<SystemData*> sys = displayedSystems->getSelectedObjects();

		for (auto system : SystemData::sSystemVector)
		{
			if (system->isCollection() || system->isGroupChildSystem())
				continue;

			if (std::find(sys.cbegin(), sys.cend(), system) == sys.cend())
			{
				if (hiddenSystems.empty())
					hiddenSystems = system->getName();
				else
					hiddenSystems = hiddenSystems + ";" + system->getName();
			}
		}

		if (Settings::getInstance()->setString("HiddenSystems", hiddenSystems))
		{
			Settings::getInstance()->saveFile();
			s->setVariable("reloadAll", true);
		}		
	});

	
	// Open gamelist at start
	auto bootOnGamelist = std::make_shared<SwitchComponent>(mWindow);
	bootOnGamelist->setState(Settings::getInstance()->getBool("StartupOnGameList"));
	s->addWithLabel(_("BOOT ON GAMELIST"), bootOnGamelist);
	s->addSaveFunc([bootOnGamelist] { Settings::getInstance()->setBool("StartupOnGameList", bootOnGamelist->getState()); });

	// Hide system view
	auto hideSystemView = std::make_shared<SwitchComponent>(mWindow);
	hideSystemView->setState(Settings::getInstance()->getBool("HideSystemView"));
	s->addWithLabel(_("HIDE SYSTEM VIEW"), hideSystemView);
	s->addSaveFunc([hideSystemView] 
	{ 
		bool hideSysView = Settings::getInstance()->getBool("HideSystemView");
		Settings::getInstance()->setBool("HideSystemView", hideSystemView->getState());

		if (!hideSysView && hideSystemView->getState())
			ViewController::get()->goToStart(true);
	});


#if defined(_WIN32)
	// quick system select (left/right in game list view)
	auto hideWindowScreen = std::make_shared<SwitchComponent>(mWindow);
	hideWindowScreen->setState(Settings::getInstance()->getBool("HideWindow"));
	s->addWithLabel(_("HIDE WHEN RUNNING GAME"), hideWindowScreen);
	s->addSaveFunc([hideWindowScreen] { Settings::getInstance()->setBool("HideWindow", hideWindowScreen->getState()); });
#endif

	// quick system select (left/right in game list view)
	auto quick_sys_select = std::make_shared<SwitchComponent>(mWindow);
	quick_sys_select->setState(Settings::getInstance()->getBool("QuickSystemSelect"));
	s->addWithLabel(_("QUICK SYSTEM SELECT"), quick_sys_select);
	s->addSaveFunc([quick_sys_select] { Settings::getInstance()->setBool("QuickSystemSelect", quick_sys_select->getState()); });

	// carousel transition option
	auto move_carousel = std::make_shared<SwitchComponent>(mWindow);
	move_carousel->setState(Settings::getInstance()->getBool("MoveCarousel"));
	s->addWithLabel(_("CAROUSEL TRANSITIONS"), move_carousel);
	s->addSaveFunc([move_carousel] {
		if (move_carousel->getState()
			&& !Settings::getInstance()->getBool("MoveCarousel")
			&& PowerSaver::getMode() == PowerSaver::INSTANT)
		{
			Settings::getInstance()->setString("PowerSaverMode", "default");
			PowerSaver::init();
		}
		Settings::getInstance()->setBool("MoveCarousel", move_carousel->getState());
	});

	// LED color
	bool ledInitialRed = isLedRed();
	auto ledColor = std::make_shared<OptionListComponent<std::string>>(mWindow, _("LED COLOR"), false);
	ledColor->add(_("RED"), "red", ledInitialRed);
	ledColor->add(_("DEFAULT"), "blue", !ledInitialRed);
	s->addWithLabel(_("LED COLOR"), ledColor);
	s->addSaveFunc([ledColor, ledInitialRed] {
		bool selectRed = ledColor->getSelected() == "red";
		if (selectRed == ledInitialRed)
			return;

		if (selectRed)
		{
			executeCommand("sudo -n sh -c 'echo 1 > /sys/class/gpio/gpio77/value' 2>/dev/null");
			executeCommand("sudo -n cp -f /usr/local/bin/batt_life_warning.py.red /usr/local/bin/batt_life_warning.py 2>/dev/null");
			executeCommand("sudo -n cp -f /usr/local/bin/fix_power_led.red /usr/local/bin/fix_power_led 2>/dev/null");
		}
		else
		{
			executeCommand("sudo -n sh -c 'echo 0 > /sys/class/gpio/gpio77/value' 2>/dev/null");
			executeCommand("sudo -n cp -f /usr/local/bin/batt_life_warning.py.green /usr/local/bin/batt_life_warning.py 2>/dev/null");
			executeCommand("sudo -n cp -f /usr/local/bin/fix_power_led.green /usr/local/bin/fix_power_led 2>/dev/null");
		}

		executeCommand("sudo -n systemctl restart batt_led 2>/dev/null");
	});

	// clock
	auto clock = std::make_shared<SwitchComponent>(mWindow);
	clock->setState(Settings::getInstance()->getBool("DrawClock"));
	s->addWithLabel(_("SHOW CLOCK"), clock);
	s->addSaveFunc(
		[clock] { Settings::getInstance()->setBool("DrawClock", clock->getState()); });


	
	auto wifiIcon = std::make_shared<SwitchComponent>(mWindow);
	wifiIcon->setState(Settings::getInstance()->getBool("networkIcon"));
	s->addWithLabel(_("SHOW WIFI ICON"), wifiIcon);
	s->addSaveFunc([s, wifiIcon]
	{
		if (Settings::getInstance()->setBool("networkIcon", wifiIcon->getState()))
			s->setVariable("reloadAll", true);
	});

	auto bluetoothIcon = std::make_shared<SwitchComponent>(mWindow);
	bluetoothIcon->setState(Settings::getInstance()->getBool("bluetoothIcon"));
	s->addWithLabel(_("SHOW BLUETOOTH ICON"), bluetoothIcon);
	s->addSaveFunc([s, bluetoothIcon]
	{
		if (Settings::getInstance()->setBool("bluetoothIcon", bluetoothIcon->getState()))
			s->setVariable("reloadAll", true);
	});

	// --- Network Icon Pack ---
	auto networkIconPack = std::make_shared<OptionListComponent<std::string>>(mWindow, _("NETWORK ICON"), false);
	std::string currentNetPack = Settings::getInstance()->getString("NetworkIconPack");
	if (currentNetPack.empty()) currentNetPack = "Default";
	networkIconPack->add(_("DEFAULT"),  "Default",  currentNetPack == "Default");
	networkIconPack->add(_("MARIO"),    "Mario",    currentNetPack == "Mario");
	networkIconPack->add(_("POKEMON"),  "Pokemon",  currentNetPack == "Pokemon");
	networkIconPack->add(_("SOLSTICE"), "Solstice", currentNetPack == "Solstice");
	networkIconPack->add(_("ZELDA"),    "Zelda",    currentNetPack == "Zelda");
	networkIconPack->add(_("STOCK"),    "Stock",    currentNetPack == "Stock");
	s->addWithLabel(_("NETWORK ICON"), networkIconPack);
	s->addSaveFunc([this, s, networkIconPack] {
		std::string pack = networkIconPack->getSelected();
		if (Settings::getInstance()->setString("NetworkIconPack", pack)) {
			std::string src = "/usr/bin/emulationstation/resources/network-packs/" + pack;
			runSystemCommand("sudo -n cp -f " + src + "/bluetooth*.svg /usr/bin/emulationstation/resources/ 2>/dev/null", "", nullptr);
			runSystemCommand("sudo -n cp -f " + src + "/network*.svg /usr/bin/emulationstation/resources/ 2>/dev/null", "", nullptr);
			Settings::getInstance()->saveFile();
			if (mWindow->getBatteryIndicator() != nullptr)
				mWindow->getBatteryIndicator()->reloadNetworkIcons();
			s->setVariable("reloadAll", true);
		}
	});

	// show help
	auto show_help = std::make_shared<SwitchComponent>(mWindow);
	show_help->setState(Settings::getInstance()->getBool("ShowHelpPrompts"));
	s->addWithLabel(_("ON-SCREEN HELP"), show_help);
	s->addSaveFunc([s, show_help]
	{
		if (Settings::getInstance()->setBool("ShowHelpPrompts", show_help->getState()))
			s->setVariable("reloadAll", true);
	});

	// Battery indicator
	if (ApiSystem::getInstance()->getBatteryInformation().hasBattery)
	{
		auto batteryStatus = std::make_shared<OptionListComponent<std::string> >(mWindow, _("SHOW BATTERY STATUS"), false);
		batteryStatus->addRange({ { _("NO"), "" },{ _("ICON"), "icon" },{ _("ICON AND TEXT"), "text" } }, Settings::getInstance()->getString("ShowBattery"));
		s->addWithLabel(_("SHOW BATTERY STATUS"), batteryStatus);
		s->addSaveFunc([s, batteryStatus]
		{
			std::string old_value = Settings::getInstance()->getString("ShowBattery");
			if (old_value != batteryStatus->getSelected())
            {
				Settings::getInstance()->setString("ShowBattery", batteryStatus->getSelected());
				s->setVariable("reloadAll", true);
			}
		});
	}

	// --- Battery Icon Pack ---
	auto batteryIconPack = std::make_shared<OptionListComponent<std::string>>(mWindow, _("BATTERY ICON"), false);
	std::string currentPack = Settings::getInstance()->getString("BatteryIconPack");
	if (currentPack.empty()) currentPack = "Default";
	batteryIconPack->add(_("DEFAULT"),       "default",    currentPack == "default");
	batteryIconPack->add(_("COLORFUL"),      "colorful",   currentPack == "colorful");
	batteryIconPack->add(_("VERTICAL"),      "vertical",   currentPack == "vertical");
	batteryIconPack->add(_("STOCK"),         "stock",      currentPack == "stock");
	batteryIconPack->add(_("HEARTS"),        "hearts",     currentPack == "hearts");
	if (!batteryIconPack->hasSelection())
		batteryIconPack->selectFirstItem();
	s->addWithLabel(_("BATTERY ICON"), batteryIconPack);
	s->addSaveFunc([s, batteryIconPack] {
		std::string pack = batteryIconPack->getSelected();
		if (Settings::getInstance()->setString("BatteryIconPack", pack)) {
			// Copier les SVG du pack sélectionné vers resources/battery/
			std::string src = "/usr/bin/emulationstation/resources/battery-packs/" + pack;
			std::string cmd = "sudo -n cp -f " + src + "/*.svg /usr/bin/emulationstation/resources/battery/ 2>/dev/null";
			runSystemCommand(cmd, "", nullptr);
			Settings::getInstance()->saveFile();
			s->setVariable("reloadAll", true);
		}
	});

	// Network indicator
	/*auto networkIndicator = std::make_shared<SwitchComponent>(mWindow);
	networkIndicator->setState(Settings::getInstance()->getBool("ShowNetworkIndicator"));
	s->addWithLabel(_("SHOW NETWORK ICON"), networkIndicator);
	s->addSaveFunc([s, networkIndicator]
	{
    		if (Settings::getInstance()->setBool("ShowNetworkIndicator", networkIndicator->getState()))
        	s->setVariable("reloadAll", true);
	});
	*/
	// filenames
	auto hidden_files = std::make_shared<SwitchComponent>(mWindow);
	hidden_files->setState(Settings::getInstance()->getBool("ShowFilenames"));
	s->addWithLabel(_("SHOW FILENAMES IN LISTS"), hidden_files);
	s->addSaveFunc([hidden_files, s] 
	{ 
		if (Settings::getInstance()->setBool("ShowFilenames", hidden_files->getState()))
		{
			FileData::resetSettings();
			s->setVariable("reloadCollections", true);
			s->setVariable("reloadAll", true);
		}
	});

	auto ignoreArticles = std::make_shared<SwitchComponent>(mWindow);
	ignoreArticles->setState(Settings::getInstance()->getBool("IgnoreLeadingArticles"));
	s->addWithLabel(_("IGNORE LEADING ARTICLES WHEN SORTING"), ignoreArticles);
	s->addSaveFunc([s, ignoreArticles]
	{
		if (Settings::getInstance()->setBool("IgnoreLeadingArticles", ignoreArticles->getState()))
		{
			s->setVariable("reloadAll", true);
		}
	});

	// enable filters (ForceDisableFilters)
	auto enable_filter = std::make_shared<SwitchComponent>(mWindow);
	enable_filter->setState(!Settings::getInstance()->getBool("ForceDisableFilters"));
	s->addWithLabel(_("ENABLE FILTERS"), enable_filter);
	s->addSaveFunc([enable_filter, s] { 
		bool filter_is_enabled = !Settings::getInstance()->getBool("ForceDisableFilters");
		if (Settings::getInstance()->setBool("ForceDisableFilters", !enable_filter->getState()))
			s->setVariable("reloadAll", true);		
	});

	// scan ports
	auto scanPorts = std::make_shared<SwitchComponent>(mWindow);
	scanPorts->setState(Settings::getInstance()->getBool("ScanPorts"));
	s->addWithLabel(_("Scan Ports folder on boot"), scanPorts);
	s->addSaveFunc([scanPorts, s]
	{
		if (Settings::getInstance()->setBool("ScanPorts", scanPorts->getState()))
			s->setVariable("reloadAll", true);
			s->setVariable("reloadGuiMenu", true);
			//mWindow->displayNotificationMessage("Please restart Emulationstation for your changes to take affect");
	});

	s->onFinalize([s, pthis, window]
	{
		if (s->getVariable("reloadCollections"))
			CollectionSystemManager::get()->updateSystemsList();

		if (s->getVariable("reloadAll"))
		{
			ViewController::get()->reloadAll(window);
			window->endRenderLoadingScreen();
		}

		if (s->getVariable("reloadGuiMenu"))
		{
			delete pthis;
			window->pushGui(new GuiMenu(window, false));
		}
	});

// Game Loading Image Mode
auto GameLoadingImageMode = std::make_shared<OptionListComponent<std::string>>(mWindow, "Game Loading Image Mode", false);
GameLoadingImageMode->addRange({ {_("PIC"), "pic"},{_("ASCII"), "ascii"},{_("GIF"), "gif"},{_("VID"), "vid"},{_("NONE"), "none"} }, Settings::getInstance()->getString("GameLoadingIMode"));
s->addWithLabel(_("GAME LOADING IMAGE MODE"), GameLoadingImageMode);
s->addSaveFunc([s, GameLoadingImageMode] {
  std::string oldvalue = Settings::getInstance()->getString("GameLoadingIMode");
  if (oldvalue != GameLoadingImageMode->getSelected()) {
    if (strstr(GameLoadingImageMode->getSelected().c_str(),"ascii")) {
      runSystemCommand("rm -f /home/ark/.config/.GameLoadingIMode*", "", nullptr);
      runSystemCommand("touch /home/ark/.config/.GameLoadingIModeASCII", "", nullptr);
    }
    else if (strstr(GameLoadingImageMode->getSelected().c_str(),"pic")) {
      runSystemCommand("rm -f /home/ark/.config/.GameLoadingIMode*", "", nullptr);
      runSystemCommand("touch /home/ark/.config/.GameLoadingIModePIC", "", nullptr);
    }
    else if (strstr(GameLoadingImageMode->getSelected().c_str(),"gif")) {
      runSystemCommand("rm -f /home/ark/.config/.GameLoadingIMode*", "", nullptr);
      runSystemCommand("touch /home/ark/.config/.GameLoadingIModeGIF", "", nullptr);
    }
    else if (strstr(GameLoadingImageMode->getSelected().c_str(),"vid")) {
      runSystemCommand("rm -f /home/ark/.config/.GameLoadingIMode*", "", nullptr);
      runSystemCommand("touch /home/ark/.config/.GameLoadingIModeVID", "", nullptr);
    }
    else {
      runSystemCommand("rm -f /home/ark/.config/.GameLoadingIMode*", "", nullptr);
      runSystemCommand("touch /home/ark/.config/.GameLoadingIModeNO", "", nullptr);
    }
    Settings::getInstance()->setString("GameLoadingIMode", GameLoadingImageMode->getSelected());
  }
});


	// Game Loading Image
    if (strstr(GameLoadingImageMode->getSelected().c_str(),"pic") || strstr(GameLoadingImageMode->getSelected().c_str(),"gif") || strstr(GameLoadingImageMode->getSelected().c_str(),"vid")){
     if (strstr(GameLoadingImageMode->getSelected().c_str(),"pic")){
	 auto GameLoadingImage = std::make_shared<OptionListComponent<std::string> >(mWindow, _("Game Loading Image"), false);
	 GameLoadingImage->addRange({ { _("DEFAULT"), "default" },{ _("MARQUEE"), "marquee" },{ _("IMAGE"), "image" },{ _("THUMB"), "thumb" } }, Settings::getInstance()->getString("GameLoadingImage"));
	 s->addWithLabel(_("  GAME LOADING IMAGE"), GameLoadingImage);
	 s->addSaveFunc([s, GameLoadingImage]
	 {
		std::string old_value = Settings::getInstance()->getString("GameLoadingImage");
		if (old_value != GameLoadingImage->getSelected())
           {
            if (strstr(GameLoadingImage->getSelected().c_str(),"default")) {
              runSystemCommand("[ ! -z $(find /home/ark/.config/.LOADING_IMAGE*) ] && rm /home/ark/.config/.LOADING_IMAGE*", "", nullptr);
            }
            else if (strstr(GameLoadingImage->getSelected().c_str(),"marquee")) {
              runSystemCommand("[ ! -z $(find /home/ark/.config/.LOADING_IMAGE*) ] && rm /home/ark/.config/.LOADING_IMAGE*", "", nullptr);
              runSystemCommand("touch /home/ark/.config/.LOADING_IMAGE_MARQUEE", "", nullptr);
            }
            else if (strstr(GameLoadingImage->getSelected().c_str(),"image")) {
              runSystemCommand("[ ! -z $(find /home/ark/.config/.LOADING_IMAGE*) ] && rm /home/ark/.config/.LOADING_IMAGE*", "", nullptr);
              runSystemCommand("touch /home/ark/.config/.LOADING_IMAGE_IMAGE", "", nullptr);
            }
            else {
              runSystemCommand("[ ! -z $(find /home/ark/.config/.LOADING_IMAGE*) ] && rm /home/ark/.config/.LOADING_IMAGE*", "", nullptr);
              runSystemCommand("touch /home/ark/.config/.LOADING_IMAGE_THUMB", "", nullptr);
            }
			Settings::getInstance()->setString("GameLoadingImage", GameLoadingImage->getSelected());
		   }
	 });
	 }
	// Game Loading Image delay
	 auto ITime = std::make_shared< OptionListComponent<std::string> >(mWindow, _("Game Loading Image Delay (secs)"), false);
	 std::vector<std::string> adelay;
	 adelay.push_back("1.5");
	 adelay.push_back("2");
	 adelay.push_back("2.5");
	 adelay.push_back("3");
	 adelay.push_back("3.5");
	 adelay.push_back("4");
	 adelay.push_back("4.5");
	 adelay.push_back("5");

	 auto delay = Settings::getInstance()->getString("ImagedelayTime");
	 if (delay.empty())
		delay = "1.5";

	 for (auto it = adelay.cbegin(); it != adelay.cend(); it++)
		ITime->add(_(it->c_str()), *it, delay == *it);

	 s->addWithLabel(_("  Game Loading Image delay (secs)"), ITime);
	 s->addSaveFunc([this, ITime] { Settings::getInstance()->setString("ImagedelayTime", ITime->getSelected()); 
		if (ITime->changed()) {
		    runSystemCommand("echo " + Settings::getInstance()->getString("ImagedelayTime") + " > /home/ark/.config/.IMAGEDELAYTIME", "", nullptr);
		}
	 });
	}
	s->updatePosition();
	mWindow->pushGui(s);
}

void GuiMenu::openSystemEmulatorSettings(SystemData* system)
{
	auto theme = ThemeData::getMenuTheme();

	GuiSettings* s = new GuiSettings(mWindow, system->getFullName().c_str());

	auto emul_choice = std::make_shared<OptionListComponent<std::string>>(mWindow, _("EMULATOR"), false);
	auto core_choice = std::make_shared<OptionListComponent<std::string>>(mWindow, _("CORE"), false);

	std::string currentEmul = Settings::getInstance()->getString(system->getName() + ".emulator");
	std::string defaultEmul = (system->getSystemEnvData()->mEmulators.size() == 0 ? "" : system->getSystemEnvData()->mEmulators[0].mName);

//	if (defaultEmul.length() == 0)
		emul_choice->add(_("AUTO"), "", false);
//	else
//		emul_choice->add(_("AUTO") + " (" + defaultEmul + ")", "", currentEmul.length() == 0);

	bool found = false;
	for (auto core : system->getSystemEnvData()->mEmulators)
	{
		if (core.mName == currentEmul)
			found = true;

		emul_choice->add(core.mName, core.mName, core.mName == currentEmul);
	}

	if (!found)
		emul_choice->selectFirstItem();
	
	ComponentListRow row;
	row.addElement(std::make_shared<TextComponent>(mWindow, _("EMULATOR"), theme->Text.font, theme->Text.color), true);
	row.addElement(emul_choice, false);

	s->addRow(row);

	emul_choice->setSelectedChangedCallback([this, system, core_choice](std::string emulatorName)
	{
		std::string currentCore = Settings::getInstance()->getString(system->getName() + ".core");
		std::string defaultCore;
		
		for (auto& emulator : system->getSystemEnvData()->mEmulators)
		{
			if (emulatorName == emulator.mName)
			{
				for (auto core : emulator.mCores)
				{
					defaultCore = core;
					break;
				}
			}
		}

		core_choice->clear();

	//	if (defaultCore.length() == 0)
			core_choice->add(_("AUTO"), "", false);
	//	else
	//		core_choice->add(_("AUTO") + " (" + defaultCore + ")", "", false);

		std::vector<std::string> cores = system->getSystemEnvData()->getCores(emulatorName);

		bool found = false;

		for (auto it = cores.begin(); it != cores.end(); it++)
		{
			std::string core = *it;
			core_choice->add(core, core, currentCore == core);
			if (currentCore == core)
				found = true;
		}

		if (!found)
			core_choice->selectFirstItem();
		else
			core_choice->invalidate();
	});

	row.elements.clear();
	row.addElement(std::make_shared<TextComponent>(mWindow, _("CORE"), theme->Text.font, theme->Text.color), true);
	row.addElement(core_choice, false);
	s->addRow(row);

	// force change event to load core list
	emul_choice->invalidate();

	// set governor
	auto gov_choice = std::make_shared<OptionListComponent<std::string>>(mWindow, _("GOVERNOR"), false);

	gov_choice->clear();

	gov_choice->add(_("AUTO"), "", false);

	std::vector<std::string> governors = system->getSystemEnvData()->allGovernors();
	std::string currentGovernor = Settings::getInstance()->getString(system->getName() + ".governor");

	bool foundgov = false;

	for (auto it = governors.begin(); it != governors.end(); it++)
	{
		std::string govena = *it;
		gov_choice->add(govena, govena, currentGovernor == govena);
		if (currentGovernor == govena)
			foundgov = true;
	}

	if (!foundgov)
		gov_choice->selectFirstItem();
	else
		gov_choice->invalidate();

	s->addWithLabel(_("GOVERNOR"), gov_choice);

	s->addSaveFunc([system, emul_choice, core_choice, gov_choice]
	{		
		Settings::getInstance()->setString(system->getName() + ".emulator", emul_choice->getSelected());
		Settings::getInstance()->setString(system->getName() + ".core", core_choice->getSelected());
		Settings::getInstance()->setString(system->getName() + ".governor", gov_choice->getSelected());
	});

	mWindow->pushGui(s);
}

void GuiMenu::openEmulatorSettings()
{
	GuiSettings* configuration = new GuiSettings(mWindow, _("EMULATOR SETTINGS").c_str());	

	Window* window = mWindow;
	
	// For each activated system
	for (auto system : SystemData::sSystemVector)
	{
		if (system->isCollection())
			continue;

		if (system->getName() == "options")
			continue;

		/*if (system->getSystemEnvData()->mEmulators.size() == 0)
			continue;*/

		/*if (system->getSystemEnvData()->mEmulators.size() == 1 && system->getSystemEnvData()->mEmulators[0].mCores.size() <= 1)
			continue;*/
		
		configuration->addEntry(system->getFullName(), true, [this, system] { openSystemEmulatorSettings(system); });
	}

	window->pushGui(configuration);
}

void GuiMenu::openUpdateSettings()
{
	Window* window = mWindow;
	auto s = new GuiSettings(mWindow, _("DOWNLOADS AND UPDATES"));

	// themes installer/browser
	s->addEntry(_("THEME INSTALLER"), true, [this]
	{
		mWindow->pushGui(new GuiThemeInstall(mWindow));
	});

	// Enable updates
	auto updates_enabled = std::make_shared<SwitchComponent>(mWindow);
	updates_enabled->setState(Settings::getInstance()->getBool("updates.enabled"));
	s->addWithLabel(_("AUTO UPDATES"), updates_enabled);
	s->addSaveFunc([updates_enabled]
	{
		Settings::getInstance()->setBool("updates.enabled", updates_enabled->getState());
	});

	// Start update
	s->addEntry(ApiSystem::state == UpdateState::State::UPDATE_READY ? _("APPLY UPDATE") : _("START UPDATE"), true, [this, s]
	{
		if (ApiSystem::checkUpdateVersion().empty())
		{
			mWindow->pushGui(new GuiMsgBox(mWindow, _("NO UPDATE AVAILABLE")));
			return;
		}

		if (ApiSystem::state == UpdateState::State::UPDATE_READY)
		{
			if (quitES(QuitMode::QUIT))
				LOG(LogWarning) << "Reboot terminated with non-zero result!";
		}
		else if (ApiSystem::state == UpdateState::State::UPDATER_RUNNING)
			mWindow->pushGui(new GuiMsgBox(mWindow, _("UPDATE IS ALREADY RUNNING")));
		else
		{
			ApiSystem::startUpdate(mWindow);

			s->setVariable("closeGuiMenu", true);
			s->close();			
		}
	});

	s->updatePosition();

	auto pthis = this;

	s->onFinalize([s, pthis, window]
	{
		if (s->getVariable("closeGuiMenu"))
			delete pthis;
	});

	mWindow->pushGui(s);

}
	

void GuiMenu::openOtherSettings()
{
	Window* window = mWindow;
	auto s = new GuiSettings(mWindow, _("ADVANCED SETTINGS"));

	/*
	// Emulator settings 
	for (auto system : SystemData::sSystemVector)
	{
		if (system->isCollection() || system->getSystemEnvData()->mEmulators.size() == 0 || (system->getSystemEnvData()->mEmulators.size() == 1 && system->getSystemEnvData()->mEmulators[0].mCores.size() <= 1))
			continue;

		s->addEntry(_("EMULATOR SETTINGS"), true, [this] { openEmulatorSettings(); }, "iconGames");
		break;
	}
	*/

	s->addEntry(_("DATE & TIME"), true, [this] {
		openDateTimeSettings();
	}, "");

	//Timezone - Adapted from emuelec

	auto es_timezones = std::make_shared<OptionListComponent<std::string> >(mWindow, _("TIMEZONE"), false);

	std::string currentTimezone = SystemConf::getInstance()->get("system.timezone");

	if (currentTimezone.empty())
		currentTimezone = std::string(getShOutput(R"(/usr/local/bin/timezones current)"));
	std::string a;
	for(std::stringstream ss(getShOutput(R"(/usr/local/bin/timezones available)")); getline(ss, a, ','); ) {
		es_timezones->add(a, a, currentTimezone == a);
	}
	s->addWithLabel(_("TIMEZONE"), es_timezones);
	s->addSaveFunc([es_timezones, this] {
		if (es_timezones->changed()) {
			std::string selectedTimezone = es_timezones->getSelected();
			runSystemCommand("sudo ln -sf /usr/share/zoneinfo/" + selectedTimezone + " /etc/localtime", "", nullptr);
			std::string setRepo = std::string(getShOutput(R"(/usr/local/bin/timezones setrepo)"));
			if (!setRepo.empty())
			  mWindow->displayNotificationMessage(setRepo);
		}
		SystemConf::getInstance()->set("system.timezone", es_timezones->getSelected());
	});

	// Clock time format (14:42 or 2:42 pm)
	auto tmFormat = std::make_shared<SwitchComponent>(mWindow);
	tmFormat->setState(Settings::getInstance()->getBool("ClockMode12"));
	s->addWithLabel(_("SHOW CLOCK IN 12-HOUR FORMAT"), tmFormat);
	s->addSaveFunc([tmFormat] { Settings::getInstance()->setBool("ClockMode12", tmFormat->getState()); });

    //Switch A and B buttons
    
	auto invertJoy = std::make_shared<SwitchComponent>(mWindow);
	invertJoy->setState(Settings::getInstance()->getBool("InvertButtons"));
	s->addWithLabel(_("SWITCH A/B BUTTONS IN EMULATIONSTATION"), invertJoy);
	s->addSaveFunc([this, s, invertJoy]
	{
		if (Settings::getInstance()->setBool("InvertButtons", invertJoy->getState()))
		{
			InputConfig::AssignActionButtons();
			ViewController::get()->reloadAll(mWindow);
		}
	});

    //Flip power button suspend/poweroff function
    
	auto powerBtn = std::make_shared<SwitchComponent>(mWindow);
	powerBtn->setState(Settings::getInstance()->getBool("InvertPwrBtn"));
	s->addWithLabel(_("SWITCH POWER BUTTON TAP TO OFF"), powerBtn);
	s->addSaveFunc([this, s, powerBtn]
	{
		if (Settings::getInstance()->setBool("InvertPwrBtn", powerBtn->getState()))
		{
          if (Settings::getInstance()->getBool("InvertPwrBtn") == 1)
            runSystemCommand("[ -z $(find /home/ark/.config/.SWAPPOWERANDSUSPEND) ] && touch /home/ark/.config/.SWAPPOWERANDSUSPEND", "", nullptr);
		  else
            runSystemCommand("[ ! -z $(find /home/ark/.config/.SWAPPOWERANDSUSPEND) ] && rm /home/ark/.config/.SWAPPOWERANDSUSPEND", "", nullptr);
		}
	});

	// --- Root File Access toggle ---
	auto rootFileAccessSwitch = std::make_shared<SwitchComponent>(mWindow);
	rootFileAccessSwitch->setState(isRootFileAccessEnabled());
	rootFileAccessSwitch->setOnChangedCallback([rootFileAccessSwitch] {
		bool enable = rootFileAccessSwitch->getState();
		std::thread([enable] {
			toggleRootFileAccess(enable);
		}).detach();
	});
	s->addWithLabel(_("ROOT FILE ACCESS"), rootFileAccessSwitch);

	// joystick deadzone
	struct DeadzoneOption { std::string label; std::string hex; std::string dec; };
	static const std::vector<DeadzoneOption> deadzoneOptions = {
		{ "64 (stock)",       "0x040", "64"  },
		{ "128",              "0x080", "128" },
		{ "256",              "0x100", "256" },
		{ "384 (recommended)","0x180", "384" },
		{ "512",              "0x200", "512" },
		{ "768 (extreme)",    "0x300", "768" }
	};

	std::string currentDeadzone = getDeadzoneDecimal();

	std::string currentDeadzoneHex;
	for (auto it = deadzoneOptions.cbegin(); it != deadzoneOptions.cend(); it++)
		if (it->dec == currentDeadzone) { currentDeadzoneHex = it->hex; break; }

	auto deadzone = std::make_shared< OptionListComponent<std::string> >(mWindow, _("JOYSTICK DEADZONE"), false);
	for (auto it = deadzoneOptions.cbegin(); it != deadzoneOptions.cend(); it++)
		deadzone->add(_(it->label.c_str()), it->hex, it->dec == currentDeadzone);

	s->addWithLabel(_("JOYSTICK DEADZONE"), deadzone);
	s->addSaveFunc([this, deadzone, currentDeadzoneHex] {
		std::string selectedHex = deadzone->getSelected();
		if (selectedHex == currentDeadzoneHex)
			return;

		for (auto it = deadzoneOptions.cbegin(); it != deadzoneOptions.cend(); it++)
		{
			if (it->hex == selectedHex)
			{
				setDeadzoneValue(it->hex, it->dec);
				mWindow->pushGui(new GuiMsgBox(mWindow, _("DEADZONE CHANGED") + "\n" + _("REBOOT REQUIRED FOR FULL EFFECT"), _("OK")));
				break;
			}
		}
	});

	// power saver
	auto power_saver = std::make_shared< OptionListComponent<std::string> >(mWindow, _("POWER SAVER MODES"), false);
	std::vector<std::string> modes;
	modes.push_back("disabled");
	modes.push_back("default");
	modes.push_back("enhanced");
	modes.push_back("instant");
	for (auto it = modes.cbegin(); it != modes.cend(); it++)
		power_saver->add(_(it->c_str()), *it, Settings::getInstance()->getString("PowerSaverMode") == *it);

	s->addWithLabel(_("POWER SAVER MODES"), power_saver);
	s->addSaveFunc([this, power_saver] {
		if (Settings::getInstance()->getString("PowerSaverMode") != "instant" && power_saver->getSelected() == "instant") {
			Settings::getInstance()->setString("TransitionStyle", "instant");
			Settings::getInstance()->setString("GameTransitionStyle", "instant");
			Settings::getInstance()->setBool("MoveCarousel", false);
			Settings::getInstance()->setBool("EnableSounds", false);
		}

		GuiComponent::ALLOWANIMATIONS = Settings::getInstance()->getString("TransitionStyle") != "instant";

		Settings::getInstance()->setString("PowerSaverMode", power_saver->getSelected());
		PowerSaver::init();
	});

	// LANGUAGE

	std::vector<std::string> langues;
	langues.push_back("en");

	std::string xmlpath = ResourceManager::getInstance()->getResourcePath(":/splash.svg");
	if (xmlpath.length() > 0)
	{
		xmlpath = Utils::FileSystem::getParent(xmlpath) + "/locale/";

		Utils::FileSystem::stringList dirContent = Utils::FileSystem::getDirContent(xmlpath, true);
		for (Utils::FileSystem::stringList::const_iterator it = dirContent.cbegin(); it != dirContent.cend(); ++it)
		{
			if (Utils::FileSystem::isDirectory(*it))
				continue;

			std::string name = *it;

			if (name.rfind("emulationstation2.po") == std::string::npos)
				continue;

			name = Utils::FileSystem::createRelativePath(name, xmlpath, false);
			if (Utils::String::startsWith(name, "./"))
			{
				name = name.substr(2);

				while (name.find("/") != std::string::npos)
					name = Utils::FileSystem::getParent(name);
			}
			else
				name = Utils::FileSystem::getParent(name);

			name = Utils::FileSystem::getFileName(name);

			if (name != "en")
				langues.push_back(name);
		}

		if (langues.size() > 1)
		{
			auto language = std::make_shared< OptionListComponent<std::string> >(mWindow, _("LANGUAGE"), false);

			for (auto it = langues.cbegin(); it != langues.cend(); it++)
				language->add(*it, *it, Settings::getInstance()->getString("Language") == *it);

			s->addWithLabel(_("LANGUAGE"), language);
			s->addSaveFunc([language, window, s] {

				if (language->getSelected() != Settings::getInstance()->getString("Language"))
				{
					if (Settings::getInstance()->setString("Language", language->getSelected()))
						s->setVariable("reloadGuiMenu", true);
				}
			});
		}
	}


	// maximum vram
	auto max_vram = std::make_shared<SliderComponent>(mWindow, 40.f, 1000.f, 10.f, "Mb");
	max_vram->setValue((float)(Settings::getInstance()->getInt("MaxVRAM")));
	s->addWithLabel(_("VRAM LIMIT"), max_vram);
	s->addSaveFunc([max_vram] { Settings::getInstance()->setInt("MaxVRAM", (int)Math::round(max_vram->getValue())); });

	// gamelists
	auto save_gamelists = std::make_shared<SwitchComponent>(mWindow);
	save_gamelists->setState(Settings::getInstance()->getBool("SaveGamelistsOnExit"));
	s->addWithLabel(_("SAVE METADATA ON EXIT"), save_gamelists);
	s->addSaveFunc([save_gamelists] { Settings::getInstance()->setBool("SaveGamelistsOnExit", save_gamelists->getState()); });

	auto parse_gamelists = std::make_shared<SwitchComponent>(mWindow);
	parse_gamelists->setState(Settings::getInstance()->getBool("ParseGamelistOnly"));
	s->addWithLabel(_("PARSE GAMESLISTS ONLY"), parse_gamelists);
	s->addSaveFunc([parse_gamelists] { Settings::getInstance()->setBool("ParseGamelistOnly", parse_gamelists->getState()); });
	
#ifndef WIN32
	auto local_art = std::make_shared<SwitchComponent>(mWindow);
	local_art->setState(Settings::getInstance()->getBool("LocalArt"));
	s->addWithLabel(_("SEARCH FOR LOCAL ART"), local_art);
	s->addSaveFunc([local_art] { Settings::getInstance()->setBool("LocalArt", local_art->getState()); });
#endif

	s->addEntry(_("RESET FILE EXTENSIONS"), false, [this, s]
	{
		for (auto system : SystemData::sSystemVector)
			Settings::getInstance()->setString(system->getName() + ".HiddenExt", "");

		Settings::getInstance()->saveFile();
		reloadAllGames(mWindow, false);		
	});

#ifdef _RPI_
	// Video Player - VideoOmxPlayer
	auto omx_player = std::make_shared<SwitchComponent>(mWindow);
	omx_player->setState(Settings::getInstance()->getBool("VideoOmxPlayer"));
	s->addWithLabel("USE OMX PLAYER (HW ACCELERATED)", omx_player);
	s->addSaveFunc([omx_player]
	{
		// need to reload all views to re-create the right video components
		bool needReload = false;
		if(Settings::getInstance()->getBool("VideoOmxPlayer") != omx_player->getState())
			needReload = true;

		Settings::getInstance()->setBool("VideoOmxPlayer", omx_player->getState());

		if(needReload)
			ViewController::get()->reloadAll();
	});

#endif

	// preload UI
	auto preloadUI = std::make_shared<SwitchComponent>(mWindow);
	preloadUI->setState(Settings::getInstance()->getBool("PreloadUI"));
	s->addWithLabel(_("PRELOAD UI"), preloadUI);
	s->addSaveFunc([preloadUI] { Settings::getInstance()->setBool("PreloadUI", preloadUI->getState()); });
	
	// optimizeVram
	auto optimizeVram = std::make_shared<SwitchComponent>(mWindow);
	optimizeVram->setState(Settings::getInstance()->getBool("OptimizeVRAM"));
	s->addWithLabel(_("OPTIMIZE IMAGES VRAM USE"), optimizeVram);
	s->addSaveFunc([optimizeVram]
	{
		TextureData::OPTIMIZEVRAM = optimizeVram->getState();
		Settings::getInstance()->setBool("OptimizeVRAM", optimizeVram->getState());
	});

#ifdef WIN32
	// vsync
	auto vsync = std::make_shared<SwitchComponent>(mWindow);
	vsync->setState(Settings::getInstance()->getBool("VSync"));
	s->addWithLabel(_("VSYNC"), vsync);
	s->addSaveFunc([vsync] 
	{ 
		Settings::getInstance()->setBool("VSync", vsync->getState()); 
		Renderer::setSwapInterval();
	});
#endif

	// framerate	
	auto framerate = std::make_shared<SwitchComponent>(mWindow);
	framerate->setState(Settings::getInstance()->getBool("DrawFramerate"));
	s->addWithLabel(_("SHOW FRAMERATE"), framerate);
	s->addSaveFunc([framerate] { Settings::getInstance()->setBool("DrawFramerate", framerate->getState()); });

	// threaded loading
	auto threadedLoading = std::make_shared<SwitchComponent>(mWindow);
	threadedLoading->setState(Settings::getInstance()->getBool("ThreadedLoading"));
	s->addWithLabel(_("THREADED LOADING"), threadedLoading);
	s->addSaveFunc([threadedLoading] { Settings::getInstance()->setBool("ThreadedLoading", threadedLoading->getState()); });

	// global default emulator performance governor
	auto gdepg = std::make_shared< OptionListComponent<std::string> >(mWindow, _("Default Emulator Governor"), false);

	EmulatorData GOVs;
	std::vector<std::string> ggovs = GOVs.mGovernors;

	auto ggov = Settings::getInstance()->getString("GlobalPerformanceGovernor");
	if (ggov.empty())
		ggov = "performance";

	for (auto it = ggovs.cbegin(); it != ggovs.cend(); it++)
		gdepg->add(_(it->c_str()), *it, ggov == *it);

	s->addWithLabel(_("Default Emulator Governor"), gdepg);
	s->addSaveFunc([this, gdepg] { Settings::getInstance()->setString("GlobalPerformanceGovernor", gdepg->getSelected()); });

	// Auto Suspend Timeout
	auto Tout = std::make_shared< OptionListComponent<std::string> >(mWindow, _("Auto Suspend Timeout (mins)"), false);
	std::vector<std::string> asuspend;
	asuspend.push_back("Off");
	asuspend.push_back("5");
	asuspend.push_back("10");
	asuspend.push_back("15");
	asuspend.push_back("20");
	asuspend.push_back("25");
	asuspend.push_back("30");
	asuspend.push_back("35");
	asuspend.push_back("40");
	asuspend.push_back("45");
	asuspend.push_back("50");
	asuspend.push_back("55");
	asuspend.push_back("60");

	auto suspend = Settings::getInstance()->getString("AutoSuspendTimeout");
	if (suspend.empty())
		suspend = "Off";

	for (auto it = asuspend.cbegin(); it != asuspend.cend(); it++)
		Tout->add(_(it->c_str()), *it, suspend == *it);

	s->addWithLabel(_("Auto Suspend Timeout (mins)"), Tout);
	s->addSaveFunc([this, Tout] { Settings::getInstance()->setString("AutoSuspendTimeout", Tout->getSelected()); 
		if (Tout->changed()) {
		    runSystemCommand("echo " + Settings::getInstance()->getString("AutoSuspendTimeout") + " > /home/ark/.config/.TIMEOUT", "", nullptr);
		    runSystemCommand("/usr/local/bin/auto_suspend_update.sh", "", nullptr);
		}
	});

        // Battery Settings
	s->addEntry(_("BATTERYPLUS SETTINGS"), true, [this] { openBatterySettings(); }, "iconBattery");

#ifndef _RPI_
	// full exit
	auto fullExitMenu = std::make_shared<SwitchComponent>(mWindow);
	fullExitMenu->setState(!Settings::getInstance()->getBool("ShowOnlyExit"));
	s->addWithLabel(_("COMPLETE QUIT MENU"), fullExitMenu);
	s->addSaveFunc([fullExitMenu] { Settings::getInstance()->setBool("ShowOnlyExit", !fullExitMenu->getState()); });
#endif

	// log level
	auto logLevel = std::make_shared< OptionListComponent<std::string> >(mWindow, _("LOG LEVEL"), false);
	std::vector<std::string> levels;
	levels.push_back("default");
	levels.push_back("disabled");
	levels.push_back("warning");
	levels.push_back("error");
	levels.push_back("debug");

	auto level = Settings::getInstance()->getString("LogLevel");
	if (level.empty())
		level = "default";

	for (auto it = levels.cbegin(); it != levels.cend(); it++)
		logLevel->add(_(it->c_str()), *it, level == *it);

	s->addWithLabel(_("LOG LEVEL"), logLevel);
	s->addSaveFunc([this, logLevel]
	{
		if (Settings::getInstance()->setString("LogLevel", logLevel->getSelected() == "default" ? "" : logLevel->getSelected()))
		{
			Log::setupReportingLevel();
			Log::init();
		}
	});

	s->updatePosition();

	auto pthis = this;

	s->onFinalize([s, pthis, window]
	{
		if (s->getVariable("reloadGuiMenu"))
		{
			delete pthis;
			window->pushGui(new GuiMenu(window, false));
		}
	});

	mWindow->pushGui(s);

}

void GuiMenu::openConfigInput()
{
	Window* window = mWindow;
//	window->pushGui(new GuiDetectDevice(window, false, nullptr));
		
	window->pushGui(new GuiMsgBox(window, _("ARE YOU SURE YOU WANT TO CONFIGURE INPUT?"), _("YES"),
		[window] {
		window->pushGui(new GuiDetectDevice(window, false, nullptr));
	}, _("NO"), nullptr)
	);

}

void GuiMenu::openQuitMenu()
{
	if (Settings::getInstance()->getBool("ShowOnlyExit"))
	{
		Scripting::fireEvent("quit");
		quitES();
		return;
	}

	auto s = new GuiSettings(mWindow, _("QUIT"));

	Window* window = mWindow;

	ComponentListRow row;
	if (UIModeController::getInstance()->isUIModeFull())
	{
#ifndef WIN32
		// Restart does not work on Windows
		row.makeAcceptInputHandler([window] {
			window->pushGui(new GuiMsgBox(window, _("REALLY RESTART?"), _("YES"),
				[] {
				Scripting::fireEvent("quit");
				if(quitES(QuitMode::RESTART) != 0)
					LOG(LogWarning) << "Restart terminated with non-zero result!";
			}, _("NO"), nullptr));
		});
		row.addElement(std::make_shared<TextComponent>(window, _("RESTART EMULATIONSTATION"), ThemeData::getMenuTheme()->Text.font, ThemeData::getMenuTheme()->Text.color), true);
		s->addRow(row);
#endif

		if(Settings::getInstance()->getBool("ShowExit"))
		{
			row.elements.clear();
			row.makeAcceptInputHandler([window] {
				window->pushGui(new GuiMsgBox(window, _("REALLY QUIT?"), _("YES"),
					[] {
					Scripting::fireEvent("quit");
					quitES();
				}, _("NO"), nullptr));
			});
			row.addElement(std::make_shared<TextComponent>(window, _("QUIT EMULATIONSTATION"), ThemeData::getMenuTheme()->Text.font, ThemeData::getMenuTheme()->Text.color), true);
			s->addRow(row);
		}
	}
	row.elements.clear();
	row.makeAcceptInputHandler([window] {
		window->pushGui(new GuiMsgBox(window, _("REALLY RESTART?"), _("YES"),
			[] {
			Scripting::fireEvent("quit", "reboot");
			Scripting::fireEvent("reboot");
			if (quitES(QuitMode::REBOOT) != 0)
				LOG(LogWarning) << "Restart terminated with non-zero result!";
		}, _("NO"), nullptr));
	});
	row.addElement(std::make_shared<TextComponent>(window, _("RESTART SYSTEM"), ThemeData::getMenuTheme()->Text.font, ThemeData::getMenuTheme()->Text.color), true);
	s->addRow(row);

	row.elements.clear();
	row.makeAcceptInputHandler([window] {
		window->pushGui(new GuiMsgBox(window, _("REALLY SHUTDOWN?"), _("YES"),
			[] {
			Scripting::fireEvent("quit", "shutdown");
			Scripting::fireEvent("shutdown");
			if (quitES(QuitMode::SHUTDOWN) != 0)
				LOG(LogWarning) << "Shutdown terminated with non-zero result!";
		}, _("NO"), nullptr));
	});
	row.addElement(std::make_shared<TextComponent>(window, _("SHUTDOWN SYSTEM"), ThemeData::getMenuTheme()->Text.font, ThemeData::getMenuTheme()->Text.color), true);
	s->addRow(row);

	s->updatePosition();
	mWindow->pushGui(s);
}

std::string getBuildTime()
{
	std::string datestr = __DATE__;
	std::string timestr = __TIME__;

	std::istringstream iss_date(datestr);
	std::string str_month;
	int day;
	int year;
	iss_date >> str_month >> day >> year;

	int month;
	if (str_month == "Jan") month = 1;
	else if (str_month == "Feb") month = 2;
	else if (str_month == "Mar") month = 3;
	else if (str_month == "Apr") month = 4;
	else if (str_month == "May") month = 5;
	else if (str_month == "Jun") month = 6;
	else if (str_month == "Jul") month = 7;
	else if (str_month == "Aug") month = 8;
	else if (str_month == "Sep") month = 9;
	else if (str_month == "Oct") month = 10;
	else if (str_month == "Nov") month = 11;
	else if (str_month == "Dec") month = 12;
	else exit(-1);

	for (std::string::size_type pos = timestr.find(':'); pos != std::string::npos; pos = timestr.find(':', pos))
		timestr[pos] = ' ';

	std::istringstream iss_time(timestr);
	int hour, min, sec;
	iss_time >> hour >> min >> sec;

	char buffer[100];
	
	sprintf(buffer, "%4d%.2d%.2d%.2d%.2d%.2d\n", year, month, day, hour, min, sec);

	return buffer;
}

void GuiMenu::addVersionInfo()
{
	std::string  buildDate = getBuildTime();
	//	(Settings::getInstance()->getBool("Debug") ? std::string( "   (" + Utils::String::toUpper(PROGRAM_BUILT_STRING) + ")") : (""));

	auto theme = ThemeData::getMenuTheme();
//	mVersion.setFont(Font::get(FONT_SIZE_SMALL));
//	mVersion.setColor(0x5E5E5EFF);

	mVersion.setFont(theme->Footer.font);
	mVersion.setColor(theme->Footer.color);

	mVersion.setLineSpacing(0);

#if WIN32
	std::string localVersion;
	std::string localVersionFile = Utils::FileSystem::getExePath() + "/version.info";
	if (Utils::FileSystem::exists(localVersionFile))
	{
		localVersion = Utils::FileSystem::readAllText(localVersionFile);
		localVersion = Utils::String::replace(Utils::String::replace(localVersion, "\r", ""), "\n", "");	
		mVersion.setText("EMULATIONSTATION V" + localVersion+" FCAMOD");	
	}
	else
#endif
		mVersion.setText("EMULATIONSTATION V" + Utils::String::toUpper(PROGRAM_VERSION_STRING) + " BUILD " + buildDate);

	mVersion.setHorizontalAlignment(ALIGN_CENTER);	
	mVersion.setVerticalAlignment(ALIGN_CENTER);
	addChild(&mVersion);
}

void GuiMenu::openScreensaverOptions() {
	mWindow->pushGui(new GuiGeneralScreensaverOptions(mWindow, _("SCREENSAVER SETTINGS")));
}

void GuiMenu::openCollectionSystemSettings() 
{
	if (ThreadedScraper::isRunning())
	{
		mWindow->pushGui(new GuiMsgBox(mWindow, _("THIS FUNCTION IS DISABLED WHEN SCRAPING IS RUNNING")));
		return;
	}

	mWindow->pushGui(new GuiCollectionSystemsOptions(mWindow));
}

void GuiMenu::onSizeChanged()
{
	float h = mMenu.getButtonGridHeight();

	mVersion.setSize(mSize.x(), h);
	mVersion.setPosition(0, mSize.y() - h); //  mVersion.getSize().y()
}

/*
void GuiMenu::openQuickStatusMenu()
{
	auto s = new GuiSettings(mWindow, _("QUICK SETTINGS"));
	auto theme = ThemeData::getMenuTheme();

	ComponentListRow row;

	row.elements.clear();
	row.makeAcceptInputHandler([this] { openBatterySettings(); });
	row.addElement(std::make_shared<TextComponent>(mWindow, _("BATTERY PLUS"), theme->Text.font, theme->Text.color), true);
	row.addElement(makeArrow(mWindow), false);
	s->addRow(row);

	row.elements.clear();
	row.makeAcceptInputHandler([this] { openSoundSettings(); });
	row.addElement(std::make_shared<TextComponent>(mWindow, _("SOUND"), theme->Text.font, theme->Text.color), true);
	row.addElement(makeArrow(mWindow), false);
	s->addRow(row);

	row.elements.clear();
	row.makeAcceptInputHandler([this] { openDisplaySettings(); });
	row.addElement(std::make_shared<TextComponent>(mWindow, _("BRIGHTNESS"), theme->Text.font, theme->Text.color), true);
	row.addElement(makeArrow(mWindow), false);
	s->addRow(row);

	row.elements.clear();
	row.makeAcceptInputHandler([this] { openNetworkSettings(); });
	row.addElement(std::make_shared<TextComponent>(mWindow, _("WI-FI"), theme->Text.font, theme->Text.color), true);
	row.addElement(makeArrow(mWindow), false);
	s->addRow(row);

	s->updatePosition();
	mWindow->pushGui(s);
}
*/

void GuiMenu::addEntry(std::string name, bool add_arrow, const std::function<void()>& func, const std::string iconName)
{
	auto theme = ThemeData::getMenuTheme();
	std::shared_ptr<Font> font = theme->Text.font;
	unsigned int color = theme->Text.color;

	// populate the list
	ComponentListRow row;

	if (!iconName.empty())
	{
		std::string iconPath = theme->getMenuIcon(iconName);
		if (!iconPath.empty())
		{
			// icon
			auto icon = std::make_shared<ImageComponent>(mWindow);
			icon->setImage(iconPath);
			icon->setColorShift(theme->Text.color);
			icon->setResize(0, theme->Text.font->getLetterHeight() * 1.25f);
			row.addElement(icon, false);

			// spacer between icon and text
			auto spacer = std::make_shared<GuiComponent>(mWindow);
			spacer->setSize(10, 0);
			row.addElement(spacer, false);
		}
	}

	row.addElement(std::make_shared<TextComponent>(mWindow, name, font, color), true);

	if (add_arrow)
	{
		std::shared_ptr<ImageComponent> bracket = makeArrow(mWindow);
		row.addElement(bracket, false);
	}

	row.makeAcceptInputHandler(func);
	mMenu.addRow(row);
}

bool GuiMenu::input(InputConfig* config, Input input)
{
	if(GuiComponent::input(config, input))
		return true;

	if((config->isMappedTo(BUTTON_BACK, input) || config->isMappedTo("start", input)) && input.value != 0)
	{
		delete this;
		return true;
	}

	return false;
}

HelpStyle GuiMenu::getHelpStyle()
{
	HelpStyle style = HelpStyle();

	if (ThemeData::getDefaultTheme() != nullptr)
	{
		std::shared_ptr<ThemeData> theme = std::shared_ptr<ThemeData>(ThemeData::getDefaultTheme(), [](ThemeData*) {});
		style.applyTheme(theme, "system");
	}
	else 
		style.applyTheme(ViewController::get()->getState().getSystem()->getTheme(), "system");

	return style;
}

std::vector<HelpPrompt> GuiMenu::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	prompts.push_back(HelpPrompt("up/down", _("CHOOSE")));
	prompts.push_back(HelpPrompt(BUTTON_OK, _("SELECT")));
	prompts.push_back(HelpPrompt("start", _("CLOSE")));
	return prompts;
}

void GuiMenu::createInputTextRow(GuiSettings *gui, std::string title, const char *settingsID, bool password)
{
	auto theme = ThemeData::getMenuTheme();
	std::shared_ptr<Font> font = theme->Text.font;
	unsigned int color = theme->Text.color;

	// LABEL
	Window *window = mWindow;
	ComponentListRow row;

	auto lbl = std::make_shared<TextComponent>(window, title, font, color);
	row.addElement(lbl, true); // label

	std::shared_ptr<GuiComponent> ed;

	std::string value = Settings::getInstance()->getString(settingsID);

	ed = std::make_shared<TextComponent>(window, ((password && value != "") ? "*********" : value), font, color, ALIGN_RIGHT); // Font::get(FONT_SIZE_MEDIUM, FONT_PATH_LIGHT)
	row.addElement(ed, true);

	auto spacer = std::make_shared<GuiComponent>(mWindow);
	spacer->setSize(Renderer::getScreenWidth() * 0.005f, 0);
	row.addElement(spacer, false);

	auto bracket = std::make_shared<ImageComponent>(mWindow);
	bracket->setImage(theme->Icons.arrow);
	bracket->setResize(Vector2f(0, lbl->getFont()->getLetterHeight()));
	row.addElement(bracket, false);

	auto updateVal = [ed, settingsID, password](const std::string &newVal) {
		if (!password)
			ed->setValue(newVal);
		else {
			ed->setValue("*********");
		}

		Settings::getInstance()->setString(settingsID, newVal);
	}; // ok callback (apply new value to ed)

	row.makeAcceptInputHandler([this, title, updateVal, settingsID]
	{
		std::string data = Settings::getInstance()->getString(settingsID);
		mWindow->pushGui(new GuiTextEditPopupKeyboard(mWindow, title, data, updateVal, false));
	});

	gui->addRow(row);
}
