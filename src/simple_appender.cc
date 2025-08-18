#include "src/simple_appender.h"

#include <memory>
#include <string>
#include <unistd.h>

#include "src/annotator.h"
#include "src/committable.h"
#include "src/compressor.h"
#include "src/core.h"
#include "src/simple.h"
#include "src/simple_txt.h"
#include "src/simple_txt_io.h"
#include "src/tokenizer.h"
#include "src/working.h"

namespace cottontail {

std::shared_ptr<Appender>
SimpleAppender::make(const std::string &recipe, std::string *error,
                     std::shared_ptr<Working> working,
                     std::shared_ptr<Featurizer> featurizer,
                     std::shared_ptr<Tokenizer> tokenizer,
                     std::shared_ptr<Annotator> annotator) {
  std::string compressor_name, compressor_recipe;
  if (!interpret_simple_txt_recipe(recipe, &compressor_name, &compressor_recipe,
                                   error))
    return nullptr;
  if (!Compressor::check(compressor_name, compressor_recipe, error))
    return nullptr;
  if (working == nullptr) {
    safe_error(error) =
        "SimpleAppender requires a working directory (got nullptr)";
    return nullptr;
  }
  if (featurizer == nullptr) {
    safe_error(error) = "SimpleAppender requires a featurizer (got nullptr)";
    return nullptr;
  }
  if (tokenizer == nullptr) {
    safe_error(error) = "SimpleAppender requires a tokenizer (got nullptr)";
    return nullptr;
  }
  if (annotator == nullptr) {
    safe_error(error) = "SimpleAppender requires an annotator (got nullptr)";
    return nullptr;
  }
  std::shared_ptr<SimpleAppender> appender =
      std::shared_ptr<SimpleAppender>(new SimpleAppender());
  appender->failed_ = false;
  appender->recipe_string_ = recipe;
  appender->featurizer_ = featurizer;
  appender->tokenizer_ = tokenizer;
  appender->annotator_ = annotator;
  appender->txt_filename_ = working->make_name(TXT_NAME);
  appender->raw_filename_ = working->make_name(RAW_NAME);
  appender->map_filename_ = working->make_name(MAP_NAME);
  appender->compressor_name_ = compressor_name;
  appender->compressor_recipe_ = compressor_recipe;
  appender->text_compressor_chunk_size_ = TEXT_COMPRESSOR_CHUNK_SIZE;
  appender->io_ = nullptr;
  appender->adding_ = false;
  return appender;
}

bool SimpleAppender::check(const std::string &recipe, std::string *error) {
  std::string compressor_name, compressor_recipe;
  return interpret_simple_txt_recipe(recipe, &compressor_name,
                                     &compressor_recipe, error);
}

std::string SimpleAppender::recipe_() { return recipe_string_; }

bool SimpleAppender::recover(const std::string &recipe, bool commit,
                             std::string *error,
                             std::shared_ptr<Working> working) {
  if (working == nullptr) {
    safe_error(error) =
        "SimpleAppender recovery requires a working directory (got nullptr)";
    return false;
  }
  std::string raw_filename = working->make_name(RAW_NAME);
  std::string map_filename = working->make_name(MAP_NAME);
  std::string compressor_name, compressor_recipe;
  if (!interpret_simple_txt_recipe(recipe, &compressor_name, &compressor_recipe,
                                   error))
    return false;
  std::shared_ptr<Compressor> text_compressor =
      Compressor::make(compressor_name, compressor_recipe, error);
  if (text_compressor == nullptr)
    return false;
  SimpleTxtIO::recover(raw_filename, map_filename, TEXT_COMPRESSOR_CHUNK_SIZE,
                       text_compressor, commit);
  std::string txt_filename = working->make_name(TXT_NAME);
  std::string new_filename = txt_filename + ".new";
  if (commit)
    std::rename(new_filename.c_str(), txt_filename.c_str());
  else
    std::remove(new_filename.c_str());
  std::string lock_filename = txt_filename + ".lock";
  std::remove(lock_filename.c_str());
  return true;
}

bool SimpleAppender::append_(const std::string &text, addr *p, addr *q,
                             std::string *error) {
  lock_.lock();
  if (failed_) {
    safe_error(error) = "SimpleAppender in failed state";
    lock_.unlock();
    return false;
  }
  if (!adding_) {
    safe_error(error) = "SimpleAppender not in transaction";
    lock_.unlock();
    return false;
  }
  if (text.size() == 0) {
    *p = address_ + 1;
    *q = address_;
    lock_.unlock();
    return true;
  }
  addr length = text.size();
  std::unique_ptr<char[]> buffer =
      std::unique_ptr<char[]>(new char[length + 2]);
  memcpy(buffer.get(), text.c_str(), length);
  if (!separator(buffer[length - 1]))
    buffer[length++] = '\n';
  buffer[length] = '\0';
  io_->append(buffer.get(), length);
  std::vector<Token> tokens =
      tokenizer_->tokenize(featurizer_, buffer.get(), length);
  if (tokens.size() == 0) {
    // There are no actual tokens in this text, but we still want
    // annotation to work correctly with the returned values.
    *p = address_ + 1;
    *q = address_;
    offset_ += length;
    lock_.unlock();
    return true;
  }
  addr last_address = address_ + tokens[tokens.size() - 1].address;
  TxtRecord txtr;
  addr last_pq = -1;
  fval zero = 0.0;
  for (auto &token : tokens) {
    token.address += address_;
    if (!annotator_->annotate(token.feature, token.address, token.address, zero,
                              error)) {
      failed_ = true;
      lock_.unlock();
      return false;
    }
    if (token.address % TXT_BLOCKING == 0 && token.address != last_pq) {
      txtr.pq = token.address;
      txtr.start = offset_ + token.offset;
      txtr.end = offset_ + token.offset + token.length - 1;
      txt_.write(reinterpret_cast<char *>(&txtr), sizeof(txtr));
      if (txt_.fail()) {
        safe_error(error) = "SimpleAppender can't write to: " + txt_filename_;
        failed_ = true;
        lock_.unlock();
        return false;
      }
    }
  }
  *p = address_;
  *q = last_address;
  address_ = (last_address + 1);
  offset_ += length;
  lock_.unlock();
  return true;
}

bool SimpleAppender::transaction_(std::string *error) {
  lock_.lock();
  if (failed_) {
    safe_error(error) = "SimpleAppender in failed state";
    lock_.unlock();
    return false;
  }
  std::string lock_filename = txt_filename_ + ".lock";
  if (link(txt_filename_.c_str(), lock_filename.c_str()) != 0) {
    safe_error(error) = "SimpleAppender can't obtain transaction lock";
    lock_.unlock();
    return false;
  }
  if (io_ == nullptr) {
    std::shared_ptr<Compressor> text_compressor =
        Compressor::make(compressor_name_, compressor_recipe_, error);
    if (text_compressor == nullptr) {
      failed_ = true;
      std::remove(lock_filename.c_str());
      lock_.unlock();
      return false;
    }
    io_ =
        SimpleTxtIO::make(raw_filename_, map_filename_,
                          text_compressor_chunk_size_, text_compressor, error);
    if (io_ == nullptr) {
      failed_ = true;
      std::remove(lock_filename.c_str());
      lock_.unlock();
      return false;
    }
  }
  if (!io_->transaction(error)) {
    std::remove(lock_filename.c_str());
    lock_.unlock();
    return false;
  }
  std::fstream old(txt_filename_, std::ios::binary | std::ios::in);
  if (old.fail()) {
    safe_error(error) = "SimpleAppender can't access: " + txt_filename_;
    failed_ = true;
    io_->abort();
    std::remove(lock_filename.c_str());
    lock_.unlock();
    return false;
  }
  old.seekg(0, old.end);
  addr old_end = old.tellg();
  old.seekg(0, old.beg);
  if (old_end % static_cast<addr>(sizeof(struct TxtRecord)) != 0) {
    safe_error(error) =
        "SimpleAppender found sizing error in: " + txt_filename_;
    failed_ = true;
    io_->abort();
    std::remove(lock_filename.c_str());
    lock_.unlock();
    return false;
  }
  std::string new_filename = txt_filename_ + ".new";
  txt_.open(new_filename, std::ios::binary | std::ios::out);
  if (txt_.fail()) {
    safe_error(error) = "SimpleAppender can't create :" + new_filename;
    failed_ = true;
    io_->abort();
    std::remove(lock_filename.c_str());
    lock_.unlock();
    return false;
  }
  TxtRecord txtr(0, 0, 0);
  addr records = 0;
  old.read(reinterpret_cast<char *>(&txtr), sizeof(txtr));
  while (!old.fail()) {
    records++;
    txt_.write(reinterpret_cast<char *>(&txtr), sizeof(txtr));
    if (txt_.fail()) {
      safe_error(error) = "SimpleAppender can't write :" + new_filename;
      failed_ = true;
      txt_.close();
      std::remove(new_filename.c_str());
      io_->abort();
      std::remove(lock_filename.c_str());
      lock_.unlock();
      return false;
    }
    old.read(reinterpret_cast<char *>(&txtr), sizeof(txtr));
  }
  old.close();
  offset_ = io_->size();
  if (records == 0) {
    address_ = 0;
  } else if (offset_ <= txtr.start) {
    safe_error(error) =
        "SimpleAppender found sizing error in: " + txt_filename_;
    failed_ = true;
    txt_.close();
    std::remove(new_filename.c_str());
    io_->abort();
    std::remove(lock_filename.c_str());
    lock_.unlock();
    return false;
  } else {
    addr actual;
    std::unique_ptr<char[]> tail =
        io_->read(txtr.start, offset_ - txtr.start, &actual);
    if (tail == nullptr || actual != offset_ - txtr.start) {
      safe_error(error) =
          "SimpleAppender found sizing error in: " + txt_filename_;
      failed_ = true;
      txt_.close();
      std::remove(new_filename.c_str());
      io_->abort();
      std::remove(lock_filename.c_str());
      lock_.unlock();
      return false;
    }
    std::string tail_string = tail.get();
    std::vector<std::string> tail_tokens = tokenizer_->split(tail_string);
    if (tail_tokens.size() == 0) {
      safe_error(error) =
          "SimpleAppender found sizing error in: " + txt_filename_;
      failed_ = true;
      txt_.close();
      std::remove(new_filename.c_str());
      io_->abort();
      std::remove(lock_filename.c_str());
      lock_.unlock();
      return false;
    }
    address_ = txtr.pq + tail_tokens.size();
  }
  adding_ = true;
  lock_.unlock();
  return true;
}

bool SimpleAppender::ready_() {
  lock_.lock();
  bool result = !failed_ && adding_ && io_->ready();
  lock_.unlock();
  return result;
}

void SimpleAppender::commit_() {
  lock_.lock();
  if (io_ != nullptr)
    io_->commit();
  if (!failed_ && adding_) {
    txt_.close();
    std::string new_filename = txt_filename_ + ".new";
    std::rename(new_filename.c_str(), txt_filename_.c_str());
    std::string lock_filename = txt_filename_ + ".lock";
    std::remove(lock_filename.c_str());
  }
  adding_ = false;
  lock_.unlock();
}

void SimpleAppender::abort_() {
  lock_.lock();
  if (io_ != nullptr)
    io_->abort();
  if (!failed_ && adding_) {
    txt_.close();
    std::string new_filename = txt_filename_ + ".new";
    std::remove(new_filename.c_str());
    std::string lock_filename = txt_filename_ + ".lock";
    std::remove(lock_filename.c_str());
  }
  adding_ = false;
  lock_.unlock();
}

} // namespace cottontail
