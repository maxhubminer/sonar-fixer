#pragma once

#include <string>
#include <sstream>
#include <fstream>
#include <unordered_set>
#include <unordered_map>
#include <set>
#include <cassert>

#include <cpr/cpr.h>
#include <json/json.h>

#include "fixer.h"

// https://next.sonarqube.com/sonarqube/web_api/api/issues

class Sonar {
public:
    Sonar(std::string ip_address, std::string username, std::string password, std::string project_key)
        : url_(std::move(ip_address)), username_(std::move(username)),
        password_(std::move(password)), project_key_(std::move(project_key))
    {}

    const std::unordered_set<std::string>& GetComponents() {
        
        std::string full_url = GetUrlBase("/api/components/tree");

        int page_number = 1;
        bool data_exists = true;

        while (data_exists) {
            auto r = cpr::Get(cpr::Url{ full_url },
                cpr::Parameters{{"component", project_key_}, { "qualifiers", "FIL" },
                { "p", std::to_string(page_number) }, { "ps", std::to_string(page_size_) },
            });

            if (r.error.code != cpr::ErrorCode::OK) {
                std::cerr << "sonar request error: " << r.error.message << std::endl;
                assert(false);
            }
            if (r.status_line != "HTTP/1.1 200") {
                std::cerr << "sonar request error: " << r.text << std::endl;
                assert(false);
            }

            ++page_number;

            Json::Value root;
            std::stringstream ss(std::move(r.text));
            ss >> root;

            for (int index = 0; index < root["components"].size(); ++index) {
                components_.insert(root["components"][index]["path"].asString());
            }

            data_exists = root["components"].size();

        }

        return components_;
    }

    IssueSet GetIssues(std::string_view component_key) {

        IssueSet issues;
        
        std::string full_url = GetUrlBase("/api/issues/search");

        int page_number = 1;
        bool data_exists = true;

        while (data_exists) {

            std::string param_key = project_key_ + ":" + std::string(component_key);
            auto r = cpr::Get(cpr::Url{ full_url },
                cpr::Parameters{{"componentKeys", param_key}, { "rules", ":MissingSpace" },
                { "resolved", "false" }, { "statuses", "OPEN" },
                { "p", std::to_string(page_number) }, { "ps", std::to_string(page_size_) },                
            });

            if (r.error.code != cpr::ErrorCode::OK) {
                std::cerr << "sonar request error: " << r.error.message << std::endl;
                assert(false);
            }
            if (r.status_line != "HTTP/1.1 200") {
                std::cerr << "sonar request error: " << r.text << std::endl;
                assert(false);
            }

            ++page_number;

            Json::Value root;
            std::stringstream ss(std::move(r.text));
            ss >> root;
            int issues_total = root.get("total", 0).asUInt();

            for (int index = 0; index < root["issues"].size(); ++index) {

                const auto& text_range_json = root["issues"][index]["textRange"];
                issues.insert(Issue{ text_range_json["startLine"].asUInt(),
                                                            text_range_json["endLine"].asUInt(),
                                                            text_range_json["startOffset"].asUInt(),
                                                            text_range_json["endOffset"].asUInt(),
                                                            root["issues"][index]["message"].asString() });
            }

            data_exists = root["issues"].size();

        }

        return issues;

    }
    
private:
    std::string url_;
    std::string username_;
    std::string password_;
    std::string project_key_;
    const int page_size_ = 500; // max
    std::unordered_set<std::string> components_;

    std::string GetUrlBase(std::string url_path) {
        std::string result = "http://";
        result += username_ + ":" + password_ + "@";
        result += url_;
        result += std::move(url_path);

        return result;
    }
};