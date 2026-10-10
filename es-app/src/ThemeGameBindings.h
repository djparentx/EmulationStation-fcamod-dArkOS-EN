#pragma once
#ifndef ES_APP_THEME_GAME_BINDINGS_H
#define ES_APP_THEME_GAME_BINDINGS_H

#include <string>

class FileData;
class SystemData;

namespace ThemeGameBindings
{
	// Resolves {game:xxx} / {system:xxx} tokens. If the string is a valid theme
	// expression (ternary, empty(), exists(), ...) it is evaluated; otherwise tokens
	// are substituted in place. Returns the input unchanged if it contains no bindings.
	std::string resolve(const std::string& raw, FileData* file, SystemData* system);

	// Evaluates a binding expression as a condition (e.g. <visible>!exists({game:video})</visible>).
	// Invalid expressions evaluate to true (element stays visible).
	bool evaluateCondition(const std::string& raw, FileData* file, SystemData* system);
}

#endif // ES_APP_THEME_GAME_BINDINGS_H