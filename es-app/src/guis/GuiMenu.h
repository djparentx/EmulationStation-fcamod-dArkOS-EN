#include <string>
#pragma once
#ifndef ES_APP_GUIS_GUI_MENU_H
#define ES_APP_GUIS_GUI_MENU_H

#include "components/BusyComponent.h"
#include "components/MenuComponent.h"
#include "components/OptionListComponent.h"
#include "GuiComponent.h"

class GuiSettings;
class SystemData;

class GuiMenu : public GuiComponent
{
public:
	GuiMenu(Window* window, bool animate = true);

	bool input(InputConfig* config, Input input) override;
	void onSizeChanged() override;
	std::vector<HelpPrompt> getHelpPrompts() override;
	HelpStyle getHelpStyle() override;

	static void openThemeConfiguration(Window* mWindow, GuiComponent* s, std::shared_ptr<OptionListComponent<std::string>> theme_set, const std::string systemTheme = "");

private:
	void addEntry(std::string name, bool add_arrow, const std::function<void()>& func, const std::string iconName = "");	

	void addVersionInfo();
	void openCollectionSystemSettings();
	void openConfigInput();
	void openOtherSettings();
	void openQuitMenu();
	void openScraperSettings();
	void openScreensaverOptions();
	void scanWifi();
	void showWifiPasswordInput(const std::string& ssid);
	void connectWifi(const std::string& ssid, const std::string& password);
	void showHostnameInput(std::shared_ptr<TextComponent> hostnameText);
	void applyHostname(const std::string& newHostname, std::shared_ptr<TextComponent> hostnameText);
	void activateExistingConnection();
	void activateConnection(const std::string& connName);
	void deleteConnections();
	void openNetworkSettings();
	void openBatterySettings();
	void openSoundSettings();
	void openUISettings();

	static void reloadAllGames(Window* window, bool deleteCurrentGui = false);

	void openUpdateSettings();
	void openEmulatorSettings();
	void openSystemEmulatorSettings(SystemData* system);

	void createInputTextRow(GuiSettings *gui, std::string title, const char *settingsID, bool password);
	void openDisplaySettings();
	void openDateTimeSettings();
	void openManualDateTimeSettings(std::function<void()> onApplied = nullptr);

	void openPerformanceSettings();
	std::string getCpuBinning();
	std::string getCpuTemp();
	int getCpuCoreCount();
	int getOnlineCpuCount();
	std::string getCpuGovernor();
	void setCpuGovernor(const std::string& governor);
	std::string getCpuMaxFreq();
	void setCpuMaxFreq(const std::string& freq);
	std::vector<std::string> getCpuAvailableFreqs();
	std::vector<std::string> getAvailableGovernors();
	void setCpuCores(int count);
	bool isCpuBootApplyEnabled();
	void toggleCpuBootApply(bool enable);	
	bool hasGpuFreqControl();
	std::string getGpuDevPath();
	std::string getGpuMaxFreq();
	void setGpuMaxFreq(const std::string& freq);
	std::vector<std::string> getGpuAvailableFreqs();
	bool isGpuBootApplyEnabled();
	void toggleGpuBootApply(bool enable);
	void writeCpuBootConfig();
	void writeGpuBootConfig();
	void writeDmcBootConfig();
	bool isDmcBootApplyEnabled();
	void toggleDmcBootApply(bool enable);	
	bool hasDmcFreqControl();
	std::string getDmcMaxFreq();
	void setDmcMaxFreq(const std::string& freq);
	std::vector<std::string> getDmcAvailableFreqs();
	std::string getZramSize();
	bool isZramEnabled();
	std::string getZramCompAlgorithm();
	std::vector<std::string> getAvailableZramAlgorithms();
	void toggleZram(bool enable, const std::string& size = "512M", const std::string& compAlgo = "lz4");
	bool isZramAutoStart();
	void toggleZramAutoStart(bool enable, const std::string& size = "512M", const std::string& compAlgo = "lz4");
	void saveZramConfig(const std::string& size, const std::string& compAlgo);

	MenuComponent mMenu;
	TextComponent mVersion;
	std::shared_ptr<TextComponent> mWifiStatusText;
	std::vector<std::pair<std::string, int>> mWifiNetworks;
};

#endif // ES_APP_GUIS_GUI_MENU_H
