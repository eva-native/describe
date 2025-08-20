#include <cassert>
#include <stdexcept>
#include <type_traits>
#include <vector>
#include <iostream>

#include <describe/describe.hpp>
#include <json/json.h>

template<typename C>
struct is_std_vector : std::false_type {};

template<typename T, typename Alloc>
struct is_std_vector<std::vector<T, Alloc>> : std::true_type {};

template<typename C>
inline constexpr bool is_std_vector_v = is_std_vector<C>::value;

const Json::Value *JsonResolve(const Json::Value &j, std::string_view n)
{
  if (auto v = j.find(n.data(), n.data()+n.size()); v) {
    return v;
  }
  return nullptr;
}

template<typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
T FromJson(const Json::Value &j)
{
  if constexpr (std::is_same_v<float, T>) {
    if (!j.isDouble())
      throw std::runtime_error("required value is not a float");
    return j.asFloat();
  }
  if constexpr (std::is_same_v<double, T> || std::is_same_v<long double, T>) {
    if (!j.isDouble())
      throw std::runtime_error("required value is not a double");
    return j.asDouble();
  }
  if constexpr (std::is_same_v<Json::Value::LargestInt, T>) {
    if (!j.isIntegral())
      throw std::runtime_error("required value is not a largest int");
    return j.asLargestInt();
  }
  if constexpr (std::is_same_v<Json::Value::Int64, T>) {
    if (!j.isIntegral())
      throw std::runtime_error("required value is not a int64");
    return j.asInt64();
  }
  if constexpr (std::is_same_v<Json::Value::Int, T> ||
                std::is_same_v<short, T> ||
                std::is_same_v<char, T>) {
    if (!j.isIntegral())
      throw std::runtime_error("required value is not a int");
    return static_cast<T>(j.asInt());
  }
  if constexpr (std::is_same_v<Json::Value::LargestUInt, T> ||
                std::is_same_v<std::size_t, T>) {
    if (!j.isIntegral())
      throw std::runtime_error("required value is not a largest uint");
    return j.asLargestUInt();
  }
  if constexpr (std::is_same_v<Json::Value::UInt64, T>) {
    if (!j.isIntegral())
      throw std::runtime_error("required value is not a uint64");
    return j.asUInt64();
  }
  if constexpr (std::is_same_v<Json::Value::UInt, T> ||
                std::is_same_v<unsigned short, T> ||
                std::is_same_v<unsigned char, T>) {
    if (!j.isIntegral())
      throw std::runtime_error("required value is not a uint");
    return static_cast<T>(j.asUInt());
  }
  if constexpr (std::is_same_v<bool, T>) {
    if (!j.isBool())
      throw std::runtime_error("required value is not a bool");
    return j.asBool();
  }
  assert(false);
}

template<typename T,
         std::enable_if_t<std::is_convertible_v<T, std::string>, int> = 0>
T FromJson(const Json::Value &j)
{
  if (!j.isString()) throw std::runtime_error("required value is not a string");
  return j.asString();
}

template<typename V,
         std::enable_if_t<is_std_vector_v<V>, int> = 0>
V FromJson(const Json::Value &j)
{
  using Vector = V;
  using ValueType = typename Vector::value_type;

  if (j.empty()) {
    return Vector{};
  }
  if (!j.isArray()) {
    return Vector{FromJson<ValueType>(j)};
  }
  Vector vec;
  auto size = j.size();

  vec.reserve(size);

  for (Json::ArrayIndex idx = 0; idx < size; ++idx) {
    vec.push_back(FromJson<ValueType>(j[idx]));
  }

  return vec;
}

template<typename T,
         std::enable_if_t<describe::is_described_struct_v<T>, int> = 0>
T FromJson(const Json::Value &j)
{
  if (!j.isObject()) throw std::runtime_error("required value is not a object");
  T r;
  describe::Get<T>::for_each([&](auto f) {
    if constexpr (f.is_field) {
      if (auto v = JsonResolve(j, f.name); v) {
        f.get(r) = FromJson<typename decltype(f)::type>(*v);
      }
    }
  });
  return r;
}

template<typename T,
         std::enable_if_t<std::is_convertible_v<T, std::string>, int> = 0>
Json::Value ToJson(const T &s)
{
  return Json::Value(std::string(s));
}

template<typename T,
         std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
Json::Value ToJson(const T &s)
{
  if (std::is_unsigned_v<T>)
    return Json::Value(static_cast<Json::Value::LargestUInt>(s));
  return Json::Value(static_cast<Json::Value::LargestInt>(s));
}

template<typename V,
         std::enable_if_t<is_std_vector_v<V>, int> = 0>
Json::Value ToJson(const V &s)
{
  Json::Value v;
  for (auto idx = 0; idx < s.size(); ++idx) {
    v[idx] = ToJson(s[idx]);
  }
  return v;
}

template<typename T,
         std::enable_if_t<describe::is_described_struct_v<T>, int> = 0>
Json::Value ToJson(const T &s)
{
  Json::Value v;
  describe::Get<T>::for_each([&](auto f) {
    if constexpr (f.is_field) {
      v[std::string(f.name)] = ToJson(f.get(s));
    }
  });
  return v;
}

struct Object {
  std::vector<unsigned char> bytes;
  std::size_t size;
  std::string name;
};

DESCRIBE("Object", Object, void) {
  MEMBER("bytes", &_::bytes);
  MEMBER("size", &_::size);
  MEMBER("name", &_::name);
};

int main(int, char **)
{
  auto raw = R"(
{
  "bytes": [ 2, 4, 8, 16, 32 ],
  "size": 128,
  "name": "hehe"
}
  )";

  Json::Value json;
  Json::Reader reader;
  reader.parse(raw, json);

  Object o = FromJson<Object>(json);
  Json::Value r = ToJson(o);
  std::cout << r << std::endl;

  return 0;
}

