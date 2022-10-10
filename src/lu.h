#ifndef __SELECTOR_LU_H__
#define __SELECTOR_LU_H__

#include <lttoolbox/alphabet.h>
#include <lttoolbox/input_file.h>
#include <lttoolbox/sorted_vector.hpp>
#include <lttoolbox/ustring.h>
#include <unicode/ustdio.h>
#include <vector>

class Reading {
private:
  UString form;
  std::vector<int32_t> symbols;
  sorted_vector<uint64_t> feats;
public:
  void read(InputFile& input, const Alphabet& alpha);
  void write(UFILE* output);
  std::vector<int32_t>& get_symbols();
  sorted_vector<uint64_t>& get_feats();
  void add_feat(uint64_t feat);
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
  bool ambiguous();
  bool isEOF();
  Reading* get_src();
  std::vector<Reading*>& get_trg();
  void keep_only(size_t idx);
};

#endif
