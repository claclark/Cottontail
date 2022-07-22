#ifndef COTTONTAIL_SRC_PARAMETERS_H_
#define COTTONTAIL_SRC_PARAMETERS_H_

#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <vector>

#include "src/core.h"

namespace cottontail {

// Functors for parameter optimization.
class RandomParameter {
public:
  RandomParameter()
      : generator_(
            std::chrono::system_clock::now().time_since_epoch().count()){};
  fval operator()() { return random_parameter_(); };
  virtual ~RandomParameter(){};
  RandomParameter(const RandomParameter &) = delete;
  RandomParameter &operator=(const RandomParameter &) = delete;
  RandomParameter(RandomParameter &&) = delete;
  RandomParameter &operator=(RandomParameter &&) = delete;

protected:
  std::default_random_engine generator_;

private:
  virtual fval random_parameter_() = 0;
};

class NotRandomParameter final : public RandomParameter {
public:
  NotRandomParameter(fval value) : value_(value){};
  virtual ~NotRandomParameter(){};
  NotRandomParameter(const NotRandomParameter &) = delete;
  NotRandomParameter &operator=(const NotRandomParameter &) = delete;
  NotRandomParameter(NotRandomParameter &&) = delete;
  NotRandomParameter &operator=(NotRandomParameter &&) = delete;

private:
  fval value_;
  fval random_parameter_() final { return value_; };
};

class UniformRandomParameter final : public RandomParameter {
public:
  UniformRandomParameter(fval a, fval b) : distribution_(a, b) {
    f_ = std::bind(distribution_, generator_);
  };
  UniformRandomParameter() : UniformRandomParameter(0.0, 1.0){};
  virtual ~UniformRandomParameter(){};
  UniformRandomParameter(const UniformRandomParameter &) = delete;
  UniformRandomParameter &operator=(const UniformRandomParameter &) = delete;
  UniformRandomParameter(UniformRandomParameter &&) = delete;
  UniformRandomParameter &operator=(UniformRandomParameter &&) = delete;

private:
  fval random_parameter_() final { return f_(); };
  std::uniform_real_distribution<fval> distribution_;
  std::function<fval()> f_;
};

class LogUniformRandomParameter final : public RandomParameter {
public:
  LogUniformRandomParameter(fval a, fval b)
      : distribution_(std::log(a), std::log(b)) {
    f_ = std::bind(distribution_, generator_);
  };
  virtual ~LogUniformRandomParameter(){};
  LogUniformRandomParameter(const LogUniformRandomParameter &) = delete;
  LogUniformRandomParameter &
  operator=(const LogUniformRandomParameter &) = delete;
  LogUniformRandomParameter(LogUniformRandomParameter &&) = delete;
  LogUniformRandomParameter &operator=(LogUniformRandomParameter &&) = delete;

private:
  fval random_parameter_() final { return std::exp(f_()); };
  std::uniform_real_distribution<fval> distribution_;
  std::function<fval()> f_;
};

class Parameters {
public:
  std::map<std::string, fval> random() {
    std::map<std::string, fval> parameters = random_();
    parameters["depth"] = default_depth();
    return parameters;
  };
  std::map<std::string, fval> defaults() {
    std::map<std::string, fval> parameters = defaults_();
    parameters["depth"] = default_depth();
    return parameters;
  };
  virtual ~Parameters(){};
  Parameters(const Parameters &) = delete;
  Parameters &operator=(const Parameters &) = delete;
  Parameters(Parameters &&) = delete;
  Parameters &operator=(Parameters &&) = delete;

  static std::shared_ptr<Parameters>
  from_ranker_name(const std::string &ranker_name);

protected:
  Parameters() = default;

private:
  fval default_depth();
  virtual std::map<std::string, fval> random_() = 0;
  virtual std::map<std::string, fval> defaults_() = 0;
};
} // namespace cottontail
#endif // COTTONTAIL_SRC_PARAMETERS_H_
