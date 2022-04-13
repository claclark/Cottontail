#include "src/fastid_txt.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "src/compressor.h"
#include "src/core.h"
#include "src/hopper.h"
#include "src/recipe.h"
#include "src/warren.h"

namespace cottontail {

namespace {

bool compressed_write(std::fstream *out, char *buffer, size_t n,
                      std::shared_ptr<Compressor> compressor,
                      std::string *error) {
  size_t available = n + compressor->extra(n);
  std::unique_ptr<char[]> compressed_buffer =
      std::unique_ptr<char[]>(new char[available + 1]);
  size_t m = compressor->crush(buffer, n, compressed_buffer.get(), available);
  out->write(reinterpret_cast<char *>(&m), sizeof(size_t));
  if (out->fail()) {
    safe_set(error) = "Fastid write failure";
    return false;
  }
  out->write(compressed_buffer.get(), m);
  if (out->fail()) {
    safe_set(error) = "Fastid write failure";
    return false;
  }
  return true;
}

std::shared_ptr<Compressor> get_text_compressor(std::string *error) {
  std::string name = "zlib";
  std::string recipe = "";
  return Compressor::make(name, recipe, error);
}

std::shared_ptr<Compressor> get_post_compressor(std::string *error) {
  std::string name = "post";
  std::string recipe = "";
  return Compressor::make(name, recipe, error);
}

inline addr search_addr(addr *a, addr n, addr value) {
  addr l = 0, h = n - 1;
  while (h >= l) {
    int m = l + (h - l) / 2;
    if (a[m] == value)
      return m;
    else if (a[m] > value)
      h = m - 1;
    else
      l = m + 1;
  }
  return -1;
}

} // namespace

std::shared_ptr<Txt> FastidTxt::make(const std::string &recipe,
                                     std::shared_ptr<Tokenizer> tokenizer,
                                     std::shared_ptr<Working> working,
                                     std::string *error) {
  safe_set(error) = "Can't make FastidTxt from a recipe";
  return nullptr;
}

std::shared_ptr<Txt> FastidTxt::make(std::shared_ptr<Txt> txt,
                                     std::shared_ptr<Working> working,
                                     std::string *error) {
  std::shared_ptr<Reader> reader = working->reader(FASTID_NAME);
  if (reader == nullptr)
    return txt;
  size_t buffer_size = reader->size();
  std::unique_ptr<char[]> buffer =
      std::unique_ptr<char[]>(new char[buffer_size]);
  char *b = buffer.get();
  reader->read(b, 0, buffer_size);
  size_t n = *(reinterpret_cast<size_t *>(b));
  b += sizeof(size_t);
  size_t m = *(reinterpret_cast<size_t *>(b));
  b += sizeof(size_t);
  std::unique_ptr<addr[]> p_buffer = std::unique_ptr<addr[]>(new addr[m]);
  std::unique_ptr<addr[]> q_buffer = std::unique_ptr<addr[]>(new addr[m]);
  std::unique_ptr<addr[]> o_buffer = std::unique_ptr<addr[]>(new addr[m + 1]);
  std::unique_ptr<char[]> t_buffer = std::unique_ptr<char[]>(new char[n + 1]);
  std::shared_ptr<Compressor> post_compressor = get_post_compressor(error);
  std::shared_ptr<Compressor> text_compressor = get_text_compressor(error);
  if (post_compressor == nullptr)
    return nullptr;
  size_t k;
  k = *(reinterpret_cast<size_t *>(b));
  b += sizeof(size_t);
  post_compressor->tang(b, k, reinterpret_cast<char *>(p_buffer.get()),
                        m * sizeof(addr));
  b += k;
  k = *(reinterpret_cast<size_t *>(b));
  b += sizeof(size_t);
  post_compressor->tang(b, k, reinterpret_cast<char *>(q_buffer.get()),
                        m * sizeof(addr));
  b += k;
  k = *(reinterpret_cast<size_t *>(b));
  b += sizeof(size_t);
  post_compressor->tang(b, k, reinterpret_cast<char *>(o_buffer.get()),
                        m * sizeof(addr));
  o_buffer[m] = n;
  b += k;
  k = *(reinterpret_cast<size_t *>(b));
  b += sizeof(size_t);
  text_compressor->tang(b, k, t_buffer.get(), n);
  t_buffer[n] = '\0';
  std::shared_ptr<FastidTxt> fastid =
      std::shared_ptr<FastidTxt>(new FastidTxt());
  fastid->txt_ = txt;
  fastid->n_ = n;
  fastid->m_ = m;
  fastid->text_ = std::move(t_buffer);
  fastid->p_ = std::move(p_buffer);
  fastid->q_ = std::move(q_buffer);
  fastid->offset_ = std::move(o_buffer);
  return fastid;
}

bool FastidTxt::check(const std::string &recipe, std::string *error) {
  safe_set(error) = "Can't check a FastidTxt recipe";
  return false;
}

std::string FastidTxt::recipe_() {
  std::map<std::string, std::string> parameters;
  return freeze(parameters);
}

std::string FastidTxt::translate_(addr p, addr q) {
  addr where = search_addr(p_.get(), m_, p);
  if (where != -1 && q_[where] == q)
    return std::string(text_.get() + offset_[where],
                       offset_[where + 1] - offset_[where]);
  else
    return txt_->translate(p, q);
}

addr FastidTxt::tokens_() { return txt_->tokens(); }

bool fastid(std::shared_ptr<Warren> warren, std::string *error) {
  warren->started();
  std::string id_gcl;
  std::string id_key = "id";
  if (!warren->get_parameter(id_key, &id_gcl)) {
    safe_set(error) = "No ids in warren";
    return false;
  }
  if (id_gcl == "") {
    safe_set(error) = "No ids in warren";
    return false;
  }
  std::unique_ptr<cottontail::Hopper> hopper =
      warren->hopper_from_gcl(id_gcl, error);
  if (hopper == nullptr)
    return false;
  struct Id {
    Id(addr p0, addr q0, std::string text0)
        : p(p0), q(q0), offset(0), text(text0){};
    addr p, q, offset;
    std::string text;
  };
  std::vector<Id> ids;
  addr p = minfinity, q;
  for (hopper->tau(p + 1, &p, &q); p < maxfinity; hopper->tau(p + 1, &p, &q))
    ids.emplace_back(p, q, warren->txt()->translate(p, q));
  size_t n = 0;
  for (auto &id : ids) {
    id.offset = n;
    n += id.text.size();
  }
  size_t m = ids.size();
  std::unique_ptr<addr[]> p_buffer = std::unique_ptr<addr[]>(new addr[m]);
  std::unique_ptr<addr[]> q_buffer = std::unique_ptr<addr[]>(new addr[m]);
  std::unique_ptr<addr[]> offset_buffer = std::unique_ptr<addr[]>(new addr[m]);
  std::unique_ptr<char[]> text_buffer =
      std::unique_ptr<char[]>(new char[n + 1]);
  addr *pb = p_buffer.get();
  addr *qb = q_buffer.get();
  addr *ob = offset_buffer.get();
  char *tb = text_buffer.get();
  for (auto &id : ids) {
    strcpy(tb, id.text.c_str());
    tb += id.text.size();
    *pb++ = id.p;
    *qb++ = id.q;
    *ob++ = id.offset;
  }
  std::shared_ptr<Compressor> text_compressor = get_text_compressor(error);
  if (text_compressor == nullptr)
    return false;
  std::shared_ptr<Compressor> post_compressor = get_post_compressor(error);
  if (post_compressor == nullptr)
    return false;
  std::string fastid_filename = warren->working()->make_name(FASTID_NAME);
  std::remove(fastid_filename.c_str());
  std::fstream out;
  out.open(fastid_filename, std::ios::binary | std::ios::out);
  if (out.fail()) {
    safe_set(error) = "Fastid file creation failure";
    return false;
  }
  out.write(reinterpret_cast<char *>(&n), sizeof(size_t));
  if (out.fail()) {
    safe_set(error) = "Fastid write failure";
    return false;
  }
  out.write(reinterpret_cast<char *>(&m), sizeof(size_t));
  if (out.fail()) {
    safe_set(error) = "Fastid write failure";
    return false;
  }
  if (!compressed_write(&out, reinterpret_cast<char *>(p_buffer.get()),
                        sizeof(addr) * m, post_compressor, error))
    return false;
  if (!compressed_write(&out, reinterpret_cast<char *>(q_buffer.get()),
                        sizeof(addr) * m, post_compressor, error))
    return false;
  if (!compressed_write(&out, reinterpret_cast<char *>(offset_buffer.get()),
                        sizeof(addr) * m, post_compressor, error))
    return false;
  if (!compressed_write(&out, text_buffer.get(), n, text_compressor, error))
    return false;
  {
    std::string fastid = "fastid";
    std::string yes = "yes";
    if (!warren->set_parameter(fastid, yes, error))
      return false;
  }
  return true;
}

} // namespace cottontail
