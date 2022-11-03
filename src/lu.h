#ifndef __SELECTOR_LU_H__
#define __SELECTOR_LU_H__

#include <lttoolbox/alphabet.h>
#include <lttoolbox/input_file.h>
#include <lttoolbox/sorted_vector.hpp>
#include <lttoolbox/ustring.h>
#include <unicode/ustdio.h>
#include <vector>

// (pos, feat)
// in processing and text files, position is an int ranging from
// -lookbehind to +lookahead
// in binary files, it is stored as non-negative by adding lookbehind
typedef std::pair<int, uint64_t> FeatLoc;

typedef std::pair<FeatLoc, FeatLoc> FeatPair;

typedef sorted_vector<FeatLoc> FeatSet;
typedef sorted_vector<FeatPair> FeatPairSet;

class Reading {
private:
  UString form;
  std::vector<int32_t> symbols;
  sorted_vector<uint64_t> feats;
public:
  void read(InputFile& input, const Alphabet& alpha);
  void write(UFILE* output) { ::write(form, output); }
  UString& get_form() { return form; }
  std::vector<int32_t>& get_symbols() { return symbols; }
  sorted_vector<uint64_t>& get_feats() { return feats; }
  void get_feats(int idx, FeatSet& feat_ls);
  void add_feat(uint64_t feat) { feats.insert(feat); }
};

class LU {
private:
  Reading* src = nullptr;
  std::vector<Reading*> trg;
  UString blank; // preceding blank
public:
  ~LU();
  void read(InputFile& input, const Alphabet& alpha);
  void write(UFILE* output, size_t selected,
             bool selected_first = false, bool with_surf = true);
  // return true if multiple readings
  // and false if 1 reading or this is stream-final blank
  bool ambiguous() { return trg.size() > 1; }
  bool isEOF() { return src == nullptr; }
  UString& get_blank() { return blank; }
  Reading* get_src() { return src; }
  std::vector<Reading*>& get_trg() { return trg; }
  void keep_only(size_t idx);
  size_t after_newline();
};

#endif
