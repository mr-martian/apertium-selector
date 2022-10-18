#ifndef __SELECTOR_TRAINER_H__
#define __SELECTOR_TRAINER_H__

#include "feature_set.h"

class SelectorTrainer {
private:
  std::vector<std::vector<std::pair<LU*, size_t>>> examples;
  size_t cur_inst = 0;
  size_t cur_iter = 0;
  size_t cur_line = 0;
  FeatureSet fs;
  std::map<FeatPair, size_t> last_update;
  std::map<FeatPair, double> totals;
  void clear_examples();
  void error(const char* msg);
  void load_corpus(InputFile& raw, InputFile& gold);
  void update_weight(FeatPair f, double w);
  void run_instance(size_t sentence, size_t word);
  void run_iteration();
public:
  SelectorTrainer() {}
  ~SelectorTrainer() { clear_examples(); }
  void read(InputFile& input) { fs.read(input); }
  void write(UFILE* output) { fs.write(output); }
  void train(InputFile& raw, InputFile& gold, size_t iterations);
};

#endif
