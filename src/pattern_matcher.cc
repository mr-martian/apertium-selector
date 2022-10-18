#include "pattern_matcher.h"
#include <lttoolbox/compression.h>
#include <lttoolbox/match_state.h>
#include <lttoolbox/string_utils.h>

PatternMatcher::PatternMatcher()
{
}

PatternMatcher::~PatternMatcher()
{
}

void PatternMatcher::get_features(Reading* reading, bool is_src,
                                  sorted_vector<uint64_t>& feats)
{
  if (reading == nullptr) return;
  MatchState ms;
  ms.init(me->getInitial());
  ms.step(is_src ? sl_sym : tl_sym);
  for (auto& sym : reading->get_symbols()) {
    ms.step(alpha(sym, sym), (sym < 0 ? any_tag : any_char));
  }
  std::set<int> states;
  while (true) {
    int state = ms.classifyFinals(me->getFinals(), states);
    if (state != -1) {
      auto rng = feature_states.equal_range(state);
      for (auto it = rng.first; it != rng.second; it++) {
        feats.insert(it->second);
      }
      states.insert(state);
    } else {
      break;
    }
  }
}

void PatternMatcher::add_pattern(size_t id, const UString& pat)
{
  while (patterns.size() <= id) {
    patterns.push_back(std::vector<UString>());
  }
  patterns[id].push_back(pat);
}

int32_t rule_number(Alphabet& a, uint64_t n)
{
  UChar buf[64];
  u_snprintf(buf, 64, "<RULE_NUMBER:%d>", n);
  UString sym = buf;
  a.includeSymbol(sym);
  return a(sym);
}

void PatternMatcher::build_trans()
{
  alpha.includeSymbol(Transducer::ANY_CHAR_SYMBOL);
  alpha.includeSymbol(Transducer::ANY_TAG_SYMBOL);
  alpha.includeSymbol("<side:sl>"_u);
  alpha.includeSymbol("<side:tl>"_u);
  any_char = alpha(alpha(Transducer::ANY_CHAR_SYMBOL), alpha(Transducer::ANY_CHAR_SYMBOL));
  any_tag = alpha(alpha(Transducer::ANY_TAG_SYMBOL), alpha(Transducer::ANY_TAG_SYMBOL));
  sl_sym = alpha(alpha("<side:sl>"_u), alpha("<side:sl>"_u));
  tl_sym = alpha(alpha("<side:tl>"_u), alpha("<side:tl>"_u));
  std::map<int32_t, size_t> final_syms;
  int sl_state = trans.insertSingleTransduction(sl_sym, 0);
  int tl_state = trans.insertSingleTransduction(tl_sym, 0);
  int both_state = trans.insertNewSingleTransduction(sl_sym, 0);
  trans.linkStates(0, both_state, tl_sym);
  for (size_t i = 0; i < patterns.size(); i++) {
    int32_t rlsym = rule_number(alpha, i);
    rlsym = alpha(rlsym, rlsym);
    final_syms[rlsym] = i;
    for (auto& pat : patterns[i]) {
      int state = 0;
      size_t i = 0;
      size_t end = pat.size();
      if (StringUtils::startswith(pat, u"sl/"_uv)) {
        state = sl_state;
        i = 3;
      } else if (StringUtils::startswith(pat, u"tl/"_uv)) {
        state = tl_state;
        i = 3;
      } else {
        state = both_state;
      }
      UChar32 c;
      bool esc = false;
      while (i < end) {
        U16_NEXT(pat.data(), i, end, c);
        if (esc) {
          int32_t sym = alpha(static_cast<int32_t>(c), static_cast<int32_t>(c));
          state = trans.insertSingleTransduction(sym, state);
        } else if (c == '\\') {
          esc = true;
        } else if (c == '*') {
          trans.linkStates(state, state, any_char);
        } else if (c == '<') {
          size_t j = i;
          while (c != '>' && j < end) {
            U16_NEXT(pat.data(), j, end, c);
          }
          if (c == '>') {
            if (i+2 == j && pat[i] == '*') {
              trans.linkStates(state, state, any_tag);
            } else {
              auto tg = pat.substr(i-1, j-i+1);
              alpha.includeSymbol(tg);
              int32_t sym = alpha(alpha(tg), alpha(tg));
              state = trans.insertSingleTransduction(sym, state);
            }
            i = j;
          }
        } else {
          int32_t sym = alpha(static_cast<int32_t>(c), static_cast<int32_t>(c));
          state = trans.insertSingleTransduction(sym, state);
        }
      }
      state = trans.insertSingleTransduction(rlsym, state);
      trans.setFinal(state);
    }
  }
  trans.minimize();
  feature_states.clear();
  auto old_finals = trans.getFinals();
  std::map<int, int> state_list;
  for (auto& state : trans.getTransitions()) {
    for (auto& arc : state.second) {
      if (!final_syms.count(arc.first)) continue;
      if (!trans.isFinal(arc.second.first)) continue;
      trans.setFinal(state.first);
      state_list.insert(std::make_pair(state.first, state.first));
      feature_states.insert(std::make_pair(state.first, final_syms[arc.first]));
    }
  }
  for (auto& it : old_finals) {
    trans.setFinal(it.first, it.second, false);
  }
  me = new MatchExe(trans, state_list);
}

void PatternMatcher::read(FILE* input)
{
  alpha.read(input);
  any_char = alpha(alpha(Transducer::ANY_CHAR_SYMBOL), alpha(Transducer::ANY_CHAR_SYMBOL));
  any_tag = alpha(alpha(Transducer::ANY_TAG_SYMBOL), alpha(Transducer::ANY_TAG_SYMBOL));
  sl_sym = alpha(alpha("<side:sl>"_u), alpha("<side:sl>"_u));
  tl_sym = alpha(alpha("<side:tl>"_u), alpha("<side:tl>"_u));
  trans.read(input);
  std::map<int, int> state_list;
  for (auto len = Compression::multibyte_read(input); len > 0; len--) {
    int state = (int)Compression::multibyte_read(input);
    uint64_t feat = Compression::multibyte_read(input);
    feature_states.insert(std::make_pair(state, feat));
    state_list.insert(std::make_pair(state, state));
  }
  me = new MatchExe(trans, state_list);
}

void PatternMatcher::write(FILE* output)
{
  alpha.write(output);
  trans.write(output);
  Compression::multibyte_write(feature_states.size(), output);
  for (auto& it : feature_states) {
    Compression::multibyte_write(it.first, output);
    Compression::multibyte_write(it.second, output);
  }
}
