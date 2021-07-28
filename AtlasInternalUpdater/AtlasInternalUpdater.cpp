#include <iostream>
#include <Windows.h>
#include <curl/curl.h>
#include <string>
#include <sstream>
#include <fstream>
#include <unzip.h>
#include "json.hpp"

constexpr const char* ATLAS_VERSION_FILE_NAME = "AtlasInternal.ver";

using json = nlohmann::json;

void replace(std::string& str, const std::string& from, const std::string& to) {
    size_t startPosition = str.find(from);
    if (startPosition == std::string::npos) return;
    str.replace(startPosition, from.length(), to);
}

std::string getExecutablePath() { // Location of the executable file. NOT the current working directory.
    char path[MAX_PATH];
    std::string stringPath = std::string(path, GetModuleFileNameA(0, path, sizeof(path)));
    replace(stringPath, "AtlasInternalUpdater.exe", "");
    return stringPath;
}

size_t writeFile(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    return fwrite(ptr, size, nmemb, stream);
}

size_t writeCallback(const char* ptr, size_t size, size_t nc, std::string* stp) {
    size_t initialLength = stp->length();
    size_t finalLength = initialLength;

    stp->append(ptr, nc);
    finalLength = stp->length();

    return (finalLength - initialLength);
}

size_t writeCallbackShim(char* ptr, size_t size, size_t nmemb, void* userdata) {
    size_t rc = 0;
    std::string* stp = (std::string*)userdata;

    rc = writeCallback(ptr, size, nmemb, stp);
    return rc;
}

int main() {
    std::string path = getExecutablePath();

    std::ifstream ifs(path + ATLAS_VERSION_FILE_NAME);
    std::string currentVersion = ifs.good() ? std::string((std::istreambuf_iterator<char>(ifs)),
        (std::istreambuf_iterator<char>())) : "Unknown";

    std::cout << "Current version: " << currentVersion << std::endl;

    if (!ifs.good()) { // Create new file if it doesnt exist.
        std::cout << "Creating version file..." << std::endl;

        std::ofstream newVersionFile(path + ATLAS_VERSION_FILE_NAME);
        newVersionFile << "v0.0";
        newVersionFile.close();
        std::cout << "Done." << std::endl;
    }

    ifs.close();

    // Latest version check.

    auto request = curl_easy_init();
    std::string result;

    curl_easy_setopt(request, CURLOPT_URL, "https://api.github.com/repos/OtherPr0jects/AtlasInternal/tags");
    curl_easy_setopt(request, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 6.1; Win64; x64; rv:47.0) Gecko/20100101 Firefox/47.0");
    curl_easy_setopt(request, CURLOPT_WRITEFUNCTION, writeCallbackShim);
    curl_easy_setopt(request, CURLOPT_WRITEDATA, &result);
    curl_easy_perform(request);

    json tags = json::parse(result);
    std::string latestVersion = tags[0]["name"];

    std::cout << "Latest version: " << latestVersion << std::endl;

    // Update check.

    if (latestVersion != currentVersion) { // Update found.
        std::cout << "Version mismatch. Starting update..." << std::endl;

        // Version file update.
        std::ofstream versionFile(path+ ATLAS_VERSION_FILE_NAME);
        versionFile.clear();
        versionFile << latestVersion;
        versionFile.close();

        // Installing zip file.
        std::stringstream fileDownloadLink;
        fileDownloadLink << "https://github.com/OtherPr0jects/AtlasInternal/releases/download/" << latestVersion << "/AtlasInternal.zip";
        
        FILE* zipFile = fopen((path + "AtlasInternal.zip").c_str(), "wb");

        curl_easy_setopt(request, CURLOPT_URL, fileDownloadLink.str());
        curl_easy_setopt(request, CURLOPT_FOLLOWLOCATION, true);
        curl_easy_setopt(request, CURLOPT_WRITEFUNCTION, writeFile);
        curl_easy_setopt(request, CURLOPT_WRITEDATA, zipFile);
        CURLcode res = curl_easy_perform(request);

        std::cout << (res == CURLE_OK ? "Success!" : "Failed, Error Code: " + res) << std::endl;
        
        curl_easy_cleanup(request);
        fclose(zipFile);

        // Unzipping zip File.

        HZIP openedZip = OpenZip((path + "AtlasInternal.zip").c_str(), 0);
        ZIPENTRY zipInfo; 
        GetZipItem(openedZip, -1, &zipInfo);
        int itemAmount = zipInfo.index;
        
        for (int i = 0; i < itemAmount; i++) {
            ZIPENTRY zipItem; 
            GetZipItem(openedZip, i, &zipItem);
            UnzipItem(openedZip, i, (path + zipItem.name).c_str());
        }

        CloseZip(openedZip);
        remove((path + "AtlasInternal.zip").c_str());

        std::cout << "Done." << std::endl;
    }

    std::cout << "Up to date! Enjoy Atlas Internal." << std::endl;

    system("pause");
}
