#pragma once
#include <string>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <string>

namespace HttpUtils {
    std::string fetchWebContent(const std::string& url);
    bool downloadImage(const std::string& url, const std::string& outputPath);
    bool downloadFile(const std::string& url, const std::string& outputPath); // Robust HTTPS download
    bool hasInternet();
}
