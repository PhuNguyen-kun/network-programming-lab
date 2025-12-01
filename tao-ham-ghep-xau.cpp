#include <iostream>
#include <string>
int main() {
    std::string output;
    std::string tmp;
    while (true) {
        std::getline(std::cin, tmp);
        if (tmp.empty()) break;
        output += tmp;
    }
    std::cout << output << std::endl;
    return 0;
}
