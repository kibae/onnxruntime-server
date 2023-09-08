#!/usr/bin/env bash

cd "$(dirname "$0")" || exit

xxd -i -n swagger_index_html ../../../../docs/swagger/index.html swagger_index_html.cpp
xxd -i -n swagger_openapi_yaml ../../../../docs/swagger/openapi.yaml swagger_openapi_yaml.cpp

