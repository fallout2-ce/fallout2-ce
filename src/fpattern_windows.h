#ifndef FALLOUT_FPATTERN_WINDOWS_H_
#define FALLOUT_FPATTERN_WINDOWS_H_

#ifdef __cplusplus
extern "C" {
#endif

int fpattern_windows_isvalid(const char* pat);
int fpattern_windows_match(const char* pat, const char* fname);
int fpattern_windows_matchn(const char* pat, const char* fname);

#ifdef __cplusplus
}
#endif

#endif
