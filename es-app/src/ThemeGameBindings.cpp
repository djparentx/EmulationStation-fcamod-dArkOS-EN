#include "ThemeGameBindings.h"
#include "FileData.h"
#include "SystemData.h"
#include "utils/StringUtil.h"
#include <sstream>

namespace ThemeGameBindings
{
	static std::string formatSecondsShort(int totalSeconds)
	{
		int hours = totalSeconds / 3600;
		int minutes = (totalSeconds % 3600) / 60;

		std::ostringstream ss;
		if (hours > 0)
			ss << hours << "h ";
		ss << minutes << "m";
		return ss.str();
	}

	static std::string getGameToken(const std::string& key, FileData* file)
	{
		if (file == nullptr)
			return "";

		if (key == "desc")
			return file->getMetadata().get("desc");
		if (key == "image")
			return file->getImagePath().empty() ? file->getThumbnailPath() : file->getImagePath();
		if (key == "marquee")
			return file->getMarqueePath();
		if (key == "thumbnail")
			return file->getThumbnailPath();
		if (key == "video")
			return file->getVideoPath();
		if (key == "playcount")
			return file->getMetadata().get("playcount");
		if (key == "gametime")
			return file->getMetadata().get("gametime");
		if (key == "releaseyear")
		{
			std::string date = file->getMetadata().get("releasedate");
			return date.size() >= 4 ? date.substr(0, 4) : "";
		}

		return "";
	}

	static std::string getSystemToken(const std::string& key, SystemData* system)
	{
		if (system == nullptr)
			return "";

		if (key == "total")
		{
			auto files = system->getRootFolder()->getFilesRecursive(GAME);
			return std::to_string((long long)files.size());
		}

		return "";
	}

	// Replaces every {game:x} / {system:x} token in the string with its resolved value.
	static std::string substituteTokens(const std::string& raw, FileData* file, SystemData* system)
	{
		std::string result = raw;
		size_t pos = 0;

		while ((pos = result.find('{', pos)) != std::string::npos)
		{
			size_t end = result.find('}', pos);
			if (end == std::string::npos)
				break;

			std::string token = result.substr(pos + 1, end - pos - 1);
			size_t colon = token.find(':');

			if (colon != std::string::npos)
			{
				std::string ns = token.substr(0, colon);
				std::string key = token.substr(colon + 1);
				std::string value;

				if (ns == "game")
					value = getGameToken(key, file);
				else if (ns == "system")
					value = getSystemToken(key, system);
				else
				{
					pos = end + 1;
					continue;
				}

				result = result.substr(0, pos) + value + result.substr(end + 1);
				pos += value.size();
				continue;
			}

			pos = end + 1;
		}

		return result;
	}

	// Handles "LEFT > 0 ? A : B" and "LEFT == N ? A : B", where LEFT is
	// numeric (post token-substitution) and A/B are quoted strings or
	// formatseconds(...)/expandseconds(...) calls wrapping a numeric token.
	static bool tryEvaluateTernary(const std::string& expr, std::string& out)
	{
		size_t qPos = expr.find('?');
		size_t cPos = expr.find(':');
		if (qPos == std::string::npos || cPos == std::string::npos || cPos < qPos)
			return false;

		std::string condition = Utils::String::trim(expr.substr(0, qPos));
		std::string thenPart = Utils::String::trim(expr.substr(qPos + 1, cPos - qPos - 1));
		std::string elsePart = Utils::String::trim(expr.substr(cPos + 1));

		bool condResult = false;

		size_t opPos;
		if ((opPos = condition.find(">")) != std::string::npos)
		{
			double left = atof(condition.substr(0, opPos).c_str());
			double right = atof(condition.substr(opPos + 1).c_str());
			condResult = left > right;
		}
		else if ((opPos = condition.find("==")) != std::string::npos)
		{
			double left = atof(condition.substr(0, opPos).c_str());
			double right = atof(condition.substr(opPos + 2).c_str());
			condResult = left == right;
		}
		else
		{
			return false;
		}

		std::string chosen = condResult ? thenPart : elsePart;

		// Strip surrounding quotes, or evaluate formatseconds(N)/expandseconds(N)
		if (chosen.size() >= 2 && chosen.front() == '"' && chosen.back() == '"')
		{
			out = chosen.substr(1, chosen.size() - 2);
			return true;
		}

		if (Utils::String::startsWith(chosen, "formatseconds(") || Utils::String::startsWith(chosen, "expandseconds("))
		{
			size_t open = chosen.find('(');
			size_t close = chosen.find(')');
			if (open != std::string::npos && close != std::string::npos && close > open)
			{
				int seconds = atoi(chosen.substr(open + 1, close - open - 1).c_str());
				out = formatSecondsShort(seconds);
				return true;
			}
		}

		out = chosen;
		return true;
	}

	std::string resolve(const std::string& raw, FileData* file, SystemData* system)
	{
		if (raw.find("{game:") == std::string::npos && raw.find("{system:") == std::string::npos)
			return raw;

		std::string substituted = substituteTokens(raw, file, system);

		std::string ternaryResult;
		if (tryEvaluateTernary(substituted, ternaryResult))
			return ternaryResult;

		return substituted;
	}
}