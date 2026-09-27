#include "utils.hpp"

#include <cstdlib>

std::string Utils::narrow(const wchar_t* str) {
    if (!str) {
        return "";
    }

#ifdef GEODE_IS_WINDOWS
    int size = WideCharToMultiByte(CP_UTF8, 0, str, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return "";
    }

    auto buffer = new char[size];
    if (!buffer) {
        return "";
    }

    WideCharToMultiByte(CP_UTF8, 0, str, -1, buffer, size, nullptr, nullptr);
    std::string result(buffer, size_t(size) - 1);
    delete[] buffer;

    return result;

#else
    size_t size = std::wcstombs(nullptr, str, 0);
    if (size == static_cast<size_t>(-1)) {
        return "";
    }

    std::string result(size, '\0');
    if (size == 0) {
        return result;
    }

    std::vector<char> buffer(size + 1, '\0');
    if (std::wcstombs(buffer.data(), str, buffer.size()) == static_cast<size_t>(-1)) {
        return "";
    }

    result.assign(buffer.data(), size);
    return result;
#endif
}

std::wstring Utils::widen(const char* str) {
#ifdef GEODE_IS_WINDOWS

    if (str == nullptr) {
        return L"Widen Error";
    }

    int size = MultiByteToWideChar(CP_UTF8, 0, str, -1, nullptr, 0);
    if (size <= 0) {
        return L"Widen Error";
    }

    auto buffer = new wchar_t[size];
    if (!buffer) {
        return L"Widen Error";
    }

    if (MultiByteToWideChar(CP_UTF8, 0, str, -1, buffer, size) <= 0) {
        delete[] buffer;
        return L"Widen Error";
    }

    std::wstring result(buffer, size_t(size) - 1);
    delete[] buffer;
    return result;

#else
    if (!str) {
        return L"Widen Error";
    }

    size_t size = std::mbstowcs(nullptr, str, 0);
    if (size == static_cast<size_t>(-1)) {
        return L"Widen Error";
    }

    std::wstring result(size, L'\0');
    if (size == 0) {
        return result;
    }

    std::vector<wchar_t> buffer(size + 1, L'\0');
    if (std::mbstowcs(buffer.data(), str, buffer.size()) == static_cast<size_t>(-1)) {
        return L"Widen Error";
    }

    result.assign(buffer.data(), size);
    return result;

#endif
}

std::string Utils::toLower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    return str;
}

std::time_t Utils::getFileCreationTime(const std::filesystem::path& path) {
#ifdef GEODE_IS_WINDOWS
    HANDLE hFile = CreateFileW(
        path.wstring().c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        return 0;
    }

    FILETIME creationTime, lastAccessTime, lastWriteTime;
    if (!GetFileTime(hFile, &creationTime, &lastAccessTime, &lastWriteTime)) {
        CloseHandle(hFile);
        return 0;
    }

    CloseHandle(hFile);

    ULARGE_INTEGER ull;
    ull.LowPart = creationTime.dwLowDateTime;
    ull.HighPart = creationTime.dwHighDateTime;

    return ull.QuadPart / 10000000ULL - 11644473600ULL;
#endif
    (void)path;
    return 0;
}

std::string Utils::formatTime(std::time_t time) {
    std::tm tm = *std::localtime(&time);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

int Utils::copyFile(const std::string& sourcePath, const std::string& destinationPath) {
    std::ifstream source(sourcePath, std::ios::binary);
    std::ofstream destination(destinationPath, std::ios::binary);

    if (!source)
        return 1;

    if (!destination)
        return 2;

    destination << source.rdbuf();

    return 0;
}

std::vector<std::string> Utils::splitByChar(std::string str, char splitChar) {
    std::vector<std::string> strs;
    strs.reserve(std::count(str.begin(), str.end(), splitChar) + 1);

    size_t start = 0;
    size_t end = str.find(splitChar);
    while (end != std::string::npos) {
        strs.emplace_back(str.substr(start, end - start));
        start = end + 1;
        end = str.find(splitChar, start);
    }
    strs.emplace_back(str.substr(start));

    return strs;
}

std::string Utils::getTexture() {
    cocos2d::ccColor3B color = Mod::get()->getSettingValue<cocos2d::ccColor3B>("background_color");
    
	std::string texture = color == ccc3(51, 68, 153) ? "GJ_square02.png" : "GJ_square06.png";

    return texture;
}

std::string Utils::getSimplifiedString(std::string str) {
    if (str.find(".") == std::string::npos) return str;

    while(str.back() == '0') {
        str.pop_back();
        if (str.empty()) break;
    }

    if (!str.empty())
        if (str.back() == '.') str.pop_back();

    return str;
}

void Utils::setBackgroundColor(cocos2d::extension::CCScale9Sprite* bg) {
    if (!bg) return;

    cocos2d::ccColor3B color = Mod::get()->getSettingValue<cocos2d::ccColor3B>("background_color");

	if (color == ccc3(51, 68, 153))
		color = ccc3(255, 255, 255);

	bg->setColor(color);
}

void Utils::setBackgroundColor(geode::NineSlice* bg) {
    if (!bg) return;

    cocos2d::ccColor3B color = Mod::get()->getSettingValue<cocos2d::ccColor3B>("background_color");

    if (color == ccc3(51, 68, 153))
        color = ccc3(255, 255, 255);

    bg->setColor(color);
}
