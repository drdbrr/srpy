#include <iostream>
#include <atomic>

template<class T>
class LFS {
public:
	struct node {
		node(const T& data) : data(data), next(nullptr) {};
	    T data;
	    node<T>* next;
	};

	void push(const T& val) {
		node<T>* new_node = new node<T>(val);
		new_node->next = _head.load();
		while(!_head.compare_exchange_weak(new_node->next, new_node));
	}

	bool pop() {
		node<T>* got = _head.load();
		node<T>* nextn = nullptr;
		do {
			if(got == nullptr)
				return false;

			nextn = got->next;
		} while(!_head.compare_exchange_weak(got, nextn));
		delete got;
		return true;
	}
private:
	std::atomic<node<T>*> _head;
}
