#ifndef SCAN_SCAN_H
#define SCAN_SCAN_H
#include <vector>


namespace parallel {
    void scan(const std::vector<long long>& array, std::vector<long long>& result);
}


#endif //SCAN_SCAN_H