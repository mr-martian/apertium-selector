#include "selector.h"
#include <iostream>

Selector::Selector()
{
  reset();
}

Selector::~Selector()
{
  reset();
}

void Selector::load(FILE* input)
{
  fs.load(input);
  lookbehind = fs.get_lookbehind();
  pos_window = lookbehind + 1 + fs.get_lookahead();
}

void Selector::reset()
{
  for (auto& it : prev) delete it;
  prev.clear();
  for (auto& it : queue) delete it;
  queue.clear();
  path.clear();
  std::vector<BeamSearchState> temp;
  temp.push_back(std::make_pair(0.0, std::make_pair(0, 0)));
  path.push_back(temp);
  cur_word = 0;
}

LU* Selector::get_lu(size_t pos)
{
  int idx = (int)(cur_word + pos) - (int)lookbehind;
  if (idx >= 0) {
    if (idx < (int)queue.size()) {
      return queue[(size_t)idx];
    }
  } else {
    idx += (int)prev.size();
    if (0 <= idx && idx < (int)prev.size()) {
      return prev[(size_t)idx];
    }
  }
  return nullptr;
}

void Selector::refill_queue(InputFile& input)
{
  if (at_eof) return;
  if (!queue.empty() && queue.back()->isEOF()) return;
  while (queue.size() < cur_word + fs.get_lookahead()) {
    LU* l = fs.read_lu(input);
    queue.push_back(l);
    if (l->isEOF()) {
      at_eof = true;
      return;
    }
  }
}

void Selector::add_feats(sorted_vector<FeatLoc>& feats, size_t loc,
                         LU* lu, size_t ridx)
{
  if (!lu) return;
  Reading* rd = nullptr;
  if (ridx < lu->get_trg().size()) rd = lu->get_trg()[ridx];
  else if (lu->get_trg().size() == 1) rd = lu->get_trg()[0];
  if (rd == nullptr) return;
  rd->get_feats((int)loc - (int)lookbehind, feats);
}

void Selector::get_path_feats(sorted_vector<FeatLoc>& feats,
                              size_t ridx, size_t sidx)
{
  add_feats(feats, lookbehind, queue[cur_word], ridx);
  size_t path_pos = sidx;
  for (size_t loc = 0; loc < lookbehind; loc++) {
    if (loc == path.size()) break;
    LU* lu = get_lu(lookbehind-loc-1);
    if (lu == nullptr) break;
    auto& state = path[path.size()-loc-1][path_pos];
    path_pos = state.second.second;
    add_feats(feats, lookbehind-loc-1, lu, state.second.first);
  }
}

void Selector::process_next_word(UFILE* output)
{
  std::vector<LU*> window;
  window.reserve(pos_window);
  sorted_vector<FeatLoc> context_feats;
  for (size_t i = 0; i < pos_window; i++) {
    LU* lu = get_lu(i);
    window.push_back(lu);
    if (lu != nullptr && lu->get_src() != nullptr) {
      lu->get_src()->get_feats((int)i - (int)lookbehind, context_feats);
    }
  }
  LU* cur = window[lookbehind];

  sorted_vector<BeamSearchState> next_path;
  size_t ridx_lim = (cur->get_trg().size() ? cur->get_trg().size() : 1);
  for (size_t ridx = 0; ridx < ridx_lim; ridx++) {
    for (size_t sidx = 0; sidx < path.back().size(); sidx++) {
      sorted_vector<FeatLoc> feats = context_feats;
      get_path_feats(feats, ridx, sidx);
      BeamSearchState next;
      // make it negative to sort
      next.first = path.back()[sidx].first - fs.get_weight(feats);
      next.second.first = ridx;
      next.second.second = sidx;
      next_path.insert(next);
    }
  }
  if (fs.get_beam_size()) {
    while (next_path.size() > fs.get_beam_size()) {
      next_path.erase(next_path.back());
    }
  }
  path.push_back(next_path.get());

  if (cur->ambiguous()) {
    cur_word++;
  } else {
    std::vector<size_t> selected;
    selected.resize(cur_word+1, 0);
    size_t path_pos = 0;
    for (size_t i = 0; i <= cur_word; i++) {
      auto& state = path[path.size()-i-1][path_pos];
      selected[selected.size()-i-1] = state.second.first;
      path_pos = state.second.second;
    }
    for (size_t i = 0; i < selected.size(); i++) {
      queue[i]->write(output, selected[i]);
      queue[i]->keep_only(selected[i]);
      prev.push_back(queue[i]);
    }
    queue.erase(queue.begin(), queue.begin()+selected.size());
    while (prev.size() > lookbehind) {
      delete prev[0];
      prev.erase(prev.begin());
    }
    BeamSearchState empty_state = std::make_pair(0.0, std::make_pair(0, 0));
    std::vector<BeamSearchState> empty_vec(1, empty_state);
    path.clear();
    path.resize(prev.size()+1, empty_vec);
    cur_word = 0;
  }
}

void Selector::process(InputFile& input, UFILE* output)
{
  while (!input.eof()) {
    at_eof = false;
    refill_queue(input);
    while (!queue.empty()) {
      process_next_word(output);
      refill_queue(input);
    }
    if (input.peek() == '\0') {
      input.get();
      u_fputc('\0', output);
      u_fflush(output);
    }
  }
}
