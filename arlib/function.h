//This is <http://www.codeproject.com/Articles/136799/> Lightweight Generic C++ Callbacks (or, Yet Another Delegate Implementation)
//with a number of changes:
//- Callback was renamed to function, and the namespace was removed.
//- BIND_FREE_CB/BIND_MEM_CB were combined to a single bind(), by using the C99 preprocessor's __VA_ARGS__.
//- Instead of the thousand lines of copypasta, the implementations were merged by using some preprocessor macros and having the file include itself.
//- The Arity, ReturnType and ParamNType constants/typedefs were removed.
//- NullCallback was replaced with support for plain NULL, by implicitly casting the NULL to a pointer to a private class (default construction also exists).
//- BoundCallbackFactory and bind_ptr were added. It's useful for legacy C-like code, and some other cases.
//- Made it safe to call an unassigned object. (Unassigned objects are still false.)

//List of libraries that do roughly the same thing:
//http://www.codeproject.com/Articles/7150/ Member Function Pointers and the Fastest Possible C++ Delegates
// rejected because it uses ugly hacks which defeat the optimizer, unknown compilers, and my brain
//http://www.codeproject.com/Articles/11015/ The Impossibly Fast C++ Delegates
// rejected because creation syntax is ugly
//http://www.codeproject.com/Articles/13287/ Fast C++ Delegate
// rejected because it's huge and can allocate
//http://www.codeproject.com/Articles/18886/ A new way to implement Delegate in C++
// rejected because it depends on sizeof in creepy ways and can throw
//http://www.codeproject.com/Articles/136799/ Lightweight Generic C++ Callbacks (or, Yet Another Delegate Implementation)
// chosen because it gives the slimmest function objects - unlike the others, it's just two pointers

#ifndef UTIL_CALLBACK_HPP
#define UTIL_CALLBACK_HPP

#include <stddef.h>

#define UTIL_CALLBACK_HPP_INSIDE

#define bind_free(func) (GetFreeCallbackFactory(func).Bind<func>())
#define bind_ptr(func, ptr) (GetCallbackFactory(func, ptr).Bind<func>(ptr))
#define bind bind_free
#define bind_this(func) bind_ptr(func, this) // reminder: bind_this(&classname::function), not bind_this(function)

template<typename FuncSignature> class function;

#define JOIN2(a,b) a##b
#define JOIN(a,b) JOIN2(a,b)

#define FreeCallbackFactory JOIN(FreeCallbackFactory,COUNT)
#define MemberCallbackFactory JOIN(MemberCallbackFactory,COUNT)
#define ConstMemberCallbackFactory JOIN(ConstMemberCallbackFactory,COUNT)
#define BoundCallbackFactory JOIN(BoundCallbackFactory,COUNT)

#define ARG_TYPES_I(n) JOIN(P,n)
#define ARG_TYPES LOOP(ARG_TYPES_I)
#define ARG_NAMES_I(n) JOIN(a,n)
#define ARG_NAMES LOOP(ARG_NAMES_I)
#define ARG_TYPES_AND_NAMES_I(n) JOIN(P,n) JOIN(a,n)
#define ARG_TYPES_AND_NAMES LOOP(ARG_TYPES_AND_NAMES_I)
#define TYPENAMES_I(n) typename JOIN(P,n)
#define TYPENAMES LOOP(TYPENAMES_I)

#define TYPENAMES2_I(n) typename JOIN(FP,n)
#define TYPENAMES2 LOOP(TYPENAMES2_I)

#define COUNT 0
#define LOOP(macro) /* */
#define C /* */
#include "function.h"
#undef C

#define C ,
#define COUNT 1
#define LOOP(macro) macro(1)
#include "function.h"

#define COUNT 2
#define LOOP(macro) macro(1), macro(2)
#include "function.h"

#define COUNT 3
#define LOOP(macro) macro(1), macro(2), macro(3)
#include "function.h"

#define COUNT 4
#define LOOP(macro) macro(1), macro(2), macro(3), macro(4)
#include "function.h"

#define COUNT 5
#define LOOP(macro) macro(1), macro(2), macro(3), macro(4), macro(5)
#include "function.h"

#define COUNT 6
#define LOOP(macro) macro(1), macro(2), macro(3), macro(4), macro(5), macro(6)
#include "function.h"

#undef C
#undef JOIN2
#undef JOIN
#undef FreeCallbackFactory
#undef MemberCallbackFactory
#undef ConstMemberCallbackFactory
#undef BoundCallbackFactory
#undef ARG_TYPES_I
#undef ARG_TYPES
#undef ARG_NAMES_I
#undef ARG_NAMES
#undef ARG_TYPES_AND_NAMES_I
#undef ARG_TYPES_AND_NAMES
#undef TYPENAMES_I
#undef TYPENAMES
#undef TYPENAMES2_I
#undef TYPENAMES2

#undef UTIL_CALLBACK_HPP_INSIDE

#endif

#ifdef UTIL_CALLBACK_HPP_INSIDE
template<typename R C TYPENAMES>
class function<R (ARG_TYPES)>
{
private:
    class null_only;

    typedef R (*FuncType)(const void* C ARG_TYPES);
    function(FuncType f, const void* o) : func(f), obj(o) {}

    FuncType func;
    const void* obj;

public:
    //to make null objects callable, 'func' must be a valid function
    //I can not:
    //- use the lowest bits - requires mask at call time, and confuses the optimizer
    //- compare it to a static null function, I don't trust the compiler to merge it correctly
    //nor can I use NULL/whatever in 'obj', because foreign code can find that argument just as easily as this one can
    //solution: set obj=func=EmptyFunction for null functions
    //- EmptyFunction doesn't use obj, it can be whatever
    //- it is not sensitive to false negatives - even if the address of EmptyFunction changes, obj==func does not
    //- it is not sensitive to false positives - EmptyFunction is private and it is impossible for foreign code to know where it is, and luck can not hit it 
    //- it is sensitive to hostile callers, but if you call bind_ptr(func, (void*)func), you're asking for bugs.
    function()                    : func(EmptyHandler), obj((void*)EmptyHandler) {}
    function(const function& rhs) : func(rhs.func), obj(rhs.obj) {}
    ~function() {}

    function(const null_only*)    : func(EmptyHandler), obj((void*)EmptyHandler) {}

    function& operator=(const function& rhs)
        { obj = rhs.obj; func = rhs.func; return *this; }

    inline R operator()(ARG_TYPES_AND_NAMES) const
    {
        return (*func)(obj C ARG_NAMES);
    }

private:
    typedef const void* function::*SafeBoolType;
    bool isTrue() const
    {
      return ((void*)func != obj);
    }
public:
    inline operator SafeBoolType() const
        { return isTrue() ? &function::obj : NULL; }
    inline bool operator!() const
        { return !isTrue(); }

private:

    static R EmptyHandler(const void* o C ARG_TYPES_AND_NAMES) { return R(); }

    template<typename FR C TYPENAMES2>
    friend class FreeCallbackFactory;
    template<typename FR, class FT C TYPENAMES2>
    friend class MemberCallbackFactory;
    template<typename FR, class FT C TYPENAMES2>
    friend class ConstMemberCallbackFactory;
    template<typename FR C TYPENAMES2, typename PTR>
    friend class BoundCallbackFactory;
};

template<typename R C TYPENAMES>
void operator==(const function<R (ARG_TYPES)>&,
                const function<R (ARG_TYPES)>&);
template<typename R C TYPENAMES>
void operator!=(const function<R (ARG_TYPES)>&,
                const function<R (ARG_TYPES)>&);

template<typename R C TYPENAMES>
class FreeCallbackFactory
{
private:
    template<R (*Func)(ARG_TYPES)>
    static R Wrapper(const void* C ARG_TYPES_AND_NAMES)
    {
        return (*Func)(ARG_NAMES);
    }

public:
    template<R (*Func)(ARG_TYPES)>
    inline static function<R (ARG_TYPES)> Bind()
    {
        return function<R (ARG_TYPES)>
            (&FreeCallbackFactory::Wrapper<Func>, 0);
    }
};

template<typename R C TYPENAMES>
inline FreeCallbackFactory<R C ARG_TYPES>
GetFreeCallbackFactory(R (*)(ARG_TYPES))
{
    return FreeCallbackFactory<R C ARG_TYPES>();
}

template<typename R, class T C TYPENAMES>
class MemberCallbackFactory
{
private:
    template<R (T::*Func)(ARG_TYPES)>
    static R Wrapper(const void* o C ARG_TYPES_AND_NAMES)
    {
        T* obj = const_cast<T*>(static_cast<const T*>(o));
        return (obj->*Func)(ARG_NAMES);
    }

public:
    template<R (T::*Func)(ARG_TYPES)>
    inline static function<R (ARG_TYPES)> Bind(T* o)
    {
        return function<R (ARG_TYPES)>
            (&MemberCallbackFactory::Wrapper<Func>,
            static_cast<const void*>(o));
    }
};

template<typename R, class T C TYPENAMES>
inline MemberCallbackFactory<R, T C ARG_TYPES>
GetCallbackFactory(R (T::*)(ARG_TYPES), T*)
{
    return MemberCallbackFactory<R, T C ARG_TYPES>();
}

template<typename R, class T C TYPENAMES>
class ConstMemberCallbackFactory
{
private:
    template<R (T::*Func)(ARG_TYPES) const>
    static R Wrapper(const void* o C ARG_TYPES_AND_NAMES)
    {
        const T* obj = static_cast<const T*>(o);
        return (obj->*Func)(ARG_NAMES);
    }

public:
    template<R (T::*Func)(ARG_TYPES) const>
    inline static function<R (ARG_TYPES)> Bind(const T* o)
    {
        return function<R (ARG_TYPES)>
            (&ConstMemberCallbackFactory::Wrapper<Func>,
            static_cast<const void*>(o));
    }
};

template<typename R, class T C TYPENAMES>
inline ConstMemberCallbackFactory<R, T C ARG_TYPES>
GetCallbackFactory(R (T::*)(ARG_TYPES) const, const T*)
{
    return ConstMemberCallbackFactory<R, T C ARG_TYPES>();
}

template<typename R C TYPENAMES, typename PTR>
class BoundCallbackFactory
{
private:
    template<R (*Func)(PTR* C ARG_TYPES)>
    static R Wrapper(const void* o C ARG_TYPES_AND_NAMES)
    {
        return (*Func)((PTR*)o C ARG_NAMES);
    }

public:
    template<R (*Func)(PTR* C ARG_TYPES)>
    inline static function<R (ARG_TYPES)> Bind(PTR* o)
    {
        return function<R (ARG_TYPES)>
            (&BoundCallbackFactory::Wrapper<Func>, o);
    }
};

template<typename R C TYPENAMES, typename PTR>
inline BoundCallbackFactory<R C ARG_TYPES, PTR>
GetCallbackFactory(R (*)(PTR* C ARG_TYPES), PTR*)
{
    return BoundCallbackFactory<R C ARG_TYPES, PTR>();
}

#undef COUNT
#undef LOOP
#endif
