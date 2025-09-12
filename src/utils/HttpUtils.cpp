#include "include/HttpUtils.h"

namespace HttpUtils {
    std::string fetchWebContent(const std::string& url) {
        std::string result;
        std::string command = "curl --globoff --cacert /etc/ssl/certs/ca-certificates.crt --max-time 15 -fsSL \"" + url + "\"";
        FILE* pipe = popen(command.c_str(), "r");
        if (pipe) {
            char buffer[128];
            while (!feof(pipe)) {
                if (fgets(buffer, 128, pipe) != nullptr) result += buffer;
            }
            int status = pclose(pipe);
        }
        if (result.empty()) {
            command = "wget --timeout=15 -q -O - \"" + url + "\"";
            pipe = popen(command.c_str(), "r");
            if (pipe) {
                char buffer[128];
                while (!feof(pipe)) {
                    if (fgets(buffer, 128, pipe) != nullptr) result += buffer;
                }
                int status = pclose(pipe);
            }
        }
        return result;
    }

    bool downloadImage(const std::string& url, const std::string& outputPath) {
        std::string command = "curl --globoff --cacert /etc/ssl/certs/ca-certificates.crt -fsSL -o \"" + outputPath + "\" \"" + url + "\"";
        int ret = system(command.c_str());
        if (ret != 0) {
            command = "wget --timeout=60 -q -O \"" + outputPath + "\" \"" + url + "\"";
            ret = system(command.c_str());
        }
        return ret == 0;
    }

    // Use downloadImage for downloadFile
    bool downloadFile(const std::string& url, const std::string& outputPath) {
        return downloadImage(url, outputPath);
    }

    bool hasInternet() {
        std::string url = "https://www.google.com/generate_204";
        std::string command = "curl --globoff --cacert /etc/ssl/certs/ca-certificates.crt --max-time 15 -fsSL \"" + url + "\"";
    // Reduced logging noise
        FILE* pipe = popen(command.c_str(), "r");
        std::string data;
        int curl_status = -1;
        if (pipe) {
            char buffer[128];
            while (!feof(pipe)) {
                if (fgets(buffer, 128, pipe) != nullptr) data += buffer;
            }
            curl_status = pclose(pipe);
            // Debug suppressed
        }
    // Suppress data size log
        if (curl_status == 0) {
            // Success, even if data is empty (204 response)
            return true;
        }
        // Fallback to wget
        std::string fallback = "wget --timeout=15 -q -O - \"" + url + "\"";
    // Suppress fallback log
        pipe = popen(fallback.c_str(), "r");
        int wget_status = -1;
        data.clear();
        if (pipe) {
            char buffer[128];
            while (!feof(pipe)) {
                if (fgets(buffer, 128, pipe) != nullptr) data += buffer;
            }
            wget_status = pclose(pipe);
            // Suppress wget exit status log
        }
    // Suppress data size after wget log
        return wget_status == 0;
    }
}