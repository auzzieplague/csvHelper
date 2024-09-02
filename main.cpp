#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <map>
#include <regex>
#include <vector>
#include <algorithm>
#include <filesystem>

class CSVReader {
private:
    std::vector<std::string> mHeadings;
    std::vector<std::map<std::string, std::string> > mCsvData;

    std::vector<std::string> splitLine(const std::string &line, char delimiter) {
        std::vector<std::string> tokens;
        std::string token;
        std::stringstream tokenStream(line);

        while (std::getline(tokenStream, token, delimiter)) {
            tokens.push_back(token);
        }

        return tokens;
    }

public:
    std::vector<std::string> splitLineAndReplaceCommas(const std::string &line) {
        std::vector<std::string> tokens;
        std::string token;
        bool insideQuotes = false;

        for (size_t i = 0; i < line.length(); ++i) {
            char currentChar = line[i];

            if (currentChar == '"') {
                insideQuotes = !insideQuotes; // Toggle the insideQuotes flag
            } else if (currentChar == ',' && insideQuotes) {
                token += "#!*"; // Replace comma inside quotes with placeholder
                continue;
            } else if (currentChar == ',' && !insideQuotes) {
                tokens.push_back(token);
                token.clear();
                continue;
            }
            token += currentChar;
        }

        tokens.push_back(token); // Add the last token

        return tokens;
    }

    std::string restoreCommas(const std::string &str) {
        std::string restoredStr = str;
        size_t pos = 0;
        while ((pos = restoredStr.find("#!*", pos)) != std::string::npos) {
            restoredStr.replace(pos, 3, ",");
            pos += 1; // Move past the replaced comma
        }
        return restoredStr;
    }

    CSVReader *readCsv(const std::string &filePath) {
        /**
         * We handle internal commas by replacing them with #!* before exploding the line and then replacing them when populating the cell value
         * We handle new lines inside quotes " \n " by joining the next line when the number of quotes in the current line is odd (should be matching open and closing quotes)
         */
        std::ifstream file(filePath);
        std::string line;

        if (!file.is_open()) {
            std::cerr << std::endl << "Unable to open file: " << filePath << std::endl;
            return this;
        }

        std::getline(file, line);
        mHeadings = splitLineAndReplaceCommas(line);

        while (std::getline(file, line)) {
            if (line.empty()) continue; // Skip empty lines

            // Check if the line has an odd number of quotes
            size_t quoteCount = std::count(line.begin(), line.end(), '"');
            while (quoteCount % 2 != 0) {
                std::string nextLine;
                if (!std::getline(file, nextLine)) {
                    break; // Exit loop if no more lines are available
                }
                line += "\n" + nextLine;
                quoteCount += std::count(nextLine.begin(), nextLine.end(), '"');
            }

            auto tokens = splitLineAndReplaceCommas(line);

            std::map<std::string, std::string> rowData;
            for (size_t i = 0; i < tokens.size(); ++i) {
                if (i < mHeadings.size()) {
                    // Restore commas from the placeholder before storing the data
                    rowData[mHeadings[i]] = restoreCommas(tokens[i]);
                }
            }
            mCsvData.push_back(rowData);
        }

        file.close();
        return this;
    }

    [[nodiscard]] const std::vector<std::map<std::string, std::string> > &getCsvData() const {
        return mCsvData;
    }

    [[nodiscard]] const std::vector<std::string> &getHeadings() const {
        return mHeadings;
    }

    CSVReader &writeCsv(const std::string &filePath) const {
        // NOLINT(*-use-nodiscard)
        // NOLINT(*-use-nodiscard)
        std::ofstream file(filePath);

        if (!file.is_open()) {
            std::cerr << std::endl << "Unable to open file: " << filePath << std::endl;
            return const_cast<CSVReader &>(*this);
        }

        // Write the headings
        for (size_t i = 0; i < mHeadings.size(); ++i) {
            file << mHeadings[i];
            if (i < mHeadings.size() - 1) {
                file << ",";
            }
        }
        file << "\n";


        for (const auto &row: mCsvData) {
            for (size_t i = 0; i < mHeadings.size(); ++i) {
                auto it = row.find(mHeadings[i]);
                if (it != row.end()) {
                    file << it->second;
                } else {
                    file << ""; // Output an empty string if the cell data is missing
                }
                if (i < mHeadings.size() - 1) {
                    file << ",";
                }
            }
            file << "\n";
        }

        file.close();
        return const_cast<CSVReader &>(*this);
    }

    [[nodiscard]] int getHeadingIndexByName(const std::string &headingName) const {
        for (size_t i = 0; i < mHeadings.size(); ++i) {
            if (mHeadings[i] == headingName) {
                return static_cast<int>(i);
            }
        }
        return -1; // Return -1 if the heading is not found
    }

    void addDescriptionDateColumn() {
        this->mHeadings.push_back("DescDate");
        auto descDateIdx = this->mHeadings.size() - 1;
        this->mHeadings.push_back("DescDateTimeStamp");
        auto timeStampIdx = this->mHeadings.size() - 1;
        auto descriptionIdx = this->getHeadingIndexByName("*Description");

        if (descriptionIdx == -1) {
            std::cerr << std::endl << "Description column not found!" << std::endl;
            return;
        }

        for (auto &row: this->mCsvData) {
            std::string &description = row["*Description"];
            std::string date;

            // Find the date in the format dd/mm/yy within the description
            std::smatch match;
            std::regex dateRegex(R"((\d{2}/\d{2}/\d{2}))");

            if (std::regex_search(description, match, dateRegex)) {
                date = match.str(0); // Extract the date

                // Remove the date from the description
                description = std::regex_replace(description, dateRegex, "");
            } else {
                date = "N/A"; // No date found, use a default value
                row["DescDate"] = date;
                row["DescDateTimeStamp"] = "0"; // Use "0" to indicate an invalid timestamp
                continue;
            }

            row["DescDate"] = date; // Insert the extracted date into the new column

            // Convert the date to a UTS timestamp
            struct tm tm = {};
            std::istringstream ss(date);
            ss >> std::get_time(&tm, "%d/%m/%y"); // Parse the date string into tm struct

            if (ss.fail()) {
                std::cerr << std::endl << "Failed to parse date: " << date << std::endl;
                row["DescDateTimeStamp"] = "0"; // Use "0" to indicate an invalid timestamp
                continue;
            }

            time_t timeStamp = mktime(&tm);
            if (timeStamp == -1) {
                std::cerr << std::endl << "Failed to convert date to timestamp: " << date << std::endl;
                row["DescDateTimeStamp"] = "0"; // Use "0" to indicate an invalid timestamp
            } else {
                row["DescDateTimeStamp"] = std::to_string(timeStamp);
            }
        }
    }

    void doColumnReplacements() {
        std::ifstream settingsFile("settings.txt");
        std::map<std::string, std::map<std::string, std::string> > replacements;
        std::string line;
        bool replacementsSectionFound = false;

        // Define the regex pattern as a string
        std::string pattern = "\"([^\"]*)\"\\s*=\\s*\"([^\"]*)\"";
        std::regex pairRegex(pattern);

        // Read and process settings.txt
        while (std::getline(settingsFile, line)) {
            if (line.empty()) {
                continue; // Skip empty lines
            }

            if (!replacementsSectionFound) {
                if (line == "replacements:") {
                    replacementsSectionFound = true;
                }
                continue; // Skip lines until "replacements:" is found
            }

            if (line == "end:") {
                break;
            }

            // Split line into heading and replacements
            size_t colonPos = line.find(':');
            if (colonPos == std::string::npos) {
                continue; // Skip lines without a colon
            }

            std::string heading = line.substr(0, colonPos);
            std::string replacementsStr = line.substr(colonPos + 1);

            auto headingIdx = this->getHeadingIndexByName(heading);
            if (headingIdx == -1) {
                std::cerr << "Warning: Heading '" << heading << "' not found in CSV. Skipping..." << std::endl;
                continue;
            }

            // Parse the CSV list of replacements
            std::map<std::string, std::string> replacementMap;
            std::sregex_iterator iter(replacementsStr.begin(), replacementsStr.end(), pairRegex);
            std::sregex_iterator end;

            while (iter != end) {
                std::string from = (*iter)[1].str();
                std::string to = (*iter)[2].str();
                replacementMap[from] = to;
                ++iter;
            }

            replacements[heading] = replacementMap;
        }

        settingsFile.close();

        // Process each row of the CSV data
        for (auto &row: this->mCsvData) {
            for (const auto &replacement: replacements) {
                const std::string &heading = replacement.first;
                const auto &replacementMap = replacement.second;

                auto headingIdx = this->getHeadingIndexByName(heading);
                if (headingIdx == -1) {
                    std::cerr << "Warning: Heading '" << heading << "' not found in CSV. Skipping..." << std::endl;
                    continue;
                }

                std::string &cellData = row[heading];
                for (const auto &pair: replacementMap) {
                    const std::string &from = pair.first;
                    const std::string &to = pair.second;
                    size_t pos = 0;

                    // Perform all occurrences of the replacement
                    while ((pos = cellData.find(from, pos)) != std::string::npos) {
                        cellData.replace(pos, from.length(), to);
                        pos += to.length(); // Move past the replacement
                    }
                }
            }
        }
    }


    void applySorting() {
        std::ifstream settingsFile("settings.txt");
        std::vector<std::pair<std::string, std::string> > sortOrder;
        std::string line;
        bool sortOrderSectionFound = false;

        // Read and process settings.txt until "sort order:" is found
        while (std::getline(settingsFile, line)) {
            if (line.empty()) {
                continue; // Skip empty lines
            }

            if (!sortOrderSectionFound) {
                if (line == "sort order:") {
                    sortOrderSectionFound = true;
                }
                continue; // Skip lines until "sort order:" is found
            }

            if (line == "end:") {
                break;
            }

            // Parse each line to extract the heading and order (asc/desc)
            size_t colonPos = line.find(':');
            if (colonPos != std::string::npos) {
                std::string heading = line.substr(0, colonPos);
                std::string order = line.substr(colonPos + 1);
                trim(heading); // Remove extra spaces around heading
                trim(order); // Remove extra spaces around order
                sortOrder.emplace_back(heading, order);
            }
        }

        settingsFile.close();

        if (sortOrder.empty()) {
            std::cerr << std::endl << "No sort order specified in settings.txt" << std::endl;
            return;
        }

        // Sort the CSV data based on the sortOrder
        // Work backwards through the vector to ensure the least significant sort order item is sorted first

        for (auto it = sortOrder.rbegin(); it != sortOrder.rend(); ++it) {
            const std::string &heading = it->first;
            const std::string &order = it->second;

            auto headingIdx = this->getHeadingIndexByName(heading);

            if (headingIdx == -1) {
                std::cerr << std::endl << "Warning: Heading '" << heading << "' not found in CSV. Skipping..." <<
                        std::endl;
                continue;
            }

            // Determine sort direction (ascending or descending)
            bool ascending = (order == "asc");

            // Lambda function to compare two rows based on the current heading
            auto compare = [heading, ascending](const std::map<std::string, std::string> &row1,
                                                const std::map<std::string, std::string> &row2) {
                if (ascending) {
                    return row1.at(heading) < row2.at(heading);
                } else {
                    return row1.at(heading) > row2.at(heading);
                }
            };

            // Use stable_sort instead of sort to maintain previous sort orders
            std::stable_sort(mCsvData.begin(), mCsvData.end(), compare);
            std::cout << "Sorting by " << heading << " (" << order << ")" << std::endl;
        }
    }

    void trim(std::string &str) {
        const char *whitespace = " \t\n\r\f\v";
        str.erase(str.find_last_not_of(whitespace) + 1);
        str.erase(0, str.find_first_not_of(whitespace));
    }

    void removeHeader(std::vector<std::string> &headings, const std::string &headerName) {
        auto it = std::find(headings.begin(), headings.end(), headerName);
        if (it != headings.end()) {
            headings.erase(it);
        } else {
            std::cerr << "Header '" << headerName << "' not found!" << std::endl;
        }
    }

    void applyDateToDescription() {
        // Get the indices of the columns to be removed
        auto descDateIdx = this->getHeadingIndexByName("DescDate");
        auto timeStampIdx = this->getHeadingIndexByName("DescDateTimeStamp");
        auto descriptionIdx = this->getHeadingIndexByName("*Description");

        if (descriptionIdx == -1) {
            std::cerr << std::endl << "Description column not found!" << std::endl;
            return;
        }

        // Iterate through each row to modify *Description
        for (auto &row: this->mCsvData) {
            if (descDateIdx != -1) {
                std::string descDate = row["DescDate"];
                std::string &description = row["*Description"];

                // Check if the description starts with a quote
                if (!description.empty() && description.front() == '"') {
                    // Insert DescDate after the opening quote
                    description.insert(1, descDate + " ");
                } else {
                    // Prepend DescDate to *Description with a space
                    description = descDate + " " + description;
                }
            }
        }

        // Remove the DescDate and DescDateTimeStamp columns
        if (descDateIdx != -1) {
            removeHeader(this->mHeadings, "DescDate");
        }

        if (timeStampIdx != -1) {
            removeHeader(this->mHeadings, "DescDateTimeStamp");
        }

        // Also remove the columns from the data in each row
        for (auto &row: this->mCsvData) {
            if (descDateIdx != -1) {
                row.erase("DescDate");
            }

            if (timeStampIdx != -1) {
                row.erase("DescDateTimeStamp");
            }
        }
    }


    void updateDueDate() {
        // Open settings.txt to find the "due date additional days:" line
        std::ifstream settingsFile("settings.txt");

        int daysToAdd = 0;
        std::string line;

        // Look for the "days to add to due date:" line
        while (std::getline(settingsFile, line)) {
            if (line.find("due date additional days:") != std::string::npos) {
                // Extract the number after the colon
                std::string value = line.substr(line.find(':') + 1);
                daysToAdd = std::stoi(value);
                break;
            }
        }

        settingsFile.close();

        if (daysToAdd == 0) {
            std::cerr << std::endl << "No days to add specified or value is 0. Skipping due date update." << std::endl;
            return;
        }

        // Update the "*DueDate" column
        auto dueDateIdx = this->getHeadingIndexByName("*DueDate");
        if (dueDateIdx == -1) {
            std::cerr << std::endl << "DueDate column not found in CSV. Skipping..." << std::endl;
            return;
        }

        for (auto &row: this->mCsvData) {
            std::string dueDateStr = row["*DueDate"];
            std::tm dueDateTm = stringToDate(dueDateStr, "%d/%m/%Y");

            // Add the specified number of days to the due date
            std::chrono::system_clock::time_point dueDateTp = std::chrono::system_clock::from_time_t(
                std::mktime(&dueDateTm));
            dueDateTp += std::chrono::hours(24 * daysToAdd);

            // Convert back to string and update the row
            std::time_t newTime = std::chrono::system_clock::to_time_t(dueDateTp);
            std::tm *newTm = std::localtime(&newTime);
            row["*DueDate"] = dateToString(*newTm, "%d/%m/%Y");
        }

        std::cout << "Due dates updated: added " << daysToAdd << " days." << std::endl;
    }

    // Helper function to convert string to std::tm
    std::tm stringToDate(const std::string &dateStr, const std::string &format) {
        std::tm tm = {};
        std::istringstream ss(dateStr);
        ss >> std::get_time(&tm, format.c_str());
        return tm;
    }

    // Helper function to convert std::tm to string
    std::string dateToString(const std::tm &tm, const std::string &format) {
        std::ostringstream ss;
        ss << std::put_time(&tm, format.c_str());
        return ss.str();
    }

    void addAppendages() {
        std::ifstream settingsFile("settings.txt");

        std::map<std::string, std::vector<std::pair<std::string, std::string> > > appendagesMap;
        std::string line;
        bool appendagesSectionFound = false;

        // Read and process settings.txt until "appendages:" is found
        while (std::getline(settingsFile, line)) {
            if (line.empty()) {
                continue; // Skip empty lines
            }

            if (!appendagesSectionFound) {
                if (line == "appendages:") {
                    appendagesSectionFound = true;
                }
                continue; // Skip lines until "appendages:" is found
            }

            // End of appendages section
            if (line == "end:") {
                break;
            }

            // Parse the line into column name and key-value pairs
            std::regex lineRegex(R"(([^:]+):(.+))");
            std::smatch lineMatch;
            if (std::regex_match(line, lineMatch, lineRegex)) {
                std::string columnName = lineMatch[1].str();
                std::string keyValues = lineMatch[2].str();

                std::string pattern = "\"([^\"]+)\"\\s*=\\s*\"([^\"]+)\"";
                std::regex pairRegex(pattern);

                // Regex to match individual key-value pairs
                auto pairBegin = std::sregex_iterator(keyValues.begin(), keyValues.end(), pairRegex);
                auto pairEnd = std::sregex_iterator();

                for (std::sregex_iterator i = pairBegin; i != pairEnd; ++i) {
                    std::smatch match = *i;
                    std::string key = match[1].str();
                    std::string value = match[2].str();

                    appendagesMap[columnName].emplace_back(key, value);
                }
            } else {
                std::cerr << std::endl << "Invalid appendages line format: " << line << std::endl;
            }
        }

        settingsFile.close();

        // Process each row in the CSV data
        for (auto &row: this->mCsvData) {
            for (const auto &appendage: appendagesMap) {
                const std::string &columnName = appendage.first;

                // Check if the specified column exists and doesn't contain "Claim Type"
                if (row.find(columnName) != row.end() && row[columnName].find("Claim Type") == std::string::npos) {
                    for (const auto &pair: appendage.second) {
                        const std::string &key = pair.first;
                        const std::string &value = pair.second;

                        // If the column contains the key, append the value
                        if (row[columnName].find(key) != std::string::npos) {
                            row[columnName] += " " + value;
                        }
                    }
                }
            }
        }
    }


    std::string getNewFileNamePostfix() {
        std::ifstream settingsFile("settings.txt");
        std::string line;
        // Look for the "days to add to due date:" line
        while (std::getline(settingsFile, line)) {
            if (line.find("new file name postfix:") != std::string::npos) {
                std::string value = line.substr(line.find(':') + 1);
                settingsFile.close();
                trim (value);
                return value;
            }
        }

        settingsFile.close();
        return "_new";
    }
};


void pushMessage(std::vector<std::string> lines) {
    // Ensure there are at least 5 lines, padding with empty strings if necessary
    while (lines.size() < 5) {
        lines.push_back(""); // Pad with empty strings
    }
    std::cout << "==================================================================================================="
            << std::endl;
    std::cout << "        .-\"-.            " << lines[0] << std::endl;
    std::cout << "       /|6 6|\\           " << lines[1] << std::endl;
    std::cout << "      {/(_0_)\\}          " << lines[2] << std::endl;
    std::cout << "       _/ ^ \\_           " << lines[3] << std::endl;
    std::cout << "      (/ /^\\ \\)-'        " << lines[4] << std::endl;
    std::cout << R"(       ""' '"")" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "==================================================================================================="
            << std::endl;
    std::cin.ignore();
}


int main(int argc, char *argv[]) {
    if (argc < 2) {
        pushMessage({
            "Howdy, this tool is for processing invoices",
            "", "",
            "Drag and drop your csv file onto the windows executable to process it",
            "text replacements and sort order can be updated in the settings.txt file"
        });
        return 0;
    }

    std::string inputFilePath = argv[1];

    // std::string inputFilePath = "test.csv"; // for debugging

    std::filesystem::path filePath(inputFilePath);

    if (!std::filesystem::exists(filePath)) {
        pushMessage({
            "Oh Dear,",
            "", "", "",
            "that file doesn't appear to be valid"
        });
    }

    std::ifstream settingsFile("settings.txt");
    if (!settingsFile.is_open()) {
        pushMessage({
            "Oh Dear,",
            "", "", "",
            "the settings file appears to be missing"
        });
        exit(0);
    }

    auto reader = new CSVReader();
    std::string postFix = reader->getNewFileNamePostfix();

    // Create new file path by appending a postfix before the file extension
    std::filesystem::path outputFilePath = filePath;
    outputFilePath.replace_filename(filePath.stem().string() + postFix + filePath.extension().string());

    reader->readCsv(inputFilePath);

    // Add temporary columns explicity formatting data as dd/mm/yy and UTS to assist with sorting
    reader->addDescriptionDateColumn();
    reader->doColumnReplacements();
    reader->applySorting();

    // note: Description may or may not be wrapped in speechmarks, depending on internal commas :|
    reader->applyDateToDescription();

    // all additional days to be added to due date
    reader->updateDueDate();

    // specific case is to add Claim Type if an item code exists
    // but extended to a more general function which might be used on other columns
    reader->addAppendages();

    reader->writeCsv(outputFilePath.string());

    delete reader;

    pushMessage({
        "All done!",
        "", "",
        "Your new file is located in the same directory as the source and named",
        outputFilePath.filename().string()
    });

    return 0;
}
