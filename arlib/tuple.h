#pragma once
#include "global.h"

template<typename Thead, typename... Ttail> class tuple_impl {
	template<typename...> friend class tuple;
	template<typename, typename...> friend class tuple_impl;
	
	Thead head;
	tuple_impl<Ttail...> tail;
	
	tuple_impl(Thead head, Ttail... tails) : head(head), tail(tails...) {}
};

template<typename Thead> class tuple_impl<Thead> {
	template<typename...> friend class tuple;
	template<typename, typename...> friend class tuple_impl;
	
	Thead head;
	tuple_impl(Thead head) : head(head) {}
};

template<class... Ts> class tuple {
	tuple_impl<Ts...> m_data;
public:
	tuple(Ts... args) : m_data(args...) {}
};

template<> class tuple<> {};
