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
  feature_names.clear();
  feature_names_inv.clear();
  feature_weights.clear();
  //pm.clear(); // TODO
}

void FeatureSet::read(InputFile& input)
{
  clear();
  pm.add_pattern(0, ""_u);
  feature_names.push_back(""_u);
  feature_names_inv.insert(std::make_pair(u""_uv, 0));
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
          pos = pm.get_patterns().size();
          feature_names_inv.insert(std::make_pair(name, pos));
        } else {
          pos = loc->second;
        }
        std::vector<UString> pat;
        tokenize_line(input, pat);
        for (auto& it : pat) pm.add_pattern(pos, it);
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
  pm.build_trans();
}

void FeatureSet::write(UFILE* output)
{
  u_fprintf(output, "B %d\n", beam_size);
  u_fprintf(output, "L %d\n", lookbehind);
  u_fprintf(output, "R %d\n", lookahead);
  auto patterns = pm.get_patterns();
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
  pm.read(input);
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
  pm.write(output);
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

LU* FeatureSet::read_lu(InputFile& input)
{
  LU* ret = new LU();
  ret->read(input, pm.get_alpha());
  if (ret->get_src() != nullptr) {
    ret->get_src()->add_feat(0);
    pm.get_features(ret->get_src(), true, ret->get_src()->get_feats());
  }
  for (auto& t : ret->get_trg()) {
    t->add_feat(0);
    pm.get_features(t, false, t->get_feats());
  }
  return ret;
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
