#include "components/NetworkIconComponent.h"
#include "Settings.h"
#include <sstream>
#include <mutex>
#include <cstdio>

#define UPDATE_NETWORK_DELAY 2000

static std::mutex g_networkCmdMutex;
static std::string executeNetworkCommand(const std::string& cmd)
{
	std::lock_guard<std::mutex> lock(g_networkCmdMutex);

	FILE* pipe = popen(cmd.c_str(), "r");
	if (!pipe)
		return "";

	char buffer[256];
	std::string result;
	while (fgets(buffer, sizeof(buffer), pipe))
		result += buffer;

	pclose(pipe);
	return result;
}

// Same check the WiFi Manager settings menu uses (see getActiveWifiInterface() in GuiMenu.cpp):
// nmcli device list, looking for a wifi-type device in "connected" state.
static bool checkNetworkConnected()
{
	std::string result = executeNetworkCommand("nmcli -t -f DEVICE,TYPE,STATE dev 2>/dev/null");
	if (result.empty())
		return false;

	std::istringstream stream(result);
	std::string line;
	while (std::getline(stream, line))
	{
		if (line.find(":wifi:") != std::string::npos && line.find(":connected") != std::string::npos)
			return true;
	}

	return false;
}

NetworkIconComponent::NetworkIconComponent(Window* window) : ImageComponent(window), mConnected(false), mUpdateElapsed(0)
{
	setVisible(false);
}

void NetworkIconComponent::update(int deltaTime)
{
	ImageComponent::update(deltaTime);

	mUpdateElapsed += deltaTime;
	if (mUpdateElapsed < UPDATE_NETWORK_DELAY)
		return;

	mUpdateElapsed = 0;

	mConnected = Settings::getInstance()->getBool("ShowNetworkIndicator") && checkNetworkConnected();

	setVisible(mConnected);

	if (mConnected && !mNetworkIcon.empty())
		setImage(mNetworkIcon);
}

void NetworkIconComponent::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties)
{
	ImageComponent::applyTheme(theme, view, element, properties);

	const ThemeData::ThemeElement* elem = theme->getElement(view, element, getThemeTypeName());
	if (!elem)
		return;

	if (elem->has("networkIcon"))
		mNetworkIcon = elem->get<std::string>("networkIcon");
}