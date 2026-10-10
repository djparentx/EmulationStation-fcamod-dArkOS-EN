#include "ThemeGameBindings.h"
#include "FileData.h"
#include "SystemData.h"
#include "utils/StringUtil.h"
#include "utils/ThemeExpr.h"
#include "utils/TimeUtil.h"
#include <map>
#include <stdexcept>

namespace ThemeGameBindings
{
	// metadata value, with this fork's MetaData defaults ("unknown", "not-a-date-time", "0")
	// treated as empty so theme expressions like empty({game:publisher}) behave as in Batocera
	static std::string getMeta(FileData* file, const std::string& key)
	{
		std::string value = file->getMetadata().get(key);
		if (value == "unknown")
			return "";
		return value;
	}

	static std::string formatDate(const std::string& iso)
	{
		if (iso.empty() || iso == "not-a-date-time" || iso == "0")
			return "";

		time_t t = Utils::Time::stringToTime(iso);
		if (t <= 0)
			return "";

		return Utils::Time::timeToString(t, "%m/%d/%Y");
	}

	static std::string getGameToken(const std::string& key, FileData* file)
	{
		if (file == nullptr)
			return "";

		if (key == "name")
			return file->getName();
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
		if (key == "publisher" || key == "developer" || key == "genre" || key == "players" || key == "rating")
			return getMeta(file, key);
		if (key == "releasedate")
			return formatDate(file->getMetadata().get("releasedate"));
		if (key == "lastplayed")
			return formatDate(file->getMetadata().get("lastplayed"));
		if (key == "releaseyear")
		{
			std::string date = file->getMetadata().get("releasedate");
			return (date.size() >= 4 && date != "not-a-date-time") ? date.substr(0, 4) : "";
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

	static bool hasBindings(const std::string& raw)
	{
		return raw.find("{game:") != std::string::npos || raw.find("{system:") != std::string::npos;
	}

	// Finds every {game:x} / {system:x} token in the string and resolves only those
	// (avoids computing expensive tokens like {system:total} when unused).
	static std::map<std::string, std::string> collectVars(const std::string& raw, FileData* file, SystemData* system)
	{
		std::map<std::string, std::string> vars;
		size_t pos = 0;

		while ((pos = raw.find('{', pos)) != std::string::npos)
		{
			size_t end = raw.find('}', pos);
			if (end == std::string::npos)
				break;

			std::string token = raw.substr(pos + 1, end - pos - 1);
			size_t colon = token.find(':');

			if (colon != std::string::npos && vars.find(token) == vars.cend())
			{
				std::string ns = token.substr(0, colon);
				std::string key = token.substr(colon + 1);

				if (ns == "game")
					vars[token] = getGameToken(key, file);
				else if (ns == "system")
					vars[token] = getSystemToken(key, system);
			}

			pos = end + 1;
		}

		return vars;
	}

	// Plain in-place substitution, for strings that aren't expressions (e.g. "Players: {game:players}").
	static std::string substituteTokens(const std::string& raw, const std::map<std::string, std::string>& vars)
	{
		std::string result = raw;
		size_t pos = 0;

		while ((pos = result.find('{', pos)) != std::string::npos)
		{
			size_t end = result.find('}', pos);
			if (end == std::string::npos)
				break;

			auto it = vars.find(result.substr(pos + 1, end - pos - 1));
			if (it == vars.cend())
			{
				pos = end + 1;
				continue;
			}

			result = result.substr(0, pos) + it->second + result.substr(end + 1);
			pos += it->second.size();
		}

		return result;
	}

	std::string resolve(const std::string& raw, FileData* file, SystemData* system)
	{
		if (!hasBindings(raw))
			return raw;

		auto vars = collectVars(raw, file, system);

		try
		{
			return Utils::ThemeExpr::evaluateToString(raw, vars);
		}
		catch (const std::exception&)
		{
			return substituteTokens(raw, vars);
		}
	}

	bool evaluateCondition(const std::string& raw, FileData* file, SystemData* system)
	{
		auto vars = collectVars(raw, file, system);

		try
		{
			return Utils::ThemeExpr::evaluate(raw, vars);
		}
		catch (const std::exception&)
		{
			return true;
		}
	}
}