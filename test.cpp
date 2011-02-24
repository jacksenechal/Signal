#include <CppSound.hpp>
#include <iostream>
using namespace std;

/* This is the C++ version of the main "csound" command 
 * line utility.
 */
int main1(int argc, char **argv)
{
  Csound *cs = new Csound();
  int result = cs->Compile(argc, argv);
  if (result == 0) {
    result = cs->Perform();
  }
  return (result >= 0 ? 0 : result);
}

/* This is a modified version which only takes one argument,
 * which is a .csd file, and plays it. Basically a verification
 * that we can get in there and do other stuff with the API.
 */
int main2(int argc, char **argv)
{
  if (argc < 2) {
    cout << "Usage: test <somefile.csd>\n";
    exit(1);
  } else {
    cout << "Performing file '" << argv[1] << "'\n";
  }
  Csound *cs = new Csound();
  int result = cs->Compile(argv[1]);
  if (result == 0) {
    result = cs->Perform();
  }
  return (result >= 0 ? 0 : result);
}

int main(int argc, char **argv)
{
  main2(argc, argv);
}
