#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace serialization_data {

struct payload {
  std::string name;
  int code;
};

struct nested_record {
  double ratio;
  payload data;
};

struct scalar_values {
  bool enabled;
  unsigned retries;
  int delta;
  std::vector<int> values;
  std::string label;
  std::vector<payload> records;
};

struct bitfield_values {
  unsigned code : 10;
  bool enabled : 1;
};

struct coordinates {
  double latitude;
  double longitude;
};

struct address {
  std::string city;
  std::string street;
  unsigned postal_code;
  coordinates location;
};

struct line_item {
  std::string sku;
  std::string description;
  unsigned quantity;
  double unit_price;
  bool taxable;
};

struct order {
  std::uint64_t id;
  std::string customer;
  bool expedited;
  address shipping;
  std::vector<line_item> items;
  std::vector<int> checkpoints;
};

struct document {
  unsigned schema_version;
  std::string source;
  std::vector<order> orders;
};

constexpr char nested_json[] =
  R"({"ratio":23.42,"data":{"name":"oceanic","code":815}})";

constexpr char scalar_json[] = R"({
  "enabled": true,
  "retries": 108,
  "delta": -42,
  "values": [8, 15],
  "label": "oceanic",
  "records": [
    {"name": "oceanic", "code": 815},
    {"name": "sunset", "code": 108}
  ]
})";

constexpr char bitfield_json[] = R"({"code":815,"enabled":true})";

// Repeated strings, nested records, and heterogeneous sequences make this a
// representative allocation-heavy server payload rather than a scalar-only
// parser microbenchmark.
constexpr char representative_json[] = R"({
  "schema_version": 3,
  "source": "checkout-api",
  "orders": [
    {
      "id": 8150001,
      "customer": "oceanic-labs",
      "expedited": true,
      "shipping": {
        "city": "Lisbon",
        "street": "Rua do Oceano 815",
        "postal_code": 1100,
        "location": {"latitude": 38.7223, "longitude": -9.1393}
      },
      "items": [
        {"sku": "sensor-815", "description": "Ocean sensor", "quantity": 4, "unit_price": 42.5, "taxable": true},
        {"sku": "cable-108", "description": "Shielded cable", "quantity": 12, "unit_price": 8.15, "taxable": true},
        {"sku": "manual-23", "description": "Calibration manual", "quantity": 1, "unit_price": 23.42, "taxable": false}
      ],
      "checkpoints": [8, 15, 42, 108]
    },
    {
      "id": 8150002,
      "customer": "northwind-research",
      "expedited": false,
      "shipping": {
        "city": "Reykjavik",
        "street": "Harbor Road 108",
        "postal_code": 101,
        "location": {"latitude": 64.1466, "longitude": -21.9426}
      },
      "items": [
        {"sku": "probe-42", "description": "Deep water probe", "quantity": 2, "unit_price": 815.0, "taxable": true},
        {"sku": "case-15", "description": "Pressure case", "quantity": 2, "unit_price": 108.5, "taxable": true},
        {"sku": "seal-8", "description": "Replacement seal", "quantity": 24, "unit_price": 3.25, "taxable": false}
      ],
      "checkpoints": [15, 23, 108, 815]
    },
    {
      "id": 8150003,
      "customer": "pelagic-systems",
      "expedited": true,
      "shipping": {
        "city": "Split",
        "street": "Adriatic Avenue 42",
        "postal_code": 21000,
        "location": {"latitude": 43.5081, "longitude": 16.4402}
      },
      "items": [
        {"sku": "relay-108", "description": "Telemetry relay", "quantity": 3, "unit_price": 208.15, "taxable": true},
        {"sku": "mount-8", "description": "Deck mount", "quantity": 6, "unit_price": 42.0, "taxable": true},
        {"sku": "tag-23", "description": "Asset tag", "quantity": 30, "unit_price": 1.08, "taxable": false}
      ],
      "checkpoints": [8, 42, 108, 815]
    }
  ]
})";

constexpr char representative_yaml[] = R"(
schema_version: 3
source: checkout-api
orders:
  - id: 8150001
    customer: oceanic-labs
    expedited: true
    shipping:
      city: Lisbon
      street: Rua do Oceano 815
      postal_code: 1100
      location: {latitude: 38.7223, longitude: -9.1393}
    items:
      - {sku: sensor-815, description: Ocean sensor, quantity: 4, unit_price: 42.5, taxable: true}
      - {sku: cable-108, description: Shielded cable, quantity: 12, unit_price: 8.15, taxable: true}
      - {sku: manual-23, description: Calibration manual, quantity: 1, unit_price: 23.42, taxable: false}
    checkpoints: [8, 15, 42, 108]
  - id: 8150002
    customer: northwind-research
    expedited: false
    shipping:
      city: Reykjavik
      street: Harbor Road 108
      postal_code: 101
      location: {latitude: 64.1466, longitude: -21.9426}
    items:
      - {sku: probe-42, description: Deep water probe, quantity: 2, unit_price: 815.0, taxable: true}
      - {sku: case-15, description: Pressure case, quantity: 2, unit_price: 108.5, taxable: true}
      - {sku: seal-8, description: Replacement seal, quantity: 24, unit_price: 3.25, taxable: false}
    checkpoints: [15, 23, 108, 815]
  - id: 8150003
    customer: pelagic-systems
    expedited: true
    shipping:
      city: Split
      street: Adriatic Avenue 42
      postal_code: 21000
      location: {latitude: 43.5081, longitude: 16.4402}
    items:
      - {sku: relay-108, description: Telemetry relay, quantity: 3, unit_price: 208.15, taxable: true}
      - {sku: mount-8, description: Deck mount, quantity: 6, unit_price: 42.0, taxable: true}
      - {sku: tag-23, description: Asset tag, quantity: 30, unit_price: 1.08, taxable: false}
    checkpoints: [8, 42, 108, 815]
)";

constexpr char invalid_nested_integer_json[] =
  R"({"ratio":23.42,"data":{"name":"oceanic","code":"invalid"}})";

constexpr char unknown_field_json[] =
  R"({"ratio":23.42,"data":{"name":"oceanic","code":815},"extra":1})";

constexpr char duplicate_field_json[] =
  R"({"ratio":23.42,"ratio":108.0,"data":{"name":"oceanic","code":815}})";

constexpr char missing_field_json[] = R"({"ratio":23.42})";

} // namespace serialization_data
