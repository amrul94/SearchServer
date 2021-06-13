#include "string_processing.h"

using std::string;

// Разделяет строку на отдельные слова и возвращает их в векторе
std::vector<std::string_view> SplitIntoWords(std::string_view str) {
    std::vector<std::string_view> result;
    const int64_t pos_end = std::string_view::npos;
    while (true) {
        int64_t space = str.find(' ');
        result.push_back(str.substr(0, space));
        str.remove_prefix(space + 1);
        if (space == pos_end) {
            break;
        }
    }
    return result;
}
