#pragma once
#ifndef ES_CORE_UTILS_THEME_EXPR_H
#define ES_CORE_UTILS_THEME_EXPR_H

#include <map>
#include <string>

namespace Utils
{
	namespace ThemeExpr
	{
		// Evaluates a theme "if" expression (Batocera-compatible subset):
		//   {var} lookups, 'strings' / "strings", numbers, true/false,
		//   == != < > <= >=, && || !, parentheses.
		// Both operands numeric -> numeric compare, otherwise string compare.
		// ${var} placeholders must be resolved by the caller beforehand.
		// Throws std::runtime_error on a malformed expression.
		bool evaluate(const std::string& expr, const std::map<std::string, std::string>& vars);
	}
}

#endif // ES_CORE_UTILS_THEME_EXPR_H