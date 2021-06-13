#pragma once

#include <algorithm>
#include <execution>
#include <list>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "concurrent_map.h"
#include "document.h"
#include "log_duration.h"
#include "read_input_functions.h"
#include "string_processing.h"

constexpr int MAX_RESULT_DOCUMENT_COUNT = 5;

class SearchServer {
public:

    // Конструктор инициализирующий сервер стоп-словами из контейнера
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);

    // Конструктор инициализирующий сервер стоп-словами из строки
    explicit SearchServer(const std::string& stop_words_text);

    // Конструктор инициализирующий сервер стоп-словами из std::string_view
    explicit SearchServer(std::string_view stop_words_text);

    // Добавление документа
    void AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings);

    // Поиск наиболее релевантных документов по предикату
    template <typename DocumentPredicate>
    [[nodiscard]] std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const; // [Исправил]

    // Поиск наиболее релевантных документов по статусу
    [[nodiscard]] std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus status = DocumentStatus::ACTUAL) const;

    // Поиск наиболее релевантных документов по предикату. Последовательная версия
    template <typename DocumentPredicate>
    [[nodiscard]] std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy&, std::string_view raw_query, DocumentPredicate document_predicate) const; // [Исправил]

    // Поиск наиболее релевантных документов по статусу. Последовательная версия
    [[nodiscard]] std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy&, std::string_view raw_query, DocumentStatus status = DocumentStatus::ACTUAL) const;

    // Поиск наиболее релевантных документов по предикату. Параллельная версия
    template <typename DocumentPredicate>
    [[nodiscard]] std::vector<Document> FindTopDocuments(const std::execution::parallel_policy&, std::string_view raw_query, DocumentPredicate document_predicate) const; // [Исправил]

    // Поиск наиболее релевантных документов по статусу. Параллельная версия
    [[nodiscard]] std::vector<Document> FindTopDocuments(const std::execution::parallel_policy&, std::string_view raw_query, DocumentStatus status = DocumentStatus::ACTUAL) const;

    [[nodiscard]] std::set<int>::const_iterator begin() const;
    [[nodiscard]] std::set<int>::const_iterator end() const;

    // Возвращает количество документов на сервере
    [[nodiscard]] int GetDocumentCount() const;

    // Метод получения частот слов по id документа
    [[nodiscard]] std::map<std::string_view, double> GetWordFrequencies(int document_id) const;

    using MatchDocumentResult = std::tuple<std::vector<std::string_view>, DocumentStatus>;

    // Возвращеет все слова из поискового запроса, присутствующие в документе.
    [[nodiscard]] MatchDocumentResult MatchDocument(std::string_view raw_query, int document_id) const;

    // Возвращеет все слова из поискового запроса, присутствующие в документе. Последовательная версия
    [[nodiscard]] MatchDocumentResult MatchDocument(const std::execution::sequenced_policy&, std::string_view raw_query, int document_id) const;

    // Возвращеет все слова из поискового запроса, присутствующие в документе. Параллельная версия
    [[nodiscard]] MatchDocumentResult MatchDocument(const std::execution::parallel_policy&, std::string_view raw_query, int document_id) const;

    // Удаление документов из поискового сервера
    void RemoveDocument(int document_id);

    // Удаление документов из поискового сервера. Последовательная версия
    void RemoveDocument(const std::execution::sequenced_policy&, int document_id);

    // Удаление документов из поискового сервера. Параллельная версия
    void RemoveDocument(const std::execution::parallel_policy&, int document_id);

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
        std::string text;   // От него зависят все поля с std::string_view
    };

    const std::set<std::string> stop_words_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::unordered_map<int, std::map<std::string_view, double>> document_to_word_freqs_;
    std::map<int, DocumentData> documents_;
    std::set<int> document_ids_;

private:
    // Проверка на стоп-слова
    [[nodiscard]] bool IsStopWord(const std::string& word) const;

    // Проверка на наличие в слове спец-символов
    static bool IsValidWord(std::string_view word);

    // Разделяет строку на отдельные слова и возвращает их в векторе, исключая стоп-слова
    [[nodiscard]] std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const;

    // Вычисление среднего рейтинга
    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string_view data;
        bool is_minus{};
        bool is_stop{};
    };

    [[nodiscard]] QueryWord ParseQueryWord(std::string_view text) const;

    struct Query {
        std::set<std::string_view> plus_words;
        std::set<std::string_view> minus_words;
    };

    [[nodiscard]] Query ParseQuery(std::string_view text) const;

    // Existence required
    [[nodiscard]] double ComputeWordInverseDocumentFreq(std::string_view word) const;

    // Поиск по запросу
    template <typename DocumentPredicate>
    [[nodiscard]] std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const;

    // Поиск по запросу. Последовательная версия
    template <typename DocumentPredicate>
    [[nodiscard]] std::vector<Document> FindAllDocuments(const std::execution::sequenced_policy&, const Query& query, DocumentPredicate document_predicate) const;

    // Поиск по запросу. Параллельная версия
    template <typename DocumentPredicate>
    [[nodiscard]] std::vector<Document> FindAllDocuments(const std::execution::parallel_policy&, const Query& query, DocumentPredicate document_predicate) const;
};

// Вспомогательные функции для обработки исключений
void ServerCreation();

void AddDocument(SearchServer& search_server, int document_id, const std::string& document, DocumentStatus status,
                 const std::vector<int>& ratings);

void FindTopDocuments(const SearchServer& search_server, const std::string& raw_query);

void PrintMatchDocumentResult(int document_id, const std::vector<std::string>& words, DocumentStatus status);

void MatchDocuments(const SearchServer& search_server, const std::string& query);

// Реализация шаблонных методов
// Конструктор инициализирующий сервер стоп-словами из контейнера
template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
        : stop_words_(MakeUniqueNonEmptyStrings(stop_words))  // Extract non-empty stop words
{
    if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
        throw std::invalid_argument("Some of stop words are invalid");
    }
}

// Поиск наиболее релевантных документов по предикату
template <typename DocumentPredicate>
[[nodiscard]] std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const {
    return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}

// Поиск наиболее релевантных документов по предикату. Последовательная версия
template <typename DocumentPredicate>
[[nodiscard]] std::vector<Document>
SearchServer::FindTopDocuments(const std::execution::sequenced_policy&, std::string_view raw_query, DocumentPredicate document_predicate) const {
    //LOG_DURATION_STREAM("Operation time", std::cout);
    const Query query = ParseQuery(raw_query);
    auto matched_documents = FindAllDocuments(query, document_predicate);

    sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
        if (std::abs(lhs.relevance - rhs.relevance) < 1e-6) {
            return lhs.rating > rhs.rating;
        } else {
            return lhs.relevance > rhs.relevance;
        }
    });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

// Поиск наиболее релевантных документов по предикату. Параллельная версия
template <typename DocumentPredicate>
[[nodiscard]] std::vector<Document>
SearchServer::FindTopDocuments(const std::execution::parallel_policy&, std::string_view raw_query, DocumentPredicate document_predicate) const {
    //LOG_DURATION_STREAM("Operation time", std::cout);
    const Query query = ParseQuery(raw_query);
    auto matched_documents = FindAllDocuments(std::execution::par, query, document_predicate);

    sort(std::execution::par, matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
        if (std::abs(lhs.relevance - rhs.relevance) < 1e-6) {
            return lhs.rating > rhs.rating;
        } else {
            return lhs.relevance > rhs.relevance;
        }
    });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}


// Поиск по запросу
template <typename DocumentPredicate>
[[nodiscard]] std::vector<Document> SearchServer::FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
    return FindAllDocuments(std::execution::seq, query, document_predicate);
}

// Поиск по запросу. Последовательная версия
template <typename DocumentPredicate>
[[nodiscard]] std::vector<Document>
SearchServer::FindAllDocuments(const std::execution::sequenced_policy&, const Query& query, DocumentPredicate document_predicate) const {
    std::map<int, double> document_to_relevance;
    for (std::string_view word : query.plus_words) {
        auto found_documents = word_to_document_freqs_.find(word);
        if (found_documents == word_to_document_freqs_.end()) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : found_documents->second) {
            if (document_predicate(document_id, documents_.at(document_id).status, documents_.at(document_id).rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }

    for (std::string_view word : query.minus_words) {
        auto found_documents = word_to_document_freqs_.find(word);
        if (found_documents == word_to_document_freqs_.end()) {
            continue;
        }
        for (const auto [document_id, _] : found_documents->second) {
            document_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    matched_documents.reserve(document_to_relevance.size());
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.emplace_back(document_id, relevance, documents_.at(document_id).rating);
    }
    return matched_documents;
}

// Поиск по запросу. Параллельная версия
template <typename DocumentPredicate>
[[nodiscard]] std::vector<Document>
SearchServer::FindAllDocuments(const std::execution::parallel_policy&, const Query& query, DocumentPredicate document_predicate) const {
    ConcurrentMap<int, double> document_to_relevance(64);
    std::for_each(
        std::execution::par,
        query.plus_words.begin(), query.plus_words.end(),
        [this, document_predicate, &document_to_relevance](std::string_view word) {
            auto found_documents = word_to_document_freqs_.find(word);
            if (found_documents == word_to_document_freqs_.end()) {
                return;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : found_documents->second) {
                if (document_predicate(document_id, documents_.at(document_id).status, documents_.at(document_id).rating)) {
                    document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                }
            }

        });

    //

    std::for_each(
            std::execution::par,
            query.minus_words.begin(), query.minus_words.end(),
            [this, &document_to_relevance](std::string_view word) {
                auto found_documents = word_to_document_freqs_.find(word);
                if (found_documents == word_to_document_freqs_.end()) {
                    return;
                }
                for (const auto [document_id, _] : found_documents->second) {
                    document_to_relevance.Erase(document_id);
                }
            });

    std::map<int, double> ordinary_map = document_to_relevance.BuildOrdinaryMap();
    std::vector<Document> matched_documents;
    matched_documents.reserve(ordinary_map.size());
    for (const auto [document_id, relevance] : ordinary_map) {
        matched_documents.emplace_back(document_id, relevance, documents_.at(document_id).rating);
    }
    return matched_documents;
}