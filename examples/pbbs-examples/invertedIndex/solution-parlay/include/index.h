// Student API for invertedIndex. One parallel variant from pbbsbench's
// parallel/index.C. Input: concatenated documents separated by a
// doc_start delimiter (the engine uses "\n" for line-per-doc tests).
// Output: text where each line is "<word> <doc_id_0> <doc_id_1> ...".

#pragma once

#include <string>

namespace student {

    std::string parallel_build_index(const std::string& text,
                                     const std::string& doc_start);

} // namespace student
