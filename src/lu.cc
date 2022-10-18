#include "lu.h"

void Reading::read(InputFile& input, const Alphabet& alpha)
{
  while (!input.eof()) {
    UChar32 c = input.get();
    if (c == '\0' || c == '/' || c == '$') {
      input.unget(c);
      break;
    }
    if (c == '\\') {
      form += c;
      c = input.get();
      form += c;
      symbols.push_back(static_cast<int32_t>(c));
    } else if (c == '<') {
      UString tag = input.readBlock('<', '>');
      form += tag;
      symbols.push_back(alpha(tag));
    } else {
      form += c;
      symbols.push_back(static_cast<int32_t>(c));
    }
  }
}

void Reading::get_feats(int idx, FeatSet& feat_ls)
{
  for (auto& it : feats) feat_ls.insert(std::make_pair(idx, it));
}

LU::~LU()
{
  if (src != nullptr) delete src;
  for (auto& r : trg) delete r;
}

void LU::read(InputFile& input, const Alphabet& alpha)
{
  blank = input.readBlank(true);
  if (!input.eof() && input.peek() == '^') {
    input.get();
    src = new Reading();
    src->read(input, alpha);
    while (!input.eof() && input.peek() == '/') {
      input.get();
      Reading* t = new Reading();
      t->read(input, alpha);
      trg.push_back(t);
    }
    if (!input.eof() && input.peek() == '$') {
      input.get();
    }
  }
}

void LU::write(UFILE* output, size_t selected,
               bool selected_first, bool with_surf)
{
  ::write(blank, output);
  if (src != nullptr) {
    u_fputc('^', output);
    if (with_surf) src->write(output);
    if (selected < trg.size()) {
      if (with_surf) u_fputc('/', output);
      trg[selected]->write(output);
    }
    if (selected_first) {
      for (size_t i = 0; i < trg.size(); i++) {
        if (i == selected) continue;
        u_fputc('/', output);
        trg[i]->write(output);
      }
    }
    u_fputc('$', output);
  }
}

void LU::keep_only(size_t idx)
{
  for (size_t i = 0; i < trg.size(); i++) {
    if (i != idx) delete trg[i];
  }
  std::vector<Reading*> temp;
  if (idx < trg.size()) temp.push_back(trg[idx]);
  temp.swap(trg);
}
