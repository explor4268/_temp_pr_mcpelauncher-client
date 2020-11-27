#include "xal_webview_qt.h"
#include <EnvPathUtil.h>
#include <array>
#include <memory>
#include <stdexcept>
#include <game_window_manager.h>
#include "util.h"

std::string XalWebViewQt::findWebView() {
    std::string path;
#ifdef MCPELAUNCHER_WEBVIEW_PATH
    if (EnvPathUtil::findInPath("mcpelauncher-webview", path, MCPELAUNCHER_WEBVIEW_PATH, EnvPathUtil::getAppDir().c_str()))
        return path;
#endif
    if (EnvPathUtil::findInPath("mcpelauncher-webview", path))
        return path;
    return std::string();
}

std::string XalWebViewQt::exec_get_stdout(const char* command) {
    std::array<char, 128> buffer{};
    std::string result;
    std::shared_ptr<FILE> pipe(popen(command, "r"), pclose);
    if (!pipe) throw std::runtime_error("popen() failed!");
    while (!feof(pipe.get())) {
        if (fgets(buffer.data(), 128, pipe.get()) != nullptr)
            result += buffer.data();
    }
    return result;
}

std::string XalWebViewQt::show(std::string starturl, std::string endurlprefix) {
    auto webview_path = findWebView();
    if (webview_path.empty()) {
        throw std::runtime_error("mcpelauncher-webview not found");
    }
    auto result = exec_get_stdout(("\"" + webview_path + "\"" + " \"" + starturl + "\" \"" + endurlprefix + "\"").c_str());
    trim(result);
    if (result.rfind(endurlprefix, 0) != 0) {
        GameWindowManager::getManager()->getErrorHandler()->onError("XalWebViewQt", "Failed to open Xboxlive login Window. Please look into the gamelog for more Information");
    }
    return result;
}