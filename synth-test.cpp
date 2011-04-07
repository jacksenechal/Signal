#include "synth-test.h"
using namespace std;

//===================================================================
// PortAudioTest Class
//===================================================================

PortAudioTest::PortAudioTest() {
    active = FALSE;
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
        *out++ = data->leftPhase;  /* left */
        *out++ = data->rightPhase;  /* right */
        if (*(data->noteOn)) {
            float step = 2.0f / ( 44100.0f / (*(data->frequency)) );
            /* Generate simple sawtooth phaser that ranges between -1.0 and 1.0. */
            data->leftPhase += step;
            /* When signal reaches top, drop back down. */
            if( data->leftPhase >= 1.0f ) data->leftPhase -= 2.0f;
            /* slightly different pitch so we get some interesting phasing. */
            data->rightPhase += step + 0.00001f;
            if( data->rightPhase >= 1.0f ) data->rightPhase -= 2.0f;
        } else {
            data->leftPhase = data->rightPhase = 0;
        }
    }
    return 0;
}

/* The main thread method, which manages the PortAudio stream. */
void* PortAudioTest::saw()
{
    PaStream* stream;
    PaError err;
    
    printf("PortAudioTest thread started\n");
    /* Initialize our data for use by callback. */
    data.leftPhase = data.rightPhase = 0.0;
    /* Initialize library before making any other calls. */
    err = Pa_Initialize();
    if( err != paNoError ) goto error;
    
    /* Open an audio I/O stream. */
    err = Pa_OpenDefaultStream( &stream,
                                0,          /* no input channels */
                                2,          /* stereo output */
                                paFloat32,  /* 32 bit floating point output */
                                44100,      /* sample rate */
                                32,        /* frames per buffer */
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
    cout << "PortAudioTest thread exiting\n";
    pthread_exit(NULL);
error:
    Pa_Terminate();
    fprintf( stderr, "An error occured while using the portaudio stream\n" );
    fprintf( stderr, "Error number: %d\n", err );
    fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
    cout << "PortAudioTest thread exiting\n";
    pthread_exit(NULL);
}

void PortAudioTest::setMonotoneSaw(bool val) {
    cout << "PortAudioTest::setMonotoneSaw(" << val << ")" << endl;
    monotoneSaw = val;
    *(data.noteOn) = val;
    *(data.frequency) = 440;
    cout << "*(data.noteOn) = " << *(data.noteOn) << endl;
    if      (val && !active) start();
    else if (!val && active && !(midiSaw || monotoneSaw)) stop();
}

void PortAudioTest::setMidiSaw(bool val) {
    cout << "PortAudioTest::setMidiSaw(" << val << ")" << endl;
    midiSaw = val;
    if      (val && !active) start();
    else if (!val && active && !(midiSaw || monotoneSaw)) stop();
}

void PortAudioTest::setMidiSource(PortMidiTest* pmtest) {
    data.noteOn    = &(pmtest->noteOn);
    data.frequency = &(pmtest->frequency);
}

/* Start the thread for the PortAudio test */
void PortAudioTest::start() {
    cout << "PortAudioTest::start()" << endl;
    pthread_t thread;
    int rc = pthread_create(&thread, NULL, PortAudioTest_saw, this);
    if (rc){
        printf("ERROR; return code from pthread_create() is %d\n", rc);
        exit(-1);
    }
    active = TRUE;
}

/* Stop the thread for the PortAudio test */
void PortAudioTest::stop() {
    cout << "PortAudioTest::stop()" << endl;
    active = FALSE;
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
    /* Initialize the midi messages queue, as well as mutex and 
     * condition variables for thread synchronization */
    //midiToSynthQueue = Pm_QueueCreate(32, sizeof(int32_t));
}

PortMidiTest::~PortMidiTest() {
    //Pm_QueueDestroy(midiToSynthQueue);
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
    Pm_OpenOutput(&midiOut, 
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
    Pm_OpenInput(&midiIn, 
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
    active = FALSE;
    Pt_Stop(); /* stop the timer */
    Pm_Close(midiIn);
    Pm_Close(midiOut);
    cout << "PortMidi devices closed" << endl;
}

void PortMidiTest::setMidiThru(bool val) {
    midiThru = val;
    if      (val && !active) start();
    else if (!val && active && !(midiToSynth || midiThru)) stop();
}

void PortMidiTest::setMidiToSynth(bool val) {
    midiToSynth = val;
    if      (val && !active) start();
    else if (!val && active && !(midiToSynth || midiThru)) stop();
}

void PortMidiTest::processMidi() {
    PmError result;
    PmEvent buffer; /* just one message at a time */
    int32_t msg;

    /* do nothing until initialization completes */
    if (!active) return;

    /* see if there is any midi input to process */
    do {
		result = Pm_Poll(midiIn);
        if (result) {
            int status, data1, data2, timestamp;
            if (Pm_Read(midiIn, &buffer, 1) == pmBufferOverflow) 
                continue;
            /* MIDI through */
            if (midiThru)
                Pm_Write(midiOut, &buffer, 1);
            /* Extract message information */
            status      = Pm_MessageStatus(buffer.message);
            data1       = Pm_MessageData1(buffer.message);
            data2       = Pm_MessageData2(buffer.message);
            timestamp   = buffer.timestamp;
            /* MIDI to synth */
            if (midiToSynth) {
                //Pm_Enqueue(midiToSynthQueue, &data1);
                // set note on/off and frequency for synth
                if      (status == 0x90) noteOn = true;
                else if (status == 0x80) noteOn = false;
                frequency = (int) 440 * pow(2.0, (float) (data1-69)/12);
                cout << "frequency = " << frequency << endl;
            }
            /* Print out message to console */
            if(status == 0xf8) continue; //filter out time clock messages
            cout << "Msg Status:     0x"<< hex <<   status << endl;
            cout << "Data1:          "  << dec <<   data1 << endl;
            cout << "Data2:          "  << dec <<   data2 << endl;
            cout << "Timestamp (ms): "  << dec <<   timestamp << endl;
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

UserInterface::UserInterface() {
    patest.setMidiSource(&pmtest);
}

/* callback for the saw wave button */
void UserInterface::cb_sawWaveButton_i(Fl_Light_Button* o, void* v) {
    // Call the PortAudioTest class' functions to start and stop the sound
    if (o->value() == 1) {
        // start the saw
        patest.setMonotoneSaw(true);
    } else {
        // terminate it
        patest.setMonotoneSaw(false);
    }
}
/* static callback method required by FLTK, hands off to cb_sawWaveButton_i */
void UserInterface::cb_sawWaveButton(Fl_Light_Button* o, void* v) {
    ((UserInterface*)(o->parent()->user_data()))->cb_sawWaveButton_i(o,v);
}

/* callback for the midi thru button */
void UserInterface::cb_midiThruButton_i(Fl_Light_Button* o, void* v) {
    // Call the PortMidiTest class' functions to start and stop the midi
    if (o->value() == 1) {
        // start the midi
        pmtest.setMidiThru(true);
    } else {
        // terminate it
        pmtest.setMidiThru(false);
    }
}
void UserInterface::cb_midiThruButton(Fl_Light_Button* o, void* v) {
    ((UserInterface*)(o->parent()->user_data()))->cb_midiThruButton_i(o,v);
}

/* callback for the midi to synth button */
void UserInterface::cb_midiSynthButton_i(Fl_Light_Button* o, void* v) {
    // Call the PortMidiTest class' functions to start and stop the synth
    if (o->value() == 1) {
        // start the synth
        pmtest.setMidiToSynth(true);
        patest.setMidiSaw(true);
    } else {
        // terminate it
        pmtest.setMidiToSynth(false);
        patest.setMidiSaw(false);
    }
}
void UserInterface::cb_midiSynthButton(Fl_Light_Button* o, void* v) {
    ((UserInterface*)(o->parent()->user_data()))->cb_midiSynthButton_i(o,v);
}

Fl_Double_Window* UserInterface::makeWindow() {
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
    Fl_Double_Window* window = ui->makeWindow();
    window->show();
    return Fl::run();
}


