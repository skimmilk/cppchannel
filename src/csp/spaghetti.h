// Testing shows that for wordstack, this is the optimal value
#define CSP_CACHE_DEFAULT 32

#define CSP_DECL_CONTAINER(fn_name, input, output, ...)\
	class _##fn_name##_t_ : public\
			csp::channel<input,output,##__VA_ARGS__>\
	{\
	public:\
		void run(__VA_ARGS__);\
	};
/*template <typename t_in>
csp::channel<t_in, t_in, CSP_CACHE_DEFAULT, bool> sort(bool a = false)
	return csp::chan_create<t_in, t_in, sort_t_<t_in>, CSP_CACHE_DEFAULT, bool>(a);*/


#define CSP_DECL_TEMPL_INIT(fn_name,templed_name,input,output,...)\
	csp::shared_ptr<csp::channel<input,output,##__VA_ARGS__>> (*fn_name)(__VA_ARGS__) =\
	csp::chan_create<input,output,templed_name,##__VA_ARGS__>;

#define CSP_DECL_INITIALIZER(fn_name,input,output,...)\
	csp::shared_ptr<csp::channel<input,output,##__VA_ARGS__>> (*fn_name)(__VA_ARGS__) =\
	csp::chan_create<input,output,_##fn_name##_t_,##__VA_ARGS__>;

#define CSP_DECL(fn_name,input,output,...)\
	CSP_DECL_CONTAINER(fn_name,input,output,##__VA_ARGS__)\
	CSP_DECL_INITIALIZER(fn_name,input,output,##__VA_ARGS__)\
void _##fn_name##_t_ ::run

// jesus christ how horrifying

// http://stackoverflow.com/questions/10766112/c11-i-can-go-from-multiple-args-to-tuple-but-can-i-go-from-tuple-to-multiple
// implementation details, users never invoke these directly
namespace detail
{
    template <typename F, typename Tuple, bool Done, int Total, int... N>
    struct call_impl
    {
        static void call(F f, Tuple && t)
        {
            call_impl<F, Tuple, Total == 1 + sizeof...(N), Total, N..., sizeof...(N)>::call(f, std::forward<Tuple>(t));
        }
    };

    template <typename F, typename Tuple, int Total, int... N>
    struct call_impl<F, Tuple, true, Total, N...>
    {
        static void call(F f, Tuple && t)
        {
            f(std::get<N>(std::forward<Tuple>(t))...);
        }
    };
}

// user invokes this
template <typename F, typename Tuple>
void call(F f, Tuple && t)
{
    typedef typename std::decay<Tuple>::type ttype;
    detail::call_impl<F, Tuple, 0 == std::tuple_size<ttype>::value, std::tuple_size<ttype>::value>::call(f, std::forward<Tuple>(t));
}
