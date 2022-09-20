#include "src/simple_idx.h"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "src/array_hopper.h"
#include "src/compressor.h"
#include "src/core.h"
#include "src/hopper.h"
#include "src/idx.h"
#include "src/recipe.h"
#include "src/simple_builder.h"
#include "src/simple_posting.h"
#include "src/working.h"

namespace cottontail {
bool interpret_simple_idx_recipe(const std::string &recipe,
                                 std::string *fvalue_compressor_name,
                                 std::string *fvalue_compressor_recipe,
                                 std::string *posting_compressor_name,
                                 std::string *posting_compressor_recipe,
                                 std::string *error, size_t *add_file_size) {

  if (recipe == "") {
    *fvalue_compressor_name = FVALUE_COMPRESSOR_NAME;
    *fvalue_compressor_recipe = FVALUE_COMPRESSOR_RECIPE;
    *posting_compressor_name = POSTING_COMPRESSOR_NAME;
    *posting_compressor_recipe = POSTING_COMPRESSOR_RECIPE;
  } else {
    std::map<std::string, std::string> parameters;
    if (!cook(recipe, &parameters, error))
      return false;
    std::map<std::string, std::string>::iterator item;
    item = parameters.find("fvalue_compressor");
    if (item != parameters.end()) {
      *fvalue_compressor_name = item->second;
    } else {
      safe_set(error) = "interpret_simple_idx_recipe missing fvalue_compressor";
      return false;
    }
    item = parameters.find("fvalue_compressor_recipe");
    if (item != parameters.end()) {
      *fvalue_compressor_recipe = item->second;
    } else {
      safe_set(error) =
          "interpret_simple_idx_recipe missing fvalue_compressor_recipe";
      return false;
    }
    item = parameters.find("posting_compressor");
    if (item != parameters.end()) {
      *posting_compressor_name = item->second;
    } else {
      safe_set(error) =
          "interpret_simple_idx_recipe missing posting_compressor";
      return false;
    }
    item = parameters.find("posting_compressor_recipe");
    if (item != parameters.end()) {
      *posting_compressor_recipe = item->second;
    } else {
      safe_set(error) =
          "interpret_simple_idx_recipe missing posting_compressor_recipe";
      return false;
    }
    if (add_file_size != nullptr) {
      item = parameters.find("add_file_size");
      if (item != parameters.end()) {
        try {
          *add_file_size = std::stoi(item->second);
        } catch (const std::invalid_argument &e) {
          safe_set(error) =
              "interpret_simple_idx_recipe got invalid add_file_size";
        }
      }
    }
  }
  return true;
}

std::shared_ptr<Idx> SimpleIdx::make(const std::string &recipe,
                                     std::shared_ptr<Working> working,
                                     std::string *error) {
  std::string fvalue_compressor_name, fvalue_compressor_recipe;
  std::string posting_compressor_name, posting_compressor_recipe;
  if (!interpret_simple_idx_recipe(
          recipe, &fvalue_compressor_name, &fvalue_compressor_recipe,
          &posting_compressor_name, &posting_compressor_recipe, error))
    return nullptr;
  if (!Compressor::check(fvalue_compressor_name, fvalue_compressor_recipe,
                         error))
    return nullptr;
  if (!Compressor::check(posting_compressor_name, posting_compressor_recipe,
                         error))
    return nullptr;

  if (working == nullptr) {
    safe_set(error) = "SimpleIdx requires a working directory (got nullptr)";
    return nullptr;
  }
  std::shared_ptr<SimpleIdx> idx = std::shared_ptr<SimpleIdx>(new SimpleIdx());
  idx->posting_compressor_name_ = posting_compressor_name;
  idx->posting_compressor_recipe_ = posting_compressor_recipe;
  idx->posting_compressor_ = Compressor::make(posting_compressor_name,
                                              posting_compressor_recipe, error);
  if (idx->posting_compressor_ == nullptr)
    return nullptr;
  idx->fvalue_compressor_name_ = fvalue_compressor_name;
  idx->fvalue_compressor_recipe_ = fvalue_compressor_recipe;
  idx->fvalue_compressor_ =
      Compressor::make(fvalue_compressor_name, fvalue_compressor_recipe, error);
  if (idx->fvalue_compressor_ == nullptr)
    return nullptr;
  idx->idx_filename_ = working->make_name(IDX_NAME);
  idx->pst_filename_ = working->make_name(PST_NAME);
  {
    std::string new_idx_filename = idx->idx_filename_ + std::string(".NEW");
    std::string new_pst_filename = idx->pst_filename_ + std::string(".NEW");
    bool have_new_idx, have_new_pst;
    {
      std::fstream new_idxf(new_idx_filename, std::ios::binary | std::ios::in);
      have_new_idx = !new_idxf.fail();
    }
    {
      std::fstream new_pstf(new_pst_filename, std::ios::binary | std::ios::in);
      have_new_pst = !new_pstf.fail();
    }
    if (have_new_pst) {
      if (have_new_idx) {
        if (std::rename(new_idx_filename.c_str(), idx->idx_filename_.c_str())) {
          safe_set(error) =
              "SimpleIdx can't rename idx update to: " + idx->idx_filename_;
          return nullptr;
        }
      }
      if (std::rename(new_pst_filename.c_str(), idx->pst_filename_.c_str())) {
        safe_set(error) =
            "SimpleIdx can't rename pst update to: " + idx->pst_filename_;
        return nullptr;
      }
    } else if (have_new_idx) {
      std::remove(new_idx_filename.c_str());
    }
  }
  std::fstream idxf(idx->idx_filename_, std::ios::binary | std::ios::in);
  if (idxf.fail()) {
    safe_set(error) = "SimpleIdx can't access: " + idx->idx_filename_;
    return nullptr;
  }
  idxf.seekg(0, idxf.end);
  addr idxf_end = idxf.tellg();
  idxf.seekg(0, idxf.beg);
  if (idxf_end % sizeof(IdxRecord) != 0) {
    safe_set(error) = "SimpleIdx sizing error in: " + idx->idx_filename_;
    return nullptr;
  }
  size_t pst_map_size = idxf_end / sizeof(IdxRecord);
  idx->pst_map_.reserve(pst_map_size);
  IdxRecord idxr;
  for (idxf.read(reinterpret_cast<char *>(&idxr), sizeof(idxr)); !idxf.fail();
       idxf.read(reinterpret_cast<char *>(&idxr), sizeof(idxr)))
    idx->pst_map_.push_back(idxr);
  if (pst_map_size != idx->pst_map_.size()) {
    safe_set(error) = "SimpleIdx could not load: " + idx->idx_filename_;
    return nullptr;
  }
  idx->pst_ = working->reader(PST_NAME, error);
  if (idx->pst_ == nullptr)
    return nullptr;
  idx->cannot_create_temp_files_ = std::make_shared<bool>(new bool());
  *idx->cannot_create_temp_files_ = false;
  idx->posting_factory_ = SimplePostingFactory::make(idx->posting_compressor_,
                                                     idx->fvalue_compressor_);
  assert(idx->posting_factory_ != nullptr);
  idx->added_files_.push_back(idx->pst_filename_);
  idx->added_ = std::make_unique<std::vector<Annotation>>();
  assert(idx->added_ != nullptr);
  return idx;
}

bool SimpleIdx::check(const std::string &recipe, std::string *error) {
  if (recipe == "")
    return true;
  std::string fvalue_compressor_name, fvalue_compressor_recipe;
  std::string posting_compressor_name, posting_compressor_recipe;
  if (!interpret_simple_idx_recipe(
          recipe, &fvalue_compressor_name, &fvalue_compressor_recipe,
          &posting_compressor_name, &posting_compressor_recipe, error))
    return false;
  if (!Compressor::check(fvalue_compressor_name, fvalue_compressor_recipe,
                         error))
    return false;
  return Compressor::check(posting_compressor_name, posting_compressor_recipe,
                           error);
}

std::string SimpleIdx::recipe_() {
  std::map<std::string, std::string> parameters;
  parameters["posting_compressor"] = posting_compressor_name_;
  parameters["posting_compressor_recipe"] = posting_compressor_recipe_;
  parameters["fvalue_compressor"] = fvalue_compressor_name_;
  parameters["fvalue_compressor_recipe"] = fvalue_compressor_recipe_;
  return freeze(parameters);
}

namespace {
IdxRecord *locate(addr feature, IdxRecord *map, addr n) {
  if (map == nullptr)
    return nullptr;
  if (feature < map[0].feature || feature > map[n - 1].feature)
    return nullptr;
  IdxRecord *l = map;
  if (l->feature == feature)
    return l;
  IdxRecord *h = map + (n - 1);
  while (h > l) {
    IdxRecord *m = l + (h - l) / 2;
    if (m->feature < feature)
      l = m + 1;
    else if (m->feature > feature)
      h = m - 1;
    else
      return m;
  }
  if (h->feature == feature)
    return h;
  else
    return nullptr;
}
} // namespace

void SimpleIdx::reset_() {
  cache_lock_.lock();
  std::fstream idxf(idx_filename_, std::ios::binary | std::ios::in);
  assert(!idxf.fail());
  idxf.seekg(0, idxf.end);
  addr idxf_end = idxf.tellg();
  idxf.seekg(0, idxf.beg);
  assert(idxf_end % sizeof(IdxRecord) == 0);
  size_t pst_map_size = idxf_end / sizeof(IdxRecord);
  pst_map_.clear();
  pst_map_.reserve(pst_map_size);
  IdxRecord idxr;
  for (idxf.read(reinterpret_cast<char *>(&idxr), sizeof(idxr)); !idxf.fail();
       idxf.read(reinterpret_cast<char *>(&idxr), sizeof(idxr)))
    pst_map_.push_back(idxr);
  assert(pst_map_size == pst_map_.size());
  pst_ = working_->reader(PST_NAME);
  assert(pst_ != nullptr);
  stamp_ = 0;
  ages_.clear();
  cache_.clear();
  counts_.clear();
  large_total_ = 0;
  cache_lock_.unlock();
}

SimpleIdx::CacheRecord SimpleIdx::load_cache(addr feature) {
  CacheRecord c;
  cache_lock_.lock();
  std::map<addr, CacheRecord>::iterator cached;
  if ((cached = cache_.find(feature)) != cache_.end()) {
    c = cached->second;
    if (c.n > large_threshold_)
      ages_[feature] = stamp_++;
    cache_lock_.unlock();
    return c;
  }
  IdxRecord *map = pst_map_.data();
  IdxRecord *irp = locate(feature, map, pst_map_.size());
  if (irp == nullptr) {
    c.n = 0;
    cache_lock_.unlock();
    return c;
  }
  addr where, amount;
  if (irp == map)
    where = 0;
  else
    where = (irp - 1)->end;
  amount = irp->end - where;
  std::unique_ptr<char[]> storage = std::unique_ptr<char[]>(new char[amount]);
  char *buffer = storage.get();
  pst_->read(buffer, where, amount);
  PstRecord *pstp = reinterpret_cast<PstRecord *>(buffer);
  c.n = pstp->n;
  buffer += sizeof(PstRecord);
  c.postings = cottontail::shared_array<cottontail::addr>(c.n);
  posting_compressor_->tang(buffer, pstp->pst,
                            reinterpret_cast<char *>(c.postings.get()),
                            c.n * sizeof(addr));
  buffer += pstp->pst;
  if (pstp->qst == 0) {
    c.qostings = c.postings;
  } else {
    c.qostings = cottontail::shared_array<cottontail::addr>(c.n);
    posting_compressor_->tang(buffer, pstp->qst,
                              reinterpret_cast<char *>(c.qostings.get()),
                              c.n * sizeof(addr));
    buffer += pstp->qst;
  }
  if (pstp->fst == 0) {
    c.fostings = nullptr;
  } else {
    c.fostings = cottontail::shared_array<cottontail::fval>(c.n);
    fvalue_compressor_->tang(buffer, pstp->fst,
                             reinterpret_cast<char *>(c.fostings.get()),
                             c.n * sizeof(fval));
  }
  if (c.n > large_threshold_) {
    if (large_total_ > large_limit_) {
      std::vector<addr> old;
      for (auto &a : ages_)
        old.push_back(a.first);
      std::sort(old.begin(), old.end(),
                [&](const auto &a, const auto &b) -> bool {
                  return ages_[a] < ages_[b];
                });
      for (size_t i = 0; large_total_ > large_limit_; i++) {
        counts_[old[i]] = cache_[old[i]].n;
        large_total_ -= cache_[old[i]].n;
        ages_.erase(old[i]);
        cache_.erase(old[i]);
      }
    }
    large_total_ += c.n;
    ages_[feature] = stamp_++;
  }
  cache_[feature] = c;
  cache_lock_.unlock();
  return c;
}

std::unique_ptr<Hopper> SimpleIdx::hopper_(addr feature) {
  CacheRecord c = load_cache(feature);
  if (c.n == 0) {
    return std::make_unique<EmptyHopper>();
  } else if (c.n == 1) {
    if (c.fostings == nullptr) {
      return std::make_unique<SingletonHopper>(*c.postings, *c.qostings, 0.0);
    } else {
      return std::make_unique<SingletonHopper>(*c.postings, *c.qostings,
                                               *c.fostings);
    }
  } else {
    return ArrayHopper::make(c.n, c.postings, c.qostings, c.fostings);
  }
}

addr SimpleIdx::count_(addr feature) {
  cache_lock_.lock();
  std::map<addr, addr>::iterator counted;
  if ((counted = counts_.find(feature)) != counts_.end()) {
    addr count = counted->second;
    cache_lock_.unlock();
    return count;
  }
  std::map<addr, CacheRecord>::iterator cached;
  if ((cached = cache_.find(feature)) != cache_.end()) {
    CacheRecord c = cached->second;
    cache_lock_.unlock();
    return c.n;
  }
  IdxRecord *map = pst_map_.data();
  IdxRecord *irp = locate(feature, map, pst_map_.size());
  if (irp == nullptr) {
    cache_lock_.unlock();
    return 0;
  }
  addr where;
  if (irp == map)
    where = 0;
  else
    where = (irp - 1)->end;
  PstRecord pstp;
  pst_->read(reinterpret_cast<char *>(&pstp), where, sizeof(PstRecord));
  counts_[feature] = pstp.n;
  cache_lock_.unlock();
  return pstp.n;
}

addr SimpleIdx::vocab_() {
  cache_lock_.lock();
  addr vocabulary_size = pst_map_.size();
  cache_lock_.unlock();
  return vocabulary_size;
}

std::map<fval, addr> SimpleIdx::feature_histogram() {
  std::map<fval, addr> histogram;
  for (auto &ir : pst_map_) {
    CacheRecord c = load_cache(ir.feature);
    if (c.fostings != nullptr) {
      fval *start = c.fostings.get();
      fval *end = start + c.n;
      for (fval *v = start; v < end; v++) {
        fval value = *v;
        if (histogram.find(value) == histogram.end())
          histogram[value] = 1;
        else
          histogram[value]++;
      }
    }
  }
  return histogram;
}
} // namespace cottontail
