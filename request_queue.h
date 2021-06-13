#pragma once

#include <deque>
#include <string>
#include <vector>

#include "document.h"
#include "search_server.h"

class RequestQueue {
public:
    explicit RequestQueue(SearchServer &searchServer);

    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate);

    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);

    std::vector<Document> AddFindRequest(const std::string& raw_query);

    [[nodiscard]] int GetNoResultRequests() const;
private:
    struct QueryResult {
        bool is_empty;
        std::vector<Document> found_docs;
    };
    std::deque<QueryResult> requests_;
    const static int sec_in_day_ = 1440;
    SearchServer& search_server_;
    int no_result_requests_ = 0;
};

template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
    std::vector<Document> found_docs = search_server_.FindTopDocuments(raw_query, document_predicate);
    if (found_docs.empty()) {
        requests_.push_back({true, found_docs});
        ++no_result_requests_;
    }
    else
        requests_.push_back({false, found_docs});
    if (requests_.size() > sec_in_day_) {
        if (requests_.front().is_empty)
            --no_result_requests_;
        requests_.pop_front();
    }
    return found_docs;
}