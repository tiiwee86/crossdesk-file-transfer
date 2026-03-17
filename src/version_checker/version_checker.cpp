/*
 * @Author: DI JUNKUN
 * @Date: 2025-11-11
 * Copyright (c) 2025 by DI JUNKUN, All Rights Reserved.
 */

#include "version_checker.h"

#include <httplib.h>

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace crossdesk {

static std::string latest_release_date_ = "";

std::string ExtractNumericPart(const std::string& ver) {
  size_t start = 0;
  while (start < ver.size() && !std::isdigit(ver[start])) start++;
  size_t end = start;
  while (end < ver.size() && (std::isdigit(ver[end]) || ver[end] == '.')) end++;
  return ver.substr(start, end - start);
}

std::vector<int> SplitVersion(const std::string& ver) {
  std::vector<int> nums;
  std::istringstream ss(ver);
  std::string token;
  while (std::getline(ss, token, '.')) {
    try {
      nums.push_back(std::stoi(token));
    } catch (...) {
      nums.push_back(0);
    }
  }
  return nums;
}

// extract date from version string (format: v1.2.3-20251113-abc
// or 1.2.3-20251113-abc)
std::string ExtractDateFromVersion(const std::string& version) {
  size_t dash1 = version.find('-');
  if (dash1 != std::string::npos) {
    size_t dash2 = version.find('-', dash1 + 1);
    if (dash2 != std::string::npos) {
      std::string date_part = version.substr(dash1 + 1, dash2 - dash1 - 1);

      bool is_date = true;
      for (char c : date_part) {
        if (!std::isdigit(c)) {
          is_date = false;
          break;
        }
      }
      if (is_date) {
        // convert YYYYMMDD to YYYY-MM-DD
        return date_part.substr(0, 4) + "-" + date_part.substr(4, 2) + "-" +
               date_part.substr(6, 2);
      }
    }
  }
  return "";
}

// compare two dates in YYYY-MM-DD format
bool IsNewerDate(const std::string& date1, const std::string& date2) {
  if (date1.empty() || date2.empty()) return false;
  // simple string comparison works for ISO date format (YYYY-MM-DD)
  return date2 > date1;
}

bool IsNewerVersion(const std::string& current, const std::string& latest) {
  auto v1 = SplitVersion(ExtractNumericPart(current));
  auto v2 = SplitVersion(ExtractNumericPart(latest));

  size_t len = std::max(v1.size(), v2.size());
  v1.resize(len, 0);
  v2.resize(len, 0);

  for (size_t i = 0; i < len; ++i) {
    if (v2[i] > v1[i]) return true;
    if (v2[i] < v1[i]) return false;
  }

  // if versions are equal, compare by release date
  if (!latest_release_date_.empty()) {
    // try to extract date from current version string
    std::string current_date = ExtractDateFromVersion(current);
    if (!current_date.empty()) {
      return IsNewerDate(current_date, latest_release_date_);
    } else {
      return true;
    }
  }

  return false;
}

bool IsNewerVersionWithDate(const std::string& current_version,
                            const std::string& current_date,
                            const std::string& latest_version,
                            const std::string& latest_date) {
  // compare versions
  auto v1 = SplitVersion(ExtractNumericPart(current_version));
  auto v2 = SplitVersion(ExtractNumericPart(latest_version));

  size_t len = std::max(v1.size(), v2.size());
  v1.resize(len, 0);
  v2.resize(len, 0);

  for (size_t i = 0; i < len; ++i) {
    if (v2[i] > v1[i]) return true;
    if (v2[i] < v1[i]) return false;
  }

  // if versions are equal, compare by release date
  if (!current_date.empty() && !latest_date.empty()) {
    return IsNewerDate(current_date, latest_date);
  }

  // if dates are not available, versions are equal
  return false;
}

nlohmann::json CheckUpdate() {
  httplib::Client cli("https://version.crossdesk.cn");

  cli.set_connection_timeout(5);
  cli.set_read_timeout(5);

  if (auto res = cli.Get("/version.json")) {
    if (res->status == 200) {
      try {
        auto j = nlohmann::json::parse(res->body);
        if (j.contains("releaseDate") && j["releaseDate"].is_string()) {
          latest_release_date_ = j["releaseDate"];
        } else {
          latest_release_date_ = "";
        }
        return j;
      } catch (std::exception&) {
        latest_release_date_ = "";
        return nlohmann::json{};
      }
    } else {
      latest_release_date_ = "";
      return nlohmann::json{};
    }
  } else {
    latest_release_date_ = "";
    return nlohmann::json{};
  }
}

}  // namespace crossdesk