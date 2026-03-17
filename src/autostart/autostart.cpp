#include "autostart.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <limits.h>
#include <mach-o/dyld.h>
#include <unistd.h>
#elif defined(__linux__)
#include <linux/limits.h>
#include <unistd.h>
#endif

namespace crossdesk {

static std::string get_home_dir() {
  const char* home = std::getenv("HOME");
  if (!home) {
    return "";
  }
  return std::string(home);
}

static bool file_exists(const std::string& path) {
  return std::filesystem::exists(path) &&
         std::filesystem::is_regular_file(path);
}

static std::string GetExecutablePath() {
#ifdef _WIN32
  char path[32768];
  DWORD length = GetModuleFileNameA(nullptr, path, sizeof(path));
  if (length > 0 && length < sizeof(path)) {
    return std::string(path);
  }
#elif defined(__APPLE__)
  char path[1024];
  uint32_t size = sizeof(path);
  if (_NSGetExecutablePath(path, &size) == 0) {
    char resolved_path[PATH_MAX];
    if (realpath(path, resolved_path) != nullptr) {
      return std::string(resolved_path);
    }
    return std::string(path);
  }
#elif defined(__linux__)
  char path[PATH_MAX];
  ssize_t count = readlink("/proc/self/exe", path, sizeof(path) - 1);
  if (count != -1) {
    path[count] = '\0';
    return std::string(path);
  }
#endif
  return "";
}

// Windows
#ifdef _WIN32
static constexpr const char* WINDOWS_RUN_KEY =
    "Software\\Microsoft\\Windows\\CurrentVersion\\Run";

static bool windows_enable(const std::string& appName,
                           const std::string& exePath) {
  if (exePath.empty() || !std::filesystem::exists(exePath)) {
    return false;
  }

  HKEY hKey = nullptr;
  // Use KEY_WRITE to ensure we have write permission
  LONG result =
      RegOpenKeyExA(HKEY_CURRENT_USER, WINDOWS_RUN_KEY, 0, KEY_WRITE, &hKey);
  if (result != ERROR_SUCCESS) {
    return false;
  }

  std::string regValue = exePath;
  if (!exePath.empty() && exePath.find(' ') != std::string::npos) {
    if (exePath.front() != '"' || exePath.back() != '"') {
      regValue = "\"" + exePath + "\"";
    }
  }

  // Ensure we close the key even if RegSetValueExA fails
  result = RegSetValueExA(hKey, appName.c_str(), 0, REG_SZ,
                          reinterpret_cast<const BYTE*>(regValue.c_str()),
                          static_cast<DWORD>(regValue.size() + 1));
  RegCloseKey(hKey);

  return result == ERROR_SUCCESS;
}

static bool windows_disable(const std::string& appName) {
  HKEY hKey = nullptr;
  LONG result =
      RegOpenKeyExA(HKEY_CURRENT_USER, WINDOWS_RUN_KEY, 0, KEY_WRITE, &hKey);
  if (result != ERROR_SUCCESS) {
    return false;
  }

  result = RegDeleteValueA(hKey, appName.c_str());
  RegCloseKey(hKey);

  // Return true even if the value doesn't exist (already disabled)
  return result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND;
}

static bool windows_exists(const std::string& appName) {
  HKEY hKey = nullptr;
  LONG result =
      RegOpenKeyExA(HKEY_CURRENT_USER, WINDOWS_RUN_KEY, 0, KEY_READ, &hKey);
  if (result != ERROR_SUCCESS) {
    return false;
  }

  result = RegQueryValueExA(hKey, appName.c_str(), nullptr, nullptr, nullptr,
                            nullptr);
  RegCloseKey(hKey);

  return result == ERROR_SUCCESS;
}
#endif

// Linux
#if defined(__linux__)
static std::string linux_desktop_path(const std::string& appName) {
  std::string home = get_home_dir();
  if (home.empty()) {
    return "";
  }
  return home + "/.config/autostart/" + appName + ".desktop";
}

static bool linux_enable(const std::string& appName,
                         const std::string& exePath) {
  std::string home = get_home_dir();
  if (home.empty()) {
    return false;
  }

  std::filesystem::path dir =
      std::filesystem::path(home) / ".config" / "autostart";

  // Create directory if it doesn't exist
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  if (ec) {
    return false;
  }

  std::string path = linux_desktop_path(appName);
  if (path.empty()) {
    return false;
  }

  std::ofstream file(path);
  if (!file.is_open()) {
    return false;
  }

  file << "[Desktop Entry]\n";
  file << "Type=Application\n";
  file << "Exec=" << exePath << "\n";
  file << "Hidden=false\n";
  file << "NoDisplay=false\n";
  file << "X-GNOME-Autostart-enabled=true\n";
  file << "Terminal=false\n";
  file << "StartupNotify=false\n";
  file << "Name=" << appName << "\n";
  file.close();

  return file.good();
}

static bool linux_disable(const std::string& appName) {
  std::string path = linux_desktop_path(appName);
  if (path.empty()) {
    return false;
  }

  std::error_code ec;
  return std::filesystem::remove(path, ec) && !ec;
}

static bool linux_exists(const std::string& appName) {
  std::string path = linux_desktop_path(appName);
  if (path.empty()) {
    return false;
  }
  return file_exists(path);
}
#endif

// macOS
#ifdef __APPLE__
static std::string mac_plist_path(const std::string& appName) {
  std::string home = get_home_dir();
  if (home.empty()) {
    return "";
  }
  return home + "/Library/LaunchAgents/" + appName + ".plist";
}

static bool mac_enable(const std::string& appName, const std::string& exePath) {
  std::string path = mac_plist_path(appName);
  if (path.empty()) {
    return false;
  }

  // Ensure LaunchAgents directory exists
  std::filesystem::path dir =
      std::filesystem::path(get_home_dir()) / "Library" / "LaunchAgents";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  if (ec) {
    return false;
  }

  std::ofstream file(path);
  if (!file.is_open()) {
    return false;
  }

  file << R"(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
"http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>)"
       << appName << R"(</string>
    <key>ProgramArguments</key>
    <array>
        <string>)"
       << exePath << R"(</string>
    </array>
    <key>RunAtLoad</key>
    <true/>
</dict>
</plist>)";
  file.close();

  return file.good();
}

static bool mac_disable(const std::string& appName) {
  std::string path = mac_plist_path(appName);
  if (path.empty()) {
    return false;
  }

  std::error_code ec;
  return std::filesystem::remove(path, ec) && !ec;
}

static bool mac_exists(const std::string& appName) {
  std::string path = mac_plist_path(appName);
  if (path.empty()) {
    return false;
  }
  return file_exists(path);
}
#endif

bool EnableAutostart(const std::string& appName) {
  std::string exePath = GetExecutablePath();
  if (exePath.empty()) {
    return false;
  }
#ifdef _WIN32
  return windows_enable(appName, exePath);
#elif __APPLE__
  return mac_enable(appName, exePath);
#else
  return linux_enable(appName, exePath);
#endif
}

bool DisableAutostart(const std::string& appName) {
#ifdef _WIN32
  return windows_disable(appName);
#elif __APPLE__
  return mac_disable(appName);
#else
  return linux_disable(appName);
#endif
}

bool IsAutostartEnabled(const std::string& appName) {
#ifdef _WIN32
  return windows_exists(appName);
#elif __APPLE__
  return mac_exists(appName);
#else
  return linux_exists(appName);
#endif
}

}  // namespace crossdesk