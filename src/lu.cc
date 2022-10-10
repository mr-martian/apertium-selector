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

void Reading::write(UFILE* output)
{
  ::write(form, output);
}

std::vector<int32_t>& Reading::get_symbols()
{
  return symbols;
}

sorted_vector<uint64_t>& Reading::get_feats()
{
  return feats;
}

void Reading::add_feat(uint64_t feat)
{
  feats.insert(feat);
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

bool LU::ambiguous()
{
  return trg.size() > 1;
}

bool LU::isEOF()
{
  return src == nullptr;
}

Reading* LU::get_src()
{
  return src;
}

std::vector<Reading*>& LU::get_trg()
{
  return trg;
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
