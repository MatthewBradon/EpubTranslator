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
#include <onnxruntime_cxx_api.h>
#include <Python.h>
#include <pybind11/embed.h>
#include <pybind11/numpy.h>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "GUI.h"

#define PYBIND11_DETAILED_ERROR_MESSAGES
#define P_TAG 0
#define IMG_TAG 1

// Struct declarations
struct tagData {
    int tagId;
    std::string text;
    int position;
    int chapterNum;
};

struct encodedData {
    pybind11::object encoded;
    int chapterNum;
    int position;
};

struct decodedData {
    std::string output;
    int chapterNum;
    int position;
};

// Function declarations
std::filesystem::path searchForOPFFiles(const std::filesystem::path& directory);
std::vector<std::string> getSpineOrder(const std::filesystem::path& directory);
std::vector<std::filesystem::path> getAllXHTMLFiles(const std::filesystem::path& directory);
std::vector<std::filesystem::path> sortXHTMLFilesBySpineOrder(const std::vector<std::filesystem::path>& xhtmlFiles, const std::vector<std::string>& spineOrder);
void updateContentOpf(const std::vector<std::string>& epubChapterList, const std::filesystem::path& contentOpfPath);
bool make_directory(const std::filesystem::path& path);
bool unzip_file(const std::string& zipPath, const std::string& outputDir);
void exportEpub(const std::string& exportPath);
void updateNavXHTML(std::filesystem::path navXHTMLPath, const std::vector<std::string>& epubChapterList);
void copyImages(const std::filesystem::path& sourceDir, const std::filesystem::path& destinationDir);
void replaceFullWidthSpaces(xmlNodePtr node);
void removeAngleBrackets(xmlNodePtr node);
void removeUnwantedTags(xmlNodePtr node);
void cleanChapter(const std::filesystem::path& chapterPath);
std::string stripHtmlTags(const std::string& input);
std::vector<tagData> extractTags(const std::vector<std::filesystem::path>& chapterPaths);
void convertEncodedDataToPython(const std::vector<encodedData>& data_vector, pybind11::module& EncodeDecode);
pybind11::object runMultiprocessingPython(pybind11::module& EncodeDecode);
std::vector<decodedData> convertPythonResultsToDecodedData(pybind11::object& results, pybind11::module& EncodeDecode);
int run(const std::string& epubToConvert, const std::string& outputEpubPath);
