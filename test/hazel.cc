#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

#include "gtest/gtest.h"

#include "src/bigwig.h"
#include "src/cottontail.h"
#include "src/fiver.h"
#include "src/hazel.h"
#include "src/simple.h"

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

std::string merge_segment_name(size_t segment, cottontail::addr start,
                               cottontail::addr end) {
  return "merge." + std::to_string(segment) + "." + seq2str(start) + "." +
         seq2str(end);
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
    *start =
        std::stoll(name.substr(full_prefix.size(), dot - full_prefix.size()));
    *end = std::stoll(name.substr(dot + 1));
  } catch (...) {
    return false;
  }
  return *start >= 0 && *end >= *start;
}

std::vector<ShardName>
fiver_shards(std::shared_ptr<cottontail::Working> working) {
  std::vector<ShardName> shards;
  for (auto &name : working->ls("fiver")) {
    ShardName shard;
    shard.name = name;
    EXPECT_TRUE(parse_shard_name(name, "fiver", &shard.start, &shard.end))
        << name;
    shards.push_back(shard);
  }
  std::sort(shards.begin(), shards.end(),
            [](const ShardName &a, const ShardName &b) {
              return a.start < b.start || (a.start == b.start && a.end < b.end);
            });
  return shards;
}

std::vector<std::string> corpus() {
  std::vector<std::string> files;
  files.push_back("How do I love thee? Let me count the ways.\n"
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
  files.push_back("My mistress' eyes are nothing like the sun;\n"
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
  cottontail::addr deletion_pair_feature =
      bigwig->featurizer()->featurize("deletion-pair:");
  cottontail::addr file_p = cottontail::maxfinity;
  cottontail::addr file_q = cottontail::minfinity;
  bool have_tokens = false;
  bool first_line = true;
  size_t line_number = 0;
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
        ASSERT_TRUE(
            bigwig->annotator()->annotate(singleton_feature, p, p, &error))
            << error;
      }
      if (ordinal == 1 && line_number < 2) {
        ASSERT_TRUE(
            bigwig->annotator()->annotate(deletion_pair_feature, p, p, &error))
            << error;
      }
      file_p = std::min(file_p, p);
      file_q = std::max(file_q, q);
      have_tokens = true;
    }
    first_line = false;
    line_number++;
  }
  ASSERT_FALSE(in.bad()) << filename;
  ASSERT_TRUE(have_tokens) << filename;
  ASSERT_TRUE(
      bigwig->annotator()->annotate(file_feature, file_p, file_q, &error))
      << error;
  if (ordinal == 3) {
    ASSERT_TRUE(bigwig->annotator()->erase(0, 0, &error)) << error;
  }
  ASSERT_TRUE(bigwig->ready()) << error;
  bigwig->commit();
}

void touch(std::shared_ptr<cottontail::Working> working,
           const std::string &name) {
  std::ofstream out(working->make_name(name), std::ios::binary);
  ASSERT_FALSE(out.fail()) << name;
}

std::shared_ptr<cottontail::Bigwig>
make_bigwig(const std::string &burrow, const CompressorProfile &compressors) {
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
  std::shared_ptr<cottontail::Bigwig> bigwig = cottontail::Bigwig::make(
      working, featurizer, tokenizer, &error, nullptr, compressors.posting,
      compressors.fvalue, compressors.text);
  EXPECT_NE(bigwig, nullptr) << error;
  if (bigwig == nullptr)
    return nullptr;
  bigwig->merge(false);
  return bigwig;
}

std::shared_ptr<cottontail::Bigwig>
build_bigwig(const std::string &burrow,
             const std::vector<std::string> &filenames,
             const CompressorProfile &compressors) {
  std::shared_ptr<cottontail::Bigwig> bigwig = make_bigwig(burrow, compressors);
  if (bigwig == nullptr)
    return nullptr;
  for (size_t i = 0; i < filenames.size(); i++)
    append_text_file(bigwig, filenames[i], i + 1);
  return bigwig;
}

void append_transaction(std::shared_ptr<cottontail::Bigwig> bigwig,
                        const std::string &text, bool commit) {
  std::string error;
  cottontail::addr p, q;
  ASSERT_TRUE(bigwig->transaction(&error)) << error;
  ASSERT_TRUE(bigwig->appender()->append(text, &p, &q, &error)) << error;
  ASSERT_TRUE(bigwig->ready(&error)) << error;
  if (commit)
    bigwig->commit();
  else
    bigwig->abort();
}

std::shared_ptr<cottontail::Hazel>
convert_fiver(std::shared_ptr<cottontail::Bigwig> bigwig,
              std::shared_ptr<cottontail::Working> working,
              const ShardName &shard, const CompressorProfile &compressors) {
  std::string error;
  std::shared_ptr<cottontail::Fiver> fiver = cottontail::Fiver::unpickle(
      shard.name, working, bigwig->featurizer(), bigwig->tokenizer(), &error,
      compressors.posting, compressors.fvalue, compressors.text);
  EXPECT_NE(fiver, nullptr) << error;
  if (fiver == nullptr)
    return nullptr;
  fiver->start();
  std::shared_ptr<cottontail::Warren> converted =
      fiver->hazel(&error, 64 * 1024, "");
  fiver->end();
  EXPECT_NE(converted, nullptr) << error;
  if (converted == nullptr)
    return nullptr;
  EXPECT_TRUE(working->remove(shard.name, &error)) << error;
  return std::dynamic_pointer_cast<cottontail::Hazel>(converted);
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
  EXPECT_EQ(left->idx()->count(left_feature),
            right->idx()->count(right_feature))
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
      {std::max(left_p, (cottontail::addr)120),
       std::min(left_q, (cottontail::addr)150)},
      {std::max(left_p, (cottontail::addr)250),
       std::min(left_q, (cottontail::addr)290)},
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

void expect_hazel_trim_memory_eq(std::shared_ptr<cottontail::Warren> source,
                                 std::shared_ptr<cottontail::Warren> warren) {
  ASSERT_TRUE(warren->started());
  std::shared_ptr<cottontail::Hazel> hazel =
      std::dynamic_pointer_cast<cottontail::Hazel>(warren);
  ASSERT_NE(hazel, nullptr);
  cottontail::addr feature = warren->featurizer()->featurize("love");
  ASSERT_GT(warren->idx()->count(feature), 1);

  std::shared_ptr<cottontail::SimplePosting> before = hazel->posting(feature);
  ASSERT_NE(before, nullptr);
  EXPECT_EQ(before, hazel->posting(feature));
  std::vector<Posting> expected = collect(warren->idx()->hopper(feature));
  std::unique_ptr<cottontail::Hopper> active =
      warren->idx()->hopper(feature);
  ASSERT_NE(active, nullptr);

  std::string error;
  std::shared_ptr<cottontail::Warren> clone = warren->clone(&error);
  ASSERT_NE(clone, nullptr) << error;
  ASSERT_TRUE(clone->started());
  std::shared_ptr<cottontail::Hazel> cloned_hazel =
      std::dynamic_pointer_cast<cottontail::Hazel>(clone);
  ASSERT_NE(cloned_hazel, nullptr);
  EXPECT_EQ(before, cloned_hazel->posting(feature));

  warren->trim_memory();
  std::shared_ptr<cottontail::SimplePosting> after =
      cloned_hazel->posting(feature);
  ASSERT_NE(after, nullptr);
  EXPECT_NE(before, after);
  expect_postings_eq(expected, collect(std::move(active)));

  warren->trim_memory();
  clone->trim_memory();
  std::shared_ptr<cottontail::SimplePosting> reloaded =
      hazel->posting(feature);
  ASSERT_NE(reloaded, nullptr);
  EXPECT_NE(after, reloaded);
  expect_feature_eq(source, clone, "love");
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
                                const CompressorProfile &compressors,
                                bool truncated_recovery = false) {
  std::string root = test_root();
  std::string label =
      "hazel_" + compressors.label + "_" + std::to_string(chunk_size);
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
  std::vector<std::shared_ptr<cottontail::Hazel>> source_hazels;
  std::vector<cottontail::addr> source_hazel_estimates;
  for (auto &shard : fivers) {
    std::string error;
    std::shared_ptr<cottontail::Fiver> fiver = cottontail::Fiver::unpickle(
        shard.name, working, bigwig->featurizer(), bigwig->tokenizer(), &error,
        compressors.posting, compressors.fvalue, compressors.text);
    ASSERT_NE(fiver, nullptr) << error;
    fiver->start();
    std::string hazel_name = shard_name("hazel", shard.start, shard.end);
    hazels.push_back(hazel_name);
    std::shared_ptr<cottontail::Warren> hazel =
        fiver->hazel(&error, chunk_size, "");
    ASSERT_NE(hazel, nullptr) << error;
    hazel->start();
    std::shared_ptr<cottontail::Owsla> hazel_owsla =
        std::dynamic_pointer_cast<cottontail::Owsla>(hazel);
    ASSERT_NE(hazel_owsla, nullptr);
    std::shared_ptr<cottontail::Hazel> source_hazel =
        std::dynamic_pointer_cast<cottontail::Hazel>(hazel);
    ASSERT_NE(source_hazel, nullptr);
    source_hazels.push_back(source_hazel);
    cottontail::addr source_hazel_estimate = hazel_owsla->estimated_size();
    EXPECT_GT(source_hazel_estimate, 0) << hazel_name;
    source_hazel_estimates.push_back(source_hazel_estimate);
    expect_warrens_eq(fiver, hazel);
    hazel->end();
    fiver->end();
  }

  std::string error;
  if (truncated_recovery) {
    std::string segment =
        merge_segment_name(0, fivers.front().start, fivers.back().end);
    std::ofstream out(working->make_name(segment), std::ios::binary);
    ASSERT_FALSE(out.fail()) << segment;
    std::vector<std::shared_ptr<cottontail::SimplePosting>> exclusions;
    for (auto &hazel : source_hazels) {
      hazel->start();
      auto posting = hazel->posting(cottontail::null_feature);
      if (posting != nullptr)
        exclusions.push_back(posting);
      hazel->end();
    }
    ASSERT_FALSE(exclusions.empty());
    auto factory = cottontail::SimplePostingFactory::make(compressors.posting,
                                                          compressors.fvalue);
    auto exclusion = factory->posting_from_merge(exclusions);
    ASSERT_NE(exclusion, nullptr);
    exclusion->write(&out);
    cottontail::PstRecord record(123, 1, sizeof(cottontail::addr), 0, 0);
    out.write(reinterpret_cast<const char *>(&record), sizeof(record));
    out.close();
  }
  ASSERT_TRUE(cottontail::Hazel::merge(working, hazels, "", &error)) << error;
  EXPECT_TRUE(working->ls("merge").empty());
  EXPECT_TRUE(working->ls("mrg").empty());
  std::string final_name =
      shard_name("hazel", fivers.front().start, fivers.back().end);
  std::string final_path = working->make_name(final_name);
  EXPECT_NE(access((final_path + ".tmp").c_str(), F_OK), 0);
  std::string standalone_path = root + "/" + label + ".merged.hazel";
  ASSERT_EQ(std::rename(final_path.c_str(), standalone_path.c_str()), 0);
  for (auto &hazel : hazels)
    std::remove(working->make_name(hazel).c_str());

  std::shared_ptr<cottontail::Warren> source = open_started(burrow);
  ASSERT_NE(source, nullptr);
  std::shared_ptr<cottontail::Warren> merged = open_started(standalone_path);
  ASSERT_NE(merged, nullptr);
  std::shared_ptr<cottontail::Owsla> merged_owsla =
      std::dynamic_pointer_cast<cottontail::Owsla>(merged);
  ASSERT_NE(merged_owsla, nullptr);
  cottontail::addr singleton_feature =
      source->featurizer()->featurize("singleton:");
  cottontail::addr deletion_pair_feature =
      source->featurizer()->featurize("deletion-pair:");
  EXPECT_EQ(source->idx()->count(singleton_feature), 1);
  EXPECT_EQ(merged->idx()->count(singleton_feature), 0);
  EXPECT_EQ(source->idx()->count(deletion_pair_feature), 2);
  EXPECT_EQ(merged->idx()->count(deletion_pair_feature), 1);
  std::vector<Posting> surviving =
      collect(merged->idx()->hopper(deletion_pair_feature));
  ASSERT_EQ(surviving.size(), size_t(1));
  EXPECT_EQ(surviving[0].p, surviving[0].q);
  EXPECT_DOUBLE_EQ(surviving[0].v, 0.0);
  cottontail::addr merged_hazel_estimate = merged_owsla->estimated_size();
  EXPECT_GT(merged_hazel_estimate, 0) << final_name;
  for (size_t i = 0; i < source_hazel_estimates.size(); i++)
    EXPECT_GT(merged_hazel_estimate, source_hazel_estimates[i])
        << final_name << " <= " << hazels[i];
  expect_warrens_eq(source, merged);
  expect_gcl_eq(source, merged, "\"Let me count the ways\"", true);
  expect_hazel_trim_memory_eq(source, merged);
  expect_started_clone_eq(source, merged);
  source->end();
}

void run_bigwig_hazel_activation_regression(
    const CompressorProfile &compressors) {
  std::string root = test_root();
  std::string label = "bigwig_hazel_activation_" + compressors.label;
  std::string burrow = root + "/" + label + ".burrow";
  std::vector<std::string> filenames = write_corpus(root, label);
  std::shared_ptr<cottontail::Bigwig> source =
      build_bigwig(burrow, filenames, compressors);
  ASSERT_NE(source, nullptr);
  source->start();

  std::shared_ptr<cottontail::Working> working =
      cottontail::Working::make(burrow);
  ASSERT_NE(working, nullptr);
  std::vector<ShardName> fivers = fiver_shards(working);
  ASSERT_EQ(fivers.size(), filenames.size());

  std::string error;
  std::shared_ptr<cottontail::Fiver> fiver = cottontail::Fiver::unpickle(
      fivers.front().name, working, source->featurizer(), source->tokenizer(),
      &error, compressors.posting, compressors.fvalue, compressors.text);
  ASSERT_NE(fiver, nullptr) << error;
  fiver->start();
  std::shared_ptr<cottontail::Hazel> hazel =
      fiver->hazel(&error, 64 * 1024, "");
  ASSERT_NE(hazel, nullptr) << error;
  fiver->end();
  ASSERT_TRUE(working->remove(fivers.front().name, &error)) << error;

  std::shared_ptr<cottontail::Warren> mixed = open_started(burrow);
  ASSERT_NE(mixed, nullptr);
  expect_warrens_eq(source, mixed);
  mixed->end();
  source->end();
  mixed.reset();
  source.reset();
  fiver.reset();
  hazel.reset();

  ASSERT_TRUE(cottontail::Bigwig::consolidate(burrow, &error)) << error;
  EXPECT_TRUE(working->ls("fiver").empty());
  EXPECT_EQ(working->ls("hazel").size(), size_t(1));
  std::shared_ptr<cottontail::Warren> consolidated = open_started(burrow);
  ASSERT_NE(consolidated, nullptr);
  expect_gcl_eq(consolidated, consolidated, "\"Let me count the ways\"", true);
  consolidated->end();
}

} // namespace

TEST(HazelMergeRecovery, NewSegmentsResumeAndLegacyFilesAreDiscarded) {
  std::string burrow = test_root() + "/hazel_recovery_discard.burrow";
  std::string error;
  std::shared_ptr<cottontail::Working> working =
      cottontail::Working::mkdir(burrow, &error);
  ASSERT_NE(working, nullptr) << error;

  std::string first = shard_name("hazel", 0, 0);
  std::string second = shard_name("hazel", 1, 1);
  std::string target = shard_name("hazel", 0, 1);
  touch(working, first);
  touch(working, second);
  touch(working, "mrg." + target);
  touch(working, "pst." + target);
  touch(working, "dct." + target);
  std::string segment = merge_segment_name(0, 0, 1);
  touch(working, segment);

  std::vector<cottontail::OwslaShard> hazels;
  std::vector<cottontail::HazelMergeRecovery> recoveries;
  ASSERT_TRUE(
      cottontail::Hazel::sanitize(working, &hazels, &recoveries, &error))
      << error;
  ASSERT_EQ(hazels.size(), size_t(2));
  ASSERT_EQ(recoveries.size(), size_t(1));
  EXPECT_EQ(recoveries[0].segment_count, size_t(1));
  EXPECT_TRUE(working->ls("mrg").empty());
  EXPECT_TRUE(working->ls("pst").empty());
  EXPECT_TRUE(working->ls("dct").empty());
  ASSERT_TRUE(
      cottontail::remove_hazel_merge_segments(working, recoveries[0], &error))
      << error;
  EXPECT_TRUE(working->ls("merge").empty());
  ASSERT_TRUE(
      cottontail::remove_hazel_merge_segments(working, recoveries[0], &error))
      << error;

  touch(working, "pst." + target);
  ASSERT_TRUE(
      cottontail::Hazel::sanitize(working, &hazels, &recoveries, &error))
      << error;
  EXPECT_TRUE(recoveries.empty());
  EXPECT_TRUE(working->ls("pst").empty());
}

TEST(HazelMergeRecovery, IncompleteSegmentGroupIsDiscarded) {
  std::string burrow = test_root() + "/hazel_recovery_conflicts.burrow";
  std::string error;
  std::shared_ptr<cottontail::Working> working =
      cottontail::Working::mkdir(burrow, &error);
  ASSERT_NE(working, nullptr) << error;

  touch(working, shard_name("hazel", 0, 0));
  touch(working, shard_name("hazel", 1, 1));
  touch(working, shard_name("hazel", 2, 2));
  touch(working, merge_segment_name(1, 0, 2));

  std::vector<cottontail::OwslaShard> hazels;
  std::vector<cottontail::HazelMergeRecovery> recoveries;
  ASSERT_TRUE(
      cottontail::Hazel::sanitize(working, &hazels, &recoveries, &error))
      << error;
  EXPECT_EQ(hazels.size(), size_t(3));
  EXPECT_TRUE(recoveries.empty());
  EXPECT_TRUE(working->ls("merge").empty());
}

TEST(HazelMergeRecovery, AbortedTransactionLeavesMergeableGap) {
  std::string burrow = test_root() + "/hazel_aborted_gap.burrow";
  CompressorProfile compressors =
      compressor_profile("aborted_gap", "null", "null", "null");
  std::shared_ptr<cottontail::Bigwig> bigwig = make_bigwig(burrow, compressors);
  ASSERT_NE(bigwig, nullptr);

  append_transaction(bigwig, "alpha committed", true);
  append_transaction(bigwig, "discarded transaction", false);
  append_transaction(bigwig, "omega committed", true);

  std::shared_ptr<cottontail::Working> working =
      cottontail::Working::make(burrow);
  ASSERT_NE(working, nullptr);
  std::vector<ShardName> fivers = fiver_shards(working);
  ASSERT_EQ(fivers.size(), size_t(2));
  EXPECT_EQ(fivers[0].start, 0);
  EXPECT_EQ(fivers[0].end, 0);
  EXPECT_EQ(fivers[1].start, 2);
  EXPECT_EQ(fivers[1].end, 2);

  std::vector<std::shared_ptr<cottontail::Hazel>> hazels;
  for (auto &fiver : fivers) {
    auto hazel = convert_fiver(bigwig, working, fiver, compressors);
    ASSERT_NE(hazel, nullptr);
    hazels.push_back(hazel);
  }
  touch(working, merge_segment_name(0, 0, 2));
  hazels.clear();
  bigwig.reset();

  std::string error;
  std::shared_ptr<cottontail::Bigwig> reopened =
      cottontail::Bigwig::make(burrow, &error);
  ASSERT_NE(reopened, nullptr) << error;
  EXPECT_EQ(working->ls("merge").size(), size_t(1));
  reopened.reset();

  ASSERT_TRUE(cottontail::Bigwig::consolidate(burrow, &error)) << error;
  EXPECT_TRUE(working->ls("merge").empty());
  EXPECT_TRUE(working->ls("fiver").empty());
  EXPECT_EQ(working->ls("hazel").size(), size_t(1));
  std::shared_ptr<cottontail::Warren> merged = open_started(burrow);
  ASSERT_NE(merged, nullptr);
  EXPECT_GT(merged->idx()->count(merged->featurizer()->featurize("alpha")), 0);
  EXPECT_GT(merged->idx()->count(merged->featurizer()->featurize("omega")), 0);
  EXPECT_EQ(merged->idx()->count(merged->featurizer()->featurize("discarded")),
            0);
  merged->end();
}

TEST(HazelMergeRecovery, FiverInGapDiscardsPartialMerge) {
  std::string burrow = test_root() + "/hazel_fiver_gap.burrow";
  CompressorProfile compressors =
      compressor_profile("fiver_gap", "null", "null", "null");
  std::shared_ptr<cottontail::Bigwig> bigwig = make_bigwig(burrow, compressors);
  ASSERT_NE(bigwig, nullptr);

  append_transaction(bigwig, "alpha", true);
  append_transaction(bigwig, "middle", true);
  append_transaction(bigwig, "omega", true);

  std::shared_ptr<cottontail::Working> working =
      cottontail::Working::make(burrow);
  ASSERT_NE(working, nullptr);
  std::vector<ShardName> fivers = fiver_shards(working);
  ASSERT_EQ(fivers.size(), size_t(3));
  std::shared_ptr<cottontail::Hazel> first =
      convert_fiver(bigwig, working, fivers.front(), compressors);
  std::shared_ptr<cottontail::Hazel> last =
      convert_fiver(bigwig, working, fivers.back(), compressors);
  ASSERT_NE(first, nullptr);
  ASSERT_NE(last, nullptr);
  touch(working, merge_segment_name(0, 0, 2));
  first.reset();
  last.reset();
  bigwig.reset();

  std::string error;
  std::shared_ptr<cottontail::Bigwig> reopened =
      cottontail::Bigwig::make(burrow, &error);
  ASSERT_NE(reopened, nullptr) << error;
  EXPECT_TRUE(working->ls("merge").empty());
}

TEST(HazelMergeRecovery, ConflictingPartialMergesAreDiscarded) {
  std::string burrow = test_root() + "/hazel_conflicting_merges.burrow";
  CompressorProfile compressors =
      compressor_profile("conflicts", "null", "null", "null");
  std::shared_ptr<cottontail::Bigwig> bigwig = make_bigwig(burrow, compressors);
  ASSERT_NE(bigwig, nullptr);

  append_transaction(bigwig, "alpha", true);
  append_transaction(bigwig, "middle", true);
  append_transaction(bigwig, "omega", true);

  std::shared_ptr<cottontail::Working> working =
      cottontail::Working::make(burrow);
  ASSERT_NE(working, nullptr);
  std::vector<ShardName> fivers = fiver_shards(working);
  ASSERT_EQ(fivers.size(), size_t(3));
  std::vector<std::shared_ptr<cottontail::Hazel>> hazels;
  for (auto &fiver : fivers) {
    auto hazel = convert_fiver(bigwig, working, fiver, compressors);
    ASSERT_NE(hazel, nullptr);
    hazels.push_back(hazel);
  }
  touch(working, merge_segment_name(0, 0, 1));
  touch(working, merge_segment_name(0, 1, 2));
  hazels.clear();
  bigwig.reset();

  std::string error;
  std::shared_ptr<cottontail::Bigwig> reopened =
      cottontail::Bigwig::make(burrow, &error);
  ASSERT_NE(reopened, nullptr) << error;
  EXPECT_TRUE(working->ls("merge").empty());
}

TEST(BigwigHazelActivation, PreservesHazelPrefixFiverSuffix) {
  run_bigwig_hazel_activation_regression(
      compressor_profile("real", "post", "zlib", "zlib"));
}

TEST(HazelMerge, PreservesBigwigBehaviorSmallChunks) {
  run_hazel_merge_regression(
      16, compressor_profile("null", "null", "null", "null"), true);
}

TEST(HazelMerge, PreservesBigwigBehaviorWithRealCompressorsSmallChunks) {
  run_hazel_merge_regression(
      16, compressor_profile("real", "post", "zlib", "zlib"));
}

TEST(HazelMerge, PreservesBigwigBehaviorWithBadCompressorsSmallChunks) {
  run_hazel_merge_regression(16,
                             compressor_profile("bad", "bad", "bad", "bad"));
}

TEST(HazelMerge, PreservesBigwigBehaviorWithRealCompressorsDefaultChunks) {
  run_hazel_merge_regression(
      64 * 1024, compressor_profile("real_default", "post", "zlib", "zlib"));
}
