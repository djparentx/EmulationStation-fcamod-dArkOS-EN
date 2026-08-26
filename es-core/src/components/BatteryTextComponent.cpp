#include "components/BatteryTextComponent.h"
#include "Settings.h"

#define UPDATE_BATTERY_DELAY 2000

BatteryTextComponent::BatteryTextComponent(Window* window) : TextComponent(window), mUpdateElapsed(0), mActive(false)
{
	mBatteryInfo = BatteryInformation();
}

void BatteryTextComponent::onShow()
{
	TextComponent::onShow();
	mActive = true;
	mUpdateElapsed = UPDATE_BATTERY_DELAY;
}

void BatteryTextComponent::onHide()
{
	TextComponent::onHide();
	mActive = false;
}

void BatteryTextComponent::update(int deltaTime)
{
	TextComponent::update(deltaTime);

	if (!mActive)
		return;

	mUpdateElapsed += deltaTime;
	if (mUpdateElapsed < UPDATE_BATTERY_DELAY)
		return;

	mUpdateElapsed = 0;

	if (Settings::getInstance()->getString("ShowBattery") != "text")
		mBatteryInfo.hasBattery = false;
	else
		mBatteryInfo = queryBatteryInformation(false);

	setVisible(mBatteryInfo.hasBattery && mBatteryInfo.level >= 0);

	if (mBatteryInfo.hasBattery)
		setText(std::to_string(mBatteryInfo.level) + "%");
}