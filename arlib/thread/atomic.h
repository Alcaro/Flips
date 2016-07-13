#pragma once
//This header defines several functions for atomically operating on integers or pointers.
//You can use int32_t, uint32_t, any typedef thereof, and any pointer.
//The following functions exist:
//lock_read(T*)
//lock_write(T*, T)
//lock_incr(T*)
//lock_decr(T*)
//lock_xchg(T*, T)
//lock_cmpxchg(T*, T, T)
//All of them use aquire-release ordering. If you know what you're doing, you can append _acq, _rel or _loose.

//All of these functions (except store) return the value before the operation.
//(cmp)xchg obviously does, so to ease memorization, the others do too.

#if GCC_VERSION > 0
#if GCC_VERSION >= 40700
//https://gcc.gnu.org/onlinedocs/gcc-4.7.0/gcc/_005f_005fatomic-Builtins.html
#define LOCKD_LOCKS_MODEL(type, model, modelname) \
	inline type lock_incr ## modelname(type * val) { return __atomic_fetch_add(val, 1, model); } \
	inline type lock_decr ## modelname(type * val) { return __atomic_fetch_sub(val, 1, model); } \
	inline type lock_xchg ## modelname(type * val, type newval) { return __atomic_exchange_n(val, newval, model); } \
	inline type lock_cmpxchg ## modelname(type * val, type old, type newval) { return __sync_val_compare_and_swap(val, old, newval); } \

//there is a modern version of cmpxchg, but it adds another move instruction for whatever reason and otherwise gives the same binary.
//__atomic_compare_exchange_n(val, &old, newval, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);

#define LOCKD_LOCKS(type) \
	inline type lock_read(type * val) { return __atomic_load_n(val, __ATOMIC_ACQUIRE); } \
	inline type lock_read_acq(type * val) { return __atomic_load_n(val, __ATOMIC_ACQUIRE); } \
	inline type lock_read_loose(type * val) { return __atomic_load_n(val, __ATOMIC_RELAXED); } \
	inline void lock_write(type * val, type newval) { __atomic_store_n(val, newval, __ATOMIC_RELEASE); } \
	inline void lock_write_rel(type * val, type newval) { __atomic_store_n(val, newval, __ATOMIC_RELEASE); } \
	inline void lock_write_loose(type * val, type newval) { __atomic_store_n(val, newval, __ATOMIC_RELAXED); } \
	LOCKD_LOCKS_MODEL(type, __ATOMIC_ACQ_REL, ) \
	LOCKD_LOCKS_MODEL(type, __ATOMIC_ACQUIRE, _acq) \
	LOCKD_LOCKS_MODEL(type, __ATOMIC_RELEASE, _rel) \
	LOCKD_LOCKS_MODEL(type, __ATOMIC_RELAXED, _loose) \

#else

//https://gcc.gnu.org/onlinedocs/gcc-4.1.2/gcc/Atomic-Builtins.html
//the memory model remains unused, but all functions must still be defined.
#define LOCKD_LOCKS_MODEL(type, modelname) \
	inline type lock_incr ## modelname(type * val) { __sync_fetch_and_add(val, 1); } \
	inline type lock_decr ## modelname(type * val) { __sync_fetch_and_sub(val, 1); } \
	inline type lock_cmpxchg ## modelname(type * val, type old, type newval) { return __sync_val_compare_and_swap(val, old, newval); } \
	inline type lock_xchg ## modelname(type * val, type newval) \
	{ \
		type prev = lock_read(val); \
		while (true) \
		{ \
			type prev2 = lock_cmpxchg(val, prev, newval); \
			if (prev == prev2) break; \
			else prev = prev2; \
		} \
	}

#define LOCKD_LOCKS(type) \
	inline type lock_read(type * val) { return __sync_fetch_and_add(val, 0); } \
	inline type lock_read_acq(type * val) { return __sync_fetch_and_add(val, 0); } \
	inline type lock_read_loose(type * val) { return __sync_fetch_and_add(val, 0); } \
	LOCKD_LOCKS_MODEL(type, ) \
	LOCKD_LOCKS_MODEL(type, _acq) \
	LOCKD_LOCKS_MODEL(type, _rel) \
	LOCKD_LOCKS_MODEL(type, _loose) \
	inline void lock_write(type * val, type newval) { lock_xchg(val, newval); } \
	inline void lock_write_rel(type * val, type newval) { lock_xchg(val, newval); } \
	inline void lock_write_loose(type * val, type newval) { lock_xchg(val, newval); } \

#endif

LOCKD_LOCKS(uint32_t)
LOCKD_LOCKS(int32_t)
LOCKD_LOCKS(void*)

#elif defined(_WIN32)

#define LOCKD_LOCKS_MODEL(type, wintype, suffix, modelname) \
	inline type lock_incr##modelname(type* val) { return (type)(InterlockedIncrement##suffix((wintype*)val)-1); } \
	inline type lock_decr##modelname(type* val) { return (type)(InterlockedDecrement##suffix((wintype*)val)+1); } \
	inline type lock_xchg##modelname(type* val, type newval) { return (type)InterlockedExchange##suffix((wintype*)val, (wintype)newval); } \
	inline type lock_cmpxchg##modelname(type* val, type old, type newval) \
		{ return (type)InterlockedCompareExchange##suffix((wintype*)val, (wintype)old, (wintype)newval); } \

//MSVC doesn't know what half of the memory model thingies do. Substitute in the strong ones.
#define LOCKD_LOCKS(type, wintype, suffix) \
	LOCKD_LOCKS_MODEL(type, wintype, suffix, ) \
	LOCKD_LOCKS_MODEL(type, wintype, suffix, _acq) \
	LOCKD_LOCKS_MODEL(type, wintype, suffix, _rel) \
	LOCKD_LOCKS_MODEL(type, wintype, suffix, _loose) \
	\
	inline type lock_read(type * val)       { return (type)InterlockedCompareExchange##suffix((wintype*)val, (wintype)0, (wintype)0); } \
	inline type lock_read_acq(type * val)   { return (type)InterlockedCompareExchange##suffix((wintype*)val, (wintype)0, (wintype)0); } \
	inline type lock_read_loose(type * val) { return (type)InterlockedCompareExchange##suffix((wintype*)val, (wintype)0, (wintype)0); } \
	inline void lock_write(type * val, type value)       { (void)InterlockedExchange##suffix((wintype*)val, (wintype)value); }\
	inline void lock_write_rel(type * val, type value)   { (void)InterlockedExchange##suffix((wintype*)val, (wintype)value); }\
	inline void lock_write_loose(type * val, type value) { (void)InterlockedExchange##suffix((wintype*)val, (wintype)value); }\

#ifdef _M_IX86
LOCKD_LOCKS(int32_t, LONG, )
LOCKD_LOCKS(uint32_t, LONG, )
LOCKD_LOCKS(void*, LONG, )
#elif defined(_M_X64)
LOCKD_LOCKS(int32_t, LONG, )
LOCKD_LOCKS(uint32_t, LONG, )
LOCKD_LOCKS(void*, LONGLONG, 64)
#endif

#endif




template<typename T> T* lock_read(T** val) { return (T*)lock_read((void**)val); }
template<typename T> void lock_write(T** val, T* newval) { lock_write((void**)val, (void*)newval); }
template<typename T> T* lock_cmpxchg(T** val, T* old, T* newval) { return (T*)lock_cmpxchg((void**)val, (void*)old, (void*)newval); }
template<typename T> T* lock_xchg(T** val, T* newval) { return (T*)lock_xchg((void**)val, (void*)newval); }

#if NULL==0
//the NULL/0 duality is one of the dumbest things I have ever seen. at least C++11 somewhat fixes that garbage
class null_only;
template<typename T> void lock_write(T** val, null_only* newval) { lock_write((void**)val, NULL); }
template<typename T> T* lock_cmpxchg(T** val, null_only* old, T* newval) { return (T*)lock_cmpxchg((void**)val, NULL, (void*)newval); }
template<typename T> T* lock_cmpxchg(T** val, T* old, null_only* newval) { return (T*)lock_cmpxchg((void**)val, (void*)old, NULL); }
template<typename T> T* lock_cmpxchg(T** val, null_only* old, null_only* newval) { return (T*)lock_cmpxchg((void**)val, NULL, NULL); }
template<typename T> T* lock_xchg(T** val, null_only* newval) { return (T*)lock_xchg((void**)val, NULL); }
#endif
