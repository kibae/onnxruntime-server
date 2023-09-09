//
// Created by kibae on 23. 9. 10.
//

#ifndef ONNXRUNTIME_SERVER_SESSION_OPTIONS_HPP
#define ONNXRUNTIME_SERVER_SESSION_OPTIONS_HPP

#include "../../onnxruntime_server.hpp"

json append_cuda_session_options(OrtSessionOptions *session_options, const json &option);

#endif // ONNXRUNTIME_SERVER_SESSION_OPTIONS_HPP
