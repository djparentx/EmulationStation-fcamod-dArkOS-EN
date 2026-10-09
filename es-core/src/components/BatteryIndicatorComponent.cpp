#include <string>
#include "components/BatteryIndicatorComponent.h"

#include "resources/TextureResource.h"
#include "ThemeData.h"
#include "InputManager.h"
#include "Settings.h"
#include <SDL_power.h>
#include "utils/StringUtil.h"

BatteryIndicatorComponent::BatteryIndicatorComponent(Window* window) : ControllerActivityComponent(window) { }

void BatteryIndicatorComponent::init()
{
	ControllerActivityComponent::init();

	mHorizontalAlignment = ALIGN_LEFT;

	if (Renderer::isSmallScreen())
	{
		setPosition(Renderer::getScreenWidth() * 0.010, Renderer::getScreenHeight() * 0);
		setSize(Renderer::getScreenWidth() * 0.320, Renderer::getScreenHeight() * 0.065);
	}
	else
	{
		setPosition(Renderer::getScreenWidth() * 0.955, Renderer::getScreenHeight() *0.0125);
		setSize(Renderer::getScreenWidth() * 0.033, Renderer::getScreenHeight() *0.033);
	}

	// Use the same gap between icons (network/bluetooth/battery) as the one used
	// between the battery icon and its percentage text, for a consistent look.
	// Reproduce the exact original battery-icon-to-percentage gap
	// (old mSpacing + old batteryTextPad) and apply it uniformly to every
	// icon in the row (network/bluetooth/battery/text).
	mSpacing = (Renderer::getScreenHeight() / 200.0f) + (mSize.y() * 0.125f);

	mView = ActivityView::BATTERY;

	if (ResourceManager::getInstance()->fileExists(":/battery/incharge.svg"))
		mIncharge = ResourceManager::getInstance()->getResourcePath(":/battery/incharge.svg");

	if (ResourceManager::getInstance()->fileExists(":/battery/full.svg"))
		mFull = ResourceManager::getInstance()->getResourcePath(":/battery/full.svg");

	if (ResourceManager::getInstance()->fileExists(":/battery/75.svg"))
		mAt75 = ResourceManager::getInstance()->getResourcePath(":/battery/75.svg");

	if (ResourceManager::getInstance()->fileExists(":/battery/50.svg"))
		mAt50 = ResourceManager::getInstance()->getResourcePath(":/battery/50.svg");

	if (ResourceManager::getInstance()->fileExists(":/battery/25.svg"))
		mAt25 = ResourceManager::getInstance()->getResourcePath(":/battery/25.svg");

	if (ResourceManager::getInstance()->fileExists(":/battery/empty.svg"))
		mEmpty = ResourceManager::getInstance()->getResourcePath(":/battery/empty.svg");	

	if (ResourceManager::getInstance()->fileExists(":/battery/empty.svg"))
		mEmpty = ResourceManager::getInstance()->getResourcePath(":/battery/empty.svg");

	if (ResourceManager::getInstance()->fileExists(":/network.svg") && Settings::getInstance()->getBool("networkIcon"))
	{
		mView |= ActivityView::NETWORK;
		mNetworkImage = TextureResource::get(ResourceManager::getInstance()->getResourcePath(":/network.svg"), false, true);
		mNetworkActiveImage = TextureResource::get(ResourceManager::getInstance()->getResourcePath(":/network_active.svg"), false, true);
		mNetworkOffImage = TextureResource::get(ResourceManager::getInstance()->getResourcePath(":/network_off.svg"), false, true);
		mNetworkShareImage = TextureResource::get(ResourceManager::getInstance()->getResourcePath(":/network_share.svg"), false, true);
		mNetworkServiceImage = TextureResource::get(ResourceManager::getInstance()->getResourcePath(":/network_service.svg"), false, true);
	}

	if (Settings::getInstance()->getBool("bluetoothIcon") && ResourceManager::getInstance()->fileExists(":/bluetooth.svg"))
	{
		mView |= ActivityView::BLUETOOTH;
		mBluetoothImage = TextureResource::get(ResourceManager::getInstance()->getResourcePath(":/bluetooth.svg"), false, true);
		mBluetoothActiveImage = TextureResource::get(ResourceManager::getInstance()->getResourcePath(":/bluetooth_active.svg"), false, true);
		mBluetoothOffImage = TextureResource::get(ResourceManager::getInstance()->getResourcePath(":/bluetooth_off.svg"), false, true);
	}

	updateBatteryInfo();
}

void ControllerActivityComponent::reloadNetworkIcons()
{
	// Drop the existing references first: TextureResource caches by path using a
	// weak_ptr, so as long as we hold the last strong reference the cache entry
	// stays "alive" and get() below would just return the stale texture again.
	mNetworkImage = nullptr;
	mNetworkActiveImage = nullptr;
	mNetworkOffImage = nullptr;
	mNetworkShareImage = nullptr;
	mNetworkServiceImage = nullptr;
	mBluetoothImage = nullptr;
	mBluetoothActiveImage = nullptr;
	mBluetoothOffImage = nullptr;

	if (ResourceManager::getInstance()->fileExists(":/network.svg") && Settings::getInstance()->getBool("networkIcon"))
	{
		mNetworkImage = TextureResource::get(ResourceManager::getInstance()->getResourcePath(":/network.svg"), false, true);
		mNetworkActiveImage = TextureResource::get(ResourceManager::getInstance()->getResourcePath(":/network_active.svg"), false, true);
		mNetworkOffImage = TextureResource::get(ResourceManager::getInstance()->getResourcePath(":/network_off.svg"), false, true);
		mNetworkShareImage = TextureResource::get(ResourceManager::getInstance()->getResourcePath(":/network_share.svg"), false, true);
		mNetworkServiceImage = TextureResource::get(ResourceManager::getInstance()->getResourcePath(":/network_service.svg"), false, true);
	}

	if (Settings::getInstance()->getBool("bluetoothIcon") && ResourceManager::getInstance()->fileExists(":/bluetooth.svg"))
	{
		mBluetoothImage = TextureResource::get(ResourceManager::getInstance()->getResourcePath(":/bluetooth.svg"), false, true);
		mBluetoothActiveImage = TextureResource::get(ResourceManager::getInstance()->getResourcePath(":/bluetooth_active.svg"), false, true);
		mBluetoothOffImage = TextureResource::get(ResourceManager::getInstance()->getResourcePath(":/bluetooth_off.svg"), false, true);
	}
}
