/* github.com/sjoon-oh/soren
 * Author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include <string>
#include <iostream>

#include "spdlog/spdlog.h"

namespace soren {

    std::string proj_name = "soren";
}

int main() {

    std::cout << soren::proj_name << std::endl;
    



    return 0;
}