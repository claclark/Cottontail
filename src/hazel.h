#ifndef COTTONTAIL_SRC_HAZEL_H_
#define COTTONTAIL_SRC_HAZEL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "src/core.h"
#include "src/warren.h"
#include "src/working.h"

namespace cottontail {

class Hazel final : public Warren {
public:
  static std::shared_ptr<Warren> make(const std::string &filename,
                                      const std::string &dna,
                                      std::string *error = nullptr);
  static bool merge(std::shared_ptr<Working> working,
                    const std::vector<std::string> &hazels,
                    const std::string &parameters,
                    std::string *error = nullptr);

  virtual ~Hazel(){};
  Hazel(const Hazel &) = delete;
  Hazel &operator=(const Hazel &) = delete;
  Hazel(Hazel &&) = delete;
  Hazel &operator=(Hazel &&) = delete;

private:
  Hazel(std::shared_ptr<Featurizer> featurizer,
        std::shared_ptr<Tokenizer> tokenizer, std::shared_ptr<Idx> idx,
        std::shared_ptr<Txt> txt)
      : Warren(nullptr, featurizer, tokenizer, idx, txt){};
  std::shared_ptr<Warren> clone_(std::string *error) final;
  std::string recipe_() final { return dna_; };
  bool set_parameter_(const std::string &key, const std::string &value,
                      std::string *error) final;
  bool get_parameter_(const std::string &key, std::string *value,
                      std::string *error) final;

  std::string filename_;
  std::string dna_;
  std::map<std::string, std::string> parameters_;
};

} // namespace cottontail
#endif // COTTONTAIL_SRC_HAZEL_H_
