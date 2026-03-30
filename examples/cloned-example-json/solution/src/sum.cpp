#include <algorithm>
#include <climits>
#include <sum.h>


namespace student {

    void analyze(
        const std::vector<long long>& array,
        bool require_positive,
        long long& sum,
        long long& max_val,
        bool& ok
    ) {
        sum = 0;
        max_val = LLONG_MIN;
        ok = true;
        for(long long v : array) {
            sum += v;
            if(v > max_val) max_val = v;
            if(require_positive && v < 0) ok = false;
        }
        if(array.empty()) max_val = 0;
    }

} // namespace student
