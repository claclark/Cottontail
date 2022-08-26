#include "src/simple_annotator.h"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <memory>
#include <string>
#include <unistd.h>
#include <vector>

#include "src/annotator.h"
#include "src/compressor.h"
#include "src/core.h"
#include "src/idx.h"
#include "src/recipe.h"
#include "src/simple_builder.h"
#include "src/simple_idx.h"
#include "src/simple_posting.h"
#include "src/working.h"

namespace cottontail {

std::shared_ptr<Annotator>
SimpleAnnotator::make(const std::string &recipe,
                      std::shared_ptr<Working> working, std::string *error) {
  if (working == nullptr) {
    safe_set(error) =
        "SimpleAnnotator requires a working director (got nullptr)";
    return nullptr;
  }
  std::string fvalue_compressor_name, fvalue_compressor_recipe;
  std::string posting_compressor_name, posting_compressor_recipe;
  size_t add_file_size = -1;
  if (!interpret_simple_idx_recipe(
          recipe, &fvalue_compressor_name, &fvalue_compressor_recipe,
          &posting_compressor_name, &posting_compressor_recipe, error,
          &add_file_size))
    return nullptr;
  if (!Compressor::check(fvalue_compressor_name, fvalue_compressor_recipe,
                         error))
    return nullptr;
  if (!Compressor::check(posting_compressor_name, posting_compressor_recipe,
                         error))
    return nullptr;
  std::shared_ptr<SimpleAnnotator> ann =
      std::shared_ptr<SimpleAnnotator>(new SimpleAnnotator());
  ann->the_recipe_ = recipe;
  ann->failed_ = false;
  ann->adding_ = false;
  ann->isready_ = false;
  ann->working_ = working;
  ann->multithreaded_ = true;
  ann->posting_compressor_name_ = posting_compressor_name;
  ann->posting_compressor_recipe_ = posting_compressor_recipe;
  ann->posting_compressor_ = Compressor::make(posting_compressor_name,
                                              posting_compressor_recipe, error);
  if (ann->posting_compressor_ == nullptr)
    return nullptr;
  ann->fvalue_compressor_name_ = fvalue_compressor_name;
  ann->fvalue_compressor_recipe_ = fvalue_compressor_recipe;
  ann->fvalue_compressor_ =
      Compressor::make(fvalue_compressor_name, fvalue_compressor_recipe, error);
  if (ann->fvalue_compressor_ == nullptr)
    return nullptr;
  ann->idx_filename_ = working->make_name(IDX_NAME);
  ann->pst_filename_ = working->make_name(PST_NAME);
  ann->cannot_create_temp_files_ = std::make_shared<bool>(new bool());
  *ann->cannot_create_temp_files_ = false;
  if (add_file_size > 0)
    ann->add_file_size_ = add_file_size;
  ann->posting_factory_ = SimplePostingFactory::make(ann->posting_compressor_,
                                                     ann->fvalue_compressor_);
  assert(ann->posting_factory_ != nullptr);
  ann->added_files_.push_back(ann->pst_filename_);
  ann->added_ = std::make_unique<std::vector<Annotation>>();
  assert(ann->added_ != nullptr);
  return ann;
}

bool SimpleAnnotator::check(const std::string &recipe, std::string *error) {
  return SimpleIdx::check(recipe, error);
}

bool SimpleAnnotator::recover(const std::string &recipe, bool commit,
                              std::shared_ptr<Working> working,
                              std::string *error) {
  if (working == nullptr) {
    safe_set(error) =
        "SimpleAnnotator requires a working director (got nullptr)";
    return false;
  }
  std::string idx_filename_ = working->make_name(IDX_NAME);
  std::string pst_filename_ = working->make_name(PST_NAME);
  std::string new_idx_filename = idx_filename_ + std::string(".new");
  std::string new_pst_filename = pst_filename_ + std::string(".new");
  std::string lock_filename = idx_filename_ + std::string(".lock");
  if (commit) {
    std::rename(new_idx_filename.c_str(), idx_filename_.c_str());
    std::rename(new_pst_filename.c_str(), pst_filename_.c_str());
  } else {
    std::remove(new_pst_filename.c_str());
    std::remove(new_idx_filename.c_str());
  }
  std::remove(lock_filename.c_str());
  return true;
}

std::string SimpleAnnotator::recipe_() { return the_recipe_; }

bool SimpleAnnotator::transaction_(std::string *error) {
  if (failed_) {
    safe_set(error) = "SimpleAnnotator in failed state";
    return false;
  }
  lock_.lock();
  if (adding_)
    return true;
  std::string lock_filename = idx_filename_ + std::string(".lock");
  if (link(idx_filename_.c_str(), lock_filename.c_str()) != 0) {
    safe_set(error) = "SimpleAnnotator can't obtain transaction lock";
    lock_.unlock();
    return false;
  }
  adding_ = true;
  lock_.unlock();
  return true;
}

namespace {
bool merge_updates(const std::vector<std::string> &posting_filenames,
                   const std::string &idx_filename,
                   const std::string &pst_filename,
                   std::shared_ptr<SimplePostingFactory> posting_factory,
                   std::string *error = nullptr) {
  std::fstream pst(pst_filename, std::ios::binary | std::ios::out);
  if (pst.fail()) {
    safe_set(error) = "SimpleAnnotator can't create: " + pst_filename;
    return false;
  }
  std::fstream idx(idx_filename, std::ios::binary | std::ios::out);
  if (idx.fail()) {
    safe_set(error) = "SimpleAnnotator can't create: " + idx_filename;
    return false;
  }
  std::vector<std::fstream> posting_fstreams;
  std::vector<std::shared_ptr<SimplePosting>> posting_nexts;
  for (auto &filename : posting_filenames) {
    posting_fstreams.emplace_back(filename, std::ios::binary | std::ios::in);
    if (posting_fstreams.back().fail()) {
      safe_set(error) =
          "SimpleAnnotator can't access posting file: " + filename;
      return false;
    }
    posting_nexts.emplace_back(
        posting_factory->posting_from_file(&posting_fstreams.back()));
  }
  for (;;) {
    addr feature = 0;
    bool feature_found = false;
    for (auto posting : posting_nexts)
      if (feature_found) {
        if (posting != nullptr && posting->feature() < feature)
          feature = posting->feature();
      } else if (posting != nullptr) {
        feature = posting->feature();
        feature_found = true;
      }
    if (!feature_found)
      break;
    std::vector<std::shared_ptr<SimplePosting>> posting_with_feature;
    for (auto posting : posting_nexts)
      if (posting != nullptr && posting->feature() == feature)
        posting_with_feature.push_back(posting);
    assert(posting_with_feature.size() > 0);
    if (posting_with_feature.size() == 1) {
      posting_with_feature[0]->write(&pst);
    } else {
      std::shared_ptr<SimplePosting> merged_posting =
          posting_factory->posting_from_merge(posting_with_feature);
      if (!merged_posting->invariants()) {
        safe_set(error) =
            "SimpleAnnotator can't store a posting list that isn't a GC list";
        return false;
      }
      merged_posting->write(&pst);
    }
    for (size_t i = 0; i < posting_nexts.size(); i++)
      if (posting_nexts[i] != nullptr && posting_nexts[i]->feature() == feature)
        posting_nexts[i] =
            posting_factory->posting_from_file(&posting_fstreams[i]);
    IdxRecord idxr(feature, pst.tellp());
    idx.write(reinterpret_cast<char *>(&idxr), sizeof(idxr));
    if (idx.fail()) {
      safe_set(error) = "SimpleAnnotator can't write to: " + idx_filename;
      return false;
    }
  }
  return true;
}

bool annotation_cmp(const Annotation &a, const Annotation &b) {
  return a.feature < b.feature || (a.feature == b.feature && a.p < b.p);
}
} // namespace

void SimpleAnnotator::maybe_flush_additions(bool force) {
  if (*cannot_create_temp_files_)
    return;
  if ((force && added_->size() > 0) || added_->size() > add_file_size_) {
    auto sorter = [](std::string add_filename,
                     std::unique_ptr<std::vector<Annotation>> added,
                     std::shared_ptr<SimplePostingFactory> posting_factory,
                     std::shared_ptr<bool> cannot_create_temp_files) {
      std::sort(added->begin(), added->end(), annotation_cmp);
      std::fstream addf(add_filename, std::ios::binary | std::ios::out);
      if (addf.fail()) {
        (*cannot_create_temp_files) = true;
        return;
      }
      std::vector<Annotation>::iterator it = added->begin();
      while (it < added->end()) {
        std::shared_ptr<cottontail::SimplePosting> posting =
            posting_factory->posting_from_annotations(&it, added->end());
        posting->write(&addf);
      }
    };
    std::string add_filename = working_->make_temp();
    added_files_.push_back(add_filename);
    if (multithreaded_)
      workers_.emplace_back(std::thread(sorter, add_filename, std::move(added_),
                                        posting_factory_,
                                        cannot_create_temp_files_));
    else
      sorter(add_filename, std::move(added_), posting_factory_,
             cannot_create_temp_files_);
    added_ = std::make_unique<std::vector<Annotation>>();
    assert(added_ != nullptr);
  }
}

bool SimpleAnnotator::annotate_(addr feature, addr p, addr q, fval v,
                                std::string *error) {
  if (failed_) {
    safe_set(error) = "SimpleAnnotator in failed state";
    return false;
  }
  lock_.lock();
  if (!adding_) {
    safe_set(error) = "SimpleAnnotator not in transaction";
    lock_.unlock();
    return false;
  }
  bool outcome = true;
  if (p >= 0 && p <= q) {
    if (*cannot_create_temp_files_) {
      safe_set(error) = "SimpleAnnotator cannot create temporary files";
      outcome = false;
    } else {
      added_->emplace_back(feature, p, q, v);
      maybe_flush_additions(false);
    }
    lock_.unlock();
  }
  return outcome;
}

void SimpleAnnotator::wait_for_workers() {
  for (auto &worker : workers_)
    worker.join();
  workers_.clear();
}

void SimpleAnnotator::cleanup_temporary_files() {
  for (auto &filename : added_files_)
    if (filename != pst_filename_)
      std::remove(filename.c_str());
  added_files_.clear();
  added_files_.push_back(pst_filename_);
}

bool SimpleAnnotator::ready_() {
  lock_.lock();
  if (isready_) {
    lock_.unlock();
    return true;
  }
  if (failed_ || !adding_) {
    lock_.unlock();
    return false;
  }
  maybe_flush_additions(true);
  wait_for_workers();
  if (*cannot_create_temp_files_) {
    cleanup_temporary_files();
    failed_ = true;
    adding_ = false;
    lock_.unlock();
    return false;
  }
  std::string tmp_idx_filename = working_->make_temp();
  std::string tmp_pst_filename = working_->make_temp();
  if (!merge_updates(added_files_, tmp_idx_filename, tmp_pst_filename,
                     posting_factory_)) {
    cleanup_temporary_files();
    std::remove(tmp_idx_filename.c_str());
    std::remove(tmp_pst_filename.c_str());
    lock_.unlock();
    return false;
  }
  cleanup_temporary_files();
  std::string new_idx_filename = idx_filename_ + std::string(".new");
  if (std::rename(tmp_idx_filename.c_str(), new_idx_filename.c_str())) {
    std::remove(tmp_idx_filename.c_str());
    std::remove(tmp_pst_filename.c_str());
    lock_.unlock();
    return false;
  }
  std::string new_pst_filename = pst_filename_ + std::string(".new");
  if (std::rename(tmp_pst_filename.c_str(), new_pst_filename.c_str())) {
    std::remove(new_idx_filename.c_str());
    std::remove(tmp_pst_filename.c_str());
    lock_.unlock();
    return false;
  }
  lock_.unlock();
  adding_ = false;
  isready_ = true;
  return true;
}

void SimpleAnnotator::commit_() {
  lock_.lock();
  assert(!failed_ && !adding_ && isready_);
  std::string new_idx_filename = idx_filename_ + std::string(".new");
  std::rename(new_idx_filename.c_str(), idx_filename_.c_str());
  std::string new_pst_filename = pst_filename_ + std::string(".new");
  std::rename(new_pst_filename.c_str(), pst_filename_.c_str());
  std::string lock_filename = idx_filename_ + std::string(".lock");
  std::remove(lock_filename.c_str());
  isready_ = false;
  lock_.unlock();
}

void SimpleAnnotator::abort_() {
  lock_.lock();
  adding_ = false;
  wait_for_workers();
  cleanup_temporary_files();
  added_ = std::make_unique<std::vector<Annotation>>();
  std::string new_pst_filename = pst_filename_ + std::string(".new");
  std::remove(new_pst_filename.c_str());
  std::string new_idx_filename = idx_filename_ + std::string(".new");
  std::remove(new_idx_filename.c_str());
  std::string lock_filename = idx_filename_ + std::string(".lock");
  std::remove(lock_filename.c_str());
  isready_ = false;
  lock_.unlock();
}

} // namespace cottontail
