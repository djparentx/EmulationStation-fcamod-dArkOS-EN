#include "components/ClockComponent.h"
#include "Settings.h"
#include "utils/TimeUtil.h"
#include "Log.h"
#include <time.h>

ClockComponent::ClockComponent(Window* window) : TextComponent(window), mActive(false)
{
	mClockElapsed = 0;
}

void ClockComponent::onShow()
{
	TextComponent::onShow();
	mActive = true;
	mClockElapsed = 0;
	LOG(LogInfo) << "[ThemeDebug] ClockComponent::onShow tag=" << getTag();
}

void ClockComponent::onHide()
{
	TextComponent::onHide();
	mActive = false;
	LOG(LogInfo) << "[ThemeDebug] ClockComponent::onHide tag=" << getTag();
}

void ClockComponent::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties)
{
	TextComponent::applyThemeWithType(theme, view, element, properties, "clock");
}

void ClockComponent::update(int deltaTime)
{
	TextComponent::update(deltaTime);

	if (!mActive)
	{
		LOG(LogInfo) << "[ThemeDebug] ClockComponent::update tag=" << getTag() << " SKIPPED mActive=false";
		return;
	}

	setVisible(Settings::getInstance()->getBool("DrawClock"));

	if (!isVisible())
	{
		LOG(LogInfo) << "[ThemeDebug] ClockComponent::update tag=" << getTag() << " SKIPPED isVisible=false DrawClock=" << Settings::getInstance()->getBool("DrawClock");
		return;
	}

	mClockElapsed -= deltaTime;
	if (mClockElapsed <= 0)
	{
		time_t     clockNow = time(0);
		struct tm  clockTstruct = *localtime(&clockNow);

		LOG(LogInfo) << "[ThemeDebug] ClockComponent::update tag=" << getTag() << " tm_year=" << clockTstruct.tm_year
			<< " ClockMode12=" << Settings::getInstance()->getBool("ClockMode12");

		if (clockTstruct.tm_year > 100)
		{
			std::string clockBuf;
			if (Settings::getInstance()->getBool("ClockMode12"))
				clockBuf = Utils::Time::timeToString(clockNow, "%I:%M %p");
			else
				clockBuf = Utils::Time::timeToString(clockNow, "%H:%M");

			LOG(LogInfo) << "[ThemeDebug] ClockComponent::update tag=" << getTag() << " setText(\"" << clockBuf << "\")";

			setText(clockBuf);
		}

		mClockElapsed = 1000;
	}
}