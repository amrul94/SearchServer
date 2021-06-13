#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server) {
    std::vector<int> duplicates;
    std::map<std::set<std::string_view>, int> originals;
    for (const auto& document_id : search_server) {
        std::set<std::string_view> temp;
        const auto& word_freqs = search_server.GetWordFrequencies(document_id);
        std::transform(word_freqs.begin(), word_freqs.end(), std::inserter(temp, temp.begin()),
                       [](const auto& key_value) {
                           return key_value.first;
                       });
        if(originals.count(temp) < 1) {
            originals.emplace(temp, document_id);
        }
        else {
            duplicates.push_back(document_id);
        }
    }

    for (const int id : duplicates) {
        std::cout << "Found duplicate document id " << id << std::endl;
        search_server.RemoveDocument(id);
    }
}