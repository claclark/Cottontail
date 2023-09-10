#include <memory>
#include <string>

#include "gtest/gtest.h"

#include "src/cottontail.h"
#include "src/fiver.h"
#include "test/fake_text.h"

TEST(Fiver, Basic) {
  std::shared_ptr<cottontail::Featurizer> featurizer =
      cottontail::Featurizer::make("hashing", "");
  EXPECT_NE(featurizer, nullptr);
  std::shared_ptr<cottontail::Tokenizer> tokenizer =
      cottontail::Tokenizer::make("ascii", "");
  ASSERT_NE(tokenizer, nullptr);
  std::shared_ptr<cottontail::Fiver> fiver =
      cottontail::Fiver::make(nullptr, featurizer, tokenizer);
  fiver->start();
  ASSERT_TRUE(fiver->transaction());
  std::string test0 = "I swear\n"
                      "That I will be faithful\n"
                      "And bear true allegiance\n"
                      "To His Majesty\n"
                      "King Charles the Third\n"
                      "King of Canada\n"
                      "His Heirs and Successors";
  std::string test1 = "And that I will faithfully observe\n"
                      "The laws of Canada";
  std::string test2 = "Including the Constitution\n"
                      "Which recognizes and affirms\n"
                      "The Aboriginal and treaty rights of\n"
                      "First Nations, Inuit and Metis peoples\n"
                      "And fulfil my duties\n"
                      "As a Canadian citizen.";
  cottontail::addr p, q;
  cottontail::addr staging = cottontail::maxfinity / 2;
  ASSERT_TRUE(fiver->appender()->append(test0, &p, &q));
  EXPECT_EQ(p, staging + 0);
  EXPECT_EQ(q, staging + 24);
  ASSERT_TRUE(fiver->appender()->append(test1, &p, &q));
  EXPECT_EQ(p, staging + 25);
  EXPECT_EQ(q, staging + 34);
  ASSERT_TRUE(fiver->appender()->append(test2, &p, &q));
  EXPECT_EQ(p, staging + 35);
  EXPECT_EQ(q, staging + 61);
  ASSERT_TRUE(fiver->ready());
  fiver->commit();
  std::unique_ptr<cottontail::Hopper> hopper =
      fiver->hopper_from_gcl("\"king of canada\"");
  hopper->tau(0, &p, &q);
  EXPECT_EQ(p, 18);
  EXPECT_EQ(q, 20);
  hopper = fiver->hopper_from_gcl("i");
  ASSERT_NE(hopper, nullptr);
  hopper->tau(0, &p, &q);
  EXPECT_EQ(p, 0);
  EXPECT_EQ(q, 0);
  hopper->uat(1000, &p, &q);
  EXPECT_EQ(p, 27);
  EXPECT_EQ(q, 27);
  EXPECT_EQ(fiver->txt()->tokens(), 62);
  EXPECT_EQ(fiver->txt()->translate(10, 16),
            "allegiance\nTo His Majesty\nKing Charles the ");
  EXPECT_EQ(fiver->txt()->translate(0, 24), test0 + "\n");
  EXPECT_EQ(fiver->txt()->translate(25, 34), test1 + "\n");
  EXPECT_EQ(fiver->txt()->translate(35, 61), test2 + "\n");
  EXPECT_EQ(fiver->txt()->translate(35, 100), test2 + "\n");
  EXPECT_EQ(fiver->txt()->translate(61, 61), "citizen.\n");
  EXPECT_EQ(fiver->txt()->translate(61, 100), "citizen.\n");
  EXPECT_EQ(fiver->txt()->translate(62, 100), "");
  EXPECT_EQ(fiver->txt()->translate(0, 0), "I ");
  EXPECT_EQ(fiver->txt()->translate(0, 2), "I swear\nThat ");
  EXPECT_EQ(fiver->txt()->translate(24, 25), "Successors\nAnd ");
  fiver->end();
}

std::shared_ptr<cottontail::Fiver>
make_fiver(std::shared_ptr<cottontail::Fiver> prev, size_t start,
           FakeText *fake) {
  std::shared_ptr<cottontail::Featurizer> featurizer;
  if (prev == nullptr)
    featurizer = cottontail::Featurizer::make("hashing", "");
  else
    featurizer = prev->featurizer();
  if (featurizer == nullptr)
    return nullptr;
  std::shared_ptr<cottontail::Tokenizer> tokenizer;
  if (prev == nullptr)
    tokenizer = cottontail::Tokenizer::make("ascii", "");
  else
    tokenizer = prev->tokenizer();
  if (tokenizer == nullptr)
    return nullptr;
  std::shared_ptr<cottontail::Fiver> fiver =
      cottontail::Fiver::make(nullptr, featurizer, tokenizer);
  if (fiver == nullptr)
    return nullptr;
  fiver->start();
  if (!fiver->transaction())
    return nullptr;
  cottontail::addr p, q;
  cottontail::addr f = featurizer->featurize("fake:");
  if (!fiver->appender()->append(fake->generate(start + 0, start + 127), &p,
                                 &q))
    return nullptr;
  if (!fiver->annotator()->annotate(f, p, q, 1.0))
    return nullptr;
  if (!fiver->appender()->append(fake->generate(start + 128, start + 216), &p,
                                 &q))
    return nullptr;
  if (!fiver->annotator()->annotate(f, p, q, 2.0))
    return nullptr;
  if (!fiver->appender()->append(fake->generate(start + 217, start + 229), &p,
                                 &q))
    return nullptr;
  fiver->annotator()->annotate(f, p, q, 3.0);
  if (prev != nullptr) {
    if (!prev->txt()->range(&p, &q))
      return nullptr;
    fiver->relocate(q + 1);
  }
  if (!fiver->ready())
    return nullptr;
  fiver->commit();
  fiver->end();
  return fiver;
}

void check_text(std::shared_ptr<cottontail::Fiver> fiver, const std::string &s,
                size_t n) {
  std::unique_ptr<cottontail::Hopper> hopper = fiver->hopper_from_gcl(s);
  ASSERT_NE(hopper, nullptr);
  cottontail::addr p, q;
  size_t i = 0;
  for (hopper->tau(cottontail::minfinity + 1, &p, &q);
       p < cottontail::maxfinity; hopper->tau(p + 1, &p, &q)) {
    std::string t = fiver->txt()->translate(p, q);
    t = "\"" + t + "\"";
    EXPECT_EQ(s, t);
    i++;
  }
  EXPECT_EQ(i, n);
}

TEST(Fiver, Merge) {
  FakeText fake;
  std::vector<std::shared_ptr<cottontail::Fiver>> fivers;
  std::shared_ptr<cottontail::Fiver> fiver0 = make_fiver(nullptr, 10, &fake);
  ASSERT_NE(fiver0, nullptr);
  fivers.push_back(fiver0);
  fiver0->start();
  std::shared_ptr<cottontail::Fiver> fiver1 = make_fiver(fiver0, 20, &fake);
  fiver0->end();
  ASSERT_NE(fiver1, nullptr);
  fivers.push_back(fiver1);
  fiver1->start();
  std::shared_ptr<cottontail::Fiver> fiver2 = make_fiver(fiver1, 30, &fake);
  fiver1->end();
  ASSERT_NE(fiver2, nullptr);
  fivers.push_back(fiver2);
  std::shared_ptr<cottontail::Fiver> fiver = cottontail::Fiver::merge(fivers);
  ASSERT_NE(fiver, nullptr);
  fiver->start();
  std::unique_ptr<cottontail::Hopper> hopper = fiver->hopper_from_gcl("fake:");
  ASSERT_NE(hopper, nullptr);
  cottontail::addr p, q;
  cottontail::fval v;
  hopper->tau(0, &p, &q, &v);
  EXPECT_EQ(p, 0);
  EXPECT_EQ(q, 127);
  EXPECT_EQ(v, 1.0);
  hopper->tau(p + 1, &p, &q, &v);
  EXPECT_EQ(p, 128);
  EXPECT_EQ(q, 216);
  EXPECT_EQ(v, 2.0);
  hopper->tau(p + 1, &p, &q, &v);
  EXPECT_EQ(p, 217);
  EXPECT_EQ(q, 229);
  EXPECT_EQ(v, 3.0);
  hopper->tau(p + 1, &p, &q, &v);
  EXPECT_EQ(p, 230);
  EXPECT_EQ(q, 357);
  EXPECT_EQ(v, 1.0);
  hopper->tau(p + 1, &p, &q, &v);
  EXPECT_EQ(p, 358);
  EXPECT_EQ(q, 446);
  EXPECT_EQ(v, 2.0);
  hopper->tau(p + 1, &p, &q, &v);
  EXPECT_EQ(p, 447);
  EXPECT_EQ(q, 459);
  EXPECT_EQ(v, 3.0);
  hopper->tau(p + 1, &p, &q, &v);
  EXPECT_EQ(p, 460);
  EXPECT_EQ(q, 587);
  EXPECT_EQ(v, 1.0);
  hopper->tau(p + 1, &p, &q, &v);
  EXPECT_EQ(p, 588);
  EXPECT_EQ(q, 676);
  EXPECT_EQ(v, 2.0);
  hopper->tau(p + 1, &p, &q, &v);
  EXPECT_EQ(p, 677);
  EXPECT_EQ(q, 689);
  EXPECT_EQ(v, 3.0);
  check_text(fiver, "\"240\n241\n242\n243\n\"", 2);
  check_text(fiver, "\"238\n239\n20\n21\n\"", 1);
  check_text(fiver, "\"146\n147\n148\n149\n\"", 3);
  check_text(fiver, "\"248\n249\n30\n31\n\"", 1);
  check_text(fiver, "\"136\n137\n138\n139\n\"", 3);
  check_text(fiver, "\"225\n226\n227\n228\n\"", 3);
  fiver->end();
}

TEST(Fiver, Merge2) {
  const char *sonnet20[] = {
      "A woman's face with nature's own hand painted,",
      "Hast thou, the master mistress of my passion;",
      "A woman's gentle heart, but not acquainted",
      "With shifting change, as is false women's fashion:",
      "An eye more bright than theirs, less false in rolling,",
      "Gilding the object whereupon it gazeth;",
      "A man in hue all 'hues' in his controlling,",
      "Which steals men's eyes and women's souls amazeth.",
      "And for a woman wert thou first created;",
      "Till Nature, as she wrought thee, fell a-doting,",
      "And by addition me of thee defeated,",
      "By adding one thing to my purpose nothing.",
      "   But since she prick'd thee out for women's pleasure,",
      "   Mine be thy love and thy love's use their treasure."};
  std::shared_ptr<cottontail::Featurizer> featurizer =
      cottontail::Featurizer::make("hashing", "");
  ASSERT_NE(featurizer, nullptr);
  std::shared_ptr<cottontail::Tokenizer> tokenizer =
      cottontail::Tokenizer::make("ascii", "");
  ASSERT_NE(tokenizer, nullptr);
  cottontail::addr p, q, v;
  cottontail::addr fline = featurizer->featurize("line:");
  std::shared_ptr<cottontail::Fiver> fiver0 =
      cottontail::Fiver::make(nullptr, featurizer, tokenizer);
  ASSERT_NE(fiver0, nullptr);
  fiver0->start();
  ASSERT_TRUE(fiver0->transaction());
  ASSERT_TRUE(fiver0->appender()->append(std::string(sonnet20[0]), &p, &q));
  ASSERT_TRUE(fiver0->annotator()->annotate(fline, p, q, (cottontail::addr)1));
  ASSERT_TRUE(fiver0->appender()->append(std::string(sonnet20[1]), &p, &q));
  ASSERT_TRUE(fiver0->annotator()->annotate(fline, p, q, (cottontail::addr)2));
  ASSERT_TRUE(fiver0->appender()->append(std::string(sonnet20[2]), &p, &q));
  ASSERT_TRUE(fiver0->annotator()->annotate(fline, p, q, (cottontail::addr)3));
  ASSERT_TRUE(fiver0->appender()->append(std::string(sonnet20[3]), &p, &q));
  ASSERT_TRUE(fiver0->annotator()->annotate(fline, p, q, (cottontail::addr)4));
  ASSERT_TRUE(fiver0->ready());
  fiver0->commit();
  fiver0->end();
  std::shared_ptr<cottontail::Fiver> fiver1 =
      cottontail::Fiver::make(nullptr, featurizer, tokenizer);
  ASSERT_NE(fiver1, nullptr);
  fiver1->start();
  ASSERT_TRUE(fiver1->transaction());
  ASSERT_TRUE(fiver1->appender()->append(std::string(sonnet20[4]), &p, &q));
  ASSERT_TRUE(fiver1->annotator()->annotate(fline, p, q, (cottontail::addr)5));
  ASSERT_TRUE(fiver1->appender()->append(std::string(sonnet20[5]), &p, &q));
  ASSERT_TRUE(fiver1->annotator()->annotate(fline, p, q, (cottontail::addr)6));
  ASSERT_TRUE(fiver1->appender()->append(std::string(sonnet20[6]), &p, &q));
  ASSERT_TRUE(fiver1->annotator()->annotate(fline, p, q, (cottontail::addr)7));
  fiver0->start();
  ASSERT_TRUE(fiver0->txt()->range(&p, &q));
  fiver0->end();
  fiver1->relocate(q + 1);
  ASSERT_TRUE(fiver1->ready());
  fiver1->commit();
  fiver1->end();
  std::shared_ptr<cottontail::Fiver> fiver2 =
      cottontail::Fiver::make(nullptr, featurizer, tokenizer);
  ASSERT_NE(fiver2, nullptr);
  fiver2->start();
  ASSERT_TRUE(fiver2->transaction());
  ASSERT_TRUE(fiver2->appender()->append(std::string(sonnet20[7]), &p, &q));
  ASSERT_TRUE(fiver2->annotator()->annotate(fline, p, q, (cottontail::addr)8));
  ASSERT_TRUE(fiver2->appender()->append(std::string(sonnet20[8]), &p, &q));
  ASSERT_TRUE(fiver2->annotator()->annotate(fline, p, q, (cottontail::addr)9));
  ASSERT_TRUE(fiver2->appender()->append(std::string(sonnet20[9]), &p, &q));
  ASSERT_TRUE(fiver2->annotator()->annotate(fline, p, q, (cottontail::addr)10));
  fiver1->start();
  ASSERT_TRUE(fiver1->txt()->range(&p, &q));
  fiver1->end();
  fiver2->relocate(q + 1);
  ASSERT_TRUE(fiver2->ready());
  fiver2->commit();
  fiver2->end();
  std::shared_ptr<cottontail::Fiver> fiver3 =
      cottontail::Fiver::make(nullptr, featurizer, tokenizer);
  ASSERT_NE(fiver3, nullptr);
  fiver3->start();
  ASSERT_TRUE(fiver3->transaction());
  ASSERT_TRUE(fiver3->appender()->append(std::string(sonnet20[10]), &p, &q));
  ASSERT_TRUE(fiver3->annotator()->annotate(fline, p, q, (cottontail::addr)11));
  ASSERT_TRUE(fiver3->appender()->append(std::string(sonnet20[11]), &p, &q));
  ASSERT_TRUE(fiver3->annotator()->annotate(fline, p, q, (cottontail::addr)12));
  ASSERT_TRUE(fiver3->appender()->append(std::string(sonnet20[12]), &p, &q));
  ASSERT_TRUE(fiver3->annotator()->annotate(fline, p, q, (cottontail::addr)13));
  ASSERT_TRUE(fiver3->appender()->append(std::string(sonnet20[13]), &p, &q));
  ASSERT_TRUE(fiver3->annotator()->annotate(fline, p, q, (cottontail::addr)14));
  fiver2->start();
  ASSERT_TRUE(fiver2->txt()->range(&p, &q));
  fiver2->end();
  fiver3->relocate(q + 1);
  ASSERT_TRUE(fiver3->ready());
  fiver3->commit();
  fiver3->end();
  std::vector<std::shared_ptr<cottontail::Fiver>> fivers;
  fivers.push_back(fiver0);
  fivers.push_back(fiver1);
  std::shared_ptr<cottontail::Fiver> fiver01 = cottontail::Fiver::merge(fivers);
  ASSERT_NE(fiver01, nullptr);
  fivers.clear();
  fivers.push_back(fiver2);
  fivers.push_back(fiver3);
  std::shared_ptr<cottontail::Fiver> fiver23 = cottontail::Fiver::merge(fivers);
  ASSERT_NE(fiver23, nullptr);
  fivers.clear();
  fivers.push_back(fiver01);
  fivers.push_back(fiver23);
  std::shared_ptr<cottontail::Fiver> fiver = cottontail::Fiver::merge(fivers);
  fiver->start();
  std::unique_ptr<cottontail::Hopper> hopper = fiver->hopper_from_gcl("line:");
  ASSERT_NE(hopper, nullptr);
  cottontail::addr i = 0;
  for (hopper->tau(0, &p, &q, &v); p < cottontail::maxfinity;
       hopper->tau(p + 1, &p, &q, &v)) {
    std::string s = fiver->txt()->translate(p, q);
    std::string t = std::string(sonnet20[i]);
    if (i == 11) {
      t += "\n   ";
    } else if (i == 12) {
      s = "   " + s;
      t = t + "\n   ";
    } else if (i == 13) {
      s = "   " + s;
      t = t + "\n";
    } else
      t += "\n";
    EXPECT_EQ(t, s);
    i++;
    EXPECT_EQ(i, v);
  }
  hopper = fiver->hopper_from_gcl("\"women s fashion an eye\"");
  ASSERT_NE(hopper, nullptr);
  hopper->tau(0, &p, &q);
  EXPECT_EQ(fiver->txt()->translate(p, q), "women's fashion:\nAn eye ");
  hopper = fiver->hopper_from_gcl("\"fell a-doting, And by\"");
  ASSERT_NE(hopper, nullptr);
  hopper->tau(0, &p, &q);
  EXPECT_EQ(fiver->txt()->translate(p, q), "fell a-doting,\nAnd by ");
  hopper = fiver->hopper_from_gcl("\"controlling, Which\"");
  ASSERT_NE(hopper, nullptr);
  hopper->tau(0, &p, &q);
  EXPECT_EQ(fiver->txt()->translate(p, q), "controlling,\nWhich ");
  std::string t = fiver->txt()->translate(0, 10000);
  std::string s;
  for (i = 0; i < 14; i++)
    s += (std::string(sonnet20[i]) + "\n");
  EXPECT_EQ(s, t);
  hopper = fiver->hopper_from_gcl("woman");
  ASSERT_NE(hopper, nullptr);
  i = 0;
  for (hopper->tau(0, &p, &q); p < cottontail::maxfinity;
       hopper->tau(p + 1, &p, &q)) {
    EXPECT_EQ(fiver->txt()->translate(p, q).substr(0, 5), "woman");
    i++;
  }
  EXPECT_EQ(i, 3);
  hopper = fiver->hopper_from_gcl("women");
  ASSERT_NE(hopper, nullptr);
  i = 0;
  for (hopper->tau(0, &p, &q); p < cottontail::maxfinity;
       hopper->tau(p + 1, &p, &q)) {
    EXPECT_EQ(fiver->txt()->translate(p, q), "women\'");
    i++;
  }
  EXPECT_EQ(i, 3);
  fiver->end();
}

TEST(Fiver, NoText) {
  std::shared_ptr<cottontail::Featurizer> featurizer =
      cottontail::Featurizer::make("hashing", "");
  ASSERT_NE(featurizer, nullptr);
  std::shared_ptr<cottontail::Tokenizer> tokenizer =
      cottontail::Tokenizer::make("ascii", "");
  ASSERT_NE(tokenizer, nullptr);
  cottontail::addr p, q;
  cottontail::addr thing = featurizer->featurize("thing:");
  std::shared_ptr<cottontail::Fiver> fiver0 =
      cottontail::Fiver::make(nullptr, featurizer, tokenizer);
  ASSERT_NE(fiver0, nullptr);
  fiver0->start();
  ASSERT_TRUE(fiver0->transaction());
  ASSERT_TRUE(fiver0->appender()->append("aaa bbb ccc ddd", &p, &q));
  ASSERT_TRUE(fiver0->annotator()->annotate(thing, p, q));
  ASSERT_TRUE(fiver0->appender()->append("eee fff ggg", &p, &q));
  ASSERT_TRUE(fiver0->annotator()->annotate(thing, p, q));
  ASSERT_TRUE(fiver0->appender()->append("hhh iii jjj kkk", &p, &q));
  ASSERT_TRUE(fiver0->annotator()->annotate(thing, p, q));
  ASSERT_TRUE(fiver0->ready());
  fiver0->commit();
  fiver0->end();
  std::shared_ptr<cottontail::Fiver> fiver1 =
      cottontail::Fiver::make(nullptr, featurizer, tokenizer);
  ASSERT_NE(fiver1, nullptr);
  fiver1->start();
  cottontail::addr foo = featurizer->featurize("foo:");
  cottontail::addr bar = featurizer->featurize("bar:");
  ASSERT_TRUE(fiver1->transaction());
  ASSERT_TRUE(fiver1->annotator()->annotate(bar, 3, 4));
  ASSERT_TRUE(fiver1->annotator()->annotate(foo, 5, 6));
  ASSERT_TRUE(fiver1->annotator()->annotate(foo, 8, 9));
  ASSERT_TRUE(fiver1->annotator()->annotate(bar, 6, 7));
  ASSERT_TRUE(fiver1->annotator()->annotate(foo, 1, 2));
  ASSERT_TRUE(fiver1->ready());
  fiver1->commit();
  fiver1->end();
  std::unique_ptr<cottontail::Hopper> hopper;
  fiver1->start();
  hopper = fiver1->idx()->hopper(foo);
  ASSERT_NE(hopper, nullptr);
  hopper->tau(3, &p, &q);
  EXPECT_EQ(p, 5);
  EXPECT_EQ(q, 6);
  hopper->uat(10000, &p, &q);
  EXPECT_EQ(p, 8);
  EXPECT_EQ(q, 9);
  hopper = fiver1->idx()->hopper(bar);
  ASSERT_NE(hopper, nullptr);
  hopper->tau(0, &p, &q);
  EXPECT_EQ(p, 3);
  EXPECT_EQ(q, 4);
  fiver1->end();
  std::vector<std::shared_ptr<cottontail::Fiver>> fivers;
  fivers.push_back(fiver0);
  fivers.push_back(fiver1);
  std::shared_ptr<cottontail::Fiver> fiver = cottontail::Fiver::merge(fivers);
  ASSERT_NE(fiver, nullptr);
  fiver->start();
  hopper = fiver->hopper_from_gcl("jjj");
  ASSERT_NE(hopper, nullptr);
  hopper->tau(0, &p, &q);
  EXPECT_EQ(p, 9);
  EXPECT_EQ(p, q);
  hopper = fiver->hopper_from_gcl("bbb");
  ASSERT_NE(hopper, nullptr);
  hopper->tau(0, &p, &q);
  EXPECT_EQ(p, 1);
  EXPECT_EQ(p, q);
  hopper = fiver->idx()->hopper(foo);
  ASSERT_NE(hopper, nullptr);
  hopper->tau(3, &p, &q);
  EXPECT_EQ(p, 5);
  EXPECT_EQ(q, 6);
  hopper->uat(10000, &p, &q);
  EXPECT_EQ(p, 8);
  EXPECT_EQ(q, 9);
  hopper = fiver->idx()->hopper(bar);
  ASSERT_NE(hopper, nullptr);
  hopper->tau(0, &p, &q);
  EXPECT_EQ(p, 3);
  EXPECT_EQ(q, 4);
  hopper = fiver->hopper_from_gcl("\"ggg hhh\"");
  hopper->tau(0, &p, &q);
  EXPECT_EQ(p, 6);
  EXPECT_EQ(q, 7);
  ASSERT_NE(hopper, nullptr);
  fiver->end();
}

TEST(Fiver, Pickling) {
  const char *helena[] = {
      "How happy some o’er other some can be",
      "Through Athens I am thought as fair as she.",
      "But what of that? Demetrius thinks not so;",
      "He will not know what all but he do know.",
      "And as he errs, doting on Hermia’s eyes,",
      "So I, admiring of his qualities.",
      "Things base and vile, holding no quantity,",
      "Love can transpose to form and dignity.",
      "Love looks not with the eyes, but with the mind;",
      "And therefore is wing’d Cupid painted blind.",
      "Nor hath love’s mind of any judgment taste.",
      "Wings, and no eyes, figure unheedy haste.",
      "And therefore is love said to be a child,",
      "Because in choice he is so oft beguil’d.",
      "As waggish boys in game themselves forswear,",
      "So the boy Love is perjur’d everywhere.",
      "For, ere Demetrius look’d on Hermia’s eyne,",
      "He hail’d down oaths that he was only mine;",
      "And when this hail some heat from Hermia felt,",
      "So he dissolv’d, and showers of oaths did melt.",
      "I will go tell him of fair Hermia’s flight.",
      "Then to the wood will he tomorrow night",
      "Pursue her; and for this intelligence",
      "If I have thanks, it is a dear expense.",
      "But herein mean I to enrich my pain,",
      "To have his sight thither and back again.",
  };
  cottontail::addr n = sizeof(helena) / sizeof(char *);
  std::shared_ptr<cottontail::Featurizer> featurizer =
      cottontail::Featurizer::make("hashing", "");
  EXPECT_NE(featurizer, nullptr);
  std::shared_ptr<cottontail::Tokenizer> tokenizer =
      cottontail::Tokenizer::make("ascii", "");
  ASSERT_NE(tokenizer, nullptr);
  {
    std::shared_ptr<cottontail::Fiver> fiver =
        cottontail::Fiver::make(nullptr, featurizer, tokenizer);
    ASSERT_NE(fiver, nullptr);
    ASSERT_TRUE(fiver->transaction());
    cottontail::addr line = featurizer->featurize("line:");
    for (int i = 0; i < n; i++) {
      cottontail::addr p, q;
      ASSERT_TRUE(fiver->appender()->append(helena[i], &p, &q));
      ASSERT_TRUE(
          fiver->annotator()->annotate(line, p, q, (cottontail::addr)(i + 1)));
    }
    ASSERT_TRUE(fiver->ready());
    fiver->commit();
    fiver->start();
    fiver->pickle("fiver.pickle");
    fiver->end();
  }
  std::shared_ptr<cottontail::Fiver> fiver = cottontail::Fiver::unpickle(
      "fiver.pickle", nullptr, featurizer, tokenizer);
  ASSERT_NE(fiver, nullptr);
  fiver->start();
  EXPECT_EQ(fiver->txt()->translate(16, 24),
            "as she.\nBut what of that? Demetrius thinks not ");
  EXPECT_EQ(fiver->txt()->translate(212, 224),
            "have his sight thither and back again.\n");
  EXPECT_EQ(fiver->txt()->translate(-10, 3), "How happy some o’");
  std::unique_ptr<cottontail::Hopper> h = fiver->hopper_from_gcl("line:");
  cottontail::addr p, q, v, i = 0;
  for (h->tau(0, &p, &q, &v); p < cottontail::maxfinity;
       h->tau(p + 1, &p, &q, &v)) {
    i++;
    ASSERT_EQ(i, v);
  }
  ASSERT_EQ(i, n);
  std::string love = "Love looks not with the eyes, but with the mind;\n";
  h = fiver->hopper_from_gcl("\"" + love + "\"");
  h->ohr(cottontail::maxfinity - 1, &p, &q);
  ASSERT_EQ(fiver->txt()->translate(p, q), love);
  ASSERT_EQ(
      fiver->txt()->translate(cottontail::maxfinity, cottontail::maxfinity),
      "");
  fiver->end();
}
