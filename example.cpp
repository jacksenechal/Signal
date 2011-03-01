#include <iostream>
// Declares ALL of CsoundAC as well as ALL of Csound.
#include <Silence.hpp>

int main()
{
    std::cout << "Hello world from CsoundAC and Csound!" << std::endl;
    csound::MusicModel model;
    CppSound *csound = model.getCppSound();
    std::cout << "Composing the music model..." << std::endl;
    // Here is where you would compose your music model.
    std::cout << "Generating the score..." << std::endl;
    model.generate();
    std::cout << "Loading a sample CSD file..." << std::endl;
    csound->load("/home/jack/src/signal/csd/0dbfs.csd");
    std::cout << csound->getCSD() << std::endl;
    std::cout << "Rendering the score..." << std::endl;
    csound->exportForPerformance();
    //model.render();
    std::cout << "Just to prove Csound is really alive in here..." << std::endl;
    csound->perform();

    return 0;
}
