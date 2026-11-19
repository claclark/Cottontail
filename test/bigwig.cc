#include "src/bigwig.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "gtest/gtest.h"

#include "src/cottontail.h"
#include "src/dna.h"

TEST(Bigwig, LockedFactories) {
  char directory[] = "/tmp/cottontail-bigwig-lock-XXXXXX";
  ASSERT_NE(mkdtemp(directory), nullptr);
  std::string burrow = directory;
  std::string error;
  EXPECT_EQ(cottontail::Bigwig::make(burrow, "tokenizer:name:unknown", &error),
            nullptr);
  EXPECT_NE(error.find("No Tokenizer named"), std::string::npos);
  auto bigwig =
      cottontail::Bigwig::make(burrow, "tokenizer:name:ascii", &error);
  ASSERT_NE(bigwig, nullptr) << error;
  EXPECT_EQ(bigwig->tokenizer()->name(), "ascii");

  auto clone = bigwig->clone(&error);
  ASSERT_NE(clone, nullptr) << error;
  bigwig.reset();
  EXPECT_EQ(cottontail::Bigwig::make(burrow, &error), nullptr);
  EXPECT_NE(error.find("Cottontail burrow locked"), std::string::npos);
  EXPECT_FALSE(cottontail::Bigwig::consolidate(burrow, &error));
  EXPECT_NE(error.find("Cottontail burrow locked"), std::string::npos);
  clone.reset();

  bigwig = cottontail::Bigwig::make(burrow, &error);
  ASSERT_NE(bigwig, nullptr) << error;
  EXPECT_EQ(bigwig->tokenizer()->name(), "ascii");
  EXPECT_EQ(cottontail::Bigwig::make(burrow, "tokenizer:name:utf8", &error),
            nullptr);
  bigwig.reset();

  // A pre-WLock burrow acquires its lock files on first activation.
  for (const char *name : {"burrow.pid", "burrow.lock"})
    ASSERT_EQ(unlink((burrow + "/" + name).c_str()), 0);
  bigwig = cottontail::Bigwig::make(burrow, &error);
  ASSERT_NE(bigwig, nullptr) << error;
  EXPECT_EQ(bigwig->tokenizer()->name(), "ascii");
  EXPECT_EQ(cottontail::Bigwig::make(burrow, &error), nullptr);
  EXPECT_NE(error.find("Cottontail burrow locked"), std::string::npos);
  bigwig.reset();
  for (const char *name : {"dna", "burrow.pid", "burrow.lock"})
    ASSERT_EQ(unlink((burrow + "/" + name).c_str()), 0);
  EXPECT_EQ(rmdir(directory), 0);
}

TEST(Bigwig, WarrenFactories) {
  char directory[] = "/tmp/cottontail-warren-factory-XXXXXX";
  ASSERT_NE(mkdtemp(directory), nullptr);
  std::string burrow = directory;
  std::string error;
  std::string keep = burrow + "/keep.txt";
  {
    std::ofstream out(keep);
    out << "keep\n";
    ASSERT_TRUE(out.good());
  }
  EXPECT_EQ(cottontail::Warren::make("bigwig", "", burrow, &error, true),
            nullptr);
  ASSERT_EQ(unlink(keep.c_str()), 0);

  auto warren = cottontail::Warren::make("bigwig", "tokenizer:name:ascii",
                                         burrow, &error);
  ASSERT_NE(warren, nullptr) << error;
  ASSERT_TRUE(warren->set_default_container("line:", &error)) << error;
  EXPECT_EQ(cottontail::Warren::make(burrow, &error), nullptr);
  EXPECT_NE(error.find("Cottontail burrow locked"), std::string::npos);
  EXPECT_EQ(cottontail::Warren::make("bigwig", "", burrow, &error, true),
            nullptr);
  warren.reset();

  EXPECT_EQ(cottontail::Warren::make("bigwig", "", burrow, &error), nullptr);
  EXPECT_EQ(cottontail::Warren::make("bigwig", burrow, &error), nullptr);
  EXPECT_EQ(cottontail::Warren::make("simple", burrow, &error), nullptr);
  EXPECT_EQ(cottontail::Warren::make("bigwig", "tokenizer:name:unknown",
                                     burrow, &error, true),
            nullptr);

  std::string blocked = burrow + "/blocked";
  {
    std::ofstream out(blocked);
    ASSERT_TRUE(out.good());
  }
  ASSERT_EQ(chmod(blocked.c_str(), 0444), 0);
  EXPECT_EQ(cottontail::Warren::make("bigwig", "", burrow, &error, true),
            nullptr);
  EXPECT_NE(error.find("Burrow contains read-only files"), std::string::npos);
  ASSERT_EQ(unlink(blocked.c_str()), 0);
  ASSERT_EQ(mkdir(blocked.c_str(), 0700), 0);
  EXPECT_EQ(cottontail::Warren::make("bigwig", "", burrow, &error, true),
            nullptr);
  EXPECT_NE(error.find("Burrow contains subdirectories"), std::string::npos);
  ASSERT_EQ(rmdir(blocked.c_str()), 0);
  ASSERT_EQ(symlink("dna", blocked.c_str()), 0);
  EXPECT_EQ(cottontail::Warren::make("bigwig", "", burrow, &error, true),
            nullptr);
  EXPECT_NE(error.find("Burrow contains non-regular files"), std::string::npos);
  ASSERT_EQ(unlink(blocked.c_str()), 0);

  std::string pid_file = burrow + "/burrow.pid";
  ASSERT_EQ(unlink(pid_file.c_str()), 0);
  ASSERT_EQ(symlink("dna", pid_file.c_str()), 0);
  EXPECT_EQ(cottontail::Warren::make(burrow, &error), nullptr);
  EXPECT_NE(error.find("Burrow PID file is not a regular file"),
            std::string::npos);
  ASSERT_EQ(unlink(pid_file.c_str()), 0);

  warren = cottontail::Warren::make(burrow, &error);
  ASSERT_NE(warren, nullptr) << error;
  EXPECT_EQ(warren->tokenizer()->name(), "ascii");
  warren->start();
  EXPECT_EQ(warren->default_container(), "line:");
  warren->end();
  warren.reset();

  warren = cottontail::Warren::make("bigwig", "", burrow, &error, true);
  ASSERT_NE(warren, nullptr) << error;
  EXPECT_EQ(warren->tokenizer()->name(), "utf8");
  warren->start();
  EXPECT_TRUE(warren->default_container().empty());
  warren->end();
  warren.reset();
  for (const char *name : {"dna", "burrow.pid", "burrow.lock"})
    ASSERT_EQ(unlink((burrow + "/" + name).c_str()), 0);
  EXPECT_EQ(rmdir(directory), 0);
}

TEST(Bigwig, DnaPublishFailure) {
  char directory[] = "/tmp/cottontail-dna-XXXXXX";
  ASSERT_NE(mkdtemp(directory), nullptr);
  auto working = cottontail::Working::make(directory);
  std::string destination = working->make_name(cottontail::DNA_NAME);
  ASSERT_EQ(mkdir(destination.c_str(), 0700), 0);
  std::string error;
  EXPECT_FALSE(cottontail::write_dna(working, "[]", &error));
  EXPECT_NE(error.find("Can't publish configuration"), std::string::npos);
  EXPECT_TRUE(working->ls("temp").empty());
  EXPECT_EQ(rmdir(destination.c_str()), 0);
  EXPECT_EQ(rmdir(directory), 0);
}

void basic(bool merge, std::shared_ptr<cottontail::Bigwig> bigwig) {
  const char *hamlet[] = {"To be, or not to be, that is the question:",
                          "Whether 'tis nobler in the mind to suffer",
                          "The slings and arrows of outrageous fortune,",
                          "Or to take arms against a sea of troubles",
                          "And by opposing end them. To die-to sleep,",
                          "No more; and by a sleep to say we end",
                          "The heart-ache and the thousand natural shocks",
                          "That flesh is heir to: 'tis a consummation",
                          "Devoutly to be wish'd. To die, to sleep;",
                          "To sleep, perchance to dream-ay, there's the rub:",
                          "For in that sleep of death what dreams may come,"};
  ASSERT_NE(bigwig, nullptr);
  cottontail::addr p, q, v;
  std::unique_ptr<cottontail::Hopper> hopper;
  cottontail::addr line = bigwig->featurizer()->featurize("line:");
  bigwig->merge(merge);
  ASSERT_TRUE(bigwig->transaction());
  ASSERT_TRUE(bigwig->appender()->append(std::string(hamlet[0]), &p, &q));
  ASSERT_TRUE(bigwig->annotator()->annotate(line, p, q, (cottontail::addr)1));
  ASSERT_TRUE(bigwig->appender()->append(std::string(hamlet[1]), &p, &q));
  ASSERT_TRUE(bigwig->annotator()->annotate(line, p, q, (cottontail::addr)2));
  ASSERT_TRUE(bigwig->appender()->append(std::string(hamlet[2]), &p, &q));
  ASSERT_TRUE(bigwig->annotator()->annotate(line, p, q, (cottontail::addr)3));
  ASSERT_TRUE(bigwig->appender()->append(std::string(hamlet[3]), &p, &q));
  ASSERT_TRUE(bigwig->annotator()->annotate(line, p, q, (cottontail::addr)4));
  ASSERT_TRUE(bigwig->ready());
  bigwig->commit();
  bigwig->start();
  hopper = bigwig->hopper_from_gcl("\"to be\"");
  ASSERT_NE(hopper, nullptr);
  hopper->tau(cottontail::minfinity + 1, &p, &q);
  EXPECT_EQ(bigwig->txt()->translate(p, q), "To be, ");
  hopper->uat(cottontail::maxfinity - 1, &p, &q);
  EXPECT_EQ(bigwig->txt()->translate(p, q), "to be, ");
  ASSERT_TRUE(bigwig->transaction());
  ASSERT_TRUE(bigwig->appender()->append(std::string(hamlet[4]), &p, &q));
  ASSERT_TRUE(bigwig->annotator()->annotate(line, p, q, (cottontail::addr)5));
  ASSERT_TRUE(bigwig->appender()->append(std::string(hamlet[5]), &p, &q));
  ASSERT_TRUE(bigwig->annotator()->annotate(line, p, q, (cottontail::addr)6));
  hopper = bigwig->hopper_from_gcl("sleep");
  ASSERT_NE(hopper, nullptr);
  hopper->tau(cottontail::minfinity + 1, &p, &q);
  EXPECT_EQ(p, cottontail::maxfinity);
  ASSERT_TRUE(bigwig->appender()->append(std::string(hamlet[6]), &p, &q));
  ASSERT_TRUE(bigwig->annotator()->annotate(line, p, q, (cottontail::addr)7));
  ASSERT_TRUE(bigwig->appender()->append(std::string(hamlet[7]), &p, &q));
  ASSERT_TRUE(bigwig->annotator()->annotate(line, p, q, (cottontail::addr)8));
  ASSERT_TRUE(bigwig->ready());
  bigwig->commit();
  hopper = bigwig->hopper_from_gcl("to");
  ASSERT_NE(hopper, nullptr);
  cottontail::addr i = 0;
  for (hopper->tau(0, &p, &q); p < cottontail::maxfinity;
       hopper->tau(p + 1, &p, &q)) {
    EXPECT_EQ(p, q);
    std::string to = bigwig->txt()->translate(p, q).substr(0, 2);
    EXPECT_TRUE(to == "To" || to == "to");
    i++;
  }
  EXPECT_EQ(i, 4);
  bigwig->end();
  i = 0;
  for (hopper->tau(0, &p, &q); p < cottontail::maxfinity;
       hopper->tau(p + 1, &p, &q))
    i++;
  EXPECT_EQ(i, 4);
  bigwig->start();
  hopper = bigwig->hopper_from_gcl("to");
  ASSERT_NE(hopper, nullptr);
  i = 0;
  for (hopper->tau(0, &p, &q); p < cottontail::maxfinity;
       hopper->tau(p + 1, &p, &q)) {
    EXPECT_EQ(p, q);
    std::string to = bigwig->txt()->translate(p, q).substr(0, 2);
    EXPECT_TRUE(to == "To" || to == "to");
    i++;
  }
  EXPECT_EQ(i, 8);
  bigwig->end();
  ASSERT_TRUE(bigwig->transaction());
  ASSERT_TRUE(bigwig->appender()->append("spam spam spam", &p, &q));
  bigwig->abort();
  bigwig->start();
  hopper = bigwig->hopper_from_gcl("spam");
  ASSERT_NE(hopper, nullptr);
  hopper->tau(0, &p, &q);
  ASSERT_EQ(p, cottontail::maxfinity);
  bigwig->end();
  ASSERT_TRUE(bigwig->transaction());
  ASSERT_TRUE(bigwig->appender()->append(std::string(hamlet[8]), &p, &q));
  ASSERT_TRUE(bigwig->annotator()->annotate(line, p, q, (cottontail::addr)9));
  ASSERT_TRUE(bigwig->appender()->append(std::string(hamlet[9]), &p, &q));
  ASSERT_TRUE(bigwig->annotator()->annotate(line, p, q, (cottontail::addr)10));
  ASSERT_TRUE(bigwig->appender()->append(std::string(hamlet[10]), &p, &q));
  ASSERT_TRUE(bigwig->annotator()->annotate(line, p, q, (cottontail::addr)11));
  ASSERT_TRUE(bigwig->ready());
  bigwig->commit();
  bigwig->start();
  hopper = bigwig->hopper_from_gcl("to");
  ASSERT_NE(hopper, nullptr);
  i = 0;
  for (hopper->tau(0, &p, &q); p < cottontail::maxfinity;
       hopper->tau(p + 1, &p, &q)) {
    EXPECT_EQ(p, q);
    std::string to = bigwig->txt()->translate(p, q).substr(0, 2);
    EXPECT_TRUE(to == "To" || to == "to");
    i++;
  }
  EXPECT_EQ(i, 13);
  hopper = bigwig->hopper_from_gcl("\"troubles and by\"");
  ASSERT_NE(hopper, nullptr);
  hopper->tau(0, &p, &q);
  EXPECT_EQ(bigwig->txt()->translate(p, q), "troubles\nAnd by ");
  hopper->tau(p + 1, &p, &q);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  hopper = bigwig->hopper_from_gcl("\"to sleep to sleep\"");
  ASSERT_NE(hopper, nullptr);
  hopper->tau(0, &p, &q);
  EXPECT_EQ(bigwig->txt()->translate(p, q), "to sleep;\nTo sleep, ");
  hopper->tau(p + 1, &p, &q);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  hopper = bigwig->hopper_from_gcl("\"to sleep\"");
  ASSERT_NE(hopper, nullptr);
  hopper->tau(0, &p, &q);
  EXPECT_EQ(bigwig->txt()->translate(p, q), "to sleep,\n");
  hopper->tau(p + 1, &p, &q);
  EXPECT_EQ(bigwig->txt()->translate(p, q), "to sleep;\n");
  hopper->tau(p + 1, &p, &q);
  EXPECT_EQ(bigwig->txt()->translate(p, q), "To sleep, ");
  hopper->tau(p + 1, &p, &q);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  hopper = bigwig->idx()->hopper(line);
  i = 0;
  ASSERT_NE(hopper, nullptr);
  for (hopper->tau(0, &p, &q, &v); p < cottontail::maxfinity;
       hopper->tau(p + 1, &p, &q, &v)) {
    std::string a = bigwig->txt()->translate(p, q);
    std::string b = std::string(hamlet[i]) + "\n";
    EXPECT_EQ(a, b);
    i++;
    EXPECT_EQ(i, v);
  }
  EXPECT_EQ(i, 11);
}

namespace {

cottontail::addr count_gcl(std::shared_ptr<cottontail::Warren> warren,
                           const std::string &gcl) {
  std::string error;
  std::unique_ptr<cottontail::Hopper> hopper =
      warren->hopper_from_gcl(gcl, &error);
  EXPECT_EQ(error, "") << error;
  EXPECT_NE(hopper, nullptr);
  if (hopper == nullptr)
    return -1;
  cottontail::addr p, q;
  cottontail::addr count = 0;
  for (hopper->tau(0, &p, &q); p < cottontail::maxfinity;
       hopper->tau(p + 1, &p, &q))
    count++;
  return count;
}

} // namespace

TEST(Bigwig, Consolidate) {
  std::string burrow_name = "bigwig.consolidate.burrow";
  ASSERT_NO_FATAL_FAILURE(basic(
      false, cottontail::Bigwig::make(burrow_name, "tokenizer:name:ascii")));
  std::shared_ptr<cottontail::Working> working =
      cottontail::Working::make(burrow_name);
  ASSERT_NE(working, nullptr);
  ASSERT_GT(working->ls("fiver").size(), size_t(1));

  std::string error;
  ASSERT_TRUE(cottontail::Bigwig::consolidate(burrow_name, &error)) << error;
  EXPECT_TRUE(working->ls("fiver").empty());
  ASSERT_EQ(working->ls("hazel").size(), size_t(1));

  std::shared_ptr<cottontail::Bigwig> bigwig =
      cottontail::Bigwig::make(burrow_name, &error);
  ASSERT_NE(bigwig, nullptr) << error;
  bigwig->start();
  EXPECT_EQ(count_gcl(bigwig, "line:"), 11);
  EXPECT_EQ(count_gcl(bigwig, "sleep"), 5);
  bigwig->end();
  bigwig.reset();

  ASSERT_TRUE(cottontail::Bigwig::consolidate(burrow_name, &error)) << error;
  EXPECT_TRUE(working->ls("fiver").empty());
  EXPECT_EQ(working->ls("hazel").size(), size_t(1));
}

TEST(Bigwig, Basic) {
  std::string error;
  EXPECT_EQ(cottontail::Bigwig::make_in_memory("tokenizer", &error), nullptr);
  for (bool merge : {false, true}) {
    auto bigwig =
        cottontail::Bigwig::make_in_memory("tokenizer:name:ascii", &error);
    ASSERT_NE(bigwig, nullptr) << error;
    EXPECT_EQ(bigwig->working(), nullptr);
    EXPECT_EQ(bigwig->tokenizer()->name(), "ascii");
    ASSERT_NO_FATAL_FAILURE(basic(merge, bigwig));
  }
}

TEST(Bigwig, MergePolicyParameters) {
  std::shared_ptr<cottontail::Bigwig> bigwig =
      cottontail::Bigwig::make_in_memory("tokenizer:name:ascii");
  ASSERT_NE(bigwig, nullptr);

  std::string value;
  ASSERT_TRUE(bigwig->get_parameter("merge", &value));
  EXPECT_EQ(value, "");
  ASSERT_TRUE(bigwig->get_parameter("convert", &value));
  EXPECT_EQ(value, "");

  bigwig->merge(false, false);
  ASSERT_TRUE(bigwig->get_parameter("merge", &value));
  EXPECT_EQ(value, "no");
  ASSERT_TRUE(bigwig->get_parameter("convert", &value));
  EXPECT_EQ(value, "");

  bigwig->merge(true, false);
  ASSERT_TRUE(bigwig->get_parameter("merge", &value));
  EXPECT_EQ(value, "yes");
  ASSERT_TRUE(bigwig->get_parameter("convert", &value));
  EXPECT_EQ(value, "no");

  bigwig->merge();
  ASSERT_TRUE(bigwig->get_parameter("merge", &value));
  EXPECT_EQ(value, "yes");
  ASSERT_TRUE(bigwig->get_parameter("convert", &value));
  EXPECT_EQ(value, "yes");
}

TEST(Bigwig, Two) {
  cottontail::addr p, q;
  std::unique_ptr<cottontail::Hopper> hopper;
  std::shared_ptr<cottontail::Bigwig> big =
      cottontail::Bigwig::make_in_memory("tokenizer:name:ascii");
  ASSERT_NE(big, nullptr);
  big->merge(false);
  std::shared_ptr<cottontail::Warren> wig = big->clone();
  ASSERT_NE(wig, nullptr);
  big->start();
  wig->start();
  EXPECT_EQ(big->txt()->translate(0, 100), "");
  EXPECT_EQ(wig->txt()->translate(0, 100), "");
  hopper = big->idx()->hopper(0);
  ASSERT_NE(hopper, nullptr);
  hopper->tau(0, &p, &q);
  EXPECT_EQ(p, cottontail::maxfinity);
  hopper = wig->idx()->hopper(0);
  ASSERT_NE(hopper, nullptr);
  hopper->tau(0, &p, &q);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_TRUE(big->transaction());
  EXPECT_TRUE(wig->transaction());
  EXPECT_TRUE(big->appender()->append("a aa aaa aaaa", &p, &q));
  EXPECT_TRUE(wig->appender()->append("1 11 111 1111", &p, &q));
  EXPECT_TRUE(big->appender()->append("b bb bbb bbbb", &p, &q));
  EXPECT_TRUE(wig->appender()->append("2 22 222 2222", &p, &q));
  ASSERT_TRUE(big->ready());
  ASSERT_TRUE(wig->ready());
  big->commit();
  wig->commit();
  EXPECT_EQ(big->txt()->translate(0, 100), "");
  EXPECT_EQ(wig->txt()->translate(0, 100), "");
  big->end();
  wig->end();
  big->start();
  wig->start();
  EXPECT_EQ(big->txt()->translate(6, 9), "bbb bbbb\n1 11 ");
  EXPECT_EQ(wig->txt()->translate(6, 9), "bbb bbbb\n1 11 ");
  big->end();
  wig->end();
  EXPECT_TRUE(wig->transaction());
  EXPECT_TRUE(wig->appender()->append("spam spam spam spam", &p, &q));
  ASSERT_TRUE(wig->ready());
  wig->abort();
  EXPECT_TRUE(wig->transaction());
  EXPECT_TRUE(big->transaction());
  EXPECT_TRUE(wig->appender()->append("1 22 333 4444", &p, &q));
  EXPECT_TRUE(big->appender()->append("a bb ccc dddd", &p, &q));
  EXPECT_TRUE(wig->appender()->append("55555 666666", &p, &q));
  EXPECT_TRUE(big->appender()->append("eeeee ffffff ggggggg", &p, &q));
  ASSERT_TRUE(wig->ready());
  ASSERT_TRUE(big->ready());
  wig->commit();
  big->commit();
  big->start();
  wig->start();
  EXPECT_EQ(big->txt()->translate(15, 20), "2222\n1 ");
  EXPECT_EQ(wig->txt()->translate(15, 20), "2222\n1 ");
  hopper = big->hopper_from_gcl("a");
  ASSERT_NE(hopper, nullptr);
  hopper->tau(0, &p, &q);
  EXPECT_EQ(p, 0);
  EXPECT_EQ(q, 0);
  hopper->tau(p + 1, &p, &q);
  EXPECT_EQ(p, 26);
  EXPECT_EQ(q, 26);
  hopper->tau(p + 1, &p, &q);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  hopper = wig->hopper_from_gcl("bb");
  ASSERT_NE(hopper, nullptr);
  hopper->tau(0, &p, &q);
  EXPECT_EQ(p, 5);
  EXPECT_EQ(q, 5);
  hopper->tau(p + 1, &p, &q);
  EXPECT_EQ(p, 27);
  EXPECT_EQ(q, 27);
  hopper->tau(p + 1, &p, &q);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  hopper = wig->hopper_from_gcl("22");
  ASSERT_NE(hopper, nullptr);
  hopper->tau(0, &p, &q);
  EXPECT_EQ(p, 13);
  EXPECT_EQ(q, 13);
  hopper->tau(p + 1, &p, &q);
  EXPECT_EQ(p, 21);
  EXPECT_EQ(q, 21);
  hopper->tau(p + 1, &p, &q);
  EXPECT_EQ(p, cottontail::maxfinity);
  EXPECT_EQ(q, cottontail::maxfinity);
  wig->end();
  big->end();
}

TEST(Bigwig, StartedClonePreservesReadView) {
  std::shared_ptr<cottontail::Bigwig> bigwig =
      cottontail::Bigwig::make_in_memory("tokenizer:name:ascii");
  ASSERT_NE(bigwig, nullptr);

  cottontail::addr p, q;
  ASSERT_TRUE(bigwig->transaction());
  ASSERT_TRUE(bigwig->appender()->append("alpha beta", &p, &q));
  ASSERT_TRUE(bigwig->ready());
  bigwig->commit();

  bigwig->start();
  EXPECT_EQ(count_gcl(bigwig, "alpha"), 1);
  std::string error;
  std::shared_ptr<cottontail::Warren> clone = bigwig->clone(&error);
  ASSERT_NE(clone, nullptr) << error;
  ASSERT_TRUE(clone->started());
  EXPECT_EQ(count_gcl(clone, "alpha"), 1);

  bigwig->end();
  ASSERT_TRUE(clone->started());
  EXPECT_EQ(count_gcl(clone, "alpha"), 1);

  ASSERT_TRUE(bigwig->transaction());
  ASSERT_TRUE(bigwig->appender()->append("alpha gamma", &p, &q));
  ASSERT_TRUE(bigwig->ready());
  bigwig->commit();
  EXPECT_EQ(count_gcl(clone, "alpha"), 1);

  clone->end();
  bigwig->start();
  EXPECT_EQ(count_gcl(bigwig, "alpha"), 2);
  bigwig->end();
}

void tiger(cottontail::addr n) {
  std::shared_ptr<cottontail::Bigwig> bigwig =
      cottontail::Bigwig::make_in_memory("tokenizer:name:ascii");
  ASSERT_NE(bigwig, nullptr);
  cottontail::addr line = bigwig->featurizer()->featurize("line:");
  auto worker = [&](std::string text) {
    std::shared_ptr<cottontail::Warren> warren = bigwig->clone();
    ASSERT_NE(warren, nullptr);
    std::static_pointer_cast<cottontail::Bigwig>(warren)->merge(true);
    for (cottontail::addr i = 0; i < n; i++) {
      cottontail::addr p, q;
      ASSERT_TRUE(warren->transaction());
      ASSERT_TRUE(warren->appender()->append(text, &p, &q));
      ASSERT_TRUE(warren->annotator()->annotate(line, p, q, i));
      ASSERT_TRUE(warren->ready());
      warren->commit();
    }
  };
  const char *diangu[] = {
      "A little effort, a little harvest",
      "Think thrice before you act",
      "No entry into the tiger's den, no tiger cub",
      "Diligence in life, no need for rewards",
      "An inch of time is an inch of gold",
      "People are in a good mood when they encounter good news",
      "The tiger's den cannot hide it, the tiger's power cannot be blocked",
      "Dancing with the shadows, just like in the human world"};
  cottontail::addr m = sizeof(diangu) / sizeof(char *);
  std::vector<std::thread> workers;
  for (cottontail::addr i = 0; i < m; i++)
    workers.emplace_back(std::thread(worker, std::string(diangu[i])));
  for (auto &worker : workers)
    worker.join();
  bigwig->start();
  std::unique_ptr<cottontail::Hopper> hopper = bigwig->idx()->hopper(line);
  ASSERT_NE(hopper, nullptr);
  cottontail::addr histogram[n];
  for (cottontail::addr i = 0; i < n; i++)
    histogram[i] = 0;
  cottontail::addr p, q, v;
  for (hopper->tau(0, &p, &q, &v); p < cottontail::maxfinity;
       hopper->tau(p + 1, &p, &q, &v))
    histogram[v]++;
  for (cottontail::addr i = 0; i < n; i++)
    EXPECT_EQ(histogram[i], m);
  bigwig->end();
  bigwig->start();
  for (cottontail::addr i = 0; i < m; i++) {
    std::string saying = diangu[i];
    std::string gcl = "\"" + saying + "\"";
    hopper = bigwig->hopper_from_gcl(gcl);
    cottontail::addr j = 0;
    for (hopper->tau(0, &p, &q); p < cottontail::maxfinity;
         hopper->tau(p + 1, &p, &q)) {
      EXPECT_EQ(bigwig->txt()->translate(p, q), saying + "\n");
      j++;
    }
    EXPECT_EQ(j, n);
  }
  hopper = bigwig->hopper_from_gcl("tiger");
  cottontail::addr i = 0;
  for (hopper->tau(0, &p, &q); p < cottontail::maxfinity;
       hopper->tau(p + 1, &p, &q)) {
    std::string tiger = bigwig->txt()->translate(p, q).substr(0, 5);
    EXPECT_EQ(tiger, "tiger");
    i++;
  }
  EXPECT_EQ(i, 4 * n);
  bigwig->end();
}

TEST(Bigwig, Tiger) {
  tiger(1);
  tiger(3);
  tiger(11);
  tiger(111);
}

void love(cottontail::addr n) {
  std::shared_ptr<cottontail::Bigwig> bigwig =
      cottontail::Bigwig::make_in_memory("tokenizer:name:ascii");
  ASSERT_NE(bigwig, nullptr);
  cottontail::addr line = bigwig->featurizer()->featurize("line:");
  auto append_worker = [&](std::string text) {
    std::shared_ptr<cottontail::Warren> warren = bigwig->clone();
    ASSERT_NE(warren, nullptr);
    std::static_pointer_cast<cottontail::Bigwig>(warren)->merge(true);
    for (cottontail::addr i = 0; i < n; i++) {
      cottontail::addr p, q;
      ASSERT_TRUE(warren->transaction());
      ASSERT_TRUE(warren->appender()->append(text, &p, &q));
      ASSERT_TRUE(warren->annotator()->annotate(line, p, q, i));
      ASSERT_TRUE(warren->ready());
      warren->commit();
    }
  };
  auto check_worker = [&](std::string text, cottontail::addr total) {
    std::shared_ptr<cottontail::Warren> warren = bigwig->clone();
    ASSERT_NE(warren, nullptr);
    cottontail::addr count = 0;
    while (count < total) {
      warren->start();
      std::unique_ptr<cottontail::Hopper> hopper =
          warren->hopper_from_gcl(text);
      ASSERT_NE(hopper, nullptr);
      cottontail::addr current = 0;
      cottontail::addr p, q;
      for (hopper->tau(0, &p, &q); p < cottontail::maxfinity;
           hopper->tau(p + 1, &p, &q)) {
        std::string t = warren->txt()->translate(p, q);
        EXPECT_GE(t.length(), text.length());
        EXPECT_EQ(t.substr(0, text.length()), text);
        current++;
      }
      warren->end();
      EXPECT_GE(current, count);
      count = current;
    }
  };
  const char *love[] = {"Baby, baby, don't you know that I need you",
                        "Coding all night, dreaming in code",
                        "Error 404: love not found",
                        "All you need is love, love is all you need",
                        "C++ is my love language",
                        "I came in like a wrecking ball",
                        "Never gonna give you up, never gonna let you down",
                        "May the force be with you",
                        "Hello from the other side"};
  cottontail::addr m = sizeof(love) / sizeof(char *);
  std::vector<std::thread> workers;
  workers.emplace_back(std::thread(check_worker, "you", 7 * n));
  for (cottontail::addr i = 0; i < m; i++)
    workers.emplace_back(std::thread(append_worker, std::string(love[i])));
  workers.emplace_back(std::thread(check_worker, "love", 4 * n));
  for (auto &worker : workers)
    worker.join();
  cottontail::addr lines = 0;
  while (lines < n * m) {
    bigwig->start();
    std::unique_ptr<cottontail::Hopper> hopper = bigwig->idx()->hopper(line);
    ASSERT_NE(hopper, nullptr);
    cottontail::addr current = 0;
    cottontail::addr p, q;
    for (hopper->tau(0, &p, &q); p < cottontail::maxfinity;
         hopper->tau(p + 1, &p, &q))
      current++;
    bigwig->end();
    EXPECT_GE(current, lines);
    lines = current;
  }
  EXPECT_EQ(lines, n * m);
}

TEST(Bigwig, Love) {
  love(1);
  love(5);
  love(18);
  love(113);
}

TEST(Bigwig, Durable) {
  std::string burrow_name = "bigwig.burrow";
  ASSERT_NO_FATAL_FAILURE(basic(
      false, cottontail::Bigwig::make(burrow_name, "tokenizer:name:ascii")));
  std::shared_ptr<cottontail::Bigwig> bigwig =
      cottontail::Bigwig::make(burrow_name);
  ASSERT_NE(bigwig, nullptr);
  bigwig->start();
  std::unique_ptr<cottontail::Hopper> hopper = bigwig->hopper_from_gcl("sleep");
  cottontail::addr p, q, v;
  for (hopper->tau(cottontail::minfinity + 1, &p, &q);
       p < cottontail::maxfinity; hopper->tau(p + 1, &p, &q))
    ASSERT_EQ(bigwig->txt()->translate(p, q).substr(0, 5), "sleep");
  bigwig->end();
  const char *hamlet[] = {"Alas, poor Yorick! I knew him, Horatio: a fellow",
                          "of infinite jest, of most excellent fancy: he hath",
                          "borne me on his back a thousand times"};
  cottontail::addr line = bigwig->featurizer()->featurize("line:");
  ASSERT_TRUE(bigwig->transaction());
  ASSERT_TRUE(bigwig->appender()->append(std::string(hamlet[0]), &p, &q));
  ASSERT_TRUE(bigwig->annotator()->annotate(line, p, q, (cottontail::addr)12));
  ASSERT_TRUE(bigwig->appender()->append(std::string(hamlet[1]), &p, &q));
  ASSERT_TRUE(bigwig->annotator()->annotate(line, p, q, (cottontail::addr)13));
  ASSERT_TRUE(bigwig->appender()->append(std::string(hamlet[2]), &p, &q));
  ASSERT_TRUE(bigwig->annotator()->annotate(line, p, q, (cottontail::addr)14));
  ASSERT_TRUE(bigwig->ready());
  bigwig->commit();
  bigwig->start();
  hopper = bigwig->hopper_from_gcl("line:");
  cottontail::addr i = 0;
  for (hopper->tau(cottontail::minfinity + 1, &p, &q, &v);
       p < cottontail::maxfinity; hopper->tau(p + 1, &p, &q, &v))
    ASSERT_EQ(++i, v);
  ASSERT_EQ(i, 14);
  hopper = bigwig->hopper_from_gcl("\"may come alas poor yorick\"");
  hopper->ohr(cottontail::maxfinity - 1, &p, &q);
  auto removeNulls = [](std::string s) {
    s.erase(std::remove(s.begin(), s.end(), '\0'), s.end());
    return s;
  };
  ASSERT_EQ(removeNulls(bigwig->txt()->translate(p, q)),
            "may come,\nAlas, poor Yorick! ");
}
