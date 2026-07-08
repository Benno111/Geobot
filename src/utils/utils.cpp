#include "utils.hpp"

#include <cstdlib>

namespace {
const char* kBlurVertShader = R"(
attribute vec4 a_position;
attribute vec2 a_texCoord;
attribute vec4 a_color;
varying vec2 v_texCoord;
varying vec4 v_fragmentColor;

void main() {
    gl_Position = CC_MVPMatrix * a_position;
    v_fragmentColor = a_color;
    v_texCoord = a_texCoord;
}
)";

const char* kBlurFragShader = R"(
#ifdef GL_ES
precision mediump float;
#endif

varying vec2 v_texCoord;
varying vec4 v_fragmentColor;
uniform sampler2D CC_Texture0;

void main() {
    vec2 px = vec2(1.0 / 512.0, 1.0 / 512.0);
    vec4 c = texture2D(CC_Texture0, v_texCoord) * 0.2;
    c += texture2D(CC_Texture0, v_texCoord + vec2(px.x, 0.0)) * 0.2;
    c += texture2D(CC_Texture0, v_texCoord - vec2(px.x, 0.0)) * 0.2;
    c += texture2D(CC_Texture0, v_texCoord + vec2(0.0, px.y)) * 0.2;
    c += texture2D(CC_Texture0, v_texCoord - vec2(0.0, px.y)) * 0.2;
    gl_FragColor = c * v_fragmentColor;
}
)";

void applyBlurRecursively(cocos2d::CCNode* node, cocos2d::CCGLProgram* shader) {
    if (!node || !shader) return;

    node->setShaderProgram(shader);

    auto children = node->getChildren();
    if (!children) return;

    for (unsigned int i = 0; i < children->count(); i++) {
        auto child = static_cast<cocos2d::CCNode*>(children->objectAtIndex(i));
        applyBlurRecursively(child, shader);
    }
}
}

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
    Utils::applyBackgroundBlur(bg);
}

void Utils::setBackgroundColor(geode::NineSlice* bg) {
    if (!bg) return;

    cocos2d::ccColor3B color = Mod::get()->getSettingValue<cocos2d::ccColor3B>("background_color");

    if (color == ccc3(51, 68, 153))
        color = ccc3(255, 255, 255);

    bg->setColor(color);
    Utils::applyBackgroundBlur(bg);
}

void Utils::applyBackgroundBlur(cocos2d::CCNode* bg) {
    if (!bg) return;

    static cocos2d::CCGLProgram* blurShader = nullptr;
    if (!blurShader) {
        auto* program = new cocos2d::CCGLProgram();
        if (!program) return;
        if (!program->initWithVertexShaderByteArray(kBlurVertShader, kBlurFragShader)) {
            program->release();
            return;
        }

        blurShader = program;
        blurShader->addAttribute(kCCAttributeNamePosition, cocos2d::kCCVertexAttrib_Position);
        blurShader->addAttribute(kCCAttributeNameColor, cocos2d::kCCVertexAttrib_Color);
        blurShader->addAttribute(kCCAttributeNameTexCoord, cocos2d::kCCVertexAttrib_TexCoords);
        blurShader->link();
        blurShader->updateUniforms();
        blurShader->retain();
    }

    applyBlurRecursively(bg, blurShader);
}
