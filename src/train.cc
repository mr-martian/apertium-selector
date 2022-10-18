#include "train.h"
#include <algorithm>

#include <iostream>

void SelectorTrainer::clear_examples()
{
  for (auto& ex : examples) {
    for (auto& it : ex) {
      delete it.first;
    }
  }
}

void SelectorTrainer::error(const char* msg)
{
  std::cerr << "ERROR at line " << cur_line
            << ", LU " << (examples.back().size() + 1)
            << ": " << msg << std::endl;
  exit(EXIT_FAILURE);
}

size_t count_nl(const UString& s)
{
  return (size_t)std::count(s.begin(), s.end(), '\n');
}

void SelectorTrainer::load_corpus(InputFile& raw, InputFile& gold)
{
  clear_examples();
  examples.resize(1);
  cur_line = 1;
  while (!raw.eof()) {
    LU* lr = fs.read_lu(raw);
    LU* lg = fs.read_lu(gold);
    if (lg->isEOF() && !lr->isEOF()) {
      error("Gold file ends before gold file.");
    }
    if (lr->isEOF()) {
      if (!lg->isEOF()) {
        error("Raw file ends before raw file.");
      }
      break;
    }
    size_t nlr = count_nl(lr->get_blank());
    size_t nlg = count_nl(lg->get_blank());
    if (nlr != nlg) {
      error("Raw and Gold files have line breaks in different places.");
    }
    if (nlr) {
      cur_line += nlr;
      examples.resize(examples.size()+1);
    }
    if (lr->get_trg().empty()) {
      error("Raw lexical unit has no targets.");
    }
    if (!lr->ambiguous()) {
      examples.back().push_back(std::make_pair(lr, 0));
      delete lg;
      continue;
    }
    if (lg->get_trg().size() != 1) {
      error("Gold file must have exactly 1 reading per lexical unit.");
    }
    Reading* gr = lg->get_trg()[0];
    auto& trg = lr->get_trg();
    size_t n = trg.size();
    for (size_t i = 0; i < trg.size(); i++) {
      if (trg[i]->get_form() == gr->get_form()) {
        n = i;
        break;
      }
    }
    if (n == trg.size()) {
      error("Gold target not present among raw targets.");
    }
    examples.back().push_back(std::make_pair(lr, n));
    delete lg;
  }
}

void SelectorTrainer::update_weight(FeatPair f, double w)
{
  double oldw = fs.get_weight(f);
  fs.set_weight(f, oldw+w);
  totals[f] += oldw * (cur_inst - last_update[f]);
  last_update[f] = cur_inst;
}

void SelectorTrainer::run_instance(size_t sentence, size_t word)
{
  auto& sent = examples[sentence];
  FeatSet context_feats;
  for (size_t i = 1; i <= fs.get_lookbehind() && i <= word; i++) {
    auto& pr = sent[word-i];
    pr.first->get_src()->get_feats(-(int)i, context_feats);
    pr.first->get_trg()[pr.second]->get_feats(-(int)i, context_feats);
  }
  sent[word].first->get_src()->get_feats(0, context_feats);
  for (size_t i = 1; i <= fs.get_lookahead(); i++) {
    if (word + i == sent.size()) break;
    sent[word+i].first->get_src()->get_feats(i, context_feats);
  }
  std::vector<double> weights;
  std::vector<FeatPairSet> feats;
  for (auto& it : sent[word].first->get_trg()) {
    FeatSet fls = context_feats;
    it->get_feats(0, fls);
    sorted_vector<FeatPair> fp;
    weights.push_back(fs.get_weight(fls, fp));
    feats.push_back(fp);
  }
  size_t max = 0;
  for (size_t i = 1; i < weights.size(); i++) {
    if (weights[i] > weights[max]) max = i;
  }
  // prediction correct => done
  if (max == sent[word].second) return;
  // prediction incorrect => update weights
  FeatPairSet good = feats[sent[word].second];
  FeatPairSet bad = feats[max];
  for (auto& it : good) {
    if (bad.count(it)) bad.erase(it);
    else update_weight(it, 1);
  }
  for (auto& it : bad) update_weight(it, -1);
  // TODO: check and warn if feats are identical?
}

void SelectorTrainer::run_iteration()
{
  cur_inst = 0;
  totals = fs.get_all_weights();
  last_update.clear();
  for (auto& it : totals) {
    last_update.insert(std::make_pair(it.first, 0));
  }
  for (size_t i = 0; i < examples.size(); i++) {
    for (size_t j = 0; j < examples[i].size(); j++) {
      if (!examples[i][j].first->ambiguous()) continue;
      cur_inst++;
      run_instance(i, j);
    }
  }
  if (cur_inst) {
    for (auto& it : totals) {
      if (last_update[it.first] > 0) {
        it.second += fs.get_weight(it.first) * (cur_inst - last_update[it.first]);
        it.second /= cur_inst;
        fs.set_weight(it.first, it.second);
      }
    }
  }
}

void SelectorTrainer::train(InputFile& raw, InputFile& gold, size_t iterations)
{
  load_corpus(raw, gold);
  for (cur_iter = 1; cur_iter <= iterations; cur_iter++) {
    run_iteration();
  }
}
