#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "gtest/gtest.h"

#include "src/bigwig.h"
#include "src/cottontail.h"
#include "src/fiver.h"
#include "src/hazel.h"
#include "src/simple_posting.h"

namespace {

using cottontail::addr;
using cottontail::fval;

struct Interval {
  addr p, q;
  fval v;
};

// Deliberately quadratic and independent of the production sort/scan. The
// input is chronological, so the last exact duplicate supplies the value.
std::vector<Interval> select(const std::vector<Interval> &input, bool outer) {
  std::vector<Interval> result;
  for (size_t i = 0; i < input.size(); i++) {
    const auto &a = input[i];
    if (a.p > a.q)
      continue;
    bool keep = true;
    for (size_t j = 0; j < input.size(); j++) {
      const auto &b = input[j];
      if (b.p > b.q)
        continue;
      if (a.p == b.p && a.q == b.q) {
        if (j > i)
          keep = false;
      } else if (outer ? b.p <= a.p && a.q <= b.q : a.p <= b.p && b.q <= a.q) {
        keep = false;
      }
    }
    if (keep)
      result.push_back(a);
  }
  std::sort(result.begin(), result.end(),
            [](const Interval &a, const Interval &b) { return a.p < b.p; });
  return result;
}

std::shared_ptr<cottontail::SimplePostingFactory> factory() {
  return cottontail::SimplePostingFactory::make(
      cottontail::Compressor::make("post", ""),
      cottontail::Compressor::make("zlib", ""));
}

std::shared_ptr<cottontail::SimplePosting>
posting(std::shared_ptr<cottontail::SimplePostingFactory> factory, addr feature,
        const std::vector<Interval> &input) {
  std::vector<cottontail::Annotation> annotations;
  for (const auto &a : input)
    annotations.push_back({feature, a.p, a.q, a.v});
  cottontail::sort_annotations(&annotations);
  auto start = annotations.begin();
  return annotations.empty()
             ? factory->posting_from_feature(feature)
             : factory->posting_from_annotations(&start, annotations.end());
}

void expect_posting(std::shared_ptr<cottontail::SimplePosting> posting,
                    const std::vector<Interval> &expected) {
  if (posting == nullptr) {
    EXPECT_TRUE(expected.empty());
    return;
  }
  ASSERT_EQ(posting->size(), expected.size());
  addr last_p = cottontail::minfinity, last_q = cottontail::minfinity;
  for (size_t i = 0; i < expected.size(); i++) {
    addr p, q;
    fval v;
    ASSERT_TRUE(posting->get(i, &p, &q, &v));
    EXPECT_EQ(p, expected[i].p);
    EXPECT_EQ(q, expected[i].q);
    EXPECT_DOUBLE_EQ(v, expected[i].v);
    EXPECT_GT(p, last_p);
    EXPECT_GT(q, last_q);
    last_p = p;
    last_q = q;
  }
}

std::vector<Interval> subset(unsigned mask, addr limit, fval value) {
  std::vector<Interval> result;
  unsigned bit = 1;
  for (addr p = 0; p < limit; p++)
    for (addr q = p; q < limit; q++, bit <<= 1)
      if (mask & bit)
        result.push_back({p, q, value});
  return result;
}

std::string temporary_directory() {
  const char *tmp = std::getenv("TEST_TMPDIR");
  std::string pattern =
      std::string(tmp == nullptr ? "/tmp" : tmp) + "/erasures-XXXXXX";
  std::vector<char> buffer(pattern.begin(), pattern.end());
  buffer.push_back('\0');
  char *directory = mkdtemp(buffer.data());
  EXPECT_NE(directory, nullptr);
  return directory == nullptr ? "" : directory;
}

struct TextLayout {
  addr raw_length = 0;
  addr payload_length = 0;
  std::vector<cottontail::HazelTextEntry> blocks;
};

TextLayout text_layout(const std::string &filename) {
  TextLayout result;
  std::fstream in(filename, std::ios::in | std::ios::binary);
  in.seekg(cottontail::cottontail_file_magic.size());
  std::string line;
  while (std::getline(in, line) && !line.empty()) {
  }
  in.seekg(cottontail::hazel_blob_dictionary_magic.size(), std::ios::cur);
  addr count;
  if (!cottontail::read_pod(&in, &count)) {
    ADD_FAILURE() << "Cannot read blob dictionary: " << filename;
    return result;
  }
  for (addr i = 0; i < count; i++) {
    addr length, offset, blob_length;
    if (!cottontail::read_pod(&in, &length) || length < 0) {
      ADD_FAILURE() << "Cannot read blob name";
      return result;
    }
    std::string name(length, '\0');
    in.read(name.data(), length);
    if (!cottontail::read_pod(&in, &offset) ||
        !cottontail::read_pod(&in, &blob_length)) {
      ADD_FAILURE() << "Cannot read blob entry";
      return result;
    }
    if (name != "txt")
      continue;
    in.seekg(offset + cottontail::hazel_txt_magic.size());
    addr directory_offset, directory_length, entries, target;
    if (!cottontail::read_pod(&in, &directory_offset) ||
        !cottontail::read_pod(&in, &directory_length) ||
        !cottontail::read_pod(&in, &entries) ||
        !cottontail::read_pod(&in, &result.raw_length) ||
        !cottontail::read_pod(&in, &target)) {
      ADD_FAILURE() << "Cannot read text header";
      return result;
    }
    result.payload_length = directory_offset;
    in.seekg(offset + cottontail::hazel_txt_magic.size() + 5 * sizeof(addr) +
             directory_offset);
    for (addr j = 0; j < entries; j++) {
      cottontail::HazelTextEntry entry;
      if (!cottontail::read_pod(&in, &entry.raw_byte_end) ||
          !cottontail::read_pod(&in, &entry.compressed_byte_end)) {
        ADD_FAILURE() << "Cannot read text block";
        return result;
      }
      result.blocks.push_back(entry);
    }
    return result;
  }
  ADD_FAILURE() << "Missing text blob: " << filename;
  return result;
}

void expect_hopper(std::unique_ptr<cottontail::Hopper> hopper,
                   const std::vector<Interval> &expected) {
  ASSERT_NE(hopper, nullptr);
  // All four seeks, including gaps and exact endpoints, in both directions.
  for (addr k = -1; k <= 17; k++) {
    for (int operation = 0; operation < 4; operation++) {
      Interval want = {cottontail::maxfinity, cottontail::maxfinity, 0.0};
      if (operation >= 2)
        want = {cottontail::minfinity, cottontail::minfinity, 0.0};
      for (const auto &a : expected) {
        if ((operation == 0 && a.p >= k) || (operation == 1 && a.q >= k)) {
          want = a;
          break;
        }
        if ((operation == 2 && a.q <= k) || (operation == 3 && a.p <= k))
          want = a;
      }
      addr p, q;
      fval v;
      if (operation == 0)
        hopper->tau(k, &p, &q, &v);
      else if (operation == 1)
        hopper->rho(k, &p, &q, &v);
      else if (operation == 2)
        hopper->uat(k, &p, &q, &v);
      else
        hopper->ohr(k, &p, &q, &v);
      EXPECT_EQ(p, want.p) << "seek " << operation << " at " << k;
      EXPECT_EQ(q, want.q) << "seek " << operation << " at " << k;
      if (p != cottontail::minfinity && p != cottontail::maxfinity) {
        EXPECT_DOUBLE_EQ(v, want.v);
      }
    }
  }
}

} // namespace

TEST(Erasures, AnnotationSelection) {
  auto f = factory();
  for (bool outer : {false, true}) {
    addr feature = outer ? cottontail::null_feature : 123;
    for (unsigned mask = 0; mask < 1024; mask++) {
      SCOPED_TRACE(std::to_string(outer) + ":" + std::to_string(mask));
      auto input = subset(mask, 4, 1.0);
      auto expected = select(input, outer);
      // Equal raw annotations intentionally have equal values: their order
      // within one batch is unspecified by the unstable canonical sort.
      auto duplicates = input;
      input.insert(input.end(), duplicates.begin(), duplicates.end());
      input.push_back({4, 3, 99.0});
      expect_posting(posting(f, feature, input), expected);
      std::reverse(input.begin(), input.end());
      expect_posting(posting(f, feature, input), expected);
    }
  }
  std::vector<cottontail::Annotation> mixed = {
      {123, 1, 8, 1.0}, {0, 3, 4, 0.0}, {456, 2, 2, 0.0},
      {123, 2, 3, 2.0}, {0, 1, 8, 0.0}, {123, 1, 3, 3.0}};
  cottontail::sort_annotations(&mixed);
  auto start = mixed.begin();
  expect_posting(f->posting_from_annotations(&start, mixed.end()),
                 {{1, 8, 0.0}});
  expect_posting(f->posting_from_annotations(&start, mixed.end()),
                 {{2, 3, 2.0}});
  expect_posting(f->posting_from_annotations(&start, mixed.end()),
                 {{2, 2, 0.0}});
  EXPECT_EQ(start, mixed.end());
}

TEST(Erasures, MergeSelectionAndNewestValues) {
  auto f = factory();
  EXPECT_EQ(f->posting_from_merge({}), nullptr);
  for (bool outer : {false, true}) {
    addr feature = outer ? cottontail::null_feature : 123;
    auto empty = f->posting_from_feature(feature);
    for (unsigned a = 0; a < 64; a++) {
      for (unsigned b = 0; b < 64; b++) {
        SCOPED_TRACE(std::to_string(outer) + ":" + std::to_string(a) + ":" +
                     std::to_string(b));
        auto old = subset(a, 3, 7.0), recent = subset(b, 3, 0.0);
        auto input = old;
        input.insert(input.end(), recent.begin(), recent.end());
        auto expected = select(input, outer);
        auto first = posting(f, feature, old);
        auto last = posting(f, feature, recent);
        // An empty source must not hide equal or nested endpoints from the
        // concatenation fast path. Zero must replace an older nonzero value.
        auto merged = f->posting_from_merge({first, empty, last});
        expect_posting(merged, expected);
        expect_posting(f->posting_from_merge(
                           {f->posting_from_merge({first, empty}), last}),
                       expected);
        expect_posting(f->posting_from_merge(
                           {first, f->posting_from_merge({empty, last})}),
                       expected);
        std::ostringstream bytes;
        merged->write(&bytes);
        auto blob = bytes.str();
        expect_posting(
            f->posting_from_compressed_blob(blob.data(), blob.size()),
            expected);
      }
    }
    // Three populated sources, including updates to the same interval.
    auto one = posting(f, feature, {{10, 100, 1.0}, {20, 200, 2.0}});
    auto two = posting(f, feature, {{20, 21, 3.0}, {30, 300, 4.0}});
    auto three = posting(f, feature, {{10, 100, 0.0}, {30, 300, 5.0}});
    auto expected = select({{10, 100, 1.0},
                            {20, 200, 2.0},
                            {20, 21, 3.0},
                            {30, 300, 4.0},
                            {10, 100, 0.0},
                            {30, 300, 5.0}},
                           outer);
    expect_posting(f->posting_from_merge({one, two, three}), expected);
    expect_posting(
        f->posting_from_merge({f->posting_from_merge({one, two}), three}),
        expected);
    expect_posting(
        f->posting_from_merge({one, f->posting_from_merge({two, three})}),
        expected);
  }
}

TEST(Erasures, ExclusionIsContainmentNotUnion) {
  auto f = factory();
  for (unsigned mask = 0; mask < 1024; mask++) {
    auto erasures = select(subset(mask, 4, 0.0), true);
    auto exclude = posting(f, cottontail::null_feature, erasures);
    // Try each possible interval independently so inner selection does not
    // remove a containing interval before we exercise the exclusion filter.
    for (const auto &a : subset(1023, 4, 0.0)) {
      bool erased = false;
      for (const auto &e : erasures)
        erased = erased || (e.p <= a.p && a.q <= e.q);
      auto old = posting(f, 123, {{a.p, a.q, 7.0}});
      auto recent = posting(f, 123, {a});
      expect_posting(f->posting_from_merge({old, recent}, exclude),
                     erased ? std::vector<Interval>{}
                            : std::vector<Interval>{a});
    }
  }
  auto exclude = posting(f, 0, {{2, 6, 0.0}, {5, 10, 0.0}});
  expect_posting(
      f->posting_from_merge({posting(f, 123, {{2, 10, 1.0}})}, exclude),
      {{2, 10, 1.0}});
}

TEST(Erasures, LiveMergedAndReopened) {
  for (bool reverse : {false, true}) {
    for (const std::string compression : {"null", "zlib"}) {
      SCOPED_TRACE(compression + (reverse ? " reversed" : " forward"));
      std::string error, root = temporary_directory();
      ASSERT_FALSE(root.empty());
      std::string burrow = root + "/live";
      auto working = cottontail::Working::mkdir(burrow, &error);
      ASSERT_NE(working, nullptr) << error;
      auto featurizer = cottontail::Featurizer::make("hashing", "", &error);
      auto tokenizer = cottontail::Tokenizer::make("utf8", "", &error);
      auto pc = cottontail::Compressor::make("post", "", &error);
      auto vc = cottontail::Compressor::make(compression, "", &error);
      auto tc = cottontail::Compressor::make(compression, "", &error);
      auto bigwig = cottontail::Bigwig::make(working, featurizer, tokenizer,
                                             &error, nullptr, pc, vc, tc);
      ASSERT_NE(bigwig, nullptr) << error;
      bigwig->merge(false);
      addr point = featurizer->featurize("point:");
      addr bridge = featurizer->featurize("bridge:");
      addr covered = featurizer->featurize("covered:");
      addr wide = featurizer->featurize("wide:");
      addr inner = featurizer->featurize("inner:");
      ASSERT_TRUE(bigwig->transaction(&error)) << error;
      addr p, q;
      ASSERT_TRUE(bigwig->appender()->append(
          "zero one two three four five six seven eight nine ten eleven "
          "twelve thirteen fourteen fifteen\n",
          &p, &q, &error))
          << error;
      ASSERT_EQ(q - p + 1, 16);
      for (addr i = 0; i < 16; i++)
        ASSERT_TRUE(bigwig->annotator()->annotate(point, p + i, p + i, 1.0));
      ASSERT_TRUE(bigwig->annotator()->annotate(bridge, p + 2, p + 10, 1.0));
      ASSERT_TRUE(bigwig->annotator()->annotate(covered, p + 2, p + 5, 1.0));
      ASSERT_TRUE(bigwig->annotator()->annotate(covered, p + 5, p + 10, 1.0));
      ASSERT_TRUE(bigwig->annotator()->annotate(wide, p, p + 15, 1.0));
      for (auto a : std::vector<Interval>{
               {0, 15, 1.0}, {4, 5, 2.0}, {4, 7, 3.0}, {2, 5, 4.0}})
        ASSERT_TRUE(
            bigwig->annotator()->annotate(inner, p + a.p, p + a.q, a.v));
      ASSERT_TRUE(bigwig->ready(&error)) << error;
      bigwig->commit();
      auto snapshot = bigwig->clone(&error);
      ASSERT_NE(snapshot, nullptr) << error;
      snapshot->start();
      expect_hopper(snapshot->idx()->hopper(inner), {{4, 5, 2.0}});

      std::vector<std::vector<Interval>> deletions = {
          {{3, 4, 0.0}, {2, 6, 0.0}, {2, 5, 0.0}, {4, 6, 0.0}},
          {{5, 10, 0.0}, {3, 4, 0.0}},
          {{2, 6, 0.0}, {15, 15, 0.0}}};
      if (reverse) {
        std::swap(deletions[0], deletions[1]);
        for (auto &batch : deletions)
          std::reverse(batch.begin(), batch.end());
      }
      for (size_t i = 0; i < deletions.size(); i++) {
        ASSERT_TRUE(bigwig->transaction(&error)) << error;
        for (const auto &a : deletions[i])
          ASSERT_TRUE(bigwig->annotator()->erase(a.p, a.q, &error)) << error;
        ASSERT_TRUE(bigwig->annotator()->annotate(point, 12, 12,
                                                  i == 2 ? 0.0 : 7.0 + i));
        ASSERT_TRUE(bigwig->ready(&error)) << error;
        bigwig->commit();
      }
      std::vector<Interval> old_points;
      for (addr i = 0; i < 16; i++)
        old_points.push_back({i, i, 1.0});
      expect_hopper(snapshot->idx()->hopper(point), old_points);
      expect_hopper(snapshot->idx()->hopper(cottontail::null_feature), {});
      snapshot->end();

      auto check = [&](std::shared_ptr<cottontail::Warren> warren) {
        ASSERT_NE(warren, nullptr) << error;
        warren->start();
        expect_hopper(warren->idx()->hopper(cottontail::null_feature),
                      {{2, 6, 0.0}, {5, 10, 0.0}, {15, 15, 0.0}});
        expect_hopper(warren->idx()->hopper(point), {{0, 0, 1.0},
                                                     {1, 1, 1.0},
                                                     {11, 11, 1.0},
                                                     {12, 12, 0.0},
                                                     {13, 13, 1.0},
                                                     {14, 14, 1.0}});
        expect_hopper(warren->idx()->hopper(bridge), {{2, 10, 1.0}});
        expect_hopper(warren->idx()->hopper(wide), {{0, 15, 1.0}});
        expect_hopper(warren->idx()->hopper(covered), {});
        expect_hopper(warren->idx()->hopper(inner), {});
        warren->end();
      };
      check(bigwig);
      check(cottontail::Bigwig::make(burrow, &error));

      auto names = working->ls("fiver");
      std::sort(names.begin(), names.end());
      ASSERT_EQ(names.size(), 4U);
      std::vector<std::shared_ptr<cottontail::Fiver>> fivers;
      std::vector<std::shared_ptr<cottontail::Hazel>> hazels;
      for (size_t i = 0; i < names.size(); i++) {
        auto fiver = cottontail::Fiver::unpickle(names[i], working, featurizer,
                                                 tokenizer, &error, pc, vc, tc);
        ASSERT_NE(fiver, nullptr) << error;
        fiver->start();
        fivers.push_back(fiver);
        std::string filename = root + "/source-" + std::to_string(i) + ".hazel";
        ASSERT_TRUE(fiver->hazel(filename, &error, 64)) << error;
        auto hazel = std::dynamic_pointer_cast<cottontail::Hazel>(
            cottontail::Warren::make(filename, &error));
        ASSERT_NE(hazel, nullptr) << error;
        hazel->start();
        hazels.push_back(hazel);
      }
      auto merged = cottontail::Fiver::merge(fivers, &error, pc, vc, tc);
      ASSERT_NE(merged, nullptr) << error;
      check(merged);
      auto left =
          cottontail::Fiver::merge({fivers[0], fivers[1]}, &error, pc, vc, tc);
      auto right =
          cottontail::Fiver::merge({fivers[2], fivers[3]}, &error, pc, vc, tc);
      ASSERT_NE(left, nullptr) << error;
      ASSERT_NE(right, nullptr) << error;
      check(cottontail::Fiver::merge({left, right}, &error, pc, vc, tc));
      std::string pickle = root + "/merged.fiver";
      ASSERT_TRUE(merged->pickle(pickle, &error)) << error;
      check(cottontail::Fiver::unpickle(pickle, nullptr, featurizer, tokenizer,
                                        &error, pc, vc, tc));
      std::string flat = root + "/flat.hazel";
      check(cottontail::Hazel::merge(hazels, flat, nullptr, &error));
      check(cottontail::Warren::make(flat, &error));
      auto hleft = cottontail::Hazel::merge(
          {hazels[0], hazels[1]}, root + "/left.hazel", nullptr, &error);
      auto hright = cottontail::Hazel::merge(
          {hazels[2], hazels[3]}, root + "/right.hazel", nullptr, &error);
      ASSERT_NE(hleft, nullptr) << error;
      ASSERT_NE(hright, nullptr) << error;
      hleft->start();
      hright->start();
      check(cottontail::Hazel::merge({hleft, hright}, root + "/tree.hazel",
                                     nullptr, &error));
      check(cottontail::Warren::make(root + "/tree.hazel", &error));
      // Reopen the collection with both shard types, then consolidate it.
      ASSERT_NE(fivers[0]->hazel(&error), nullptr) << error;
      ASSERT_NE(fivers[1]->hazel(&error), nullptr) << error;
      check(cottontail::Bigwig::make(burrow, &error));
      ASSERT_TRUE(cottontail::Bigwig::consolidate(burrow, &error)) << error;
      check(cottontail::Bigwig::make(burrow, &error));
    }
  }
}

TEST(Erasures, SimpleBuilderAndAnnotatorUseCanonicalOrder) {
  std::string root = temporary_directory(), error;
  ASSERT_FALSE(root.empty());
  auto working = cottontail::Working::mkdir(root + "/simple", &error);
  ASSERT_NE(working, nullptr) << error;
  auto builder =
      cottontail::SimpleBuilder::make(working, "tokenizer:noxml", &error);
  ASSERT_NE(builder, nullptr) << error;
  ASSERT_TRUE(
      builder->add_text("zero one two three four five six seven\n", &error));
  std::vector<Interval> nested = {
      {0, 7, 1.0}, {3, 4, 2.0}, {3, 6, 3.0}, {2, 4, 4.0}};
  for (const auto &a : nested)
    ASSERT_TRUE(builder->add_annotation("nested:", a.p, a.q, a.v, &error));
  ASSERT_TRUE(builder->finalize(&error)) << error;
  auto warren = cottontail::Warren::make("simple", root + "/simple", &error);
  ASSERT_NE(warren, nullptr) << error;
  warren->start();
  addr feature = warren->featurizer()->featurize("nested:");
  addr other = warren->featurizer()->featurize("other:");
  expect_hopper(warren->idx()->hopper(feature), {{3, 4, 2.0}});
  std::string recipe = warren->idx()->recipe();
  warren->end();
  warren.reset();
  auto annotator =
      cottontail::Annotator::make("simple", recipe, &error, working);
  ASSERT_NE(annotator, nullptr) << error;
  ASSERT_TRUE(annotator->transaction(&error)) << error;
  std::reverse(nested.begin(), nested.end());
  for (const auto &a : nested)
    ASSERT_TRUE(annotator->annotate(other, a.p, a.q, a.v, &error));
  ASSERT_TRUE(annotator->annotate(feature, 3, 4, 0.0, &error));
  ASSERT_TRUE(annotator->ready(&error)) << error;
  annotator->commit();
  auto idx = cottontail::Idx::make("simple", recipe, &error, working);
  ASSERT_NE(idx, nullptr) << error;
  expect_hopper(idx->hopper(other), {{3, 4, 2.0}});
  expect_hopper(idx->hopper(feature), {{3, 4, 0.0}});
}

TEST(Erasures, Translation) {
  for (const std::string compression : {"null", "zlib"}) {
    SCOPED_TRACE(compression);
    std::string error, root = temporary_directory();
    ASSERT_FALSE(root.empty());
    auto working = cottontail::Working::mkdir(root + "/live", &error);
    ASSERT_NE(working, nullptr) << error;
    auto featurizer = cottontail::Featurizer::make("hashing", "", &error);
    auto tokenizer = cottontail::Tokenizer::make("utf8", "", &error);
    auto pc = cottontail::Compressor::make("post", "", &error);
    auto vc = cottontail::Compressor::make(compression, "", &error);
    auto tc = cottontail::Compressor::make(compression, "", &error);
    auto bigwig = cottontail::Bigwig::make(working, featurizer, tokenizer,
                                           &error, nullptr, pc, vc, tc);
    ASSERT_NE(bigwig, nullptr) << error;
    bigwig->merge(false);
    for (const std::string text :
         {"zero one two three four five six seven eight nine ten eleven "
          "twelve thirteen fourteen fifteen\n",
          "sixteen seventeen eighteen nineteen\n"}) {
      ASSERT_TRUE(bigwig->transaction(&error)) << error;
      addr p, q;
      ASSERT_TRUE(bigwig->appender()->append(text, &p, &q, &error)) << error;
      ASSERT_TRUE(bigwig->ready(&error)) << error;
      bigwig->commit();
    }
    bigwig->start();
    auto before = bigwig->clone(&error);
    ASSERT_NE(before, nullptr) << error;
    ASSERT_TRUE(before->started());
    bigwig->end();

    // Capture ordinary translations, including clipping and sentinel bounds,
    // before introducing erasures in a separate, text-free transaction.
    std::vector<std::pair<addr, addr>> ranges;
    for (addr p = -1; p <= 30; p++)
      for (addr q = p; q <= 30; q++)
        ranges.emplace_back(p, q);
    ranges.emplace_back(cottontail::minfinity, 2);
    ranges.emplace_back(cottontail::minfinity, cottontail::maxfinity);
    ranges.emplace_back(16, cottontail::maxfinity);
    ranges.emplace_back(cottontail::maxfinity, cottontail::maxfinity);
    ranges.emplace_back(8, 7);
    ranges.emplace_back(cottontail::minfinity, cottontail::minfinity);
    std::vector<std::string> baseline;
    for (const auto &range : ranges)
      baseline.push_back(before->txt()->translate(range.first, range.second));
    EXPECT_EQ(before->txt()->translate(cottontail::minfinity, 0), "zero ");
    EXPECT_EQ(before->txt()->translate(16, cottontail::maxfinity),
              "sixteen seventeen eighteen nineteen\n");

    std::vector<Interval> erasures = {
        {3, 5, 0.0}, {7, 9, 0.0}, {15, 15, 0.0}, {25, 28, 0.0}};
    ASSERT_TRUE(bigwig->transaction(&error)) << error;
    for (const auto &e : erasures)
      ASSERT_TRUE(bigwig->annotator()->erase(e.p, e.q, &error)) << error;
    ASSERT_TRUE(bigwig->ready(&error)) << error;
    bigwig->commit();
    bigwig->start();
    auto after = bigwig->clone(&error);
    ASSERT_NE(after, nullptr) << error;
    ASSERT_TRUE(after->started());
    bigwig->end();

    auto check = [&](std::shared_ptr<cottontail::Warren> warren) {
      ASSERT_NE(warren, nullptr) << error;
      if (!warren->started())
        warren->start();
      for (size_t i = 0; i < ranges.size(); i++) {
        addr p = ranges[i].first, q = ranges[i].second;
        bool touched = false;
        for (const auto &e : erasures)
          touched = touched || (p <= e.q && e.p <= q);
        EXPECT_EQ(warren->txt()->translate(p, q), touched ? "" : baseline[i])
            << "translate [" << p << "," << q << "]";
      }
      // Repeated seeks in differing orders exercise shared hopper locking.
      std::vector<std::thread> readers;
      for (size_t t = 0; t < 4; t++)
        readers.emplace_back([&, t] {
          for (size_t j = 0; j < ranges.size(); j++) {
            size_t i = t % 2 == 0 ? j : ranges.size() - 1 - j;
            addr p = ranges[i].first, q = ranges[i].second;
            bool touched = false;
            for (const auto &e : erasures)
              touched = touched || (p <= e.q && e.p <= q);
            EXPECT_EQ(warren->txt()->translate(p, q),
                      touched ? "" : baseline[i]);
          }
        });
      for (auto &reader : readers)
        reader.join();
      warren->end();
    };
    check(after);
    check(cottontail::Bigwig::make(root + "/live", &error));
    // The pre-deletion snapshot keeps its original text and empty null hopper.
    for (size_t i = 0; i < ranges.size(); i++)
      EXPECT_EQ(before->txt()->translate(ranges[i].first, ranges[i].second),
                baseline[i]);
    before->end();

    auto names = working->ls("fiver");
    std::sort(names.begin(), names.end());
    ASSERT_EQ(names.size(), 3U);
    std::vector<std::shared_ptr<cottontail::Fiver>> fivers;
    std::vector<std::shared_ptr<cottontail::Hazel>> hazels;
    for (size_t i = 0; i < names.size(); i++) {
      auto fiver = cottontail::Fiver::unpickle(names[i], working, featurizer,
                                               tokenizer, &error, pc, vc, tc);
      ASSERT_NE(fiver, nullptr) << error;
      fiver->start();
      fivers.push_back(fiver);
      std::string filename = root + "/source-" + std::to_string(i) + ".hazel";
      ASSERT_TRUE(fiver->hazel(filename, &error, 32)) << error;
      auto hazel = std::dynamic_pointer_cast<cottontail::Hazel>(
          cottontail::Warren::make(filename, &error));
      ASSERT_NE(hazel, nullptr) << error;
      hazel->start();
      hazels.push_back(hazel);
    }
    // A standalone shard without erasures still clips as before.
    EXPECT_EQ(hazels[1]->txt()->translate(0, cottontail::maxfinity),
              "sixteen seventeen eighteen nineteen\n");
    // Local erasures outside this shard's text must be checked before clipping.
    auto suffix = cottontail::Hazel::merge(
        {hazels[1], hazels[2]}, root + "/suffix.hazel", nullptr, &error);
    ASSERT_NE(suffix, nullptr) << error;
    suffix->start();
    EXPECT_EQ(suffix->txt()->translate(16, 24),
              "sixteen seventeen eighteen nineteen\n");
    EXPECT_EQ(suffix->txt()->translate(0, 19), "");
    EXPECT_EQ(suffix->txt()->translate(16, 30), "");
    suffix->end();

    auto merged = cottontail::Fiver::merge(fivers, &error, pc, vc, tc);
    ASSERT_NE(merged, nullptr) << error;
    merged->start();
    std::string converted = root + "/converted.hazel";
    ASSERT_TRUE(merged->hazel(converted, &error, 32)) << error;
    check(cottontail::Warren::make(converted, &error));
    std::string flat = root + "/flat.hazel";
    check(cottontail::Hazel::merge(hazels, flat, nullptr, &error));
    check(cottontail::Warren::make(flat, &error));
    // Mixed collections must reject the complete request, even when only a
    // different shard carries the erasure and other shards could return text.
    ASSERT_NE(fivers[0]->hazel(&error), nullptr) << error;
    check(cottontail::Bigwig::make(root + "/live", &error));
    ASSERT_TRUE(cottontail::Bigwig::consolidate(root + "/live", &error))
        << error;
    check(cottontail::Bigwig::make(root + "/live", &error));
  }
}

TEST(Erasures, CompressedBlockReclamation) {
  for (const std::string compression : {"null", "zlib"}) {
    std::string error, root = temporary_directory();
    ASSERT_FALSE(root.empty());
    auto feat = cottontail::Featurizer::make("hashing", "", &error);
    auto tok = cottontail::Tokenizer::make("ascii", "", &error);
    auto pc = cottontail::Compressor::make("post", "", &error);
    auto vc = cottontail::Compressor::make(compression, "", &error);
    auto tc = cottontail::Compressor::make(compression, "", &error);
    std::string text;
    for (int i = 0; i < 2052; i++)
      text += "word ";
    text += "\n";
    auto source =
        cottontail::Fiver::make(nullptr, feat, tok, &error, pc, vc, tc);
    ASSERT_NE(source, nullptr) << error;
    source->set_sequence(0);
    ASSERT_TRUE(source->transaction(&error)) << error;
    addr source_p, source_q;
    ASSERT_TRUE(source->appender()->append(text, &source_p, &source_q, &error))
        << error;
    EXPECT_EQ(source->relocate(0), 2052);
    ASSERT_TRUE(source->ready(&error)) << error;
    source->commit();
    source->start();
    std::string original = root + "/original.hazel";
    ASSERT_TRUE(source->hazel(original, &error, 64)) << error;
    auto base = text_layout(original);
    ASSERT_EQ(base.blocks.size(), 4U);
    EXPECT_EQ(base.raw_length, static_cast<addr>(text.size()));
    auto original_hazel = std::dynamic_pointer_cast<cottontail::Hazel>(
        cottontail::Warren::make(original, &error));
    ASSERT_NE(original_hazel, nullptr) << error;
    original_hazel->start();

    // Whole first/middle/tail/all blocks, adjacent erased chunks, no erasures,
    // and a partially erased chunk that must keep its entire compressed block.
    for (unsigned mask : {0U, 1U, 2U, 3U, 8U, 15U, 16U}) {
      SCOPED_TRACE(compression + ":" + std::to_string(mask));
      auto erase =
          cottontail::Fiver::make(nullptr, feat, tok, &error, pc, vc, tc);
      ASSERT_NE(erase, nullptr) << error;
      erase->set_sequence(1);
      ASSERT_TRUE(erase->transaction(&error)) << error;
      std::vector<Interval> deletions;
      for (addr i = 0; i < 4; i++)
        if (mask & (1U << i))
          deletions.push_back({513 * i, 513 * (i + 1) - 1, 0.0});
      if (mask == 16)
        deletions.push_back({0, 511, 0.0});
      for (const auto &e : deletions)
        ASSERT_TRUE(erase->annotator()->erase(e.p, e.q, &error)) << error;
      erase->relocate(2052);
      ASSERT_TRUE(erase->ready(&error)) << error;
      erase->commit();
      erase->start();
      std::string tag = root + "/" + std::to_string(mask);
      auto combined =
          cottontail::Fiver::merge({source, erase}, &error, pc, vc, tc);
      ASSERT_NE(combined, nullptr) << error;
      combined->start();
      ASSERT_TRUE(combined->hazel(tag + "-convert.hazel", &error, 64)) << error;
      ASSERT_TRUE(erase->hazel(tag + "-erase.hazel", &error, 64)) << error;
      auto erase_hazel = std::dynamic_pointer_cast<cottontail::Hazel>(
          cottontail::Warren::make(tag + "-erase.hazel", &error));
      ASSERT_NE(erase_hazel, nullptr) << error;
      erase_hazel->start();
      auto merged = cottontail::Hazel::merge(
          {original_hazel, erase_hazel}, tag + "-merge.hazel", nullptr, &error);
      ASSERT_NE(merged, nullptr) << error;
      merged->start();
      // A subsequent merge must copy holes without trying to read their bytes.
      auto empty =
          cottontail::Fiver::make(nullptr, feat, tok, &error, pc, vc, tc);
      ASSERT_NE(empty, nullptr) << error;
      empty->set_sequence(2);
      ASSERT_TRUE(empty->transaction(&error)) << error;
      empty->relocate(2052);
      ASSERT_TRUE(empty->ready(&error)) << error;
      empty->commit();
      empty->start();
      ASSERT_TRUE(empty->hazel(tag + "-empty.hazel", &error, 64)) << error;
      auto empty_hazel = std::dynamic_pointer_cast<cottontail::Hazel>(
          cottontail::Warren::make(tag + "-empty.hazel", &error));
      ASSERT_NE(empty_hazel, nullptr) << error;
      empty_hazel->start();
      ASSERT_NE(cottontail::Hazel::merge({merged, empty_hazel},
                                         tag + "-again.hazel", nullptr, &error),
                nullptr)
          << error;

      for (const std::string suffix :
           {"-convert.hazel", "-merge.hazel", "-again.hazel"}) {
        auto layout = text_layout(tag + suffix);
        ASSERT_EQ(layout.blocks.size(), base.blocks.size());
        EXPECT_EQ(layout.raw_length, base.raw_length);
        addr old_end = 0, new_end = 0, saved = 0;
        for (size_t i = 0; i < base.blocks.size(); i++) {
          addr old_size = base.blocks[i].compressed_byte_end - old_end;
          addr new_size = layout.blocks[i].compressed_byte_end - new_end;
          EXPECT_EQ(layout.blocks[i].raw_byte_end, base.blocks[i].raw_byte_end);
          bool deleted = mask & (1U << i);
          EXPECT_EQ(new_size, deleted ? 0 : old_size);
          if (deleted)
            saved += old_size;
          old_end = base.blocks[i].compressed_byte_end;
          new_end = layout.blocks[i].compressed_byte_end;
        }
        EXPECT_EQ(layout.payload_length, base.payload_length - saved);
        auto view = cottontail::Warren::make(tag + suffix, &error);
        ASSERT_NE(view, nullptr) << error;
        view->start();
        addr p, q;
        ASSERT_TRUE(view->txt()->range(&p, &q));
        EXPECT_EQ(p, 0);
        EXPECT_EQ(q, 2051);
        EXPECT_EQ(view->txt()->tokens(), 2052);
        expect_hopper(view->idx()->hopper(cottontail::null_feature),
                      select(deletions, true));
        // Check every token, particularly those next to a reclaimed block.
        for (addr token = 0; token < 2052; token++) {
          bool touched = false;
          for (const auto &e : deletions)
            touched = touched || (e.p <= token && token <= e.q);
          EXPECT_EQ(view->txt()->translate(token, token),
                    touched ? "" : source->txt()->translate(token, token))
              << token;
        }
        view->end();
      }
      // The still-open source snapshot remains readable after output GC.
      EXPECT_EQ(original_hazel->txt()->translate(0, 2051), text);
      // Coarser compression keeps a block that includes any live text.
      if (mask == 1) {
        ASSERT_TRUE(combined->hazel(tag + "-coarse.hazel", &error, 1 << 20))
            << error;
        auto layout = text_layout(tag + "-coarse.hazel");
        ASSERT_EQ(layout.blocks.size(), 1U);
        EXPECT_GT(layout.payload_length, 0);
      }
    }
  }
}

TEST(Erasures, ReclaimedTextPreservesAppendAddress) {
  std::string error, root = temporary_directory();
  ASSERT_FALSE(root.empty());
  auto working = cottontail::Working::mkdir(root + "/live", &error);
  auto feat = cottontail::Featurizer::make("hashing", "", &error);
  auto tok = cottontail::Tokenizer::make("ascii", "", &error);
  auto bigwig = cottontail::Bigwig::make(working, feat, tok, &error);
  ASSERT_NE(bigwig, nullptr) << error;
  bigwig->merge(false);
  std::string text;
  for (int i = 0; i < 1026; i++)
    text += "word ";
  ASSERT_TRUE(bigwig->transaction(&error)) << error;
  addr p, q;
  ASSERT_TRUE(bigwig->appender()->append(text, &p, &q, &error)) << error;
  ASSERT_TRUE(bigwig->ready(&error)) << error;
  bigwig->commit();
  ASSERT_TRUE(bigwig->transaction(&error)) << error;
  ASSERT_TRUE(bigwig->annotator()->erase(0, 1025, &error)) << error;
  ASSERT_TRUE(bigwig->ready(&error)) << error;
  bigwig->commit();
  bigwig.reset();
  ASSERT_TRUE(cottontail::Bigwig::consolidate(root + "/live", &error)) << error;
  auto names = working->ls("hazel");
  ASSERT_EQ(names.size(), 1U);
  auto layout = text_layout(working->make_name(names[0]));
  EXPECT_EQ(layout.raw_length, static_cast<addr>(text.size()));
  EXPECT_EQ(layout.payload_length, 0);
  bigwig = cottontail::Bigwig::make(root + "/live", &error);
  ASSERT_NE(bigwig, nullptr) << error;
  ASSERT_TRUE(bigwig->transaction(&error)) << error;
  ASSERT_TRUE(bigwig->appender()->append("new content\n", &p, &q, &error))
      << error;
  ASSERT_TRUE(bigwig->ready(&error)) << error;
  bigwig->commit();
  bigwig->start();
  auto hopper = bigwig->idx()->hopper(feat->featurize("new"));
  hopper->tau(0, &p, &q);
  EXPECT_EQ(p, 1026);
  EXPECT_EQ(q, 1026);
  EXPECT_EQ(bigwig->txt()->translate(p, q), "new ");
  EXPECT_EQ(bigwig->txt()->translate(0, 1025), "");
  bigwig->end();
  bigwig.reset();
  ASSERT_TRUE(cottontail::Bigwig::consolidate(root + "/live", &error)) << error;
  bigwig = cottontail::Bigwig::make(root + "/live", &error);
  ASSERT_NE(bigwig, nullptr) << error;
  bigwig->start();
  EXPECT_EQ(bigwig->txt()->translate(1026, 1027), "new content\n");
  EXPECT_EQ(bigwig->txt()->translate(0, 1025), "");
  bigwig->end();
}

TEST(Erasures, ReclamationKeepsUnindexedTokenlessBytes) {
  std::string root = temporary_directory(), error;
  ASSERT_FALSE(root.empty());
  auto feat = cottontail::Featurizer::make("hashing", "", &error);
  auto tok = cottontail::Tokenizer::make("ascii", "", &error);
  for (const std::string text : {"", "\n \t\n"}) {
    auto fiver = cottontail::Fiver::make(nullptr, feat, tok, &error);
    ASSERT_NE(fiver, nullptr) << error;
    fiver->set_sequence(0);
    ASSERT_TRUE(fiver->transaction(&error)) << error;
    addr p, q;
    ASSERT_TRUE(fiver->appender()->append(text, &p, &q, &error)) << error;
    ASSERT_TRUE(fiver->annotator()->erase(0, 100, &error)) << error;
    fiver->relocate(0);
    ASSERT_TRUE(fiver->ready(&error)) << error;
    fiver->commit();
    fiver->start();
    std::string filename = root + "/" + std::to_string(text.size()) + ".hazel";
    ASSERT_TRUE(fiver->hazel(filename, &error, 64)) << error;
    auto layout = text_layout(filename);
    EXPECT_EQ(layout.raw_length, static_cast<addr>(text.size()));
    if (text.empty()) {
      EXPECT_EQ(layout.payload_length, 0);
      EXPECT_TRUE(layout.blocks.empty());
    } else {
      EXPECT_GT(layout.payload_length, 0);
      EXPECT_EQ(layout.blocks.size(), 1U);
    }
    auto view = cottontail::Warren::make(filename, &error);
    ASSERT_NE(view, nullptr) << error;
    view->start();
    EXPECT_EQ(view->txt()->tokens(), 0);
    EXPECT_EQ(view->txt()->translate(0, cottontail::maxfinity), "");
    view->end();
  }
}
