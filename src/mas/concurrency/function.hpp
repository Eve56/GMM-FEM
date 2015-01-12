#include <tuple>
#include <functional>
// #include <future>
#include <memory>
#include <iostream>

namespace mas {
namespace concurrency {

namespace type_traits {

template <typename T1, typename... T>
struct first_type {
	typedef T1 type;
};

template<>
struct first_type<void> {
	typedef void type;
};

template<typename T>
struct target_type {
	typedef void type;
};

template<typename Class, typename Member>
struct target_type<Member Class::*> {
	typedef Class type;
};

template<typename>
  struct is_smart_pointer_helper : public std::false_type { };

template<typename T>
  struct is_smart_pointer_helper<std::unique_ptr<T>>  : public std::true_type { };

template<typename T>
  struct is_smart_pointer_helper<std::shared_ptr<T>>  : public std::true_type { };

/// is_smart_pointer
template<typename T>
  struct is_smart_pointer : public std::integral_constant<bool, (is_smart_pointer_helper<typename
				      std::remove_cv<T>::type>::value)>  { };

template<typename T, typename Enable = void>
struct remove_pointer {
	typedef T type;
};

template<typename T>
struct remove_pointer<T, typename std::enable_if<std::is_pointer<T>::value ||
	type_traits::is_smart_pointer<T>::value>::type> {
	typedef typename std::pointer_traits<T>::element_type type;

};

template <typename Object, typename Pointer>
struct is_target_pointer : public std::integral_constant<bool,
		std::is_pointer<Object>::value
		&& std::is_base_of<typename target_type<Pointer>::type,
			typename remove_pointer<typename std::decay<Object>::type>::type>::value> {
};

//is reference to pointer target or derived
template<typename Object, typename Pointer>
struct is_target_reference: public std::integral_constant<bool,
		std::is_reference<Object>::value
				&& std::is_base_of<typename target_type<Pointer>::type,
						typename std::decay<Object>::type>::value> {
};


template <typename Object, typename Pointer>
struct is_target_smart_pointer : public std::integral_constant<bool,
		is_smart_pointer<Object>::value
		&& std::is_base_of<typename target_type<Pointer>::type,
			typename remove_pointer<typename std::decay<Object>::type>::type>::value> {
};

template <typename T>
struct ERROR {
	typedef typename T::error_here error_type;
	error_type error;
};

}

// Member function, by reference
template<typename Functor, typename Object, typename ... Args>
inline typename std::enable_if<
		std::is_member_function_pointer<typename std::decay<Functor>::type>::value
				&& type_traits::is_target_reference<Object&&,
						typename std::decay<Functor>::type>::value,

		typename std::result_of<Functor(Object,Args&&...)>::type>::type invoke(
		Functor&& functor, Object&& object, Args&&... args) {
	// std::cout << "MEMBER FUNCTION, BY REFERENCE" << std::endl;
	return (object.*functor)(std::forward<Args>(args)...);
}

// Member function, by pointer
template<typename Functor, typename Object, typename ... Args>
inline typename std::enable_if<
		std::is_member_function_pointer<typename std::decay<Functor>::type>::value
			&& !type_traits::is_target_reference<Object&&,
				typename std::decay<Functor>::type>::value,

		typename std::result_of<Functor(Object,Args&&...)>::type>::type invoke(
		Functor&& functor, Object&& object,	Args&&... args) {
	// std::cout << "MEMBER FUNCTION, BY POINTER" << std::endl;
	return ((*std::forward<Object>(object)).*functor)(	std::forward<Args>(args)...);
}

// Member object, by reference
template<typename Functor, typename Object>
inline typename std::enable_if<
		std::is_member_object_pointer<typename std::decay<Functor>::type>::value
			&& type_traits::is_target_reference<Object&&,
				typename std::decay<Functor>::type>::value,
		typename std::result_of<Functor(Object)>::type>::type
		invoke(Functor&& functor, Object&& object) {
	// std::cout << "MEMBER OBJECT, BY REFERENCE" << std::endl;
	return object.*functor;
}

// member object, by pointer
template<typename Functor, typename Object>
inline typename std::enable_if<
		std::is_member_object_pointer<typename std::decay<Functor>::type>::value
			&& !type_traits::is_target_reference<Object&&,
				typename std::decay<Functor>::type>::value,
		typename std::result_of<Functor(Object)>::type>::type
		invoke(Functor&& functor, Object&& object) {
	// std::cout << "MEMBER OBJECT, BY POINTER" << std::endl;
	return (*std::forward<Object>(object)).*functor;
}

// UNKNOWN!!!
template<typename Functor, typename ... Args>
inline typename std::enable_if<
		std::is_member_pointer<typename std::decay<Functor>::type>::value,
		typename std::result_of<Functor(Args...)>::type>::type
		invoke(Functor&& functor, Args&&... args) {
	std::cout << "WHAT IS THIS?" << std::endl;
	return std::forward<Functor>(functor)(std::forward<Args>(args)...);
}

// Function/objects with operator(...)
template<typename Functor, typename ... Args>
inline typename std::enable_if<
		!std::is_member_pointer<typename std::decay<Functor>::type>::value,
		typename std::result_of<Functor&(Args&&...)>::type>::type
		invoke(Functor&& functor, Args&&... args) {
	// std::cout << "REGULAR FUNCTION" << std::endl;
	return std::forward<Functor>(functor)(std::forward<Args>(args)...);
}

// Index tuple for accessing arguments
template<std::size_t ... Indexes>
struct index_tuple {
	typedef index_tuple<Indexes..., sizeof...(Indexes)> next;
};

// Builds an index_tuple<0, 1, 2, ..., Num-1>.
template<std::size_t Num>
struct build_index_tuple {
	typedef typename build_index_tuple<Num - 1>::type::next type;
};

template<>
struct build_index_tuple<0> {
	typedef index_tuple<> type;
};

template<typename Signature>
class function_wrapper;

template<typename Fn, typename ... Args>
class function_wrapper<Fn(Args...)> {
public:
	typedef typename std::result_of<Fn(Args...)>::type result_type;

private:
	std::tuple<Fn, Args...> bound;

	template<std::size_t ... Indices>
	result_type invoke(index_tuple<Indices...>) {
		// std::bind always forwards bound arguments as lvalues,
		// but this type can call functions which only accept rvalues.
		return mas::concurrency::invoke(std::forward<Fn>(std::get<0>(bound)), std::forward<Args>(std::get<Indices+1>(bound))...);
	}

public:

	template<typename ... Args2, typename = typename std::enable_if<sizeof...(Args) == sizeof...(Args2)>::type>
	explicit function_wrapper(const Fn& fn, Args2&&... args) :
			bound(fn, std::forward<Args2>(args)...) {
	}

	template<typename... Args2, typename = typename std::enable_if< sizeof...(Args) == sizeof...(Args2)>::type>
	explicit function_wrapper(Fn&& fn, Args2&&... args) :
			bound(std::move(fn), std::forward<Args2>(args)...) {
	}

	// no copy, only move
	function_wrapper(const function_wrapper&) = delete;
	function_wrapper(function_wrapper&&) = default;

	result_type operator()() {
		return invoke();
	}

	result_type invoke() {
		typedef typename build_index_tuple<sizeof...(Args)>::type Indices;
		return invoke(Indices());
	}
};

//template<typename Fn, typename ... Args>
//class future_function_wrapper {
//	typedef typename std::result_of<Fn(Args...)>::type result_type;
//
//private:
//	std::tuple<Fn, Args&&...> bound;
//	std::promise<result_type> promise;
//
//	template<std::size_t ... Indices>
//	void invoke(index_tuple<Indices...>) {
//		// std::bind always forwards bound arguments as lvalues,
//		// but this type can call functions which accept rvalues.
//		//return std::forward<Fn>(std::get<0>(bound))(std::forward<Args>(std::get<Indices+1>(bound))...);
//		promise.set_value(
//				mas::concurrency::invoke(std::forward<Fn>(std::get<0>(bound)),std::forward<Args>(std::get<Indices+1>(bound))...));
//	}
//
//public:
//	explicit future_function_wrapper(const Fn& fn, Args... args) :
//		bound(fn, std::forward<Args>(args)...) {
//	}
//
//	explicit future_function_wrapper(Fn&& fn, Args&&... args) :
//			bound(std::move(fn), std::forward<Args>(args)...) {
//	}
//
//	future_function_wrapper(const future_function_wrapper&) = delete;
//	future_function_wrapper(future_function_wrapper&&) = default;
//
//	void operator()() {
//		invoke();
//	}
//
//	void invoke() {
//		typedef typename build_index_tuple<sizeof...(Args)>::type Indices;
//		invoke(Indices());
//	}
//
//	std::future<result_type> get_future() {
//		return promise.get_future();
//	}
//
//};

template<typename Tp>
struct maybe_wrap_function_pointer {
    typedef Tp type;

    static const Tp&  do_wrap(const Tp& x) {
        return x;
    }

    static Tp&& do_wrap(Tp&& x) {
        return static_cast<Tp&&>(x);
    }
};

template<>
struct maybe_wrap_function_pointer<void> {
    typedef void type;
};

template<typename Fn, typename... Args>
struct function_wrapper_t {
    typedef maybe_wrap_function_pointer<typename std::decay<Fn>::type> maybe_type;
    typedef typename maybe_type::type function_type;
    typedef function_wrapper<function_type(typename std::decay<Args>::type...)> type;
    typedef typename type::result_type result_type;

};

template<typename Fn, typename ... Args>
void print_type_info(Fn&& fn, Args&&... args) {
	typedef typename type_traits::first_type<Args...>::type object_type;
	typedef typename std::decay<Fn>::type ptr_type;
	std::cout << "Member pointer: " << std::is_member_pointer<ptr_type>::value << std::endl;
	std::cout << "Object: " << std::is_object<object_type>::value << std::endl;
	std::cout << "Object pointer: " << std::is_pointer<object_type>::value << std::endl;
	std::cout << "Object LValue reference: " << std::is_lvalue_reference<object_type>::value << std::endl;
	std::cout << "Object RValue reference: " << std::is_rvalue_reference<object_type>::value << std::endl;
	std::cout << "Object reference: " << std::is_reference<object_type>::value << std::endl;
	std::cout << "Object smart pointer: " << type_traits::is_smart_pointer<object_type>::value << std::endl;

	std::cout << "Object target pointer: " << type_traits::is_target_pointer<object_type, ptr_type>::value << std::endl;
	std::cout << "Object target reference: " << type_traits::is_target_reference<object_type, ptr_type>::value << std::endl;
	std::cout << "Object target smart pointer: " << type_traits::is_target_smart_pointer<object_type, ptr_type>::value << std::endl;

	//type_traits::ERROR<object_type> v;
}

template<typename Fn, typename ... Args>
typename function_wrapper_t<Fn,Args...>::type wrap(Fn&& fn, Args&&... args) {
	typedef function_wrapper_t<Fn, Args...> helper_type;
	typedef typename helper_type::maybe_type maybe_type;
	typedef typename helper_type::type wrapper_type;
	typedef typename type_traits::first_type<Args...>::type object_type;

	static_assert(
			!(std::is_member_pointer<typename std::decay<Fn>::type>::value)
			|| (std::is_pointer<object_type>::value)
			|| (type_traits::is_smart_pointer<object_type>::value),
			"For class members, the supplied object must be a pointer-type");

	return wrapper_type(
			maybe_type::do_wrap(std::forward<Fn>(fn)),
			std::forward<Args>(args)...);
}

template<typename Fn, typename ... Args>
std::unique_ptr<typename function_wrapper_t<Fn,Args...>::type> make_unique_wrapper(Fn&& fn, Args&&... args) {
	typedef function_wrapper_t<Fn, Args...> helper_type;
	typedef typename helper_type::maybe_type maybe_type;
	typedef typename helper_type::type wrapper_type;
	typedef typename type_traits::first_type<Args...>::type object_type;

	static_assert(
			!(std::is_member_pointer<typename std::decay<Fn>::type>::value)
			|| (std::is_pointer<object_type>::value)
			|| (type_traits::is_smart_pointer<object_type>::value),
			"For class members, the supplied object must be a pointer-type");

	return std::unique_ptr<typename function_wrapper_t<Fn,Args...>::type>(new wrapper_type(
			maybe_type::do_wrap(std::forward<Fn>(fn)),
			std::forward<Args>(args)...));
}

//template<typename Fn, typename ... Args>
//typename future_function_wrapper_t<Fn,Args...>::type wrap_future(Fn&& fn, Args&&... args) {
//	typedef future_function_wrapper_t<Fn, Args...> helper_type;
//	typedef typename helper_type::maybe_type maybe_type;
//	typedef typename helper_type::type result_type;
//	typedef typename type_traits::first_type<Args...>::type object_type;
//
//	static_assert(
//			!(std::is_member_pointer<typename std::decay<Fn>::type>::value)
//			|| (std::is_pointer<object_type>::value)
//			|| (type_traits::is_smart_pointer<object_type>::value),
//			"For class members, the supplied object must be a pointer-type");
//
//	return result_type(
//			maybe_type::do_wrap(std::forward<Fn>(fn)),
//			std::forward<Args>(args)...);
//}
//
//template<typename Fn, typename ... Args>
//std::unique_ptr<typename future_function_wrapper_t<Fn,Args...>::type> make_unique_future_wrapper(Fn&& fn, Args&&... args) {
//	typedef future_function_wrapper_t<Fn, Args...> helper_type;
//	typedef typename helper_type::maybe_type maybe_type;
//	typedef typename helper_type::type result_type;
//	typedef typename type_traits::first_type<Args...>::type object_type;
//
//	static_assert(
//			!(std::is_member_pointer<typename std::decay<Fn>::type>::value)
//			|| (std::is_pointer<object_type>::value)
//			|| (type_traits::is_smart_pointer<object_type>::value),
//			"For class members, the supplied object must be a pointer-type");
//
//	return std::unique_ptr<typename future_function_wrapper_t<Fn,Args...>::type>(new result_type(
//			maybe_type::do_wrap(std::forward<Fn>(fn)),
//			std::forward<Args>(args)...));
//}

}
}
