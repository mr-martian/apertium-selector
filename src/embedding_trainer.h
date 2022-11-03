#ifndef __SELECTOR_EMBED_TRAIN_H__
#define __SELECTOR_EMBED_TRAIN_H__

#include "lu.h"

typedef sorted_vector<uint64_t> WordFeats;
// (count, feat)
typedef std::pair<uint64_t, uint64_t> VocabWord;

class EmbeddingTrainer {
private:
  // settings
  uint64_t window = 5;
  uint64_t min_count = 5;
  double alpha = 0.005;
  uint64_t unigram_table_size = 1e8;
  uint64_t dimension = 100;
  uint64_t negative_samples = 0;

  // calculated values
  uint64_t token_count = 0;
  uint64_t type_count = 0;

  Alphabet alphabet;
  // feat => freq
  std::vector<uint64_t> vocab;
  std::vector<UString> vocab_pats;
  std::map<UString, uint64_t> vocab_pats_inv;

  std::vector<uint64_t> unigram_table;

  std::vector<double> hidden_layer;
  std::vector<double> negative_layer;
  std::vector<double> errors;

  std::vector<std::vector<LU*>> sentences_raw;
  std::vector<std::vector<WordFeats>> sentences;

  uint64_t current_random = 1;
  uint64_t random() {
    return (current_random = (current_random * (uint64_t)25214903917 + 11));
  }
  float frandom() {
    return ((random() & 0xFFFF) / (float)65536);
  }

  // pre-computed softmax
  std::vector<double> _exp_table;
  size_t MAX_EXP = 6;
  size_t EXP_TABLE_SIZE = 1000;

  double softmax(double f);
  void make_exp_table();
  void init_unigram_table();

  uint64_t get_or_create_feat(const UString& key);
  WordFeats get_keys(LU* l);
  void init_corpus();
  void trim_vocab();
  void train_pair(uint64_t input, uint64_t output, bool neg);
  void train_word(size_t sent_idx, size_t word_idx);

public:
  EmbeddingTrainer();
  ~EmbeddingTrainer();
  void read_corpus(InputFile& input);
  void train();
  void write(UFILE* output);
};

#endif
