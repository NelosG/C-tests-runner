#ifndef SUM_H
#define SUM_H

#include <vector>


namespace student {
    void analyze(
        const std::vector<long long>& array,
        bool require_positive,
        long long& sum,
        long long& max_val,
        bool& ok
    );
}

#endif
