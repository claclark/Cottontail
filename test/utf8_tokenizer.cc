#include "src/utf8_tokenizer.h"

#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

#include "src/cottontail.h"

TEST(Utf8Tokenizer, Split) {
  std::shared_ptr<cottontail::Tokenizer> tokenizer =
      cottontail::Tokenizer::make("utf8", "");
  ASSERT_NE(tokenizer, nullptr);
  std::vector<std::string> tokens;
  tokens = tokenizer->split("hello world");
  ASSERT_EQ(tokens.size(), 2);
  ASSERT_EQ(tokens[0], "hello");
  ASSERT_EQ(tokens[1], "world");
  tokens = tokenizer->split("HeLlO     wOrLd");
  ASSERT_EQ(tokens.size(), 2);
  ASSERT_EQ(tokens[0], "hello");
  ASSERT_EQ(tokens[1], "world");
  tokens = tokenizer->split("");
  ASSERT_EQ(tokens.size(), 0);
  tokens = tokenizer->split("。. ...");
  ASSERT_EQ(tokens.size(), 0);
  const char *libai[] = {
      "自遣 (李白)  Entertaining myself (Li Bai)",
      "",
      "对酒不觉瞑   Too drunk to notice it had gotten dark,",
      "落花盈我衣。 Or that flowers had fallen all over my clothes,",
      "醉起步溪月， I stagger up and walk along the moonlit stream,",
      "鸟还人亦稀。 The birds have gone home; most of the people too."};
  std::string s;
  size_t n = sizeof(libai) / sizeof(char *);
  for (size_t i = 0; i < n; i++)
    s += libai[i] + std::string("\n");
  tokens = tokenizer->split(s);
  ASSERT_EQ(tokens.size(), 64);
  ASSERT_EQ(tokens[0], "自");
  ASSERT_EQ(tokens[2], "李");
  ASSERT_EQ(tokens[6], "li");
  ASSERT_EQ(tokens[7], "bai");
  ASSERT_EQ(tokens[24], "我");
  ASSERT_EQ(tokens[34], "clothes");
  ASSERT_EQ(tokens[40], "i");
  ASSERT_EQ(tokens[53], "稀");
  ASSERT_EQ(tokens[54], "the");
  ASSERT_EQ(tokens[62], "people");
  ASSERT_EQ(tokens[63], "too");
  tokens = tokenizer->split(
      "Μήνυμα της Προέδρου της Δημοκρατίας Κατερίνας Σακελλαροπούλου προς τον "
      "απόδημο Ελληνισμό με την ευκαιρία της εθνικής εορτής της 25ης Μαρτίου.");
  ASSERT_EQ(tokens.size(), 20);
  ASSERT_EQ(tokens[0], "μήνυμα");
  ASSERT_EQ(tokens[6], "σακελλαροπούλου");
  ASSERT_EQ(tokens[18], "25ησ");
  ASSERT_EQ(tokens[19], "μαρτίου");
  tokens =
      tokenizer->split("💙Blue Heart💗Growing Heart 💚绿色爱心💖 Sparkling "
                       "Heart 💓 Beating Heart 🖤 Black Heart 💟 Heart "
                       "Decoration 💔 Broken Heart 💛 Yellow Heart");

  ASSERT_EQ(tokens.size(), 29);
  ASSERT_EQ(tokens[0], "💙");
  ASSERT_EQ(tokens[5], "heart");
  ASSERT_EQ(tokens[10], "心");
  ASSERT_EQ(tokens[14], "💓");
  ASSERT_EQ(tokens[28], "heart");
  tokens = tokenizer->split(
      "조태열 장관, 제6차 한-호주 외교･국방(2+2) 장관회의 개최");
  ASSERT_EQ(tokens.size(), 11);
  ASSERT_EQ(tokens[2], "제6차");
  ASSERT_EQ(tokens[10], "개최");
  tokens = tokenizer->split(
      "Zu unseren Werten stehen, Risiken vorbeugen, aber auch Gemeinsamkeiten "
      "erkennen und Wege der Zusammenarbeit finden – so sehe ich meine "
      "Aufgabe.  – Botschafterin Dr. Patricia Flor");
  ASSERT_EQ(tokens.size(), 24);
  ASSERT_EQ(tokens[0], "zu");
  ASSERT_EQ(tokens[8], "gemeinsamkeiten");
  ASSERT_EQ(tokens[15], "so");
  ASSERT_EQ(tokens[21], "dr");
  ASSERT_EQ(tokens[23], "flor");
  tokens = tokenizer->split("5月7日、金杉大使は、北京市在住の中村京子さん（93歳"
                            "）を公邸に招き、面会しました。");
  ASSERT_EQ(tokens.size(), 31);
  ASSERT_EQ(tokens[0], "5");
  ASSERT_EQ(tokens[1], "月");
  ASSERT_EQ(tokens[19], "さん");
  ASSERT_EQ(tokens[23], "公");
  ASSERT_EQ(tokens[30], "しました");
  tokens = tokenizer->split(
      "استقبل معالي نائب وزير الخارجية المهندس وليد بن عبدالكريم الخريجي، "
      "اليوم، دولة رئيس وزراء فلسطين وزير الخارجية الدكتور ...");
  ASSERT_EQ(tokens.size(), 18);
  ASSERT_EQ(tokens[0], "استقبل");
  ASSERT_EQ(tokens[17], "الدكتور");
}
