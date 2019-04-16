#pragma once

#include "deps/cpptoml/cpptoml.h"
#include <filesystem>
#include <regex>
#include <vector>
#include <map>

static // The main function that checks if two given strings 
// match. The first string may contain wildcard characters 
bool match_wildcard(const char* pattern, const char* test)
{
	// If we reach at the end of both strings, we are done 
	if (*pattern == '\0' && *test == '\0')
		return true;

	// Make sure that the characters after '*' are present 
	// in second string. This function assumes that the first 
	// string will not contain two consecutive '*' 
	if (*pattern == '*' && *(pattern + 1) != '\0' && *test == '\0')
		return false;

	// If the first string contains '?', or current characters 
	// of both strings match 
	if (*pattern == '?' || *pattern == *test)
		return match_wildcard(pattern + 1, test + 1);

	// If there is *, then there are two possibilities 
	// a) We consider current character of second string 
	// b) We ignore current character of second string. 
	if (*pattern == '*')
		return match_wildcard(pattern + 1, test) || match_wildcard(pattern, test + 1);
	return false;
}

class StreamElementsFileSystemMapper
{
private:
	typedef std::pair<std::string, std::string> redirect_t;

public:
	StreamElementsFileSystemMapper(std::string rootFolderPath) :
		m_rootFolderPath(rootFolderPath)
	{
		for (auto& p : std::experimental::filesystem::directory_iterator(rootFolderPath)) {
			std::wstring path = p.path();

			if (!std::experimental::filesystem::is_regular_file(path)) {
				continue;
			}

			if (path.size() > 5 && path.substr(path.size() - 5) == L".toml") {
				ParseTOMLFile(path);
			}
		}
	}

	bool QueryRedirectRuleReference(std::string relative, redirect_t& redirect_rule)
	{
		std::string relative2 = relative + "/";

		for (auto& rule : m_redirects) {
			std::string pattern = rule.first;

			bool is_match =
				match_wildcard(pattern.c_str(), relative.c_str()) ||
				match_wildcard(pattern.c_str(), relative2.c_str());

			if (is_match) {
				redirect_rule = rule;

				return true;
			}
		}

		return false;
	}

	std::string MapRelativePath(std::string relative)
	{
		redirect_t rule;
		if (!QueryRedirectRuleReference(relative, rule)) {
			return relative;
		}

		std::string match = rule.first;
		std::string value = rule.second;

		size_t pos = match.find('*');

		if (pos == std::string::npos) {
			return value;
		}

		std::string to = value;

		if (pos < relative.size()) {
			std::string splat = relative.substr(pos);
			to = std::regex_replace(
				to,
				std::regex(":splat"),
				splat);
		}

		return to;
	}

	bool MapAbsolutePath(std::string relative, std::string& absolute_path)
	{
		/////////////////////////////////////////////////////////////////////
		// The input string is safe.
		//
		// There is no need to handle '..' in this routine since those
		// are stripped from the URL by the CEF URL parser.
		/////////////////////////////////////////////////////////////////////

		std::string input = relative;
		std::string result = MapRelativePath(input);
		while (result != input) {
			input = result;
			result = MapRelativePath(input);
		}

		std::string path = m_rootFolderPath + result;

		if (std::experimental::filesystem::is_regular_file(path)) {
			absolute_path = path;

			return true;
		}
		else if (std::experimental::filesystem::is_regular_file(path + ".html")) {
			absolute_path = path + ".html";

			return true;
		}
		else if (std::experimental::filesystem::is_regular_file(path + "/index.html")) {
			absolute_path = path + "/index.html";

			return true;
		}

		absolute_path = path;

		return false;
	}

protected:
	void ParseTOML(std::shared_ptr<cpptoml::table> toml)
	{
		auto redirect_arr = toml->get_table_array("redirects");

		if (redirect_arr) {
			for (const auto& redirect : *redirect_arr) {
				std::string from = redirect->get_as<std::string>("from").value_or("");
				std::string to = redirect->get_as<std::string>("to").value_or("");

				if (from.size() && to.size()) {
					redirect_t rule;
					rule.first = from;
					rule.second = to;

					m_redirects.push_back(rule);
				}
			}
		}
	}

	void ParseTOMLFile(std::string tomlFilePath)
	{
		ParseTOML(cpptoml::parse_file(tomlFilePath));
	}

	void ParseTOMLFile(std::wstring tomlFilePath)
	{
		std::ifstream inputStream;

		inputStream.open(tomlFilePath, std::ifstream::in);
		cpptoml::parser parser(inputStream);

		ParseTOML(parser.parse());
	}

private:
	std::string m_rootFolderPath;
	std::vector<redirect_t> m_redirects;
};
