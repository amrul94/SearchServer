#include "test_example_functions.h"

#include <vector>

using namespace std::literals;

void AssertImpl(bool value, const std::string& expr_str, const std::string& file, const std::string& func, unsigned line,
                const std::string& hint) {
    using std::cerr;
    if (!value) {
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << std::endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

// Тест проверяет, что корректное добавление документов в поисковую систему
void TestAddDocument() {
    const std::string document = "cat in the city";
    const std::vector<int> ratings = {1, 2, 3};

    SearchServer server(""sv);
    server.AddDocument(0, document, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(1, document, DocumentStatus::BANNED, ratings);
    ASSERT_EQUAL(server.GetDocumentCount(), 2);

    // [Здесь сделал как в TestMinusWords]
    // Сначала убеждаемся, что система находит нужный документ
    {
        const std::vector<Document>& found_docs = server.FindTopDocuments("cat"s);
        ASSERT_EQUAL(found_docs.size(), 1);
        ASSERT_EQUAL(found_docs.at(0).id, 0);
        ASSERT_EQUAL(found_docs.at(0).rating, 2);
    }


    // Затем убеждаемся, что лишние документы найдены не будут
    {
        const std::vector<Document>& found_docs = server.FindTopDocuments("dog"s);
        ASSERT(found_docs.empty());
    }

}

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const std::string content = "cat in the city"s;
    const std::vector<int> ratings = {1, 2, 3};

    // Сначала убеждаемся, что поиск слова, не входящего в список стоп-слов,
    // находит нужный документ
    {
        SearchServer server(""sv);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    // Затем убеждаемся, что поиск этого же слова, входящего в список стоп-слов,
    // возвращает пустой результат
    {
        SearchServer server("in the"sv);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in"s).empty());
    }
}

// Тест проверяет поддержку минус слов
void TestMinusWords() {
    SearchServer server(""sv);
    const int cat_id = 0, dog_id = 1;
    const std::vector<int> ratings = {1, 2, 3};
    const DocumentStatus actual_doc = DocumentStatus::ACTUAL;

    server.AddDocument(cat_id, "cat in the city"s, actual_doc, ratings);
    server.AddDocument(dog_id, "dog in the village"s, actual_doc, ratings);
    ASSERT_EQUAL(server.GetDocumentCount(), 2);

    // Убеждаемся, что минус слово отсекает второй документ
    {
        const std::vector<Document>& found_docs = server.FindTopDocuments("cat or dog in the -village");
        ASSERT_EQUAL(found_docs.size(),1);
        ASSERT_EQUAL(found_docs.at(0).id, cat_id);
    }
    // Убеждаемся, что минус слово отсекает первый документ
    {
        const std::vector<Document>& found_docs = server.FindTopDocuments("cat or dog in the -city");
        ASSERT_EQUAL(found_docs.size(), 1);
        ASSERT_EQUAL(found_docs.at(0).id, dog_id);
    }

    // Убеждаемся, что минус слово работает для обоих документов
    {
        const std::vector<Document>& found_docs = server.FindTopDocuments("rat -in the space");
        ASSERT(found_docs.empty());
    }

    // Убеждаемся, что минус слово которого нет в обоих документах не повлияет на результат
    {
        const std::vector<Document>& found_docs = server.FindTopDocuments("-rat in the space");
        ASSERT_EQUAL(found_docs.size(), 2);
        ASSERT(found_docs.at(0).id == cat_id);
        ASSERT(found_docs.at(1).id == dog_id);
    }
}

// Основная функция находится снизу, это вспомагательная
void TestMatchDocumentStatus(const SearchServer& server, int id, DocumentStatus status) {
    const auto [words_1, status_1] = server.MatchDocument(std::execution::par, "cat -city", id);
    ASSERT(words_1.empty());
    ASSERT_EQUAL(static_cast<int>(status_1), static_cast<int>(status));

    const auto [words_2, status_2] = server.MatchDocument(std::execution::par, "cat city -fake", id);
    ASSERT_EQUAL(words_2.size(), 2);
    ASSERT_EQUAL(words_2.at(0), "cat"s);
    ASSERT_EQUAL(words_2.at(1), "city"s);
    ASSERT_EQUAL(static_cast<int>(status_2), static_cast<int>(status));
}

// Проверка матчинга документов
void TestMatchDocument() {
    SearchServer server(""s);
    const int id_1 = 1, id_2 = 2, id_3 = 3, id_4 = 4;
    const std::vector<int> ratings = {1, 2, 3};
    const std::string content{"cat in the city"};

    server.AddDocument(id_1, content, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(id_2, content, DocumentStatus::BANNED, ratings);
    server.AddDocument(id_3, content, DocumentStatus::IRRELEVANT, ratings);
    server.AddDocument(id_4, content, DocumentStatus::REMOVED, ratings);
    ASSERT_EQUAL(server.GetDocumentCount(), 4);

    TestMatchDocumentStatus(server, id_1, DocumentStatus::ACTUAL);
    TestMatchDocumentStatus(server, id_2, DocumentStatus::BANNED);
    TestMatchDocumentStatus(server, id_3, DocumentStatus::IRRELEVANT);
    TestMatchDocumentStatus(server, id_4, DocumentStatus::REMOVED);
}

// Сортировка документов по релевантности
void TestSortRelevance() {
    const double epsilon = 1e-6;
    const DocumentStatus actual_doc = DocumentStatus::ACTUAL;
    const std::vector<int> rating{1, 2, 3};
    const std::string query = "kind cat with long tail";
    SearchServer server(""sv);

    server.AddDocument(6, "human tail", actual_doc, rating);
    server.AddDocument(5, "old angry fat dog with short tail"s, actual_doc, rating);
    server.AddDocument(4, "nasty cat beautiful tail"s, actual_doc, rating);
    server.AddDocument(3, "not beautiful cat"s, actual_doc, rating);
    server.AddDocument(2, "huge fat parrot"s, actual_doc, rating);
    server.AddDocument(1, "removed cat"s, actual_doc, rating);

    const auto& docs = server.FindTopDocuments(query);
    for (size_t i = 0; i + 1 < docs.size(); ++i) {
        ASSERT(CompareNumbers(docs.at(i).relevance, docs.at(i + 1).relevance, epsilon) ||
               EqualNumbers(docs.at(i).relevance, docs.at(i + 1).relevance, epsilon));
    }
}

// Тест на корректное вычисление рейтинга документов
void TestRating() {
    const DocumentStatus actual_doc = DocumentStatus::ACTUAL;
    const std::string query = "cat in the city"s;
    SearchServer server(""sv);

    server.AddDocument(1, query, actual_doc, {1, 2, 3});
    server.AddDocument(2, query, actual_doc, {1, 2, 3, 4, 5});
    server.AddDocument(3, query, actual_doc, {5, 10, 15});
    server.AddDocument(4, query, actual_doc, {-5, -10, -15});
    server.AddDocument(5, query, actual_doc, {-1, -3, -5});
    ASSERT_EQUAL(server.GetDocumentCount(), 5);

    const std::vector<Document>& found_docs = server.FindTopDocuments(query, actual_doc);
    ASSERT_EQUAL(found_docs.size(), 5);
    ASSERT_EQUAL(found_docs.at(0).rating, 10);
    ASSERT_EQUAL(found_docs.at(1).rating, 3);
    ASSERT_EQUAL(found_docs.at(2).rating, 2);
    ASSERT_EQUAL(found_docs.at(3).rating,-3);
    ASSERT_EQUAL(found_docs.at(4).rating, -10);
}

// Тест на корректную фильтрацию с использованием предиката
void TestFilterPredicate() {
    const std::string query = "cat in the city"s;
    SearchServer server(""s);

    server.AddDocument(1, query, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(2, query, DocumentStatus::ACTUAL, {5, 10, 15});
    server.AddDocument(3, query, DocumentStatus::IRRELEVANT, {-1, -3, -5});
    ASSERT_EQUAL(server.GetDocumentCount(), 3);

    // Убеждаемся, что можно найти только документы с четным id
    {
        const std::vector<Document>& found_docs = server.FindTopDocuments(query,
                                                                          [](int document_id, [[maybe_unused]] DocumentStatus st, [[maybe_unused]] int rating) {
                                                                              return document_id % 2 == 0;});
        ASSERT_EQUAL(found_docs.size(), 1);
        ASSERT_EQUAL(found_docs.at(0).id, 2);
    }

    // Убеждаемся, что можно ничего не найти!
    {
        const std::vector<Document>& found_docs = server.FindTopDocuments(query,
                                                                          []([[maybe_unused]] int document_id, [[maybe_unused]] DocumentStatus st, [[maybe_unused]] int rating)
                                                                          { return false; });
        ASSERT(found_docs.empty());
    }

    // Убеждаемся, что можно найти только документы с четным рейтингом выше 0
    {
        const std::vector<Document>& found_docs = server.FindTopDocuments(query,
                                                                          []([[maybe_unused]] int document_id, [[maybe_unused]] DocumentStatus st, int rating)
                                                                          { return rating > 0; });
        ASSERT_EQUAL(found_docs.size(), 2);
        ASSERT_EQUAL(found_docs.at(0).id, 2);
        ASSERT_EQUAL(found_docs.at(1).id, 1);
    }


}

// Основная функция находится снизу, это вспомогательная
void TestDocumentsWithStatusProcess(const SearchServer& server,
                                    const std::string& query,
                                    int id,
                                    DocumentStatus status) {
    const auto docs = server.FindTopDocuments(query, status);
    ASSERT_EQUAL(docs.size(), 1);
    ASSERT_EQUAL(docs.at(0).id, id);
}

// Тест поиска документа с заданным статусом
void TestDocumentsWithStatus() {
    const std::string query = "cat in the city"s;
    SearchServer server("in the"sv);

    server.AddDocument(11, query, DocumentStatus::ACTUAL, {0, 5, 10});
    server.AddDocument(21, query, DocumentStatus::BANNED, {-5, 0, 35});
    server.AddDocument(31, query, DocumentStatus::IRRELEVANT, {-2, -1, 0});

    // Проверяем, что ничего не будет найдено по несуществующему статусу
    const auto status_does_not_exist = server.FindTopDocuments(query, DocumentStatus::REMOVED);
    ASSERT(status_does_not_exist.empty());

    server.AddDocument(41, query, DocumentStatus::REMOVED, {-7, -3, -5});

    // Проверяем последовательно каждый статус
    TestDocumentsWithStatusProcess(server, query, 11, DocumentStatus::ACTUAL);
    TestDocumentsWithStatusProcess(server, query, 21, DocumentStatus::BANNED);
    TestDocumentsWithStatusProcess(server, query, 31, DocumentStatus::IRRELEVANT);
    TestDocumentsWithStatusProcess(server, query, 41, DocumentStatus::REMOVED);
}

// Тест на корректное вычисление релевантности найденных документов
void TestRelevanceValue() {
    SearchServer search_server("и в на"sv);
    const std::string query = "пушистый ухоженный кот"s;
    const double delta = 1e-6;

    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {0});

    std::vector<double> res = {0.650672, 0.274653, 0.101366};
    const auto found_docs = search_server.FindTopDocuments(query);
    for (size_t i = 0; i < found_docs.size(); ++i)
        ASSERT(EqualNumbers(found_docs.at(i).relevance, res.at(i), delta));
}

std::string GenerateWord(std::mt19937& generator, int max_length) {
    const int length = std::uniform_int_distribution(1, max_length)(generator);
    std::string word;
    word.reserve(length);
    for (int i = 0; i < length; ++i) {
        word.push_back(std::uniform_int_distribution('a', 'z')(generator));
    }
    return word;
}

std::vector<std::string> GenerateDictionary(std::mt19937& generator, int word_count, int max_length) {
    std::vector<std::string> words;
    words.reserve(word_count);
    for (int i = 0; i < word_count; ++i) {
        words.push_back(GenerateWord(generator, max_length));
    }
    sort(words.begin(), words.end());
    words.erase(unique(words.begin(), words.end()), words.end());
    return words;
}

std::string GenerateQuery(std::mt19937& generator, const std::vector<std::string>& dictionary, int word_count, double minus_prob) {
    std::string query;
    for (int i = 0; i < word_count; ++i) {
        if (!query.empty()) {
            query.push_back(' ');
        }
        if (std::uniform_real_distribution<>(0, 1)(generator) < minus_prob) {
            query.push_back('-');
        }
        query += dictionary[std::uniform_int_distribution<int>(0, dictionary.size() - 1)(generator)];
    }
    return query;
}

std::vector<std::string> GenerateQueries(std::mt19937& generator, const std::vector<std::string>& dictionary, int query_count, int max_word_count) {
    std::vector<std::string> queries;
    queries.reserve(query_count);
    for (int i = 0; i < query_count; ++i) {
        queries.push_back(GenerateQuery(generator, dictionary, max_word_count));
    }
    return queries;
}

void TestQueriesProcessor() {
    std::cerr << std::endl;
    std::mt19937 generator;
    const auto dictionary = GenerateDictionary(generator, 10000, 25);
    const auto documents = GenerateQueries(generator, dictionary, 100'000, 10);

    SearchServer search_server(dictionary[0]);
    for (size_t i = 0; i < documents.size(); ++i) {
        search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
    }

    const auto queries = GenerateQueries(generator, dictionary, 10'000, 7);
    TEST_QUERIES_PROCESSOR(ProcessQueries);
}

void TestParallelRemoveDocument() {
    std::cerr << std::endl;
    std::mt19937 generator;

    const auto dictionary = GenerateDictionary(generator, 10000, 25);
    const auto documents = GenerateQueries(generator, dictionary, 10'000, 100);

    SearchServer search_server(dictionary[0]);
    for (size_t i = 0; i < documents.size(); ++i) {
        search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
    }

    TEST_PARALLEL_REMOVE(seq);
    TEST_PARALLEL_REMOVE(par);
}

void TestParallelMatchDocument()  {
    std::cerr << std::endl;
    std::mt19937 generator;

    const auto dictionary = GenerateDictionary(generator, 1000, 10);
    const auto documents = GenerateQueries(generator, dictionary, 10'000, 70);

    const std::string query = GenerateQuery(generator, dictionary, 500, 0.1);

    SearchServer search_server(dictionary[0]);
    for (size_t i = 0; i < documents.size(); ++i) {
        search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
    }

    TEST_PARALLEL_MATCH_DOCUMENT(seq);
    TEST_PARALLEL_MATCH_DOCUMENT(par);
}

void TestParallelFindTopDocuments() {
    std::cerr << std::endl;
    std::mt19937 generator;

    const auto dictionary = GenerateDictionary(generator, 1000, 10);
    const auto documents = GenerateQueries(generator, dictionary, 10'000, 70);

    SearchServer search_server(dictionary[0]);
    for (size_t i = 0; i < documents.size(); ++i) {
        search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
    }

    const auto queries = GenerateQueries(generator, dictionary, 100, 70);

    TEST_PARALLEL_FIND_DOCUMENTS(seq);
    TEST_PARALLEL_FIND_DOCUMENTS(par);
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestAddDocument);
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestMinusWords);
    RUN_TEST(TestMatchDocument);
    RUN_TEST(TestSortRelevance);
    RUN_TEST(TestRating);
    RUN_TEST(TestFilterPredicate);
    RUN_TEST(TestDocumentsWithStatus);
    RUN_TEST(TestRelevanceValue);
    RUN_TEST(TestQueriesProcessor);
    RUN_TEST(TestParallelRemoveDocument);
    RUN_TEST(TestParallelMatchDocument);
    RUN_TEST(TestParallelFindTopDocuments);
}