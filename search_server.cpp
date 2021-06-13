#include <cmath>
#include <iterator>

#include "document.h"
#include "search_server.h"
#include "string_processing.h"

using namespace std::literals;

using std::cout;
using std::endl;
using std::exception;
using std::string;
using std::string_view;
using std::vector;

SearchServer::SearchServer(const string& stop_words_text)
        : SearchServer(SplitIntoWords(stop_words_text)) {
}

SearchServer::SearchServer(string_view stop_words_text)
        : SearchServer(SplitIntoWords(stop_words_text)) {
}

void SearchServer::AddDocument(int document_id, string_view document, DocumentStatus status, const vector<int>& ratings) {
    if (document_id < 0) {
        throw std::invalid_argument("Попытка добавить документ с отрицательным id");
    }
    if (documents_.count(document_id) > 0) {
        throw std::invalid_argument("Попытка добавить документ c id ранее добавленного документа");
    }

    const auto [it, inserted] = documents_.emplace(document_id, DocumentData {ComputeAverageRating(ratings), status, string{document}});
    const auto words = SplitIntoWordsNoStop(it->second.text);

    const double inv_word_count = 1.0 / words.size();
    for (string_view word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        document_to_word_freqs_[document_id][word] += inv_word_count;
    }
    document_ids_.insert(document_id);
}

// Поиск наиболее релевантных документов по статусу
vector<Document> SearchServer::FindTopDocuments(string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(raw_query,
                            [status]([[maybe_unused]] int document_id, DocumentStatus document_status, [[maybe_unused]] int rating) {
                                return document_status == status;
    });
}

// Поиск наиболее релевантных документов по статусу. Последовательная версия
[[nodiscard]] std::vector<Document>
SearchServer::FindTopDocuments(const std::execution::sequenced_policy&, std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(std::execution::seq, raw_query,
                            [status]([[maybe_unused]] int document_id, DocumentStatus document_status, [[maybe_unused]] int rating) {
                                return document_status == status;
                            });
}

// Поиск наиболее релевантных документов по статусу. Параллельная версия
[[nodiscard]] std::vector<Document>
SearchServer::FindTopDocuments(const std::execution::parallel_policy&, std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(std::execution::par, raw_query,
                            [status]([[maybe_unused]] int document_id, DocumentStatus document_status, [[maybe_unused]] int rating) {
                                return document_status == status;
                            });
}


std::set<int>::const_iterator SearchServer::begin() const {
    return document_ids_.cbegin();
}

std::set<int>::const_iterator SearchServer::end() const {
    return document_ids_.cend();
}

// Возвращает количество документов на сервере
int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

// Метод получения частот слов по id документа
std::map<std::string_view, double> SearchServer::GetWordFrequencies(int document_id) const {
    static const std::map<string_view, double> empty_map;
    const auto found = document_to_word_freqs_.find(document_id);
    return (found != document_to_word_freqs_.end()) ? found->second : empty_map;
}

// Возвращает все слова из поискового запроса, присутствующие в документе.
[[nodiscard]] SearchServer::MatchDocumentResult SearchServer::MatchDocument(string_view raw_query, int document_id) const {
    return MatchDocument(std::execution::seq, raw_query, document_id);
}

// Возвращает все слова из поискового запроса, присутствующие в документе.
// Последовательная версия
[[nodiscard]] SearchServer::MatchDocumentResult SearchServer::MatchDocument(const std::execution::sequenced_policy&, string_view raw_query, int document_id) const {
    const Query query = ParseQuery(raw_query);
    vector<std::string_view> matched_words;
    for (string_view word : query.plus_words) {
        auto found_documents = word_to_document_freqs_.find(std::string(word));
        if (found_documents == word_to_document_freqs_.end()) {
            continue;
        }
        if (found_documents->second.count(document_id)) {
            matched_words.emplace_back(word);
        }
    }
    for (string_view word : query.minus_words) {
        auto found_documents = word_to_document_freqs_.find(std::string(word));
        if (found_documents == word_to_document_freqs_.end()) {
            continue;
        }
        if (found_documents->second.count(document_id)) {
            matched_words.clear();
            break;
        }
    }
    return {matched_words, documents_.at(document_id).status};
}

// Возвращает все слова из поискового запроса, присутствующие в документе.
// Параллельная версия
[[nodiscard]] SearchServer::MatchDocumentResult SearchServer::MatchDocument(const std::execution::parallel_policy&, string_view raw_query, int document_id) const {
    const Query query = ParseQuery(raw_query);
    std::vector<string_view> matched_words;

    const auto word_checker = [this, document_id](string_view word) {
        const auto found = word_to_document_freqs_.find(word);
        return found != word_to_document_freqs_.end() && found->second.count(document_id);
    };

    if (std::any_of(std::execution::par, query.minus_words.begin(), query.minus_words.end(), word_checker)) {
        return { matched_words, documents_.at(document_id).status };
    }

    std::copy_if(
        std::execution::par,
        query.plus_words.begin(), query.plus_words.end(),
        std::back_inserter(matched_words),
        word_checker
    );

    return { matched_words, documents_.at(document_id).status};
}

// Удаление документов из поискового сервера
void SearchServer::RemoveDocument(int document_id) {
    this->RemoveDocument(std::execution::par, document_id);
}

// Удаление документов из поискового сервера
// Последовательная версия
void SearchServer::RemoveDocument(const std::execution::sequenced_policy&, int document_id) {
    const auto& found = GetWordFrequencies(document_id);
    if (!found.empty()) {
        for (const auto& [word, freq] : found) {
            word_to_document_freqs_[string(word)].erase(document_id);
        }
        document_to_word_freqs_.erase(document_id);
        document_ids_.erase(document_id);
        documents_.erase(document_id);
    }
}

// Удаление документов из поискового сервера
// Параллельная версия
void SearchServer::RemoveDocument(const std::execution::parallel_policy&, int document_id) {
    const auto& found = GetWordFrequencies(document_id);
    if (!found.empty()) {
        std::for_each(std::execution::par, found.begin(), found.end(),
              [this, document_id](auto& word_freq) {
                  word_to_document_freqs_[word_freq.first].erase(document_id);
              });
        document_to_word_freqs_.erase(document_id);
        document_ids_.erase(document_id);
        documents_.erase(document_id);
    }
}


// Реализация private методов класса SearchServer
// Проверка на стоп-слова
bool SearchServer::IsStopWord(const string& word) const {
    return stop_words_.count(word) > 0;
}

// Проверка на наличие в слове спец-символов
bool SearchServer::IsValidWord(string_view word) {
    // A valid word must not contain special characters
    return std::none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}

// Разделяет строку на отдельные слова и возвращает их в векторе, исключая стоп-слова
vector<string_view> SearchServer::SplitIntoWordsNoStop(string_view text) const {
    vector<string_view> words;
    for (string_view word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument("Наличие недопустимых символов (с кодами от 0 до 31) в тексте добавляемого документа");
        }
        if (!IsStopWord(string(word))) {
            words.emplace_back(word);
        }
    }
    return words;
}

// Вычисление среднего рейтинга
int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = 0;
    for (const int rating : ratings) {
        rating_sum += rating;
    }
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(string_view text) const {
    if (text.empty()) {
        throw std::invalid_argument("В тексте запроса нет слов");
    }
    bool is_minus = false;
    if (text[0] == '-') {
        is_minus = true;
        text.remove_prefix(1);
    }
    if (text.empty())
        throw std::invalid_argument("Отсутствие текста после символа «минус»: в поисковом запросе");
    if (text[0] == '-')
        throw std::invalid_argument("Наличие более чем одного минуса перед словами, которых не должно быть в искомых документах");
    if (!IsValidWord(text))
        throw std::invalid_argument("Отсутствие текста после символа «минус»: в поисковом запросе");

    return {text, is_minus, IsStopWord(string(text))};
}

SearchServer::Query SearchServer::ParseQuery(string_view text) const {
    Query query;
    for (string_view word : SplitIntoWords(text)) {
        const QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query.minus_words.insert(query_word.data);
            } else {
                query.plus_words.insert(query_word.data);
            }
        }
    }
    return query;
}

double SearchServer::ComputeWordInverseDocumentFreq(string_view word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}


// Реализация вспомогательных функций для обработки исключений
// В некоторых примерах используются эти функции
void ServerCreation() {
    try {
        [[maybe_unused]] SearchServer search_server("и в на\x12"s);
    }
    catch (const exception& e) {
        cout << "Ошибка в создании документа: "s << e.what() << endl;
    }
}

void AddDocument(SearchServer& search_server, int document_id, const string& document, DocumentStatus status,
                 const vector<int>& ratings) {
    try {
        search_server.AddDocument(document_id, document, status, ratings);
    } catch (const exception& e) {
        cout << "Ошибка добавления документа "s << document_id << ": "s << e.what() << endl;
    }
}

void FindTopDocuments(const SearchServer& search_server, const string& raw_query) {
    cout << "Результаты поиска по запросу: "s << raw_query << std::endl;
    try {
        for (const Document& document : search_server.FindTopDocuments(raw_query)) {
            cout << document << endl;
        }
    } catch (const exception& e) {
        cout << "Ошибка поиска: "s << e.what() << endl;
    }
}

void PrintMatchDocumentResult(int document_id, const std::vector<std::string_view>& words, DocumentStatus status) {
    std::cout << "{ "s
              << "document_id = "s << document_id << ", "s
              << "status = "s << static_cast<int>(status) << ", "s
              << "words ="s;
    for (std::string_view word : words) {
        std::cout << ' ' << word;
    }
    std::cout << "}"s << std::endl;
}

void MatchDocuments(const SearchServer& search_server, const string& query) {
    try {
        cout << "Матчинг документов по запросу: "s << query << endl;
        const int document_count = search_server.GetDocumentCount();
        for (int index = 0; index < document_count; ++index) {
            const int document_id = *(std::next(search_server.begin(), index));
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    } catch (const exception& e) {
        cout << "Ошибка матчинга документов на запрос "s << query << ": "s << e.what() << endl;
    }
}