#include <vector>
#include <iostream>

template<typename T, typename Comp = std::less<T>,
    typename Requires = std::void_t<decltype(T{}), decltype(Comp{}), decltype(Comp{}(std::declval<T>(), std::declval<T>())) > >
class BinaryHeap {
private:
    static constexpr size_t MIN_INIT_SIZE{ 1024 };
    std::vector<T> heap;
    Comp comp{};

protected:
    void build_heap() {
        for (size_t i = heap.size() - 1; i >= 0; i--) {
            size_t cur{ i };
            while (2 * cur + 1 < heap.size()) {
                size_t son{ 2 * cur + 1 };
                if (son + 1 < heap.size() && comp(heap[son], heap[son + 1])) {
                    ++son;
                }
                if (comp(heap[cur], heap[son])) {
                    using std::swap;
                    swap(heap[cur], heap[son]);
                    cur = son;
                }
                else {
                    break;
                }
            }
            if (i == 0) break;
        }
    }

public:

    BinaryHeap() :
        heap(0, T{}) {
        heap.reserve(MIN_INIT_SIZE);
    }

    template<typename Size, typename T2, typename Requires2 = std::void_t<decltype(T{ std::declval<T2>() }), std::enable_if_t<std::is_integral_v<Size>> > >
    BinaryHeap(Size init_size, T2 init_val) :
        heap(init_size, init_val) {
        heap.reserve(std::min(static_cast<size_t>(init_size), MIN_INIT_SIZE));
    }

    template<typename... Args>
    BinaryHeap(Args&&... args) :
        heap{ std::forward<Args>(args)... } {
        heap.reserve(std::max(sizeof...(args), MIN_INIT_SIZE));
        build_heap();
    }

    std::vector<T>& interface() {
        return &heap;
    }

    void push(const T& obj) {
        heap.push_back(obj);
        size_t cur{ heap.size() - 1 };
        while (cur > 0 && comp(heap[(cur - 1) / 2], heap[cur])) {
            using std::swap;
            swap(heap[(cur - 1) / 2], heap[cur]);
            cur = (cur - 1) / 2;
        }
    }

    void pop() {
        if (heap.empty()) return;
        heap[0] = heap.back();
        heap.pop_back();
        size_t cur{};
        while (2 * cur + 1 < heap.size()) {
            size_t son{ 2 * cur + 1 };
            if (son + 1 < heap.size() && comp(heap[son], heap[son + 1])) {
                ++son;
            }
            if (comp(heap[cur], heap[son])) {
                using std::swap;
                swap(heap[cur], heap[son]);
                cur = son;
            }
            else {
                break;
            }
        }
    }

    const T& front() const {
        return heap.front();
    }

};