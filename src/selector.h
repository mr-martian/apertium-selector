#ifndef __SELECTOR_PROC_H__
#define __SELECTOR_PROC_H__

#include "feature_set.h"

// (-weight of path, (selected reading, prev state))
typedef std::pair<double, std::pair<size_t, size_t>> BeamSearchState;

class Selector {
private:
  std::vector<LU*> prev;
  std::vector<LU*> queue;
  std::vector<std::vector<BeamSearchState>> path;
  FeatureSet fs;
  size_t cur_word = 0;

  // numbers we reference a lot - copied/calculated from fs
  size_t lookbehind = 0;
  size_t pos_window = 0;

  bool at_eof = false;

  void reset();
  void refill_queue(InputFile& input);
  void process_next_word(UFILE* output);
  LU* get_lu(size_t pos);
  void add_feats(sorted_vector<FeatLoc>& feats, size_t loc, LU* rd, size_t ridx);
  void get_path_feats(sorted_vector<FeatLoc>& feats, size_t ridx, size_t sidx);
public:
  Selector();
  ~Selector();
  void load(FILE* input);
  void process(InputFile& input, UFILE* output);
};

#endif
