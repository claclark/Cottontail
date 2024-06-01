#include "src/utf8_tokenizer.h"

#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

#include "src/cottontail.h"

namespace {
const char *libai[] = {
    "自遣 (李白)  Entertaining myself (Li Bai)",
    "",
    "对酒不觉瞑   Too drunk to notice it had gotten dark,",
    "落花盈我衣。 Or that flowers had fallen all over my clothes,",
    "醉起步溪月， I stagger up and walk along the moonlit stream,",
    "鸟还人亦稀。 The birds have gone home; most of the people too."};

void tokenize_test(std::shared_ptr<cottontail::Tokenizer> tokenizer,
                   std::shared_ptr<cottontail::Featurizer> featurizer,
                   const std::string &target) {
  size_t n = target.length() + 1;
  std::unique_ptr<char[]> buffer = std::unique_ptr<char[]>(new char[n]);
  strcpy(buffer.get(), target.c_str());
  std::vector<cottontail::Token> tokens =
      tokenizer->tokenize(featurizer, buffer.get(), n);
  std::vector<std::string> splits = tokenizer->split(target);
  EXPECT_EQ(tokens.size(), splits.size());
  for (size_t i = 0; i < tokens.size(); i++) {
    cottontail::addr feature = featurizer->featurize(splits[i]);
    EXPECT_EQ(tokens[i].feature, feature);
    EXPECT_EQ(tokens[i].address, (cottontail::addr)i);
    if (i > 0) {
      EXPECT_GT(tokens[i].offset, tokens[i - 1].offset);
    }
    EXPECT_NE(tokens[i].length, 0);
    std::vector<std::string> single =
        tokenizer->split(target.substr(tokens[i].offset, tokens[i].length));
    EXPECT_EQ(single.size(), 1);
    EXPECT_EQ(tokens[i].feature, featurizer->featurize(single[0]));
  }
}
} // namespace

TEST(Utf8Tokenizer, Tokenize) {
  std::shared_ptr<cottontail::Tokenizer> tokenizer;
  tokenizer = cottontail::Tokenizer::make("utf8", "");
  ASSERT_NE(tokenizer, nullptr);
  tokenizer = cottontail::Tokenizer::make("utf8", "");
  ASSERT_NE(tokenizer, nullptr);
  std::shared_ptr<cottontail::Featurizer> featurizer =
      cottontail::Featurizer::make("hashing", "");
  ASSERT_NE(featurizer, nullptr);
  tokenize_test(tokenizer, featurizer, "hello world");
  tokenize_test(tokenizer, featurizer, "HeLlO***WoRld");
  tokenize_test(tokenizer, featurizer, "");
  tokenize_test(tokenizer, featurizer, "!(*)*!*#&#&#)@");
  std::string s;
  size_t n = sizeof(libai) / sizeof(char *);
  for (size_t i = 0; i < n; i++)
    s += libai[i] + std::string("\n");
  tokenize_test(tokenizer, featurizer, s);
  tokenize_test(tokenizer, featurizer, "Mixing英语and中文with no spaces");
  tokenize_test(
      tokenizer, featurizer,
      "習近平国家主席のパリ到着を各界の関係者が沿道で歓迎（2024-05-08）");
  tokenize_test(tokenizer, featurizer,
                "신시대 중국을 이해하고 더 나은 미래를 함께 만들어가자 - "
                "싱하이밍 대사 한국 우송대학교 강연（2024-05-05）");
  tokenize_test(
      tokenizer, featurizer,
      "FAIRE RAYONNER L’ESPRIT PRÉSIDANT À L’ÉTABLISSEMENT DES RELATIONS "
      "DIPLOMATIQUES ENTRE LA CHINE ET LA FRANCE ET PROMOUVOIR ENSEMBLE LA "
      "PAIX ET LE DÉVELOPPEMENT DANS LE MONDE");
  tokenize_test(
      tokenizer, featurizer,
      "В среду в Туньси /пров. Аньхой, Восточный Китай/"
      " член Госсовета и министр иностранных дел КНР Ван И провел переговоры с "
      "главой МИД РФ Сергеем Лавровым,"
      " в ходе которых обе стороны пообещали укреплять двусторонние связи");
  tokenize_test(tokenizer, featurizer,
                "في يوم 19 يوليو عام 2021 بالتوقيت المحلي، عقد مستشار الدولة "
                "وزير الخارجية الصيني وانغ يي مؤتمرا صحفيا مشتركا مع وزير "
                "الخارجية الجزائري رمطان لعمامرة في مدينة الجزائر.");

  // Test with some characters whose UTF-8 byte-length changes when case folded.
  // Unicode Character “ſ” (U+017F) Latin Small Letter Long S
  // Unicode Character "Ⱥ" (U+023A) Latin Capital Letter A With Stroke
  // Unicode Character "K" (U+212A) - Kelvin sign
  s = "ſſſ aaaſbbbſ ſſſſſſ 1234 ſ";
  tokenize_test(tokenizer, featurizer, s);
  s = "ȺBC ȺȺȺ Ⱥ1Ⱥ ȺȺȺȺ aAȺAa";
  tokenize_test(tokenizer, featurizer, s);
  s = "100K is ſȺKing cold";
  tokenize_test(tokenizer, featurizer, s);
}

TEST(Utf8Tokenizer, Split) {
  std::shared_ptr<cottontail::Tokenizer> tokenizer =
      cottontail::Tokenizer::make("utf8", "");
  ASSERT_NE(tokenizer, nullptr);
  std::vector<std::string> tokens;
  tokens = tokenizer->split("hello world");
  EXPECT_EQ(tokens.size(), 2);
  EXPECT_EQ(tokens[0], "hello");
  EXPECT_EQ(tokens[1], "world");
  tokens = tokenizer->split("HeLlO     wOrLd");
  EXPECT_EQ(tokens.size(), 2);
  EXPECT_EQ(tokens[0], "hello");
  EXPECT_EQ(tokens[1], "world");
  tokens = tokenizer->split("");
  EXPECT_EQ(tokens.size(), 0);
  tokens = tokenizer->split("。. ...");
  EXPECT_EQ(tokens.size(), 0);
  std::string s;
  size_t n = sizeof(libai) / sizeof(char *);
  for (size_t i = 0; i < n; i++)
    s += libai[i] + std::string("\n");
  tokens = tokenizer->split(s);
  EXPECT_EQ(tokens.size(), 64);
  EXPECT_EQ(tokens[0], "自");
  EXPECT_EQ(tokens[2], "李");
  EXPECT_EQ(tokens[6], "li");
  EXPECT_EQ(tokens[7], "bai");
  EXPECT_EQ(tokens[24], "我");
  EXPECT_EQ(tokens[34], "clothes");
  EXPECT_EQ(tokens[40], "i");
  EXPECT_EQ(tokens[53], "稀");
  EXPECT_EQ(tokens[54], "the");
  EXPECT_EQ(tokens[62], "people");
  EXPECT_EQ(tokens[63], "too");
  tokens = tokenizer->split(
      "Μήνυμα της Προέδρου της Δημοκρατίας Κατερίνας Σακελλαροπούλου προς τον "
      "απόδημο Ελληνισμό με την ευκαιρία της εθνικής εορτής της 25ης Μαρτίου.");
  EXPECT_EQ(tokens.size(), 20);
  EXPECT_EQ(tokens[0], "μήνυμα");
  EXPECT_EQ(tokens[6], "σακελλαροπούλου");
  EXPECT_EQ(tokens[18], "25ησ");
  EXPECT_EQ(tokens[19], "μαρτίου");
  tokens =
      tokenizer->split("💙Blue Heart💗Growing Heart 💚绿色爱心💖 Sparkling "
                       "Heart 💓 Beating Heart 🖤 Black Heart 💟 Heart "
                       "Decoration 💔 Broken Heart 💛 Yellow Heart");

  EXPECT_EQ(tokens.size(), 29);
  EXPECT_EQ(tokens[0], "💙");
  EXPECT_EQ(tokens[5], "heart");
  EXPECT_EQ(tokens[10], "心");
  EXPECT_EQ(tokens[14], "💓");
  EXPECT_EQ(tokens[28], "heart");
  tokens = tokenizer->split(
      "조태열 장관, 제6차 한-호주 외교･국방(2+2) 장관회의 개최");
  EXPECT_EQ(tokens.size(), 11);
  EXPECT_EQ(tokens[2], "제6차");
  EXPECT_EQ(tokens[10], "개최");
  tokens = tokenizer->split(
      "Zu unseren Werten stehen, Risiken vorbeugen, aber auch Gemeinsamkeiten "
      "erkennen und Wege der Zusammenarbeit finden – so sehe ich meine "
      "Aufgabe.  – Botschafterin Dr. Patricia Flor");
  EXPECT_EQ(tokens.size(), 24);
  EXPECT_EQ(tokens[0], "zu");
  EXPECT_EQ(tokens[8], "gemeinsamkeiten");
  EXPECT_EQ(tokens[15], "so");
  EXPECT_EQ(tokens[21], "dr");
  EXPECT_EQ(tokens[23], "flor");
  tokens = tokenizer->split("5月7日、金杉大使は、北京市在住の中村京子さん（93歳"
                            "）を公邸に招き、面会しました。");
  EXPECT_EQ(tokens.size(), 31);
  EXPECT_EQ(tokens[0], "5");
  EXPECT_EQ(tokens[1], "月");
  EXPECT_EQ(tokens[19], "さん");
  EXPECT_EQ(tokens[23], "公");
  EXPECT_EQ(tokens[30], "しました");
  tokens = tokenizer->split(
      "استقبل معالي نائب وزير الخارجية المهندس وليد بن عبدالكريم الخريجي، "
      "اليوم، دولة رئيس وزراء فلسطين وزير الخارجية الدكتور ...");
  EXPECT_EQ(tokens.size(), 18);
  EXPECT_EQ(tokens[0], "استقبل");
  EXPECT_EQ(tokens[17], "الدكتور");
}

namespace {
void skip_test(std::shared_ptr<cottontail::Tokenizer> tokenizer,
               std::shared_ptr<cottontail::Featurizer> featurizer,
               const std::string &target) {
  size_t n = target.length() + 1;
  std::unique_ptr<char[]> buffer = std::unique_ptr<char[]>(new char[n]);
  strcpy(buffer.get(), target.c_str());
  std::vector<cottontail::Token> tokens =
      tokenizer->tokenize(featurizer, buffer.get(), n);
  for (size_t i = 0; i < tokens.size(); i++) {
    const char *remainder = tokenizer->skip(target.c_str(), target.length(), i);
    EXPECT_EQ(target.length(), tokens[i].offset + strlen(remainder));
  }
}
} // namespace

TEST(Utf8Tokenizer, Skip) {
  std::shared_ptr<cottontail::Tokenizer> tokenizer =
      cottontail::Tokenizer::make("utf8", "");
  ASSERT_NE(tokenizer, nullptr);
  std::shared_ptr<cottontail::Featurizer> featurizer =
      cottontail::Featurizer::make("hashing", "");
  ASSERT_NE(featurizer, nullptr);
  std::string s;
  s = "(((((hello ((world)))))))";
  skip_test(tokenizer, featurizer, std::string(s));
  EXPECT_STREQ("hello ((world)))))))",
               tokenizer->skip(s.c_str(), s.length(), 0));
  EXPECT_STREQ("world)))))))", tokenizer->skip(s.c_str(), s.length(), 1));
  EXPECT_STREQ("", tokenizer->skip(s.c_str(), s.length(), 2));
  EXPECT_STREQ("", tokenizer->skip(s.c_str(), s.length(), 3));
  s = libai[0];
  skip_test(tokenizer, featurizer, std::string(s));
  EXPECT_STREQ("自遣 (李白)  Entertaining myself (Li Bai)",
               tokenizer->skip(s.c_str(), s.length(), 0));
  EXPECT_STREQ("白)  Entertaining myself (Li Bai)",
               tokenizer->skip(s.c_str(), s.length(), 3));
  EXPECT_STREQ("Bai)", tokenizer->skip(s.c_str(), s.length(), 7));
  EXPECT_STREQ("", tokenizer->skip(s.c_str(), s.length(), 999));
  skip_test(tokenizer, featurizer, std::string(s));
  s = "Mixing英语and中文with no spaces";
  skip_test(tokenizer, featurizer, std::string(s));
  EXPECT_STREQ("Mixing英语and中文with no spaces",
               tokenizer->skip(s.c_str(), s.length(), -99));
  EXPECT_STREQ("Mixing英语and中文with no spaces",
               tokenizer->skip(s.c_str(), s.length(), 0));
  EXPECT_STREQ("语and中文with no spaces",
               tokenizer->skip(s.c_str(), s.length(), 2));
  EXPECT_STREQ("and中文with no spaces",
               tokenizer->skip(s.c_str(), s.length(), 3));
  EXPECT_STREQ("中文with no spaces", tokenizer->skip(s.c_str(), s.length(), 4));
  EXPECT_STREQ("文with no spaces", tokenizer->skip(s.c_str(), s.length(), 5));
  EXPECT_STREQ("with no spaces", tokenizer->skip(s.c_str(), s.length(), 6));
  EXPECT_STREQ("no spaces", tokenizer->skip(s.c_str(), s.length(), 7));
  EXPECT_STREQ("spaces", tokenizer->skip(s.c_str(), s.length(), 8));
  EXPECT_STREQ("", tokenizer->skip(s.c_str(), s.length(), 9));
  EXPECT_STREQ("", tokenizer->skip(s.c_str(), s.length(), 9999));
  s = "...If you need to find an ATM in order to withdraw 💲 money, then send "
      "the combination 🆘🏧 — “Where is the ATM?” by combining just two "
      "emoticons – emoji 🆘 SOS sign and 🏧 ATM sign emojis.";
  skip_test(tokenizer, featurizer, std::string(s));
  s = "４月26日、楊宇駐日臨時代理大使は日本創価学会の招きに応じ、間もなく中国を"
      "訪れる予定の創価学会の代表団に向けてスピーチした。";
  skip_test(tokenizer, featurizer, std::string(s));
  s = "Capi was dormido aquí.  The girl say, “¿Qué podemos hacer?” to the boy. "
      " Hacen una casa.  He was making the house and he cut out a door.  Y él "
      "tiene su cobija y está happy.";
  skip_test(tokenizer, featurizer, std::string(s));
  s = "skip_test(tokenizer, featurizer, std::string(s))";
  skip_test(tokenizer, featurizer, std::string(s));
  s = ">>>>Cách đây 70 năm, Hiệp định Geneva về đình chỉ chiến sự ở Việt Nam "
      "đã được ký kết, mở ra một trang mới trong sự nghiệp đấu tranh giải "
      "phóng dân tộc, thống nhất đất nước của nhân dân ta. Trải qua 70 năm, "
      "những bài học từ đàm phán, ký kết và thực thi Hiệp định Geneva vẫn còn "
      "nguyên giá trị đối với công cuộc xây dựng, phát triển đất nước và bảo "
      "vệ Tổ quốc ngày nay.<<<<<";
  skip_test(tokenizer, featurizer, std::string(s));
  s = "。。。。。สำนักงานฝ่ายกงสุล  สถานทูตจีนจะปิดทำการในวันที่ 1 – 5 พฤษภาคม พ.ศ.2567 "
      "เนื่องในวันแรงงาน วันที่ 6 พฤษภาคม พ.ศ.2567 เนื่องในวันหยุดชดเชยวันฉัตรมงคล วันที่ 22 "
      "พฤษภาคม พ.ศ.2567 เนื่องในวันวิสาขบูชา。。。。";
  skip_test(tokenizer, featurizer, std::string(s));
}

TEST(Utf8Tokenizer, Boundary) {
  std::shared_ptr<cottontail::Featurizer> featurizer =
      cottontail::Featurizer::make("hashing", "");
  ASSERT_NE(featurizer, nullptr);
  std::shared_ptr<cottontail::Tokenizer> tokenizer =
      cottontail::Tokenizer::make("utf8", "");
  ASSERT_NE(tokenizer, nullptr);
  std::string text = "0.5000";
  std::vector<cottontail::Token> tokens = tokenizer->tokenize(featurizer, text);
  EXPECT_EQ(tokens.size(), 2);
  EXPECT_EQ(tokens[0].address, 0);
  EXPECT_EQ(tokens[0].offset, 0);
  EXPECT_EQ(tokens[0].length, 1);
  EXPECT_EQ(tokens[1].address, 1);
  EXPECT_EQ(tokens[1].offset, 2);
  EXPECT_EQ(tokens[1].length, 4);
  std::vector<std::string> splits = tokenizer->split(text);
  EXPECT_EQ(splits.size(), 2);
  EXPECT_EQ(splits[0], "0");
  EXPECT_EQ(splits[1], "5000");
}
