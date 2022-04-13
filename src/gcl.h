#ifndef COTTONTAIL_SRC_GCL_H_
#define COTTONTAIL_SRC_GCL_H_

#include <memory>

#include "src/core.h"
#include "src/hopper.h"

namespace cottontail {

namespace gcl {

class Binary : public Hopper {
public:
  Binary(std::unique_ptr<Hopper> left, std::unique_ptr<Hopper> right)
      : left_(std::move(left)), right_(std::move(right)){};
  Binary(Binary const &) = delete;
  Binary &operator=(Binary const &) = delete;

protected:
  std::unique_ptr<Hopper> left_;
  std::unique_ptr<Hopper> right_;
};

class Combinational : public Binary {
public:
  Combinational(std::unique_ptr<Hopper> left, std::unique_ptr<Hopper> right)
      : Binary(std::move(left), std::move(right)){};

private:
  void tau_(addr k, addr *p, addr *q, fval *v) final;
  void rho_(addr k, addr *p, addr *q, fval *v) final;
  void uat_(addr k, addr *p, addr *q, fval *v) final;
  void ohr_(addr k, addr *p, addr *q, fval *v) final;
};

class And final : public Combinational {
public:
  And(std::unique_ptr<Hopper> left, std::unique_ptr<Hopper> right)
      : Combinational(std::move(left), std::move(right)){};

private:
  addr L_(addr k) final;
  addr R_(addr k) final;
};

class Or final : public Combinational {
public:
  Or(std::unique_ptr<Hopper> left, std::unique_ptr<Hopper> right)
      : Combinational(std::move(left), std::move(right)){};

private:
  addr L_(addr k) final;
  addr R_(addr k) final;
};

class FollowedBy final : public Combinational {
public:
  FollowedBy(std::unique_ptr<Hopper> left, std::unique_ptr<Hopper> right)
      : Combinational(std::move(left), std::move(right)){};

private:
  addr L_(addr k) final;
  addr R_(addr k) final;
};

class ContainedIn final : public Binary {
public:
  ContainedIn(std::unique_ptr<Hopper> left, std::unique_ptr<Hopper> right)
      : Binary(std::move(left), std::move(right)){};

private:
  void tau_(addr k, addr *p, addr *q, fval *v) final;
  void rho_(addr k, addr *p, addr *q, fval *v) final;
  void uat_(addr k, addr *p, addr *q, fval *v) final;
  void ohr_(addr k, addr *p, addr *q, fval *v) final;
};

class Containing final : public Binary {
public:
  Containing(std::unique_ptr<Hopper> left, std::unique_ptr<Hopper> right)
      : Binary(std::move(left), std::move(right)){};

private:
  void tau_(addr k, addr *p, addr *q, fval *v) final;
  void rho_(addr k, addr *p, addr *q, fval *v) final;
  void uat_(addr k, addr *p, addr *q, fval *v) final;
  void ohr_(addr k, addr *p, addr *q, fval *v) final;
};

class NotContainedIn final : public Binary {
public:
  NotContainedIn(std::unique_ptr<Hopper> left, std::unique_ptr<Hopper> right)
      : Binary(std::move(left), std::move(right)){};

private:
  void tau_(addr k, addr *p, addr *q, fval *v) final;
  void rho_(addr k, addr *p, addr *q, fval *v) final;
  void uat_(addr k, addr *p, addr *q, fval *v) final;
  void ohr_(addr k, addr *p, addr *q, fval *v) final;
};

class NotContaining final : public Binary {
public:
  NotContaining(std::unique_ptr<Hopper> left, std::unique_ptr<Hopper> right)
      : Binary(std::move(left), std::move(right)){};

private:
  void tau_(addr k, addr *p, addr *q, fval *v) final;
  void rho_(addr k, addr *p, addr *q, fval *v) final;
  void uat_(addr k, addr *p, addr *q, fval *v) final;
  void ohr_(addr k, addr *p, addr *q, fval *v) final;
};

}
}
#endif // COTTONTAIL_SRC_GCL_H_
