
#include <iostream>
#include <vector>
#include <string>
#include <unordered_set>
#include <set>
#include <filesystem>

#include "fixer.h"
#include "sonar.h"

using namespace std::string_literals;

int main(int argc, char* argv[]) {

	if (argc < 6) {
		std::cerr << "Please provide parameters in the following order:" << std::endl;
		std::cerr << "1. sonar ip address with port number, for ex., 192.168.0.1:123" << std::endl;
		std::cerr << "2. sonar username" << std::endl;
		std::cerr << "3. sonar user password" << std::endl;
		std::cerr << "4. sonar project key" << std::endl;
		std::cerr << "5. absolute path to folder with DB xml files (with forward slashes), for ex., D:/db/" << std::endl;
		
		return 1;
	}

	std::cout << "module_fixer running..." << std::endl;
	
	std::string db_xml_folder_path = argv[5];
	
	Sonar sonar(argv[1], argv[2], argv[3], argv[4]);

	std::vector<std::string> changed;

	int cur_comp = 0;
	const auto& components = sonar.GetComponents();
	for (const auto& component : components) {
		const auto issues = sonar.GetIssues(component);

		++cur_comp;
		if (!issues.size()) {
			//std::cout << "Skipping " << cur_comp << "/" << components.size() << std::endl;
			continue;
		}

		std::cout << "Fixing " << cur_comp << "/" << components.size() << std::endl;
		Fixer fixer(issues, component, db_xml_folder_path);
		fixer.Fix();

		std::filesystem::path component_path = db_xml_folder_path + component;
		changed.push_back(component_path.make_preferred().string());
	}

	std::ofstream changed_file("changed_components.txt");
	for (const auto& val : changed) {
		changed_file << val << "\n";
	}
	changed_file.close();

	std::cout << "DONE." << std::endl;

	return 0;
	
}