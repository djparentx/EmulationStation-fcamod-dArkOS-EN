#pragma once
#ifndef ES_CORE_UTILS_THEME_EXPR_H
#define ES_CORE_UTILS_THEME_EXPR_H

#include <map>
#include <string>

namespace Utils
{
	namespace ThemeExpr
	{
		// Batocera-compatible theme expression subset:
		//   {var} lookups (e.g. {game:image}, {screen.ratio}), 'strings' / "strings",
		//   numbers, true/false, == != < > <= >=, && || !, ( ), cond ? a : b,
		//   empty(x) exists(path) translate(s) formatseconds(n) expandseconds(n).
		// Both operands numeric -> numeric compare, otherwise string compare.
		// ${var} placeholders must be resolved by the caller beforehand.
		// Throw std::runtime_error on a malformed expression.
		bool        evaluate(const std::string& expr, const std::map<std::string, std::string>& vars);
		std::string evaluateToString(const std::string& expr, const std::map<std::string, std::string>& vars);
	}
}

#endif // ES_CORE_UTILS_THEME_EXPR_H