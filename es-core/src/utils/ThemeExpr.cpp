#include "utils/ThemeExpr.h"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace Utils
{
	namespace ThemeExpr
	{
		struct Value
		{
			std::string str;
			bool        isNum = false;
			double      num = 0;
		};

		static Value makeValue(const std::string& s)
		{
			Value v;
			v.str = s;

			if (!s.empty())
			{
				char* end = nullptr;
				double d = strtod(s.c_str(), &end);
				if (end != nullptr && *end == '\0')
				{
					v.isNum = true;
					v.num = d;
				}
			}
			return v;
		}

		static Value makeBool(bool b)
		{
			Value v;
			v.str = b ? "true" : "false";
			v.isNum = true;
			v.num = b ? 1 : 0;
			return v;
		}

		static bool truthy(const Value& v)
		{
			if (v.isNum)
				return v.num != 0;
			return !v.str.empty() && v.str != "false";
		}

		class Parser
		{
		public:
			Parser(const std::string& expr, const std::map<std::string, std::string>& vars) : mExpr(expr), mVars(vars), mPos(0) { }

			Value parse()
			{
				Value v = parseOr();
				skipSpaces();
				if (mPos != mExpr.size())
					throw std::runtime_error("unexpected character at position " + std::to_string(mPos));
				return v;
			}

		private:
			const std::string& mExpr;
			const std::map<std::string, std::string>& mVars;
			size_t mPos;

			void skipSpaces()
			{
				while (mPos < mExpr.size() && isspace((unsigned char)mExpr[mPos]))
					mPos++;
			}

			bool match(const char* tok)
			{
				skipSpaces();
				size_t len = strlen(tok);
				if (mExpr.compare(mPos, len, tok) == 0)
				{
					mPos += len;
					return true;
				}
				return false;
			}

			Value parseOr()
			{
				Value left = parseAnd();
				while (match("||"))
				{
					Value right = parseAnd();
					left = makeBool(truthy(left) || truthy(right));
				}
				return left;
			}

			Value parseAnd()
			{
				Value left = parseCompare();
				while (match("&&"))
				{
					Value right = parseCompare();
					left = makeBool(truthy(left) && truthy(right));
				}
				return left;
			}

			Value parseCompare()
			{
				Value left = parseUnary();

				// longest operators first
				static const char* ops[] = { "==", "!=", "<=", ">=", "<", ">" };
				for (const char* op : ops)
				{
					if (!match(op))
						continue;

					Value right = parseUnary();
					std::string o(op);

					if (left.isNum && right.isNum)
					{
						if (o == "==") return makeBool(left.num == right.num);
						if (o == "!=") return makeBool(left.num != right.num);
						if (o == "<=") return makeBool(left.num <= right.num);
						if (o == ">=") return makeBool(left.num >= right.num);
						if (o == "<")  return makeBool(left.num <  right.num);
						return makeBool(left.num > right.num);
					}

					if (o == "==") return makeBool(left.str == right.str);
					if (o == "!=") return makeBool(left.str != right.str);
					if (o == "<=") return makeBool(left.str <= right.str);
					if (o == ">=") return makeBool(left.str >= right.str);
					if (o == "<")  return makeBool(left.str <  right.str);
					return makeBool(left.str > right.str);
				}

				return left;
			}

			Value parseUnary()
			{
				skipSpaces();
				// "!" but not "!="
				if (mPos < mExpr.size() && mExpr[mPos] == '!' && (mPos + 1 >= mExpr.size() || mExpr[mPos + 1] != '='))
				{
					mPos++;
					return makeBool(!truthy(parseUnary()));
				}
				return parsePrimary();
			}

			Value parsePrimary()
			{
				skipSpaces();
				if (mPos >= mExpr.size())
					throw std::runtime_error("unexpected end of expression");

				char c = mExpr[mPos];

				if (c == '(')
				{
					mPos++;
					Value v = parseOr();
					if (!match(")"))
						throw std::runtime_error("missing ')'");
					return v;
				}

				if (c == '\'' || c == '"')
				{
					size_t end = mExpr.find(c, mPos + 1);
					if (end == std::string::npos)
						throw std::runtime_error("unterminated string");
					std::string s = mExpr.substr(mPos + 1, end - mPos - 1);
					mPos = end + 1;
					return makeValue(s);
				}

				if (c == '{')
				{
					size_t end = mExpr.find('}', mPos + 1);
					if (end == std::string::npos)
						throw std::runtime_error("unterminated {variable}");
					std::string name = mExpr.substr(mPos + 1, end - mPos - 1);
					mPos = end + 1;
					auto it = mVars.find(name);
					return makeValue(it != mVars.cend() ? it->second : "");
				}

				// bare token: number, true/false, or unquoted word
				size_t start = mPos;
				while (mPos < mExpr.size() && (isalnum((unsigned char)mExpr[mPos]) || mExpr[mPos] == '.' || mExpr[mPos] == '-' || mExpr[mPos] == '_'))
					mPos++;

				if (start == mPos)
					throw std::runtime_error(std::string("unexpected character '") + c + "'");

				std::string tok = mExpr.substr(start, mPos - start);
				if (tok == "true")  return makeBool(true);
				if (tok == "false") return makeBool(false);
				return makeValue(tok);
			}
		};

		bool evaluate(const std::string& expr, const std::map<std::string, std::string>& vars)
		{
			Parser parser(expr, vars);
			return truthy(parser.parse());
		}
	}
}