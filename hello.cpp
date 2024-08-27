#include <iostream>
#include "BinaryHeap.h"
using namespace std;
int main() {

	BinaryHeap<int, less<int>> heap{};
	for (int i = 1; i <= 20; i++) {
		heap.push(i);
	}
	for (int i = 1; i <= 20; i++) {
		cout << heap.front() << endl;
		heap.pop();
	}

}

