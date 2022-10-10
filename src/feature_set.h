#ifndef __SELECTOR_RULES_H__
#define __SELECTOR_RULES_H__

#include "lu.h"

#include <lttoolbox/alphabet.h>
#include <lttoolbox/match_exe.h>
#include <lttoolbox/sorted_vector.hpp>
#include <lttoolbox/transducer.h>
#include <lttoolbox/ustring.h>

// (pos, feat)
// in processing and text files, position is an int ranging from
// -lookbehind to +lookahead
// in binary files, it is stored as non-negative by adding lookbehind
typedef std::pair<int, uint64_t> FeatLoc;

class FeatureSet {
private:
  size_t beam_size = 0;
  size_t lookbehind = 0;
  size_t lookahead = 0;

  std::vector<std::vector<UString>> patterns;
  std::vector<UString> feature_names;
  std::map<UString, uint64_t> feature_names_inv;
  // feat1 => feat2 => weight
  std::map<FeatLoc, std::map<FeatLoc, double>> feature_weights;
  Alphabet alpha;
  Transducer trans;
  MatchExe* me = nullptr;
  std::multimap<int, uint64_t> feature_states;
  bool parse_featloc(const UString& tok, FeatLoc& fl);
  void parse_single_number(InputFile& input, UChar32 c);
  void clear();
  void build_transducer();
public:
  FeatureSet();
  ~FeatureSet();
  void read(InputFile& input);
  void write(UFILE* output);
  void load(FILE* input);
  void compile(FILE* output);
  void get_features(Reading* reading, bool is_src);
  void get_features(LU* lu);
  double get_weight(sorted_vector<FeatLoc>& feats);
  Alphabet& get_alpha();
  size_t get_beam_size() { return beam_size; }
  size_t get_lookahead() { return lookahead; }
  size_t get_lookbehind() { return lookbehind; }
};

#endif
