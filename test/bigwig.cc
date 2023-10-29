#include "src/bigwig.h"

#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

#include "src/cottontail.h"

void basic(bool merge, std::shared_ptr<cottontail::Working> working = nullptr) {
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
  std::shared_ptr<cottontail::Featurizer> featurizer =
      cottontail::Featurizer::make("hashing", "");
  ASSERT_NE(featurizer, nullptr);
  std::shared_ptr<cottontail::Tokenizer> tokenizer =
      cottontail::Tokenizer::make("ascii", "");
  ASSERT_NE(tokenizer, nullptr);
  cottontail::addr p, q, v;
  std::unique_ptr<cottontail::Hopper> hopper;
  cottontail::addr line = featurizer->featurize("line:");
  std::shared_ptr<cottontail::Fluffle> fluffle = cottontail::Fluffle::make();
  std::shared_ptr<cottontail::Bigwig> bigwig =
      cottontail::Bigwig::make(working, featurizer, tokenizer, fluffle);
  ASSERT_NE(bigwig, nullptr);
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

TEST(Bigwig, Basic) {
  basic(false);
  basic(true);
}

TEST(Bigwig, Two) {
  std::shared_ptr<cottontail::Featurizer> featurizer =
      cottontail::Featurizer::make("hashing", "");
  ASSERT_NE(featurizer, nullptr);
  std::shared_ptr<cottontail::Tokenizer> tokenizer =
      cottontail::Tokenizer::make("ascii", "");
  ASSERT_NE(tokenizer, nullptr);
  cottontail::addr p, q;
  std::unique_ptr<cottontail::Hopper> hopper;
  std::shared_ptr<cottontail::Fluffle> fluffle = cottontail::Fluffle::make();
  std::shared_ptr<cottontail::Bigwig> big =
      cottontail::Bigwig::make(nullptr, featurizer, tokenizer, fluffle);
  big->merge(false);
  std::shared_ptr<cottontail::Bigwig> wig =
      cottontail::Bigwig::make(nullptr, featurizer, tokenizer, fluffle);
  wig->merge(false);
  ASSERT_NE(big, nullptr);
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

void tiger(cottontail::addr n) {
  std::shared_ptr<cottontail::Featurizer> featurizer =
      cottontail::Featurizer::make("hashing", "");
  ASSERT_NE(featurizer, nullptr);
  std::shared_ptr<cottontail::Tokenizer> tokenizer =
      cottontail::Tokenizer::make("ascii", "");
  ASSERT_NE(tokenizer, nullptr);
  std::shared_ptr<cottontail::Bigwig> bigwig =
      cottontail::Bigwig::make(nullptr, featurizer, tokenizer);
  ASSERT_NE(bigwig, nullptr);
  cottontail::addr line = featurizer->featurize("line:");
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
  std::shared_ptr<cottontail::Featurizer> featurizer =
      cottontail::Featurizer::make("hashing", "");
  ASSERT_NE(featurizer, nullptr);
  std::shared_ptr<cottontail::Tokenizer> tokenizer =
      cottontail::Tokenizer::make("ascii", "");
  ASSERT_NE(tokenizer, nullptr);
  std::shared_ptr<cottontail::Bigwig> bigwig =
      cottontail::Bigwig::make(nullptr, featurizer, tokenizer);
  ASSERT_NE(bigwig, nullptr);
  cottontail::addr line = featurizer->featurize("line:");
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
  for (auto &worker : workers)
    worker.join();
}

TEST(bigwig, Love) {
  love(1);
  love(5);
  love(18);
  love(113);
}

TEST(bigwig, Durable) {
  std::string burrow_name = "bigwig.burrow";
  {
    std::shared_ptr<cottontail::Working> working =
        cottontail::Working::mkdir(burrow_name);
    ASSERT_NE(working, nullptr);
    basic(false, working);
  }
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
  ASSERT_EQ(bigwig->txt()->translate(p, q), "may come,\nAlas, poor Yorick! ");
}
