#include "components/BatteryIconComponent.h"
#include "Settings.h"

#define UPDATE_BATTERY_DELAY 2000

BatteryIconComponent::BatteryIconComponent(Window* window) : ImageComponent(window), mUpdateElapsed(0)
{
	mBatteryInfo = BatteryInformation();
}

void BatteryIconComponent::update(int deltaTime)
{
	ImageComponent::update(deltaTime);

	mUpdateElapsed += deltaTime;
	if (mUpdateElapsed < UPDATE_BATTERY_DELAY)
		return;

	mUpdateElapsed = 0;

	if (Settings::getInstance()->getString("ShowBattery").empty())
		mBatteryInfo.hasBattery = false;
	else
		mBatteryInfo = queryBatteryInformation(false);

	setVisible(mBatteryInfo.hasBattery);

	if (!mBatteryInfo.hasBattery)
		return;

	std::string txName = mEmpty;

	if (mBatteryInfo.isCharging && !mIncharge.empty())
		txName = mIncharge;
	else if (mBatteryInfo.level > 75 && !mFull.empty())
		txName = mFull;
	else if (mBatteryInfo.level > 50 && !mAt75.empty())
		txName = mAt75;
	else if (mBatteryInfo.level > 25 && !mAt50.empty())
		txName = mAt50;
	else if (mBatteryInfo.level > 5 && !mAt25.empty())
		txName = mAt25;

	if (!txName.empty())
		setImage(txName);
}

void BatteryIconComponent::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties)
{
	ImageComponent::applyTheme(theme, view, element, properties);

	const ThemeData::ThemeElement* elem = theme->getElement(view, element, getThemeTypeName());
	if (!elem)
		return;

	if (elem->has("incharge"))
		mIncharge = elem->get<std::string>("incharge");

	if (elem->has("full"))
		mFull = elem->get<std::string>("full");

	if (elem->has("at75"))
		mAt75 = elem->get<std::string>("at75");

	if (elem->has("at50"))
		mAt50 = elem->get<std::string>("at50");

	if (elem->has("at25"))
		mAt25 = elem->get<std::string>("at25");

	if (elem->has("empty"))
		mEmpty = elem->get<std::string>("empty");
}