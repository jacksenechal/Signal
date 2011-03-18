#include <stdio.h>
#include <iostream>
#include <pthread.h>
#include <math.h>
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Button.H>
#include <portaudio.h>
using namespace std;

#define SAMPLE_RATE   (44100)

typedef struct
{
    float left_phase;
    float right_phase;
}
paTestData;

bool paSawStop;
pthread_mutex_t paSawStopMutex;
pthread_cond_t buttonPressedCondition;

/* This routine will be called by the PortAudio engine when audio is needed.
** It may called at interrupt level on some machines so don't do anything
** that could mess up the system like calling malloc() or free().
*/
static int paSawCallback( const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo *timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData )
{
    /* Cast data passed through stream to our structure. */
    paTestData *data = (paTestData*)userData;
    float *out = (float*)outputBuffer;
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

/*******************************************************************/
static paTestData data;
void *paSaw(void*)
{
    PaStream *stream;
    PaError err;
    
    printf("PortAudio Test: output sawtooth wave.\n");
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
                                SAMPLE_RATE,
                                256,        /* frames per buffer */
                                paSawCallback,
                                &data );
    if( err != paNoError ) goto error;

    err = Pa_StartStream( stream );
    if( err != paNoError ) goto error;

    /* Sleep until button turned off */
    pthread_mutex_lock(&paSawStopMutex);
    pthread_cond_wait(&buttonPressedCondition, &paSawStopMutex);
    pthread_mutex_unlock(&paSawStopMutex);

    err = Pa_StopStream( stream );
    if( err != paNoError ) goto error;
    err = Pa_CloseStream( stream );
    if( err != paNoError ) goto error;
    Pa_Terminate();
    printf("Test finished.\n");
    cout << "thread exiting.\n";
    pthread_exit(NULL);
error:
    Pa_Terminate();
    fprintf( stderr, "An error occured while using the portaudio stream\n" );
    fprintf( stderr, "Error number: %d\n", err );
    fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
    cout << "thread exiting.\n";
    pthread_exit(NULL);
}

void setButtonState( Fl_Widget* obj, int* buttonOn )
{
    if (( *buttonOn ) == 0) {
        obj->label( "ON" );
        ( *buttonOn ) = 1;
    } else {
        obj->label( "OFF" );
        ( *buttonOn ) = 0;
    }
    obj->redraw();
}

void buttonCallback( Fl_Widget* obj, int* buttonOn )
{
    // toggle the button state
    setButtonState( obj, buttonOn );

    // handle threading to start/stop the PortAudio function
    if (( *buttonOn ) == 1) {
        paSawStop = false;

        // start the saw
        cout << "starting thread...\n";
        pthread_t thread;
        int rc = pthread_create(&thread, NULL, paSaw, NULL);
        if (rc){
            printf("ERROR; return code from pthread_create() is %d\n", rc);
            exit(-1);
        }
        cout << "created thread.\n";
    } else {
        // terminate it
        pthread_mutex_lock(&paSawStopMutex);
        paSawStop = true;
        pthread_mutex_unlock(&paSawStopMutex);
        pthread_cond_signal(&buttonPressedCondition);
    }
}

void makeWindow()
{
    Fl_Window* win= new Fl_Window( 180, 180, "C++ FLTK Buttons Tutorial - Example 1" );
    win->begin();  
    Fl_Button* but = new Fl_Button( 60, 60, 60, 60, "OFF" );
    int buttonOn = 0;
    win->end();
    but -> callback( ( Fl_Callback* ) buttonCallback, &buttonOn );
    win->show();
}

int main(int argc, char *argv[]) {
    /* Initialize mutex and condition variable objects */
    pthread_mutex_init(&paSawStopMutex, NULL);
    pthread_cond_init (&buttonPressedCondition, NULL);

    makeWindow();
    return Fl::run();

    pthread_mutex_destroy(&paSawStopMutex);
    pthread_cond_destroy(&buttonPressedCondition);
}

