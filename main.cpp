#include "main.h"

std::filesystem::path searchForOPFFiles(const std::filesystem::path& directory) {
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

std::vector<std::string> getSpineOrder(const std::filesystem::path& directory) {
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
        std::cerr << "Filesystem error: " << e.what() << "\n";
    } catch (const std::exception& e) {
        std::cerr << "General error: " << e.what() << "\n";
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

void updateContentOpf(const std::vector<std::string>& epubChapterList, const std::filesystem::path& contentOpfPath) {
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

bool make_directory(const std::filesystem::path& path) {
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

// Function to unzip a file using libzip
bool unzip_file(const std::string& zipPath, const std::string& outputDir) {
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

void exportEpub(const std::string& exportPath, const std::string& outputDir) {
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

void updateNavXHTML(std::filesystem::path navXHTMLPath, const std::vector<std::string>& epubChapterList) {
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

void copyImages(const std::filesystem::path& sourceDir, const std::filesystem::path& destinationDir) {
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

void replaceFullWidthSpaces(xmlNodePtr node) {
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

void removeAngleBrackets(xmlNodePtr node) {
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

void removeUnwantedTags(xmlNodePtr node) {
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

void cleanChapter(const std::filesystem::path& chapterPath) {
    std::ifstream file(chapterPath);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << chapterPath << "\n";
        return;
    }

    // Read the entire content of the chapter file
    std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    // Parse the content using libxml2
    htmlDocPtr doc = htmlReadMemory(data.c_str(), data.size(), NULL, NULL, HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
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

std::string stripHtmlTags(const std::string& input) {
    // Regular expression to match HTML tags
    std::regex tagRegex("<[^>]*>");
    // Replace all occurrences of HTML tags with an empty string
    return std::regex_replace(input, tagRegex, "");
}

std::vector<tagData> extractTags(const std::vector<std::filesystem::path>& chapterPaths) {
    // Initialize the 2D vector to store the tag data for each chapter
    std::vector<tagData> bookTags;  // This will hold tag data for the entire book
    
    int chapterNum = 0;

    for (const std::filesystem::path& chapterPath : chapterPaths) {
        
        std::ifstream file(chapterPath);
        if (!file.is_open()) {
            std::cerr << "Failed to open file: " << chapterPath << "\n";
            continue;  // Continue with the next chapter instead of returning
        }

        // Read the entire content of the chapter file
        std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        // Parse the content using libxml2
        htmlDocPtr doc = htmlReadMemory(data.c_str(), data.size(), NULL, NULL, HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
        if (!doc) {
            std::cerr << "Failed to parse HTML content." << "\n";
            continue;  // Continue with the next chapter
        }

        // Create an XPath context
        xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
        if (!xpathCtx) {
            std::cerr << "Failed to create XPath context." << "\n";
            xmlFreeDoc(doc);
            continue;  // Continue with the next chapter
        }

        // Extract all <p> and <img> tags
        xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>("//p | //img"), xpathCtx);
        if (!xpathObj) {
            std::cerr << "Failed to evaluate XPath expression." << "\n";
            xmlXPathFreeContext(xpathCtx);
            xmlFreeDoc(doc);
            continue;  // Continue with the next chapter
        }

        xmlNodeSetPtr nodes = xpathObj->nodesetval;
        int nodeCount = (nodes) ? nodes->nodeNr : 0;

        std::vector<tagData> chapterTags;  // Vector to hold tag data for the current chapter
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

                            // Append the tag to chapterTags
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

                    // Append the tag to chapterTags
                    bookTags.push_back(tag);

                    xmlFree(src);
                }
            }
        }



        // Free resources for the current chapter
        xmlXPathFreeObject(xpathObj);
        xmlXPathFreeContext(xpathCtx);
        xmlFreeDoc(doc);
        
        std::cout << "Chapter done: " << chapterPath.filename().string() << "\n";
        chapterNum++;
    }

    return bookTags;
}


std::vector<decodedData> convertPythonResultsToDecodedData(pybind11::object& results, pybind11::module& tokenizer) {
    std::vector<decodedData> decodedDataVector;
     
    pybind11::object detokenizeText = tokenizer.attr("detokenize_text");

    // Iterate over the results and convert them to decodedData objects
    for (const auto& item : results) {
        decodedData decoded;
        // (generated[0], chapterNum, position) 
        pybind11::tuple resultTuple = item.cast<pybind11::tuple>();
        decoded.output = detokenizeText(resultTuple[0]).cast<std::string>(); 
        decoded.chapterNum = resultTuple[1].cast<int>();
        decoded.position = resultTuple[2].cast<int>();

        decodedDataVector.push_back(decoded);
    }

    return decodedDataVector;
}

int run(const std::string& epubToConvert, const std::string& outputEpubPath) {
    
    // Get the current executable path and resolve to the known relative path
    std::filesystem::path currentDirPath = std::filesystem::current_path();
    std::filesystem::path libPath;
    #if defined(__APPLE__)
        libPath = currentDirPath / "build" / ".venv" / "lib" / "python3.11";
    #elif defined(_WIN32)
        libPath = currentDirPath / "build" / ".venv" / "Lib" / "python3.11";
    #else
        std::cerr << "Unsupported platform!" << std::endl;
        return 1; // Or some other error handling
    #endif

    // std::filesystem::path libPath = currentDirPath / "build" / "vcpkg_installed" /  / "tools" / "python3" / "Lib";
    // std::filesystem::path libPath = currentDirPath / "build" / "vcpkg_installed" / "x64-windows" / "tools" / "python3" / "Lib";
    std::filesystem::path sitePackagesPath = libPath / "site-packages";


    std::cout << "Running the EPUB conversion process..." << "\n";
    std::cout << "epubToConvert: " << epubToConvert << "\n";
    std::cout << "outputEpubPath: " << outputEpubPath << "\n";

    std::string unzippedPath = "unzipped";
    std::string templatePath = "export";
    std::string templateEpub = "rawEpub/template.epub";
    // Initialize the Python interpreter
    pybind11::scoped_interpreter guard{};

    pybind11::module sys = pybind11::module::import("sys");

    sys.attr("path").attr("append")(sitePackagesPath.u8string());

    pybind11::print(sys.attr("path"));

    try {
        pybind11::module EncodeDecode = pybind11::module::import("EncodeAndDecode");
        pybind11::module tokenizer = pybind11::module::import("tokenizer");


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




        std::vector<tagData> bookTags = extractTags(spineOrderXHTMLFiles);

        pybind11::object encodeText = tokenizer.attr("tokenize_text");

        std::string encodedTagsPathString = "./encodedTags.txt";
        std::filesystem::path encodedTagsPath = encodedTagsPathString;
        std::ofstream encodedTagsFile(encodedTagsPath);
        if (!encodedTagsFile.is_open()) {
            std::cerr << "Failed to open file for writing: " << encodedTagsPath << "\n";
            return 1;
        }

        for (auto& tag : bookTags) {
            if (tag.tagId == P_TAG) {
                pybind11::object encodedText = encodeText(tag.text);
                std::string convertedText = pybind11::str(encodedText);

                // Replace line breaks in the tensor data with a single space
                std::string singleLineText;
                for (char c : convertedText) {
                    if (c == '\n' || c == '\r') {
                        singleLineText += ' ';
                    } else {
                        singleLineText += c;
                    }
                }

                encodedTagsFile << tag.chapterNum << "," << tag.position << "," << singleLineText << "\n";
            }
        }

        pybind11::object readEncodedData = EncodeDecode.attr("readEncodedData");

        readEncodedData(encodedTagsPathString);

        std::filesystem::path scriptPath = currentDirPath / "EncodeAndDecode.py";
        std::string multiprocessPythonScriptPath = scriptPath.string();
        std::cout << multiprocessPythonScriptPath << "\n";
        // Create pipes for capturing stdout and stderr
        boost::process::ipstream pipe_stdout;
        boost::process::ipstream pipe_stderr;
        std::filesystem::path pythonEXEPath;
        #if defined(__APPLE__)
            pythonEXEPath = currentDirPath / "build" / ".venv" / "bin" / "python3";
        #elif defined(_WIN32)
            pythonEXEPath = currentDirPath / "build" / ".venv" / "bin" / "python3";
        #else
            std::cerr << "Unsupported platform!" << std::endl;
            return 1; // Or some other error handling
        #endif

        std::string pythonEXEStringPath = pythonEXEPath.string();

        try {
            // Start the Python script as a child process
            boost::process::child c(
                pythonEXEStringPath,           // Find the Python executable
                multiprocessPythonScriptPath,                    // Script path
                boost::process::std_out > pipe_stdout,           // Redirect stdout
                boost::process::std_err > pipe_stderr            // Redirect stderr
            );

            // Read the outputs asynchronously
            std::string line;
            while (c.running() && std::getline(pipe_stdout, line)) {
                std::cout << "Python stdout: " << line << "\n";
            }

            // Read any remaining output after the process has finished
            while (std::getline(pipe_stdout, line)) {
                std::cout << "Python stdout: " << line << "\n";
            }

            // Read any errors
            while (std::getline(pipe_stderr, line)) {
                std::cerr << "Python stderr: " << line << "\n";
            }

            // Wait for the process to exit
            c.wait();

            // Check exit code
            if (c.exit_code() == 0) {
                std::cout << "Python script executed successfully." << "\n";
            } else {
                std::cerr << "Python script exited with code: " << c.exit_code() << "\n";
            }
        } catch (const std::exception& ex) {
            std::cerr << "Exception: " << ex.what() << "\n";
        }

        // std::vector<decodedData> decodedDataVector = convertPythonResultsToDecodedData(results, tokenizer);

        std::vector<decodedData> decodedDataVector;
        std::ifstream file("translatedTags.txt");
        if (!file.is_open()) {
            std::cerr << "Failed to open file: translatedTags.txt" << "\n";
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
                    std::cout << "Decoded text: " << decoded.output << "\n";
                }
            }
        }


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

        // Remove the unzipped and export directories
        std::filesystem::remove_all(unzippedPath);
        std::filesystem::remove_all(templatePath);

        if (std::filesystem::exists("translatedTags.txt")) {
            std::filesystem::remove("translatedTags.txt");  // Deletes the file
            std::cout << "File deleted successfully!" << std::endl;
        }
        if (std::filesystem::exists("encodedTags.txt")) {
            std::filesystem::remove("encodedTags.txt");  // Deletes the file
            std::cout << "File deleted successfully!" << std::endl;
        }

        // End timer
        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double> elapsed = end - start;

        std::cout << "Time taken: " << elapsed.count() << "s" << "\n";

    }
    catch (const pybind11::error_already_set& e) {
        // Catch and print any Python exceptions
        std::cerr << "Python exception: " << e.what() << "\n";
        return 1;
    }    
    return 0;
}


int main() {

    std::streambuf* originalBuffer = std::cout.rdbuf();
    std::ostringstream captureOutput;
    std::cout.rdbuf(captureOutput.rdbuf());
	
    // Setup window
	if (!glfwInit())
		return 1;

// Decide GL+GLSL versions
#if __APPLE__
	// GL 3.2 + GLSL 150
	const char *glsl_version = "#version 150";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // 3.2+ only
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);		   // Required on Mac
#else
	// GL 3.0 + GLSL 130
	const char *glsl_version = "#version 130";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	//glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
	//glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

	// Create window with graphics context
	GLFWwindow *window = glfwCreateWindow(720, 360, "Dear ImGui - Example", NULL, NULL);
	if (window == NULL)
		return 1;
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1); // Enable vsync

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << "\n";
        return -1;
    }

	int screen_width, screen_height;
	glfwGetFramebufferSize(window, &screen_width, &screen_height);
	glViewport(0, 0, screen_width, screen_height);

    std::cout << "Running..." << "\n";
    GUI gui;

    gui.init(window, glsl_version);

    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);

        gui.newFrame();
        gui.update(captureOutput);
        gui.render();

        glfwSwapBuffers(window);
    }

    gui.shutdown();
    std::cout.rdbuf(originalBuffer); // Restore original std::cout buffer
    return 0;
}