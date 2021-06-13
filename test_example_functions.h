#pragma once

#include <execution>
#include <iostream>
#include <random>
#include "document.h"
#include "log_duration.h"
#include "search_server.h"
#include "process_queries.h"

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const std::string& t_str, const std::string& u_str, const std::string& file,
                     const std::string& func, unsigned line, const std::string& hint) {
    using std::cerr;
    using std::operator""s;
    if (t != u) {
        cerr << std::boolalpha;
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << std::endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const std::string& expr_str, const std::string& file, const std::string& func, unsigned line,
                const std::string& hint);

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

// Тест проверяет, что корректное добавление документов в поисковую систему
void TestAddDocument();

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent();

// Тест проверяет поддержку минус слов
void TestMinusWords();

// Основная функция находится снизу, это вспомагательная
void TestMatchDocumentStatus(const SearchServer& server, int id, DocumentStatus status);

// Проверка матчинга документов
void TestMatchDocument();

// Проверка на равенство чисел
inline bool EqualNumbers(const double d1, const double d2, const double epsilon) {
    return std::abs(d1 - d2) < epsilon;
}

// Проверка, что одно число больше другого
inline bool CompareNumbers(const double d1, const double d2, const double epsilon) {
    return d1 - d2 > epsilon;
}

// Сортировка документов по релевантности
void TestSortRelevance();

// Тест на корректное вычисление рейтинга документов
void TestRating();

// Тест на корректную фильтрацию с использованием предиката
void TestFilterPredicate();

// Основная функция находится снизу, это вспомагательная
void TestDocumentsWithStatusProcess(const SearchServer& server,
                                    const std::string& query,
                                    int id,
                                    DocumentStatus status);

// Тест поиска документа с заданным статусом
void TestDocumentsWithStatus();

// Тест на корректное вычисление релевантности найденных документов
void TestRelevanceValue();

template <typename Function>
void RunTestImpl(Function func, const std::string& func_str) {
    func();
    std::cerr << func_str << " OK" << std::endl;
}

std::string GenerateWord(std::mt19937& generator, int max_length);

std::vector<std::string> GenerateDictionary(std::mt19937& generator, int word_count, int max_length);

std::string GenerateQuery(std::mt19937& generator, const std::vector<std::string>& dictionary, int max_word_count, double minus_prob = 0);

std::vector<std::string> GenerateQueries(std::mt19937& generator, const std::vector<std::string>& dictionary, int query_count, int max_word_count);

template <typename QueriesProcessor>
void TestQueriesProcessorImpl(std::string_view mark, QueriesProcessor processor, const SearchServer& search_server, const std::vector<std::string>& queries) {
    LOG_DURATION(mark);
    const auto documents_lists = processor(search_server, queries);
}

template <typename ExecutionPolicy>
void TestParallelRemoveDocumentImpl(std::string_view mark, SearchServer search_server, ExecutionPolicy&& policy) {
    LOG_DURATION(mark);
    std::cerr << "DocumentCount before RemoveDocument: " << search_server.GetDocumentCount() << std::endl;
    const int document_count = search_server.GetDocumentCount();
    for (int id = 0; id < document_count; ++id) {
        search_server.RemoveDocument(policy, id);
    }
    std::cerr << "DocumentCount after RemoveDocument: " << search_server.GetDocumentCount() << std::endl;
}

template <typename ExecutionPolicy>
void TestParallelMatchDocumentImpl(std::string_view mark, SearchServer search_server, const std::string& query, ExecutionPolicy&& policy) {
    LOG_DURATION(mark);
    const int document_count = search_server.GetDocumentCount();
    int word_count = 0;
    for (int id = 0; id < document_count; ++id) {
        const auto [words, status] = search_server.MatchDocument(policy, query, id);
        word_count += words.size();
    }
}

template <typename ExecutionPolicy>
void TestParallelFindTopDocumentsImpl(std::string_view mark, const SearchServer& search_server, const std::vector<std::string>& queries, ExecutionPolicy&& policy) {
    LOG_DURATION(mark);
    double total_relevance = 0;
    for (const std::string_view query : queries) {
        for (const auto& document : search_server.FindTopDocuments(policy, query)) {
            total_relevance += document.relevance;
        }
    }
    std::cerr << "Total_relevance: " << total_relevance << std::endl;
}

#define TEST_QUERIES_PROCESSOR(processor) TestQueriesProcessorImpl(#processor, processor, search_server, queries)

#define TEST_PARALLEL_REMOVE(policy) TestParallelRemoveDocumentImpl(#policy, search_server, std::execution::policy)

#define TEST_PARALLEL_MATCH_DOCUMENT(policy) TestParallelMatchDocumentImpl(#policy, search_server, query, std::execution::policy)

#define TEST_PARALLEL_FIND_DOCUMENTS(policy) TestParallelFindTopDocumentsImpl(#policy, search_server, queries, std::execution::policy)

#define RUN_TEST(func)  RunTestImpl((func), #func)


void TestQueriesProcessor();

void TestParallelRemoveDocument();

void TestParallelMatchDocument();

void TestParallelFindTopDocuments();

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer();
