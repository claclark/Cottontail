#ifndef COTTONTAIL_SRC_HAZEL_H_
#define COTTONTAIL_SRC_HAZEL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "src/core.h"
#include "src/owsla.h"
#include "src/simple_posting.h"
#include "src/warren.h"
#include "src/working.h"

namespace cottontail {

class Hazel final : public Owsla {
public:
  static std::shared_ptr<Warren> make(const std::string &filename,
                                      const std::string &dna,
                                      std::string *error = nullptr);
  static bool merge(std::shared_ptr<Working> working,
                    const std::vector<std::string> &hazels,
                    const std::string &parameters,
                    std::string *error = nullptr);
  static std::shared_ptr<Hazel>
  merge(const std::vector<std::shared_ptr<Hazel>> &hazels,
        const std::string &dst,
        std::shared_ptr<std::map<std::string, std::string>> parameters,
        std::string *error = nullptr);
  static bool sanitize(std::shared_ptr<Working> working,
                       std::vector<OwslaShard> *hazels,
                       std::vector<HazelMergeRecovery> *recoveries,
                       std::string *error = nullptr);
  std::shared_ptr<SimplePosting> posting(addr feature) final;
  addr estimated_size() const final { return estimated_size_; }
  void get_sequence(addr *start, addr *end) const final;
  bool discard(std::string *error = nullptr) final;

  virtual ~Hazel(){};
  Hazel(const Hazel &) = delete;
  Hazel &operator=(const Hazel &) = delete;
  Hazel(Hazel &&) = delete;
  Hazel &operator=(Hazel &&) = delete;

private:
  Hazel(std::shared_ptr<Featurizer> featurizer,
        std::shared_ptr<Tokenizer> tokenizer, std::shared_ptr<Idx> idx,
        std::shared_ptr<Txt> txt)
      : Owsla(nullptr, featurizer, tokenizer, idx, txt){};
  std::shared_ptr<Warren> clone_(std::string *error) final;
  std::string recipe_() final { return dna_; };
  bool set_parameter_(const std::string &key, const std::string &value,
                      std::string *error) final;
  bool get_parameter_(const std::string &key, std::string *value,
                      std::string *error) final;

  std::string filename_;
  std::string dna_;
  std::map<std::string, std::string> parameters_;
  addr estimated_size_ = 0;
  addr sequence_start_ = 0;
  addr sequence_end_ = 0;
};

} // namespace cottontail
#endif // COTTONTAIL_SRC_HAZEL_H_
