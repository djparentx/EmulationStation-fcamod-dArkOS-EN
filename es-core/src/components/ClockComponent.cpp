#include "components/ClockComponent.h"
#include "Settings.h"
#include "utils/TimeUtil.h"
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