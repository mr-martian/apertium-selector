#ifndef __SELECTOR_RULES_H__
#define __SELECTOR_RULES_H__

#include "lu.h"
#include "pattern_matcher.h"

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

  PatternMatcher pm;
  std::vector<UString> feature_names;
  std::map<UString, uint64_t> feature_names_inv;
  // feat1 => feat2 => weight
  std::map<FeatLoc, std::map<FeatLoc, double>> feature_weights;
  bool parse_featloc(const UString& tok, FeatLoc& fl);
  void parse_single_number(InputFile& input, UChar32 c);
  void clear();
public:
  FeatureSet();
  ~FeatureSet();
  void read(InputFile& input);
  void write(UFILE* output);
  void load(FILE* input);
  void compile(FILE* output);
  LU* read_lu(InputFile& input);
  double get_weight(sorted_vector<FeatLoc>& feats);
  size_t get_beam_size() { return beam_size; }
  size_t get_lookahead() { return lookahead; }
  size_t get_lookbehind() { return lookbehind; }
};

#endif
