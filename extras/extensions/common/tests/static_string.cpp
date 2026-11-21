#include <omnirefl/extensions/static_string.hpp>

#include <gtest/gtest.h>

#include <string>

namespace {

struct work { static constexpr const char *str() { return "work"; } };
struct dot { static constexpr const char *str() { return "."; } };
struct city { static constexpr const char *str() { return "city"; } };
struct empty { static constexpr const char *str() { return ""; } };
struct crossing { static constexpr const char *str() { return "k.c"; } };
struct high_byte { static constexpr const char *str() { return "\xff"; } };

using path = omni::cpp11_static_string<work, dot, city>;
using no_text = omni::cpp11_static_string<>;

static_assert(path::size() == 9, "Joined text excludes the terminator");
static_assert(omni::detail::string_length<city>() == 4,
  "NTS provider length excludes its terminator");
static_assert((omni::cpp11_static_string<work, dot>{}
    + omni::cpp11_static_string<city>{}).equals(path::view()),
  "Concatenation joins the provider packs");
static_assert(path::starts_with("work."), "Prefix crosses source boundaries");
static_assert(path::ends_with("city"), "Suffix matches the final source");
static_assert(path::contains(omni::cpp11_static_string<crossing>{}),
  "Search crosses source boundaries");
static_assert(path::find(omni::cpp11_static_string<city>{}) == 5,
  "First matching offset");
static_assert(path::find(omni::cpp11_static_string<city>{}, 6) == path::npos,
  "Search offset");
static_assert(path::find(no_text{}, 9) == 9, "Empty search matches at the end");
static_assert(path::find(no_text{}, 10) == path::npos,
  "Search past the end fails");
static_assert(!path::starts_with("work.city.extra"), "Long prefixes fail");
static_assert(!path::ends_with("work.city.extra"), "Long suffixes fail");
static_assert(path::starts_with('w') && path::ends_with('y'), "Char patterns");
static_assert(path::contains('.') && !path::contains('/'), "Char search");
static_assert(path::compare("work.city") == 0, "Equal text compares equal");
static_assert(path::compare("work") > 0, "An extended prefix compares later");
static_assert(path::compare("work.zoo") < 0, "Lexicographic comparison");
static_assert(omni::cpp11_static_string<high_byte>::compare("\x7f") > 0,
  "Ordering compares unsigned bytes");
static_assert(no_text::empty() && no_text::contains(no_text{}), "Empty text");
static_assert(no_text::starts_with(""), "Empty prefix");
static_assert(no_text::ends_with(""), "Empty suffix");
static_assert(!no_text::starts_with('x'), "Empty text has no character prefix");
static_assert(!no_text::ends_with('x'), "Empty text has no character suffix");
static_assert(no_text::find('x') == no_text::npos, "Empty character search");
static_assert(omni::cpp11_static_string<empty, work, empty>::equals("work"),
  "Empty providers do not change concatenation");
static_assert(path{}.str()[path::size()] == '\0', "CString terminator");
static_assert(path{}[4] == '.', "Indexed access");

#if 201703L <= OMNI_CPLUSPLUS
static_assert(path::find(OMNI_META_STRING("city")) == 5,
  "Direct macro construction is constexpr from C++17");
#endif

#if OMNI_HAS_NTTP_STRING
using literal = omni::nttp_string<"work.city">;
static_assert(literal::equals(path::view()), "Representations agree");
static_assert(omni::detail::string_length<literal>() == 9,
  "NTTP string length uses its extent");
static_assert(literal::contains("k.c") && literal::ends_with("city"));
static_assert(omni::nttp_string<"">::find("") == 0);
#endif

TEST(static_strings, exposes_joined_text_as_a_view) {
  constexpr auto view = path::view();
  EXPECT_EQ("work.city", std::string(view.data(), view.size()));

  const auto token = OMNI_META_STRING("anonymous");
  constexpr decltype(token) anonymous{};
  EXPECT_EQ(0u, anonymous.find(OMNI_META_STRING("anonymous")));
  EXPECT_TRUE(anonymous.equals("anonymous"));

#if OMNI_HAS_NTTP_STRING
  constexpr auto token_literal = OMNI_META_STRING("work.city");
  static_assert(token_literal.equals(path::view()));
  constexpr auto literal_view = literal::view();
  EXPECT_EQ("work.city", std::string(literal_view.data(), literal_view.size()));
#endif
}

} // namespace
