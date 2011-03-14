#include <CppSound.hpp>
#include <iostream>

/* This is the C++ version of the main "csound" command 
 * line utility.
 */
int main(int argc, char **argv)
{
  CppSound *csoundsplat = new CppSound();
  CppSound csound = *csoundsplat;
  int result = csound.compile(argc, argv);
  if (result == 0) {
    csound.perform();
  }
  return (result >= 0 ? 0 : result);
}

