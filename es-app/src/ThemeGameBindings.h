#pragma once
#ifndef ES_APP_THEME_GAME_BINDINGS_H
#define ES_APP_THEME_GAME_BINDINGS_H

#include <string>

class FileData;
class SystemData;

namespace ThemeGameBindings
{
	// Resolves {game:xxx} / {system:xxx} tokens and the small set of
	// ternary expressions this theme format uses (X > 0 ? A : B / X == N ? A : B).
	// Returns the input unchanged if it contains no bindings.
	std::string resolve(const std::string& raw, FileData* file, SystemData* system);
}

#endif // ES_APP_THEME_GAME_BINDINGS_H