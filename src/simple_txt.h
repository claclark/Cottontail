#ifndef COTTONTAIL_SRC_SIMPLE_TXT_H_
#define COTTONTAIL_SRC_SIMPLE_TXT_H_

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "src/core.h"
#include "src/simple_txt_io.h"
#include "src/txt.h"
#include "src/working.h"

namespace cottontail {

class SimpleTxt final : public Txt {
public:
  static std::shared_ptr<Txt> make(const std::string &recipe,
                                   std::shared_ptr<Tokenizer> tokenizer,
                                   std::shared_ptr<Working> working,
                                   std::string *error = nullptr);
  static bool check(const std::string &recipe, std::string *error = nullptr);

  virtual ~SimpleTxt(){};
  SimpleTxt(const SimpleTxt &) = delete;
  SimpleTxt &operator=(const SimpleTxt &) = delete;
  SimpleTxt(SimpleTxt &&) = delete;
  SimpleTxt &operator=(SimpleTxt &&) = delete;

private:
  SimpleTxt(){};
  std::string recipe_() final;
  std::string translate_(addr p, addr q) final;
  addr tokens_() final;
  bool load_map(const std::string &txt_filename, std::string *error);

  std::shared_ptr<Tokenizer> tokenizer_;
  std::mutex io_lock_;
  std::shared_ptr<SimpleTxtIO> io_;
  std::unique_ptr<addr[]> map_;
  addr map_size_;
  addr map_blocking_;
  addr computed_tokens_;
  bool computed_tokens_valid_;
  std::string compressor_name_;
  std::string compressor_recipe_;
};

bool interpret_simple_txt_recipe(const std::string &recipe,
                                 std::string *compressor_name,
                                 std::string *compressor_recipe,
                                 std::string *error = nullptr);

} // namespace cottontail
#endif // COTTONTAIL_SRC_SIMPLE_TXT_H_
