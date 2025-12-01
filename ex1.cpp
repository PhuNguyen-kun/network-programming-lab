#include <iostream>
#include <iomanip>
using namespace std;

int main() {
    float num;
    cin >> num;
    unsigned char* p = reinterpret_cast<unsigned char*>(&num);
    for (int i = 3; i >= 0; --i)
        cout << hex << uppercase << setw(2) << setfill('0') << (int)p[i] << " ";
    cout << endl;
    return 0;
}