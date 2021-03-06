#define CATCH_CONFIG_ENABLE_BENCHMARKING
#include <catch2/catch.hpp>

#include <type_traits>

#include "unique_function.hpp"

TEST_CASE("unique_ptr does not support copies")
{
  STATIC_REQUIRE(
      !std::is_copy_constructible_v<beyond::unique_function<void()>>);
  STATIC_REQUIRE(!std::is_copy_assignable_v<beyond::unique_function<void()>>);
}

TEST_CASE("Default constructor")
{
  beyond::unique_function<void()> f;
  REQUIRE(!f);

#ifndef BEYOND_UNIQUE_FUNCTION_NO_EXCEPTION
  SECTION("Invoking empty unique_function throws std::bad_function_call")
  {
    REQUIRE_THROWS_AS(f(), std::bad_function_call);
  }
#endif
}

TEST_CASE("unique_function with a capturless lambda")
{
  beyond::unique_function<int()> f{[]() { return 1; }};
  REQUIRE(f);
  REQUIRE(f() == 1);
}

TEST_CASE("unique_function with a captured lambda")
{
  int x = 1;
  beyond::unique_function<int()> f{[x]() { return x; }};
  beyond::unique_function<int()> f2{[&x]() { return x; }};

  REQUIRE(f);
  REQUIRE(f() == 1);

  x = 2;
  REQUIRE(f() == 1);
  REQUIRE(f2() == 2);
}

TEST_CASE("unique_function with arguments")
{
  beyond::unique_function<int(int, int)> f{[](int x, int y) { return x + y; }};
  REQUIRE(f);
  REQUIRE(f(1, 2) == 3);
}

TEST_CASE("unique_function can move")
{
  const int x = 1;
  beyond::unique_function<int()> f{[&]() { return x; }};
  SECTION("Move contructor")
  {
    auto f2 = std::move(f);
    CHECK(!f);
    REQUIRE(f2);
    REQUIRE(f2() == x);
  }

  SECTION("Move constructor from empty")
  {
    beyond::unique_function<int()> f3;
    auto f2 = std::move(f3);
    CHECK(!f2);
    CHECK(!f3);
  }

  SECTION("Move assignment")
  {
    beyond::unique_function<int()> f2;
    f2 = std::move(f);
    CHECK(!f);
    CHECK(f2);
    CHECK(f2() == x);

    beyond::unique_function<int()> f3;
    f2 = std::move(f3);
    CHECK(!f2);
    CHECK(!f3);
  }
}

struct Counters {
  int constructor = 0;
  int destructor = 0;
  int move = 0;
  int copy = 0;
  int invoke = 0;
};

struct Small {
  Small(Counters& c) : counters{c}
  {
    ++(counters.constructor);
  }

  ~Small()
  {
    ++(counters.destructor);
  }

  Small(const Small& other) : counters{other.counters}
  {
    ++(counters.copy);
  }

  auto operator=(const Small& other) -> Small&
  {
    counters = other.counters;
    ++(counters.copy);
    return *this;
  }

  Small(Small&& other) : counters{other.counters}
  {
    ++(counters.move);
  }

  auto operator=(Small&& other) -> Small&
  {
    counters = other.counters;
    ++(counters.move);
    return *this;
  }

  void operator()()
  {
    ++(counters.invoke);
  }

  Counters& counters;
};

struct Large : Small {
  using Small::Small;

  char b[128]{};
};

TEMPLATE_TEST_CASE("unique_function constructor forwarding and clean-up", "",
                   Small, Large)
{
  Counters cs;
  {
    beyond::unique_function<void()> f{TestType{cs}};
    f();

    REQUIRE(cs.constructor == 1);
    REQUIRE(cs.destructor == 1);
    REQUIRE(cs.move <= 1);
    REQUIRE(cs.copy == 0);
    REQUIRE(cs.invoke == 1);
  }
  REQUIRE(cs.destructor == 2);
}

int func(double)
{
  return 0;
}

TEST_CASE("Swap")
{
  int x = 1;
  beyond::unique_function<int()> f{[x]() { return x; }};
  beyond::unique_function<int()> f2{[]() { return 2; }};

  SECTION("Member function")
  {
    f.swap(f2);
    REQUIRE(f() == 2);
    REQUIRE(f2() == 1);
  }

  SECTION("Free function")
  {
    swap(f, f2);
    REQUIRE(f() == 2);
    REQUIRE(f2() == 1);
  }
}

TEST_CASE("nullptr comparison")
{
  beyond::unique_function<int()> f1;
  beyond::unique_function<int()> f2{[]() { return 42; }};
  REQUIRE(f1 == nullptr);
  REQUIRE(!(f1 != nullptr));
  REQUIRE(nullptr == f1);
  REQUIRE(!(nullptr != f1));

  REQUIRE(f2 != nullptr);
  REQUIRE(!(f2 == nullptr));
  REQUIRE(nullptr != f2);
  REQUIRE(!(nullptr == f2));
}

TEST_CASE("CTAD")
{
  SECTION("Function")
  {
    beyond::unique_function f{func};
    STATIC_REQUIRE(std::is_same_v<decltype(f),
                                  beyond::unique_function<int(double) const>>);
  }

  SECTION("Function pointers")
  {
    beyond::unique_function f{&func};
    STATIC_REQUIRE(std::is_same_v<decltype(f),
                                  beyond::unique_function<int(double) const>>);
  }

  SECTION("Const Function objects")
  {
    int i = 5;
    beyond::unique_function f{[&](double) { return i; }};
    STATIC_REQUIRE(
        std::is_same_v<decltype(f), beyond::unique_function<int(double)>>);
  }

  SECTION("Mutable Function objects")
  {
    int i = 42;
    beyond::unique_function f{[&](double) mutable { return ++i; }};
    STATIC_REQUIRE(
        std::is_same_v<decltype(f), beyond::unique_function<int(double)>>);
  }
}

TEST_CASE("const unique_function")
{
  // beyond::unique_function<int() const> f2{[]() mutable { return 42; }};

  SECTION("A unique_function to const function")
  {
    beyond::unique_function<int() const> f{[]() { return 42; }};
    REQUIRE(f() == 42);

    SECTION("const specialization can implicit convert to none-const version")
    {
      beyond::unique_function<int()> f2 = std::move(f);
      REQUIRE(f2() == 42);
    }
  }
}
