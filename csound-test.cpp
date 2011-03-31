#include <Silence.hpp>
#include <iostream>
using namespace std;

/* This is the C++ version of the main "csound" command 
 * line utility.
 */
int main(int argc, char **argv)
{
  string orchestra = "\
    sr = 44100\
    ksmps = 100\
    nchnls = 2\
    instr 1\
      ; Sharp attack, but not sharp enough to click.\
      iattack = 0.005\
      ; Moderate decay.\
      idecay = 0.2\
      ; Fast but gentle release.\
      irelease = 0.05\
      ; Extend the total duration (p3) to include the attack, decay, and release.\
      isustain = p3\
      p3 = iattack + idecay + isustain + irelease\
      ; Exponential envelope.\
      kenvelope transeg 0.0, iattack, -3.0, 1.0, idecay, -3.0, 0.25, isustain, -3.0, 0.25, irelease, -3.0, 0.0\
      ; Translate MIDI key number to frequency in cycles per second.\
      ifrequency = cpsmidinn(p4)\
      ; Translate MIDI velocity to amplitude.\
      iamplitude = ampdb(p5)\
      ; Band-limited oscillator with integrated sawtooth wave.\
      aout vco2 iamplitude * kenvelope, ifrequency, 8\
      ; Output stereo signal\
      outs aout, aout\
    endin\
  ";
  string score = "i 1 0 10 68 80";
  //string command = "csound -RWfo toot1.wav toot1.orc toot1.sco";
  csound::MusicModel model;
  CppSound *cs = model.getCppSound();
  model.setCsoundOrchestra(orchestra);
  model.setCsoundScoreHeader(score);
  model.generate();
  model.render();
  //model->setCsoundCommand(command);
  model.perform();
  cs->exportForPerformance();
  cs->perform();
}
