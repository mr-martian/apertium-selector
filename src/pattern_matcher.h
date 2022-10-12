#ifndef __SELECTOR_PATTERN_H__
#define __SELECTOR_PATTERN_H__

#include "lu.h"
#include <lttoolbox/match_exe.h>
#include <lttoolbox/transducer.h>

class PatternMatcher
{
private:
  std::vector<std::vector<UString>> patterns;
  Alphabet alpha;
  Transducer trans;
  MatchExe* me = nullptr;
  std::multimap<int, uint64_t> feature_states;
  int32_t any_char = 0;
  int32_t any_tag = 0;
  int32_t sl_sym = 0;
  int32_t tl_sym = 0;
public:
  PatternMatcher();
  ~PatternMatcher();
  void get_features(Reading* reading, bool is_src,
                    sorted_vector<uint64_t>& feats);
  void add_pattern(size_t id, const UString& pat);
  void build_trans();
  void read(FILE* input);
  void write(FILE* output);
  Alphabet& get_alpha() { return alpha; }
  std::vector<std::vector<UString>>& get_patterns() { return patterns; }
};

#endif
