#include "synth-test.h"
using namespace std;

//===================================================================
// PortAudioTest Class
//===================================================================

PortAudioTest::PortAudioTest() {
    /* Initialize mutex and condition variable objects */
    pthread_mutex_init(&sawButtonMutex, NULL);
    pthread_cond_init (&sawButtonOffCondition, NULL);
}

PortAudioTest::~PortAudioTest() {
    /* Destroy mutex and condition variable objects */
    pthread_mutex_destroy(&sawButtonMutex);
    pthread_cond_destroy(&sawButtonOffCondition);
}

/* This routine will be called by the PortAudio engine when audio is needed.
 * It may called at interrupt level on some machines so don't do anything
 * that could mess up the system like calling malloc() or free().
 */
int PortAudioTest::paSawCallback( const void* inputBuffer, void* outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void* userData )
{
    /* Cast data passed through stream to our structure. */
    paTestData* data = (paTestData*)userData;
    float* out = (float*)outputBuffer;
    unsigned int i;
    (void) inputBuffer; /* Prevent unused variable warning. */

    for( i=0; i<framesPerBuffer; i++ )
    {
        *out++ = data->left_phase;  /* left */
        *out++ = data->right_phase;  /* right */
        /* Generate simple sawtooth phaser that ranges between -1.0 and 1.0. */
        data->left_phase += 0.01f;
        /* When signal reaches top, drop back down. */
        if( data->left_phase >= 1.0f ) data->left_phase -= 2.0f;
        /* higher pitch so we can distinguish left and right. */
        data->right_phase += 0.03f;
        if( data->right_phase >= 1.0f ) data->right_phase -= 2.0f;
    }
    return 0;
}

/* The main thread method, which manages the PortAudio stream. */
void* PortAudioTest::saw()
{
    PaStream* stream;
    PaError err;
    
    printf("PortAudio Test: output sawtooth wave\n");
    /* Initialize our data for use by callback. */
    data.left_phase = data.right_phase = 0.0;
    /* Initialize library before making any other calls. */
    err = Pa_Initialize();
    if( err != paNoError ) goto error;
    
    /* Open an audio I/O stream. */
    err = Pa_OpenDefaultStream( &stream,
                                0,          /* no input channels */
                                2,          /* stereo output */
                                paFloat32,  /* 32 bit floating point output */
                                44100,      /* sample rate */
                                256,        /* frames per buffer */
                                paSawCallback,
                                &data );
    if( err != paNoError ) goto error;

    err = Pa_StartStream( stream );
    if( err != paNoError ) goto error;

    /* Sleep until button turned off */
    pthread_mutex_lock(&sawButtonMutex);
    pthread_cond_wait(&sawButtonOffCondition, &sawButtonMutex);
    pthread_mutex_unlock(&sawButtonMutex);

    err = Pa_StopStream( stream );
    if( err != paNoError ) goto error;
    err = Pa_CloseStream( stream );
    if( err != paNoError ) goto error;
    Pa_Terminate();
    cout << "thread exiting\n";
    pthread_exit(NULL);
error:
    Pa_Terminate();
    fprintf( stderr, "An error occured while using the portaudio stream\n" );
    fprintf( stderr, "Error number: %d\n", err );
    fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
    cout << "thread exiting\n";
    pthread_exit(NULL);
}

/* Start the thread for the PortAudio test */
void PortAudioTest::start() {
    pthread_t thread;
    int rc = pthread_create(&thread, NULL, PortAudioTest_saw, this);
    if (rc){
        printf("ERROR; return code from pthread_create() is %d\n", rc);
        exit(-1);
    }
    cout << "created audio thread\n";
}

/* Stop the thread for the PortAudio test */
void PortAudioTest::stop() {
    pthread_mutex_lock(&sawButtonMutex);
    pthread_cond_broadcast(&sawButtonOffCondition);
    pthread_mutex_unlock(&sawButtonMutex);
}

/* Pthreads are a C level API, so don't understand member methods.
 * Hence we must use this global function to call the member method
 * PortAudioTest::saw()
 */
void* PortAudioTest_saw(void* arg) {
    PortAudioTest* patest = static_cast<PortAudioTest*>(arg);
    patest->saw();
}

//==================================================================
// PortMidiTest Class
//==================================================================

PortMidiTest::PortMidiTest() {
    active = FALSE;
}

/* Start the thread for the PortMidi test */
bool PortMidiTest::start() {
    /* always start the timer before you start midi */
    Pt_Start(1, &PortMidiTest_processMidi, this); /* start a timer with millisecond accuracy */
    /* the timer will call our function, process_midi() every millisecond */

    Pm_Initialize();

    int id = Pm_GetDefaultOutputDeviceID();
    const PmDeviceInfo* info = Pm_GetDeviceInfo(id);
    if (info == NULL) {
        printf("Could not open default output device (%d).", id);
        stop();
        return false;
    }
    printf("Opening output device %s %s\n", info->interf, info->name);

    /* use zero latency because we want output to be immediate */
    Pm_OpenOutput(&midi_out, 
            id, 
            NULL, //DRIVER_INFO
            100,  //OUTPUT_BUFFER_SIZE
            NULL, //TIME_PROC
            NULL, //TIME_INFO
            0    //LATENCY
            );

    id = Pm_GetDefaultInputDeviceID();
    info = Pm_GetDeviceInfo(id);
    if (info == NULL) {
        printf("Could not open default input device (%d).", id);
        stop();
        return false;
    }
    printf("Opening input device %s %s\n", info->interf, info->name);
    Pm_OpenInput(&midi_in, 
            id, 
            NULL, //DRIVER_INFO
            0,    //INPUT_BUFFER_SIZE
            NULL, //TIME_PROC
            NULL //TIME_INFO
            );
 
    active = TRUE; /* enable processing in the midi thread -- yes, this
                      is a shared variable without synchronization, but
                      this simple assignment is safe */

    return true;
}

/* Stop the thread for the PortMidi test */
void PortMidiTest::stop() {
    /* at this point, midi thread is inactive and we need to shut down
     * the midi input and output
     */
    Pt_Stop(); /* stop the timer */
    Pm_Close(midi_in);
    Pm_Close(midi_out);
    cout << "PortMidi devices closed" << endl;
}

void PortMidiTest::processMidi() {
    PmError result;
    PmEvent buffer; /* just one message at a time */
    int32_t msg;

    /* do nothing until initialization completes */
    if (!active) 
        return;

    /* see if there is any midi input to process */
    do {
		result = Pm_Poll(midi_in);
        if (result) {
            if (Pm_Read(midi_in, &buffer, 1) == pmBufferOverflow) 
                continue;
            /* MIDI through */
            Pm_Write(midi_out, &buffer, 1);
            /* unless there was overflow, we should have a message to print */
            if(Pm_MessageStatus(buffer.message) == 0xf8) continue; //filter out time clock messages
            cout << "Msg Status:     "<< hex << Pm_MessageStatus(buffer.message) << endl;
            cout << "Data1:          "<< Pm_MessageData1(buffer.message) << endl;
            cout << "Data2:          "<< Pm_MessageData2(buffer.message) << endl;
            cout << "Timestamp (ms): "<< dec << buffer.timestamp << endl;
        }
    } while (result);
}

/* C-level callback wrapper for PortMidiTest::processMidi() */
void PortMidiTest_processMidi(PtTimestamp timestamp, void *arg) {
    PortMidiTest* pmtest = static_cast<PortMidiTest*>(arg);
    pmtest->processMidi();
}


//==================================================================
// UserInterface Class
//==================================================================

/* callback for the saw wave button */
void UserInterface::cb_sawWaveButton_i(Fl_Light_Button* o, void* v) {
    // Call the PortAudioTest class' functions to start and stop the sound
    if (o->value() == 1) {
        // start the saw
        patest.start();
    } else {
        // terminate it
        patest.stop();
    }
}
void UserInterface::cb_sawWaveButton(Fl_Light_Button* o, void* v) {
    ((UserInterface*)(o->parent()->user_data()))->cb_sawWaveButton_i(o,v);
}

/* callback for the midi thru button */
void UserInterface::cb_midiThruButton_i(Fl_Light_Button* o, void* v) {
    // Call the PortMidiTest class' functions to start and stop the midi
    if (o->value() == 1) {
        // start the midi
        bool success = pmtest.start();
        if (!success) {
            // reset the button to off
            o->value(0);
        }
    } else {
        // terminate it
        pmtest.stop();
    }
}
void UserInterface::cb_midiThruButton(Fl_Light_Button* o, void* v) {
    ((UserInterface*)(o->parent()->user_data()))->cb_midiThruButton_i(o,v);
}

/* callback for the midi to synth button */
void UserInterface::cb_midiSynthButton_i(Fl_Light_Button* o, void* v) {
    // do something in the callback;
}
void UserInterface::cb_midiSynthButton(Fl_Light_Button* o, void* v) {
    ((UserInterface*)(o->parent()->user_data()))->cb_midiSynthButton_i(o,v);
}

Fl_Double_Window* UserInterface::make_window() {
    // make window
    Fl_Double_Window* w = new Fl_Double_Window(180, 170);
    w->user_data((void*)(this));

    // add saw wave button
    sawWaveButton = new Fl_Light_Button(25, 25, 125, 25, "Saw &Wave");
    sawWaveButton->callback((Fl_Callback*)cb_sawWaveButton, NULL); // (void*)(userdata));

    // add midi through button
    midiThruButton = new Fl_Light_Button(25, 65, 125, 25, "Midi &Through");
    midiThruButton->callback((Fl_Callback*)cb_midiThruButton, NULL);

    // add midi to synth button
    midiSynthButton = new Fl_Light_Button(25, 105, 125, 25, "Midi to &Synth");
    midiSynthButton->callback((Fl_Callback*)cb_midiSynthButton, NULL);

    // finish
    w->end();
    return w;
}

//==================================================================
// Main
//==================================================================

int main(int argc, char* argv[]) {
    UserInterface* ui = new UserInterface();
    Fl_Double_Window* window = ui->make_window();
    window->show();
    return Fl::run();
}


