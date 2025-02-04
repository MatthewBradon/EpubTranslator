#include "EpubTranslator.h"
#define MAX_TOKENS 256


std::filesystem::path EpubTranslator::searchForOPFFiles(const std::filesystem::path& directory) {
    try {
        // Iterate through the directory and its subdirectories
        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file() && entry.path().extension() == ".opf") {
                return entry.path();
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << "\n";
    } catch (const std::exception& e) {
        std::cerr << "General error: " << e.what() << "\n";
    }

    return std::filesystem::path();
}

std::vector<std::string> EpubTranslator::getSpineOrder(const std::filesystem::path& directory) {
    std::vector<std::string> spineOrder;

    try {
        std::ifstream opfFile(directory);

        if (!opfFile.is_open()) {
            std::cerr << "Failed to open OPF file: " << directory << "\n";
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
            std::cerr << "No <spine> tag found in the OPF file." << "\n";
            return spineOrder;  // No <spine> tag found
        }

        // Search for idrefs between <spine> and </spine>
        auto spineEndPos = spineEndIt->position();
        auto contentUpToEnd = content.substr(0, spineEndPos);
        
        std::smatch idrefMatch;
        auto idrefStartIt = std::sregex_iterator(contentUpToEnd.begin(), contentUpToEnd.end(), idrefPattern);

        for (auto it = idrefStartIt; it != std::sregex_iterator(); ++it) {
            idrefMatch = *it;
            // Check if it if it doesnt have .xhtml extension
            if (idrefMatch[1].str().find(".xhtml") == std::string::npos) {
                std::string idref = idrefMatch[1].str() + ".xhtml";  // Append .xhtml extension
                spineOrder.push_back(idref);
            } else {
                std::string idref = idrefMatch[1].str();
                spineOrder.push_back(idref);
            }
            
        }

    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << "\n";
    } catch (const std::exception& e) {
        std::cerr << "General error: " << e.what() << "\n";
    }

    return spineOrder;  // Ensure we always return a vector
}

std::vector<std::filesystem::path> EpubTranslator::getAllXHTMLFiles(const std::filesystem::path& directory) {
    std::vector<std::filesystem::path> xhtmlFiles;

    try {
        // Iterate through the directory and its subdirectories
        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file() && entry.path().extension() == ".xhtml") {
                xhtmlFiles.push_back(entry.path());
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << "\n";
    } catch (const std::exception& e) {
        std::cerr << "General error: " << e.what() << "\n";
    }

    return xhtmlFiles;
}

std::vector<std::filesystem::path> EpubTranslator::sortXHTMLFilesBySpineOrder(const std::vector<std::filesystem::path>& xhtmlFiles, const std::vector<std::string>& spineOrder) {
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

void EpubTranslator::updateContentOpf(const std::vector<std::string>& epubChapterList, const std::filesystem::path& contentOpfPath) {
    std::ifstream inputFile(contentOpfPath);
    if (!inputFile.is_open()) {
        std::cerr << "Failed to open content.opf file!" << "\n";
        return;
    }

    std::vector<std::string> fileContent;  // Store the entire file content
    std::vector<std::string> manifest;
    std::vector<std::string> spine;
    std::string line;
    bool insideManifest = false;
    bool insideSpine = false;
    bool packageClosed = false;

    while (std::getline(inputFile, line)) {
        if (line.find("</package>") != std::string::npos) {
            packageClosed = true;
        }

        if (line.find("<manifest>") != std::string::npos) {
            manifest.push_back(line);
            insideManifest = true;
        } else if (line.find("</manifest>") != std::string::npos) {
            insideManifest = false;
            for (size_t i = 0; i < epubChapterList.size(); ++i) {
                std::string chapterId = "chapter" + std::to_string(i + 1);
                manifest.push_back("    <item id=\"" + chapterId + "\" href=\"Text/" + epubChapterList[i] + "\" media-type=\"application/xhtml+xml\"/>\n");
            }
            manifest.push_back(line);
        } else if (line.find("<spine>") != std::string::npos) {
            spine.push_back(line);
            insideSpine = true;
        } else if (line.find("</spine>") != std::string::npos) {
            insideSpine = false;
            for (size_t i = 0; i < epubChapterList.size(); ++i) {
                std::string chapterId = "chapter" + std::to_string(i + 1);
                spine.push_back("    <itemref idref=\"" + chapterId + "\" />\n");
            }
            spine.push_back(line);
        } else if (insideManifest) {
            if (line.find("Section0001.xhtml") == std::string::npos) { // Skip Section0001.xhtml
                manifest.push_back(line);
            }
        } else if (insideSpine) {
            if (line.find("Section0001.xhtml") == std::string::npos) { // Skip Section0001.xhtml
                spine.push_back(line);
            }
        } else {
            fileContent.push_back(line);  // Add everything else to fileContent
        }
    }

    inputFile.close();

    // Write the updated content back to the file
    std::ofstream outputFile(contentOpfPath);
    if (!outputFile.is_open()) {
        std::cerr << "Failed to open content.opf file for writing!" << "\n";
        return;
    }

    // Output everything before </package>
    for (const auto& fileLine : fileContent) {
        if (fileLine.find("</metadata>") != std::string::npos) {
            outputFile << fileLine << "\n";
            // Write manifest after metadata
            for (const auto& manifestLine : manifest) {
                outputFile << manifestLine;
            }
            // Write spine after manifest
            for (const auto& spineLine : spine) {
                outputFile << spineLine;
            }
        } else if (fileLine.find("</package>") != std::string::npos) {
            // Skip writing </package> for now; it will be written later
            continue;
        } else {
            outputFile << fileLine << "\n";
        }
    }

    // Finally, close the package
    outputFile << "</package>" << "\n";

    outputFile.close();
}

bool EpubTranslator::make_directory(const std::filesystem::path& path) {
    try {

        // Check if the directory already exists
        if (std::filesystem::exists(path)) {
            // std::cerr << "Directory already exists: " << path << "\n";
            return true;
        }
        // Use create_directories to create the directory and any necessary parent directories
        return std::filesystem::create_directories(path);
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error creating directory: " << e.what() << "\n";
        return false;
    }
}

bool EpubTranslator::unzip_file(const std::string& zipPath, const std::string& outputDir) {
    int err = 0;
    zip* archive = zip_open(zipPath.c_str(), ZIP_RDONLY, &err);
    if (archive == nullptr) {
        std::cerr << "Error opening ZIP archive: " << zipPath << "\n";
        return false;
    }

    // Get the number of entries (files) in the archive
    zip_int64_t numEntries = zip_get_num_entries(archive, 0);
    for (zip_uint64_t i = 0; i < numEntries; ++i) {
        // Get the file name
        const char* name = zip_get_name(archive, i, 0);
        if (name == nullptr) {
            std::cerr << "Error getting file name in ZIP archive." << "\n";
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
                std::cerr << "Error opening file in ZIP archive: " << name << "\n";
                zip_close(archive);
                return false;
            }

            // Open the output file
            std::ofstream outputFile(outputPath, std::ios::binary);
            if (!outputFile.is_open()) {
                std::cerr << "Error creating output file: " << outputPath << "\n";
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

void EpubTranslator::exportEpub(const std::string& exportPath, const std::string& outputDir) {
    // Check if the exportPath directory exists
    if (!std::filesystem::exists(exportPath)) {
        std::cerr << "Export directory does not exist: " << exportPath << "\n";
        return;
    }

    // Create the output Epub file path
    std::string epubPath = outputDir + "/output.epub";

    // Create a ZIP archive
    zip_t* archive = zip_open(epubPath.c_str(), ZIP_CREATE | ZIP_TRUNCATE, nullptr);
    if (!archive) {
        std::cerr << "Error creating ZIP archive: " << epubPath << "\n";
        return;
    }

    // Add all files in the exportPath directory to the ZIP archive
    for (const auto& entry : std::filesystem::recursive_directory_iterator(exportPath)) {
        if (entry.is_regular_file()) {
            std::filesystem::path filePath = entry.path();
            std::filesystem::path relativePath = filePath.lexically_relative(exportPath);

            // Convert file paths to UTF-8 encoded strings
            std::string filePathUtf8 = filePath.u8string();
            std::string relativePathUtf8 = relativePath.u8string();

            // Create a zip_source_t from the file
            zip_source_t* source = zip_source_file(archive, filePathUtf8.c_str(), 0, 0);
            if (source == nullptr) {
                std::cerr << "Error creating zip_source_t for file: " << filePath << "\n";
                zip_discard(archive);
                return;
            }

            // Add the file to the ZIP archive
            zip_int64_t index = zip_file_add(archive, relativePathUtf8.c_str(), source, ZIP_FL_ENC_UTF_8);
            if (index < 0) {
                std::cerr << "Error adding file to ZIP archive: " << filePath << "\n";
                zip_source_free(source);
                zip_discard(archive);
                return;
            }
        }
    }

    // Close the ZIP archive
    if (zip_close(archive) < 0) {
        std::cerr << "Error closing ZIP archive: " << epubPath << "\n";
        return;
    }

    std::cout << "Epub file created: " << epubPath << "\n";
}

void EpubTranslator::updateNavXHTML(std::filesystem::path navXHTMLPath, const std::vector<std::string>& epubChapterList) {
    std::ifstream navXHTMLFile(navXHTMLPath);
    if (!navXHTMLFile.is_open()) {
        std::cerr << "Failed to open nav.xhtml file!" << "\n";
        return;
    }

    std::vector<std::string> fileContent;  // Store the entire file content
    std::string line;
    bool insideTocNav = false;
    bool insideOlTag = false;

    while (std::getline(navXHTMLFile, line)) {
        fileContent.push_back(line);

        if (line.find("<nav epub:type=\"toc\"") != std::string::npos) {
            insideTocNav = true;
        }

        if (line.find("<ol>") != std::string::npos && insideTocNav) {
            insideOlTag = true;
            // Append new chapter entries
            for (size_t i = 0; i < epubChapterList.size(); ++i) {
                std::string chapterFilename = std::filesystem::path(epubChapterList[i]).filename().string();
                fileContent.push_back("    <li><a href=\"" + chapterFilename + "\">Chapter " + std::to_string(i + 1) + "</a></li>");
            }
        }

        if (line.find("</ol>") != std::string::npos && insideTocNav) {
            insideOlTag = false;
        }

        if (line.find("</nav>") != std::string::npos && insideTocNav) {
            insideTocNav = false;
        }
    }

    navXHTMLFile.close();

    // Write the updated content back to the file
    std::ofstream outputFile(navXHTMLPath);
    if (!outputFile.is_open()) {
        std::cerr << "Failed to open nav.xhtml file for writing!" << "\n";
        return;
    }

    for (const auto& fileLine : fileContent) {
        outputFile << fileLine << "\n";
    }

    outputFile.close();
}

void EpubTranslator::copyImages(const std::filesystem::path& sourceDir, const std::filesystem::path& destinationDir) {
    try {
        // Create the destination directory if it doesn't exist
        if (!std::filesystem::exists(destinationDir)) {
            std::filesystem::create_directories(destinationDir);
        }

        // Traverse the source directory
        for (const auto& entry : std::filesystem::recursive_directory_iterator(sourceDir)) {
            if (entry.is_regular_file()) {
                std::string extension = entry.path().extension().string();
                if (extension == ".jpg" || extension == ".jpeg" || extension == ".png") {
                    std::filesystem::path destinationPath = destinationDir / entry.path().filename();
                    std::filesystem::copy(entry.path(), destinationPath, std::filesystem::copy_options::overwrite_existing);
                }
            }
        }
        std::cout << "Image files copied successfully." << "\n";
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << "\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
}

void EpubTranslator::replaceFullWidthSpaces(xmlNodePtr node) {
    if (node->type != XML_TEXT_NODE && node->content == nullptr) {
        return;
    }

    std::string textContent = reinterpret_cast<const char*>(node->content);
    std::string fullWidthSpace = "\xE3\x80\x80"; // UTF-8 encoding of \u3000
    std::string normalSpace = " ";

    size_t pos = 0;
    while ((pos = textContent.find(fullWidthSpace, pos)) != std::string::npos) {
        textContent.replace(pos, fullWidthSpace.length(), normalSpace);
        pos += normalSpace.length();
    }
    // Update the node's content with the modified text
    xmlNodeSetContent(node, reinterpret_cast<const xmlChar*>(textContent.c_str()));
}

void EpubTranslator::removeAngleBrackets(xmlNodePtr node) {
    if (node->type != XML_TEXT_NODE || node->content == nullptr) {
        return;
    }

    std::string textContent = reinterpret_cast<const char*>(node->content);
    std::string cleanedText;
    cleanedText.reserve(textContent.size());  // Reserve memory for performance

    // Iterate through the text once, appending characters except for `《` and `》`
    for (size_t i = 0; i < textContent.size(); ++i) {
        if (textContent[i] == '\xE3' && i + 2 < textContent.size() &&
            textContent[i + 1] == '\x80' &&
            (textContent[i + 2] == '\x8A' || textContent[i + 2] == '\x8B')) {
            // Skip both `《` and `》`, as their UTF-8 encoding is 3 bytes: \xE3\x80\x8A and \xE3\x80\x8B
            i += 2;
        } else {
            cleanedText += textContent[i];
        }
    }

    // Update the node's content with the cleaned text
    xmlNodeSetContent(node, reinterpret_cast<const xmlChar*>(cleanedText.c_str()));
}

void EpubTranslator::removeUnwantedTags(xmlNodePtr node) {
    xmlNodePtr current = node;
    while (current != nullptr) {
        if (current->type == XML_ELEMENT_NODE) {
            // Check if it's an unwanted tag
            if (xmlStrcmp(current->name, reinterpret_cast<const xmlChar*>("br")) == 0 ||
                xmlStrcmp(current->name, reinterpret_cast<const xmlChar*>("i")) == 0 || 
                xmlStrcmp(current->name, reinterpret_cast<const xmlChar*>("span")) == 0 || 
                xmlStrcmp(current->name, reinterpret_cast<const xmlChar*>("ruby")) == 0 ||
                xmlStrcmp(current->name, reinterpret_cast<const xmlChar*>("rt")) == 0) {
                
                // Move the content of the unwanted tag to its parent node before removing it
                xmlNodePtr child = current->children;
                while (child) {
                    xmlNodePtr nextChild = child->next;
                    xmlUnlinkNode(child);  // Unlink child from current node
                    xmlAddNextSibling(current, child);  // Insert it as a sibling to the unwanted tag
                    child = nextChild;
                }

                // Remove the unwanted node
                xmlNodePtr toRemove = current;
                current = current->next;  // Move to the next node before removal
                xmlUnlinkNode(toRemove);
                xmlFreeNode(toRemove);
                continue;  // Skip to the next node after removal
            }
        }

        // Replace full-width spaces in text nodes
        replaceFullWidthSpaces(current);
        removeAngleBrackets(current);

        // Recursively check child nodes
        if (current->children) {
            removeUnwantedTags(current->children);
        }
        current = current->next;
    }
}

void EpubTranslator::cleanChapter(const std::filesystem::path& chapterPath) {
    std::ifstream file(chapterPath);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << chapterPath << "\n";
        return;
    }

    // Read the entire content of the chapter file
    std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    // Parse the content using libxml2
    htmlDocPtr doc = htmlReadMemory(data.c_str(), data.size(), NULL, "UTF-8", HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
    if (!doc) {
        std::cerr << "Failed to parse HTML content." << "\n";
        return;
    }

    // Create an XPath context
    xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
    if (!xpathCtx) {
        std::cerr << "Failed to create XPath context." << "\n";
        xmlFreeDoc(doc);
        return;
    }

    // Extract all <p> and <img> tags
    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>("//p | //img"), xpathCtx);
    if (!xpathObj) {
        std::cerr << "Failed to evaluate XPath expression." << "\n";
        xmlXPathFreeContext(xpathCtx);
        xmlFreeDoc(doc);
        return;
    }

    xmlNodeSetPtr nodes = xpathObj->nodesetval;
    int nodeCount = (nodes) ? nodes->nodeNr : 0;

    // Remove unwanted tags and replace full-width spaces
    for (int i = 0; i < nodeCount; ++i) {
        xmlNodePtr node = nodes->nodeTab[i];
        removeUnwantedTags(node);
    }

    // Create a buffer to hold the serialized XML content
    xmlBufferPtr buffer = xmlBufferCreate();
    if (buffer == nullptr) {
        std::cerr << "Failed to create XML buffer." << "\n";
        xmlXPathFreeObject(xpathObj);
        xmlXPathFreeContext(xpathCtx);
        xmlFreeDoc(doc);
        return;
    }

    // Dump the document into the buffer
    int result = xmlNodeDump(buffer, doc, xmlDocGetRootElement(doc), 0, 1);
    if (result == -1) {
        std::cerr << "Failed to serialize XML document." << "\n";
        xmlBufferFree(buffer);
        xmlXPathFreeObject(xpathObj);
        xmlXPathFreeContext(xpathCtx);
        xmlFreeDoc(doc);
        return;
    }

    // Write the updated content back to the original file
    std::ofstream outFile(chapterPath);
    if (!outFile.is_open()) {
        std::cerr << "Failed to open file for writing: " << chapterPath << "\n";
        xmlBufferFree(buffer);
        xmlXPathFreeObject(xpathObj);
        xmlXPathFreeContext(xpathCtx);
        xmlFreeDoc(doc);
        return;
    }
    outFile.write(reinterpret_cast<const char*>(xmlBufferContent(buffer)), xmlBufferLength(buffer));
    outFile.close();

    // Clean up
    xmlBufferFree(buffer);
    xmlXPathFreeObject(xpathObj);
    xmlXPathFreeContext(xpathCtx);
    xmlFreeDoc(doc);
}

std::string EpubTranslator::stripHtmlTags(const std::string& input) {
    // Regular expression to match HTML tags
    std::regex tagRegex("<[^>]*>");
    // Replace all occurrences of HTML tags with an empty string
    return std::regex_replace(input, tagRegex, "");
}

std::vector<tagData> EpubTranslator::extractTags(const std::vector<std::filesystem::path>& chapterPaths) {
    // Helper function to read file content as UTF-8

    auto readFileUtf8 = [](const std::filesystem::path& filePath) -> std::string {
        std::ifstream file(filePath, std::ios::binary); // Open as binary
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file: " + filePath.string());
        }
        std::ostringstream buffer;
        buffer << file.rdbuf(); // Read the entire file
        file.close();

        return buffer.str(); // Return the UTF-8 string
    };

    std::vector<tagData> bookTags; // This will hold tag data for the entire book
    int chapterNum = 0;

    for (const std::filesystem::path& chapterPath : chapterPaths) {
        std::string data;

        try {
            data = readFileUtf8(chapterPath); // Read file as UTF-8
        } catch (const std::exception& e) {
            std::cerr << "Error reading file: " << e.what() << "\n";
            continue; // Continue with the next chapter
        }

        // Parse the content using libxml2
        htmlDocPtr doc = htmlReadMemory(data.c_str(), data.size(), NULL, "UTF-8", HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
        if (!doc) {
            std::cerr << "Failed to parse HTML content.\n";
            continue; // Continue with the next chapter
        }

        // Create an XPath context
        xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
        if (!xpathCtx) {
            std::cerr << "Failed to create XPath context.\n";
            xmlFreeDoc(doc);
            continue; // Continue with the next chapter
        }

        // Extract all <p> and <img> tags
        xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>("//p | //img"), xpathCtx);
        if (!xpathObj) {
            std::cerr << "Failed to evaluate XPath expression.\n";
            xmlXPathFreeContext(xpathCtx);
            xmlFreeDoc(doc);
            continue; // Continue with the next chapter
        }

        xmlNodeSetPtr nodes = xpathObj->nodesetval;
        int nodeCount = (nodes) ? nodes->nodeNr : 0;

        for (int i = 0; i < nodeCount; ++i) {
            xmlNodePtr node = nodes->nodeTab[i];
            if (node->type == XML_ELEMENT_NODE && xmlStrcmp(node->name, reinterpret_cast<const xmlChar*>("p")) == 0) {
                // Extract text content of <p> tag
                xmlChar* textContent = xmlNodeGetContent(node);
                if (textContent) {
                    try {
                        std::string pText(reinterpret_cast<char*>(textContent));

                        if (!pText.empty()) {
                            // Strip HTML tags and prepare the text
                            std::string strippedText = stripHtmlTags(pText);

                            // Create tagData struct
                            tagData tag;
                            tag.tagId = P_TAG;
                            tag.text = strippedText;
                            tag.position = i;
                            tag.chapterNum = chapterNum;

                            // Append the tag to bookTags
                            bookTags.push_back(tag);
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "Error: " << e.what() << "\n";
                    }
                    xmlFree(textContent);
                }
            } else if (node->type == XML_ELEMENT_NODE && xmlStrcmp(node->name, reinterpret_cast<const xmlChar*>("img")) == 0) {
                // Extract src attribute of <img> tag
                xmlChar* src = xmlGetProp(node, reinterpret_cast<const xmlChar*>("src"));
                if (src) {
                    std::string imgSrc(reinterpret_cast<char*>(src));
                    std::string imgFilename = std::filesystem::path(imgSrc).filename().string();

                    // Create tagData struct for the image
                    tagData tag;
                    tag.tagId = IMG_TAG;
                    tag.text = imgFilename;  // Store the image filename
                    tag.position = i;
                    tag.chapterNum = chapterNum;

                    // Append the tag to bookTags
                    bookTags.push_back(tag);

                    xmlFree(src);
                }
            }
        }

        // Free resources for the current chapter
        xmlXPathFreeObject(xpathObj);
        xmlXPathFreeContext(xpathCtx);
        xmlFreeDoc(doc);

        std::cout << "Extracted Tags for: " << chapterPath.filename().string() << "\n";
        chapterNum++;
    }

    return bookTags;
}


std::string formatHTML(const std::string& input) {


    htmlDocPtr doc = htmlReadMemory(input.c_str(), input.size(), NULL, "UTF-8", HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);

    if (doc == nullptr) {
        std::cerr << "Error: unable to parse HTML." << std::endl;
        return "";
    }

    xmlChar* output = nullptr;
    int size = 0;

    xmlDocDumpFormatMemory(doc, &output, &size, 1);

    std::string formattedHtml;
    if (output != nullptr) {
        formattedHtml.assign(reinterpret_cast<const char*>(output), size);
        xmlFree(output);
    }

    xmlFreeDoc(doc);

    return formattedHtml;
}

std::string escapeJsonString(const std::string& input) {
    std::string output;
    for (char c : input) {
        switch (c) {
            case '\"': output += "'"; break;  // Swap to single quotes
            case '\\': output += "\\\\"; break;  // Escape backslashes
            case '\n': output += "\\n"; break;   // Escape newlines
            case '\r': output += "\\r"; break;   // Escape carriage returns
            case '\t': output += "\\t"; break;   // Escape tabs
            default:
                output += c;
        }
    }
    return output;
}

std::string removeWhitespace(const std::string& input) {
    std::string output;
    for (char c : input) {
        if (c != '\n' && c != '\t' && c != '\r') {
            output += c;
        }
    }
    return output;
}

size_t EpubTranslator::writeCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t totalSize = size * nmemb;
    output->append((char*)contents, totalSize);
    return totalSize;
}


std::string EpubTranslator::uploadDocumentToDeepL(const std::string& filePath, const std::string& deepLKey) {
    CURL* curl;
    CURLcode res;
    std::string response_string;
    std::string api_url = "https://api-free.deepl.com/v2/document";
    std::string auth_key = "DeepL-Auth-Key " + deepLKey;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();

    if (curl) {
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, ("Authorization: " + auth_key).c_str());

        // Prepare the multipart form data
        curl_mime* form = curl_mime_init(curl);
        curl_mimepart* field = curl_mime_addpart(form);
        curl_mime_name(field, "target_lang");
        curl_mime_data(field, "EN", CURL_ZERO_TERMINATED); // Change to desired target language
        field = curl_mime_addpart(form);
        curl_mime_name(field, "file");
        curl_mime_filedata(field, filePath.c_str());  // Path to the file you want to upload

        curl_easy_setopt(curl, CURLOPT_URL, api_url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, form);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);

        // Perform the request
        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            std::cerr << "Curl upload request failed: " << curl_easy_strerror(res) << std::endl;
        }

        curl_mime_free(form);
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }
    curl_global_cleanup();

    // Assuming the response contains document_id and document_key, extract them
    try {
        std::cout << "Document upload response: " << response_string << std::endl;
        nlohmann::json jsonResponse = nlohmann::json::parse(response_string);
        std::string document_id = jsonResponse["document_id"];
        std::string document_key = jsonResponse["document_key"];
        return document_id + "|" + document_key;
    } catch (const nlohmann::json::exception& e) {
        std::cerr << "Error parsing document upload response: " << e.what() << std::endl;
        return "";
    }
}

std::string EpubTranslator::checkDocumentStatus(const std::string& document_id, const std::string& document_key, const std::string& deepLKey) {
    CURL* curl;
    CURLcode res;
    std::string response_string;
    std::string api_url = "https://api-free.deepl.com/v2/document/" + document_id;
    std::string auth_key = "DeepL-Auth-Key " + deepLKey;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();

    if (curl) {
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, ("Authorization: " + auth_key).c_str());
        headers = curl_slist_append(headers, "Content-Type: application/json");

        // Create JSON payload
        nlohmann::json jsonPayload;
        jsonPayload["document_key"] = document_key;
        std::string json_data = jsonPayload.dump();

        curl_easy_setopt(curl, CURLOPT_URL, api_url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POST, 1);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);

        // Perform the request
        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            std::cerr << "Curl status check request failed: " << curl_easy_strerror(res) << std::endl;
        }

        std::cout << "Document status response: " << response_string << std::endl;

        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }
    curl_global_cleanup();

    return response_string;  // You can parse this response to check if status == "done"
}

std::string EpubTranslator::downloadTranslatedDocument(const std::string& document_id, const std::string& document_key, const std::string& deepLKey) {
    CURL* curl;
    CURLcode res;
    std::string response_string;
    std::string api_url = "https://api-free.deepl.com/v2/document/" + document_id + "/result";
    std::string auth_key = "DeepL-Auth-Key " + deepLKey;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();

    if (curl) {
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, ("Authorization: " + auth_key).c_str());
        headers = curl_slist_append(headers, "Content-Type: application/json");

        // Create JSON payload
        nlohmann::json jsonPayload;
        jsonPayload["document_key"] = document_key;
        std::string json_data = jsonPayload.dump();

        curl_easy_setopt(curl, CURLOPT_URL, api_url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POST, 1);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);

        // Perform the request
        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            std::cerr << "Curl download request failed: " << curl_easy_strerror(res) << std::endl;
        }

        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }
    curl_global_cleanup();

    return response_string;
}


int EpubTranslator::handleDeepLRequest(const std::vector<tagData>& bookTags, const std::vector<std::filesystem::path>& spineOrderXHTMLFiles, std::string deepLKey) {
    
    std::vector<std::string> htmlStringVector;

    std::vector<std::vector<tagData>> chapterTags;
    // Divide bookTags into chapters chapterNum is the chapter number
    for(auto& tag : bookTags) {
        if (tag.chapterNum >= chapterTags.size()) {
            chapterTags.resize(tag.chapterNum + 1);
        }
        chapterTags[tag.chapterNum].push_back(tag);
    }

    // Build the position map for each chapter and position
    std::unordered_map<int, std::unordered_map<int, tagData*>> positionMap;

    for (size_t chapterNum = 0; chapterNum < chapterTags.size(); ++chapterNum) {
        for (auto& tag : chapterTags[chapterNum]) {
            positionMap[chapterNum][tag.position] = &tag;
        }
    }

    // Update chapterTags using bookTags
    for (const auto& tag : bookTags) {
        auto chapterIt = positionMap.find(tag.chapterNum);
        if (chapterIt != positionMap.end()) {
            auto& positionTags = chapterIt->second;
            auto tagIt = positionTags.find(tag.position);
            if (tagIt != positionTags.end()) {
                tagIt->second->text = tag.text;
            }
        }
    }

    // Write out to the template EPUB
    std::string htmlHeader = R"(
<!DOCTYPE html>
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
    <title>)";

    std::string htmlFooter = R"(</body>
</html>)";

    for (size_t i = 0; i < spineOrderXHTMLFiles.size(); ++i) {
        std::string htmlString;
        htmlString += htmlHeader + spineOrderXHTMLFiles[i].filename().string() + "</title>\n</head>\n<body>\n";
        std::cout << "Chapter: " << i << "\n";
        // Write content-specific parts
        for (const auto& tag : chapterTags[i]) {
            if (tag.tagId == P_TAG) {
                htmlString += "\t<p>" + tag.text + "</p>\n";
            } else if (tag.tagId == IMG_TAG) {
                htmlString += "\t<img src=\"../Images/" + tag.text + "\" alt=\"\"/>\n";
            }
        }

        // Append footer
        htmlString += htmlFooter;
        htmlStringVector.push_back(htmlString);
    }

    std::cout << "HTML strings generated successfully." << "\n";

    if (!make_directory("testHTML")) {
        std::cerr << "Failed to create testHTML directory." << "\n";
        return 1;
    }

    for (size_t i = 0; i < htmlStringVector.size(); ++i) {
        
        std::ofstream outFile("testHTML/" + std::to_string(i) + ".html");
        if (!outFile.is_open()) {
            std::cerr << "Failed to open file for writing: " << i << ".html" << "\n";
            return 1;
        }
        outFile << htmlStringVector[i];
        outFile.close();
    }

    std::vector<bool> htmlContainsPTagsVector(htmlStringVector.size(), false);

    for (size_t i = 0; i < htmlStringVector.size(); ++i) {
        if (htmlStringVector[i].find("<p>") != std::string::npos) {
            htmlContainsPTagsVector[i] = true;
            std::cout << "Chapter " << i << " contains <p> tags." << "\n";
        }
    }

    for (size_t i = 0; i < htmlStringVector.size(); ++i) {
        std::cout << "Inside for loop" << "\n";
        if (!htmlContainsPTagsVector[i]) {
            continue;
        }

        std::string chapterPath = "testHTML/" + std::to_string(i) + ".html";

        std::string uploadResult = uploadDocumentToDeepL(chapterPath, deepLKey);

        if (uploadResult.empty()) {
            std::cerr << "Failed to upload document to DeepL." << "\n";
            return 1;
        }

        std::string document_id = uploadResult.substr(0, uploadResult.find('|'));
        std::string document_key = uploadResult.substr(uploadResult.find('|') + 1);

        std::cout << "Uploaded document. Document ID: " << document_id << ", Document Key: " << document_key << "\n";

        std::string status;
        bool isTranslationComplete = false;

        while (!isTranslationComplete) {
            status = checkDocumentStatus(document_id, document_key, deepLKey);
            // Parse the JSON response to check if status == "done"
            nlohmann::json jsonResponse = nlohmann::json::parse(status);
            if (jsonResponse["status"] == "done") {
                isTranslationComplete = true;
            } else {
                std::cout << "Translation in progress..." << "\n";
                std::this_thread::sleep_for(std::chrono::seconds(5));  // Wait for 5 seconds before checking again
            }
        }

        std::string responseHTMLString = downloadTranslatedDocument(document_id, document_key, deepLKey);

        std::cout << responseHTMLString << "\n";

        htmlStringVector[i] = responseHTMLString;
        // Limit the number of translations for testing
        if(i == 9 ) {
            break;
        }
    }

    // Create testHTML directory if it doesn't exist
    if (!make_directory("translatedHTML")) {
        std::cerr << "Failed to create testHTML directory." << "\n";
        return 1;
    }

    // Write the updated content to the XHTML files in directory testHTML
    for (size_t i = 0; i < htmlStringVector.size(); ++i) {
        
        std::ofstream outFile("translatedHTML/" + std::to_string(i) + ".xhtml");
        if (!outFile.is_open()) {
            std::cerr << "Failed to open file for writing: " << i << ".xhtml" << "\n";
            return 1;
        }
        outFile << htmlStringVector[i];
        outFile.close();
    }

    for (size_t i = 0; i < spineOrderXHTMLFiles.size(); ++i) {
        // Copy the contents of each of the xhtml  files in the translatedHTML directory to the corresponding xhtml files in the export directory
        std::filesystem::path translatedFilePath = std::filesystem::path("translatedHTML/" + std::to_string(i) + ".xhtml");

        if (!std::filesystem::exists(translatedFilePath)) {
            std::cerr << "Translated file not found: " << translatedFilePath << "\n";
            return 1;
        }

        std::filesystem::path exportFilePath = std::filesystem::path("export/OEBPS/Text/" + spineOrderXHTMLFiles[i].filename().string());
        if (!std::filesystem::exists(exportFilePath)) {
            std::cerr << "Export file not found: " << exportFilePath << "\n";
            return 1;
        }
        std::filesystem::copy(translatedFilePath, exportFilePath, std::filesystem::copy_options::overwrite_existing);
        std::cout << "Translated file copied successfully: " << translatedFilePath << "\n";

    }

    return 0; 
}



std::string correctGrammar(llama_context *ctx, llama_model *model, const std::string &input) {
    std::string prompt = "Fix the grammar while keeping the original meaning:\n\n"
                         "Original: \"" + input + "\"\n"
                         "Corrected: ";

    // Tokenize input prompt
    const llama_vocab *vocab = llama_model_get_vocab(model);
    std::vector<llama_token> tokens(512);
    int n_tokens = llama_tokenize(vocab, prompt.c_str(), prompt.size(), tokens.data(), tokens.size(), true, true);

    if (n_tokens < 0) {
        std::cerr << "Tokenization failed!" << std::endl;
        return "";
    }
    tokens.resize(n_tokens);
    std::cout << "Tokenized " << n_tokens << " tokens.\n";

    // Create batch for initial input tokens
    llama_batch batch = llama_batch_get_one(tokens.data(), tokens.size());

    // Run decoding for initial tokens
    if (llama_decode(ctx, batch) != 0) {
        std::cerr << "LLama decoding failed!" << std::endl;
        return "";
    }

    // Initialize sampler (greedy)
    auto sparams = llama_sampler_chain_default_params();
    sparams.no_perf = false;
    llama_sampler *sampler = llama_sampler_chain_init(sparams);
    llama_sampler_chain_add(sampler, llama_sampler_init_greedy());

    std::string output;
    int max_tokens = 100;  // Adjust based on expected output length
    llama_token token;

    for (int i = 0; i < max_tokens; i++) {
        token = llama_sampler_sample(sampler, ctx, -1);
        
        if (llama_vocab_is_eog(vocab, token)) {
            break;  // Stop if EOS token is generated
        }

        char token_str[64];
        if (llama_token_to_piece(vocab, token, token_str, sizeof(token_str), 0, true) > 0) {
            output += token_str;
        }

        // Prepare batch for next token
        batch = llama_batch_get_one(&token, 1);

        if (llama_decode(ctx, batch) != 0) {
            std::cerr << "LLama decoding failed during generation!" << std::endl;
            break;
        }
    }

    // Cleanup
    llama_sampler_free(sampler);
    return output;
}




int EpubTranslator::run(const std::string& epubToConvert, const std::string& outputEpubPath, int localModel, const std::string& deepLKey) {
    const std::string MODEL_PATH = "llama-model/tinyllama-1.1b-chat-v1.0.Q5_K_M.gguf";

    // Initialize LLaMA backend
    llama_backend_init();

    // Set up model parameters
    llama_model_params model_params = llama_model_default_params();
    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = 512;  
    ctx_params.n_batch = 1;  
    ctx_params.n_threads = std::thread::hardware_concurrency();

    // Load model
    llama_model *model = llama_model_load_from_file(MODEL_PATH.c_str(), model_params);
    if (!model) {
        std::cerr << "Failed to load model from " << MODEL_PATH << std::endl;
        return 1;
    }

    // Create context
    llama_context *ctx = llama_init_from_model(model, ctx_params);
    if (!ctx) {
        std::cerr << "Failed to create LLama context!" << std::endl;
        llama_model_free(model);
        return 1;
    }

    // Open input and output files
    std::ifstream infile("translatedDeepL.txt");
    std::ofstream outfile("corrected_text.txt");

    if (!infile.is_open()) {
        std::cerr << "Failed to open input file: translatedDeepL.txt" << std::endl;
        llama_free(ctx);
        llama_model_free(model);
        return 1;
    }
    
    std::string sentenceLine;
    while (std::getline(infile, sentenceLine)) {
        if (!sentenceLine.empty() && sentenceLine.find_first_not_of(" \t\r\n") != std::string::npos) {
            std::string corrected = correctGrammar(ctx, model, sentenceLine);
            outfile << corrected << std::endl;
            std::cout << "Fixed: " << corrected << std::endl; 
        }
    }

    // Cleanup
    llama_free(ctx);
    llama_model_free(model);
    llama_backend_free();

    std::cout << "Grammar correction complete. Output saved to corrected_text.txt\n";
    return 0;



    std::cout << "localModel: " << localModel << "\n";

    std::filesystem::path currentDirPath = std::filesystem::current_path();

    std::cout << "Running the EPUB conversion process..." << "\n";
    std::cout << "epubToConvert: " << epubToConvert << "\n";
    std::cout << "outputEpubPath: " << outputEpubPath << "\n";

    std::string unzippedPath = "unzipped";
    std::string templatePath = "export";
    std::string templateEpub = "rawEpub/template.epub";

    // Check if the unzipped directory already exists
    if (std::filesystem::exists(unzippedPath)) {
        std::cout << "Unzipped directory already exists. Deleting it..." << "\n";
        std::filesystem::remove_all(unzippedPath);
    }

    // Check if the export directory already exists
    if (std::filesystem::exists(templatePath)) {
        std::cout << "Export directory already exists. Deleting it..." << "\n";
        std::filesystem::remove_all(templatePath);
    }


    // Start the timer
    auto start = std::chrono::high_resolution_clock::now();
    std::cout << "START" << "\n";

    // Create the output directory if it doesn't exist
    if (!make_directory(unzippedPath)) {
        std::cerr << "Failed to create output directory: " << unzippedPath << "\n";
        return 1;
    }

    // Unzip the EPUB file
    if (!unzip_file(epubToConvert, unzippedPath)) {
        std::cerr << "Failed to unzip EPUB file: " << epubToConvert << "\n";
        return 1;
    }

    std::cout << "EPUB file unzipped successfully to: " << unzippedPath << "\n";

    // Create the template directory if it doesn't exist
    if (!make_directory(templatePath)) {
        std::cerr << "Failed to create template directory: " << templatePath << "\n";
        return 1;
    }

    // Unzip the template EPUB file
    if (!unzip_file(templateEpub, templatePath)) {
        std::cerr << "Failed to unzip EPUB file: " << templateEpub << "\n";
        return 1;
    }

    std::cout << "EPUB file unzipped successfully to: " << templatePath << "\n";

    
    std::filesystem::path contentOpfPath = searchForOPFFiles(std::filesystem::path(unzippedPath));

    if (contentOpfPath.empty()) {
        std::cerr << "No OPF file found in the unzipped EPUB directory." << "\n";
        return 1;
    }

    std::cout << "Found OPF file: " << contentOpfPath << "\n";

    std::vector<std::string> spineOrder = getSpineOrder(contentOpfPath);

    if (spineOrder.empty()) {
        std::cerr << "No spine order found in the OPF file." << "\n";
        return 1;
    }


    std::vector<std::filesystem::path> xhtmlFiles = getAllXHTMLFiles(std::filesystem::path(unzippedPath));
    if (xhtmlFiles.empty()) {
        std::cerr << "No XHTML files found in the unzipped EPUB directory." << "\n";
        return 1;
    }


    // Sort the XHTML files based on the spine order
    std::vector<std::filesystem::path> spineOrderXHTMLFiles = sortXHTMLFilesBySpineOrder(xhtmlFiles, spineOrder);
    if (spineOrderXHTMLFiles.empty()) {
        std::cerr << "No XHTML files found in the unzipped EPUB directory matching the spine order." << "\n";
        return 1;
    }


    // Duplicate Section001.xhtml for each xhtml file in spineOrderXHTMLFiles and rename it
    std::filesystem::path Section001Path = std::filesystem::path("export/OEBPS/Text/Section0001.xhtml");
    std::ifstream Section001File(Section001Path);
    if (!Section001File.is_open()) {
        std::cerr << "Failed to open Section001.xhtml file: " << Section001Path << "\n";
        return 1;
    }

    std::string Section001Content((std::istreambuf_iterator<char>(Section001File)), std::istreambuf_iterator<char>());
    Section001File.close();

    for (size_t i = 0; i < spineOrderXHTMLFiles.size(); ++i) {
        std::filesystem::path newSectionPath = std::filesystem::path("export/OEBPS/Text/" +  spineOrderXHTMLFiles[i].filename().string());
        std::ofstream newSectionFile(newSectionPath);
        if (!newSectionFile.is_open()) {
            std::cerr << "Failed to create new Section" << i + 1 << ".xhtml file." << "\n";
            return 1;
        }
        newSectionFile << Section001Content;
        newSectionFile.close();
    }
    //Remove Section001.xhtml
    std::filesystem::remove(Section001Path);

    // Update the spine and manifest in the templates OPF file
    std::filesystem::path templateContentOpfPath = "export/OEBPS/content.opf";
    updateContentOpf(spineOrder, templateContentOpfPath);

    // Update the nav.xhtml file
    std::filesystem::path navXHTMLPath = "export/OEBPS/Text/nav.xhtml";
    updateNavXHTML(navXHTMLPath, spineOrder);


    // Copy images from the unzipped directory to the template directory
    copyImages(std::filesystem::path(unzippedPath), std::filesystem::path("export/OEBPS/Images"));




    // Clean each chapter
    for(const auto& xhtmlFile : spineOrderXHTMLFiles) {
            cleanChapter(xhtmlFile);
            std::cout << "Chapter cleaned: " << xhtmlFile.string() << "\n";
    }

    //Extract all of the relevant tags
    std::vector<tagData> bookTags = extractTags(spineOrderXHTMLFiles);

    if(bookTags.empty()) {
        std::cerr << "No tags extracted from the book." << "\n";
        return 1;
    }

    if (localModel == 1){
        if(deepLKey.empty()) {
            std::cerr << "No DeepL API key provided." << "\n";
            return 1;
        }

        int result = handleDeepLRequest(bookTags, spineOrderXHTMLFiles, deepLKey);

        if (result != 0) {
            std::cerr << "Failed to handle DeepL request." << "\n";
            return 1;
        }



        exportEpub(templatePath, outputEpubPath);
        
        // // Remove the unzipped and export directories
        // std::filesystem::remove_all(unzippedPath);
        // std::filesystem::remove_all(templatePath);

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        std::cout << "Time taken: " << elapsed.count() << "s" << "\n";

        return 0;
    }



    std::string rawTagsPathString = "rawTags.txt";
    std::string encodedTagsPathString = "encodedTags.txt";
    std::string translatedTagsPathString = "translatedTags.txt";

    // Write out a file of the raw tags
    std::filesystem::path rawTagsPath = rawTagsPathString;
    std::ofstream rawTagsFile(rawTagsPath);
    if(!rawTagsFile.is_open()) {
        std::cerr << "Failed to open file for writing: " << rawTagsPath << "\n";
        return 1;
    }

    for(auto& tag : bookTags) {
        rawTagsFile <<  tag.tagId << "," << tag.chapterNum << "," << tag.position << "," << tag.text << "\n";
    }
    rawTagsFile.close();
    

    std::cout << "Before call to tokenizeRawTags.exe" << '\n';
    boost::filesystem::path exePath;
    #if defined(__APPLE__)
        exePath = "tokenizeRawTags";

    #elif defined(_WIN32)
        exePath = "tokenizeRawTags.exe";
    #else
        std::cerr << "Unsupported platform!" << std::endl;
        return 1; // Or some other error handling
    #endif
    boost::filesystem::path inputFilePath = "rawTags.txt";  // Path to the input file
    std::string chapterNumberMode = "0";
    // Ensure the .exe exists
    if (!boost::filesystem::exists(exePath)) {
        std::cerr << "Executable not found: " << exePath << std::endl;
        return 1;
    }

    // Ensure the input file exists
    if (!boost::filesystem::exists(inputFilePath)) {
        std::cerr << "Input file not found: " << inputFilePath << std::endl;
        return 1;
    }

    // Create pipes for capturing stdout and stderr
    boost::process::ipstream encodeTagspipe_stdout;
    boost::process::ipstream encodeTagspipe_stderr;

    try {
        // Start the .exe process with arguments
        boost::process::child c(
            exePath.string(),                 // Executable path
            inputFilePath.string(),           // Argument: path to input file
            chapterNumberMode,                // Argument: chapter number mode
            boost::process::std_out > encodeTagspipe_stdout,        // Redirect stdout
            boost::process::std_err > encodeTagspipe_stderr         // Redirect stderr
        );

        // Read stdout
        std::string line;
        while (c.running() && std::getline(encodeTagspipe_stdout, line)) {
            std::cout << line << std::endl;
        }

        // Read any remaining stdout
        while (std::getline(encodeTagspipe_stdout, line)) {
            std::cout << line << std::endl;
        }

        // Read stderr
        while (std::getline(encodeTagspipe_stderr, line)) {
            std::cerr << "Error: " << line << std::endl;
        }

        // Wait for the process to exit
        c.wait();

        // Check the exit code
        if (c.exit_code() == 0) {
            std::cout << "tokenizeRawTags.exe executed successfully." << std::endl;
        } else {
            std::cerr << "tokenizeRawTags.exe exited with code: " << c.exit_code() << std::endl;
        }
    } catch (const std::exception& ex) {
        std::cerr << "Exception: " << ex.what() << std::endl;
        return 1;
    }

    std::cout << "After call to tokenizeRawTags.exe" << '\n';


    //Start the multiprocessing translaton
    std::filesystem::path multiprocessExe;

    #if defined(__APPLE__)
        multiprocessExe = currentDirPath / "multiprocessTranslation";

    #elif defined(_WIN32)
        multiprocessExe = currentDirPath / "multiprocessTranslation.exe";
    #else
        std::cerr << "Unsupported platform!" << std::endl;
        return 1; // Or some other error handling
    #endif

    if (!std::filesystem::exists(multiprocessExe)) {
        std::cerr << "Executable not found: " << multiprocessExe << std::endl;
        return 1;
    }

    std::string multiprocessExePath = multiprocessExe.string();
    

    std::cout << "Before call to multiprocessTranslation.py" << '\n';

    boost::process::ipstream pipe_stdout, pipe_stderr;

    try {

        boost::process::child c(
            multiprocessExePath,
            chapterNumberMode,
            boost::process::std_out > pipe_stdout, 
            boost::process::std_err > pipe_stderr
        );

        // Threads to handle asynchronous reading
        std::thread stdout_thread([&pipe_stdout]() {
            std::string line;
            while (std::getline(pipe_stdout, line)) {
                std::cout << "Python stdout: " << line << "\n";
            }
        });

        std::thread stderr_thread([&pipe_stderr]() {
            std::string line;
            while (std::getline(pipe_stderr, line)) {
                std::cerr << "Python stderr: " << line << "\n";
            }
        });

        c.wait(); // Wait for process completion

        stdout_thread.join();
        stderr_thread.join();

        if (c.exit_code() == 0) {
            std::cout << "Translation Python script executed successfully." << "\n";
        } else {
            std::cerr << "Translation Python script exited with code: " << c.exit_code() << "\n";
        }
    } catch (const std::exception& ex) {
        std::cerr << "Exception: " << ex.what() << "\n";
    }

    std::cout << "After call to multiprocessTranslation.py" << '\n';

    std::vector<decodedData> decodedDataVector;
    std::ifstream file(translatedTagsPathString);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << translatedTagsPathString << "\n";
        return 1;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream lineStream(line);
        std::string chapterNumStr, positionStr, text;

        // Extract chapterNum (first value)
        if (!std::getline(lineStream, chapterNumStr, ',')) continue;

        // Extract position (second value)
        if (!std::getline(lineStream, positionStr, ',')) continue;

        // The rest of the line is text
        std::getline(lineStream, text);

        // Convert chapterNum and position to integers
        try {
            decodedData data;
            data.chapterNum = std::stoi(chapterNumStr);
            data.position = std::stoi(positionStr);
            data.output = text;
            // Store the extracted data
            decodedDataVector.push_back(data);
        } catch (const std::exception& e) {
            std::cerr << "Error parsing line: " << line << " - " << e.what() << "\n";
        }
    }
    file.close();




    std::vector<std::vector<tagData>> chapterTags;
    // Divide bookTags into chapters chapterNum is the chapter number
    for(auto& tag : bookTags) {
        if (tag.chapterNum >= chapterTags.size()) {
            chapterTags.resize(tag.chapterNum + 1);
        }
        chapterTags[tag.chapterNum].push_back(tag);
    }

    // Build the position map for each chapter and position
    std::unordered_map<int, std::unordered_map<int, tagData*>> positionMap;

    for (size_t chapterNum = 0; chapterNum < chapterTags.size(); ++chapterNum) {
        for (auto& tag : chapterTags[chapterNum]) {
            positionMap[chapterNum][tag.position] = &tag;
        }
    }

    // Update chapterTags using decodedDataVector
    for (const auto& decoded : decodedDataVector) {
        // Find the corresponding chapter in the map
        auto chapterIt = positionMap.find(decoded.chapterNum);
        if (chapterIt != positionMap.end()) {
            auto& positionTags = chapterIt->second;
            // Find the tag by position
            auto tagIt = positionTags.find(decoded.position);
            if (tagIt != positionTags.end()) {
                // Update the text of the corresponding tag
                tagIt->second->text = decoded.output;
                // std::cout << "Decoded text: " << decoded.output << "\n";
            }
        }
    }

    //Write out to the template EPUB
    std::string htmlHeader = R"(<?xml version="1.0" encoding="UTF-8"?>
    <!DOCTYPE html>
    <html xmlns="http://www.w3.org/1999/xhtml">
    <head>
    <title>)";

    std::string htmlFooter = R"(</body>
    </html>)";

    for (size_t i = 0; i < spineOrderXHTMLFiles.size(); ++i) {
        std::filesystem::path outputPath = "export/OEBPS/Text/" + spineOrderXHTMLFiles[i].filename().string();
        std::ofstream outFile(outputPath);
        std::cout << "Writing to: " << outputPath << "\n";
        if (!outFile.is_open()) {
            std::cerr << "Failed to open file for writing: " << outputPath << "\n";
            return 1;
        }

        // Write pre-built header
        outFile << htmlHeader << spineOrderXHTMLFiles[i].filename().string() << "</title>\n</head>\n<body>\n";

        // Write content-specific parts
        for (const auto& tag : chapterTags[i]) {
            if (tag.tagId == P_TAG) {
                outFile << "<p>" << tag.text << "</p>\n";
            } else if (tag.tagId == IMG_TAG) {
                outFile << "<img src=\"../Images/" << tag.text << "\" alt=\"\"/>\n";
            }
        }

        // Write pre-built footer
        outFile << htmlFooter;
        outFile.close();
    }

    // Zip export directory to create the final EPUB file
    exportEpub(templatePath, outputEpubPath);

    // // Remove the unzipped and export directories
    // std::filesystem::remove_all(unzippedPath);
    // std::filesystem::remove_all(templatePath);


    // Remove the temp text files
    try {
        if (std::filesystem::exists(encodedTagsPathString)) {
            std::filesystem::remove(encodedTagsPathString);
            std::cout << "Deleted file: " << encodedTagsPathString << "\n";
        }
        if (std::filesystem::exists(translatedTagsPathString)) {
            std::filesystem::remove(translatedTagsPathString);
            std::cout << "Deleted file: " << translatedTagsPathString << "\n";
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << "\n";
    }

    // End timer
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Time taken: " << elapsed.count() << "s" << "\n";

    return 0;
}
