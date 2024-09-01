#include <iostream>
#include <zip.h>
#include <sys/stat.h>    // For Unix-like systems
#ifdef _WIN32
#include <direct.h>      // For Windows
#endif
#include <fstream>
#include <cstring>
#include <filesystem>
#include <string>
#include <regex>

std::filesystem::path searchForOPFFiles(const std::filesystem::path& directory) {
    try {
        // Iterate through the directory and its subdirectories
        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file() && entry.path().extension() == ".opf") {
                return entry.path();
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "General error: " << e.what() << std::endl;
    }

    return nullptr;
}

std::vector<std::string> getSpineOrder(const std::filesystem::path& directory) {
    std::vector<std::string> spineOrder;

    try {
        std::ifstream opfFile(directory);

        if (!opfFile.is_open()) {
            std::cerr << "Failed to open OPF file: " << directory << std::endl;
            return spineOrder;  // Return the empty vector
        }

        std::string content((std::istreambuf_iterator<char>(opfFile)), std::istreambuf_iterator<char>());
        opfFile.close();

        // Regex patterns to match <spine>, idref attributes, and </spine>
        std::regex spineStartPattern(R"(<spine\b[^>]*>)");
        std::regex spineEndPattern(R"(<\/spine>)");
        std::regex idrefPattern(R"(idref\s*=\s*"([^\"]*)\")");

        auto spineStartIt = std::sregex_iterator(content.begin(), content.end(), spineStartPattern);
        auto spineEndIt = std::sregex_iterator(content.begin(), content.end(), spineEndPattern);

        if (spineStartIt == spineEndIt) {
            std::cerr << "No <spine> tag found in the OPF file." << std::endl;
            return spineOrder;  // No <spine> tag found
        }

        // Search for idrefs between <spine> and </spine>
        auto spineEndPos = spineEndIt->position();
        auto contentUpToEnd = content.substr(0, spineEndPos);
        
        std::smatch idrefMatch;
        auto idrefStartIt = std::sregex_iterator(contentUpToEnd.begin(), contentUpToEnd.end(), idrefPattern);

        for (auto it = idrefStartIt; it != std::sregex_iterator(); ++it) {
            idrefMatch = *it;
            std::string idref = idrefMatch[1].str() + ".xhtml";  // Append .xhtml extension
            spineOrder.push_back(idref);
        }

    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "General error: " << e.what() << std::endl;
    }

    return spineOrder;  // Ensure we always return a vector
}


std::vector<std::filesystem::path> getAllXHTMLFiles(const std::filesystem::path& directory) {
    std::vector<std::filesystem::path> xhtmlFiles;

    try {
        // Iterate through the directory and its subdirectories
        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file() && entry.path().extension() == ".xhtml") {
                xhtmlFiles.push_back(entry.path());
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "General error: " << e.what() << std::endl;
    }

    return xhtmlFiles;
}

std::vector<std::filesystem::path> sortXHTMLFilesBySpineOrder(const std::vector<std::filesystem::path>& xhtmlFiles, const std::vector<std::string>& spineOrder) {
    std::vector<std::filesystem::path> sortedXHTMLFiles;

    for (const auto& idref : spineOrder) {
        for (const auto& xhtmlFile : xhtmlFiles) {
            if (xhtmlFile.filename() == idref) {
                sortedXHTMLFiles.push_back(xhtmlFile);
                break;
            }
        }
    }

    return sortedXHTMLFiles;
}

bool make_directory(const std::filesystem::path& path) {
    try {

        // Check if the directory already exists
        if (std::filesystem::exists(path)) {
            // std::cerr << "Directory already exists: " << path << std::endl;
            return true;
        }
        // Use create_directories to create the directory and any necessary parent directories
        return std::filesystem::create_directories(path);
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error creating directory: " << e.what() << std::endl;
        return false;
    }
}

// Function to unzip a file using libzip
bool unzip_file(const std::string& zipPath, const std::string& outputDir) {
    int err = 0;
    zip* archive = zip_open(zipPath.c_str(), ZIP_RDONLY, &err);
    if (archive == nullptr) {
        std::cerr << "Error opening ZIP archive: " << zipPath << std::endl;
        return false;
    }

    // Get the number of entries (files) in the archive
    zip_int64_t numEntries = zip_get_num_entries(archive, 0);
    for (zip_uint64_t i = 0; i < numEntries; ++i) {
        // Get the file name
        const char* name = zip_get_name(archive, i, 0);
        if (name == nullptr) {
            std::cerr << "Error getting file name in ZIP archive." << std::endl;
            zip_close(archive);
            return false;
        }

        // Create the full output path for this file
        std::filesystem::path outputPath = std::filesystem::path(outputDir) / name;

        // Check if the file is in a directory (ends with '/')
        if (outputPath.filename().string().back() == '/') {
            // Create directory
            make_directory(outputPath);
        } else {
            // Ensure the directory for the output file exists
            make_directory(outputPath.parent_path());

            // Open the file inside the ZIP archive
            zip_file* file = zip_fopen_index(archive, i, 0);
            if (file == nullptr) {
                std::cerr << "Error opening file in ZIP archive: " << name << std::endl;
                zip_close(archive);
                return false;
            }

            // Open the output file
            std::ofstream outputFile(outputPath, std::ios::binary);
            if (!outputFile.is_open()) {
                std::cerr << "Error creating output file: " << outputPath << std::endl;
                zip_fclose(file);
                zip_close(archive);
                return false;
            }

            // Read from the ZIP file and write to the output file
            char buffer[4096];
            zip_int64_t bytesRead;
            while ((bytesRead = zip_fread(file, buffer, sizeof(buffer))) > 0) {
                outputFile.write(buffer, bytesRead);
            }

            outputFile.close();

            // Close the current file in the ZIP archive
            zip_fclose(file);
        }
    }

    // Close the ZIP archive
    zip_close(archive);

    return true;
}

int main() {

    // std::string epubToConvert = "rawEpub/この素晴らしい世界に祝福を！ 01　あぁ、駄女神さま.epub";
    std::string epubToConvert = "rawEpub/Ascendance of a Bookworm Part 5 volume 11 『Premium Ver』.epub";
    std::string unzippedPath = "unzipped";

    std::string templatePath = "export";
    std::string templateEpub = "rawEpub/template.epub";

    // Create the output directory if it doesn't exist
    if (!make_directory(unzippedPath)) {
        std::cerr << "Failed to create output directory: " << unzippedPath << std::endl;
        return 1;
    }

    // Unzip the EPUB file
    if (!unzip_file(epubToConvert, unzippedPath)) {
        std::cerr << "Failed to unzip EPUB file: " << epubToConvert << std::endl;
        return 1;
    }

    std::cout << "EPUB file unzipped successfully to: " << unzippedPath << std::endl;

    // Create the template directory if it doesn't exist
    if (!make_directory(templatePath)) {
        std::cerr << "Failed to create template directory: " << templatePath << std::endl;
        return 1;
    }

    // Unzip the template EPUB file
    if (!unzip_file(templateEpub, templatePath)) {
        std::cerr << "Failed to unzip EPUB file: " << templateEpub << std::endl;
        return 1;
    }

    std::cout << "EPUB file unzipped successfully to: " << templatePath << std::endl;

    

    std::filesystem::path contentOpfPath = searchForOPFFiles(std::filesystem::path(unzippedPath));

    if (contentOpfPath.empty()) {
        std::cerr << "No OPF file found in the unzipped EPUB directory." << std::endl;
        return 1;
    }

    std::cout << "Found OPF file: " << contentOpfPath << std::endl;

    std::vector<std::string> spineOrder = getSpineOrder(contentOpfPath);

    if (spineOrder.empty()) {
        std::cerr << "No spine order found in the OPF file." << std::endl;
        return 1;
    }

    // Print the spine order
    for (const auto& idref : spineOrder) {
        std::cout << "Spine order: " << idref << std::endl;
    }

    std::vector<std::filesystem::path> xhtmlFiles = getAllXHTMLFiles(std::filesystem::path(unzippedPath));

    if (xhtmlFiles.empty()) {
        std::cerr << "No XHTML files found in the unzipped EPUB directory." << std::endl;
        return 1;
    }

    // Print the XHTML files
    for (const auto& xhtmlFile : xhtmlFiles) {
        std::cout << "XHTML file: " << xhtmlFile << std::endl;
    }

    // Sort the XHTML files based on the spine order
    std::vector<std::filesystem::path> spineOrderXHTMLFiles = sortXHTMLFilesBySpineOrder(xhtmlFiles, spineOrder);

    if (spineOrderXHTMLFiles.empty()) {
        std::cerr << "No XHTML files found in the unzipped EPUB directory matching the spine order." << std::endl;
        return 1;
    }

    // Print the sorted XHTML files
    for (const auto& xhtmlFile : spineOrderXHTMLFiles) {
        std::cout << "Sorted XHTML file: " << xhtmlFile << std::endl;
    }

    // Duplicate Section001.xhtml for each xhtml file in spineOrderXHTMLFiles and rename it
    std::filesystem::path Section001Path = std::filesystem::path("export/OEBPS/Text/Section0001.xhtml");
    std::ifstream Section001File(Section001Path);

    if (!Section001File.is_open()) {
        std::cerr << "Failed to open Section001.xhtml file: " << Section001Path << std::endl;
        return 1;
    }

    std::string Section001Content((std::istreambuf_iterator<char>(Section001File)), std::istreambuf_iterator<char>());
    Section001File.close();

    for (size_t i = 0; i < spineOrderXHTMLFiles.size(); ++i) {
        std::filesystem::path newSectionPath = std::filesystem::path("export/OEBPS/Text/" +  spineOrderXHTMLFiles[i].filename().string());
        std::ofstream newSectionFile(newSectionPath);

        if (!newSectionFile.is_open()) {
            std::cerr << "Failed to create new Section" << i + 1 << ".xhtml file." << std::endl;
            return 1;
        }

        newSectionFile << Section001Content;
        newSectionFile.close();
        //Print the new file path
        std::cout << "New file path: " << newSectionPath << std::endl;
    }


    // // Remove the unzipped directory
    // std::filesystem::remove_all(unzippedPath);
    // // Remove the template directory
    // std::filesystem::remove_all(templatePath);

    return 0;
}
