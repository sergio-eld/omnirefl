#include "structs.h"

#include <omnirefl/refl.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

// fixme: it seems that cmake doesn't pick up the changes that happen in an included header file. it
// makes sence, since only the .cpp files are tracked by cmake. There needs to be some solution,
// i.e.:
// - collect the headers via the tool and track them (too complex, roi low, not robust)
// - force the tool rerunning each time the .cpp file needs recompilation (is it even possible?)

TEST(print_names, simple) {
  {
    const example_types::championship v{};
    const static std::vector<std::string> expected{
      "name",
      "title",
    };
    std::vector<std::string> result;
    omni::reflected_call(example_impl::print_field_names_simple, v, result);
    EXPECT_EQ(expected, result);
  }

  {
    const example_types::wrestler v{};
    const static std::vector<std::string> expected{
      "name",
      "age",
      "catchphrase",
      "titles",
      "info",
    };
    std::vector<std::string> result;
    omni::reflected_call(example_impl::print_field_names_simple, v, result);
    EXPECT_EQ(expected, result);
  }
}

TEST(print_names, recursive) {
  const example_types::wrestler v{};
  const static std::vector<std::string> expected{
    "name",
    "age",
    "catchphrase",

    "titles",
    "titles[].name",
    "titles[].title",

    "info",
    "info.ring_name",
    "info.signature_move",
    "info.debut_year",
  };
  std::vector<std::string> result;
  omni::reflected_call(example_impl::print_field_names_recursive, v, result);
  EXPECT_EQ(expected, result);
}

TEST(print_values, recursive) {
  const example_types::wrestler v{
    "John Cena",
    47,
    "You can't see me",
    /*titles=*/
    {
      {"WWE Championship", "16-time champion"},
      {"World Heavyweight Championship", "3-time champion"},
      {"United States Championship", "5-time champion"},
      {"Royal Rumble", "2-time winner"},
      {"Money in the Bank", "1-time winner"},
      {"Tag Team Championship", "4-time champion"},
    },
    /*info=*/
    {
      "John Cena",
      "Attitude Adjustment",
      2002,
    },
  };

  const static std::vector<std::string> expected{
    // basic Fields
    "name: \"John Cena\"",
    "age: 47",
    "catchphrase: \"You can't see me\"",

    // titles (vector elements)
    "titles[0].name: \"WWE Championship\"",
    "titles[0].title: \"16-time champion\"",
    "titles[1].name: \"World Heavyweight Championship\"",
    "titles[1].title: \"3-time champion\"",
    "titles[2].name: \"United States Championship\"",
    "titles[2].title: \"5-time champion\"",
    "titles[3].name: \"Royal Rumble\"",
    "titles[3].title: \"2-time winner\"",
    "titles[4].name: \"Money in the Bank\"",
    "titles[4].title: \"1-time winner\"",
    "titles[5].name: \"Tag Team Championship\"",
    "titles[5].title: \"4-time champion\"",

    // nested info fields
    "info.ring_name: \"John Cena\"",
    "info.signature_move: \"Attitude Adjustment\"",
    "info.debut_year: 2002",
  };

  std::vector<std::string> result;
  omni::reflected_call(example_impl::print_field_values_recursive, v, result);
  EXPECT_EQ(expected, result);
}

TEST(modify_fields, simple) {
  const static std::map<std::string, std::string> input{
    {"str", "oceanic"},
    {"i", "815"},
  };
  const auto value = [](const std::string &k) { return input.find(k)->second; };
  example_types::settable output;
  omni::reflected_call(example_impl::simple_from_map, output, input);
  EXPECT_EQ(std::to_string(output.i), value("i"));
  EXPECT_EQ(output.str, value("str"));
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
