#ifndef __SELECTOR_RULES_H__
#define __SELECTOR_RULES_H__

#include "pattern_matcher.h"

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
  double get_weight(FeatSet& feats);
  double get_weight(FeatSet& feats, FeatPairSet& used_feats);
  size_t get_beam_size() { return beam_size; }
  size_t get_lookahead() { return lookahead; }
  size_t get_lookbehind() { return lookbehind; }
  std::map<FeatPair, double> get_all_weights();
  double get_weight(FeatPair fp);
  void set_weight(FeatPair fp, double w);
};

#endif
