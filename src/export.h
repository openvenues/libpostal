#ifndef EXPORT_H
#define EXPORT_H

#ifdef _WIN32
#ifdef LIBPOSTAL_EXPORTS
#define LIBPOSTAL_EXPORT __declspec(dllexport)
#else
#define LIBPOSTAL_EXPORT __declspec(dllimport)
#endif
#elif __GNUC__ >= 4
#define LIBPOSTAL_EXPORT __attribute__ ((visibility("default")))
#else
#define LIBPOSTAL_EXPORT
#endif

#endif //EXPORT_H
