#include <iostream>
using namespace std;
int main() {
	string in{};
	cin >> in;
	if (in == "hello") {
		cout << "Hello world!";
	} else if (in == "nihao") {
		cout << "Nihao world!";
	} else {
		cout << "byebye!";
	}
	return 0;
}

