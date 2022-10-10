#ifndef __SELECTOR_FILE_HEADER_H__
#define __SELECTOR_FILE_HEADER_H__

constexpr char HEADER_APSL[4]{'A', 'P', 'S', 'L'};
enum APSL_FEATURES : uint64_t {
  APSL_UNKNOWN = (1ull << 0),
  APSL_RESERVED = (1ull << 63),
};

#endif
