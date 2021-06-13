#pragma once

#include <iostream>
#include <vector>

template <typename Iterator>
class IteratorRange {
public:
    IteratorRange(Iterator begin, Iterator end)
            : first_(begin)
            , last_(end)
            , size_(distance(first_, last_)) {
    }

    [[nodiscard]] Iterator begin() const {
        return first_;
    }

    [[nodiscard]] Iterator end() const {
        return last_;
    }

    [[nodiscard]] size_t size() const {
        return size_;
    }

private:
    Iterator first_, last_;
    size_t size_;
};

template <typename Iterator>
std::ostream& operator<<(std::ostream& out, const IteratorRange<Iterator>& iteratorRange) {
    for (const auto& element : iteratorRange)
        out << element;
    return out;
}

template <typename Iterator>
class Paginator {
public:
    Paginator(Iterator begin, Iterator end, size_t page_size) {
        for (size_t left = distance(begin, end); left > 0;) {
            const size_t current_page_size = std::min(page_size, left);
            const Iterator current_page_end = next(begin, current_page_size);
            pages_.push_back({begin, current_page_end});

            left -= current_page_size;
            begin = current_page_end;
        }
    }
    [[nodiscard]] auto begin() const {
        return pages_.begin();
    }

    [[nodiscard]] auto end() const {
        return pages_.end();
    }

    [[nodiscard]] std::size_t size() const {
        return pages_.size();
    }

private:
    std::vector<IteratorRange<Iterator>> pages_;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}