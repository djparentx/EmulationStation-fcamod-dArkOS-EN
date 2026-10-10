#include <string>
#include "utils/TimeUtil.h"
#include "utils/StringUtil.h"
#include "EsLocale.h"
#include "Settings.h"

#include <time.h>

namespace Utils
{
	namespace Time
	{
		DateTime::DateTime()
		{
			mTime       = 0;
			mTimeStruct = { 0, 0, 0, 1, 0, 0, 0, 0, -1 };
			mIsoString  = "00000000T000000";

		} // DateTime::DateTime

		DateTime::DateTime(const time_t& _time)
		{
			setTime(_time);

		} // DateTime::DateTime

		DateTime::DateTime(const tm& _timeStruct)
		{
			setTimeStruct(_timeStruct);

		} // DateTime::DateTime

		DateTime::DateTime(const std::string& _isoString)
		{
			setIsoString(_isoString);

		} // DateTime::DateTime

		DateTime::~DateTime()
		{

		} // DateTime::~DateTime

		void DateTime::setTime(const time_t& _time)
		{
			mTime       = (_time < 0) ? 0 : _time;
			mTimeStruct = *localtime(&mTime);
			mIsoString  = timeToString(mTime);

		} // DateTime::setTime

		void DateTime::setTimeStruct(const tm& _timeStruct)
		{
			setTime(mktime((tm*)&_timeStruct));

		} // DateTime::setTimeStruct

		void DateTime::setIsoString(const std::string& _isoString)
		{
			setTime(stringToTime(_isoString));

		} // DateTime::setIsoString

		Duration::Duration(const time_t& _time)
		{
			mTotalSeconds = (unsigned int)_time;
			mDays         = (mTotalSeconds - (mTotalSeconds % (60*60*24))) / (60*60*24);
			mHours        = ((mTotalSeconds % (60*60*24)) - (mTotalSeconds % (60*60))) / (60*60);
			mMinutes      = ((mTotalSeconds % (60*60)) - (mTotalSeconds % (60))) / 60;
			mSeconds      = mTotalSeconds % 60;

		} // Duration::Duration

		Duration::~Duration()
		{

		} // Duration::~Duration

		time_t now()
		{
			time_t time;
			::time(&time);
			return time;

		} // now

		time_t stringToTime(const std::string& _string, const std::string& _format)
		{
			const char* s           = _string.c_str();
			const char* f           = _format.c_str();
			tm          timeStruct  = { 0, 0, 0, 1, 0, 0, 0, 0, -1 };
			size_t      parsedChars = 0;

			if(_string == "not-a-date-time")
				return mktime(&timeStruct);

			while(*f && (parsedChars < _string.length()))
			{
				if(*f == '%')
				{
					++f;

					switch(*f++)
					{
						case 'Y': // The year [1970,xxxx]
						{
							if((parsedChars + 4) <= _string.length())
							{
								timeStruct.tm_year  = (*s++ - '0') * 1000;
								timeStruct.tm_year += (*s++ - '0') * 100;
								timeStruct.tm_year += (*s++ - '0') * 10;
								timeStruct.tm_year += (*s++ - '0');
								if(timeStruct.tm_year >= 1900)
									timeStruct.tm_year -= 1900;
							}

							parsedChars += 4;
						}
						break;

						case 'm': // The month number [01,12]
						{
							if((parsedChars + 2) <= _string.length())
							{
								timeStruct.tm_mon  = (*s++ - '0') * 10;
								timeStruct.tm_mon += (*s++ - '0');
								if(timeStruct.tm_mon >= 1)
									timeStruct.tm_mon -= 1;
							}

							parsedChars += 2;
						}
						break;

						case 'd': // The day of the month [01,31]
						{
							if((parsedChars + 2) <= _string.length())
							{
								timeStruct.tm_mday  = (*s++ - '0') * 10;
								timeStruct.tm_mday += (*s++ - '0');
							}

							parsedChars += 2;
						}
						break;

						case 'H': // The hour (24-hour clock) [00,23]
						{
							if((parsedChars + 2) <= _string.length())
							{
								timeStruct.tm_hour  = (*s++ - '0') * 10;
								timeStruct.tm_hour += (*s++ - '0');
							}

							parsedChars += 2;
						}
						break;

						case 'M': // The minute [00,59]
						{
							if((parsedChars + 2) <= _string.length())
							{
								timeStruct.tm_min  = (*s++ - '0') * 10;
								timeStruct.tm_min += (*s++ - '0');
							}

							parsedChars += 2;
						}
						break;

						case 'S': // The second [00,59]
						{
							if((parsedChars + 2) <= _string.length())
							{
								timeStruct.tm_sec  = (*s++ - '0') * 10;
								timeStruct.tm_sec += (*s++ - '0');
							}

							parsedChars += 2;
						}
						break;
					}
				}
				else
				{
					++s;
					++f;
				}
			}

			return mktime(&timeStruct);

		} // stringToTime

		std::string timeToString(const time_t& _time, const std::string& _format)
		{
			// strftime: superset of the old hand-rolled %Y %m %d %H %M %S formatter
			// (which silently dropped %I %p %y %b ... - see ClockComponent 12h bug)
			const tm timeStruct = *localtime(&_time);
			char buf[256] = { '\0' };

			if (strftime(buf, sizeof(buf), _format.c_str(), &timeStruct) == 0)
				return "";

			return std::string(buf);

		} // timeToString

		int daysInMonth(const int _year, const int _month)
		{
			tm timeStruct = { 0, 0, 0, 0, _month, _year - 1900, 0, 0, -1 };
			mktime(&timeStruct);

			return timeStruct.tm_mday;

		} // daysInMonth

		int daysInYear(const int _year)
		{
			tm timeStruct = { 0, 0, 0, 0, 0, _year - 1900 + 1, 0, 0, -1 };
			mktime(&timeStruct);

			return timeStruct.tm_yday + 1;

		} // daysInYear

		// ===== ported from AmberELEC ES - used by MathExpr / theme bindings =====

		// AmberELEC asks the OS locale (nl_langinfo); fixed here so that theme-bound dates
		// ({game:releasedate}, {game:lastplayed}) and the date()/year()/... expression
		// functions always agree on one format
		std::string getSystemDateFormat(bool includeHours)
		{
			if (!includeHours)
				return "%m/%d/%Y";

			return Settings::getInstance()->getBool("ClockMode12") ? "%m/%d/%Y %I:%M %p" : "%m/%d/%Y %H:%M";
		}

		// transforms a number of seconds into a human readable string
		std::string secondsToString(const long seconds, bool asTime)
		{
			if (seconds == 0)
				return _("never");

			if (asTime)
			{
				int d = 0, h = 0, m = 0, s = 0;
				d = seconds / 86400;
				h = (seconds / 3600) % 24;
				m = (seconds / 60) % 60;
				s = seconds % 60;

				if (d > 0)
					return Utils::String::format("%02d %02d:%02d:%02d", d, h, m, s);
				else if (h > 0)
					return Utils::String::format("%02d:%02d:%02d", h, m, s);

				return Utils::String::format("%02d:%02d", m, s);
			}

			char buf[256];

			int d = 0, h = 0, m = 0, s = 0;
			d = seconds / 86400;
			h = (seconds / 3600) % 24;
			m = (seconds / 60) % 60;
			s = seconds % 60;
			if (d > 1)
			{
				snprintf(buf, 256, _("%d d").c_str(), d);
				if (h > 0)
				{
					std::string days(buf);
					snprintf(buf, 256, _("%d h").c_str(), h);
					if (m > 0)
					{
						std::string hours(buf);
						snprintf(buf, 256, _("%d mn").c_str(), m);
						return days + " " + hours + " " + std::string(buf);
					}
					return days + " " + std::string(buf);
				}
				else if (m > 0)
				{
					std::string days(buf);
					snprintf(buf, 256, _("%d mn").c_str(), m);
					return days + " " + std::string(buf);
				}
			}
			else if (h > 0 || d > 0)
			{
				if (d > 0)
					h += d * 24;

				snprintf(buf, 256, _("%d h").c_str(), h);
				if (m > 0)
				{
					std::string hours(buf);
					snprintf(buf, 256, _("%d mn").c_str(), m);
					return hours + " " + std::string(buf);
				}
			}
			else if (m > 0)
				snprintf(buf, 256, _("%d mn").c_str(), m);
			else
				snprintf(buf, 256, _("%d sec").c_str(), s);

			return std::string(buf);
		}

		std::string getElapsedSinceString(const time_t& _time)
		{
			if (_time == 0 || _time == -1)
				return _("never");

			Utils::Time::Duration dur(Utils::Time::now() - _time);

			char buf[256];

			if (dur.getDays() > 365)
			{
				unsigned int years = dur.getDays() / 365;
				snprintf(buf, 256, EsLocale::nGetText("%d year ago", "%d years ago", years).c_str(), years);
			}
			else if (dur.getDays() > 0)
				snprintf(buf, 256, EsLocale::nGetText("%d day ago", "%d days ago", dur.getDays()).c_str(), dur.getDays());
			else if (dur.getHours() > 0)
				snprintf(buf, 256, EsLocale::nGetText("%d hour ago", "%d hours ago", dur.getHours()).c_str(), dur.getHours());
			else if (dur.getMinutes() > 0)
				snprintf(buf, 256, EsLocale::nGetText("%d minute ago", "%d minutes ago", dur.getMinutes()).c_str(), dur.getMinutes());
			else
				snprintf(buf, 256, EsLocale::nGetText("%d second ago", "%d seconds ago", dur.getSeconds()).c_str(), dur.getSeconds());

			return std::string(buf);
		}

	} // Time::

} // Utils::
