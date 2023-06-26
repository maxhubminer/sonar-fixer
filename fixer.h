#pragma once

#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <cassert>
#include <fstream>
#include <filesystem>
#include <execution>
#include <array>
#include <unordered_map>
#include <unordered_set>
#include <set>


std::wstring UTF8_to_wchar(const char* in)
{
	std::wstring out;
	unsigned int codepoint;
	while (*in != 0)
	{
		unsigned char ch = static_cast<unsigned char>(*in);
		if (ch <= 0x7f)
			codepoint = ch;
		else if (ch <= 0xbf)
			codepoint = (codepoint << 6) | (ch & 0x3f);
		else if (ch <= 0xdf)
			codepoint = ch & 0x1f;
		else if (ch <= 0xef)
			codepoint = ch & 0x0f;
		else
			codepoint = ch & 0x07;
		++in;
		if (((*in & 0xc0) != 0x80) && (codepoint <= 0x10ffff))
		{
			if (sizeof(wchar_t) > 2)
				out.append(1, static_cast<wchar_t>(codepoint));
			else if (codepoint > 0xffff)
			{
				codepoint -= 0x10000;
				out.append(1, static_cast<wchar_t>(0xd800 + (codepoint >> 10)));
				out.append(1, static_cast<wchar_t>(0xdc00 + (codepoint & 0x03ff)));
			}
			else if (codepoint < 0xd800 || codepoint >= 0xe000)
				out.append(1, static_cast<wchar_t>(codepoint));
		}
	}
	return out;
}

struct Issue {
	unsigned int start_line = 0, end_line = 0;
	unsigned int startOffset = 0, endOffset = 0;
	std::string desc;
};

struct IssueComparator {
	bool operator() (const Issue& a, const Issue& b) const {
		if (a.start_line == b.start_line) {
			return a.startOffset < b.startOffset;
		}
		return a.start_line < b.start_line;
	}
};

using IssueSet = std::set<Issue, IssueComparator>;

constexpr unsigned char CODE_POINT1 = 0b00000000;
constexpr unsigned char CODE_POINT2 = 0b11000000;
constexpr unsigned char CODE_POINT3 = 0b11100000;
constexpr unsigned char CODE_POINT4 = 0b11110000;
constexpr unsigned char CONTINUATION = 0b10000000;
constexpr unsigned char BOM1 = 0xEF;
constexpr unsigned char BOM2 = 0xBB;
constexpr unsigned char BOM3 = 0xBF;

class Fixer {
public:
	Fixer(const IssueSet& issues, std::string component_path, std::string db_folder)
		: issues_(issues), component_path_(std::move(component_path)), db_folder_(std::move(db_folder)) {}

	void Fix() {

		using namespace std::string_literals;

		std::wstring wide_path = UTF8_to_wchar((db_folder_ + component_path_).c_str());
		std::ifstream module_file(wide_path);

		std::string pathname_ = db_folder_ + component_path_ + "_"s;
		std::wstring wide_path_ = UTF8_to_wchar(pathname_.c_str());
		std::ofstream fixed_file(wide_path_);

		std::string check_pathname_ = db_folder_ + component_path_ + "check"s;
		std::wstring wide_check_path = UTF8_to_wchar(check_pathname_.c_str());
		std::ofstream check_file(wide_check_path);

		auto& in = module_file;
		auto& out = fixed_file;

		std::string line;
		auto it = line.begin();
		int char_index = 0;
		
		unsigned int line_number = 0;
		unsigned int prev_issue_line_number = 0;
		bool first_line = true;

		std::for_each(std::execution::seq, issues_.begin(), issues_.end(), [&](const auto& issue) {

			Check(issue);

			std::wstring wide_msg = UTF8_to_wchar(issue.desc.c_str());
			bool left_insert_required = (wide_msg.find(L"Слева") != std::string::npos || wide_msg.find(L"слева") != std::string::npos);
			bool right_insert_required = (wide_msg.find(L"Справа") != std::string::npos || wide_msg.find(L"справа") != std::string::npos);
			assert(left_insert_required || right_insert_required);

			bool new_line = (issue.start_line != prev_issue_line_number);
			prev_issue_line_number = issue.start_line;

			if (new_line) {
				char_index = 0;
			}
			
			while (true) {

				if (new_line) {

					for (; it != line.end(); ++it) {
						out << *it;
					}

					if (std::getline(in, line)) {
						++line_number;
						AddToCheckFile(check_file, line);
						it = line.begin();
					} else {
						break;
					}

					if (first_line) {
						first_line = false;
					}
					else {
						out << "\n";
					}
				}

				if (line_number == issue.start_line) {
					//std::cout << line.size() << std::endl;
					//std::cout << line.length() << std::endl;
					//std::cout << sizeof(line) << std::endl;
					
					// Output BOM:
					if (line.size() >= 3 &&
						line[0] == BOM1 && line[1] == BOM2 && line[2] == BOM3) {
						
						out << *it++;						
						out << *it++;						
						out << *it++;
					}

					for (; it != line.end(); ++it) {
						const unsigned char ch = *it;

						if (ch == '\t') { // tab as 1 char
							++char_index;
							out << ch;
							continue;
						}

						if ((ch >> 7) == (CODE_POINT1 >> 7)
							|| (ch >> 5) == (CODE_POINT2 >> 5)
							|| (ch >> 4) == (CODE_POINT3 >> 4)
							|| (ch >> 3) == (CODE_POINT4 >> 3)) {
							++char_index; // contains NEXT char number (not the amount of left behind)

							if (left_insert_required) {
								if (char_index == (issue.startOffset + 1)) {
									left_insert_required = false;
									out << " ";									
									
									if (!right_insert_required) {
										out << ch;
										++it;
										break;
									}
								}
							}
							// as characters may take several bytes - insert space after the end of sequence
							if (right_insert_required) {
								if (char_index == (issue.endOffset + 1)) {
									right_insert_required = false;
									out << " ";									
									out << ch; // don't lose
									++it;
									
									assert(!left_insert_required);
									break;
								}
							}
						}
						else {
							assert((ch >> 6) == (CONTINUATION >> 6));
						}

						out << ch;

					}

					break;

				}
				else {
					out << line; // just output the original line
					it = line.end();
				}
				
			} // end of while

		}); // end of for_each

		
		for (; it != line.end(); ++it) {
			out << *it;
		}

		// output all remaining original lines
		while (std::getline(in, line)) {
			AddToCheckFile(check_file, line);
			out << "\n" << line;
		}
		out << "\n";

		module_file.close();
		fixed_file.close();
		check_file.close();

		assert(IsFixCorrect(wide_path_, wide_check_path));

		std::filesystem::rename(wide_path_, wide_path);
			
	}

private:
	const IssueSet& issues_;
	const std::string component_path_;
	const std::string db_folder_;
	
	bool IsFixCorrect(const std::wstring& fixed_path, const std::wstring& check_path) {
		
		
		std::ifstream fixed_file(fixed_path);
		std::ifstream check_file(check_path);

		int64_t fixed_line_amount = std::count(std::istreambuf_iterator<char>(fixed_file),
			std::istreambuf_iterator<char>(), '\n');
		int64_t check_line_amount = std::count(std::istreambuf_iterator<char>(check_file),
			std::istreambuf_iterator<char>(), '\n');
		assert(fixed_line_amount == check_line_amount);

		std::string line;
		std::string checkline;

		while (std::getline(check_file, checkline)) {
			
			if (!std::getline(fixed_file, line)) {
				std::cerr << "line count is not equal" << std::endl;
					return false;
			}

			if (CountNonSpaces(line) != std::stoi(checkline)) return false;
		}

		return true;
	}
	
	void Check(const Issue& issue) {
		assert(issue.start_line == issue.end_line);
	}

	void AddToCheckFile(std::ofstream& check_file, const std::string& line) {
		check_file << CountNonSpaces(line) << "\n";
	}

	int CountNonSpaces(const std::string& line) {
		return std::count_if(line.begin(), line.end(), [](const unsigned char val) {
			return !(val == '\t' || val == ' ');
			});
	}
};