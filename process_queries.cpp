#include <execution>
#include "process_queries.h"

std::vector<std::vector<Document>> ProcessQueries(
        const SearchServer& search_server,
        const std::vector<std::string>& queries) {
    std::vector<std::vector<Document>> result(queries.size());
    std::transform(std::execution::par, queries.begin(), queries.end(), result.begin(),
                          [&search_server](const std::string& str) {
                              return search_server.FindTopDocuments(str);
                          });
    return result;
}

std::vector<Document> ProcessQueriesJoined(
        const SearchServer& search_server,
        const std::vector<std::string>& queries) {
    auto query_result = ProcessQueries(search_server, queries);
    std::vector<Document> final_result;
    for (auto& documents : query_result) {
        for (auto& document : documents) {
            final_result.push_back(document);
        }
    }
    return final_result;
}
