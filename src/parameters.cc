#include "src/parameters.h"

#include "src/cottontail.h"

// Support parameter tuning tools

namespace cottontail {

fval Parameters::default_depth() { return 1000.0; }

class TieredParameters final : public Parameters {
public:
  TieredParameters() : delta_(0.0, 10.0), default_delta_(1.0){};
  virtual ~TieredParameters(){};
  TieredParameters(const TieredParameters &) = delete;
  TieredParameters &operator=(const TieredParameters &) = delete;
  TieredParameters(TieredParameters &&) = delete;
  TieredParameters &operator=(TieredParameters &&) = delete;

private:
  std::map<std::string, fval> random_() {
    std::map<std::string, fval> parameters;
    parameters.emplace(std::make_pair("tiered:delta", delta_()));
    return parameters;
  };
  std::map<std::string, fval> defaults_() {
    std::map<std::string, fval> parameters;
    parameters.emplace(std::make_pair("tiered:delta", default_delta_));
    return parameters;
  };
  UniformRandomParameter delta_;
  fval default_delta_;
};

class SSRParameters final : public Parameters {
public:
  SSRParameters() : K_(1.0, 1000.0), default_K_(42.0){};
  virtual ~SSRParameters(){};
  SSRParameters(const SSRParameters &) = delete;
  SSRParameters &operator=(const SSRParameters &) = delete;
  SSRParameters(SSRParameters &&) = delete;
  SSRParameters &operator=(SSRParameters &&) = delete;

private:
  std::map<std::string, fval> random_() {
    std::map<std::string, fval> parameters;
    parameters.emplace(std::make_pair("ssr:K", K_()));
    return parameters;
  };
  std::map<std::string, fval> defaults_() {
    std::map<std::string, fval> parameters;
    parameters.emplace(std::make_pair("ssr:K", default_K_));
    return parameters;
  };
  LogUniformRandomParameter K_;
  fval default_K_;
};

class BM25Parameters final : public Parameters {
public:
  BM25Parameters()
      : b_(0.0, 1.0), default_b_(0.25), k1_(0.01, 1000.0), default_k1_(0.5){};
  virtual ~BM25Parameters(){};
  BM25Parameters(const BM25Parameters &) = delete;
  BM25Parameters &operator=(const BM25Parameters &) = delete;
  BM25Parameters(BM25Parameters &&) = delete;
  BM25Parameters &operator=(BM25Parameters &&) = delete;

private:
  std::map<std::string, fval> random_() {
    std::map<std::string, fval> parameters;
    parameters.emplace(std::make_pair("bm25:b", b_()));
    parameters.emplace(std::make_pair("bm25:k1", k1_()));
    return parameters;
  };
  std::map<std::string, fval> defaults_() {
    std::map<std::string, fval> parameters;
    parameters.emplace(std::make_pair("bm25:b", default_b_));
    parameters.emplace(std::make_pair("bm25:k1", default_k1_));
    return parameters;
  };
  UniformRandomParameter b_;
  fval default_b_;
  LogUniformRandomParameter k1_;
  fval default_k1_;
};

class LMDParameters final : public Parameters {
public:
  LMDParameters() : mu_(100.0, 10000.0), default_mu_(2000.0){};
  virtual ~LMDParameters(){};
  LMDParameters(const LMDParameters &) = delete;
  LMDParameters &operator=(const LMDParameters &) = delete;
  LMDParameters(LMDParameters &&) = delete;
  LMDParameters &operator=(LMDParameters &&) = delete;

private:
  std::map<std::string, fval> random_() {
    std::map<std::string, fval> parameters;
    parameters.emplace(std::make_pair("lmd:mu", mu_()));
    return parameters;
  };
  std::map<std::string, fval> defaults_() {
    std::map<std::string, fval> parameters;
    parameters.emplace(std::make_pair("lmd:mu", default_mu_));
    return parameters;
  }
  LogUniformRandomParameter mu_;
  fval default_mu_;
};

class RSJParameters final : public Parameters {
public:
  RSJParameters()
      : expansions_(4, 32), default_expansions_(25), depth_(8, 32),
        default_depth_(10), gamma_(0.0, 1.0), default_gamma_(0.2){};
  virtual ~RSJParameters(){};
  RSJParameters(const RSJParameters &) = delete;
  RSJParameters &operator=(const RSJParameters &) = delete;
  RSJParameters(RSJParameters &&) = delete;
  RSJParameters &operator=(RSJParameters &&) = delete;

private:
  std::map<std::string, fval> random_() {
    std::map<std::string, fval> parameters;
    parameters.emplace(std::make_pair("rsj:expansions", expansions_()));
    parameters.emplace(std::make_pair("rsj:depth", depth_()));
    parameters.emplace(std::make_pair("rsj:gamma", gamma_()));
    return parameters;
  };
  std::map<std::string, fval> defaults_() {
    std::map<std::string, fval> parameters;
    parameters.emplace(std::make_pair("rsj:expansions", default_expansions_));
    parameters.emplace(std::make_pair("rsj:depth", default_depth_));
    parameters.emplace(std::make_pair("rsj:gamma", default_gamma_));
    return parameters;
  };
  LogUniformRandomParameter expansions_;
  fval default_expansions_;
  LogUniformRandomParameter depth_;
  fval default_depth_;
  UniformRandomParameter gamma_;
  fval default_gamma_;
};

class KLDParameters final : public Parameters {
public:
  KLDParameters()
      : expansions_(4, 32), default_expansions_(8), depth_(8, 32),
        default_depth_(16), window_(32, 256), default_window_(64),
        gamma_(0.0, 1.0), default_gamma_(1.0 / 3.0){};
  virtual ~KLDParameters(){};
  KLDParameters(const KLDParameters &) = delete;
  KLDParameters &operator=(const KLDParameters &) = delete;
  KLDParameters(KLDParameters &&) = delete;
  KLDParameters &operator=(KLDParameters &&) = delete;

private:
  std::map<std::string, fval> random_() {
    std::map<std::string, fval> parameters;
    parameters.emplace(std::make_pair("kld:expansions", expansions_()));
    parameters.emplace(std::make_pair("kld:depth", depth_()));
    parameters.emplace(std::make_pair("kld:window", window_()));
    parameters.emplace(std::make_pair("kld:gamma", gamma_()));
    return parameters;
  };
  std::map<std::string, fval> defaults_() {
    std::map<std::string, fval> parameters;
    parameters.emplace(std::make_pair("kld:expansions", default_expansions_));
    parameters.emplace(std::make_pair("kld:depth", default_depth_));
    parameters.emplace(std::make_pair("kld:window", default_window_));
    parameters.emplace(std::make_pair("kld:gamma", default_gamma_));
    return parameters;
  };
  LogUniformRandomParameter expansions_;
  fval default_expansions_;
  LogUniformRandomParameter depth_;
  fval default_depth_;
  LogUniformRandomParameter window_;
  fval default_window_;
  UniformRandomParameter gamma_;
  fval default_gamma_;
};

class QAPParameters final : public Parameters {
public:
  QAPParameters()
      : default_depth_(5), default_length_(64), passages_(16, 256),
        default_passages_(32), window_(16, 256), default_window_(64){};
  virtual ~QAPParameters(){};
  QAPParameters(const QAPParameters &) = delete;
  QAPParameters &operator=(const QAPParameters &) = delete;
  QAPParameters(QAPParameters &&) = delete;
  QAPParameters &operator=(QAPParameters &&) = delete;

private:
  std::map<std::string, fval> random_() {
    std::map<std::string, fval> parameters;
    parameters.emplace(std::make_pair("qap:depth", default_depth_));
    parameters.emplace(std::make_pair("qap:length", default_length_));
    parameters.emplace(std::make_pair("qap:passages", passages_()));
    parameters.emplace(std::make_pair("qap:window", window_()));
    return parameters;
  };
  std::map<std::string, fval> defaults_() {
    std::map<std::string, fval> parameters;
    parameters.emplace(std::make_pair("qap:depth", default_depth_));
    parameters.emplace(std::make_pair("qap:length", default_length_));
    parameters.emplace(std::make_pair("qap:passages", default_passages_));
    parameters.emplace(std::make_pair("qap:window", default_window_));
    return parameters;
  };
  fval default_depth_, default_length_;
  LogUniformRandomParameter passages_;
  fval default_passages_;
  LogUniformRandomParameter window_;
  fval default_window_;
};

std::shared_ptr<Parameters>
Parameters::from_ranker_name(const std::string &ranker_name) {
  if (ranker_name == "bm25")
    return std::make_shared<BM25Parameters>();
  else if (ranker_name == "kld")
    return std::make_shared<KLDParameters>();
  else if (ranker_name == "lmd")
    return std::make_shared<LMDParameters>();
  else if (ranker_name == "rsj")
    return std::make_shared<RSJParameters>();
  else if (ranker_name == "ssr")
    return std::make_shared<SSRParameters>();
  else if (ranker_name == "tiered")
    return std::make_shared<TieredParameters>();
  else if (ranker_name == "qap")
    return std::make_shared<QAPParameters>();
  else
    return nullptr;
}
} // namespace cottontail
