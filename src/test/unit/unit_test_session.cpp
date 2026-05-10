//
// Created by Kibae Shin on 2023/09/02.
//
#include "../../onnxruntime_server.hpp"
#include "../test_common.hpp"

// input_shape / output_shape options must override the model's dynamic dimensions with the
// supplied static values, and any shape whose rank does not match the model's must be rejected.
TEST(unit_test_session, SesionWithShapeOption) {
	Orts::onnx::session_key key("sample", "1");
	const auto session1 = std::make_shared<Orts::onnx::session>(key, model1_path.string());
	ASSERT_EQ(session1->inputs().size(), 3);
	ASSERT_EQ(session1->inputs()[0].shape.size(), 2);
	ASSERT_EQ(session1->inputs()[0].shape[0], -1);
	ASSERT_EQ(session1->inputs()[0].shape[1], 1);

	const auto session2 = std::make_shared<Orts::onnx::session>(
		key, model1_path.string(),
		R"({"input_shape":{"x":[1,1],"y":[2,1],"z":[3,1]}, "output_shape":{"output":[3,1]}})"_json
	);
	std::cout << "session2: " << session2->to_json() << "\n";

	ASSERT_EQ(session2->inputs().size(), 3);
	ASSERT_EQ(session2->inputs()[0].shape.size(), 2);
	for (const auto &input : session2->inputs()) {
		if (input.name == "x") {
			ASSERT_EQ(input.shape[0], 1);
			ASSERT_EQ(input.shape[1], 1);
		} else if (input.name == "y") {
			ASSERT_EQ(input.shape[0], 2);
			ASSERT_EQ(input.shape[1], 1);
		} else if (input.name == "z") {
			ASSERT_EQ(input.shape[0], 3);
			ASSERT_EQ(input.shape[1], 1);
		}
	}
	ASSERT_EQ(session2->outputs().size(), 1);
	ASSERT_EQ(session2->outputs()[0].shape.size(), 2);
	ASSERT_EQ(session2->outputs()[0].shape[0], 3);
	ASSERT_EQ(session2->outputs()[0].shape[1], 1);

	EXPECT_ANY_THROW(
		const auto session3 = std::make_shared<Orts::onnx::session>(
			key, model1_path.string(),
			R"({"input_shape":{"x":[1,-2,1],"y":[2,1],"z":[3,1]}, "output_shape":{"output":[3,1]}})"_json
		);
	);
}

// Each key in the session_options group (threads, execution mode, graph optimization level,
// memory, logging, config_entries) must be applied to onnxruntime's SessionOptions and echoed
// back in option.session_options in a normalized form.
TEST(unit_test_session, SessionWithSessionOptions) {
	Orts::onnx::session_key key("sample", "1");
	auto session = std::make_shared<Orts::onnx::session>(
		key, model1_path.string(),
		R"({
			"session_options": {
				"intra_op_num_threads": 2,
				"inter_op_num_threads": 1,
				"execution_mode": "sequential",
				"graph_optimization_level": "all",
				"enable_cpu_mem_arena": false,
				"enable_mem_pattern": true,
				"logid": "test-session",
				"log_severity_level": 3,
				"config_entries": {
					"session.disable_prepacking": "1"
				}
			}
		})"_json
	);
	auto j = session->to_json();
	ASSERT_TRUE(j["option"].contains("session_options"));
	auto so = j["option"]["session_options"];
	ASSERT_EQ(so["intra_op_num_threads"], 2);
	ASSERT_EQ(so["inter_op_num_threads"], 1);
	ASSERT_EQ(so["execution_mode"], "sequential");
	ASSERT_EQ(so["graph_optimization_level"], "all");
	ASSERT_EQ(so["enable_cpu_mem_arena"], false);
	ASSERT_EQ(so["enable_mem_pattern"], true);
	ASSERT_EQ(so["logid"], "test-session");
	ASSERT_EQ(so["log_severity_level"], 3);
	ASSERT_EQ(so["config_entries"]["session.disable_prepacking"], "1");
}

// Type-mismatched values (e.g. string for an int field), enum strings outside our mapping, and
// keys we do not pass to ORT at all must be silently dropped from the echo. Sibling entries that
// pass our shape check and our enum mapping are still applied and echoed. Note that ORT's own
// validity checks (e.g. allowed numeric ranges) are intentionally not duplicated here; we only
// validate JSON shape and our enum string -> ORT enum mapping.
TEST(unit_test_session, SessionOptionsIgnoresInvalidEntries) {
	// Bad types or unknown keys under session_options are silently dropped; valid ones still apply.
	Orts::onnx::session_key key("sample", "1");
	auto session = std::make_shared<Orts::onnx::session>(
		key, model1_path.string(),
		R"({
			"session_options": {
				"intra_op_num_threads": "not-a-number",
				"graph_optimization_level": "absurd-level",
				"execution_mode": "weird",
				"logid": "still-applies",
				"totally_unknown_key": "ignore-me"
			}
		})"_json
	);
	auto j = session->to_json();
	ASSERT_TRUE(j["option"].contains("session_options"));
	auto so = j["option"]["session_options"];
	ASSERT_FALSE(so.contains("intra_op_num_threads"));
	ASSERT_FALSE(so.contains("graph_optimization_level"));
	ASSERT_FALSE(so.contains("execution_mode"));
	ASSERT_FALSE(so.contains("totally_unknown_key"));
	ASSERT_EQ(so["logid"], "still-applies");
}

// AddSessionConfigEntry round-trips through GetSessionConfigEntry. The echo therefore reflects
// what ORT actually stored, which proves the bool/int -> string conversion the server performs
// before forwarding to ORT (true -> "1", 42 -> "42") matches what ORT will return on lookup.
TEST(unit_test_session, SessionOptionsConfigEntriesReadback) {
	Orts::onnx::session_key key("sample", "1");
	auto session = std::make_shared<Orts::onnx::session>(
		key, model1_path.string(),
		R"({
			"session_options": {
				"config_entries": {
					"key.string": "hello",
					"key.bool": true,
					"key.int": 42
				}
			}
		})"_json
	);
	auto j = session->to_json();
	auto ce = j["option"]["session_options"]["config_entries"];
	ASSERT_EQ(ce["key.string"], "hello");
	ASSERT_EQ(ce["key.bool"], "1");
	ASSERT_EQ(ce["key.int"], "42");
}

// free_dimension_overrides has no readback API; AddFreeDimensionOverrideByName accepts any name
// without raising, so the echo just confirms what we asked ORT to store. Whether a name actually
// matches a model dimension is decided later at session creation time and is ORT's concern, not
// ours. Non-integer values are dropped at our shape-check stage.
TEST(unit_test_session, SessionOptionsFreeDimensionOverrides) {
	Orts::onnx::session_key key("sample", "1");
	auto session = std::make_shared<Orts::onnx::session>(
		key, model1_path.string(),
		R"({
			"session_options": {
				"free_dimension_overrides": {
					"batch": 1,
					"seq": 128,
					"bad": "not-an-int"
				}
			}
		})"_json
	);
	auto j = session->to_json();
	ASSERT_TRUE(j["option"]["session_options"].contains("free_dimension_overrides"));
	auto fd = j["option"]["session_options"]["free_dimension_overrides"];
	ASSERT_EQ(fd["batch"], 1);
	ASSERT_EQ(fd["seq"], 128);
	ASSERT_FALSE(fd.contains("bad"));
}

// session::collect_extensions normalizes both the new "extensions" array and the legacy
// "ortextensions_path" string into a single ordered, deduplicated array of paths in the order
// they would be registered. Pure-function checks here cover what session construction would
// actually attempt to register, without needing a loadable shared library on disk.
TEST(unit_test_session, CollectExtensionsNormalization) {
	using S = Orts::onnx::session;

	// Empty / missing input yields an empty array.
	ASSERT_EQ(S::collect_extensions(json::object()), json::array());
	ASSERT_EQ(S::collect_extensions(R"({"extensions": []})"_json), json::array());

	// Bare extensions array, single element.
	auto only_array = S::collect_extensions(R"({"extensions": ["/lib1.so"]})"_json);
	ASSERT_EQ(only_array, json::array({"/lib1.so"}));

	// Multiple entries preserve input order.
	auto ordered = S::collect_extensions(R"({"extensions": ["/lib1.so", "/lib2.so", "/lib3.so"]})"_json);
	ASSERT_EQ(ordered, json::array({"/lib1.so", "/lib2.so", "/lib3.so"}));

	// Duplicates within the extensions array are dropped, first occurrence wins.
	auto deduped = S::collect_extensions(R"({"extensions": ["/lib.so", "/lib.so", "/lib.so"]})"_json);
	ASSERT_EQ(deduped, json::array({"/lib.so"}));

	// Legacy ortextensions_path alone is normalized into the extensions array.
	auto only_legacy = S::collect_extensions(R"({"ortextensions_path": "/legacy.so"})"_json);
	ASSERT_EQ(only_legacy, json::array({"/legacy.so"}));

	// Legacy is appended after the extensions array, with dedupe across both sources.
	auto mixed = S::collect_extensions(
		R"({"extensions": ["/a.so", "/b.so"], "ortextensions_path": "/a.so"})"_json
	);
	ASSERT_EQ(mixed, json::array({"/a.so", "/b.so"}));

	// Legacy path that is not in the extensions array is appended at the end.
	auto mixed_distinct = S::collect_extensions(
		R"({"extensions": ["/a.so"], "ortextensions_path": "/b.so"})"_json
	);
	ASSERT_EQ(mixed_distinct, json::array({"/a.so", "/b.so"}));

	// Non-string entries inside the extensions array are silently ignored.
	auto with_garbage = S::collect_extensions(
		R"({"extensions": ["/a.so", 42, null, {"x":1}, "/b.so"]})"_json
	);
	ASSERT_EQ(with_garbage, json::array({"/a.so", "/b.so"}));

	// extensions field with a wrong type is treated as if absent.
	ASSERT_EQ(S::collect_extensions(R"({"extensions": "/lib.so"})"_json), json::array());
	ASSERT_EQ(S::collect_extensions(R"({"extensions": 123})"_json), json::array());

	// ortextensions_path with a wrong type is also ignored.
	ASSERT_EQ(S::collect_extensions(R"({"ortextensions_path": 123})"_json), json::array());

	// Non-object option input does not crash and yields an empty array.
	ASSERT_EQ(S::collect_extensions(json("/raw-string")), json::array());
	ASSERT_EQ(S::collect_extensions(json(nullptr)), json::array());
}

// At session construction time, both the new "extensions" array and the legacy "ortextensions_path"
// string must reach the registration path: an unloadable library must surface as a clear
// runtime_error instead of being silently dropped. (The actual successful registration path
// requires a real onnxruntime_extensions shared library on disk and is not exercised here;
// the normalization that drives it is fully covered by the CollectExtensionsNormalization test
// above and by the parse-level tests below.)
TEST(unit_test_session, ExtensionsRegistrationFailsLoudly) {
	Orts::onnx::session_key key("sample", "1");
	EXPECT_ANY_THROW(
		auto session = std::make_shared<Orts::onnx::session>(
			key, model1_path.string(),
			R"({"ortextensions_path": "/nonexistent/path/to/lib.so"})"_json
		);
	);
	EXPECT_ANY_THROW(
		auto session = std::make_shared<Orts::onnx::session>(
			key, model1_path.string(),
			R"({"extensions": ["/nonexistent/path/to/lib.so"]})"_json
		);
	);
}

// Cover the option string grammar end-to-end: empty/whitespace input, malformed model keys that
// must throw, well-formed lists with various spacing, the legacy scalar cuda shortcut, dotted
// notation producing nested objects, repeated "extensions" keys accumulating into a deduped
// array, the legacy ortextensions_path normalization, value type inference (bool/int/string),
// pass-through of unknown keys, and lenient skipping of malformed option entries.
TEST(unit_test_session_key, Parse) {
	// empty cases
	std::string empty_cases[] = {"", " ", "\n", "\r\n", "\n \n", " \r \n \r \n "};
	for (const auto &empty_case : empty_cases) {
		// std::cout << "* empty_case: " << empty_case << "\n";
		ASSERT_EQ(Orts::onnx::session_key_with_option::parse(empty_case).size(), 0);
	}

	// error cases
	std::string fail_cases[] = {
		" model: version", " model :version", "model:ver\bsion\r\n", "model:version(cuda=true", "model:version (cuda=1)"
	};
	for (const auto &fail_case : fail_cases) {
		// std::cout << "* fail_case: " << fail_case << "\n";
		ASSERT_THROW(Orts::onnx::session_key_with_option::parse(fail_case), std::runtime_error);
	}

	// success cases
	std::unordered_map<std::string, size_t> success_cases = {
		{"model:version \bmodel:version", 2},
		{"model:version(cuda =\n1, cuda =2)", 1},
		{"model:version(cuda =\n1)\tmodel:version", 2},
		{"model:version model:version model:version(cuda= 1)", 3},
		{"  model:version", 1},
		{" model:version  model:version  ", 2},
		{"   model:version   model:version(cuda =\r\n1, cuda =\r\n2) model:version ", 3},
		{"model:version ", 1},
		{" model:version \n model:version", 2},
		{" \r \n model:version \n model:version", 2},
		{"   model:version \r \n  model:version model:version ", 3},
	};
	for (const auto &success_case : success_cases) {
		// std::cout << "* success_case: " << success_case.first << "\n";
		ASSERT_EQ(Orts::onnx::session_key_with_option::parse(success_case.first).size(), success_case.second);
	}

	auto parse_case1 = Orts::onnx::session_key_with_option::parse("model:version");
	ASSERT_FALSE(parse_case1[0].option.contains("cuda"));

	auto parse_case2 = Orts::onnx::session_key_with_option::parse("model:version(cuda=1)");
	ASSERT_EQ(parse_case2[0].option["cuda"], 1);

	auto parse_case3 = Orts::onnx::session_key_with_option::parse("model:version(cuda=1, cuda=2)");
	ASSERT_EQ(parse_case3[0].option["cuda"], 2);

	auto parse_case4 = Orts::onnx::session_key_with_option::parse("model:version(cuda=true)");
	ASSERT_TRUE(parse_case4[0].option["cuda"]);

	// dotted notation produces nested objects
	auto parse_dotted_cuda = Orts::onnx::session_key_with_option::parse("model:version(cuda.device_id=0)");
	ASSERT_TRUE(parse_dotted_cuda[0].option["cuda"].is_object());
	ASSERT_EQ(parse_dotted_cuda[0].option["cuda"]["device_id"], 0);

	auto parse_dotted_session = Orts::onnx::session_key_with_option::parse(
		"model:version(session_options.intra_op_num_threads=4, session_options.graph_optimization_level=all)"
	);
	ASSERT_TRUE(parse_dotted_session[0].option["session_options"].is_object());
	ASSERT_EQ(parse_dotted_session[0].option["session_options"]["intra_op_num_threads"], 4);
	ASSERT_EQ(parse_dotted_session[0].option["session_options"]["graph_optimization_level"], "all");

	// scalar followed by dotted on the same group: dotted wins (scalar discarded)
	auto parse_scalar_then_dotted = Orts::onnx::session_key_with_option::parse(
		"model:version(cuda=true, cuda.device_id=1)"
	);
	ASSERT_TRUE(parse_scalar_then_dotted[0].option["cuda"].is_object());
	ASSERT_EQ(parse_scalar_then_dotted[0].option["cuda"]["device_id"], 1);

	// extensions key accumulates as an array
	auto parse_extensions = Orts::onnx::session_key_with_option::parse(
		"model:version(extensions=/lib1.so, extensions=/lib2.so)"
	);
	ASSERT_TRUE(parse_extensions[0].option["extensions"].is_array());
	ASSERT_EQ(parse_extensions[0].option["extensions"].size(), 2);
	ASSERT_EQ(parse_extensions[0].option["extensions"][0], "/lib1.so");
	ASSERT_EQ(parse_extensions[0].option["extensions"][1], "/lib2.so");

	// extensions dedupe
	auto parse_extensions_dedup = Orts::onnx::session_key_with_option::parse(
		"model:version(extensions=/lib.so, extensions=/lib.so)"
	);
	ASSERT_EQ(parse_extensions_dedup[0].option["extensions"].size(), 1);

	// legacy ortextensions_path is normalized into the extensions array
	auto parse_legacy_ext = Orts::onnx::session_key_with_option::parse(
		"model:version(ortextensions_path=/usr/local/lib/libortextensions.so)"
	);
	ASSERT_FALSE(parse_legacy_ext[0].option.contains("ortextensions_path"));
	ASSERT_TRUE(parse_legacy_ext[0].option["extensions"].is_array());
	ASSERT_EQ(parse_legacy_ext[0].option["extensions"].size(), 1);
	ASSERT_EQ(parse_legacy_ext[0].option["extensions"][0], "/usr/local/lib/libortextensions.so");

	// extensions + legacy ortextensions_path mixed, with dedupe
	auto parse_mixed_ext = Orts::onnx::session_key_with_option::parse(
		"model:version(extensions=/a.so, ortextensions_path=/a.so, extensions=/b.so)"
	);
	ASSERT_EQ(parse_mixed_ext[0].option["extensions"].size(), 2);

	// value type inference: bool, int, string
	auto parse_types = Orts::onnx::session_key_with_option::parse(
		"model:version(session_options.enable_cpu_mem_arena=false, "
		"session_options.intra_op_num_threads=8, "
		"session_options.logid=my-model)"
	);
	ASSERT_EQ(parse_types[0].option["session_options"]["enable_cpu_mem_arena"], false);
	ASSERT_EQ(parse_types[0].option["session_options"]["intra_op_num_threads"], 8);
	ASSERT_EQ(parse_types[0].option["session_options"]["logid"], "my-model");

	// unknown / unrecognized option keys pass through silently (caller decides what to do)
	auto parse_unknown = Orts::onnx::session_key_with_option::parse(
		"model:version(some_unknown_key=hello, another.deep.key=42)"
	);
	ASSERT_EQ(parse_unknown[0].option["some_unknown_key"], "hello");
	ASSERT_EQ(parse_unknown[0].option["another"]["deep"]["key"], 42);

	// malformed option entries inside parens are silently skipped, well-formed ones still apply
	auto parse_malformed_options = Orts::onnx::session_key_with_option::parse(
		"model:version(=garbage, cuda=1, !!!, session_options.intra_op_num_threads=2)"
	);
	ASSERT_EQ(parse_malformed_options.size(), 1);
	ASSERT_EQ(parse_malformed_options[0].option["cuda"], 1);
	ASSERT_EQ(parse_malformed_options[0].option["session_options"]["intra_op_num_threads"], 2);
}
