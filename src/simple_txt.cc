#include "src/simple_txt.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <string>

#include "src/core.h"
#include "src/recipe.h"
#include "src/simple.h"
#include "src/txt.h"

namespace cottontail {

bool SimpleTxt::load_map(const std::string &txt_filename, std::string *error) {
  std::fstream txtf(txt_filename, std::ios::binary | std::ios::in);
  if (txtf.fail()) {
    safe_error(error) = "SimpleTxt can't access: " + txt_filename;
    return false;
  }
  txtf.seekg(0, txtf.end);
  addr txtf_end = txtf.tellg();
  txtf.seekg(0, txtf.beg);
  if (txtf_end % static_cast<addr>(sizeof(struct TxtRecord)) != 0) {
    safe_error(error) = "SimpleTxt found sizing error in: " + txt_filename;
    return false;
  }
  map_size_ = txtf_end / sizeof(TxtRecord) + 1;
  map_ = shared_array<addr>(map_size_);
  addr *next = map_.get();
  TxtRecord txt;
  addr last_pq = -1;
  map_blocking_ = -1;
  txtf.read(reinterpret_cast<char *>(&txt), sizeof(txt));
  while (!txtf.fail()) {
    if (last_pq > -1) {
      addr current_blocking = txt.pq - last_pq;
      if (map_blocking_ > -1) {
        if (current_blocking != map_blocking_) {
          safe_error(error) =
              "SimpleTxt found blocking error in: " + txt_filename;
          return false;
        }
      } else {
        map_blocking_ = current_blocking;
      }
    }
    last_pq = txt.pq;
    *next++ = txt.start;
    txtf.read(reinterpret_cast<char *>(&txt), sizeof(txt));
  }
  *next++ = io_->size();
  if (next - map_.get() != map_size_) {
    safe_error(error) = "SimpleTxt got loading error in: " + txt_filename;
    return false;
  }
  return true;
}

bool interpret_simple_txt_recipe(const std::string &recipe,
                                 std::string *compressor_name,
                                 std::string *compressor_recipe,
                                 std::string *error) {

  if (recipe == "") {
    *compressor_name = TEXT_COMPRESSOR_NAME;
    *compressor_recipe = TEXT_COMPRESSOR_RECIPE;
  } else {
    std::map<std::string, std::string> parameters;
    if (!cook(recipe, &parameters, error))
      return false;
    std::map<std::string, std::string>::iterator item;
    item = parameters.find("compressor");
    if (item != parameters.end())
      *compressor_name = item->second;
    else
      *compressor_name = TEXT_COMPRESSOR_NAME;
    item = parameters.find("compressor_recipe");
    if (item != parameters.end())
      *compressor_recipe = item->second;
    else
      *compressor_recipe = TEXT_COMPRESSOR_RECIPE;
  }
  return true;
}

std::shared_ptr<Txt> SimpleTxt::make(const std::string &recipe,
                                     std::shared_ptr<Tokenizer> tokenizer,
                                     std::shared_ptr<Working> working,
                                     std::string *error) {
  std::string compressor_name, compressor_recipe;
  if (!interpret_simple_txt_recipe(recipe, &compressor_name, &compressor_recipe,
                                   error))
    return nullptr;
  if (!Compressor::check(compressor_name, compressor_recipe, error))
    return nullptr;
  if (tokenizer == nullptr) {
    safe_error(error) = "SimpleTxt requires a tokenizer (got nullptr)";
    return nullptr;
  }
  if (working == nullptr) {
    safe_error(error) = "SimpleTxt requires a working directory (got nullptr)";
    return nullptr;
  }
  std::shared_ptr<SimpleTxt> txt = std::shared_ptr<SimpleTxt>(new SimpleTxt());
  txt->tokenizer_ = tokenizer;
  txt->compressor_name_ = compressor_name;
  txt->compressor_recipe_ = compressor_recipe;
  std::shared_ptr<Compressor> text_compressor =
      Compressor::make(compressor_name, compressor_recipe, error);
  std::string raw_filename = working->make_name(RAW_NAME);
  std::string map_filename = working->make_name(MAP_NAME);
  txt->io_ =
      SimpleTxtIO::make(raw_filename, map_filename, TEXT_COMPRESSOR_CHUNK_SIZE,
                        text_compressor, error);
  if (txt->io_ == nullptr)
    return nullptr;
  std::string txt_filename = working->make_name(TXT_NAME);
  if (!txt->load_map(txt_filename, error))
    return nullptr;
  txt->computed_tokens_valid_ = false;
  txt->memo_valid_ = false;
  return txt;
}

bool SimpleTxt::check(const std::string &recipe, std::string *error) {
  if (recipe == "")
    return true;
  std::string compressor_name, compressor_recipe;
  if (!interpret_simple_txt_recipe(recipe, &compressor_name, &compressor_recipe,
                                   error))
    return false;
  return Compressor::check(compressor_name, compressor_recipe, error);
}

std::string SimpleTxt::recipe_() {
  std::map<std::string, std::string> parameters;
  parameters["compressor"] = compressor_name_;
  parameters["compressor_recipe"] = compressor_recipe_;
  return freeze(parameters);
}

std::shared_ptr<Txt> SimpleTxt::clone_(std::string *error) {
  const std::lock_guard<std::mutex> lock(mutex_);
  std::shared_ptr<SimpleTxtIO> io = io_->clone(error);
  if (io == nullptr)
    return nullptr;
  std::shared_ptr<SimpleTxt> txt = std::shared_ptr<SimpleTxt>(new SimpleTxt());
  if (txt == nullptr) {
    safe_error(error) = "Failed to create SimpleTxt clone";
    return nullptr;
  }
  txt->io_ = io;
  txt->tokenizer_ = tokenizer_;
  txt->map_ = map_;
  txt->map_size_ = map_size_;
  txt->map_blocking_ = map_blocking_;
  txt->computed_tokens_ = computed_tokens_;
  txt->computed_tokens_valid_ = computed_tokens_valid_;
  txt->memo_valid_ = false;
  txt->compressor_name_ = compressor_name_;
  txt->compressor_recipe_ = compressor_recipe_;
  return txt;
}

std::string SimpleTxt::translate_(addr p, addr q) {
  if (p == maxfinity)
    return "";
  if (q == maxfinity)
    --q;
  if (p < 0)
    p = 0;
  if (p > q)
    return "";
  addr p_block = (map_blocking_ == -1 ? 0 : p / map_blocking_);
  if (p_block >= map_size_ - 1)
    return "";
  addr offset = map_.get()[p_block];
  addr q_block = (map_blocking_ == -1 ? 1 : q / map_blocking_ + 1);
  if (q_block >= map_size_)
    q_block = map_size_ - 1;
  addr size = map_.get()[q_block] - offset;
  addr token_skip = p - p_block * map_blocking_;
  addr token_count = q - p + 1;
  addr token_total = token_skip + token_count;
  const std::lock_guard<std::mutex> lock(mutex_);
  if (memo_valid_ && memo_offset_ == offset && memo_skip_ <= token_skip &&
      memo_end_ <= size) {
    offset += memo_end_;
    size -= memo_end_;
    token_skip -= memo_skip_;
  } else {
    memo_offset_ = offset;
    memo_end_ = 0;
  }
  memo_valid_ = false;
  std::unique_ptr<char[]> buffer_unique_ptr = io_->read(offset, size, &size);
  const char *buffer = buffer_unique_ptr.get();
  if (buffer == nullptr || size <= 0)
    return "";
  const char *start = tokenizer_->skip(buffer, size, token_skip);
  if (start >= buffer + size)
    return "";
  const char *end =
      tokenizer_->skip(start, size - (start - buffer), token_count);
  if (end < buffer + size) {
    memo_skip_ = token_total;
    memo_end_ += end - buffer;
    memo_valid_ = true;
  }
  return std::string(start, end - start);
}

addr SimpleTxt::tokens_() {
  if (computed_tokens_valid_)
    return computed_tokens_;
  addr full_blocks;
  if (map_size_ < 3)
    full_blocks = 0;
  else
    full_blocks = map_blocking_ * (map_size_ - 2);
  std::string tail_string = translate(full_blocks, maxfinity);
  std::vector<std::string> tail_tokens = tokenizer_->split(tail_string);
  const std::lock_guard<std::mutex> lock(mutex_);
  if (computed_tokens_valid_)
    return computed_tokens_;
  computed_tokens_ = full_blocks + tail_tokens.size();
  computed_tokens_valid_ = true;
  return computed_tokens_;
}

} // namespace cottontail
