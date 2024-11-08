#!/bin/bash

echo "updating submodules recursively..."
git submodule update --init --recursive

echo "building jjson..."
cd jjson && cmake -DCMAKE_BUILD_TYPE="Release" . && make
cd ../

echo "building json-c..."
cd json-c && cmake . && make || exit 1
cd ../
