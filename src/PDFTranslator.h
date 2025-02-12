#pragma once

#include <vector>
#include <fstream>
#include <cstring>
#include <filesystem>
#include <regex>
#include <iostream>
#include <string>
#include <memory>
#include <boost/process.hpp>
#include <boost/filesystem.hpp>
#include <sstream>
#include <iostream>
#include <codecvt>
#include <locale>
#include "mupdf/fitz.h"
#include "mupdf/pdf.h"
#include <chrono>
#include <cairo.h>
#include <cairo-pdf.h>
#include "Translator.h"
#include <nlohmann/json.hpp>
#include <curl/curl.h>

class PDFTranslator : public Translator {
public:
    // Implement the run method from the Translator interface
    int run(const std::string& inputPath, const std::string& outputPath, int localModel, const std::string& deepLKey);
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* output);


protected:
    // Private helper methods
    std::string removeWhitespace(const std::string& input);
    void extractTextFromPDF(const std::string& pdfFilePath, const std::string& outputFilePath);
    std::vector<std::string> processAndSplitText(const std::string& inputFilePath, size_t maxLength);
    std::vector<std::string> splitLongSentences(const std::string& sentence, size_t maxLength = 300);
    std::vector<std::string> splitJapaneseText(const std::string& text, size_t maxLength = 300);
    size_t getUtf8CharLength(unsigned char firstByte);
    void convertPdfToImages(const std::string& pdfPath, const std::string& outputFolder, float stdDevThreshold);
    bool isImageAboveThreshold(const std::string& imagePath, float threshold);
    void createPDF(const std::string& output_file, const std::string& text, const std::string& images_dir);
    std::string uploadDocumentToDeepL(const std::string& filePath, const std::string& deepLKey);
    std::string checkDocumentStatus(const std::string& document_id, const std::string& document_key, const std::string& deepLKey);
    std::string downloadTranslatedDocument(const std::string& document_id, const std::string& document_key, const std::string& deepLKey);
    int handleDeepLRequest(const std::string& inputPath, const std::string& outputPath, const std::string& deepLKey);
    void processPDF(fz_context* ctx, const std::string& inputPath, std::ofstream& outputFile);
    fz_context* createMuPDFContext();
    void extractTextFromPage(fz_context* ctx, fz_document* doc, int pageIndex, std::ofstream& outputFile);
    bool pageContainsText(fz_stext_page* textPage);
    void extractTextFromBlocks(fz_stext_page* textPage, std::ofstream& outputFile);
    void extractTextFromLines(fz_stext_block* block, std::ofstream& outputFile);
    void extractTextFromChars(fz_stext_line* line, std::ofstream& outputFile);
};