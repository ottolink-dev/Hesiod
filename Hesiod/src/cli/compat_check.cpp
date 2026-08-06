/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */

/**
 * @file compat_check.cpp
 * @brief `--compat-check=<dir>` corpus mode.
 *
 * For every legacy `.hsd` file under <dir>, and for every node json in it:
 *   1. instantiate the node type via the factory,
 *   2. run json_from (exercising the compat legacy fallback decoders in
 *      legacy_compat.hpp),
 *   3. compare the decoded Meta-container values against the legacy per-key
 *      json (values_equivalent),
 *   4. round-trip the node through its own `_meta` serialization and confirm
 *      the attribute parity record is byte-identical.
 *
 * No graph construction and no compute: node-level decode verification only.
 * This proves the compat decoders read real legacy VALUES correctly (Task 9
 * only proved the DEFAULT records matched).
 */
#include <cmath>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>

#include <nlohmann/json.hpp>

#include "hesiod/cli/batch_mode.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/graph/graph_config.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/node_factory.hpp"

namespace hesiod::cli
{

namespace
{

// Recursively round floats to 5 decimals, mirroring the norm() in
// scripts/compat_parity_diff.py so float noise never trips the comparison.
// nlohmann compares integer- and float-typed numbers by value, so a legacy
// integer array ([2,2]) still equals a canonicalized float array ([2.0,2.0]).
nlohmann::json norm(const nlohmann::json &v)
{
  if (v.is_number_float())
    return std::round(v.get<double>() * 1e5) / 1e5;

  if (v.is_array())
  {
    nlohmann::json out = nlohmann::json::array();
    for (const auto &e : v)
      out.push_back(norm(e));
    return out;
  }

  if (v.is_object())
  {
    nlohmann::json out = nlohmann::json::object();
    for (auto it = v.begin(); it != v.end(); ++it)
      out[it.key()] = norm(it.value());
    return out;
  }

  return v;
}

bool int_eq(const nlohmann::json &a, const nlohmann::json &b)
{
  if (a.is_number() && b.is_number())
    return a.get<long long>() == b.get<long long>();
  return norm(a) == norm(b);
}

// The legacy attribute type_strings (attr::attribute_type_map). A parity entry
// whose "type" is one of these is compat-backed (compat.legacy_type marker set)
// and therefore went through a legacy fallback decoder -- the only attributes
// in scope here. Native Meta params (Noise/Saturate/Cloud's own kw/seed/...)
// carry a mangled C++ type name instead: they have NO compat decoder, so an old
// file's stored value is intentionally NOT read into them, and their parity
// value is never canonicalized to the legacy shape. Skip those.
bool is_legacy_type(const std::string &type)
{
  static const std::set<std::string> legacy_types = {"Bool",
                                                     "Choice",
                                                     "Color",
                                                     "Color gradient",
                                                     "Enumeration",
                                                     "Filename",
                                                     "Float",
                                                     "Array",
                                                     "Cloud",
                                                     "Integer",
                                                     "Value range",
                                                     "Resolution (w x h)",
                                                     "Random seed number",
                                                     "String",
                                                     "Vector of floats",
                                                     "Vector of integers",
                                                     "Vec2Float",
                                                     "Wavenumber"};
  return legacy_types.count(type) > 0;
}

} // namespace

// Compare a decoded (already parity-canonicalized) value against the legacy
// per-key json object. `decoded` is the parity record's `value` field (glm
// values already folded to arrays, gradient stops unwrapped); `legacy_attr` is
// the full legacy attribute object from the .hsd file (value + siblings like
// is_active / x / y / values); `type` is the legacy type_string.
bool values_equivalent(const nlohmann::json &decoded,
                       const nlohmann::json &legacy_attr,
                       const std::string    &type)
{
  const nlohmann::json lv = legacy_attr.contains("value") ? legacy_attr["value"]
                                                          : nlohmann::json();

  // Value range: the parity record folds is_active into the value as
  // {"value":[min,max], "is_active":bool}; legacy keeps a top-level "value"
  // array plus a sibling "is_active".
  if (type == "Value range")
  {
    const nlohmann::json dval = (decoded.is_object() && decoded.contains("value"))
                                    ? decoded["value"]
                                    : decoded;
    const bool           d_active = (decoded.is_object() && decoded.contains("is_active"))
                                        ? decoded["is_active"].get<bool>()
                                        : true;
    const bool           l_active = legacy_attr.value("is_active", true);
    return norm(dval) == norm(lv) && d_active == l_active;
  }

  // Enumeration: the decoder resolves the legacy "choice" string through the
  // enum_items map to an int; in a faithful legacy file that int equals the
  // stored "value". Integer compare against "value".
  if (type == "Enumeration")
    return int_eq(decoded, lv);

  // Seed / Integer: integer compare (avoids float-repr surprises).
  if (type == "Random seed number" || type == "Integer")
    return int_eq(decoded, lv);

  // Cloud: legacy serializes parallel x/y/values arrays and NO "value" key, so
  // the caller's `contains("value")` guard normally skips it (and the parity
  // value is null on both backends). Defensive: rebuild vec3 triplets and
  // compare should a file ever carry a "value" alongside x/y/values.
  if (type == "Cloud")
  {
    if (!(legacy_attr.contains("x") && legacy_attr.contains("y") &&
          legacy_attr.contains("values")))
      return true; // nothing comparable

    const nlohmann::json &x = legacy_attr["x"];
    const nlohmann::json &y = legacy_attr["y"];
    const nlohmann::json &vals = legacy_attr["values"];

    if (!x.is_array() || !y.is_array() || !vals.is_array() || x.size() != y.size() ||
        x.size() != vals.size())
      return false;

    if (!decoded.is_array() || decoded.size() != x.size())
      return false;

    for (size_t k = 0; k < x.size(); ++k)
    {
      const nlohmann::json triplet = nlohmann::json::array({x[k], y[k], vals[k]});
      if (norm(decoded[k]) != norm(triplet))
        return false;
    }
    return true;
  }

  // Everything else (Float, Bool, Filename, String, Choice, Color, Wavenumber,
  // Vec2Float, Color gradient, Vector of floats, ...): both sides are already
  // in matching shape after the parity canonicalization -- direct compare after
  // float rounding.
  return norm(decoded) == norm(lv);
}

int run_compat_check(const std::string &dir)
{
  Logger::log()->info("executing Hesiod in compat-check mode: {}", dir);

  if (!std::filesystem::exists(dir))
  {
    Logger::log()->error("compat-check: directory does not exist: {}", dir);
    return 1;
  }

  auto config = std::make_shared<hesiod::GraphConfig>();
  int  files = 0, nodes = 0, failures = 0;

  for (const auto &entry : std::filesystem::recursive_directory_iterator(dir))
  {
    if (entry.path().extension() != ".hsd")
      continue;
    files++;

    nlohmann::json root;
    try
    {
      std::ifstream f(entry.path());
      f >> root;
    }
    catch (const std::exception &e)
    {
      // A malformed / non-JSON .hsd is a corpus quirk, not a decoder bug: log
      // and skip rather than fail the run.
      Logger::log()->warn("compat-check: could not parse {}: {}",
                          entry.path().string(),
                          e.what());
      continue;
    }

    if (!root.contains("graph_manager") || !root["graph_manager"].contains("graph_nodes"))
      continue;

    for (auto &[graph_id, graph] : root["graph_manager"]["graph_nodes"].items())
    {
      (void)graph_id;
      if (!graph.contains("nodes"))
        continue;

      for (auto &node_json : graph["nodes"])
      {
        const std::string type = node_json.value("label", "");
        nodes++;

        std::shared_ptr<gnode::Node> p_node;
        try
        {
          p_node = hesiod::node_factory(type, config);
        }
        catch (const std::exception &)
        {
          // Node type not present in this build: tolerate (legacy-file quirk).
          Logger::log()->warn("compat-check: unknown node type '{}' in {}",
                              type,
                              entry.path().string());
          continue;
        }

        auto *p_base = dynamic_cast<hesiod::BaseNode *>(p_node.get());
        if (!p_base)
          continue;

        p_base->json_from(node_json);

        // 1) decoded values vs legacy per-key fields
        nlohmann::json parity = p_base->attribute_parity_record();
        for (auto &[key, rec] : parity.items())
        {
          if (key == "__order" || !node_json.contains(key))
            continue;
          if (!rec.contains("value") || !rec.contains("type"))
            continue;
          // Only compat-backed attributes (a legacy type_string) went through a
          // fallback decoder; native Meta params are out of scope.
          if (!is_legacy_type(rec["type"].get<std::string>()))
            continue;
          if (!node_json[key].is_object() || !node_json[key].contains("value"))
            continue;

          if (!values_equivalent(rec["value"], node_json[key], rec["type"]))
          {
            Logger::log()->error("compat-check: {} {}[{}]: decoded {} vs legacy {}",
                                 entry.path().filename().string(),
                                 type,
                                 key,
                                 rec["value"].dump(),
                                 node_json[key]["value"].dump());
            failures++;
          }
        }

        // 2) _meta round-trip: json_to writes "_meta", json_from reads it back
        //    through the Meta path (not the legacy decoders).
        nlohmann::json               saved = p_base->json_to();
        std::shared_ptr<gnode::Node> p_node2;
        try
        {
          p_node2 = hesiod::node_factory(type, config);
        }
        catch (const std::exception &)
        {
          continue;
        }
        auto *p_base2 = dynamic_cast<hesiod::BaseNode *>(p_node2.get());
        if (!p_base2)
          continue;

        p_base2->json_from(saved);
        if (p_base2->attribute_parity_record() != parity)
        {
          Logger::log()->error("compat-check: {} {}: _meta round-trip mismatch",
                               entry.path().filename().string(),
                               type);
          failures++;
        }
      }
    }
  }

  Logger::log()->info("compat-check: {} files, {} nodes, {} failures",
                      files,
                      nodes,
                      failures);
  return failures ? 1 : 0;
}

} // namespace hesiod::cli
