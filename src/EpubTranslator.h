#include <zip.h>
#include <fstream>
#include <cstring>
#include <filesystem>
#include <string>
#include <regex>
#include <vector>
#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>
#include <libxml/uri.h>
#include <libxml/xmlstring.h>
#include <libxml/encoding.h>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cstdlib>
#include <boost/process.hpp>
#include <boost/filesystem.hpp>
#include <sstream>
#include <iostream>
#include <curl/curl.h>
#include "Translator.h"
#include <nlohmann/json.hpp>
#include "llama.h"

#define P_TAG 0
#define IMG_TAG 1

// Struct declarations
struct tagData {
    int tagId;
    std::string text;
    int position;
    int chapterNum;
};


struct decodedData {
    std::string output;
    int chapterNum;
    int position;
};

class EpubTranslator : public Translator {
public:
    int run(const std::string& epubToConvert, const std::string& outputEpubPath, int localModel, const std::string& deepLKey);
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* output);

protected:
    std::filesystem::path searchForOPFFiles(const std::filesystem::path& directory);
    std::vector<std::string> getSpineOrder(const std::filesystem::path& directory);
    std::vector<std::filesystem::path> getAllXHTMLFiles(const std::filesystem::path& directory);
    std::vector<std::filesystem::path> sortXHTMLFilesBySpineOrder(const std::vector<std::filesystem::path>& xhtmlFiles, const std::vector<std::string>& spineOrder);
    void updateContentOpf(const std::vector<std::string>& epubChapterList, const std::filesystem::path& contentOpfPath);
    bool make_directory(const std::filesystem::path& path);
    bool unzip_file(const std::string& zipPath, const std::string& outputDir);
    void exportEpub(const std::string& exportPath, const std::string& outputDir);
    void updateNavXHTML(std::filesystem::path navXHTMLPath, const std::vector<std::string>& epubChapterList);
    void copyImages(const std::filesystem::path& sourceDir, const std::filesystem::path& destinationDir);
    void replaceFullWidthSpaces(xmlNodePtr node);
    void removeAngleBrackets(xmlNodePtr node);
    void removeUnwantedTags(xmlNodePtr node);
    void cleanChapter(const std::filesystem::path& chapterPath);
    std::string stripHtmlTags(const std::string& input);
    std::vector<tagData> extractTags(const std::vector<std::filesystem::path>& chapterPaths);
    std::string uploadDocumentToDeepL(const std::string& filePath, const std::string& deepLKey);
    std::string checkDocumentStatus(const std::string& document_id, const std::string& document_key, const std::string& deepLKey);
    std::string downloadTranslatedDocument(const std::string& document_id, const std::string& document_key, const std::string& deepLKey);
    int handleDeepLRequest(const std::vector<tagData>& bookTags, const std::vector<std::filesystem::path>& spineOrderXHTMLFiles, std::string deepLKey);


};
