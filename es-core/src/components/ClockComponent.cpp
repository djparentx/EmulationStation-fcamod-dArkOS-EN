#include "components/ClockComponent.h"
#include "Settings.h"
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
}

void ClockComponent::onHide()
{
	TextComponent::onHide();
	mActive = false;
}

void ClockComponent::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties)
{
	TextComponent::applyThemeWithType(theme, view, element, properties, "clock");
}

void ClockComponent::update(int deltaTime)
{
	TextComponent::update(deltaTime);

	if (!mActive)
		return;

	setVisible(Settings::getInstance()->getBool("DrawClock"));

	if (!isVisible())
		return;

	mClockElapsed -= deltaTime;
	if (mClockElapsed <= 0)
	{
		time_t     clockNow = time(0);
		struct tm  clockTstruct = *localtime(&clockNow);

		if (clockTstruct.tm_year > 100)
		{
			// strftime, not Utils::Time::timeToString - the latter only supports
			// %Y %m %d %H %M %S and silently drops %I/%p (12-hour mode rendered ":20 ")
			char clockBuf[16];
			if (Settings::getInstance()->getBool("ClockMode12"))
				strftime(clockBuf, sizeof(clockBuf), "%I:%M %p", &clockTstruct);
			else
				strftime(clockBuf, sizeof(clockBuf), "%H:%M", &clockTstruct);

			setText(clockBuf);
		}

		mClockElapsed = 1000;
	}
}