//
// Created by kibae on 24. 4. 20.
//

#include "../onnxruntime_server.hpp"
#include "./test_common.hpp"

TEST(test_lib_version, LibVersion) {
	EXPECT_EQ(onnxruntime_server::onnx::version(), "1.20.2");
}
