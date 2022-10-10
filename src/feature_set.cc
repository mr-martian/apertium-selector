#include "feature_set.h"
#include "file_header.h"
#include <lttoolbox/compression.h>
#include <lttoolbox/match_state.h>
#include <lttoolbox/string_utils.h>
#include <unicode/utf16.h>
#include <cstring>

#include <iostream>

FeatureSet::FeatureSet()
{
}

FeatureSet::~FeatureSet()
{
  clear();
}

void skip_space(InputFile& input)
{
  while (!input.eof() && u_isspace(input.peek()) && input.peek() != '\n')
    input.get();
}

UString read_to_space(InputFile& input)
{
  UString ret;
  while (!input.eof()) {
    UChar32 c = input.get();
    if (c == '\\') {
      ret += c;
      ret += input.get();
    } else if (u_isspace(c)) {
      input.unget(c);
      break;
    } else {
      ret += c;
    }
  }
  return ret;
}

void skip_line(InputFile& input)
{
  while (!input.eof() && input.peek() != '\n') input.get();
}

void tokenize_line(InputFile& input, std::vector<UString>& vec)
{
  while (!input.eof() && input.peek() != '\n') {
    skip_space(input);
    UString temp = read_to_space(input);
    if (!temp.empty()) vec.push_back(temp);
  }
}

bool FeatureSet::parse_featloc(const UString& tok, FeatLoc& fl)
{
  for (size_t i = 0; i < tok.size(); i++) {
    if (tok[i] == ':') {
      if (i+1 == tok.size()) return false;
      try {
        int n = StringUtils::stoi(UString{tok.substr(0, i)});
        if (n < 0 && -n > (int)lookbehind) return false;
        if (n > (int)lookahead) return false;
        fl.first = n;
      } catch (...) {
        return false;
      }
      auto name = tok.substr(i+1);
      auto loc = feature_names_inv.find(name);
      if (loc == feature_names_inv.end()) return false;
      fl.second = loc->second;
      return true;
    } else if (u_isdigit(tok[i]) || (tok[i] == '-' && i == 0)) {
      continue;
    } else {
      return false;
    }
  }
  return false;
}

void FeatureSet::parse_single_number(InputFile& input, UChar32 c)
{
  std::vector<UString> toks;
  tokenize_line(input, toks);
  if (toks.size() != 1) return;
  try {
    int n = StringUtils::stoi(toks[0]);
    if (n < 0) return;
    switch (c) {
    case 'L':
      if (!lookbehind) lookbehind = (size_t)n;
      break;
    case 'R':
      if (!lookahead) lookahead = (size_t)n;
      break;
    case 'B':
      if (!beam_size) beam_size = (size_t)n;
      break;
    }
  } catch (...) {
    return;
  }
}

void FeatureSet::clear()
{
  lookbehind = 0;
  lookahead = 0;
  beam_size = 0;
  patterns.clear();
  feature_names.clear();
  feature_names_inv.clear();
  feature_weights.clear();
  trans.clear();
  if (me != nullptr) {
    delete me;
    me = nullptr;
  }
  feature_states.clear();
}

void FeatureSet::read(InputFile& input)
{
  clear();
  patterns.push_back(std::vector<UString>());
  feature_names.push_back(""_u);
  feature_names_inv.insert(std::make_pair(u""_uv, 0));
  alpha.includeSymbol(Transducer::ANY_CHAR_SYMBOL);
  alpha.includeSymbol(Transducer::ANY_TAG_SYMBOL);
  while (!input.eof()) {
    skip_space(input);
    UChar32 c = input.get();
    skip_space(input);
    switch (c) {
    case 'L':
    case 'R':
    case 'B':
      parse_single_number(input, c);
      break;
    case 'P':
      {
        UString name = read_to_space(input);
        size_t pos;
        auto loc = feature_names_inv.find(name);
        if (loc == feature_names_inv.end()) {
          feature_names.push_back(name);
          feature_names_inv.insert(std::make_pair(name, patterns.size()));
          pos = patterns.size();
          patterns.push_back(std::vector<UString>());
        } else {
          pos = loc->second;
        }
        tokenize_line(input, patterns[pos]);
      }
      break;
    case 'W':
      {
        std::vector<UString> toks;
        tokenize_line(input, toks);
        FeatLoc f1, f2;
        bool ok = true;
        if (toks.size() == 2) {
          ok &= parse_featloc(toks[0], f1);
          f2 = std::make_pair(0, 0);
        } else if (toks.size() == 3) {
          ok &= parse_featloc(toks[0], f1);
          ok &= parse_featloc(toks[1], f2);
        } else {
          break;
        }
        if (!ok) break;
        double w;
        try {
          w = StringUtils::stod(toks.back());
        } catch (...) {
          break;
        }
        if (f1 < f2) {
          feature_weights[f1].insert(std::make_pair(f2, w));
        } else {
          feature_weights[f2].insert(std::make_pair(f1, w));
        }
      }
      break;
    default: // ignore other lines for now
      skip_line(input);
      break;
    }
    if (!input.eof() && input.peek() == '\n') input.get();
  }
  build_transducer();
}

int32_t rule_number(Alphabet& a, uint64_t n)
{
  UChar buf[64];
  u_snprintf(buf, 64, "<RULE_NUMBER:%d>", n);
  UString sym = buf;
  a.includeSymbol(sym);
  return a(sym);
}

void FeatureSet::build_transducer()
{
  int32_t any_char = alpha(alpha(Transducer::ANY_CHAR_SYMBOL),
                           alpha(Transducer::ANY_CHAR_SYMBOL));
  int32_t any_tag = alpha(alpha(Transducer::ANY_TAG_SYMBOL),
                          alpha(Transducer::ANY_TAG_SYMBOL));
  std::map<int32_t, size_t> final_syms;
  for (size_t i = 0; i < patterns.size(); i++) {
    int32_t rlsym = rule_number(alpha, i);
    rlsym = alpha(rlsym, rlsym);
    final_syms[rlsym] = i;
    for (auto& pat : patterns[i]) {
      int state = 0;
      size_t i = 0;
      size_t end = pat.size();
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
  for (auto& state : trans.getTransitions()) {
    for (auto& arc : state.second) {
      if (!final_syms.count(arc.first)) continue;
      if (!trans.isFinal(arc.second.first)) continue;
      trans.setFinal(state.first);
      feature_states.insert(std::make_pair(state.first, final_syms[arc.first]));
    }
  }
  for (auto& it : old_finals) {
    trans.setFinal(it.first, it.second, false);
  }
}

void FeatureSet::write(UFILE* output)
{
  u_fprintf(output, "B %d\n", beam_size);
  u_fprintf(output, "L %d\n", lookbehind);
  u_fprintf(output, "R %d\n", lookahead);
  for (size_t i = 0; i < patterns.size(); i++) {
    for (auto& it : patterns[i]) {
      u_fprintf(output, "P %S %S\n", feature_names[i].c_str(), it.c_str());
    }
  }
  for (auto& it : feature_weights) {
    for (auto& it2 : it.second) {
      if (it.first.second == 0) {
        u_fprintf(output, "W %d:%S %f\n",
                  it2.first.first, feature_names[it2.first.second].c_str(),
                  it2.second);
      } else {
        u_fprintf(output, "W %d:%S %d:%S\n",
                  it.first.first, feature_names[it.first.second].c_str(),
                  it2.first.first, feature_names[it2.first.second].c_str(),
                  it2.second);
      }
    }
  }
}

void FeatureSet::load(FILE* input)
{
  fpos_t pos;
  if (fgetpos(input, &pos) == 0) {
    char header[4]{};
    fread_unlocked(header, 1, 4, input);
    if (strncmp(header, HEADER_APSL, 4) == 0) {
      auto features = read_le<uint64_t>(input);
      if (features >= APSL_UNKNOWN) {
        throw std::runtime_error("This weights file has features that are unknown to this version of apertium-selector - upgrade!");
      }
    } else {
      throw std::runtime_error("Weights file is missing header!");
    }
  }
  // settings
  beam_size = Compression::multibyte_read(input);
  lookbehind = Compression::multibyte_read(input);
  lookahead = Compression::multibyte_read(input);
  // FST
  alpha.read(input);
  trans.read(input);
  // features
  std::map<int, int> state_list;
  for (auto len = Compression::multibyte_read(input); len > 0; len--) {
    int state = (int)Compression::multibyte_read(input);
    uint64_t feat = Compression::multibyte_read(input);
    feature_states.insert(std::make_pair(state, feat));
    state_list.insert(std::make_pair(state, state));
  }
  me = new MatchExe(trans, state_list);
  // weights
  for (auto len1 = Compression::multibyte_read(input); len1 > 0; len1--) {
    FeatLoc f1;
    f1.first = (int)Compression::multibyte_read(input) - (int)lookbehind;
    f1.second = Compression::multibyte_read(input);
    feature_weights[f1].clear();
    for (auto len2 = Compression::multibyte_read(input); len2 > 0; len2--) {
      FeatLoc f2;
      f2.first = (int)Compression::multibyte_read(input) - (int)lookbehind;
      f2.second = Compression::multibyte_read(input);
      double weight = Compression::long_multibyte_read(input);
      feature_weights[f1].insert(std::make_pair(f2, weight));
    }
  }
}

void FeatureSet::compile(FILE* output)
{
  // header
  fwrite_unlocked(HEADER_APSL, 1, 4, output);
  uint64_t header_features = 0;
  write_le(output, header_features);
  // settings
  Compression::multibyte_write(beam_size, output);
  Compression::multibyte_write(lookbehind, output);
  Compression::multibyte_write(lookahead, output);
  // FST
  alpha.write(output);
  trans.write(output);
  // features
  Compression::multibyte_write(feature_states.size(), output);
  for (auto& it : feature_states) {
    Compression::multibyte_write(it.first, output);
    Compression::multibyte_write(it.second, output);
  }
  // weights
  Compression::multibyte_write(feature_weights.size(), output);
  for (auto& it : feature_weights) {
    Compression::multibyte_write(it.first.first + lookbehind, output);
    Compression::multibyte_write(it.first.second, output);
    Compression::multibyte_write(it.second.size(), output);
    for (auto& it2 : it.second) {
      Compression::multibyte_write(it2.first.first + lookbehind, output);
      Compression::multibyte_write(it2.first.second, output);
      Compression::long_multibyte_write(it2.second, output);
    }
  }
}

void FeatureSet::get_features(Reading* reading, bool is_src)
{
  if (reading == nullptr) return;
  MatchState ms;
  ms.init(me->getInitial());
  // TODO: optionally step by <SL>/<TL> or something
  int32_t any_char = alpha(alpha(u"<ANY_CHAR>"_u), alpha(u"<ANY_CHAR>"_u));
  int32_t any_tag = alpha(alpha(u"<ANY_TAG>"_u), alpha(u"<ANY_TAG>"_u));
  for (auto& sym : reading->get_symbols()) {
    ms.step(alpha(sym, sym), (sym < 0 ? any_tag : any_char));
  }
  std::set<int> states;
  while (true) {
    int state = ms.classifyFinals(me->getFinals(), states);
    if (state == -1) break;
    states.insert(state);
  }
  reading->add_feat(0);
  for (auto& it : states) {
    auto rng = feature_states.equal_range(it);
    for (auto it2 = rng.first; it2 != rng.second; it2++) {
      reading->add_feat(it2->second);
    }
  }
}

void FeatureSet::get_features(LU* lu)
{
  get_features(lu->get_src(), true);
  for (auto& it : lu->get_trg()) get_features(it, false);
}

double FeatureSet::get_weight(sorted_vector<FeatLoc>& feats)
{
  double ret = 0.0;
  auto vec = feats.get();
  for (size_t i = 0; i < vec.size(); i++) {
    auto loc = feature_weights.find(vec[i]);
    if (loc == feature_weights.end()) continue;
    auto dct = loc->second;
    for (size_t j = i+1; j < vec.size(); j++) {
      auto loc2 = dct.find(vec[j]);
      if (loc2 == dct.end()) continue;
      ret += loc2->second;
    }
  }
  return ret;
}

Alphabet& FeatureSet::get_alpha()
{
  return alpha;
}
