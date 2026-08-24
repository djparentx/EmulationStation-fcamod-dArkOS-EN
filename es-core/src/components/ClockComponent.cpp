#include "components/ClockComponent.h"
#include "Settings.h"
#include "utils/TimeUtil.h"
#include <time.h>

ClockComponent::ClockComponent(Window* window) : TextComponent(window)
{
	mClockElapsed = 0;
}

void ClockComponent::update(int deltaTime)
{
	TextComponent::update(deltaTime);

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
			std::string clockBuf;
			if (Settings::getInstance()->getBool("ClockMode12"))
				clockBuf = Utils::Time::timeToString(clockNow, "%I:%M %p");
			else
				clockBuf = Utils::Time::timeToString(clockNow, "%H:%M");

			setText(clockBuf);
		}

		mClockElapsed = 1000;
	}
}