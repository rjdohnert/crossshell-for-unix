#ifndef DU_PATH_FILTER_HPP
#define DU_PATH_FILTER_HPP

#include <string>
#include <vector>

namespace du {

bool IsPathExcluded(const std::string& name,
                    const std::vector<std::string>& patterns);

} // namespace du

#endif
