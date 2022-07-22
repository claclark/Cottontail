#ifndef COTTONTAIL_SRC_GCL_H_
#define COTTONTAIL_SRC_GCL_H_

#include <memory>

#include "src/core.h"
#include "src/hopper.h"

namespace cottontail {

namespace gcl {

class Unary : public Hopper {
public:
  Unary(std::unique_ptr<Hopper> expr) : expr_(std::move(expr)){};
  virtual ~Unary(){};
  Unary(Unary const &) = delete;
  Unary &operator=(Unary const &) = delete;
  Unary(Unary &&) = delete;
  Unary &operator=(Unary &&) = delete;

protected:
  std::unique_ptr<Hopper> expr_;
};

class Binary : public Hopper {
public:
  Binary(std::unique_ptr<Hopper> left, std::unique_ptr<Hopper> right)
      : left_(std::move(left)), right_(std::move(right)){};
  virtual ~Binary(){};
  Binary(Binary const &) = delete;
  Binary &operator=(Binary const &) = delete;
  Binary(Binary &&) = delete;
  Binary &operator=(Binary &&) = delete;

protected:
  std::unique_ptr<Hopper> left_;
  std::unique_ptr<Hopper> right_;
};

class Combinational : public Binary {
public:
  Combinational(std::unique_ptr<Hopper> left, std::unique_ptr<Hopper> right)
      : Binary(std::move(left), std::move(right)){};
  virtual ~Combinational(){};
  Combinational(Combinational const &) = delete;
  Combinational &operator=(Combinational const &) = delete;
  Combinational(Combinational &&) = delete;
  Combinational &operator=(Combinational &&) = delete;

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
  virtual ~And(){};
  And(And const &) = delete;
  And &operator=(And const &) = delete;
  And(And &&) = delete;
  And &operator=(And &&) = delete;

private:
  addr L_(addr k) final;
  addr R_(addr k) final;
};

class Or final : public Combinational {
public:
  Or(std::unique_ptr<Hopper> left, std::unique_ptr<Hopper> right)
      : Combinational(std::move(left), std::move(right)){};
  virtual ~Or(){};
  Or(Or const &) = delete;
  Or &operator=(Or const &) = delete;
  Or(Or &&) = delete;
  Or &operator=(Or &&) = delete;

private:
  addr L_(addr k) final;
  addr R_(addr k) final;
};

class FollowedBy final : public Combinational {
public:
  FollowedBy(std::unique_ptr<Hopper> left, std::unique_ptr<Hopper> right)
      : Combinational(std::move(left), std::move(right)){};
  virtual ~FollowedBy(){};
  FollowedBy(FollowedBy const &) = delete;
  FollowedBy &operator=(FollowedBy const &) = delete;
  FollowedBy(FollowedBy &&) = delete;
  FollowedBy &operator=(FollowedBy &&) = delete;

private:
  addr L_(addr k) final;
  addr R_(addr k) final;
};

class ContainedIn final : public Binary {
public:
  ContainedIn(std::unique_ptr<Hopper> left, std::unique_ptr<Hopper> right)
      : Binary(std::move(left), std::move(right)){};
  virtual ~ContainedIn(){};
  ContainedIn(ContainedIn const &) = delete;
  ContainedIn &operator=(ContainedIn const &) = delete;
  ContainedIn(ContainedIn &&) = delete;
  ContainedIn &operator=(ContainedIn &&) = delete;

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
  virtual ~Containing(){};
  Containing(Containing const &) = delete;
  Containing &operator=(Containing const &) = delete;
  Containing(Containing &&) = delete;
  Containing &operator=(Containing &&) = delete;

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
  virtual ~NotContainedIn(){};
  NotContainedIn(NotContainedIn const &) = delete;
  NotContainedIn &operator=(NotContainedIn const &) = delete;
  NotContainedIn(NotContainedIn &&) = delete;
  NotContainedIn &operator=(NotContainedIn &&) = delete;

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
  virtual ~NotContaining(){};
  NotContaining(NotContaining const &) = delete;
  NotContaining &operator=(NotContaining const &) = delete;
  NotContaining(NotContaining &&) = delete;
  NotContaining &operator=(NotContaining &&) = delete;

private:
  void tau_(addr k, addr *p, addr *q, fval *v) final;
  void rho_(addr k, addr *p, addr *q, fval *v) final;
  void uat_(addr k, addr *p, addr *q, fval *v) final;
  void ohr_(addr k, addr *p, addr *q, fval *v) final;
};

class Link : public Unary {
public:
  Link(std::unique_ptr<Hopper> expr) : Unary(std::move(expr)){};
  virtual ~Link(){};
  Link(Link const &) = delete;
  Link &operator=(Link const &) = delete;
  Link(Link &&) = delete;
  Link &operator=(Link &&) = delete;

private:
  bool linked_ = false;
  void tau_(addr k, addr *p, addr *q, fval *v) final;
  void rho_(addr k, addr *p, addr *q, fval *v) final;
  void uat_(addr k, addr *p, addr *q, fval *v) final;
  void ohr_(addr k, addr *p, addr *q, fval *v) final;
};

} // namespace gcl
} // namespace cottontail
#endif // COTTONTAIL_SRC_GCL_H_
