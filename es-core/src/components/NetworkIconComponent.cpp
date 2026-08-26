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

NetworkIconComponent::NetworkIconComponent(Window* window) : ImageComponent(window), mConnected(false), mUpdateElapsed(0), mActive(false)
{
	setVisible(false);
}

void NetworkIconComponent::onShow()
{
	ImageComponent::onShow();
	mActive = true;
	// Poll right away instead of waiting up to UPDATE_NETWORK_DELAY so a freshly-selected
	// system's status bar isn't showing stale/default state for up to 2 seconds.
	mUpdateElapsed = UPDATE_NETWORK_DELAY;
}

void NetworkIconComponent::onHide()
{
	ImageComponent::onHide();
	mActive = false;
}

void NetworkIconComponent::update(int deltaTime)
{
	ImageComponent::update(deltaTime);

	// SystemView creates one of these per carousel entry but calls update() on all of them
	// every frame regardless of selection - only do the actual (blocking, subprocess-based)
	// network check while this entry is the one currently shown.
	if (!mActive)
		return;

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

	const ThemeData::ThemeElement* elem = theme->getElement(view, element, "networkIcon");
	if (!elem)
		return;

	if (elem->has("networkIcon"))
		mNetworkIcon = elem->get<std::string>("networkIcon");
}