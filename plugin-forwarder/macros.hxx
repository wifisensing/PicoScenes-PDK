#ifdef __GNUC__
#define PACKED( ... ) __VA_ARGS__ __attribute__((__packed__))
#endif

#ifdef _MSC_VER
#define PACKED( ... ) __pragma( pack(push, 1) ) __VA_ARGS__ __pragma( pack(pop))
#endif
