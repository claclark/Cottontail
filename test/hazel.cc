#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "src/bigwig.h"
#include "src/cottontail.h"
#include "src/fiver.h"
#include "src/hazel.h"

namespace {

struct Posting {
  cottontail::addr p;
  cottontail::addr q;
  cottontail::fval v;
};

struct ShardName {
  cottontail::addr start;
  cottontail::addr end;
  std::string name;
};

struct CompressorProfile {
  std::string label;
  std::shared_ptr<cottontail::Compressor> posting;
  std::shared_ptr<cottontail::Compressor> fvalue;
  std::shared_ptr<cottontail::Compressor> text;
};

CompressorProfile compressor_profile(const std::string &label,
                                      const std::string &posting,
                                      const std::string &fvalue,
                                      const std::string &text) {
  std::string error;
  CompressorProfile profile;
  profile.label = label;
  profile.posting = cottontail::Compressor::make(posting, "", &error);
  EXPECT_NE(profile.posting, nullptr) << error;
  profile.fvalue = cottontail::Compressor::make(fvalue, "", &error);
  EXPECT_NE(profile.fvalue, nullptr) << error;
  profile.text = cottontail::Compressor::make(text, "", &error);
  EXPECT_NE(profile.text, nullptr) << error;
  return profile;
}

std::string test_root() {
  const char *tmp = std::getenv("TEST_TMPDIR");
  if (tmp != nullptr && *tmp != '\0')
    return tmp;
  return "/tmp";
}

std::string seq2str(cottontail::addr sequence) {
  std::stringstream ss;
  ss.fill('0');
  ss.width(20);
  ss << sequence;
  return ss.str();
}

std::string shard_name(const std::string &prefix, cottontail::addr start,
                       cottontail::addr end) {
  return prefix + "." + seq2str(start) + "." + seq2str(end);
}

bool parse_shard_name(const std::string &name, const std::string &prefix,
                      cottontail::addr *start, cottontail::addr *end) {
  std::string full_prefix = prefix + ".";
  if (name.compare(0, full_prefix.size(), full_prefix) != 0)
    return false;
  size_t dot = name.find('.', full_prefix.size());
  if (dot == std::string::npos)
    return false;
  try {
    *start = std::stoll(name.substr(full_prefix.size(),
                                    dot - full_prefix.size()));
    *end = std::stoll(name.substr(dot + 1));
  } catch (...) {
    return false;
  }
  return *start >= 0 && *end >= *start;
}

std::vector<ShardName> fiver_shards(std::shared_ptr<cottontail::Working> working) {
  std::vector<ShardName> shards;
  for (auto &name : working->ls("fiver")) {
    ShardName shard;
    shard.name = name;
    EXPECT_TRUE(parse_shard_name(name, "fiver", &shard.start, &shard.end))
        << name;
    shards.push_back(shard);
  }
  std::sort(shards.begin(), shards.end(), [](const ShardName &a,
                                             const ShardName &b) {
    return a.start < b.start || (a.start == b.start && a.end < b.end);
  });
  return shards;
}

std::vector<std::string> corpus() {
  std::vector<std::string> files;
  files.push_back(
      "How do I love thee? Let me count the ways.\n"
      "I love thee to the depth and breadth and height\n"
      "My soul can reach, when feeling out of sight\n"
      "For the ends of being and ideal grace.\n"
      "I love thee to the level of every day's\n"
      "Most quiet need, by sun and candle-light.\n"
      "I love thee freely, as men strive for right.\n"
      "I love thee purely, as they turn from praise.\n"
      "I love thee with the passion put to use\n"
      "In my old griefs, and with my childhood's faith.\n"
      "I love thee with a love I seemed to lose\n"
      "With my lost saints. I love thee with the breath,\n"
      "Smiles, tears, of all my life; and, if God choose,\n"
      "I shall but love thee better after death.\n");
  files.push_back(
      "My mistress' eyes are nothing like the sun;\n"
      "Coral is far more red than her lips' red;\n"
      "If snow be white, why then her breasts are dun;\n"
      "If hairs be wires, black wires grow on her head.\n"
      "I have seen roses damasked, red and white,\n"
      "But no such roses see I in her cheeks;\n"
      "And in some perfumes is there more delight\n"
      "Than in the breath that from my mistress reeks.\n"
      "I love to hear her speak, yet well I know\n"
      "That music hath a far more pleasing sound;\n"
      "I grant I never saw a goddess go;\n"
      "My mistress, when she walks, treads on the ground:\n"
      "And yet, by heaven, I think my love as rare\n"
      "As any she belied with false compare.\n");
  std::stringstream third;
  third << "Like as the waves make towards the pebbled shore,\n"
        << "So do our minutes hasten to their end;\n"
        << "Each changing place with that which goes before,\n"
        << "Our love shall live, and later life renew.\n";
  for (int i = 0; i < 140; i++) {
    third << "steady love token alpha beta gamma line " << i << "\n";
  }
  files.push_back(third.str());
  return files;
}

std::vector<std::string> write_corpus(const std::string &root,
                                      const std::string &label) {
  std::vector<std::string> filenames;
  std::vector<std::string> texts = corpus();
  for (size_t i = 0; i < texts.size(); i++) {
    std::string filename =
        root + "/" + label + ".source." + std::to_string(i) + ".txt";
    std::ofstream out(filename);
    EXPECT_FALSE(out.fail()) << filename;
    out << texts[i];
    out.close();
    filenames.push_back(filename);
  }
  return filenames;
}

void append_text_file(std::shared_ptr<cottontail::Bigwig> bigwig,
                      const std::string &filename, cottontail::addr ordinal) {
  std::ifstream in(filename);
  ASSERT_FALSE(in.fail()) << filename;
  std::string error;
  ASSERT_TRUE(bigwig->transaction(&error)) << error;
  cottontail::addr line_feature = bigwig->featurizer()->featurize("line:");
  cottontail::addr file_feature = bigwig->featurizer()->featurize("file:");
  cottontail::addr ordinal_feature =
      bigwig->featurizer()->featurize("ordinal:");
  cottontail::addr singleton_feature =
      bigwig->featurizer()->featurize("singleton:");
  cottontail::addr file_p = cottontail::maxfinity;
  cottontail::addr file_q = cottontail::minfinity;
  bool have_tokens = false;
  bool first_line = true;
  std::string line;
  while (std::getline(in, line)) {
    cottontail::addr p, q;
    ASSERT_TRUE(bigwig->appender()->append(line + "\n", &p, &q, &error))
        << error;
    if (p <= q) {
      ASSERT_TRUE(bigwig->annotator()->annotate(line_feature, p, q, &error))
          << error;
      if (first_line) {
        ASSERT_TRUE(bigwig->annotator()->annotate(ordinal_feature, p, q,
                                                  ordinal, &error))
            << error;
      }
      if (ordinal == 1 && first_line) {
        ASSERT_TRUE(bigwig->annotator()->annotate(singleton_feature, p, p,
                                                  &error))
            << error;
      }
      file_p = std::min(file_p, p);
      file_q = std::max(file_q, q);
      have_tokens = true;
    }
    first_line = false;
  }
  ASSERT_FALSE(in.bad()) << filename;
  ASSERT_TRUE(have_tokens) << filename;
  ASSERT_TRUE(bigwig->annotator()->annotate(file_feature, file_p, file_q,
                                            &error))
      << error;
  ASSERT_TRUE(bigwig->ready()) << error;
  bigwig->commit();
}

std::shared_ptr<cottontail::Bigwig>
build_bigwig(const std::string &burrow,
             const std::vector<std::string> &filenames,
             const CompressorProfile &compressors) {
  std::string error;
  std::shared_ptr<cottontail::Working> working =
      cottontail::Working::mkdir(burrow, &error);
  EXPECT_NE(working, nullptr) << error;
  if (working == nullptr)
    return nullptr;
  std::shared_ptr<cottontail::Featurizer> featurizer =
      cottontail::Featurizer::make("hashing", "", &error, working);
  EXPECT_NE(featurizer, nullptr) << error;
  if (featurizer == nullptr)
    return nullptr;
  std::shared_ptr<cottontail::Tokenizer> tokenizer =
      cottontail::Tokenizer::make("utf8", "", &error);
  EXPECT_NE(tokenizer, nullptr) << error;
  if (tokenizer == nullptr)
    return nullptr;
  std::shared_ptr<cottontail::Bigwig> bigwig =
      cottontail::Bigwig::make(working, featurizer, tokenizer, &error, nullptr,
                               compressors.posting, compressors.fvalue,
                               compressors.text);
  EXPECT_NE(bigwig, nullptr) << error;
  if (bigwig == nullptr)
    return nullptr;
  bigwig->merge(false);
  for (size_t i = 0; i < filenames.size(); i++)
    append_text_file(bigwig, filenames[i], i + 1);
  return bigwig;
}

std::vector<Posting> collect(std::unique_ptr<cottontail::Hopper> hopper) {
  std::vector<Posting> postings;
  if (hopper == nullptr)
    return postings;
  cottontail::addr p, q;
  cottontail::fval v;
  for (hopper->tau(cottontail::minfinity + 1, &p, &q, &v);
       p < cottontail::maxfinity; hopper->tau(p + 1, &p, &q, &v))
    postings.push_back({p, q, v});
  return postings;
}

void expect_postings_eq(const std::vector<Posting> &left,
                        const std::vector<Posting> &right) {
  ASSERT_EQ(left.size(), right.size());
  for (size_t i = 0; i < left.size(); i++) {
    EXPECT_EQ(left[i].p, right[i].p) << i;
    EXPECT_EQ(left[i].q, right[i].q) << i;
    EXPECT_DOUBLE_EQ(left[i].v, right[i].v) << i;
  }
}

void expect_probe_eq(std::shared_ptr<cottontail::Warren> left,
                     std::shared_ptr<cottontail::Warren> right,
                     cottontail::addr feature) {
  std::vector<cottontail::addr> probes = {
      cottontail::minfinity + 1, 0, 1, 5, 16, 64, 129, 256,
      cottontail::maxfinity - 1};
  for (auto k : probes) {
    std::unique_ptr<cottontail::Hopper> a = left->idx()->hopper(feature);
    std::unique_ptr<cottontail::Hopper> b = right->idx()->hopper(feature);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(a->L(k), b->L(k)) << k;
    EXPECT_EQ(a->R(k), b->R(k)) << k;
    cottontail::addr ap, aq, bp, bq;
    cottontail::fval av, bv;
    a->tau(k, &ap, &aq, &av);
    b->tau(k, &bp, &bq, &bv);
    EXPECT_EQ(ap, bp) << "tau " << k;
    EXPECT_EQ(aq, bq) << "tau " << k;
    EXPECT_DOUBLE_EQ(av, bv) << "tau " << k;
    a->rho(k, &ap, &aq, &av);
    b->rho(k, &bp, &bq, &bv);
    EXPECT_EQ(ap, bp) << "rho " << k;
    EXPECT_EQ(aq, bq) << "rho " << k;
    EXPECT_DOUBLE_EQ(av, bv) << "rho " << k;
    a->uat(k, &ap, &aq, &av);
    b->uat(k, &bp, &bq, &bv);
    EXPECT_EQ(ap, bp) << "uat " << k;
    EXPECT_EQ(aq, bq) << "uat " << k;
    EXPECT_DOUBLE_EQ(av, bv) << "uat " << k;
    a->ohr(k, &ap, &aq, &av);
    b->ohr(k, &bp, &bq, &bv);
    EXPECT_EQ(ap, bp) << "ohr " << k;
    EXPECT_EQ(aq, bq) << "ohr " << k;
    EXPECT_DOUBLE_EQ(av, bv) << "ohr " << k;
  }
}

void expect_feature_eq(std::shared_ptr<cottontail::Warren> left,
                       std::shared_ptr<cottontail::Warren> right,
                       const std::string &feature_name) {
  cottontail::addr left_feature = left->featurizer()->featurize(feature_name);
  cottontail::addr right_feature = right->featurizer()->featurize(feature_name);
  ASSERT_EQ(left_feature, right_feature) << feature_name;
  EXPECT_EQ(left->idx()->count(left_feature), right->idx()->count(right_feature))
      << feature_name;
  expect_postings_eq(collect(left->idx()->hopper(left_feature)),
                     collect(right->idx()->hopper(right_feature)));
  expect_probe_eq(left, right, left_feature);
}

void expect_gcl_eq(std::shared_ptr<cottontail::Warren> left,
                   std::shared_ptr<cottontail::Warren> right,
                   const std::string &gcl, bool require_match = false) {
  std::string left_error;
  std::string right_error;
  std::vector<Posting> left_postings =
      collect(left->hopper_from_gcl(gcl, &left_error));
  std::vector<Posting> right_postings =
      collect(right->hopper_from_gcl(gcl, &right_error));
  ASSERT_EQ(left_error, "") << left_error;
  ASSERT_EQ(right_error, "") << right_error;
  expect_postings_eq(left_postings, right_postings);
  if (require_match) {
    ASSERT_FALSE(left_postings.empty()) << gcl;
  }
  for (auto &posting : left_postings) {
    EXPECT_EQ(left->txt()->translate(posting.p, posting.q),
              right->txt()->translate(posting.p, posting.q))
        << gcl << " " << posting.p << "," << posting.q;
  }
}

void expect_txt_eq(std::shared_ptr<cottontail::Warren> left,
                   std::shared_ptr<cottontail::Warren> right) {
  EXPECT_EQ(left->txt()->tokens(), right->txt()->tokens());
  cottontail::addr left_p, left_q, right_p, right_q;
  EXPECT_EQ(left->txt()->range(&left_p, &left_q),
            right->txt()->range(&right_p, &right_q));
  EXPECT_EQ(left_p, right_p);
  EXPECT_EQ(left_q, right_q);
  std::vector<std::pair<cottontail::addr, cottontail::addr>> ranges = {
      {left_p, std::min(left_q, left_p + 9)},
      {std::max(left_p, (cottontail::addr)120), std::min(left_q, (cottontail::addr)150)},
      {std::max(left_p, (cottontail::addr)250), std::min(left_q, (cottontail::addr)290)},
      {std::max(left_p, left_q - 20), left_q},
      {left_p, left_q}};
  for (auto &range : ranges)
    if (range.first <= range.second) {
      EXPECT_EQ(left->txt()->translate(range.first, range.second),
                right->txt()->translate(range.first, range.second))
          << range.first << "," << range.second;
    }
}

void expect_warrens_eq(std::shared_ptr<cottontail::Warren> left,
                       std::shared_ptr<cottontail::Warren> right) {
  expect_txt_eq(left, right);
  expect_feature_eq(left, right, "line:");
  expect_feature_eq(left, right, "file:");
  expect_feature_eq(left, right, "love");
  expect_feature_eq(left, right, "the");
  expect_feature_eq(left, right, "singleton:");
  expect_feature_eq(left, right, "ordinal:");
  expect_feature_eq(left, right, "absent-feature:");
  expect_gcl_eq(left, right, "line:", true);
  expect_gcl_eq(left, right, "file:", true);
  expect_gcl_eq(left, right, "(>> line: love)", true);
  expect_gcl_eq(left, right, "(>> file: love)", true);
  expect_gcl_eq(left, right, "\"Let me count the ways\"");
  expect_gcl_eq(left, right, "\"after death my mistress\"");
  expect_gcl_eq(left, right, "\"not present anywhere\"");
  expect_gcl_eq(left, right, "love", true);
  expect_gcl_eq(left, right, "love", true);
}

void expect_started_clone_eq(std::shared_ptr<cottontail::Warren> source,
                             std::shared_ptr<cottontail::Warren> warren) {
  ASSERT_TRUE(warren->started());
  std::string error;
  std::shared_ptr<cottontail::Warren> clone = warren->clone(&error);
  ASSERT_NE(clone, nullptr) << error;
  ASSERT_TRUE(clone->started());
  expect_warrens_eq(warren, clone);
  warren->end();
  ASSERT_TRUE(clone->started());
  expect_warrens_eq(source, clone);
  clone->end();
}

std::shared_ptr<cottontail::Warren> open_started(const std::string &burrow) {
  std::string error;
  std::shared_ptr<cottontail::Warren> warren =
      cottontail::Warren::make(burrow, &error);
  EXPECT_NE(warren, nullptr) << error;
  if (warren != nullptr)
    warren->start();
  return warren;
}

void run_hazel_merge_regression(cottontail::addr chunk_size,
                                const CompressorProfile &compressors) {
  std::string root = test_root();
  std::string label = "hazel_" + compressors.label + "_" +
                      std::to_string(chunk_size);
  std::string burrow = root + "/" + label + ".burrow";
  std::vector<std::string> filenames = write_corpus(root, label);
  std::shared_ptr<cottontail::Bigwig> bigwig =
      build_bigwig(burrow, filenames, compressors);
  ASSERT_NE(bigwig, nullptr);

  std::shared_ptr<cottontail::Working> working =
      cottontail::Working::make(burrow);
  ASSERT_NE(working, nullptr);
  std::vector<ShardName> fivers = fiver_shards(working);
  ASSERT_EQ(fivers.size(), filenames.size());

  std::vector<std::string> hazels;
  for (auto &shard : fivers) {
    std::string error;
    std::shared_ptr<cottontail::Fiver> fiver = cottontail::Fiver::unpickle(
        shard.name, working, bigwig->featurizer(), bigwig->tokenizer(), &error,
        compressors.posting, compressors.fvalue, compressors.text);
    ASSERT_NE(fiver, nullptr) << error;
    fiver->start();
    ASSERT_TRUE(fiver->hazel(&error, false, chunk_size, "")) << error;
    std::string hazel_name = shard_name("hazel", shard.start, shard.end);
    hazels.push_back(hazel_name);
    std::shared_ptr<cottontail::Warren> hazel =
        open_started(working->make_name(hazel_name));
    ASSERT_NE(hazel, nullptr);
    expect_warrens_eq(fiver, hazel);
    hazel->end();
    fiver->end();
  }

  std::string error;
  ASSERT_TRUE(cottontail::Hazel::merge(working, hazels, "", &error)) << error;
  std::string final_name =
      shard_name("hazel", fivers.front().start, fivers.back().end);
  std::string final_path = working->make_name(final_name);
  std::string standalone_path = root + "/" + label + ".merged.hazel";
  ASSERT_EQ(std::rename(final_path.c_str(), standalone_path.c_str()), 0);
  for (auto &hazel : hazels)
    std::remove(working->make_name(hazel).c_str());

  std::shared_ptr<cottontail::Warren> source = open_started(burrow);
  ASSERT_NE(source, nullptr);
  std::shared_ptr<cottontail::Warren> merged = open_started(standalone_path);
  ASSERT_NE(merged, nullptr);
  expect_warrens_eq(source, merged);
  expect_gcl_eq(source, merged, "\"Let me count the ways\"", true);
  expect_started_clone_eq(source, merged);
  source->end();
}

} // namespace

TEST(HazelMerge, PreservesBigwigBehaviorSmallChunks) {
  run_hazel_merge_regression(
      16, compressor_profile("null", "null", "null", "null"));
}

TEST(HazelMerge, PreservesBigwigBehaviorWithRealCompressorsSmallChunks) {
  run_hazel_merge_regression(
      16, compressor_profile("real", "post", "zlib", "zlib"));
}

TEST(HazelMerge, PreservesBigwigBehaviorWithBadCompressorsSmallChunks) {
  run_hazel_merge_regression(
      16, compressor_profile("bad", "bad", "bad", "bad"));
}

TEST(HazelMerge, PreservesBigwigBehaviorWithRealCompressorsDefaultChunks) {
  run_hazel_merge_regression(
      64 * 1024, compressor_profile("real_default", "post", "zlib", "zlib"));
}
