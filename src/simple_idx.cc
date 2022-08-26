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
    if (item != parameters.end())
      *fvalue_compressor_name = item->second;
    else
      *fvalue_compressor_name = FVALUE_COMPRESSOR_NAME;
    item = parameters.find("fvalue_compressor_recipe");
    if (item != parameters.end())
      *fvalue_compressor_recipe = item->second;
    else
      *fvalue_compressor_recipe = FVALUE_COMPRESSOR_RECIPE;
    item = parameters.find("posting_compressor");
    if (item != parameters.end())
      *posting_compressor_name = item->second;
    else
      *posting_compressor_name = POSTING_COMPRESSOR_NAME;
    item = parameters.find("posting_compressor_recipe");
    if (item != parameters.end())
      *posting_compressor_recipe = item->second;
    else
      *posting_compressor_recipe = POSTING_COMPRESSOR_RECIPE;
    if (add_file_size != nullptr) {
      item = parameters.find("add_file_size");
      if (item != parameters.end()) {
        try {
          *add_file_size = std::stoi(item->second);
        } catch (const std::invalid_argument &e) {
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

void SimpleIdx::reset() {
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

namespace {
void merged_posting(std::shared_ptr<SimplePosting> posting0,
                    std::shared_ptr<SimplePosting> posting1,
                    std::shared_ptr<SimplePosting> merged) {
  addr p0, q0, p1, q1;
  fval v0, v1;
  size_t i0 = 0, i1 = 0;
  bool okay0 = posting0->get(i0, &p0, &q0, &v0);
  i0++;
  bool okay1 = posting1->get(i1, &p1, &q1, &v1);
  i1++;
  while (okay0 && okay1)
    if (p0 < p1) {
      if (q0 < q1)
        merged->push(p0, q0, v0);
      okay0 = posting0->get(i0, &p0, &q0, &v0);
      i0++;
    } else if (p0 > p1) {
      if (q0 > q1)
        merged->push(p1, q1, v1);
      okay1 = posting1->get(i1, &p1, &q1, &v1);
      i1++;
    } else {
      if (q0 < q1) {
        merged->push(p0, q0, v0);
      } else if (q0 > q1) {
        merged->push(p1, q1, v1);
      } else {
        // could just pick one arbitrarily, but there's probably something
        // wrong
        assert(v0 == v1);
        merged->push(p0, q0, v0);
      }
      okay0 = posting0->get(i0, &p0, &q0, &v0);
      i0++;
      okay1 = posting1->get(i1, &p1, &q1, &v1);
      i1++;
    }
  while (okay0) {
    merged->push(p0, q0, v0);
    okay0 = posting0->get(i0, &p0, &q0, &v0);
    i0++;
  }
  while (okay1) {
    merged->push(p1, q1, v1);
    okay1 = posting1->get(i1, &p1, &q1, &v1);
    i1++;
  }
}
} // namespace

bool SimpleIdx::add_annotations_locked(const std::string &annotations_filename,
                                       std::string *error) {
  std::string sorted_annotations_filename;
  if (!sort_annotations(working_, annotations_filename,
                        &sorted_annotations_filename, error))
    return false;
  if (!cottontail::check_annotations(sorted_annotations_filename, error))
    return false;
  std::string new_postings_filename = working_->make_temp();
  SimplePostingFactory posting_factory(posting_compressor_, fvalue_compressor_);
  {
    std::fstream annf(sorted_annotations_filename,
                      std::ios::binary | std::ios::in);
    if (annf.fail()) {
      safe_set(error) = "SimpleIdx can't access annotations in: " +
                        sorted_annotations_filename;
      return false;
    }
    std::fstream pstf(new_postings_filename, std::ios::binary | std::ios::out);
    if (pstf.fail()) {
      safe_set(error) =
          "SimpleIdx can't create postings in: " + new_postings_filename;
      return false;
    }
    Annotation current;
    addr feature = 0;
    std::vector<Annotation> group;
    for (annf.read(reinterpret_cast<char *>(&current), sizeof(Annotation));
         !annf.fail();
         annf.read(reinterpret_cast<char *>(&current), sizeof(Annotation))) {
      if (group.size() > 0 && current.feature != feature) {
        std::vector<Annotation>::iterator start = group.begin();
        std::shared_ptr<SimplePosting> posting =
            posting_factory.posting_from_annotations(&start, group.end());
        posting->write(&pstf);
        group.clear();
      }
      group.push_back(current);
      feature = current.feature;
    }
    if (group.size() > 0) {
      std::vector<Annotation>::iterator start = group.begin();
      std::shared_ptr<SimplePosting> posting =
          posting_factory.posting_from_annotations(&start, group.end());
      posting->write(&pstf);
    }
    std::remove(sorted_annotations_filename.c_str());
  }
  std::string combined_postings_filename = working_->make_temp();
  std::string temp_idx_filename = working_->make_temp();
  std::fstream idxf(temp_idx_filename, std::ios::binary | std::ios::out);
  if (idxf.fail()) {
    safe_set(error) =
        "SimpleIdx can't create temporary file: " + temp_idx_filename;
    return false;
  }
  {
    std::fstream inf0(new_postings_filename, std::ios::binary | std::ios::in);
    if (inf0.fail()) {
      safe_set(error) = "SimpleIdx can't access: " + new_postings_filename;
      return false;
    }
    std::fstream inf1(pst_filename_, std::ios::binary | std::ios::in);
    if (inf1.fail()) {
      safe_set(error) = "SimpleIdx can't access: " + pst_filename_;
      return false;
    }
    std::fstream outf(combined_postings_filename,
                      std::ios::binary | std::ios::out);
    if (outf.fail()) {
      safe_set(error) =
          "SimpleIdx can't create postings in: " + combined_postings_filename;
      return false;
    }
    std::shared_ptr<SimplePosting> posting0 =
        posting_factory.posting_from_file(&inf0);
    std::shared_ptr<SimplePosting> posting1 =
        posting_factory.posting_from_file(&inf1);
    while (posting0 != nullptr && posting1 != nullptr)
      if (posting0->feature() < posting1->feature()) {
        posting0->write(&outf);
        IdxRecord idxr(posting0->feature(), outf.tellp());
        idxf.write(reinterpret_cast<char *>(&idxr), sizeof(idxr));
        if (idxf.fail()) {
          safe_set(error) = "SimpleIdx write failure";
          return false;
        }
        posting0 = posting_factory.posting_from_file(&inf0);
      } else if (posting0->feature() > posting1->feature()) {
        posting1->write(&outf);
        IdxRecord idxr(posting1->feature(), outf.tellp());
        idxf.write(reinterpret_cast<char *>(&idxr), sizeof(idxr));
        if (idxf.fail()) {
          safe_set(error) = "SimpleIdx write failure";
          return false;
        }
        posting1 = posting_factory.posting_from_file(&inf1);
      } else {
        std::shared_ptr<SimplePosting> merged =
            posting_factory.posting_from_feature(posting0->feature());
        merged_posting(posting0, posting1, merged);
        merged->write(&outf);
        IdxRecord idxr(posting0->feature(), outf.tellp());
        idxf.write(reinterpret_cast<char *>(&idxr), sizeof(idxr));
        if (idxf.fail()) {
          safe_set(error) = "SimpleIdx write failure";
          return false;
        }
        posting0 = posting_factory.posting_from_file(&inf0);
        posting1 = posting_factory.posting_from_file(&inf1);
      }
    while (posting0 != nullptr) {
      posting0->write(&outf);
      IdxRecord idxr(posting0->feature(), outf.tellp());
      idxf.write(reinterpret_cast<char *>(&idxr), sizeof(idxr));
      if (idxf.fail()) {
        safe_set(error) = "SimpleIdx write failure";
        return false;
      }
      posting0 = posting_factory.posting_from_file(&inf0);
    }
    while (posting1 != nullptr) {
      posting1->write(&outf);
      IdxRecord idxr(posting1->feature(), outf.tellp());
      idxf.write(reinterpret_cast<char *>(&idxr), sizeof(idxr));
      if (idxf.fail()) {
        safe_set(error) = "SimpleIdx write failure";
        return false;
      }
      posting1 = posting_factory.posting_from_file(&inf1);
    }
  }
  std::remove(new_postings_filename.c_str());
  std::string idx_filename = working_->make_name(IDX_NAME);
  if (std::rename(temp_idx_filename.c_str(), idx_filename.c_str())) {
    safe_set(error) = "SimpleIdx can't rename temporary to: " + idx_filename;
    return false;
  }
  std::string pst_filename = working_->make_name(PST_NAME);
  if (std::rename(combined_postings_filename.c_str(), pst_filename.c_str())) {
    safe_set(error) = "SimpleIdx can't rename temporary to: " + pst_filename;
    return false;
  }
  return true;
}

bool SimpleIdx::add_annotations_(const std::string &annotations_filename,
                                 std::string *error) {
  update_lock_.lock();
  bool outcome = add_annotations_locked(annotations_filename, error);
  reset();
  update_lock_.unlock();
  return outcome;
}

namespace {
bool merge_updates(const std::vector<std::string> &posting_filenames,
                   const std::string &idx_filename,
                   const std::string &pst_filename,
                   std::shared_ptr<SimplePostingFactory> posting_factory,
                   std::string *error = nullptr) {
  std::fstream pst(pst_filename, std::ios::binary | std::ios::out);
  if (pst.fail()) {
    safe_set(error) = "SimpleIdx can't create: " + pst_filename;
    return false;
  }
  std::fstream idx(idx_filename, std::ios::binary | std::ios::out);
  if (idx.fail()) {
    safe_set(error) = "SimpleIdx can't create: " + idx_filename;
    return false;
  }
  std::vector<std::fstream> posting_fstreams;
  std::vector<std::shared_ptr<SimplePosting>> posting_nexts;
  for (auto &filename : posting_filenames) {
    posting_fstreams.emplace_back(filename, std::ios::binary | std::ios::in);
    if (posting_fstreams.back().fail()) {
      safe_set(error) = "SimpleIdx can't access posting file: " + filename;
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
            "SimpleIdx can't store a posting list that isn't a GC list";
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
      safe_set(error) = "SimpleIdx can't write to: " + idx_filename;
      return false;
    }
  }
  return true;
}

bool annotation_cmp(const Annotation &a, const Annotation &b) {
  return a.feature < b.feature || (a.feature == b.feature && a.p < b.p);
}
} // namespace

void SimpleIdx::maybe_flush_additions(bool force) {
  if (*cannot_create_temp_files_)
    return;
  if ((force && added_->size() > 0) || added_->size() > ADD_FILE_SIZE) {
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

bool SimpleIdx::add_annotation_(addr feature, addr p, addr q, fval v,
                                std::string *error) {
  bool outcome = true;
  if (p >= 0 && p <= q) {
    update_lock_.lock();
    if (*cannot_create_temp_files_) {
      safe_set(error) =
          "SimpleIdx cannot create temporary files (check permissions)";
      outcome = false;
    } else {
      added_->emplace_back(feature, p, q, v);
      maybe_flush_additions(false);
    }
    update_lock_.unlock();
  }
  return outcome;
}

bool SimpleIdx::finalize_(std::string *error) {
  update_lock_.lock();
  maybe_flush_additions(true);
  for (auto &worker : workers_)
    worker.join();
  workers_.clear();
  if (*cannot_create_temp_files_) {
    safe_set(error) = "SimpleIdx cannot create temporary files in burrow";
    update_lock_.unlock();
    return false;
  }
  std::string tmp_idx_filename = working_->make_temp();
  std::string tmp_pst_filename = working_->make_temp();
  if (!merge_updates(added_files_, tmp_idx_filename, tmp_pst_filename,
                     posting_factory_, error)) {
    update_lock_.unlock();
    return false;
  }
  for (auto &filename : added_files_)
    if (filename != pst_filename_)
      std::remove(filename.c_str());
  added_files_.clear();
  added_files_.push_back(pst_filename_);
  std::string new_pst_filename = pst_filename_ + std::string(".NEW");
  std::string new_idx_filename = idx_filename_ + std::string(".NEW");
  if (std::rename(tmp_idx_filename.c_str(), new_idx_filename.c_str())) {
    safe_set(error) =
        "SimpleIdx can't rename temporary to: " + new_idx_filename;
    update_lock_.unlock();
    return false;
  }
  if (std::rename(tmp_pst_filename.c_str(), new_pst_filename.c_str())) {
    safe_set(error) =
        "SimpleIdx can't rename temporary to: " + new_pst_filename;
    update_lock_.unlock();
    return false;
  }
  if (std::rename(new_idx_filename.c_str(), idx_filename_.c_str())) {
    safe_set(error) = "SimpleIdx can't rename idx update to: " + idx_filename_;
    update_lock_.unlock();
    return false;
  }
  if (std::rename(new_pst_filename.c_str(), pst_filename_.c_str())) {
    safe_set(error) = "SimpleIdx can't rename pst update to: " + pst_filename_;
    update_lock_.unlock();
    return false;
  }
  reset();
  update_lock_.unlock();
  return true;
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
