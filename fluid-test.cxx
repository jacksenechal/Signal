// generated by Fast Light User Interface Designer (fluid) version 1.0110

#include <stdio.h>
#include <iostream>
#include <pthread.h>
#include <math.h>
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Button.H>
#include <portaudio.h>
using namespace std;
#include "fluid-test.h"

void UserInterface::cb_sawWaveButton_i(Fl_Light_Button*, void*) {
  // do something in the callback;
}
void UserInterface::cb_sawWaveButton(Fl_Light_Button* o, void* v) {
  ((UserInterface*)(o->parent()->user_data()))->cb_sawWaveButton_i(o,v);
}

void UserInterface::cb_midiThroughButton_i(Fl_Light_Button*, void*) {
  // do something in the callback;
}
void UserInterface::cb_midiThroughButton(Fl_Light_Button* o, void* v) {
  ((UserInterface*)(o->parent()->user_data()))->cb_midiThroughButton_i(o,v);
}

void UserInterface::cb_midiSynthButton_i(Fl_Light_Button*, void*) {
  // do something in the callback;
}
void UserInterface::cb_midiSynthButton(Fl_Light_Button* o, void* v) {
  ((UserInterface*)(o->parent()->user_data()))->cb_midiSynthButton_i(o,v);
}

Fl_Double_Window* UserInterface::make_window() {
  Fl_Double_Window* w;
  { Fl_Double_Window* o = new Fl_Double_Window(180, 170);
    w = o;
    o->user_data((void*)(this));
    { sawWaveButton = new Fl_Light_Button(25, 25, 125, 25, "Saw &Wave");
      sawWaveButton->shortcut(0x77);
      //sawWaveButton->callback((Fl_Callback*)cb_sawWaveButton, (void*)(userdata));
      sawWaveButton->callback((Fl_Callback*)cb_sawWaveButton, NULL);
    } // Fl_Light_Button* sawWaveButton
    { midiThroughButton = new Fl_Light_Button(25, 65, 125, 25, "&Midi Through");
      midiThroughButton->shortcut(0x6d);
      //midiThroughButton->callback((Fl_Callback*)cb_midiThroughButton, (void*)(userdata));
      midiThroughButton->callback((Fl_Callback*)cb_midiThroughButton, NULL);
    } // Fl_Light_Button* midiThroughButton
    { midiSynthButton = new Fl_Light_Button(25, 105, 125, 25, "Midi &Synth");
      midiSynthButton->shortcut(0x73);
      //midiSynthButton->callback((Fl_Callback*)cb_midiSynthButton, (void*)(userdata));
      midiSynthButton->callback((Fl_Callback*)cb_midiSynthButton, NULL);
    } // Fl_Light_Button* midiSynthButton
    o->end();
  } // Fl_Double_Window* o
  return w;
}

int main(int argc, char *argv[]) {
    UserInterface *ui = new UserInterface();
    Fl_Double_Window *window = ui->make_window();
    window->show();
    return Fl::run();
}

