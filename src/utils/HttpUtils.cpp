#include "include/HttpUtils.h"
#include "include/Process.h"

// All requests run curl (wget as a fallback) directly via Process::Run, with
// the URL as a separate argument: never through a shell, so URLs scraped from
// websites can't inject commands.
namespace HttpUtils {

namespace {
const char* kCaBundle = "/etc/ssl/certs/ca-certificates.crt";

bool fetchTo(const std::string& url, std::string* body, const std::string* outPath, int timeoutSecs) {
    std::vector<std::string> curl = {"curl", "--globoff", "--cacert", kCaBundle, "-fsSL"};
    if (timeoutSecs > 0) { curl.push_back("--max-time"); curl.push_back(std::to_string(timeoutSecs)); }
    if (outPath) { curl.push_back("-o"); curl.push_back(*outPath); }
    curl.push_back("--");
    curl.push_back(url);
    if (Process::Run(curl, body) == 0 && (outPath || !body->empty())) return true;

    // Fallback: wget (some firmwares ship an older curl without our CA bundle).
    if (body) body->clear();
    std::vector<std::string> wget = {"wget", "-q", "--timeout=" + std::to_string(timeoutSecs > 0 ? timeoutSecs : 60),
                                     "-O", outPath ? *outPath : "-", "--", url};
    return Process::Run(wget, body) == 0 && (outPath || !body->empty());
}
}  // namespace

std::string fetchWebContent(const std::string& url) {
    std::string body;
    fetchTo(url, &body, nullptr, 15);
    return body;
}

bool downloadImage(const std::string& url, const std::string& outputPath) {
    return fetchTo(url, nullptr, &outputPath, 0);
}

bool downloadFile(const std::string& url, const std::string& outputPath) {
    return downloadImage(url, outputPath);
}

bool hasInternet() {
    // generate_204 answers with an empty body: success = the tool exits cleanly.
    const std::string url = "https://www.google.com/generate_204";
    std::string ignored;
    if (Process::Run({"curl", "--globoff", "--cacert", kCaBundle, "-fsS", "--max-time", "15", "--", url}, &ignored) == 0) return true;
    return Process::Run({"wget", "-q", "--timeout=15", "-O", "-", "--", url}, &ignored) == 0;
}

}  // namespace HttpUtils
