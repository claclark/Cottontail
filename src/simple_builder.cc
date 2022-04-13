#include "src/simple_builder.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <regex>
#include <string>
#include <thread>

#include "src/builder.h"
#include "src/compressor.h"
#include "src/core.h"
#include "src/featurizer.h"
#include "src/recipe.h"
#include "src/simple.h"
#include "src/simple_idx.h"
#include "src/simple_posting.h"
#include "src/simple_txt.h"
#include "src/simple_txt_io.h"
#include "src/tokenizer.h"
#include "src/working.h"

namespace cottontail {

namespace {

bool tok_cmp(const TokRecord &a, const TokRecord &b) {
  return a.feature < b.feature || (a.feature == b.feature && a.pq < b.pq);
}

bool annotation_cmp(const Annotation &a, const Annotation &b) {
  return a.feature < b.feature || (a.feature == b.feature && a.p < b.p);
}

template <typename T, bool CMP(const T &, const T &)>
bool merge_records(const std::string &in0_filename,
                   const std::string &in1_filename,
                   const std::string &out_filename, std::string *error) {
  std::fstream in0(in0_filename, std::ios::binary | std::ios::in);
  if (in0.fail()) {
    safe_set(error) = "In merge_records, can't access: " + in0_filename;
    return false;
  }
  std::fstream in1(in1_filename, std::ios::binary | std::ios::in);
  if (in1.fail()) {
    safe_set(error) = "In merge_records, can't access: " + in1_filename;
    return false;
  }
  std::fstream out(out_filename, std::ios::binary | std::ios::out);
  if (out.fail()) {
    safe_set(error) = "In merge_records, can't create: " + out_filename;
    return false;
  }
  T t0, t1;
  in0.read(reinterpret_cast<char *>(&t0), sizeof(t0));
  in1.read(reinterpret_cast<char *>(&t1), sizeof(t1));
  while (!in0.fail() && !in1.fail()) {
    if (CMP(t0, t1)) {
      out.write(reinterpret_cast<char *>(&t0), sizeof(t0));
      in0.read(reinterpret_cast<char *>(&t0), sizeof(t0));
    } else {
      out.write(reinterpret_cast<char *>(&t1), sizeof(t1));
      in1.read(reinterpret_cast<char *>(&t1), sizeof(t1));
    }
  }
  while (!in0.fail()) {
    out.write(reinterpret_cast<char *>(&t0), sizeof(t0));
    in0.read(reinterpret_cast<char *>(&t0), sizeof(t0));
  }
  while (!in1.fail()) {
    out.write(reinterpret_cast<char *>(&t1), sizeof(t1));
    in1.read(reinterpret_cast<char *>(&t1), sizeof(t1));
  }
  if (out.fail()) {
    safe_set(error) = "In merge_records, write failure to: " + out_filename;
    return false;
  }
  return true;
}

template <typename T, bool CMP(const T &, const T &)>
bool sort_records(std::shared_ptr<Working> working,
                  const std::string &in_filename, std::string *out_filename,
                  std::string *error) {
  size_t LOTS = 64 * 1024 * 1024;
  std::fstream in(in_filename, std::ios::binary | std::ios::in);
  if (in.fail()) {
    safe_set(error) = "In sort_records, can't access: " + in_filename;
    return false;
  }
  T t;
  std::vector<T> ts;
  std::queue<std::string> tq;
  in.read(reinterpret_cast<char *>(&t), sizeof(t));
  while (!in.fail()) {
    ts.emplace_back(t);
    if (ts.size() >= LOTS) {
      std::sort(ts.begin(), ts.end(), CMP);
      std::string temp_filename = working->make_temp();
      std::fstream tout(temp_filename, std::ios::binary | std::ios::out);
      if (tout.fail()) {
        safe_set(error) =
            "In sort_records, can't create working file: " + temp_filename;
        return false;
      }
      for (auto &tt : ts)
        tout.write(reinterpret_cast<char *>(&tt), sizeof(tt));
      if (tout.fail()) {
        safe_set(error) = "In sort_records, write failure to: " + temp_filename;
        return false;
      }
      tout.close();
      ts.clear();
      tq.push(temp_filename);
    }
    in.read(reinterpret_cast<char *>(&t), sizeof(t));
  }
  in.close();
  if (ts.size() > 0) {
    std::sort(ts.begin(), ts.end(), CMP);
    std::string temp_filename = working->make_temp();
    std::fstream tout(temp_filename, std::ios::binary | std::ios::out);
    if (tout.fail()) {
      safe_set(error) =
          "In sort_records, can't create working file: " + temp_filename;
      return false;
    }
    for (auto &tt : ts)
      tout.write(reinterpret_cast<char *>(&tt), sizeof(tt));
    if (tout.fail()) {
      safe_set(error) = "In sort_records, write failure to: " + temp_filename;
      return false;
    }
    tout.close();
    ts.clear();
    tq.push(temp_filename);
  }
  std::string filename0;
  while (!tq.empty()) {
    filename0 = tq.front();
    tq.pop();
    if (!tq.empty()) {
      std::string filename1 = tq.front();
      tq.pop();
      std::string filename2 = working->make_temp();
      if (merge_records<T, CMP>(filename0, filename1, filename2, error)) {
        tq.push(filename2);
        std::remove(filename0.c_str());
        std::remove(filename1.c_str());
      } else {
        return false;
      }
    }
  }
  *out_filename = filename0;
  std::remove(in_filename.c_str());
  return true;
}

} // namespace

bool check_annotations(const std::string &filename, std::string *error) {
  std::fstream in(filename, std::ios::binary | std::ios::in);
  if (in.fail()) {
    safe_set(error) = "In check_annotations, can't access: " + filename;
    return false;
  }
  Annotation previous, current;
  in.read(reinterpret_cast<char *>(&previous), sizeof(previous));
  if (in.fail())
    return true;
  if (!(previous.p <= previous.q)) {
    safe_set(error) =
        "In check_annotations, GCL invariant failure in: " + filename;
    return false;
  }
  in.read(reinterpret_cast<char *>(&current), sizeof(current));
  while (!in.fail()) {
    if (!(current.p <= current.q &&
          (previous.feature < current.feature ||
           (previous.feature == current.feature && previous.p < current.p &&
            previous.q < current.q)))) {
      safe_set(error) =
          "In check_annotations, GCL invariant failure in: " + filename;
      return false;
    }
    previous = current;
    in.read(reinterpret_cast<char *>(&current), sizeof(current));
  }
  in.close();
  return true;
}

bool sort_annotations(std::shared_ptr<Working> working,
                      const std::string &in_filename, std::string *out_filename,
                      std::string *error) {
  return sort_records<Annotation, annotation_cmp>(working, in_filename,
                                                  out_filename, error);
}

namespace {

bool name_and_recipe(const std::string &dna, const std::string &key,
                     std::string *error = nullptr, std::string *name = nullptr,
                     std::string *recipe = nullptr) {
  std::map<std::string, std::string> parameters;
  if (!cook(dna, &parameters)) {
    safe_set(error) = "Bad parameters";
    return false;
  }
  std::map<std::string, std::string>::iterator v = parameters.find(key);
  if (v == parameters.end()) {
    safe_set(error) = "No " + key + " found";
    return false;
  }
  std::map<std::string, std::string> k_parameters;
  if (!cook(v->second, &k_parameters)) {
    safe_set(error) = "Bad " + key + " parameters";
    return false;
  }
  std::map<std::string, std::string>::iterator k_name =
      k_parameters.find("name");
  if (k_name == k_parameters.end()) {
    safe_set(error) = "No " + key + " name found";
    return false;
  }
  safe_set(name) = k_name->second;
  std::map<std::string, std::string>::iterator k_recipe =
      k_parameters.find("recipe");
  if (k_recipe == k_parameters.end()) {
    safe_set(error) = "No " + key + " recipe found";
    return false;
  }
  safe_set(recipe) = k_recipe->second;
  return true;
}

const std::string default_dna = "["
                                "  featurizer:["
                                "    name:\"hashing\","
                                "    recipe:\"\","
                                "  ],"
                                "  idx:["
                                "    name:\"simple\","
                                "    recipe:["
                                "      fvalue_compressor:\"zlib\","
                                "      fvalue_compressor_recipe:\"\","
                                "      posting_compressor:\"post\","
                                "      posting_compressor_recipe:\"\","
                                "    ],"
                                "  ],"
                                "  tokenizer:["
                                "    name:\"ascii\","
                                "    recipe:\"xml\","
                                "  ],"
                                "  txt:["
                                "    name:\"simple\","
                                "    recipe:["
                                "      compressor:\"zlib\","
                                "      compressor_recipe:\"\","
                                "    ],"
                                "  ],"
                                "]";
} // namespace

bool SimpleBuilder::check(const std::string &recipe, std::string *error) {
  std::string dna = default_dna;
  std::regex whitespace("\\s+");
  std::vector<std::string> options{
      std::sregex_token_iterator(recipe.begin(), recipe.end(), whitespace, -1),
      {}};
  for (auto &option : options)
    if (!interpret_option(&dna, option, error))
      return false;
  std::string tokenizer_key = "tokenizer";
  std::string tokenizer_name, tokenizer_recipe;
  if (!name_and_recipe(dna, tokenizer_key, error, &tokenizer_name,
                       &tokenizer_recipe))
    return false;
  if (!Tokenizer::check(tokenizer_name, tokenizer_recipe, error))
    return false;
  std::string featurizer_key = "featurizer";
  std::string featurizer_name, featurizer_recipe;
  if (!name_and_recipe(dna, featurizer_key, error, &featurizer_name,
                       &featurizer_recipe))
    return false;
  if (!Featurizer::check(featurizer_name, featurizer_recipe, error))
    return false;
  std::string idx_key = "idx";
  std::string idx_name, idx_recipe;
  if (!name_and_recipe(dna, idx_key, error, &idx_name, &idx_recipe))
    return false;
  if (idx_name != "simple") {
    safe_set(error) = "SimpleBuilder can't build idx: " + idx_name;
    return false;
  }
  if (!SimpleIdx::check(idx_recipe, error))
    return false;
  std::string txt_key = "txt";
  std::string txt_name, txt_recipe;
  if (!name_and_recipe(dna, txt_key, error, &txt_name, &txt_recipe))
    return false;
  if (txt_name != "simple") {
    safe_set(error) = "SimpleBuilder can't build txt: " + txt_name;
    return false;
  }
  if (!SimpleTxt::check(txt_recipe, error))
    return false;
  return true;
}

std::shared_ptr<Builder> SimpleBuilder::make(std::shared_ptr<Working> working,
                                             const std::string &recipe,
                                             std::string *error) {
  if (!check(recipe, error))
    return nullptr;
  std::string dna = default_dna;
  std::regex whitespace("\\s+");
  std::vector<std::string> options{
      std::sregex_token_iterator(recipe.begin(), recipe.end(), whitespace, -1),
      {}};
  for (auto &option : options)
    if (!interpret_option(&dna, option, error))
      return nullptr;
  std::string key;
  std::string posting_compressor_name, posting_compressor_recipe;
  std::string fvalue_compressor_name, fvalue_compressor_recipe;
  std::string text_compressor_name, text_compressor_recipe;
  key = "idx:posting_compressor";
  if (!extract_option(dna, key, &posting_compressor_name, error))
    return nullptr;
  key = "idx:posting_compressor_recipe";
  if (!extract_option(dna, key, &posting_compressor_recipe, error))
    return nullptr;
  key = "idx:fvalue_compressor";
  if (!extract_option(dna, key, &fvalue_compressor_name, error))
    return nullptr;
  key = "idx:fvalue_compressor_recipe";
  if (!extract_option(dna, key, &fvalue_compressor_recipe, error))
    return nullptr;
  key = "txt:compressor";
  if (!extract_option(dna, key, &text_compressor_name, error))
    return nullptr;
  key = "txt:compressor_recipe";
  if (!extract_option(dna, key, &text_compressor_recipe, error))
    return nullptr;
  std::string tokenizer_key = "tokenizer";
  std::string tokenizer_name, tokenizer_recipe;
  if (!name_and_recipe(dna, tokenizer_key, error, &tokenizer_name,
                       &tokenizer_recipe))
    return nullptr;
  std::shared_ptr<cottontail::Tokenizer> tokenizer =
      cottontail::Tokenizer::make(tokenizer_name, tokenizer_recipe, error);
  if (tokenizer == nullptr)
    return nullptr;
  std::string featurizer_key = "featurizer";
  std::string featurizer_name, featurizer_recipe;
  if (!name_and_recipe(dna, featurizer_key, error, &featurizer_name,
                       &featurizer_recipe))
    return nullptr;
  std::shared_ptr<cottontail::Featurizer> featurizer =
      cottontail::Featurizer::make(featurizer_name, featurizer_recipe, error,
                                   working);
  if (featurizer == nullptr)
    return nullptr;
  std::string dna_filename = working->make_name(DNA_NAME);
  std::remove(dna_filename.c_str());
  if (!write_dna(working, dna, error))
    return nullptr;
  return make(working, featurizer, tokenizer, error, DEFAULT_TOK_FILE_SIZE,
              DEFAULT_ANN_FILE_SIZE, posting_compressor_name,
              posting_compressor_recipe, fvalue_compressor_name,
              fvalue_compressor_recipe, text_compressor_name,
              text_compressor_recipe);
}

std::shared_ptr<Builder> SimpleBuilder::make(
    std::shared_ptr<Working> working, std::shared_ptr<Featurizer> featurizer,
    std::shared_ptr<Tokenizer> tokenizer, std::string *error,
    size_t tok_file_size, size_t ann_file_size,
    std::string posting_compressor_name, std::string posting_compressor_recipe,
    std::string fvalue_compressor_name, std::string fvalue_compressor_recipe,
    std::string text_compressor_name, std::string text_compressor_recipe) {
  assert(working != nullptr && featurizer != nullptr && tokenizer != nullptr);
  std::shared_ptr<SimpleBuilder> builder =
      std::shared_ptr<SimpleBuilder>(new SimpleBuilder());
  builder->working_ = working;
  builder->featurizer_ = featurizer;
  builder->tokenizer_ = tokenizer;
  builder->tok_file_size_ = tok_file_size;
  builder->ann_file_size_ = ann_file_size;
  builder->posting_compressor_name_ = posting_compressor_name;
  builder->posting_compressor_recipe_ = posting_compressor_recipe;
  builder->fvalue_compressor_name_ = fvalue_compressor_name;
  builder->fvalue_compressor_recipe_ = fvalue_compressor_recipe;
  builder->text_compressor_name_ = text_compressor_name;
  builder->text_compressor_recipe_ = text_compressor_recipe;
  std::shared_ptr<Compressor> text_compressor =
      Compressor::make(text_compressor_name, text_compressor_recipe, error);
  if (text_compressor == nullptr)
    return nullptr;
  std::string raw_filename = working->make_name(RAW_NAME);
  std::remove(raw_filename.c_str());
  std::string map_filename = working->make_name(MAP_NAME);
  std::remove(map_filename.c_str());
  std::string txt_filename = working->make_name(TXT_NAME);
  std::remove(txt_filename.c_str());
  builder->txt_filename_ = txt_filename;
  std::string idx_filename = working->make_name(IDX_NAME);
  std::remove(idx_filename.c_str());
  builder->idx_filename_ = idx_filename;
  std::string pst_filename = working->make_name(PST_NAME);
  std::remove(pst_filename.c_str());
  builder->pst_filename_ = pst_filename;
  builder->io_ =
      SimpleTxtIO::make(raw_filename, map_filename, TEXT_COMPRESSOR_CHUNK_SIZE,
                        text_compressor, error);
  if (builder->io_ == nullptr)
    return nullptr;
  builder->address_ = 0;
  builder->offset_ = 0;
  builder->txt_.open(builder->txt_filename_, std::ios::binary | std::ios::out);
  if (builder->txt_.fail()) {
    safe_set(error) = "SimpleBuilder can't create :" + builder->txt_filename_;
    return nullptr;
  }
  std::shared_ptr<Compressor> posting_compressor = Compressor::make(
      posting_compressor_name, posting_compressor_recipe, error);
  if (posting_compressor == nullptr)
    return nullptr;
  std::shared_ptr<Compressor> fvalue_compressor =
      Compressor::make(fvalue_compressor_name, fvalue_compressor_recipe, error);
  if (fvalue_compressor == nullptr)
    return nullptr;
  builder->posting_factory_ =
      SimplePostingFactory::make(posting_compressor, fvalue_compressor);
  assert(builder->posting_factory_ != nullptr);
  builder->tokens_ = std::make_unique<std::vector<TokRecord>>();
  assert(builder->tokens_ != nullptr);
  builder->annotations_ = std::make_unique<std::vector<Annotation>>();
  assert(builder->annotations_ != nullptr);
  return builder;
};

bool SimpleBuilder::maybe_flush_tokens(bool force, std::string *error) {
  if ((force && tokens_->size() > 0) || tokens_->size() > tok_file_size_) {
    if (multithreaded_) {
      auto token_sorter =
          [](std::string tok_filename,
             std::unique_ptr<std::vector<TokRecord>> tokens,
             std::shared_ptr<SimplePostingFactory> posting_factory) {
            std::sort(tokens->begin(), tokens->end(), tok_cmp);
            std::fstream tok(tok_filename, std::ios::binary | std::ios::out);
            assert(!tok.fail());
            std::vector<cottontail::TokRecord>::iterator it = tokens->begin();
            while (it < tokens->end()) {
              std::shared_ptr<cottontail::SimplePosting> posting =
                  posting_factory->posting_from_tokens(&it, tokens->end());
              posting->write(&tok);
            }
          };
      if (verbose())
        std::cout << "SimpleBuilder starting thread to sort token buffer\n";
      std::string tok_filename = working_->make_temp();
      tokq_.push(tok_filename);
      workers_.emplace_back(std::thread(token_sorter, tok_filename,
                                        std::move(tokens_), posting_factory_));
      tokens_ = std::make_unique<std::vector<TokRecord>>();
    } else {
      if (verbose())
        std::cout << "SimpleBuilder sorting token buffer\n";
      std::sort(tokens_->begin(), tokens_->end(), tok_cmp);
      if (verbose())
        std::cout << "SimpleBuilder flushing tokens\n";
      std::string tok_filename = working_->make_temp();
      std::fstream tok(tok_filename, std::ios::binary | std::ios::out);
      if (tok.fail()) {
        safe_set(error) =
            "SimpleBuilder can't create temporary file for tokens";
        return false;
      }
      std::vector<cottontail::TokRecord>::iterator it = tokens_->begin();
      while (it < tokens_->end()) {
        std::shared_ptr<cottontail::SimplePosting> posting =
            posting_factory_->posting_from_tokens(&it, tokens_->end());
        posting->write(&tok);
      }
      tokq_.push(tok_filename);
      tokens_->clear();
    }
  }
  return true;
}

bool SimpleBuilder::build_index(std::string *error) {
  std::fstream idx(idx_filename_, std::ios::binary | std::ios::out);
  if (idx.fail()) {
    safe_set(error) = "SimpleBuilder can't open: " + idx_filename_;
    return false;
  }
  std::fstream pst(pst_filename_, std::ios::binary | std::ios::out);
  if (pst.fail()) {
    safe_set(error) = "SimpleBuilder can't open: " + pst_filename_;
    return false;
  }
  if (tokq_.empty())
    return true; // empty index
  std::vector<std::fstream> posting_fstreams;
  std::vector<std::shared_ptr<SimplePosting>> posting_nexts;
  std::queue<std::string> tmpq = tokq_;
  while (!tmpq.empty()) {
    std::string tok_filename = tmpq.front();
    tmpq.pop();
    posting_fstreams.emplace_back(tok_filename,
                                  std::ios::binary | std::ios::in);
    if (posting_fstreams.back().fail()) {
      safe_set(error) =
          "SimpleBuilder can't access token file: " + tok_filename;
      return false;
    }
    posting_nexts.emplace_back(
        posting_factory_->posting_from_file(&posting_fstreams.back()));
  }
  tmpq = annq_;
  while (!tmpq.empty()) {
    std::string ann_filename = tmpq.front();
    tmpq.pop();
    posting_fstreams.emplace_back(ann_filename,
                                  std::ios::binary | std::ios::in);
    if (posting_fstreams.back().fail()) {
      safe_set(error) =
          "SimpleBuilder can't access annotation file: " + ann_filename;
      return false;
    }
    posting_nexts.emplace_back(
        posting_factory_->posting_from_file(&posting_fstreams.back()));
  }
  for (;;) {
    addr feature = 0;
    bool feature_found = false;
    for (auto &posting : posting_nexts)
      if (feature_found) {
        if (posting != nullptr && posting->feature() < feature)
          feature = posting->feature();
      } else if (posting != nullptr) {
        feature = posting->feature();
        feature_found = true;
      }
    if (!feature_found)
      break;
    std::shared_ptr<SimplePosting> merged_posting =
        posting_factory_->posting_from_feature(feature);
    for (size_t i = 0; i < posting_nexts.size(); i++)
      if (posting_nexts[i] != nullptr &&
          posting_nexts[i]->feature() == feature) {
        merged_posting->append(posting_nexts[i]);
        posting_nexts[i] =
            posting_factory_->posting_from_file(&posting_fstreams[i]);
      }
    // If you got here because the following invariant failed, then either:
    //   1) your builder isn't adding tokens/annonations in increasing order, or
    //   2) your tokens/annotations nest (but it's okay if they overlap).
    assert(merged_posting->invariants());
    merged_posting->write(&pst);
    IdxRecord idxr(feature, pst.tellp());
    idx.write(reinterpret_cast<char *>(&idxr), sizeof(idxr));
    if (idx.fail()) {
      safe_set(error) = "SimpleBuilder can't write to: " + idx_filename_;
      return false;
    }
  }
  while (!tokq_.empty()) {
    std::string remove_me = tokq_.front();
    std::remove(remove_me.c_str());
    tokq_.pop();
  }
  while (!annq_.empty()) {
    std::string remove_me = annq_.front();
    std::remove(remove_me.c_str());
    annq_.pop();
  }
  return true;
}

bool SimpleBuilder::add_text_(const std::string &text, addr *p, addr *q,
                              std::string *error) {
  // Even if there's no text, we still add a newline character,
  // which makes sure that a token at the end of one text doesn't get
  // glued to a token at the start of the next text.
  std::unique_ptr<char[]> buffer =
      std::unique_ptr<char[]>(new char[text.size() + 2]);
  memcpy(buffer.get(), text.c_str(), text.size());
  buffer[text.size()] = '\n';
  buffer[text.size() + 1] = '\0';
  addr length = text.size() + 1;
  io_->append(buffer.get(), length);
  std::vector<Token> tokens =
      tokenizer_->tokenize(featurizer_, buffer.get(), length);
  if (tokens.size() == 0) {
    // There are no actual tokens in this text, but we still want
    // add_annotation to work correctly with the returned values.
    *p = address_ + 1;
    *q = address_;
    offset_ += length;
    return true;
  }
  addr last_address = address_ + tokens[tokens.size() - 1].address;
  TxtRecord txtr;
  addr last_pq = -1;
  for (auto &token : tokens) {
    token.address += address_;
    tokens_->emplace_back(token.feature, token.address);
    if (token.address % TXT_BLOCKING == 0 && token.address != last_pq) {
      txtr.pq = token.address;
      txtr.start = offset_ + token.offset;
      txtr.end = offset_ + token.offset + token.length - 1;
      txt_.write(reinterpret_cast<char *>(&txtr), sizeof(txtr));
      if (txt_.fail()) {
        safe_set(error) = "SimpleBuilder can't write to: " + txt_filename_;
        return false;
      }
      last_pq = token.address;
    }
  }
  *p = address_;
  *q = last_address;
  address_ = (last_address + 1);
  offset_ += length;
  return maybe_flush_tokens(false, error);
};

bool SimpleBuilder::maybe_flush_annotations(bool force, std::string *error) {
  if ((force && annotations_->size() > 0) ||
      annotations_->size() > ann_file_size_) {
    auto annotation_sorter =
        [](std::string ann_filename,
           std::unique_ptr<std::vector<Annotation>> annotations,
           std::shared_ptr<SimplePostingFactory> posting_factory) {
          std::sort(annotations->begin(), annotations->end(), annotation_cmp);
          std::fstream ann(ann_filename, std::ios::binary | std::ios::out);
          assert(!ann.fail());
          std::vector<Annotation>::iterator it = annotations->begin();
          while (it < annotations->end()) {
            std::shared_ptr<cottontail::SimplePosting> posting =
                posting_factory->posting_from_annotations(&it,
                                                          annotations->end());
            posting->write(&ann);
          }
        };
    std::string ann_filename = working_->make_temp();
    annq_.push(ann_filename);
    if (multithreaded_) {
      if (verbose())
        std::cout << "SimpleBuilder starting thread to sort annotations\n";
      workers_.emplace_back(std::thread(annotation_sorter, ann_filename,
                                        std::move(annotations_),
                                        posting_factory_));
    } else {
      if (verbose())
        std::cout << "SimpleBuilder sorting annotations\n";
      annotation_sorter(ann_filename, std::move(annotations_),
                        posting_factory_);
    }
    annotations_ = std::make_unique<std::vector<Annotation>>();
    assert(annotations_ != nullptr);
  }
  return true;
};

bool SimpleBuilder::add_annotation_(const std::string &tag, addr p, addr q,
                                    fval v, std::string *error) {
  // for various reasons, it should be correct to silently ignore invalid
  // annotations
  if (p >= 0 && p <= q) {
    annotations_->emplace_back(featurizer_->featurize(tag), p, q, v);
    maybe_flush_annotations(false, error);
  }
  return true;
};

bool SimpleBuilder::finalize_(std::string *error) {
  if (verbose())
    std::cout << "SimpleBuilder finalizing\n";
  txt_.close();
  io_->flush();
  if (!maybe_flush_tokens(true, error))
    return false;
  if (!maybe_flush_annotations(true, error))
    return false;
  if (multithreaded_) {
    if (verbose())
      std::cout << "SimpleBuilder gathering threads\n";
    for (auto &worker : workers_)
      worker.join();
  }
  if (verbose())
    std::cout << "SimpleBuilder building inverted index\n";
  if (!build_index(error))
    return false;
  if (verbose())
    std::cout << "SimpleBuilder writing parameters\n";
  std::map<std::string, std::string> txt_recipe;
  txt_recipe["compressor"] = text_compressor_name_;
  txt_recipe["compressor_recipe"] = text_compressor_recipe_;
  std::map<std::string, std::string> txt_parameters;
  txt_parameters["name"] = "simple";
  txt_parameters["recipe"] = freeze(txt_recipe);
  std::map<std::string, std::string> idx_recipe;
  idx_recipe["posting_compressor"] = posting_compressor_name_;
  idx_recipe["posting_compressor_recipe"] = posting_compressor_recipe_;
  idx_recipe["fvalue_compressor"] = fvalue_compressor_name_;
  idx_recipe["fvalue_compressor_recipe"] = fvalue_compressor_recipe_;
  std::map<std::string, std::string> idx_parameters;
  idx_parameters["name"] = "simple";
  idx_parameters["recipe"] = freeze(idx_recipe);
  std::map<std::string, std::string> tokenizer_parameters;
  tokenizer_parameters["name"] = tokenizer_->name();
  tokenizer_parameters["recipe"] = tokenizer_->recipe();
  std::map<std::string, std::string> featurizer_parameters;
  featurizer_parameters["name"] = featurizer_->name();
  featurizer_parameters["recipe"] = featurizer_->recipe();
  std::map<std::string, std::string> warren_parameters;
  warren_parameters["warren"] = "simple";
  warren_parameters["txt"] = freeze(txt_parameters);
  warren_parameters["idx"] = freeze(idx_parameters);
  warren_parameters["tokenizer"] = freeze(tokenizer_parameters);
  warren_parameters["featurizer"] = freeze(featurizer_parameters);
  std::string dna = freeze(warren_parameters);
  if (!write_dna(working_, dna, error))
    return false;
  if (verbose())
    std::cout << "SimpleBuilder finalized\n";
  return true;
};

} // namespace cottontail
