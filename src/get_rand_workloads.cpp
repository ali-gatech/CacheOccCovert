#include <sstream>
#include <iostream>
#include <fstream>
#include <random>
#include <string>

using namespace std;

random_device rd;
uniform_int_distribution<uint32_t> dist (16384, 16384*1.25);

int main()
{
    string filename = "workload1.txt\0";
    ofstream outf(filename);
    for (int i = 0; i < 5000; i++)
    {
        outf << "0x" << hex << dist(rd) << "," << endl;
    }
    outf.close();
    return 0;

}