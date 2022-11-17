#include "src/simple_posting.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "src/array_hopper.h"
#include "src/compressor.h"
#include "src/core.h"
#include "src/simple.h"

namespace cottontail {

std::shared_ptr<SimplePosting> SimplePostingFactory::posting_from_tokens(
    std::vector<TokRecord>::iterator *start,
    const std::vector<TokRecord>::iterator &end) {
  if ((*start) >= end)
    return nullptr;
  std::shared_ptr<SimplePosting> posting = std::shared_ptr<SimplePosting>(
      new SimplePosting(posting_compressor_, fvalue_compressor_));
  posting->feature_ = (*start)->feature;
  std::vector<TokRecord>::iterator stop;
  for (stop = (*start); stop < end && stop->feature == posting->feature_;
       stop++) {
    assert(posting->postings_.size() == 0 ||
           stop->pq >= posting->postings_.back());
    if (posting->postings_.size() == 0 || stop->pq > posting->postings_.back())
      posting->postings_.push_back(stop->pq);
  }
  (*start) = stop;
  return posting;
}

std::shared_ptr<SimplePosting> SimplePostingFactory::posting_from_annotations(
    std::vector<Annotation>::iterator *start,
    const std::vector<Annotation>::iterator &end) {
  if ((*start) >= end)
    return nullptr;
  std::shared_ptr<SimplePosting> posting = std::shared_ptr<SimplePosting>(
      new SimplePosting(posting_compressor_, fvalue_compressor_));
  posting->feature_ = (*start)->feature;
  std::vector<Annotation>::iterator stop;
  bool clear_qostings = true;
  bool clear_fostings = true;
  for (stop = (*start); stop < end && stop->feature == posting->feature_;
       stop++)
    if (stop->p <= stop->q) {
      if (stop->p < stop->q)
        clear_qostings = false;
      if (stop->v != 0.0)
        clear_fostings = false;
      if (posting->postings_.size() == 0 ||
          (stop->p >= posting->postings_.back() &&
           stop->q >= posting->qostings_.back())) {
        posting->postings_.push_back(stop->p);
        posting->qostings_.push_back(stop->q);
        posting->fostings_.push_back(stop->v);
      }
    }
  if (clear_qostings)
    posting->qostings_.clear();
  if (clear_fostings)
    posting->fostings_.clear();
  (*start) = stop;
  return posting;
}

std::shared_ptr<SimplePosting>
SimplePostingFactory::posting_from_feature(addr feature) {
  std::shared_ptr<SimplePosting> posting = std::shared_ptr<SimplePosting>(
      new SimplePosting(posting_compressor_, fvalue_compressor_));
  posting->feature_ = feature;
  return posting;
}

std::shared_ptr<SimplePosting>
SimplePostingFactory::posting_from_file(std::fstream *f) {
  if (f->fail())
    return nullptr;
  PstRecord idx;
  f->read(reinterpret_cast<char *>(&idx), sizeof(PstRecord));
  if (f->fail())
    return nullptr;
  std::shared_ptr<SimplePosting> posting = std::shared_ptr<SimplePosting>(
      new SimplePosting(posting_compressor_, fvalue_compressor_));
  posting->feature_ = idx.feature;
  if (idx.n == 0)
    return posting;
  addr crushed_size = std::max(std::max(idx.pst, idx.qst), idx.fst) + 1;
  std::unique_ptr<char[]> crushed =
      std::unique_ptr<char[]>(new char[crushed_size]);
  addr tanged_size = std::max(idx.n * sizeof(addr), idx.n * sizeof(fval));
  std::unique_ptr<char[]> tanged =
      std::unique_ptr<char[]>(new char[tanged_size]);
  f->read(crushed.get(), idx.pst);
  assert(!f->fail());
  addr posting_size = posting_compressor_->tang(crushed.get(), idx.pst,
                                                tanged.get(), tanged_size);
  assert(posting_size % sizeof(addr) == 0 &&
         posting_size / (addr)sizeof(addr) == idx.n);
  addr *start = reinterpret_cast<addr *>(tanged.get());
  addr *end = start + idx.n;
  for (addr *p = start; p < end; p++)
    posting->postings_.push_back(*p);
  if (idx.qst > 0) {
    f->read(crushed.get(), idx.qst);
    assert(!f->fail());
    posting_size = posting_compressor_->tang(crushed.get(), idx.qst,
                                             tanged.get(), tanged_size);
    assert(posting_size % sizeof(addr) == 0 &&
           posting_size / (addr)sizeof(addr) == idx.n);
    for (addr *q = start; q < end; q++)
      posting->qostings_.push_back(*q);
  }
  if (idx.fst > 0) {
    f->read(crushed.get(), idx.fst);
    assert(!f->fail());
    posting_size = fvalue_compressor_->tang(crushed.get(), idx.fst,
                                            tanged.get(), tanged_size);
    assert(posting_size % sizeof(addr) == 0 &&
           posting_size / (addr)sizeof(addr) == idx.n);
    fval *start = reinterpret_cast<fval *>(tanged.get());
    fval *end = start + idx.n;
    for (fval *f = start; f < end; f++)
      posting->fostings_.push_back(*f);
  }
  return posting;
}

std::shared_ptr<SimplePosting> SimplePostingFactory::posting_from_merge(
    const std::vector<std::shared_ptr<SimplePosting>> &postings) {
  if (postings.size() == 0)
    return nullptr;
  addr feature = postings[0]->feature();
  bool can_append = true;
  for (size_t i = 1; can_append && i < postings.size(); i++) {
    assert(postings[i]->feature() == feature);
    size_t size0 = postings[i - 1]->size();
    size_t size1 = postings[i]->size();
    if (size0 > 0 && size1 > 0) {
      addr p0, p1, q0, q1;
      fval v0, v1;
      postings[i - 1]->get(size0 - 1, &p0, &q0, &v0);
      postings[i]->get(0, &p1, &q1, &v1);
      can_append = (p0 < p1 && q0 < q1);
    }
  }
  std::shared_ptr<SimplePosting> merged_posting = posting_from_feature(feature);
  if (can_append) {
    for (auto posting : postings)
      merged_posting->append(posting);
  } else {
    std::vector<size_t> index(postings.size(), 0);
    for (;;) {
      addr p0, q0, p = maxfinity, q = maxfinity, minq = maxfinity;
      fval v0, v = 0.0;
      for (size_t i = 0; i < postings.size(); i++) {
        addr p0, q0;
        fval v0;
        if (postings[i]->get(index[i], &p0, &q0, &v0)) {
          if (q0 < minq)
            minq = q0;
          if (p0 < p || (p0 == p && q0 <= q)) {
            p = p0;
            q = q0;
            v = v0;
          }
        }
      }
      if (p == maxfinity)
        break;
      if (q == minq)
        merged_posting->push(p, q, v);
      for (size_t i = 0; i < postings.size(); i++)
        if (postings[i]->get(index[i], &p0, &q0, &v0) && p == p0)
          index[i]++;
    }
  }
  return merged_posting;
}

void SimplePosting::append(std::shared_ptr<SimplePosting> more) {
  assert(feature_ == more->feature_);
  if (qostings_.size() > 0) {
    if (more->qostings_.size() > 0) {
      qostings_.reserve(qostings_.size() + more->qostings_.size());
      qostings_.insert(qostings_.end(), more->qostings_.begin(),
                       more->qostings_.end());
    } else {
      qostings_.reserve(qostings_.size() + more->postings_.size());
      qostings_.insert(qostings_.end(), more->postings_.begin(),
                       more->postings_.end());
    }
  } else if (more->qostings_.size() > 0) {
    qostings_ = postings_;
    qostings_.reserve(qostings_.size() + more->qostings_.size());
    qostings_.insert(qostings_.end(), more->qostings_.begin(),
                     more->qostings_.end());
  }
  if (fostings_.size() > 0) {
    if (more->fostings_.size() > 0) {
      fostings_.reserve(fostings_.size() + more->fostings_.size());
      fostings_.insert(fostings_.end(), more->fostings_.begin(),
                       more->fostings_.end());
    } else {
      fostings_.resize(postings_.size() + more->postings_.size(), 0.0);
    }
  } else if (more->fostings_.size() > 0) {
    fostings_.resize(postings_.size(), 0.0);
    fostings_.reserve(fostings_.size() + more->fostings_.size());
    fostings_.insert(fostings_.end(), more->fostings_.begin(),
                     more->fostings_.end());
  }
  postings_.reserve(postings_.size() + more->postings_.size());
  postings_.insert(postings_.end(), more->postings_.begin(),
                   more->postings_.end());
}

bool SimplePosting::get(size_t index, addr *p, addr *q, fval *v) {
  if (index < postings_.size()) {
    *p = postings_[index];
    if (qostings_.size() > 0)
      *q = qostings_[index];
    else
      *q = *p;
    if (fostings_.size() > 0)
      *v = fostings_[index];
    else
      *v = 0.0;
    return true;
  } else {
    return false;
  }
}

void SimplePosting::push(addr p, addr q, fval v) {
  if (p < q) {
    if (qostings_.size() == 0)
      qostings_ = postings_;
    qostings_.push_back(q);
  } else if (qostings_.size() > 0) {
    qostings_.push_back(p);
  }
  if (v != 0.0) {
    if (fostings_.size() == 0)
      fostings_.resize(postings_.size(), 0.0);
    fostings_.push_back(v);
  } else if (fostings_.size() > 0) {
    fostings_.push_back(0.0);
  }
  postings_.push_back(p);
}

void SimplePosting::write(std::fstream *f) {
  assert(!f->fail());
  addr n = postings_.size();
  if (n == 0) {
    PstRecord idx(feature_, 0, 0, 0, 0);
    f->write(reinterpret_cast<char *>(&idx), sizeof(PstRecord));
    assert(!f->fail());
    return;
  }
  addr postings_size = sizeof(addr) * n;
  addr postings_buffer_size =
      postings_size + posting_compressor_->extra(postings_size);
  std::unique_ptr<char[]> postings_buffer =
      std::unique_ptr<char[]>(new char[postings_buffer_size + 1]);
  addr postings_compressed = posting_compressor_->crush(
      reinterpret_cast<char *>(&postings_[0]), postings_size,
      postings_buffer.get(), postings_buffer_size);
  addr qostings_compressed = 0;
  std::unique_ptr<char[]> qostings_buffer = nullptr;
  if (qostings_.size() != 0) {
    assert((addr)qostings_.size() == n);
    qostings_buffer =
        std::unique_ptr<char[]>(new char[postings_buffer_size + 1]);
    qostings_compressed = posting_compressor_->crush(
        reinterpret_cast<char *>(&qostings_[0]), postings_size,
        qostings_buffer.get(), postings_buffer_size);
  }
  addr fostings_compressed = 0;
  std::unique_ptr<char[]> fostings_buffer = nullptr;
  if (fostings_.size() != 0) {
    assert((addr)fostings_.size() == n);
    addr fostings_size = sizeof(fval) * n;
    addr fostings_buffer_size =
        fostings_size + fvalue_compressor_->extra(fostings_size);
    fostings_buffer =
        std::unique_ptr<char[]>(new char[fostings_buffer_size + 1]);
    fostings_compressed = fvalue_compressor_->crush(
        reinterpret_cast<char *>(&fostings_[0]), fostings_size,
        fostings_buffer.get(), fostings_buffer_size);
  }
  PstRecord idx(feature_, n, postings_compressed, qostings_compressed,
                fostings_compressed);
  f->write(reinterpret_cast<char *>(&idx), sizeof(PstRecord));
  assert(!f->fail());
  f->write(postings_buffer.get(), postings_compressed);
  assert(!f->fail());
  if (qostings_compressed > 0) {
    f->write(qostings_buffer.get(), qostings_compressed);
    assert(!f->fail());
  }
  if (fostings_compressed > 0) {
    f->write(fostings_buffer.get(), fostings_compressed);
    assert(!f->fail());
  }
}

bool SimplePosting::invariants(std::string *error) {
  if (qostings_.size() != 0 && qostings_.size() != postings_.size()) {
    safe_set(error) = "Postings and qostings have inconsistent sizes";
    return false;
  }
  if (fostings_.size() != 0 && fostings_.size() != fostings_.size()) {
    safe_set(error) = "Postings and fostings have inconsistent sizes";
    return false;
  }
  if (postings_.size() > 0 &&
      !std::is_sorted(postings_.begin(), postings_.end())) {
    safe_set(error) = "Postings not sorted";
    return false;
  }
  if (qostings_.size() > 0 &&
      !std::is_sorted(qostings_.begin(), qostings_.end())) {
    safe_set(error) = "Qostings not sorted";
    return false;
  }
  if (postings_.size() > 0 && qostings_.size() > 0) {
    for (size_t i = 0; i < postings_.size(); i++)
      if (postings_[i] > qostings_[i]) {
        safe_set(error) = "Posting list not gc list";
        return false;
      }
  }
  return true;
}

bool SimplePosting::operator==(const SimplePosting &other) {
  return feature_ == other.feature_ && postings_ == other.postings_ &&
         qostings_ == other.qostings_ && fostings_ == other.fostings_ &&
         posting_compressor_ == other.posting_compressor_ &&
         fvalue_compressor_ == other.fvalue_compressor_;
}

namespace {
template <typename T> std::shared_ptr<T> v2sa(const std::vector<T> &v) {
  std::shared_ptr<T> sa = shared_array<T>(v.size());
  memcpy(sa.get(), v.data(), v.size() * sizeof(T));
  return sa;
}
} // namespace

std::unique_ptr<Hopper> SimplePosting::hopper() {
  addr n = postings_.size();
  if (n == 0) {
    return std::make_unique<EmptyHopper>();
  } else if (n == 1) {
    if (qostings_.size() == 0) {
      if (fostings_.size() == 0) {
        return std::make_unique<SingletonHopper>(postings_[0], postings_[0],
                                                 0.0);
      } else {
        return std::make_unique<SingletonHopper>(postings_[0], postings_[0],
                                                 fostings_[0]);
      }
    } else {
      if (fostings_.size() == 0) {
        return std::make_unique<SingletonHopper>(postings_[0], qostings_[0],
                                                 0.0);
      } else {
        return std::make_unique<SingletonHopper>(postings_[0], qostings_[0],
                                                 fostings_[0]);
      }
    }
  } else {
    std::shared_ptr<addr> postings;
    std::shared_ptr<addr> qostings;
    std::shared_ptr<fval> fostings;
    postings = v2sa<addr>(postings_);
    if (qostings_.size() == 0)
      qostings = postings;
    else
      qostings = v2sa<addr>(qostings_);
    if (fostings_.size() == 0)
      fostings = nullptr;
    else
      fostings = v2sa<fval>(fostings_);
    return ArrayHopper::make(n, postings, qostings, fostings);
  }
}

} // namespace cottontail
