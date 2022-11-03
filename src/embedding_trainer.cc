#include "embedding_trainer.h"
#include <cmath>

EmbeddingTrainer::EmbeddingTrainer()
{
  make_exp_table();
}

EmbeddingTrainer::~EmbeddingTrainer() {}

double EmbeddingTrainer::softmax(double f) {
  if (f > MAX_EXP) return -1;
  if (f < -MAX_EXP) return 0;
  return _exp_table[(size_t)((f + MAX_EXP) * (EXP_TABLE_SIZE / MAX_EXP / 2))];
}

void EmbeddingTrainer::make_exp_table() {
  _exp_table.reserve(EXP_TABLE_SIZE);
  for (size_t i = 0; i < EXP_TABLE_SIZE; i++) {
    _exp_table[i] = exp((i / (double)EXP_TABLE_SIZE * 2 - 1) * MAX_EXP);
    _exp_table[i] = _exp_table[i] / (_exp_table[i] + 1);
  }
}

void EmbeddingTrainer::init_unigram_table()
{
  double denom = 0;
  for (size_t i = 1; i < vocab.size(); i++) {
    denom += pow(vocab[i], 0.75);
  }
  unigram_table.reserve(unigram_table_size);
  uint64_t vocab_idx = 1; // skip UNK
  // everything is UNK - something has gone VERY wrong
  if (vocab.size() <= 1) return;
  double frac_done = pow(vocab[vocab_idx], 0.75) / denom;
  for (uint64_t uni_idx = 0; uni_idx < unigram_table_size; uni_idx++) {
    unigram_table[uni_idx] = vocab_idx;
    if (uni_idx / (double)unigram_table_size > frac_done) {
      vocab_idx++;
      if (vocab_idx >= vocab.size()) {
        frac_done = 1.0;
        vocab_idx = vocab.size()-1;
      } else {
        frac_done = pow(vocab[vocab_idx], 0.75) / denom;
      }
    }
  }
}

uint64_t EmbeddingTrainer::get_or_create_feat(const UString& key)
{
  uint64_t ret = 0;
  auto loc = vocab_pats_inv.find(key);
  if (loc == vocab_pats_inv.end()) {
    ret = vocab.size();
    vocab.push_back(0);
    vocab_pats.push_back(key);
    vocab_pats_inv.insert(std::make_pair(key, ret));
  } else {
    ret = loc->second;
  }
  return ret;
}

WordFeats EmbeddingTrainer::get_keys(LU* l)
{
  WordFeats ret;
  ret.insert(get_or_create_feat(l->get_src()->get_form()));
  return ret;
}

void EmbeddingTrainer::init_corpus()
{
  vocab.clear();
  vocab_pats.clear();
  vocab_pats_inv.clear();
  sentences.clear();
  sentences_raw.clear();

  sentences.resize(1);
  sentences_raw.resize(1);
  get_or_create_feat(""_u);
}

void EmbeddingTrainer::trim_vocab()
{
  std::map<uint64_t, uint64_t> feat_remap;
  sorted_vector<std::pair<uint64_t, uint64_t>, std::greater<std::pair<uint64_t, uint64_t>>> sort_counts;
  uint64_t unk_count = vocab[0];
  for (uint64_t i = 1; i < vocab.size(); i++) {
    sort_counts.insert(std::make_pair(vocab[i], i));
  }
  while (!sort_counts.empty() && sort_counts.back().first < min_count) {
    unk_count += sort_counts.back().first;
    feat_remap[sort_counts.back().second] = 0;
    sort_counts.pop_back();
  }
  std::vector<UString> temp_pats;
  temp_pats.swap(vocab_pats);
  vocab.clear();
  vocab_pats_inv.clear();
  vocab.reserve(sort_counts.size()+1);
  get_or_create_feat(""_u);
  vocab[0] = unk_count;
  for (auto& it : sort_counts) {
    uint64_t newfeat = get_or_create_feat(temp_pats[it.second]);
    vocab[newfeat] = it.first;
    feat_remap[it.first] = newfeat;
  }
  for (auto& sent : sentences) {
    for (auto& wd : sent) {
      WordFeats wfnew;
      for (auto& it : wd) wfnew.insert(feat_remap[it]);
      // only include UNK if all feats are UNK
      if (wfnew.size() > 1 && wfnew.count(0)) wfnew.erase(0);
      wfnew.swap(wd);
    }
  }
}

void EmbeddingTrainer::read_corpus(InputFile& input)
{
  init_corpus();
  while (!input.eof()) {
    LU* l = new LU();
    l->read(input, alphabet);
    if (l->isEOF()) break;
    if (l->after_newline()) {
      sentences_raw.resize(sentences_raw.size()+1);
      sentences.resize(sentences.size()+1);
    }
    sentences_raw.back().push_back(l);
    auto keys = get_keys(l);
    sentences.back().push_back(keys);
    for (auto& it : keys) vocab[it] += 1;
  }
  trim_vocab();
  init_unigram_table();
}

void EmbeddingTrainer::train_pair(uint64_t input, uint64_t output, bool neg)
{
  double val = 0.0;
  for (size_t i = 0; i < dimension; i++) {
    val += hidden_layer[(input * dimension) + i] * negative_layer[(output * dimension) + i];
  }
  double err = (neg ? 0.0 : 1.0);
  err = (err - softmax(val)) * alpha;
  for (size_t i = 0; i < dimension; i++) {
    errors[i] += err * negative_layer[(output * dimension) + i];
    negative_layer[(output * dimension) + i] += err * hidden_layer[(input * dimension)];
  }
}

void EmbeddingTrainer::train_word(size_t sent_idx, size_t word_idx)
{
  size_t start = (word_idx > window ? word_idx - window : 0);
  auto& sent = sentences[sent_idx];
  WordFeats ctx;
  for (size_t i = start; i < sent.size() && i <= word_idx + window; i++) {
    if (i == word_idx) continue;
    ctx.insert(sent[i].begin(), sent[i].end());
  }
  for (auto& w : sent[word_idx]) {
    errors.clear();
    errors.resize(dimension, 0.0);
    for (auto& c : ctx) {
      train_pair(w, c, false);
      for (size_t n = 0; n < negative_samples; n++) {
        size_t idx = (random() >> 16) % unigram_table.size();
        uint64_t negfeat = unigram_table[idx];
        if (ctx.count(negfeat)) continue;
        train_pair(w, negfeat, true);
      }
    }
    for (size_t i = 0; i < dimension; i++) {
      hidden_layer[(w * dimension) + i] += errors[i];
    }
  }
}

void EmbeddingTrainer::train()
{
  hidden_layer.clear();
  hidden_layer.resize(vocab.size() * dimension, 0.0);
  negative_layer.clear();
  negative_layer.resize(vocab.size() * dimension, 0.0);
  for (size_t i = 0; i < vocab.size() * dimension; i++) {
    hidden_layer[i] = (frandom() - 0.5) / dimension;
  }
  for (size_t iteration = 0; iteration < min_count; iteration++) {
    for (size_t sent = 0; sent < sentences.size(); sent++) {
      for (size_t word = 0; word < sentences[sent].size(); word++) {
        train_word(sent, word);
      }
    }
  }
}

void EmbeddingTrainer::write(UFILE* output)
{
  for (size_t i = 0; i < vocab.size(); i++) {
    if (i == 0) {
      u_fprintf(output, "V UNK");
    } else {
      u_fprintf(output, "P F%d %S\n", i, vocab_pats[i].c_str());
      u_fprintf(output, "V F%d", i);
    }
    for (size_t j = 0; j < dimension; j++) {
      u_fprintf(output, " %f", hidden_layer[(i * dimension) + j]);
    }
    u_fputc('\n', output);
  }
}
